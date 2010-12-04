/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <asterisk.h>
#include <asterisk/dsp.h>			/* ast_dsp_digitreset() */
#include <asterisk/pbx.h>			/* pbx_builtin_setvar_helper() */
#include <asterisk/module.h>			/* ast_module_ref() ast_module_info = shit */
#include <asterisk/causes.h>			/* AST_CAUSE_INCOMPATIBLE_DESTINATION AST_CAUSE_FACILITY_NOT_IMPLEMENTED AST_CAUSE_REQUESTED_CHAN_UNAVAIL */
#include <asterisk/musiconhold.h>		/* ast_moh_start() ast_moh_stop() */
#include <asterisk/lock.h>			/* AST_MUTEX_DEFINE_STATIC */
#include <asterisk/timing.h>			/* ast_timer_fd() ast_timer_set_rate() ast_timer_ack() */
#include <asterisk/version.h>			/* ASTERISK_VERSION_NUM */

#include "channel.h"
#include "chan_datacard.h"
#include "at_command.h"
#include "helpers.h"				/* get_at_clir_value()  */

static char silence_frame[FRAME_SIZE];

static int parse_dial_string(char * dialstr, const char** number, int * opts)
{
	char* options;
	char* dest_num;
	int lopts = 0;
	
	options = strchr (dialstr, '/');
	if (!options)
	{
		ast_log (LOG_WARNING, "Can't determine destination in chan_datacard\n");
		return AST_CAUSE_INCOMPATIBLE_DESTINATION;
	}
	*options++ = '\0';

	dest_num = strchr(options, ':');
	if(!dest_num)
	{
		dest_num = options;
	}
	else
	{
		*dest_num++ = '\0';

		if (!strcasecmp(options, "holdother"))
			lopts = CALL_FLAG_HOLD_OTHER;
		else
		{
			ast_log (LOG_WARNING, "Invalid options in chan_datacard\n");
			return AST_CAUSE_INCOMPATIBLE_DESTINATION;
		}
	}
	
	if (*dest_num == '\0')
	{
		ast_log (LOG_WARNING, "Empty destination in chan_datacard\n");
		return AST_CAUSE_INCOMPATIBLE_DESTINATION;
	}
	if (!is_valid_phone_number(dest_num))
	{
		ast_log (LOG_WARNING, "Invalid destination '%s' in chan_datacard, only 0123456789*#+ABC allowed\n", dest_num);
		return AST_CAUSE_INCOMPATIBLE_DESTINATION;
	}

	*number = dest_num;
	*opts = lopts;
	return 0;
}

// TODO: add check when request 'holdother' what requestor is not on same device
// TODO: simplify by move common code to functions
#if ASTERISK_VERSION_NUM >= 10800
static struct ast_channel* channel_request (attribute_unused const char* type, format_t format, attribute_unused const struct ast_channel *requestor, void* data, int* cause)
#else
static struct ast_channel* channel_request (attribute_unused const char* type, int format, void* data, int* cause)
#endif
{
#if ASTERISK_VERSION_NUM >= 10800
	format_t		oldformat;
#else
	int			oldformat;
#endif
	char*			dest_dev;
	const char*		dest_num;
	struct ast_channel*	channel = NULL;
	struct pvt*		pvt = NULL;
	int			group;
	size_t			i;
	size_t			j;
	size_t			c;
	size_t			last_used;
	int			opts = CALL_FLAG_NONE;
	
	if (!data)
	{
		ast_log (LOG_WARNING, "Channel requested with no data\n");
		*cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
		return NULL;
	}

	oldformat = format;
	format &= AST_FORMAT_SLINEAR;
	if (!format)
	{
#if ASTERISK_VERSION_NUM >= 10800
		ast_log (LOG_WARNING, "Asked to get a channel of unsupported format '%s'\n", ast_getformatname (oldformat));
#else
		ast_log (LOG_WARNING, "Asked to get a channel of unsupported format '%d'\n", oldformat);
#endif
		*cause = AST_CAUSE_FACILITY_NOT_IMPLEMENTED;
		return NULL;
	}

	dest_dev = ast_strdupa (data);

	*cause = parse_dial_string(dest_dev, &dest_num, &opts);
	if(*cause)
		return NULL;

	/* Find requested device and make sure it's connected and initialized. */

	AST_RWLIST_RDLOCK (&gpublic->devices);

	if (((dest_dev[0] == 'g') || (dest_dev[0] == 'G')) && ((dest_dev[1] >= '0') && (dest_dev[1] <= '9')))
	{
/*
		if(opts == CALL_OPTION_CONFERENCE)
		{
badconf:
			ast_log (LOG_WARNING, "option 'conference' uncompatible in chan_datacard channel_request()\n");
			*cause = AST_CAUSE_INCOMPATIBLE_DESTINATION;
			return NULL;
		}
*/
		errno = 0;
		group = (int) strtol (&dest_dev[1], (char**) NULL, 10);
		if (errno != EINVAL)
		{
			AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
			{
				ast_mutex_lock (&pvt->lock);

				if (CONF_SHARED(pvt, group) == group && ready4voice_call(pvt, NULL, opts))
				{
					break;
				}
				ast_mutex_unlock (&pvt->lock);
			}
		}
	}
	else if (((dest_dev[0] == 'r') || (dest_dev[0] == 'R')) && ((dest_dev[1] >= '0') && (dest_dev[1] <= '9')))
	{
		errno = 0;
		group = (int) strtol (&dest_dev[1], (char**) NULL, 10);
		if (errno != EINVAL)
		{
			ast_mutex_lock (&gpublic->round_robin_mtx);

			/* Generate a list of all availible devices */
			j = ITEMS_OF (gpublic->round_robin);
			c = 0; last_used = 0;
			AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
			{
				ast_mutex_lock (&pvt->lock);
				if (CONF_SHARED(pvt, group) == group)
				{
					gpublic->round_robin[c] = pvt;
					if (pvt->group_last_used == 1)
					{
						pvt->group_last_used = 0;
						last_used = c;
					}

					c++;

					if (c == j)
					{
						ast_mutex_unlock (&pvt->lock);
						break;
					}
				}
				ast_mutex_unlock (&pvt->lock);
			}

			/* Search for a availible device starting at the last used device */
			for (i = 0, j = last_used + 1; i < c; i++, j++)
			{
				if (j == c)
				{
					j = 0;
				}

				pvt = gpublic->round_robin[j];

				ast_mutex_lock (&pvt->lock);
				if (ready4voice_call(pvt, NULL, opts))
				{
					pvt->group_last_used = 1;
					break;
				}
				ast_mutex_unlock (&pvt->lock);
			}

			ast_mutex_unlock (&gpublic->round_robin_mtx);
		}
	}
	else if (((dest_dev[0] == 'p') || (dest_dev[0] == 'P')) && dest_dev[1] == ':')
	{
		ast_mutex_lock (&gpublic->round_robin_mtx);

		/* Generate a list of all availible devices */
		j = ITEMS_OF (gpublic->round_robin);
		c = 0; last_used = 0;
		AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strcmp (pvt->provider_name, &dest_dev[2]))
			{
				gpublic->round_robin[c] = pvt;
				if (pvt->prov_last_used == 1)
				{
					pvt->prov_last_used = 0;
					last_used = c;
				}

				c++;

				if (c == j)
				{
					ast_mutex_unlock (&pvt->lock);
					break;
				}
			}
			ast_mutex_unlock (&pvt->lock);
		}

		/* Search for a availible device starting at the last used device */
		for (i = 0, j = last_used + 1; i < c; i++, j++)
		{
			if (j == c)
			{
				j = 0;
			}

			pvt = gpublic->round_robin[j];

			ast_mutex_lock (&pvt->lock);
			if (ready4voice_call(pvt, NULL, opts))
			{
				pvt->prov_last_used = 1;
				break;
			}
			ast_mutex_unlock (&pvt->lock);
		}

		ast_mutex_unlock (&gpublic->round_robin_mtx);
	}
	else if (((dest_dev[0] == 's') || (dest_dev[0] == 'S')) && dest_dev[1] == ':')
	{
		ast_mutex_lock (&gpublic->round_robin_mtx);

		/* Generate a list of all availible devices */
		j = ITEMS_OF (gpublic->round_robin);
		c = 0; last_used = 0;
		i = strlen (&dest_dev[2]);
		AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strncmp (pvt->imsi, &dest_dev[2], i))
			{
				gpublic->round_robin[c] = pvt;
				if (pvt->sim_last_used == 1)
				{
					pvt->sim_last_used = 0;
					last_used = c;
				}

				c++;

				if (c == j)
				{
					ast_mutex_unlock (&pvt->lock);
					break;
				}
			}
			ast_mutex_unlock (&pvt->lock);
		}

		/* Search for a availible device starting at the last used device */
		for (i = 0, j = last_used + 1; i < c; i++, j++)
		{
			if (j == c)
			{
				j = 0;
			}

			pvt = gpublic->round_robin[j];

			ast_mutex_lock (&pvt->lock);
			if (ready4voice_call(pvt, NULL, opts))
			{
				pvt->sim_last_used = 1;
				break;
			}
			ast_mutex_unlock (&pvt->lock);
		}

		ast_mutex_unlock (&gpublic->round_robin_mtx);
	}
	else if (((dest_dev[0] == 'i') || (dest_dev[0] == 'I')) && dest_dev[1] == ':')
	{
		AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strcmp(pvt->imei, &dest_dev[2]))
			{
				break;
			}
			ast_mutex_unlock (&pvt->lock);
		}
	}
	else
	{
		AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
		{
			ast_mutex_lock (&pvt->lock);
			if (!strcmp (PVT_ID(pvt), dest_dev))
			{
				break;
			}
			ast_mutex_unlock (&pvt->lock);
		}
	}

	AST_RWLIST_UNLOCK (&gpublic->devices);
	if (!pvt || !is_dial_possible(pvt, opts, NULL))
	{
		if (pvt)
		{
			ast_mutex_unlock (&pvt->lock);
		}
	
		ast_log (LOG_WARNING, "Request to call on device '%s' which can not make call at this moment\n", dest_dev);
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;

		return NULL;
	}

	channel = channel_new (pvt, AST_STATE_DOWN, NULL, pvt_get_pseudo_call_idx(pvt), CALL_DIR_OUTGOING, CALL_STATE_INIT);

	ast_mutex_unlock (&pvt->lock);

	if (!channel)
	{
		ast_log (LOG_WARNING, "Unable to allocate channel structure\n");
		*cause = AST_CAUSE_REQUESTED_CHAN_UNAVAIL;

		return NULL;
	}

	return channel;
}

static int channel_call (struct ast_channel* channel, char* dest, attribute_unused int timeout)
{
	struct cpvt* cpvt = channel->tech_pvt;
	struct pvt* pvt;
	char* dest_dev;
	const char* dest_num;
	int clir = 0;
	int opts;

	if(!cpvt || cpvt->channel != channel || !cpvt->pvt)
	{
		ast_log (LOG_WARNING, "call on unreferenced %s\n", channel->name);
		return -1;
	}
	pvt = cpvt->pvt;

	dest_dev = ast_strdupa (dest);

	if(parse_dial_string(dest_dev, &dest_num, &opts))
		return -1;

	if ((channel->_state != AST_STATE_DOWN) && (channel->_state != AST_STATE_RESERVED))
	{
		ast_log (LOG_WARNING, "channel_call called on %s, neither down nor reserved\n", channel->name);
		return -1;
	}

	ast_mutex_lock (&pvt->lock);

	if (!ready4voice_call(pvt, cpvt, opts))
	{
		ast_mutex_unlock (&pvt->lock);
		ast_log (LOG_ERROR, "[%s] Error device already in use or uninitialized\n", PVT_ID(pvt));
		return -1;
	}
	CPVT_SET_FLAGS(cpvt, opts);

	ast_debug (1, "[%s] Calling %s on %s\n", PVT_ID(pvt), dest, channel->name);

	if (CONF_SHARED(pvt, usecallingpres))
	{
		if (CONF_SHARED(pvt, callingpres) < 0)
		{
#if ASTERISK_VERSION_NUM >= 10800
			clir = channel->connected.id.number.presentation;
#else
			clir = channel->cid.cid_pres;
#endif
		}
		else
		{
			clir = CONF_SHARED(pvt, callingpres);
		}

		clir = get_at_clir_value (pvt, clir);
	}
	else
	{
		clir = -1;
	}
	
	if (at_enque_dial (cpvt, dest_num, clir))
	{
		ast_mutex_unlock (&pvt->lock);
		ast_log (LOG_ERROR, "[%s] Error sending ATD command\n", PVT_ID(pvt));
		return -1;
	}

	ast_mutex_unlock (&pvt->lock);

	return 0;
}

#/* */
static void channel_disactivate(struct cpvt* cpvt)
{
	if(cpvt->channel && CPVT_TEST_FLAG(cpvt, CALL_FLAG_ACTIVATED))
	{
		ast_channel_set_fd (cpvt->channel, 1, -1);
		ast_channel_set_fd (cpvt->channel, 0, -1);
	}
}

#/* */
static void channel_activate_onlyone(struct cpvt* cpvt)
{
	struct cpvt* cpvt2;
	struct pvt* pvt = cpvt->pvt;
	
	if(CPVT_TEST_FLAG(cpvt, CALL_FLAG_ACTIVATED))
		return;
	
	AST_LIST_TRAVERSE(&pvt->chans, cpvt2, entry)
	{
		if(cpvt2 != cpvt)
		{
			channel_disactivate(cpvt2);
		}
	}
	
	if (pvt->audio_fd >= 0)
	{
		CPVT_SET_FLAGS(cpvt, CALL_FLAG_ACTIVATED);
		ast_channel_set_fd (cpvt->channel, 0, pvt->audio_fd);
		if (pvt->a_timer)
		{
			ast_channel_set_fd (cpvt->channel, 1, ast_timer_fd (pvt->a_timer));
//			if (PVT_SINGLE_CHAN(pvt))
//			{
				ast_timer_set_rate (pvt->a_timer, 50);
//				ast_debug (3, "[%s] Timer set\n", PVT_ID(pvt));
//			}
		}
	}
}

/* we has 2 case of call this function, when local side want terminate call and when called for cleanup after remote side alreay terminate call, CEND received and cpvt destroyed */
static int channel_hangup (struct ast_channel* channel)
{
	struct cpvt* cpvt = channel->tech_pvt;
	struct pvt* pvt;

	/* its possible call with channel w/o tech_pvt */
	if(cpvt && cpvt->channel == channel && cpvt->pvt)
	{
		pvt = cpvt->pvt;

		ast_mutex_lock (&pvt->lock);

		ast_debug (1, "[%s] Hanging up call idx %d need hangup %d\n", PVT_ID(pvt), cpvt->call_idx, CPVT_TEST_FLAG(cpvt, CALL_FLAG_NEED_HANGUP));

		if (CPVT_TEST_FLAG(cpvt, CALL_FLAG_NEED_HANGUP))
		{
			if (at_enque_hangup (cpvt, cpvt->call_idx))
				ast_log (LOG_ERROR, "[%s] Error adding AT+CHUP command to queue, call not terminated!\n", PVT_ID(pvt));
			else
				CPVT_RESET_FLAG(cpvt, CALL_FLAG_NEED_HANGUP);
		}

		channel_disactivate(cpvt);

		/* drop cpvt->channel reference */
		cpvt->channel = NULL;
		ast_mutex_unlock (&pvt->lock);
	}

	/* drop channel -> cpvt reference */
	channel->tech_pvt = NULL;

	ast_module_unref (self_module());
	ast_setstate (channel, AST_STATE_DOWN);

	return 0;
}

static int channel_answer (struct ast_channel* channel)
{
	struct cpvt* cpvt = channel->tech_pvt;
	struct pvt* pvt;

	if(!cpvt || cpvt->channel != channel || !cpvt->pvt)
	{
		ast_log (LOG_WARNING, "call on unreferenced %s\n", channel->name);
		return 0;
	}
	pvt = cpvt->pvt;
	
	ast_mutex_lock (&pvt->lock);

	if (cpvt->dir == CALL_DIR_INCOMING)
	{
		if (at_enque_answer (cpvt))
		{
			ast_log (LOG_ERROR, "[%s] Error sending answer commands\n", PVT_ID(pvt));
		}
	}

	ast_mutex_unlock (&pvt->lock);

	return 0;

}

static int channel_digit_begin (struct ast_channel* channel, char digit)
{
	struct cpvt* cpvt = channel->tech_pvt;
	struct pvt* pvt;

	if(!cpvt || cpvt->channel != channel || !cpvt->pvt)
	{
		ast_log (LOG_WARNING, "call on unreferenced %s\n", channel->name);
		return -1;
	}
	pvt = cpvt->pvt;

	ast_mutex_lock (&pvt->lock);

	if (at_enque_dtmf (cpvt, digit))
	{
		ast_mutex_unlock (&pvt->lock);
		ast_log (LOG_ERROR, "[%s] Error adding DTMF %c command to queue\n", PVT_ID(pvt), digit);

		return -1;
	}

	ast_mutex_unlock (&pvt->lock);

	ast_debug (1, "[%s] Send DTMF %c\n", PVT_ID(pvt), digit);

	return 0;
}


static int channel_digit_end (attribute_unused struct ast_channel* channel, attribute_unused char digit, attribute_unused unsigned int duration)
{
	return 0;
}

static void iov_write(struct pvt* pvt, int fd, struct iovec * iov, int iovcnt)
{
	ssize_t written;
	ssize_t done = 0;
	int count = 10;
	
	while(iovcnt)
	{
again:
		written = writev (fd, iov, iovcnt);
		if(written < 0)
		{
			if((errno == EINTR || errno == EAGAIN))
			{
				--count;
				if(count != 0)
				    goto again;
				ast_debug (1, "[%s] Deadlock avoided for write!\n", PVT_ID(pvt));
			}
			break;
		}
		else
		{
			done += written;
			count = 10;
			do
			{
				if((size_t)written >= iov->iov_len)
				{
					written -= iov->iov_len;
					iovcnt--;
					iov++;
				}
				else
				{
					iov->iov_len -= written;
					goto again;
				}
			} while(written > 0);
		}
	}
	PVT_STAT_PUMP(write_bytes, += done);
	
	if (done != FRAME_SIZE)
	{
		ast_debug (1, "[%s] Write error!\n", PVT_ID(pvt));
	}
}

static void channel_timing_write (struct pvt* pvt)
{
	size_t		used;
	int		iovcnt;
	struct iovec	iov[3];
	const char * msg = NULL;


	used = rb_used (&pvt->a_write_rb);

	if (used >= FRAME_SIZE)
	{
		iovcnt = rb_read_n_iov (&pvt->a_write_rb, iov, FRAME_SIZE);
		rb_read_upd (&pvt->a_write_rb, FRAME_SIZE);
	}
	else if (used > 0)
	{
		PVT_STAT_PUMP(write_tframes, ++);
		msg = "[%s] write truncated frame\n";

		iovcnt = rb_read_all_iov (&pvt->a_write_rb, iov);
		rb_read_upd (&pvt->a_write_rb, used);

		iov[iovcnt].iov_base	= silence_frame;
		iov[iovcnt].iov_len	= FRAME_SIZE - used;
		iovcnt++;
	}
	else
	{
		PVT_STAT_PUMP(write_sframes, ++);
		msg = "[%s] write silence\n";

		iov[0].iov_base		= silence_frame;
		iov[0].iov_len		= FRAME_SIZE;
		iovcnt			= 1;
	}

	PVT_STAT_PUMP(write_frames, ++);
	iov_write(pvt, pvt->audio_fd, iov, iovcnt);

	if(msg)
		ast_debug (7, msg, PVT_ID(pvt));
}

#if ASTERISK_VERSION_NUM >= 10800
#define subclass_codec		subclass.codec
#define subclass_integer	subclass.integer
#else
#define subclass_codec		subclass
#define subclass_integer	subclass
#endif

static struct ast_frame* channel_read (struct ast_channel* channel)
{
	struct cpvt*		cpvt = channel->tech_pvt;
	struct pvt *		pvt;
	struct ast_frame*	f = &ast_null_frame;
	ssize_t			res;

	if(!cpvt || cpvt->channel != channel || !cpvt->pvt)
	{
		return f;
	}
	pvt = cpvt->pvt;

	while (ast_mutex_trylock (&pvt->lock))
	{
		CHANNEL_DEADLOCK_AVOIDANCE (channel);
	}

	ast_debug (7, "[%s] read call idx %d state %d audio_fd %d\n", PVT_ID(pvt), cpvt->call_idx, cpvt->state, pvt->audio_fd);

	if (!CPVT_IS_SOUND_SOURCE(cpvt) || pvt->audio_fd < 0)
	{
		goto e_return;
	}

	if (pvt->a_timer && channel->fdno == 1)
	{
		ast_timer_ack (pvt->a_timer, 1);
		channel_timing_write (pvt);
		ast_debug (7, "[%s] *** timing ***\n", PVT_ID(pvt));
	}
	else
	{
		memset (&pvt->a_read_frame, 0, sizeof (pvt->a_read_frame));

		pvt->a_read_frame.frametype	= AST_FRAME_VOICE;
		pvt->a_read_frame.subclass_codec = AST_FORMAT_SLINEAR;
		pvt->a_read_frame.data.ptr	= pvt->a_read_buf + AST_FRIENDLY_OFFSET;
		pvt->a_read_frame.offset	= AST_FRIENDLY_OFFSET;

		res = read (pvt->audio_fd, pvt->a_read_frame.data.ptr, FRAME_SIZE);
		if (res <= 0)
		{
			if (errno != EAGAIN && errno != EINTR)
			{
				ast_debug (1, "[%s] Read error %d, going to wait for new connection\n", PVT_ID(pvt), errno);
			}

			goto e_return;
		}

//		ast_debug (7, "[%s] call idx %d read %u\n", PVT_ID(pvt), cpvt->call_idx, (unsigned)res);
		PVT_STAT_PUMP(read_bytes, += res);
		PVT_STAT_PUMP(read_frames, ++);
		if(res < FRAME_SIZE)
			PVT_STAT_PUMP(read_sframes, ++);

		pvt->a_read_frame.samples	= res / 2;
		pvt->a_read_frame.datalen	= res;

		if (pvt->dsp)
		{
			f = ast_dsp_process (channel, pvt->dsp, &pvt->a_read_frame);
			if ((f->frametype == AST_FRAME_DTMF_END) || (f->frametype == AST_FRAME_DTMF_BEGIN))
			{
				if ((f->subclass_integer == 'm') || (f->subclass_integer == 'u'))
				{
					f->frametype = AST_FRAME_NULL;
					f->subclass_integer = 0;
					ast_mutex_unlock (&pvt->lock);
					return(f);
				}
				if(f->frametype == AST_FRAME_DTMF_BEGIN)
				{
					pvt->dtmf_begin_time = ast_tvnow();
				}
				else if (f->frametype == AST_FRAME_DTMF_END)
				{
					if(!ast_tvzero(pvt->dtmf_begin_time) && ast_tvdiff_ms(ast_tvnow(), pvt->dtmf_begin_time) < CONF_SHARED(pvt, mindtmfgap))
					{
						ast_debug(1, "[%s] DTMF char %c ignored min gap %d > %ld\n", PVT_ID(pvt), f->subclass_integer, CONF_SHARED(pvt, mindtmfgap), (long)ast_tvdiff_ms(ast_tvnow(), pvt->dtmf_begin_time));
						f->frametype = AST_FRAME_NULL;
						f->subclass_integer = 0;
					}
					else if(f->len < CONF_SHARED(pvt, mindtmfduration))
					{
						ast_debug(1, "[%s] DTMF char %c ignored min duration %d > %ld\n", PVT_ID(pvt), f->subclass_integer, CONF_SHARED(pvt, mindtmfduration), f->len);
						f->frametype = AST_FRAME_NULL;
						f->subclass_integer = 0;
					}
					else if(f->subclass_integer == pvt->dtmf_digit
							&& 
						!ast_tvzero(pvt->dtmf_end_time)
							&& 
						ast_tvdiff_ms(ast_tvnow(), pvt->dtmf_end_time) < CONF_SHARED(pvt, mindtmfinterval))
					{
						ast_debug(1, "[%s] DTMF char %c ignored min interval %d > %ld\n", PVT_ID(pvt), f->subclass_integer, CONF_SHARED(pvt, mindtmfinterval), (long)ast_tvdiff_ms(ast_tvnow(), pvt->dtmf_end_time));
						f->frametype = AST_FRAME_NULL;
						f->subclass_integer = 0;
					}
					else
					{
						ast_debug(1, "[%s] Got DTMF char %c\n",PVT_ID(pvt), f->subclass_integer);
						pvt->dtmf_digit = f->subclass_integer;
						pvt->dtmf_end_time = ast_tvnow();
					}

				}
				goto e_return;
			}
		}

		if (CONF_SHARED(pvt, rxgain))
		{
			if (ast_frame_adjust_volume (f, CONF_SHARED(pvt, rxgain)) == -1)
			{
				ast_debug (1, "[%s] Volume could not be adjusted!\n", PVT_ID(pvt));
			}
		}
	}

e_return:
	ast_mutex_unlock (&pvt->lock);

	return f;
}


static int channel_write (struct ast_channel* channel, struct ast_frame* f)
{
	struct cpvt* cpvt = channel->tech_pvt;
	struct pvt* pvt;
	size_t count;

	if(!cpvt || cpvt->channel != channel || !cpvt->pvt)
	{
		ast_log (LOG_WARNING, "call on unreferenced %s\n", channel->name);
		return 0;
	}
	pvt = cpvt->pvt;

	ast_debug (7, "[%s] write call idx %d state %d\n", PVT_ID(pvt), cpvt->call_idx, cpvt->state);

	if (f->frametype != AST_FRAME_VOICE || f->subclass_codec != AST_FORMAT_SLINEAR)
	{
		return 0;
	}

	while (ast_mutex_trylock (&pvt->lock))
	{
		CHANNEL_DEADLOCK_AVOIDANCE (channel);
	}

	if (!CPVT_IS_ACTIVE(cpvt))
		goto e_return;
	
	if (pvt->audio_fd < 0)
	{
		ast_debug (1, "[%s] audio_fd not ready\n", PVT_ID(pvt));
	}
	else
	{

		if (CONF_SHARED(pvt, txgain) && f->datalen > 0)
		{
			if (ast_frame_adjust_volume (f, CONF_SHARED(pvt, txgain)) == -1)
			{
				ast_debug (1, "[%s] Volume could not be adjusted!\n", PVT_ID(pvt));
			}
		}

		if (pvt->a_timer)
		{
			count = rb_free (&pvt->a_write_rb);

			if (count < (size_t) f->datalen)
			{
				rb_read_upd (&pvt->a_write_rb, f->datalen - count);

				PVT_STAT_PUMP(write_rb_overflow_bytes, += f->datalen - count);
				PVT_STAT_PUMP(write_rb_overflow, ++);
			}

			rb_write (&pvt->a_write_rb, f->data.ptr, f->datalen);
		}
		else
		{
			int		iovcnt;
			struct iovec	iov[2];

			iov[0].iov_base		= f->data.ptr;
			iov[0].iov_len		= FRAME_SIZE;
			iovcnt			= 1;

			if (f->datalen < FRAME_SIZE)
			{
				iov[0].iov_len	= f->datalen;
				iov[1].iov_base	= silence_frame;
				iov[1].iov_len	= FRAME_SIZE - f->datalen;
				iovcnt		= 2;
			}

			iov_write(pvt, pvt->audio_fd, iov, iovcnt);
		}

//		if (f->datalen != 320)
		{
			ast_debug (7, "[%s] Write frame: samples = %d, data lenght = %d byte\n", PVT_ID(pvt), f->samples, f->datalen);
		}
	}

e_return:
	ast_mutex_unlock (&pvt->lock);

	return 0;
}

#undef subclass_integer
#undef subclass_codec


static int channel_fixup (struct ast_channel* oldchannel, struct ast_channel* newchannel)
{
	struct cpvt * cpvt = newchannel->tech_pvt;
	struct pvt* pvt;
	
	if (!cpvt || !cpvt->pvt)
	{
		ast_log (LOG_WARNING, "call on unreferenced %s\n", newchannel->name);
		return -1;
	}
	pvt = cpvt->pvt;
	
	ast_mutex_lock (&pvt->lock);
	if (cpvt->channel == oldchannel)
	{
		cpvt->channel = newchannel;
	}
	ast_mutex_unlock (&pvt->lock);

	return 0;
}

// FIXME: must modify in conjuction with state on call not whole device
static int channel_devicestate (void* data)
{
	char*	device;
	struct pvt*	pvt;
	int	res = AST_DEVICE_INVALID;

	device = ast_strdupa (data ? data : "");

	ast_debug (1, "Checking device state for device %s\n", device);

	pvt = find_device (device);
	if (pvt)
	{
		ast_mutex_lock (&pvt->lock);
		if (pvt->connected)
		{
			if (is_dial_possible(pvt, CALL_FLAG_NONE, NULL))
			{
				res = AST_DEVICE_NOT_INUSE;
			}
			else
			{
				res = AST_DEVICE_INUSE;
			}
		}
		ast_mutex_unlock (&pvt->lock);
	}

	return res;
}

static int channel_indicate (struct ast_channel* channel, int condition, const void* data, attribute_unused size_t datalen)
{
	int res = 0;

	ast_debug (1, "[%s] Requested indication %d\n", channel->name, condition);

	switch (condition)
	{
		case AST_CONTROL_BUSY:
		case AST_CONTROL_CONGESTION:
		case AST_CONTROL_RINGING:
			break;

		case -1:
			res = -1;
			break;

		case AST_CONTROL_SRCCHANGE:
		case AST_CONTROL_PROGRESS:
		case AST_CONTROL_PROCEEDING:
		case AST_CONTROL_VIDUPDATE:
		case AST_CONTROL_SRCUPDATE:
			break;

		case AST_CONTROL_HOLD:
			ast_moh_start (channel, data, NULL);
			break;

		case AST_CONTROL_UNHOLD:
			ast_moh_stop (channel);
			break;

		default:
			ast_log (LOG_WARNING, "[%s] Don't know how to indicate condition %d\n", channel->name, condition);
			res = -1;
			break;
	}

	return res;
}


#/* NOTE: called from device level with locked pvt */
// FIXME: protection for cpvt->channel if exists
EXPORT_DEF void channel_change_state(struct cpvt * cpvt, unsigned newstate, int cause)
{
	struct pvt* pvt;
	call_state_t oldstate = cpvt->state;

	if(newstate != oldstate)
	{
		pvt = cpvt->pvt;

		cpvt->state = newstate;
		pvt->chan_count[oldstate]--;
		pvt->chan_count[newstate]++;

		ast_debug (1, "[%s] Call idx %d change state from '%s' to '%s' has%s channel\n", PVT_ID(pvt), cpvt->call_idx, call_state2str(oldstate), call_state2str(newstate), cpvt->channel ? "" : "'t");
		if(!cpvt->channel)
		{
			/* channel already dead */
			if(newstate == CALL_STATE_RELEASED)
			{
				cpvt_free(cpvt);
			}
			return;
		}

		switch(newstate)
		{
			case CALL_STATE_DIALING:
				/* from ^ORIG:idx,y */
				channel_activate_onlyone(cpvt);
				channel_queue_control (cpvt, AST_CONTROL_PROGRESS);
				ast_setstate (cpvt->channel, AST_STATE_DIALING);
				break;

			case CALL_STATE_ALERTING:
				channel_activate_onlyone(cpvt);
				channel_queue_control (cpvt, AST_CONTROL_RINGING);
				ast_setstate (cpvt->channel, AST_STATE_RINGING);
				break;

			case CALL_STATE_ACTIVE:
				channel_activate_onlyone(cpvt);
				if (oldstate == CALL_STATE_ONHOLD)
				{
					ast_debug (1, "[%s] Unhold call idx %d\n", PVT_ID(pvt), cpvt->call_idx);
					channel_queue_control (cpvt, AST_CONTROL_UNHOLD);
				}
				else if (cpvt->dir == CALL_DIR_OUTGOING)
				{
					ast_debug (1, "[%s] Remote end answered on call idx %d\n", PVT_ID(pvt), cpvt->call_idx);
					channel_queue_control (cpvt, AST_CONTROL_ANSWER);
				}
				else /* if (cpvt->answered) */
				{
					ast_debug (1, "[%s] Call idx %d answer\n", PVT_ID(pvt), cpvt->call_idx);
					ast_setstate (cpvt->channel, AST_STATE_UP);
				}
				break;

			case CALL_STATE_ONHOLD:
				channel_disactivate(cpvt);
				ast_debug (1, "[%s] Hold call idx %d\n", PVT_ID(pvt), cpvt->call_idx);
				channel_queue_control (cpvt, AST_CONTROL_HOLD);
				break;

			case CALL_STATE_RELEASED:
				channel_disactivate(cpvt);
				/* from +CEND: */
//				ast_debug (1, "[%s] hanging up channel for call idx %d\n", PVT_ID(pvt), cpvt->call_idx);

				if (channel_queue_hangup (cpvt, cause))
				{
					ast_log (LOG_ERROR, "[%s] Error queueing hangup...\n", PVT_ID(pvt));
				}
				/* drop channel -> cpvt reference */
				cpvt->channel->tech_pvt = NULL;
				cpvt_free(cpvt);

				break;
		}
	}
}

/* NOTE: called from device and current levels with locked pvt */
EXPORT_DEF struct ast_channel* channel_new (struct pvt* pvt, int ast_state, const char* cid_num, int call_idx, unsigned dir, call_state_t state)
{
	struct ast_channel* channel;
	struct cpvt * cpvt;

	cpvt = cpvt_alloc(pvt, call_idx, dir, state);
	if (cpvt)
	{
#if ASTERISK_VERSION_NUM >= 10800
		channel = ast_channel_alloc (1, ast_state, cid_num, PVT_ID(pvt), NULL, 0, CONF_SHARED(pvt, context), 0, 0, "Datacard/%s-%02u%08lx", PVT_ID(pvt), call_idx, pvt->channel_instanse);
#else
		channel = ast_channel_alloc (1, ast_state, cid_num, PVT_ID(pvt), 0, 0, CONF_SHARED(pvt, context), 0, "Datacard/%s-%02u%08lx", PVT_ID(pvt), call_idx, pvt->channel_instanse);
#endif
		if (channel)
		{
			cpvt->channel = channel;
			pvt->channel_instanse++;

			channel->tech_pvt	= cpvt;
			channel->tech		= &channel_tech;
			channel->nativeformats	= AST_FORMAT_SLINEAR;
			channel->writeformat	= AST_FORMAT_SLINEAR;
			channel->readformat	= AST_FORMAT_SLINEAR;

			if (ast_state == AST_STATE_RING)
			{
				channel->rings = 1;
			}

			pbx_builtin_setvar_helper (channel, "DATACARD",		PVT_ID(pvt));
			pbx_builtin_setvar_helper (channel, "PROVIDER",		pvt->provider_name);
			pbx_builtin_setvar_helper (channel, "IMEI",		pvt->imei);
			pbx_builtin_setvar_helper (channel, "IMSI",		pvt->imsi);
			pbx_builtin_setvar_helper (channel, "CNUMBER",		pvt->number);

			ast_string_field_set (channel, language, CONF_SHARED(pvt, language));
			ast_jb_configure (channel, &CONF_GLOBAL(jbconf));

			ast_module_ref (self_module());

			return channel;
		}
		cpvt_free(cpvt);
	}
	return NULL;
}

/* NOTE: bg: hmm ast_queue_control() say no need channel lock, trylock got deadlock up to 30 seconds here */
/* NOTE: called from device and current levels with pvt locked */
EXPORT_DEF int channel_queue_control (struct cpvt * cpvt, enum ast_control_frame_type control)
{
/*
	for (;;)
	{
*/
		if (cpvt->channel)
		{
/*
			if (ast_channel_trylock (cpvt->channel))
			{
				DEADLOCK_AVOIDANCE (&cpvt->pvt->lock);
			}
			else
			{
*/
				ast_queue_control (cpvt->channel, control);
/*
				ast_channel_unlock (cpvt->channel);
				break;
			}
*/
		}
/*
		else
		{
			break;
		}
	}
*/
	return 0;
}

/* NOTE: bg: hmm ast_queue_hangup() say no need channel lock before call, trylock got deadlock up to 30 seconds here */
/* NOTE: bg: called from device level and channel_change_state() with pvt locked */
EXPORT_DEF int channel_queue_hangup (struct cpvt * cpvt, int hangupcause)
{
/*
	for (;;)
	{
*/
		if (cpvt->channel)
		{
/*
			if (ast_channel_trylock (cpvt->channel))
			{
				DEADLOCK_AVOIDANCE (&cpvt->pvt->lock);
			}
			else
			{
*/
				if (hangupcause != 0)
				{
					cpvt->channel->hangupcause = hangupcause;
				}

				ast_queue_hangup (cpvt->channel);
/*
				ast_channel_unlock (cpvt->channel);

				break;
			}
*/
		}
/*
		else
		{
			break;
		}
	}
*/
	return 0;
}

/* NOTE: bg: called from device level with pvt locked */
EXPORT_DEF struct ast_channel* channel_local_request (struct pvt* pvt, void* data, const char* cid_name, const char* cid_num)
{
	struct ast_channel*	channel;
	int			cause = 0;

#if ASTERISK_VERSION_NUM >= 10800
	if (!(channel = ast_request ("Local", AST_FORMAT_AUDIO_MASK, NULL, data, &cause)))
#else
	if (!(channel = ast_request ("Local", AST_FORMAT_AUDIO_MASK, data, &cause)))
#endif
	{
		ast_log (LOG_ERROR, "Unable to request channel Local/%s\n", (char*) data);
		return channel;
	}

	ast_set_callerid (channel, cid_num, cid_name, cid_num);
	pbx_builtin_setvar_helper (channel, "DATACARD",	PVT_ID(pvt));
	pbx_builtin_setvar_helper (channel, "PROVIDER",	pvt->provider_name);
	pbx_builtin_setvar_helper (channel, "IMEI",	pvt->imei);
	pbx_builtin_setvar_helper (channel, "IMSI",	pvt->imsi);
	pbx_builtin_setvar_helper (channel, "CNUMBER", pvt->number);

	ast_string_field_set (channel, language, CONF_SHARED(pvt, language));

	return channel;
}

#/* */
static int channel_func_read(struct ast_channel* channel, attribute_unused const char* function, char* data, char* buf, size_t len)
{
	struct cpvt* cpvt = channel->tech_pvt;
	struct pvt* pvt;
	int ret = 0;

	if(!cpvt || !cpvt->pvt)
	{
		ast_log (LOG_WARNING, "call on unreferenced %s\n", channel->name);
		return -1;
	}
	pvt = cpvt->pvt;

	if (!strcasecmp(data, "callstate"))
	{
		while (ast_mutex_trylock (&pvt->lock))
		{
			CHANNEL_DEADLOCK_AVOIDANCE (channel);
		}
		call_state_t state = cpvt->state;
		ast_mutex_unlock(&pvt->lock);

		ast_copy_string(buf, call_state2str(state), len);
	}
/*
	else if (!strcasecmp(data, "calls"))
	{
		char buffer[20];
		while (ast_mutex_trylock (&pvt->lock))
		{
			CHANNEL_DEADLOCK_AVOIDANCE (channel);
		}
		unsigned calls = pvt->chansno;
		ast_mutex_unlock(&pvt->lock);

		snprintf(buffer, sizeof(buffer), "%u", calls);
		ast_copy_string(buf, buffer, len);
	}
*/
	else
		ret = -1;

	return ret;
}

#/* */
static int channel_func_write(struct ast_channel* channel, const char* function, char* data, const char* value)
{
	struct cpvt* cpvt = channel->tech_pvt;
	struct pvt* pvt;
	call_state_t newstate, oldstate;
	int ret = 0;

	if(!cpvt || !cpvt->pvt)
	{
		ast_log (LOG_WARNING, "call on unreferenced %s\n", channel->name);
		return -1;
	}
	pvt = cpvt->pvt;

	if (!strcasecmp(data, "callstate"))
	{
		if (!strcasecmp(value, "active"))
		{
			newstate = CALL_STATE_ACTIVE;
		}
		else
		{
			ast_log(LOG_WARNING, "Invalid value for %s(callstate).", function);
			return -1;
		}
		
		while (ast_mutex_trylock (&cpvt->pvt->lock))
		{
			CHANNEL_DEADLOCK_AVOIDANCE (channel);
		}
		oldstate = cpvt->state;
		
		if (oldstate == newstate)
			;
		else if (oldstate == CALL_STATE_ONHOLD)
		{
			if(at_enque_activate(cpvt))
			{
				// TODO: handle error
				ast_log(LOG_ERROR, "Error state to active for call idx %d in %s(callstate).", cpvt->call_idx, function);
			}
		}
		else
		{
			ast_log(LOG_WARNING, "allow change state to 'active' only from 'held' in %s(callstate).", function);
			ret = -1;
		}
		ast_mutex_unlock(&cpvt->pvt->lock);
	}
	else
		ret = -1;

	return ret;
}

EXPORT_DEF const struct ast_channel_tech channel_tech =
{
	.type			= "Datacard",
	.description		= "Datacard Channel Driver",
	.capabilities		= AST_FORMAT_SLINEAR,
	.requester		= channel_request,
	.call			= channel_call,
	.hangup			= channel_hangup,
	.answer			= channel_answer,
	.send_digit_begin	= channel_digit_begin,
	.send_digit_end		= channel_digit_end,
	.read			= channel_read,
	.write			= channel_write,
	.exception		= channel_read,
	.fixup			= channel_fixup,
	.devicestate		= channel_devicestate,
	.indicate		= channel_indicate,
	.func_channel_read	= channel_func_read,
	.func_channel_write	= channel_func_write
};
