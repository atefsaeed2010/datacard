/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <asterisk.h>
#include <asterisk/lock.h>			/* AST_RWLIST_RDLOCK AST_RWLIST_TRAVERSE AST_RWLIST_UNLOCK */
#include <asterisk/callerid.h>			/*  AST_PRES_* */

#include "chan_datacard.h"			/* devices */
#include "at_command.h"

#/* */
EXPORT_DEF struct pvt* find_device (const char* name)
{
	struct pvt*	pvt = 0;

	AST_RWLIST_RDLOCK (&gpublic->devices);
	AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
	{
		if (!strcmp (PVT_ID(pvt), name))
		{
			break;
		}
	}
	AST_RWLIST_UNLOCK (&gpublic->devices);

	return pvt;
}

#/* */
EXPORT_DEF int get_at_clir_value (struct pvt* pvt, int clir)
{
	int res = 0;

	switch (clir)
	{
		case AST_PRES_ALLOWED_NETWORK_NUMBER:
		case AST_PRES_ALLOWED_USER_NUMBER_FAILED_SCREEN:
		case AST_PRES_ALLOWED_USER_NUMBER_NOT_SCREENED:
		case AST_PRES_ALLOWED_USER_NUMBER_PASSED_SCREEN:
		case AST_PRES_NUMBER_NOT_AVAILABLE:
			ast_debug (2, "[%s] callingpres: %s\n", PVT_ID(pvt), ast_describe_caller_presentation (clir));
			res = 2;
			break;

		case AST_PRES_PROHIB_NETWORK_NUMBER:
		case AST_PRES_PROHIB_USER_NUMBER_FAILED_SCREEN:
		case AST_PRES_PROHIB_USER_NUMBER_NOT_SCREENED:
		case AST_PRES_PROHIB_USER_NUMBER_PASSED_SCREEN:
			ast_debug (2, "[%s] callingpres: %s\n", PVT_ID(pvt), ast_describe_caller_presentation (clir));
			res = 1;
			break;

		default:
			ast_log (LOG_WARNING, "[%s] Unsupported callingpres: %d\n", PVT_ID(pvt), clir);
			if ((clir & AST_PRES_RESTRICTION) != AST_PRES_ALLOWED)
			{
				res = 0;
			}
			else
			{
				res = 2;
			}
			break;
	}

	return res;
}

typedef int (*at_cmd_f)(struct cpvt*, const char*, const char*);

#/* */
static const char* send2(const char* dev_name, int * status, int online, at_cmd_f func, const char* arg1, const char* arg2, const char* emsg, const char* okmsg)
{
	struct pvt* pvt;
	const char* msg;

	if(status)
		*status = 0;
	pvt = find_device (dev_name);
	if (pvt)
	{
		ast_mutex_lock (&pvt->lock);
		if (pvt->connected && (!online || (pvt->initialized && pvt->gsm_registered)))
		{
			if ((*func) (&pvt->sys_chan, arg1, arg2))
			{
				msg = emsg;
				ast_log (LOG_ERROR, "[%s] %s\n", PVT_ID(pvt), emsg);
			}
			else
			{
				msg = okmsg;
				if(status)
					*status = 1;
			}
		}
		else
			msg = "Device not connected / initialized / registered";
		ast_mutex_unlock (&pvt->lock);
	}
	else
	{
		msg = "Device not found";
	}
	return msg;
}

#/* */
EXPORT_DEF const char* send_ussd(const char* dev_name, const char* ussd, int * status)
{
	return send2(dev_name, status, 1, (at_cmd_f)at_enque_cusd, ussd, NULL, 
			"Error adding USSD command to queue", "USSD queued for send");
}

#/* */
EXPORT_DEF const char* send_sms(const char* dev_name, const char* number, const char* message, int * status)
{
	return send2(dev_name, status, 1, at_enque_sms, number, message, 
			"Error adding SMS commands to queue", "SMS queued for send");
}

#/* */
EXPORT_DEF const char* send_reset(const char* dev_name, int * status)
{
	return send2(dev_name, status, 0, (at_cmd_f)at_enque_reset, NULL, NULL, 
			"Error adding reset command to queue", "Reset command queued for execute");
}

#/* */
EXPORT_DEF const char* send_ccwa_set(const char* dev_name, call_waiting_t enable, int * status)
{
/* FIXME: 64bit compiler complain here
*/
	return send2(dev_name, status, 1, (at_cmd_f)at_enque_set_ccwa, (const char*)enable, 
			NULL, "Error adding CCWA commands to queue", 
			"Call-Waiting commands queued for execute");
}

#/* */
EXPORT_DEF const char* send_at_command(const char* dev_name, const char* command)
{
	return send2(dev_name, NULL, 0, (at_cmd_f)at_enque_unknown_cmd, command, 
			NULL, "Error adding command", "Command queued for execute");
}

/* TODO: use also for SMS and USSD?
*/

#/* get digit code, 0 if invalid  */
EXPORT_DEF char dial_digit_code(char digit)
{
	switch(digit)
	{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			break;
		case '*':
			digit = 'A';
			break;
		case '#':
			digit = 'B';
			break;
		case 'a':
		case 'A':
			digit = 'C';
			break;
		case 'b':
		case 'B':
			digit = 'D';
			break;
		case 'c':
		case 'C':
			digit = 'E';
			break;
		default:
			return 0;
	}
	return digit;
}

#/* */
EXPORT_DEF int is_valid_phone_number(const char* number)
{
	for(; *number; number++)
		if(dial_digit_code(*number) == 0)
			return 0;

	return 1;
}
