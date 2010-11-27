#ifdef __MANAGER__ /* no manager, no copyright */
/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>
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
	const char*	id = astman_get_header (m, "ActionID");
	char		idtext[256] = "";
	struct pvt*		pvt;
	size_t		count = 0;

	if (!ast_strlen_zero (id))
	{
		snprintf (idtext, sizeof (idtext), "ActionID: %s\r\n", id);
	}

	astman_send_listack (s, m, "Device status list will follow", "start");

	AST_RWLIST_RDLOCK (&gpublic->devices);
	AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
	{
		ast_mutex_lock (&pvt->lock);
		astman_append (s, "Event: DatacardDeviceEntry\r\n%s", idtext);
		astman_append (s, "Device: %s\r\n", PVT_ID(pvt));
		astman_append (s, "Group: %d\r\n", CONF_SHARED(pvt, group));
		astman_append (s, "GSM Registration Status: %s\r\n", GSM_regstate2str(pvt->gsm_reg_status));
		astman_append (s, "State: %s\r\n", pvt_str_state(pvt));
		astman_append (s, "Voice: %s\r\n", (pvt->has_voice) ? "Yes" : "No");
		astman_append (s, "SMS: %s\r\n", (pvt->has_sms) ? "Yes" : "No");
		astman_append (s, "RSSI: %d\r\n", pvt->rssi);
		astman_append (s, "Mode: %s\r\n", sys_mode2str(pvt->linkmode));
		astman_append (s, "Submode: %s\r\n", sys_submode2str(pvt->linksubmode));
		astman_append (s, "ProviderName: %s\r\n", pvt->provider_name);
		astman_append (s, "Manufacturer: %s\r\n", pvt->manufacturer);
		astman_append (s, "Model: %s\r\n", pvt->model);
		astman_append (s, "Firmware: %s\r\n", pvt->firmware);
		astman_append (s, "IMEI: %s\r\n", pvt->imei);
		astman_append (s, "IMSI: %s\r\n", pvt->imsi);
		astman_append (s, "Number: %s\r\n", pvt->number);
		astman_append (s, "SMS Service Center: %s\r\n", pvt->sms_scenter);
		astman_append (s, "Use CallingPres: %s\r\n", CONF_SHARED(pvt, usecallingpres) ? "Yes" : "No");
		astman_append (s, "Default CallingPres: %s\r\n", CONF_SHARED(pvt, callingpres) < 0 ? "<Not set>" : ast_describe_caller_presentation (CONF_SHARED(pvt, callingpres)));
		astman_append (s, "Use UCS-2 encoding: %s\r\n", pvt->use_ucs2_encoding ? "Yes" : "No");
		astman_append (s, "USSD use 7 bit encoding: %s\r\n", pvt->cusd_use_7bit_encoding ? "Yes" : "No");
		astman_append (s, "USSD use UCS-2 decoding: %s\r\n", pvt->cusd_use_ucs2_decoding ? "Yes" : "No");
		astman_append (s, "Location area code: %s\r\n", pvt->location_area_code);
		astman_append (s, "Cell ID: %s\r\n", pvt->cell_id);
		astman_append (s, "Auto delete SMS: %s\r\n", CONF_SHARED(pvt, auto_delete_sms) ? "Yes" : "No");
		astman_append (s, "Send SMS as PDU: %s\r\n", CONF_SHARED(pvt, smsaspdu) ? "Yes" : "No");
		astman_append (s, "Disable SMS: %s\r\n", CONF_SHARED(pvt, disablesms) ? "Yes" : "No");
		astman_append (s, "Channel Language: %s\r\n", CONF_SHARED(pvt, language));
		astman_append (s, "Minimal DTMF Gap: %d\r\n", CONF_SHARED(pvt, mindtmfgap));
		astman_append (s, "Minimal DTMF Duration: %d\r\n", CONF_SHARED(pvt, mindtmfduration));
		astman_append (s, "Minimal DTMF Interval: %d\r\n", CONF_SHARED(pvt, mindtmfinterval));
		astman_append (s, "Call Waiting: %s\r\n", pvt->has_call_waiting ? "Enabled" : "Disabled");
//		astman_append (s, "Tasks in Queue: %u\r\n", pvt->at_tasks);
//		astman_append (s, "Commands in Queue: %u\r\n", pvt->at_cmds);
		astman_append (s, "\r\n");
		ast_mutex_unlock (&pvt->lock);
		count++;
	}
	AST_RWLIST_UNLOCK (&gpublic->devices);

	astman_append (s,
		"Event: DatacardShowDevicesComplete\r\n%s"
		"EventList: Complete\r\n"
		"ListItems: %zu\r\n"
		"\r\n",
		idtext, count
	);

	return 0;
}

static int manager_send_ussd (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	ussd	= astman_get_header (m, "USSD");
	const char*	id	= astman_get_header (m, "ActionID");

	char		idtext[256] = "";
	char		buf[256];
	const char*	msg;
	int		status;

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

	if (!ast_strlen_zero (id))
	{
		snprintf (idtext, sizeof (idtext), "ActionID: %s\r\n", id);
	}

	msg = send_ussd(device, ussd, &status);
	snprintf (buf, sizeof (buf), "[%s] %s", device, msg);
	(status ? astman_send_ack : astman_send_error)(s, m, buf);


	return 0;
}

static int manager_send_sms (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	number	= astman_get_header (m, "Number");
	const char*	message	= astman_get_header (m, "Message");
	const char*	id	= astman_get_header (m, "ActionID");

	char		idtext[256] = "";
	char		buf[256];
	const char*	msg;
	int		status;
	
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

	if (!ast_strlen_zero (id))
	{
		snprintf (idtext, sizeof(idtext), "ActionID: %s\r\n", id);
	}
	
	msg = send_sms(device, number, message, &status);
	snprintf (buf, sizeof (buf), "[%s] %s", device, msg);
	(status ? astman_send_ack : astman_send_error)(s, m, buf);

	return 0;
}

/*!
 * \brief Send a DatacardNewUSSD event to the manager
 * This function splits the message in multiple lines, so multi-line
 * USSD messages can be send over the manager API.
 * \param pvt a pvt structure
 * \param message a null terminated buffer containing the message
 */

EXPORT_DEF void manager_event_new_ussd (struct pvt* pvt, char* message)
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
		"%s\r\n",
		PVT_ID(pvt), linecount, ast_str_buffer (buf)
	);

	ast_free (buf);
}

/*!
 * \brief Send a DatacardNewUSSD event to the manager
 * This function splits the message in multiple lines, so multi-line
 * USSD messages can be send over the manager API.
 * \param pvt a pvt structure
 * \param message a null terminated buffer containing the message
 */

EXPORT_DEF void manager_event_new_ussd_base64 (struct pvt* pvt, char* message)
{
        manager_event (EVENT_FLAG_CALL, "DatacardNewUSSDBase64",
                "Device: %s\r\n"
                "Message: %s\r\n",
                PVT_ID(pvt), message
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

EXPORT_DEF void manager_event_new_sms (struct pvt* pvt, char* number, char* message)
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
		PVT_ID(pvt), number, linecount, ast_str_buffer (buf)
	);

	ast_free (buf);
}

/*!
 * \brief Send a DatacardNewSMSBase64 event to the manager
 * \param pvt a pvt structure
 * \param number a null terminated buffer containing the from number
 * \param message_base64 a null terminated buffer containing the base64 encoded message
 */

EXPORT_DEF void manager_event_new_sms_base64 (struct pvt* pvt, char* number, char* message_base64)
{
	manager_event (EVENT_FLAG_CALL, "DatacardNewSMSBase64",
		"Device: %s\r\n"
		"From: %s\r\n"
		"Message: %s\r\n",
		PVT_ID(pvt), number, message_base64
	);
}

static int manager_ccwa_set (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	value	= astman_get_header (m, "Value");
	const char*	id	= astman_get_header (m, "ActionID");

	char		idtext[256] = "";
	char		buf[256];
	const char*	msg;
	int		status;
	int		enable;

	if (ast_strlen_zero (device))
	{
		astman_send_error (s, m, "Device not specified");
		return 0;
	}

	if (strcmp("enable", value) == 0)
		enable = 1;
	else if (strcmp("disable", value) == 0)
		enable = 0;
	else
	{
		astman_send_error (s, m, "Invalid Value");
		return 0;
	}

	if (!ast_strlen_zero (id))
	{
		snprintf (idtext, sizeof (idtext), "ActionID: %s\r\n", id);
	}


	msg = send_ccwa_set(device, enable, &status);
	snprintf (buf, sizeof (buf), "[%s] %s", device, msg);
	(status ? astman_send_ack : astman_send_error)(s, m, buf);

	return 0;
}

static int manager_reset (struct mansession* s, const struct message* m)
{
	const char*	device	= astman_get_header (m, "Device");
	const char*	id	= astman_get_header (m, "ActionID");

	char		idtext[256] = "";
	char		buf[256];
	const char*	msg;
	int		status;

	if (ast_strlen_zero (device))
	{
		astman_send_error (s, m, "Device not specified");
		return 0;
	}

	if (!ast_strlen_zero (id))
	{
		snprintf (idtext, sizeof (idtext), "ActionID: %s\r\n", id);
	}

	msg = send_reset(device, &status);
	snprintf (buf, sizeof (buf), "[%s] %s", device, msg);
	(status ? astman_send_ack : astman_send_error)(s, m, buf);

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
	}
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

#endif /* __MANAGER__ */
