/*
 * chan_datacard
 *
 * Copyright (C) 2009 - 2010
 *
 * Artem Makhutov <artem@makhutov.org>
 * http://www.makhutov.org
 * 
 * Dmitry Vagin <dmitry2004@yandex.ru>
 *
 * chan_datacard is based on chan_mobile by Digium
 * (Mark Spencer <markster@digium.com>)
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief UMTS Voice Datacard channel driver
 *
 * \author Artem Makhutov <artem@makhutov.org>
 * \author Dave Bowerman <david.bowerman@gmail.com>
 * \author Dmitry Vagin <dmitry2004@yandex.ru>
 * \author B      G <bg@mail.ru>
 *
 * \ingroup channel_drivers
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */


#include <asterisk.h>
ASTERISK_FILE_VERSION(__FILE__, "$Rev: " PACKAGE_REVISION " $")

#include <asterisk/stringfields.h>		/* AST_DECLARE_STRING_FIELDS for asterisk/manager.h */
#include <asterisk/manager.h>
#include <asterisk/dsp.h>
#include <asterisk/callerid.h>
#include <asterisk/module.h>			/* AST_MODULE_LOAD_DECLINE ... */
#include <asterisk/timing.h>			/* ast_timer_open() ast_timer_fd() */

#include <termios.h>				/* struct termios tcgetattr() tcsetattr()  */
#include <pthread.h>				/* pthread_t pthread_kill() pthread_join() */
#include <fcntl.h>				/* O_RDWR O_NOCTTY */
#include <signal.h>				/* SIGURG */

#include "chan_datacard.h"
#include "at_response.h"			/* at_res_t */
#include "at_queue.h"				/* struct at_queue_task_cmd at_queue_head_cmd() */
#include "at_command.h"				/* at_cmd2str() */
#include "helpers.h"				/* ITEMS_OF() */
#include "at_read.h"
#include "cli.h"
#include "app.h"
#include "manager.h"
#include "channel.h"				/* channel_queue_hangup() */
#include "dc_config.h"				/* dc_uconfig_fill() dc_gconfig_fill() dc_sconfig_fill()  */

typedef struct
{
	pthread_t		discovery_thread;		/* The discovery thread handler */
	int			unloading_flag;			/* no need mutex or other locking for protect this variable because no concurent r/w and set non-0 atomically */
} global_state_t;


static global_state_t globals = 
{
	AST_PTHREADT_NULL,
	0,
};

EXPORT_DEF public_state_t * gpublic;

static int opentty (char* dev)
{
	int		fd;
	struct termios	term_attr;

	fd = open (dev, O_RDWR | O_NOCTTY);

	if (fd < 0)
	{
		ast_log (LOG_WARNING, "Unable to open '%s'\n", dev);
		return -1;
	}
	if (tcgetattr (fd, &term_attr) != 0)
	{
		ast_log (LOG_WARNING, "tcgetattr() failed '%s'\n", dev);
		return -1;
	}

	term_attr.c_cflag = B115200 | CS8 | CREAD | CRTSCTS;
	term_attr.c_iflag = 0;
	term_attr.c_oflag = 0;
	term_attr.c_lflag = 0;
	term_attr.c_cc[VMIN] = 1;
	term_attr.c_cc[VTIME] = 0;

	if (tcsetattr (fd, TCSAFLUSH, &term_attr) != 0)
	{
		ast_log (LOG_WARNING, "tcsetattr() failed '%s'\n", dev);
	}

	return fd;
}

/*!
 * Get status of the datacard. It might happen that the device disappears
 * (e.g. due to a USB unplug).
 *
 * \return 1 if device seems ok, 0 if it seems not available
 */

static int device_status (int fd)
{
	struct termios t;

	if (fd < 0)
	{
		return -1;
	}

	return tcgetattr (fd, &t);
}

static void disconnect_datacard (struct pvt* pvt)
{
	struct cpvt * cpvt, * next;
	
	if (pvt->chansno)
	{
		ast_debug (1, "[%s] Datacard disconnecting, hanging up channels\n", PVT_ID(pvt));
		
		for(cpvt = pvt->chans.first; cpvt; cpvt = next)
		{
			next = cpvt->entry.next;
			at_hangup_immediality(cpvt);
			CPVT_RESET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
			change_channel_state(cpvt, CALL_STATE_RELEASED, 0);
		}
	}
	at_queue_flush(pvt);
	pvt->last_dialed_cpvt = NULL;

	close (pvt->data_fd);
	close (pvt->audio_fd);
	pvt->data_fd = -1;
	pvt->audio_fd = -1;

	ast_dsp_digitreset(pvt->dsp);
	pvt_on_remove_last_channel(pvt);
/*	pvt->a_write_rb */

	pvt->dtmf_digit = 0;
	pvt->rings = 0;

	if(pvt->restarting)
	{
		pvt->restarting = 0;
	}
//	else
	{
		/* unaffected in case of restart */
		pvt->use_ucs2_encoding = 0;
		pvt->cusd_use_7bit_encoding = 0;
		pvt->cusd_use_ucs2_decoding = 0;
		pvt->gsm_reg_status = -1;
		pvt->rssi = 0;
		pvt->linkmode = 0;
		pvt->linksubmode = 0;
		ast_copy_string (pvt->provider_name, "NONE", sizeof (pvt->provider_name));
		pvt->manufacturer[0] = '\0';
		pvt->model[0] = '\0';
		pvt->firmware[0] = '\0';
		pvt->imei[0] = '\0';
		pvt->imsi[0] = '\0';
		pvt->has_subscriber_number = 0;
		ast_copy_string (pvt->subscriber_number, "Unknown", sizeof (pvt->subscriber_number));
		pvt->location_area_code[0] = '\0';
		pvt->cell_id[0] = '\0';
		pvt->sms_scenter[0] = '\0';

		pvt->gsm_registered	= 0;
		pvt->has_sms = 0;
		pvt->has_voice = 0;
		pvt->has_call_waiting = 0;
		pvt->use_pdu = 0;
	}

	pvt->connected		= 0;
	pvt->initialized	= 0;
	pvt->use_pdu		= 0;
	pvt->has_call_waiting	= 0;

	/* FIXME: LOST real device state */
	pvt->dialing = 0;
	pvt->ring = 0;
	pvt->cwaiting = 0;
	pvt->outgoing_sms = 0;
	pvt->incoming_sms = 0;
	pvt->volume_sync_step = VOLUME_SYNC_BEGIN;
	
	ast_verb (3, "Datacard %s has disconnected\n", PVT_ID(pvt));
	ast_debug (1, "[%s] Datacard disconnected\n", PVT_ID(pvt));

#ifdef BUILD_MANAGER
	manager_event (EVENT_FLAG_SYSTEM, "DatacardStatus", "Status: Disconnect\r\nDevice: %s\r\n", PVT_ID(pvt));
#endif
	/* TODO: wakeup discovery thread after some delay */
}

/*!
 * \brief Check if the module is unloading.
 * \retval 0 not unloading
 * \retval 1 unloading
 */

static void* do_monitor_phone (void* data)
{
	struct pvt*	pvt = (struct pvt*) data;
	at_res_t	at_res;
	const struct at_queue_cmd * ecmd;
	int		t;
	char		buf[2*1024];
	struct ringbuffer rb;
	struct iovec	iov[2];
	int		iovcnt;
	char		dev[sizeof(PVT_ID(pvt))];
	int 		fd;
	int		read_result = 0;

	rb_init (&rb, buf, sizeof (buf));
	pvt->timeout = DATA_READ_TIMEOUT;

	ast_mutex_lock (&pvt->lock);
	
	/* 4 reduce locking time make copy of this readonly fields */
	fd = pvt->data_fd;
	ast_copy_string(dev, PVT_ID(pvt), sizeof(dev));
	
	/* anybody wrote some to device before me, and not read results, clean pending results here */

	for (t = 0; at_wait (fd, &t); t = 0)
	{
		iovcnt = at_read (fd, dev, &rb);
		ast_debug (4, "[%s] drop %u bytes of pending data before initialization\n", PVT_ID(pvt), (unsigned)rb_used(&rb));
		/* drop readed */
		rb_init (&rb, buf, sizeof (buf));
		if (iovcnt)
			break;
	}

	/* schedule datacard initilization  */
	if (at_enque_initialization (&pvt->sys_chan, CMD_AT))
	{
		ast_log (LOG_ERROR, "[%s] Error adding initialization commands to queue\n", PVT_ID(pvt));
		goto e_cleanup;
	}

	ast_mutex_unlock (&pvt->lock);


	/* TODO: also check if voice write failed with ENOENT? */
	while (globals.unloading_flag == 0)
	{
		ast_mutex_lock (&pvt->lock);

		if (device_status (pvt->data_fd) || device_status (pvt->audio_fd))
		{
			ast_log (LOG_ERROR, "Lost connection to Datacard %s\n", PVT_ID(pvt));
			goto e_cleanup;
		}

		if(pvt->restarting)
		{
			ast_log (LOG_NOTICE, "[%s] Restarting by request\n", PVT_ID(pvt));
			goto e_restart;
		}
		
		t = pvt->timeout;

		ast_mutex_unlock (&pvt->lock);


		if (!at_wait (fd, &t))
		{
			ast_mutex_lock (&pvt->lock);
			if (!pvt->initialized)
			{
				ast_debug (1, "[%s] timeout waiting for data, disconnecting\n", PVT_ID(pvt));
				
				ecmd = at_queue_head_cmd (pvt);
				if (ecmd)
				{
					ast_debug (1, "[%s] timeout while waiting '%s' in response to '%s'\n", PVT_ID(pvt),
							at_res2str (ecmd->res), at_cmd2str (ecmd->cmd));
				}

				goto e_cleanup;
			}
			else
			{
				ast_mutex_unlock (&pvt->lock);
				continue;
			}
		}

		if (at_read (fd, dev, &rb))
		{
			break;
		}
		while ((iovcnt = at_read_result_iov (dev, &read_result, &rb, iov)) > 0)
		{
			at_res = at_read_result_classification (&rb, iov[0].iov_len + iov[1].iov_len);

			ast_mutex_lock (&pvt->lock);
			if (at_response (pvt, iov, iovcnt, at_res))
			{
				goto e_cleanup;
			}
			ast_mutex_unlock (&pvt->lock);
		}
	}
	ast_mutex_lock (&pvt->lock);

e_cleanup:
	if (!pvt->initialized)
	{
		ast_verb (3, "Error initializing Datacard %s\n", PVT_ID(pvt));
	}
	/* it real disconnect */
	pvt->restarting = 0;

e_restart:
	disconnect_datacard (pvt);

	ast_mutex_unlock (&pvt->lock);

	return NULL;
}


static inline int start_monitor (struct pvt* pvt)
{
	if (ast_pthread_create_background (&pvt->monitor_thread, NULL, do_monitor_phone, pvt) < 0)
	{
		pvt->monitor_thread = AST_PTHREADT_NULL;
		return 0;
	}

	return 1;
}

static void* do_discovery (attribute_unused void * data)
{
	struct pvt* pvt;
	
	while (globals.unloading_flag == 0)
	{
		AST_RWLIST_RDLOCK (&gpublic->devices);
		AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);

			if (!pvt->connected)
			{
				ast_verb (3, "Datacard %s trying to connect on %s...\n", PVT_ID(pvt), CONF_UNIQ(pvt, data_tty));

				if ((pvt->data_fd = opentty (CONF_UNIQ(pvt, data_tty))) > -1)
				{
					// TODO: delay until device activate voice call or at pvt_on_create_1st_channel()
					if ((pvt->audio_fd = opentty (CONF_UNIQ(pvt, audio_tty))) > -1)
					{
						if (start_monitor (pvt))
						{
							pvt->connected = 1;
#ifdef BUILD_MANAGER
							manager_event (EVENT_FLAG_SYSTEM, "DatacardStatus", "Status: Connect\r\nDevice: %s\r\n", PVT_ID(pvt));
#endif
							ast_verb (3, "Datacard %s has connected, initializing...\n", PVT_ID(pvt));
						}
					}
				}
			}

			ast_mutex_unlock (&pvt->lock);
		}
		AST_RWLIST_UNLOCK (&gpublic->devices);

		/* Go to sleep (only if we are not unloading) */
		if (globals.unloading_flag == 0)
		{
			sleep (CONF_GLOBAL(discovery_interval));
		}
	}

	return NULL;
}

#/* */
EXPORT_DEF void pvt_on_create_1st_channel(struct pvt* pvt)
{
	mixb_init (&pvt->a_write_mixb, pvt->a_write_buf, sizeof (pvt->a_write_buf));
//	rb_init (&pvt->a_write_rb, pvt->a_write_buf, sizeof (pvt->a_write_buf));

	if(!pvt->a_timer)
		pvt->a_timer = ast_timer_open ();

/* FIXME: do on each channel switch */
	ast_dsp_digitreset (pvt->dsp);
	pvt->dtmf_digit = 0;
	pvt->dtmf_begin_time.tv_sec = 0;
	pvt->dtmf_begin_time.tv_usec = 0;
	pvt->dtmf_end_time.tv_sec = 0;
	pvt->dtmf_end_time.tv_usec = 0;
}

#/* */
EXPORT_DEF void pvt_on_remove_last_channel(struct pvt* pvt)
{
	if (pvt->a_timer)
	{
		ast_timer_close(pvt->a_timer);
		pvt->a_timer = NULL;
	}
}

#define SET_BIT(dw_array,bitno)		do { (dw_array)[(bitno) >> 5] |= 1 << ((bitno) & 31) ; } while(0)
#define TEST_BIT(dw_array,bitno)	((dw_array)[(bitno) >> 5] & 1 << ((bitno) & 31))

#/* */
EXPORT_DEF int pvt_get_pseudo_call_idx(const struct pvt * pvt)
{
	struct cpvt * cpvt;
	int * bits;
	int dwords = ((MAX_CALL_IDX + sizeof(*bits) - 1) / sizeof(*bits));

	bits = alloca(dwords * sizeof(*bits));
	memset(bits, 0, dwords * sizeof(*bits));
	
	AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
		SET_BIT(bits, cpvt->call_idx);
	}

	for(dwords = 1; dwords <= MAX_CALL_IDX; dwords++)
	{
		if(!TEST_BIT(bits, dwords))
			return dwords;
	}
	return 0;
}

#undef TEST_BIT
#undef SET_BIT

#/* */
static int is_dial_possible2(const struct pvt * pvt, int opts, const struct cpvt * ignore_cpvt)
{
	struct cpvt * cpvt;
	int hold = 0;
	int active = 0;
	int use_call_waiting = opts & CALL_FLAG_HOLD_OTHER;
	
	if(pvt->ring || pvt->cwaiting || pvt->dialing)
		return 0;

	AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
		switch(cpvt->state)
		{
			case CALL_STATE_INIT:
				if(cpvt != ignore_cpvt)
					return 0;
				break;

			case CALL_STATE_DIALING:
			case CALL_STATE_ALERTING:
			case CALL_STATE_INCOMING:
			case CALL_STATE_WAITING:
				return 0;

			case CALL_STATE_ACTIVE:
				if(hold || !use_call_waiting)
					return 0;
				active++;
				break;

			case CALL_STATE_ONHOLD:
				if(active || !use_call_waiting)
					return 0;
				hold++;
				break;

			case CALL_STATE_RELEASED:
				;
		}
	}
	return 1;
}

#/* */
EXPORT_DEF int is_dial_possible(const struct pvt * pvt, int opts)
{
	return is_dial_possible2(pvt, opts, NULL);
}

#/* */
EXPORT_DEF int ready4voice_call(const struct pvt* pvt, const struct cpvt * current_cpvt, int opts)
{
	if(!pvt->connected || !pvt->initialized || !pvt->has_voice || !pvt->gsm_registered)
		return 0;

	return is_dial_possible2(pvt, opts, current_cpvt);
}

#/* */
EXPORT_DEF const char* pvt_str_state(const struct pvt* pvt)
{
	const char * state = "Free";

	if(!pvt->connected)
		state = "Not connected";
	else if(!pvt->initialized)
		state = "Not initialized";
	else if(!pvt->gsm_registered)
		state = "GSM not registered";
	else if(!is_dial_possible(pvt, CALL_FLAG_NONE))
		state = "Busy";
	else if(pvt->outgoing_sms || pvt->incoming_sms)
		state = "SMS";

	return state;
}

#/* */
EXPORT_DEF struct ast_str* pvt_str_state_ex(const struct pvt* pvt)
{
	struct ast_str* buf = ast_str_create (256);

	if(!pvt->connected)
		ast_str_append (&buf, 0, "Not connected");
	else if(!pvt->initialized)
		ast_str_append (&buf, 0, "Not initialized");
	else if(!pvt->gsm_registered)
		ast_str_append (&buf, 0, "GSM not registered");
	else
	{
		if(pvt->ring || pvt->chan_count[CALL_STATE_INCOMING])
			ast_str_append (&buf, 0, "Ring ");

		if(pvt->dialing || (pvt->chan_count[CALL_STATE_INIT] + pvt->chan_count[CALL_STATE_DIALING] + pvt->chan_count[CALL_STATE_ALERTING]) > 0)
			ast_str_append (&buf, 0, "Dialing ");

		if(pvt->cwaiting || pvt->chan_count[CALL_STATE_WAITING])
			ast_str_append (&buf, 0, "Waiting ");

		if(pvt->chan_count[CALL_STATE_ACTIVE] > 0)
			ast_str_append (&buf, 0, "Active %u ", pvt->chan_count[CALL_STATE_ACTIVE]);

		if(pvt->chan_count[CALL_STATE_ONHOLD] > 0)
			ast_str_append (&buf, 0, "Held %u ", pvt->chan_count[CALL_STATE_ONHOLD]);

		if(pvt->incoming_sms)
			ast_str_append (&buf, 0, "Incoming SMS ");

		if(pvt->outgoing_sms)
			ast_str_append (&buf, 0, "Outgoing SMS");

		if(!ast_str_strlen(buf))
			ast_str_append (&buf, 0, "Free");
	}

	return buf;
}

#/* */
static const char* str_descibe(const char * const * strings, unsigned items, int value)
{
	if(value >= 0 && value < (int)items)
		return strings[value];

	return "Unknown";
}

#/* */
EXPORT_DEF const char* GSM_regstate2str(int gsm_reg_status)
{
	static const char * const gsm_states[] = {
		"Not registered, not searching",
		"Registered, home network",
		"Not registered, but searching",
		"Registration denied",
		"Registered, roaming",
		};
	return str_descibe (gsm_states, ITEMS_OF (gsm_states), gsm_reg_status);
}

#/* */
EXPORT_DEF const char* sys_mode2str(int sys_mode)
{
	static const char * const sys_modes[] = {
		"No Service",
		"AMPS",
		"CDMA",
		"GSM/GPRS",
		"HDR",
		"WCDMA",
		"GPS",
		};

	return str_descibe (sys_modes, ITEMS_OF (sys_modes), sys_mode);
}

#/* */
EXPORT_DEF const char * sys_submode2str(int sys_submode)
{
	static const char * const sys_submodes[] = {
		"No service",
		"GSM",
		"GPRS",
		"EDGE",
		"WCDMA",
		"HSDPA",
		"HSUPA",
		"HSDPA and HSUPA",
		};

	return str_descibe (sys_submodes, ITEMS_OF (sys_submodes), sys_submode);
}

#/* */
EXPORT_DEF char* rssi2dBm(int rssi, char * buf, unsigned len)
{
	if(rssi <= 0)
	{
		snprintf(buf, len, "<= -125 dBm");
	}
	else if(rssi <= 30)
	{
		snprintf(buf, len, "%d dBm", 31 * rssi / 50 - 125);
	}
	else if(rssi == 31)
	{
		snprintf(buf, len, ">= -75 dBm");
	}
	else
	{
		snprintf(buf, len, "unknown");
	}
	return buf;
}


/* Module */

/*!
 * \brief Load a device from the configuration file.
 * \param cfg the config to load the device from
 * \param cat the device to load
 * \return NULL on error, a pointer to the device that was loaded on success
 */

static struct pvt* load_device (struct ast_config* cfg, const char* cat, const struct dc_sconfig * defaults)
{
	struct pvt*		pvt;
	int err;

	/* create and initialize our pvt structure */
	pvt = ast_calloc (1, sizeof (*pvt));
	if(pvt)
	{
		err = dc_config_fill(cfg, cat, defaults, &pvt->settings);
		if(!err)
		{
			if(!CONF_SHARED(pvt, disable))
			{
				/* initialize pvt */
				ast_mutex_init (&pvt->lock);

				AST_LIST_HEAD_INIT_NOLOCK (&pvt->at_queue);
				AST_LIST_HEAD_INIT_NOLOCK (&pvt->chans);
				pvt->sys_chan.pvt = pvt;
				pvt->sys_chan.state = CALL_STATE_RELEASED;

				pvt->monitor_thread		= AST_PTHREADT_NULL;
				pvt->audio_fd			= -1;
				pvt->data_fd			= -1;
				pvt->timeout			= DATA_READ_TIMEOUT;
				pvt->cusd_use_ucs2_decoding	=  1;
				pvt->gsm_reg_status		= -1;

				ast_copy_string (pvt->provider_name, "NONE", sizeof (pvt->provider_name));
				ast_copy_string (pvt->subscriber_number, "Unknown", sizeof (pvt->subscriber_number));
				pvt->has_subscriber_number = 0;

				/* setup the dsp */
				pvt->dsp = ast_dsp_new ();
				if (pvt->dsp)
				{
					ast_dsp_set_features (pvt->dsp, DSP_FEATURE_DIGIT_DETECT);
					ast_dsp_set_digitmode (pvt->dsp, DSP_DIGITMODE_DTMF | DSP_DIGITMODE_RELAXDTMF);

					AST_RWLIST_WRLOCK (&gpublic->devices);
					AST_RWLIST_INSERT_HEAD (&gpublic->devices, pvt, entry);
					AST_RWLIST_UNLOCK (&gpublic->devices);

					ast_debug (1, "[%s] Loaded device\n", PVT_ID(pvt));
					ast_log (LOG_NOTICE, "Loaded device %s\n", PVT_ID(pvt));
					return pvt;
				}
				else
				{
					ast_log(LOG_ERROR, "Skipping device %s. Error setting up dsp for dtmf detection\n", cat);
				}
			}
			else
			{
				ast_log (LOG_NOTICE, "Skipping device %s i.e. disabled\n", cat);
			}
		}
		ast_free (pvt);
	}
	else
	{
		ast_log (LOG_ERROR, "Skipping device %s. Error allocating memory\n", cat);
	}

	return NULL;
}

static int load_config ()
{
	struct ast_config*	cfg;
	const char*		cat;
	struct ast_flags	config_flags = { 0 };
	struct dc_sconfig	config_defaults;

	if ((cfg = ast_config_load (CONFIG_FILE, config_flags)) == NULL)
	{
		return -1;
	}

	/* read global config */
	dc_gconfig_fill(cfg, "general", &gpublic->global_settings);
	
	/* read defaults */
	dc_sconfig_fill_defaults(&config_defaults);
	dc_sconfig_fill(cfg, "defaults", &config_defaults);


	/* now load devices */
	for (cat = ast_category_browse (cfg, NULL); cat; cat = ast_category_browse (cfg, cat))
	{
		if (strcasecmp (cat, "general") && strcasecmp (cat, "defaults"))
		{
			load_device (cfg, cat, &config_defaults);
		}
	}

	ast_config_destroy (cfg);

	return 0;
}

static int load_module ()
{
	gpublic = ast_calloc(1, sizeof(*gpublic));
	if(!gpublic)
	{
		ast_log (LOG_ERROR, "Unable to allocate global state structure\n");
		return AST_MODULE_LOAD_DECLINE;
	}
	
	AST_RWLIST_HEAD_INIT(&gpublic->devices);
	ast_mutex_init(&gpublic->round_robin_mtx);
	

	if (load_config ())
	{
		ast_log (LOG_ERROR, "Errors reading config file " CONFIG_FILE ", Not loading module\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	/* Spin the discovery thread */
	if (ast_pthread_create_background (&globals.discovery_thread, NULL, do_discovery, NULL) < 0)
	{
		ast_log (LOG_ERROR, "Unable to create discovery thread\n");
		return AST_MODULE_LOAD_FAILURE;
	}

	/* register our channel type */
	if (ast_channel_register (&channel_tech))
	{
		ast_log (LOG_ERROR, "Unable to register channel class %s\n", "Datacard");
		return AST_MODULE_LOAD_FAILURE;
	}

	cli_register();

#ifdef BUILD_APPLICATIONS
	app_register();
#endif

#ifdef BUILD_MANAGER
	manager_register();
#endif

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module ()
{
	struct pvt* pvt;

	/* First, take us out of the channel loop */
	ast_channel_unregister (&channel_tech);

	/* Unregister the CLI & APP & MANAGER */

#ifdef BUILD_MANAGER
	manager_unregister ();
#endif

#ifdef BUILD_APPLICATIONS
	app_unregister();
#endif

	cli_unregister();

	/* signal everyone we are unloading */
	globals.unloading_flag = 1;

	/* Kill the discovery thread */
	if (globals.discovery_thread != AST_PTHREADT_NULL)
	{
		pthread_kill (globals.discovery_thread, SIGURG);
		pthread_join (globals.discovery_thread, NULL);
	}

	/* Destroy the device list */
	AST_RWLIST_WRLOCK (&gpublic->devices);
	while ((pvt = AST_RWLIST_REMOVE_HEAD (&gpublic->devices, entry)))
	{
		if (pvt->monitor_thread != AST_PTHREADT_NULL)
		{
			pthread_kill (pvt->monitor_thread, SIGURG);
			pthread_join (pvt->monitor_thread, NULL);
		}

		close (pvt->audio_fd);
		close (pvt->data_fd);

		at_queue_flush (pvt);

		ast_dsp_free (pvt->dsp);
		ast_free (pvt);
	}
	AST_RWLIST_UNLOCK (&gpublic->devices);

	ast_mutex_destroy(&gpublic->round_robin_mtx);
	AST_RWLIST_HEAD_DESTROY(&gpublic->devices);

	ast_free(gpublic);
	return 0;
}

AST_MODULE_INFO_STANDARD (ASTERISK_GPL_KEY, MODULE_DESCRIPTION);

EXPORT_DEF struct ast_module* self_module()
{
	return ast_module_info->self;
}
