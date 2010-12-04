#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef BUILD_APPLICATIONS
/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>
*/

#include <asterisk.h>
#include <asterisk/app.h>	/* AST_DECLARE_APP_ARGS() ... */
#include <asterisk/pbx.h>	/* pbx_builtin_setvar_helper() */
#include <asterisk/module.h>	/* ast_register_application2() ast_unregister_application() */
#include <asterisk/version.h>	/* ASTERISK_VERSION_NUM */

#include "app.h"		/* app_register() app_unregister() */
#include "chan_datacard.h"	/* struct pvt */
#include "helpers.h"		/* send_sms() ITEMS_OF() */

struct ast_channel;

static int app_status_exec (struct ast_channel* channel, const char* data)
{
	struct pvt* pvt;
	char*	parse;
	int	stat;
	char	status[2];

	AST_DECLARE_APP_ARGS (args,
		AST_APP_ARG (device);
		AST_APP_ARG (variable);
	);

	if (ast_strlen_zero (data))
	{
		return -1;
	}

	parse = ast_strdupa (data);

	AST_STANDARD_APP_ARGS (args, parse);

	if (ast_strlen_zero (args.device) || ast_strlen_zero (args.variable))
	{
		return -1;
	}

	pvt = find_device(args.device);
	if(pvt)
	{
		ast_mutex_lock(&pvt->lock);

		if (pvt->connected)
			stat = 2;
		if (pvt->chansno)
			stat = 3;

		ast_mutex_unlock (&pvt->lock);
	}
	else
		stat = 1;

	snprintf (status, sizeof (status), "%d", stat);
	pbx_builtin_setvar_helper (channel, args.variable, status);

	return 0;
}

static int app_send_sms_exec (attribute_unused struct ast_channel* channel, const char* data)
{
	char*	parse;
	const char* msg;
	int status;

	AST_DECLARE_APP_ARGS (args,
		AST_APP_ARG (device);
		AST_APP_ARG (number);
		AST_APP_ARG (message);
	);

	if (ast_strlen_zero (data))
	{
		return -1;
	}

	parse = ast_strdupa (data);

	AST_STANDARD_APP_ARGS (args, parse);

	if (ast_strlen_zero (args.device))
	{
		ast_log (LOG_ERROR, "NULL device for message -- SMS will not be sent\n");
		return -1;
	}

	if (ast_strlen_zero (args.number))
	{
		ast_log (LOG_ERROR, "NULL destination for message -- SMS will not be sent\n");
		return -1;
	}

	if (!is_valid_phone_number (args.number))
	{
		ast_log (LOG_ERROR, "Invalid destination for message -- SMS will not be sent\n");
		return -1;
	}
	
/* its possible to send empty SMS message
	if (ast_strlen_zero (args.message))
	{
		ast_log (LOG_ERROR, "NULL Message to be sent -- SMS will not be sent\n");
		return -1;
	}
*/

	msg = send_sms(args.device, args.number, args.message, &status);
	if(!status)
		ast_log (LOG_ERROR, "[%s] %s\n", args.device, msg);
	return !status;
}



static const struct datacard_application
{
	const char*	name;

	int		(*func)(struct ast_channel* channel, const char* data);
	const char*	synopsis;
	const char*	desc;
} dca[] = 
{
	{
		"DatacardStatus", 
		app_status_exec,
		"DatacardStatus(Device,Variable)", 
		"DatacardStatus(Device,Variable)\n"
		"  Device   - Id of device from datacard.conf\n"
		"  Variable - Variable to store status in will be 1-3.\n"
		"             In order, Disconnected, Connected & Free, Connected & Busy.\n"
	},
	{
		"DatacardSendSMS", 
		app_send_sms_exec, 
		"DatacardSendSMS(Device,Dest,Message)", 
		"DatacardSendSMS(Device,Dest,Message)\n"
		"  Device  - Id of device from datacard.conf\n"
		"  Dest    - destination\n"
		"  Message - text of the message\n"
	}
};

#if ASTERISK_VERSION_NUM >= 10800
typedef int		(*app_func_t)(struct ast_channel* channel, const char * data);
#else
typedef int		(*app_func_t)(struct ast_channel* channel, void * data);
#endif

#/* */
EXPORT_DEF void app_register()
{
	unsigned i;
	for(i = 0; i < ITEMS_OF(dca); i++)
	{
		ast_register_application2 (dca[i].name, (app_func_t)(dca[i].func), dca[i].synopsis, dca[i].desc, self_module());
	}
}

#/* */
EXPORT_DEF void app_unregister()
{
	int i;
	for(i = ITEMS_OF(dca)-1; i >= 0; i--)
	{
		ast_unregister_application(dca[i].name);
	}
}

#endif /* BUILD_APPLICATIONS */