#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef BUILD_MANAGER /* no manager, no copyright */
/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/

#include <asterisk.h>
#include <asterisk/stringfields.h>		/* AST_DECLARE_STRING_FIELDS for asterisk/manager.h */
#include <asterisk/manager.h>			/* struct mansession, struct message ... */
#include <asterisk/strings.h>			/* ast_strlen_zero() */
#include <asterisk/callerid.h>			/* ast_describe_caller_presentation */

#include "chan_datacard.h"			/* devices */
#include "helpers.h"				/* ITEMS_OF() send_ccwa_set() send_reset() send_sms() send_ussd() */

static int manager_show_devices (struct mansession* s, const struct message* m)
{
	const char * id = astman_get_header (m, "ActionID");
	const char * device = astman_get_header (m, "Device");
	struct pvt * pvt;
	size_t count = 0;
	char buf[40];

	astman_send_listack (s, m, "Device status list will follow", "start");

	AST_RWLIST_RDLOCK (&gpublic->devices);
	AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
	{
		ast_mutex_lock (&pvt->lock);
		if(ast_strlen_zero(device) || strcmp(device, PVT_ID(pvt)) == 0)
		{
			astman_append (s, "Event: DatacardDeviceEntry\r\n");
			if(!ast_strlen_zero (id))
				astman_append (s, "ActionID: %s\r\n", id);
			astman_append (s, "Device: %s\r\n", PVT_ID(pvt));
/* settings */
			astman_append (s, "AudioSetting: %s\r\n", CONF_UNIQ(pvt, audio_tty));
			astman_append (s, "DataSetting: %s\r\n", CONF_UNIQ(pvt, data_tty));
			astman_append (s, "IMEISetting: %s\r\n", CONF_UNIQ(pvt, imei));
			astman_append (s, "IMSISetting: %s\r\n", CONF_UNIQ(pvt, imsi));
			astman_append (s, "ChannelLanguage: %s\r\n", CONF_SHARED(pvt, language));
			astman_append (s, "Context: %s\r\n", CONF_SHARED(pvt, context));
			astman_append (s, "Exten: %s\r\n", CONF_SHARED(pvt, exten));
			astman_append (s, "Group: %d\r\n", CONF_SHARED(pvt, group));
			astman_append (s, "RXGain: %d\r\n", CONF_SHARED(pvt, rxgain));
			astman_append (s, "TXGain: %d\r\n", CONF_SHARED(pvt, txgain));
			astman_append (s, "U2DIAG: %d\r\n", CONF_SHARED(pvt, u2diag));
			astman_append (s, "UseCallingPres: %s\r\n", CONF_SHARED(pvt, usecallingpres) ? "Yes" : "No");
			astman_append (s, "DefaultCallingPres: %s\r\n", CONF_SHARED(pvt, callingpres) < 0 ? "<Not set>" : ast_describe_caller_presentation (CONF_SHARED(pvt, callingpres)));
			astman_append (s, "AutoDeleteSMS: %s\r\n", CONF_SHARED(pvt, autodeletesms) ? "Yes" : "No");
			astman_append (s, "DisableSMS: %s\r\n", CONF_SHARED(pvt, disablesms) ? "Yes" : "No");
			astman_append (s, "ResetDatacard: %s\r\n", CONF_SHARED(pvt, resetdatacard) ? "Yes" : "No");
			astman_append (s, "SMSPDU: %s\r\n", CONF_SHARED(pvt, smsaspdu) ? "Yes" : "No");
			astman_append (s, "CallWaitingSetting: %s\r\n", dc_cw_setting2str(CONF_SHARED(pvt, callwaiting)));
			astman_append (s, "DTMF: %s\r\n", dc_dtmf_setting2str(CONF_SHARED(pvt, dtmf)));
			astman_append (s, "MinimalDTMFGap: %d\r\n", CONF_SHARED(pvt, mindtmfgap));
			astman_append (s, "MinimalDTMFDuration: %d\r\n", CONF_SHARED(pvt, mindtmfduration));
			astman_append (s, "MinimalDTMFInterval: %d\r\n", CONF_SHARED(pvt, mindtmfinterval));
/* state */
			astman_append (s, "State: %s\r\n", pvt_str_state(pvt));
			astman_append (s, "AudioState: %s\r\n", PVT_STATE(pvt, audio_tty));
			astman_append (s, "DataState: %s\r\n", PVT_STATE(pvt, data_tty));
			astman_append (s, "Voice: %s\r\n", (pvt->has_voice) ? "Yes" : "No");
			astman_append (s, "SMS: %s\r\n", (pvt->has_sms) ? "Yes" : "No");
			astman_append (s, "Manufacturer: %s\r\n", pvt->manufacturer);
			astman_append (s, "Model: %s\r\n", pvt->model);
			astman_append (s, "Firmware: %s\r\n", pvt->firmware);
			astman_append (s, "IMEIState: %s\r\n", pvt->imei);
			astman_append (s, "IMSIState: %s\r\n", pvt->imsi);
			astman_append (s, "GSMRegistrationStatus: %s\r\n", GSM_regstate2str(pvt->gsm_reg_status));
			astman_append (s, "RSSI: %d, %s\r\n", pvt->rssi, rssi2dBm(pvt->rssi, buf, sizeof(buf)));
			astman_append (s, "Mode: %s\r\n", sys_mode2str(pvt->linkmode));
			astman_append (s, "Submode: %s\r\n", sys_submode2str(pvt->linksubmode));
			astman_append (s, "ProviderName: %s\r\n", pvt->provider_name);
			astman_append (s, "LocationAreaCode: %s\r\n", pvt->location_area_code);
			astman_append (s, "CellID: %s\r\n", pvt->cell_id);
			astman_append (s, "SubscriberNumber: %s\r\n", pvt->subscriber_number);
			astman_append (s, "SMSServiceCenter: %s\r\n", pvt->sms_scenter);
			astman_append (s, "UseUCS2Encoding: %s\r\n", pvt->use_ucs2_encoding ? "Yes" : "No");
			astman_append (s, "USSDUse7BitEncoding: %s\r\n", pvt->cusd_use_7bit_encoding ? "Yes" : "No");
			astman_append (s, "USSDUseUCS2Decoding: %s\r\n", pvt->cusd_use_ucs2_decoding ? "Yes" : "No");
			astman_append (s, "TasksInQueue: %u\r\n", PVT_STATE(pvt, at_tasks));
			astman_append (s, "CommandsInQueue: %u\r\n", PVT_STATE(pvt, at_cmds));
			astman_append (s, "CallWaitingState: %s\r\n", pvt->has_call_waiting ? "Enabled" : "Disabled");
			astman_append (s, "CurrentDeviceState: %s\r\n", dev_state2str(pvt->current_state));
			astman_append (s, "DesiredDeviceState: %s\r\n", dev_state2str(pvt->desired_state));
			astman_append (s, "CallsChannels: %u\r\n", PVT_STATE(pvt, chansno));
			astman_append (s, "Active: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_ACTIVE]));
			astman_append (s, "Held: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_ONHOLD]));
			astman_append (s, "Dialing: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_DIALING]));
			astman_append (s, "Alerting: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_ALERTING]));
			astman_append (s, "Incoming: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_INCOMING]));
			astman_append (s, "Waiting: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_WAITING]));
			astman_append (s, "Releasing: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_RELEASED]));
			astman_append (s, "Initializing: %u\r\n", PVT_STATE(pvt, chan_count[CALL_STATE_INIT]));
/* TODO: stats */

			astman_append (s, "\r\n");
			count++;
		}
		ast_mutex_unlock (&pvt->lock);
	}
	AST_RWLIST_UNLOCK (&gpublic->devices);

	astman_append (s, "Event: DatacardShowDevicesComplete\r\n");
	if(!ast_strlen_zero (id))
		astman_append (s, "ActionID: %s\r\n", id);
	astman_append (s, 
		"EventList: Complete\r\n"
		"ListItems: %zu\r\n"
		"\r\n",
		count
	);

	return 0;
}

static int manager_send_ussd (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	ussd	= astman_get_header (m, "USSD");

	char		buf[256];
	const char*	msg;
	int		status;
	void * msgid = NULL;

	if (ast_strlen_zero (device))
	{
		astman_send_error (s, m, "Device not specified");
		return 0;
	}

	if (ast_strlen_zero (ussd))
	{
		astman_send_error (s, m, "USSD not specified");
		return 0;
	}

	msg = send_ussd(device, ussd, &status, &msgid);
	snprintf(buf, sizeof (buf), "[%s] %s\r\nID: %p", device, msg, msgid);
	if(status)
	{
		astman_send_ack(s, m, buf);
	}
	else
	{
		astman_send_error(s, m, buf);
	}

	return 0;
}

static int manager_send_sms (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	number	= astman_get_header (m, "Number");
	const char*	message	= astman_get_header (m, "Message");
	const char*	validity= astman_get_header (m, "Validity");
	const char*	report	= astman_get_header (m, "Report");

	char		buf[256];
	const char*	msg;
	int		status;
	void * msgid;
	
	if (ast_strlen_zero (device))
	{
		astman_send_error (s, m, "Device not specified");
		return 0;
	}

	if (ast_strlen_zero (number))
	{
		astman_send_error (s, m, "Number not specified");
		return 0;
	}

	if (ast_strlen_zero (message))
	{
		astman_send_error (s, m, "Message not specified");
		return 0;
	}

	msg = send_sms(device, number, message, validity, report, &status, &msgid);
	snprintf (buf, sizeof (buf), "[%s] %s\r\nID: %p", device, msg, msgid);
	if(status)
	{
		astman_send_ack(s, m, buf);
	}
	else
	{
		astman_send_error(s, m, buf);
	}

	return 0;
}

#/* */
EXPORT_DEF void manager_event_sent_notify(const char * devname, const char * type, const void * id, const char * result)
{
	char buf[40];
	snprintf(buf, sizeof(buf), "Datacard%sStatus", type);

	manager_event (EVENT_FLAG_CALL, buf,
		"Device: %s\r\n"
		"ID: %p\r\n"
		"Status: %s\r\n",
		devname,
		id,
		result
	);
}

/*!
 * \brief Send a DatacardNewUSSD event to the manager
 * This function splits the message in multiple lines, so multi-line
 * USSD messages can be send over the manager API.
 * \param pvt a pvt structure
 * \param message a null terminated buffer containing the message
 */

EXPORT_DEF void manager_event_new_ussd (const char * devname, char* message)
{
	struct ast_str*	buf;
	char*		s = message;
	char*		sl;
	size_t		linecount = 0;

	buf = ast_str_create (256);

	while ((sl = strsep (&s, "\r\n")))
	{
		if (*sl != '\0')
		{
			ast_str_append (&buf, 0, "MessageLine%zu: %s\r\n", linecount, sl);
			linecount++;
		}
	}

	manager_event (EVENT_FLAG_CALL, "DatacardNewUSSD",
		"Device: %s\r\n"
		"LineCount: %zu\r\n"
/* FIXME: empty lines inserted */
//		"%s\r\n",
		"%s",
		devname, linecount, ast_str_buffer (buf)
	);

	ast_free (buf);
}


#/* */
EXPORT_DEF void manager_event_message(const char * event, const char * devname, const char * message)
{
	manager_event (EVENT_FLAG_CALL, event,
		"Device: %s\r\n"
		"Message: %s\r\n",
		devname, 
		message
	);
}

#/* */
EXPORT_DEF void manager_event_cend(const char * devname, int call_index, int duration, int end_status, int cc_cause)
{
	manager_event( EVENT_FLAG_CALL, "DatacardCEND",
		"Device: %s\r\n"
		"CallIdx: %d\r\n"
		"Duration: %d\r\n"
		"EndStatus: %d\r\n"
		"CCCause: %d\r\n",
		devname,
		call_index,
		duration,
		end_status,
		cc_cause
		);
}

#/* */
EXPORT_DEF void manager_event_call_state_change(const char * devname, int call_index, const char * newstate)
{
	manager_event(EVENT_FLAG_CALL, "DatacardCallStateChange",
		"Device: %s\r\n"
		"CallIdx: %d\r\n"
		"NewState: %s\r\n",
		devname,
		call_index,
		newstate
		);
}

#/* */
EXPORT_DEF void manager_event_device_status(const char * devname, const char * newstate)
{
	manager_event(EVENT_FLAG_CALL, "DatacardStatus",
		"Device: %s\r\n"
		"Status: %s\r\n",
		devname,
		newstate
		);
}


/*!
 * \brief Send a DatacardNewSMS event to the manager
 * This function splits the message in multiple lines, so multi-line
 * SMS messages can be send over the manager API.
 * \param pvt a pvt structure
 * \param number a null terminated buffer containing the from number
 * \param message a null terminated buffer containing the message
 */

EXPORT_DEF void manager_event_new_sms (const char * devname, char* number, char* message)
{
	struct ast_str* buf;
	size_t	linecount = 0;
	char*	s = message;
	char*	sl;

	buf = ast_str_create (256);

	while ((sl = strsep (&s, "\r\n")))
	{
		if (*sl != '\0')
		{
			ast_str_append (&buf, 0, "MessageLine%zu: %s\r\n", linecount, sl);
			linecount++;
		}
	}

	manager_event (EVENT_FLAG_CALL, "DatacardNewSMS",
		"Device: %s\r\n"
		"From: %s\r\n"
		"LineCount: %zu\r\n"
		"%s\r\n",
		devname, number, linecount, ast_str_buffer (buf)
	);
	ast_free (buf);
}

/*!
 * \brief Send a DatacardNewSMSBase64 event to the manager
 * \param pvt a pvt structure
 * \param number a null terminated buffer containing the from number
 * \param message_base64 a null terminated buffer containing the base64 encoded message
 */

EXPORT_DEF void manager_event_new_sms_base64 (const char * devname, char * number, char * message_base64)
{
	manager_event (EVENT_FLAG_CALL, "DatacardNewSMSBase64",
		"Device: %s\r\n"
		"From: %s\r\n"
		"Message: %s\r\n",
		devname, number, message_base64
	);
}

static int manager_ccwa_set (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	value	= astman_get_header (m, "Value");
	const char*	id	= astman_get_header (m, "ActionID");

	char		buf[256];
	const char*	msg;
	int		status;
	call_waiting_t	enable;

	if (ast_strlen_zero (device))
	{
		astman_send_error (s, m, "Device not specified");
		return 0;
	}

	if (strcmp("enable", value) == 0)
		enable = CALL_WAITING_ALLOWED;
	else if (strcmp("disable", value) == 0)
		enable = CALL_WAITING_DISALLOWED;
	else
	{
		astman_send_error (s, m, "Invalid Value");
		return 0;
	}

	msg = send_ccwa_set(device, enable, &status);
	snprintf (buf, sizeof (buf), "[%s] %s", device, msg);
	(status ? astman_send_ack : astman_send_error)(s, m, buf);

	if(!ast_strlen_zero(id))
		astman_append (s, "ActionID: %s\r\n", id);

	return 0;
}

static int manager_reset (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	id	= astman_get_header (m, "ActionID");

	char		buf[256];
	const char*	msg;
	int		status;

	if (ast_strlen_zero (device))
	{
		astman_send_error (s, m, "Device not specified");
		return 0;
	}

	msg = send_reset(device, &status);
	snprintf (buf, sizeof (buf), "[%s] %s", device, msg);
	(status ? astman_send_ack : astman_send_error)(s, m, buf);

	if(!ast_strlen_zero(id))
		astman_append (s, "ActionID: %s\r\n", id);

	return 0;
}

static const char * const b_choices[] = { "now", "gracefully", "when convenient" };
#/* */
static int manager_restart_action(struct mansession * s, const struct message * m, dev_state_t event)
{

	const char * device = astman_get_header (m, "Device");
	const char * when = astman_get_header (m, "When");
	const char * id = astman_get_header (m, "ActionID");

	char buf[256];
	const char * msg;
	int status;
	unsigned i;

	if (ast_strlen_zero (device))
	{
		astman_send_error (s, m, "Device not specified");
		return 0;
	}

	for(i = 0; i < ITEMS_OF(b_choices); i++)
	{
		if(event == DEV_STATE_STARTED || strcasecmp(when, b_choices[i]) == 0)
		{
			msg = schedule_restart_event(event, i, device, &status);
			snprintf (buf, sizeof (buf), "[%s] %s", device, msg);
			(status ? astman_send_ack : astman_send_error)(s, m, buf);
			if(!ast_strlen_zero(id))
				astman_append (s, "ActionID: %s\r\n", id);
			return 0;
		}
	}

	astman_send_error (s, m, "Invalid value of When");
	if(!ast_strlen_zero(id))
		astman_append (s, "ActionID: %s\r\n", id);
	return 0;
}

#/* */
static int manager_restart(struct mansession * s, const struct message * m)
{
	return manager_restart_action(s, m, DEV_STATE_RESTARTED);
}

#/* */
static int manager_stop(struct mansession * s, const struct message * m)
{
	return manager_restart_action(s, m, DEV_STATE_STOPPED);
}

#/* */
static int manager_start(struct mansession * s, const struct message * m)
{
	return manager_restart_action(s, m, DEV_STATE_STARTED);
}

#/* */
static int manager_remove(struct mansession * s, const struct message * m)
{
	return manager_restart_action(s, m, DEV_STATE_REMOVED);
}

#/* */
static int manager_reload(struct mansession * s, const struct message * m)
{
	const char * when = astman_get_header (m, "When");
	const char * id = astman_get_header (m, "ActionID");

	unsigned i;

	for(i = 0; i < ITEMS_OF(b_choices); i++)
	{
		if(strcasecmp(when, b_choices[i]) == 0)
		{
			pvt_reload(i);
			astman_send_ack(s, m, "reload scheduled");
			if(!ast_strlen_zero(id))
				astman_append (s, "ActionID: %s\r\n", id);
			return 0;
		}
	}

	astman_send_error (s, m, "Invalid value of When");
	if(!ast_strlen_zero(id))
		astman_append (s, "ActionID: %s\r\n", id);
	return 0;
}

static const struct datacard_manager
{
	int		(*func)(struct mansession* s, const struct message* m);
	const char*	name;
	const char*	brief;
	const char*	desc;
} dcm[] = 
{
	{
	manager_show_devices, 
	"DatacardShowDevices", 
	"List Datacard devices", 
	"Description: Lists Datacard devices in text format with details on current status.\n\n"
	"DatacardShowDevicesComplete.\n"
	"Variables:\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	Device:   <name>	Optional name of device.\n"
	},
	{
	manager_send_ussd, 
	"DatacardSendUSSD", 
	"Send a ussd command to the datacard.",
	"Description: Send a ussd message to a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard to which the ussd code will be send.\n"
	"	*USSD:    <code>	The ussd code that will be send to the device.\n"
	 },
	{
	manager_send_sms, 
	"DatacardSendSMS", 
	"Send a sms message.", 
	"Description: Send a sms message from a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard to which the sms be send.\n"
	"	*Number:  <number>	The phone number to which the sms will be send.\n"
	"	*Message: <message>	The sms message that will be send.\n"
	},
	{
	manager_ccwa_set, 
	"DatacardSetCCWA", 
	"Enable/Disabled Call-Waiting on a datacard.",
	"Description: Enable/Disabled Call-Waiting on a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard to which the command be send.\n"
	"	*Value:   <enable|disable> Value of Call Waiting\n"
	 },
	{
	manager_reset,
	"DatacardReset", 
	"Reset a datacard.", 
	"Description: Reset a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard which should be reset.\n"
	},
	{
	manager_restart,
	"DatacardRestart", 
	"Restart a datacard.", 
	"Description: Restart a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard which should be restart.\n"
	"	*When:    < now | gracefully | when convenient > Time when device restarted.\n"
	},
	{
	manager_stop,
	"DatacardStop", 
	"Stop a datacard.", 
	"Description: Stop a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard which should be stopped.\n"
	"	*When:    < now | gracefully | when convenient > Time when device stopped.\n"
	},
	{
	manager_start,
	"DatacardStart", 
	"Start a datacard.", 
	"Description: Start a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard which should be started.\n"
	},
	{
	manager_remove,
	"DatacardRemove", 
	"Remove a datacard.", 
	"Description: Remove a datacard.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*Device:  <device>	The datacard which should be removed.\n"
	"	*When:    < now | gracefully | when convenient > Time when device removed.\n"
	},
	{
	manager_reload,
	"DatacardReload", 
	"Reload a module configuration.", 
	"Description: Reload the module configuration.\n\n"
	"Variables: (Names marked with * are required)\n"
	"	ActionID: <id>		Action ID for this transaction. Will be returned.\n"
	"	*When:    < now | gracefully | when convenient > Time when devices reconfigured.\n"
	},
};

EXPORT_DEF void manager_register()
{
	unsigned i;
	for(i = 0; i < ITEMS_OF(dcm); i++)
	{
		ast_manager_register2 (dcm[i].name, EVENT_FLAG_SYSTEM | EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING, dcm[i].func, dcm[i].brief, dcm[i].desc);
	}
}

EXPORT_DEF void manager_unregister()
{
	int i;
	for(i = ITEMS_OF(dcm)-1; i >= 0; i--)
	{
		ast_manager_unregister((char*)dcm[i].name);
	}
}

#endif /* BUILD_MANAGER */
