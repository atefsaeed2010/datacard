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
#include <asterisk/cli.h>			/* struct ast_cli_entry; struct ast_cli_args */
#include <asterisk/callerid.h>			/* ast_describe_caller_presentation() */
#include <asterisk/version.h>			/* ASTERISK_VERSION_NUM */

#include "cli.h"
#include "chan_datacard.h"			/* devices */
#include "helpers.h"				/* find_device() ITEMS_OF() send_ccwa_set() send_reset() send_sms() send_ussd() */

static char* complete_device (const char* word, int state)
{
	struct pvt*	pvt;
	char*	res = NULL;
	int	which = 0;
	int	wordlen = strlen (word);

	AST_RWLIST_RDLOCK (&gpublic->devices);
	AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
	{
		if (!strncasecmp (PVT_ID(pvt), word, wordlen) && ++which > state)
		{
			res = ast_strdup (PVT_ID(pvt));
			break;
		}
	}
	AST_RWLIST_UNLOCK (&gpublic->devices);

	return res;
}

static char* cli_show_devices (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	struct pvt* pvt;

#define FORMAT1 "%-12.12s %-5.5s %-10.10s %-4.4s %-4.4s %-7.7s %-14.14s %-10.10s %-17.17s %-16.16s %-16.16s %-14.14s\n"
#define FORMAT2 "%-12.12s %-5d %-10.10s %-4d %-4d %-7d %-14.14s %-10.10s %-17.17s %-16.16s %-16.16s %-14.14s\n"

	switch (cmd)
	{
		case CLI_INIT:
			e->command =	"datacard show devices";
			e->usage   =	"Usage: datacard show devices\n"
					"       Shows the state of Datacard devices.\n";
			return NULL;

		case CLI_GENERATE:
			return NULL;
	}

	if (a->argc < 3)
	{
		return CLI_SHOWUSAGE;
	}

	ast_cli (a->fd, FORMAT1, "ID", "Group", "State", "RSSI", "Mode", "Submode", "Provider Name", "Model", "Firmware", "IMEI", "IMSI", "Number");

	AST_RWLIST_RDLOCK (&gpublic->devices);
	AST_RWLIST_TRAVERSE (&gpublic->devices, pvt, entry)
	{
		ast_mutex_lock (&pvt->lock);
		ast_cli (a->fd, FORMAT2,
			PVT_ID(pvt),
			CONF_SHARED(pvt, group),
			pvt_str_state(pvt),
			pvt->rssi,
			pvt->linkmode,
			pvt->linksubmode,
			pvt->provider_name,
			pvt->model,
			pvt->firmware,
			pvt->imei,
			pvt->imsi,
			pvt->subscriber_number
		);
		ast_mutex_unlock (&pvt->lock);
	}
	AST_RWLIST_UNLOCK (&gpublic->devices);

#undef FORMAT1
#undef FORMAT2

	return CLI_SUCCESS;
}

static char* cli_show_device (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	struct pvt* pvt;
	struct ast_str* statebuf;
	char buf[40];

	switch (cmd)
	{
		case CLI_INIT:
			e->command =	"datacard show device";
			e->usage   =	"Usage: datacard show device <device>\n"
					"       Shows the state and config of Datacard device.\n";
			return NULL;

		case CLI_GENERATE:
			if (a->pos == 3)
			{
				return complete_device (a->word, a->n);
			}
			return NULL;
	}

	if (a->argc < 4)
	{
		return CLI_SHOWUSAGE;
	}

	pvt = find_device (a->argv[3]);
	if (pvt)
	{
		statebuf = pvt_str_state_ex(pvt);

		ast_mutex_lock (&pvt->lock);

		ast_cli (a->fd, "\n Current device settings:\n");
		ast_cli (a->fd, "------------------------------------\n");
		ast_cli (a->fd, "  Device                  : %s\n", PVT_ID(pvt));
		ast_cli (a->fd, "  Group                   : %d\n", CONF_SHARED(pvt, group));
		ast_cli (a->fd, "  GSM Registration Status : %s\n", GSM_regstate2str(pvt->gsm_reg_status));
		ast_cli (a->fd, "  State                   : %s\n", ast_str_buffer(statebuf));
		ast_cli (a->fd, "  Voice                   : %s\n", (pvt->has_voice) ? "Yes" : "No");
		ast_cli (a->fd, "  SMS                     : %s\n", (pvt->has_sms) ? "Yes" : "No");
		ast_cli (a->fd, "  RSSI                    : %d, %s\n", pvt->rssi, rssi2dBm(pvt->rssi, buf, sizeof(buf)));
		ast_cli (a->fd, "  Mode                    : %s\n", sys_mode2str(pvt->linkmode));
		ast_cli (a->fd, "  Submode                 : %s\n", sys_submode2str(pvt->linksubmode));
		ast_cli (a->fd, "  ProviderName            : %s\n", pvt->provider_name);
		ast_cli (a->fd, "  Manufacturer            : %s\n", pvt->manufacturer);
		ast_cli (a->fd, "  Model                   : %s\n", pvt->model);
		ast_cli (a->fd, "  Firmware                : %s\n", pvt->firmware);
		ast_cli (a->fd, "  IMEI                    : %s\n", pvt->imei);
		ast_cli (a->fd, "  IMSI                    : %s\n", pvt->imsi);
		ast_cli (a->fd, "  Subscriber Number       : %s\n", pvt->subscriber_number);
		ast_cli (a->fd, "  SMS Service Center      : %s\n", pvt->sms_scenter);
		ast_cli (a->fd, "  Use CallingPres         : %s\n", CONF_SHARED(pvt, usecallingpres) ? "Yes" : "No");
		ast_cli (a->fd, "  Default CallingPres     : %s\n", CONF_SHARED(pvt, callingpres) < 0 ? "<Not set>" : ast_describe_caller_presentation (CONF_SHARED(pvt, callingpres)));
		ast_cli (a->fd, "  Use UCS-2 encoding      : %s\n", pvt->use_ucs2_encoding ? "Yes" : "No");
		ast_cli (a->fd, "  USSD use 7 bit encoding : %s\n", pvt->cusd_use_7bit_encoding ? "Yes" : "No");
		ast_cli (a->fd, "  USSD use UCS-2 decoding : %s\n", pvt->cusd_use_ucs2_decoding ? "Yes" : "No");
		ast_cli (a->fd, "  Location area code      : %s\n", pvt->location_area_code);
		ast_cli (a->fd, "  Cell ID                 : %s\n", pvt->cell_id);
		ast_cli (a->fd, "  Auto delete SMS         : %s\n", CONF_SHARED(pvt, auto_delete_sms) ? "Yes" : "No");
		ast_cli (a->fd, "  Disable SMS             : %s\n", CONF_SHARED(pvt, disablesms) ? "Yes" : "No");
		ast_cli (a->fd, "  Channel Language        : %s\n", CONF_SHARED(pvt, language));
		ast_cli (a->fd, "  Send SMS as PDU         : %s\n", CONF_SHARED(pvt, smsaspdu) ? "Yes" : "No");
		ast_cli (a->fd, "  Minimal DTMF Gap        : %d\n", CONF_SHARED(pvt, mindtmfgap));
		ast_cli (a->fd, "  Minimal DTMF Duration   : %d\n", CONF_SHARED(pvt, mindtmfduration));
		ast_cli (a->fd, "  Minimal DTMF Interval   : %d\n", CONF_SHARED(pvt, mindtmfinterval));
		ast_cli (a->fd, "  Call Waiting            : %s\n\n", pvt->has_call_waiting ? "Enabled" : "Disabled" );
/* TODO: show call waiting  network setting and local config value */
		ast_mutex_unlock (&pvt->lock);

		ast_free(statebuf);
	}
	else
	{
		ast_cli (a->fd, "Device %s not found\n", a->argv[2]);
	}

	return CLI_SUCCESS;
}

static char* cli_show_version (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	switch (cmd)
	{
		case CLI_INIT:
			e->command =	"datacard show version";
			e->usage   =	"Usage: datacard show version\n"
					"       Shows the version of module.\n";
			return NULL;

		case CLI_GENERATE:
			return NULL;
	}

	if (a->argc < 3)
	{
		return CLI_SHOWUSAGE;
	}

	ast_cli (a->fd, "\n%s: %s, Version %s, Revision %s\nProject Home: %s\nBug Reporting: %s\n\n", AST_MODULE, MODULE_DESCRIPTION, MODULE_VERSION, PACKAGE_REVISION, MODULE_URL, MODULE_BUGREPORT);

	return CLI_SUCCESS;
}

static char* cli_cmd (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	const char * msg;

	switch (cmd)
	{
		case CLI_INIT:
			e->command =	"datacard cmd";
			e->usage   =	"Usage: datacard cmd <device> <command>\n"
					"       Send <command> to the rfcomm port on the device\n"
					"       with the specified <device>.\n";
			return NULL;

		case CLI_GENERATE:
			if (a->pos == 2)
			{
				return complete_device (a->word, a->n);
			}
			return NULL;
	}

	if (a->argc < 4)
	{
		return CLI_SHOWUSAGE;
	}

	msg = send_at_command(a->argv[2], a->argv[3]);
	ast_cli (a->fd, "[%s] '%s' %s\n", a->argv[2], a->argv[3], msg);

	return CLI_SUCCESS;
}

static char* cli_ussd (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	const char * msg;
	
	switch (cmd)
	{
		case CLI_INIT:
			e->command = "datacard ussd";
			e->usage =
				"Usage: datacard ussd <device> <command>\n"
				"       Send ussd <command> to the datacard\n"
				"       with the specified <device>.\n";
			return NULL;

		case CLI_GENERATE:
			if (a->pos == 2)
			{
				return complete_device (a->word, a->n);
			}
			return NULL;
	}

	if (a->argc < 4)
	{
		return CLI_SHOWUSAGE;
	}

	msg = send_ussd(a->argv[2], a->argv[3], NULL);
	ast_cli (a->fd, "[%s] %s\n", a->argv[2], msg);

	return CLI_SUCCESS;
}

static char* cli_sms (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	const char * msg;
	struct ast_str*	buf;
	int	i;

	switch (cmd)
	{
		case CLI_INIT:
			e->command = "datacard sms";
			e->usage =
				"Usage: datacard sms <device ID> <number> <message>\n"
				"       Send a sms to <number> with the <message>\n";
			return NULL;

		case CLI_GENERATE:
			if (a->pos == 2)
			{
				return complete_device (a->word, a->n);
			}
			return NULL;
	}

	if (a->argc < 5)
	{
		return CLI_SHOWUSAGE;
	}

	buf = ast_str_create (256);
	for (i = 4; i < a->argc; i++)
	{
		if (i < (a->argc - 1))
		{
			ast_str_append (&buf, 0, "%s ", a->argv[i]);
		}
		else
		{
			ast_str_append (&buf, 0, "%s", a->argv[i]);
		}
	}

	msg = send_sms(a->argv[2], a->argv[3], ast_str_buffer(buf), 0, 0, NULL);
	ast_free (buf);
	ast_cli(a->fd, "[%s] %s\n", a->argv[2], msg);

	return CLI_SUCCESS;
}

#if ASTERISK_VERSION_NUM >= 10800
typedef const char * const * ast_cli_complete2_t;
#else
typedef char * const * ast_cli_complete2_t;
#endif

static char* cli_ccwa_set (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	static const char * const choices[] = { "enable", "disable", NULL };
	const char * msg;
	call_waiting_t enable;
	
	switch (cmd)
	{
		case CLI_INIT:
			e->command = "datacard callwaiting";
			e->usage =
				"Usage: datacard ccwa disable|enable <device>\n"
				"       Disable/Enable Call-Waiting on <device>\n";
			return NULL;

		case CLI_GENERATE:
			if (a->pos == 2)
			{
				return ast_cli_complete(a->word, (ast_cli_complete2_t)choices, a->n);
			}
			if (a->pos == 3)
			{
				return complete_device (a->word, a->n);
			}
			return NULL;
	}

	if (a->argc < 4)
	{
		return CLI_SHOWUSAGE;
	}
	if (strcmp("disable", a->argv[2]) == 0)
		enable = CALL_WAITING_DISALLOWED;
	else if (strcmp("enable", a->argv[2]) == 0)
		enable = CALL_WAITING_ALLOWED;
	else
		return CLI_SHOWUSAGE;

	msg = send_ccwa_set(a->argv[3], enable, NULL);
	ast_cli (a->fd, "[%s] %s\n", a->argv[3], msg);

	return CLI_SUCCESS;
}

static char* cli_reset (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	const char * msg;

	switch (cmd)
	{
		case CLI_INIT:
			e->command = "datacard reset";
			e->usage =
				"Usage: datacard reset <device>\n"
				"       Reset datacard <device>\n";
			return NULL;

		case CLI_GENERATE:
			if (a->pos == 2)
			{
				return complete_device (a->word, a->n);
			}
			return NULL;
	}

	if (a->argc < 3)
	{
		return CLI_SHOWUSAGE;
	}

	msg = send_reset(a->argv[2], NULL);
	ast_cli (a->fd, "[%s] %s\n", a->argv[2], msg);

	return CLI_SUCCESS;
}

#/* */
static char* cli_restart (struct ast_cli_entry* e, int cmd, struct ast_cli_args* a)
{
	const char * msg;

	switch (cmd)
	{
		case CLI_INIT:
			e->command = "datacard restart";
			e->usage =
				"Usage: datacard restart <device>\n"
				"       Restart datacard <device>\n";
			return NULL;

		case CLI_GENERATE:
			if (a->pos == 2)
			{
				return complete_device (a->word, a->n);
			}
			return NULL;
	}

	if (a->argc < 3)
	{
		return CLI_SHOWUSAGE;
	}

	msg = schedule_restart (a->argv[2], NULL);
	ast_cli (a->fd, "[%s] %s\n", a->argv[2], msg);

	return CLI_SUCCESS;
}

static struct ast_cli_entry cli[] = {
	AST_CLI_DEFINE (cli_show_devices,	"Show Datacard devices state"),
	AST_CLI_DEFINE (cli_show_device,	"Show Datacard device state and config"),
	AST_CLI_DEFINE (cli_show_version,	"Show module version"),
	AST_CLI_DEFINE (cli_cmd,		"Send commands to port for debugging"),
	AST_CLI_DEFINE (cli_ussd,		"Send USSD commands to the datacard"),
	AST_CLI_DEFINE (cli_sms,		"Send SMS from the datacard"),
	AST_CLI_DEFINE (cli_ccwa_set,		"Enable/Disable Call-Waiting on the datacard"),
	AST_CLI_DEFINE (cli_reset,		"Reset datacard"),
	AST_CLI_DEFINE (cli_restart,		"Restart datacard"),
};

#/* */
EXPORT_DEF void cli_register()
{
	ast_cli_register_multiple (cli, ITEMS_OF(cli));
}

#/* */
EXPORT_DEF void cli_unregister()
{
	ast_cli_unregister_multiple (cli, ITEMS_OF(cli));
}
