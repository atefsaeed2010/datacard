/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dc_config.h"
#include <asterisk/callerid.h>				/* ast_parse_caller_presentation() */

static struct ast_jb_conf jbconf_default = 
{
	.flags			= 0,
	.max_size		= -1,
	.resync_threshold	= -1,
	.impl			= "",
	.target_extra		= -1,
};

#/* assume config is zerofill */
static int dc_uconfig_fill(struct ast_config * cfg, const char * cat, struct dc_uconfig * config)
{
	const char* audio_tty;
	const char* data_tty;

	audio_tty = ast_variable_retrieve (cfg, cat, "audio");
	if(!audio_tty)
	{
		ast_log (LOG_ERROR, "Skipping device %s. Missing required audio_tty setting\n", cat);
		return 1;
	}
	
	data_tty  = ast_variable_retrieve (cfg, cat, "data");
	if(!data_tty)
	{
		ast_log (LOG_ERROR, "Skipping device %s. Missing required audio_tty setting\n", cat);
		return 1;
	}

	ast_copy_string (config->id,		cat,		sizeof (config->id));
	ast_copy_string (config->data_tty,	data_tty,	sizeof (config->data_tty));
	ast_copy_string (config->audio_tty,	audio_tty,	sizeof (config->audio_tty));

	return 0;
}

#/* */
EXPORT_DEF void dc_sconfig_fill_defaults(struct dc_sconfig * config)
{
	/* first set default values */
	memset(config, 0, sizeof(*config));

	ast_copy_string (config->context, "default", sizeof (config->context));
	ast_copy_string (config->exten, "", sizeof (config->exten));
	ast_copy_string (config->language, DEFAULT_LANGUAGE, sizeof (config->language));

	config->u2diag			= -1;
	config->reset_datacard		=  1;
	config->callingpres		= -1;
	config->call_waiting 		= CALL_WAITING_AUTO;
	config->dtmf			= DC_DTMF_SETTING_RELAX;

	config->mindtmfgap		= DEFAULT_MINDTMFGAP;
	config->mindtmfduration		= DEFAULT_MINDTMFDURATION;
	config->mindtmfinterval		= DEFAULT_MINDTMFINTERVAL;
}

#/* */
EXPORT_DEF void dc_sconfig_fill(struct ast_config * cfg, const char * cat, struct dc_sconfig * config)
{
	struct ast_variable * v;

	/*  read config and translate to values */
	for (v = ast_variable_browse (cfg, cat); v; v = v->next)
	{
		if (!strcasecmp (v->name, "context"))
		{
			ast_copy_string (config->context, v->value, sizeof (config->context));
		}
		if (!strcasecmp (v->name, "exten"))
		{
			ast_copy_string (config->exten, v->value, sizeof (config->exten));
		}
		else if (!strcasecmp (v->name, "language"))
		{
			ast_copy_string (config->language, v->value, sizeof (config->language));	/* set channel language */
		}
		else if (!strcasecmp (v->name, "group"))
		{
			config->group = (int) strtol (v->value, (char**) NULL, 10);		/* group is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "rxgain"))
		{
			config->rxgain = (int) strtol (v->value, (char**) NULL, 10);		/* rxgain is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "txgain"))
		{
			config->txgain = (int) strtol (v->value, (char**) NULL, 10);		/* txgain is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "u2diag"))
		{
			errno = 0;
			config->u2diag = (int) strtol (v->value, (char**) NULL, 10);		/* u2diag is set to -1 if invalid */
			if (config->u2diag == 0 && errno == EINVAL)
			{
				config->u2diag = -1;
			}
		}
		else if (!strcasecmp (v->name, "callingpres"))
		{
			config->callingpres = ast_parse_caller_presentation (v->value);
			if (config->callingpres == -1)
			{
				errno = 0;
				config->callingpres = (int) strtol (v->value, (char**) NULL, 10);	/* callingpres is set to -1 if invalid */
				if (config->callingpres == 0 && errno == EINVAL)
				{
					config->callingpres = -1;
				}
			}
		}
		else if (!strcasecmp (v->name, "usecallingpres"))
		{
			config->usecallingpres = ast_true (v->value);		/* usecallingpres is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "autodeletesms"))
		{
			config->auto_delete_sms = ast_true (v->value);		/* auto_delete_sms is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "resetdatacard"))
		{
			config->reset_datacard = ast_true (v->value);		/* reset_datacard is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "disablesms"))
		{
			config->disablesms = ast_true (v->value);		/* disablesms is set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "smsaspdu"))
		{
			config->smsaspdu = ast_true (v->value);			/* send_sms_as_pdu us set to 0 if invalid */
		}
		else if (!strcasecmp (v->name, "disable"))
		{
			config->disable = ast_true (v->value);
		}
		else if (!strcasecmp (v->name, "callwaiting"))
		{
			if(strcasecmp(v->value, "auto"))
				config->call_waiting = ast_true (v->value);
		}
		else if (!strcasecmp (v->name, "dtmf"))
		{
			if(!strcasecmp(v->value, "off"))
				config->dtmf = DC_DTMF_SETTING_OFF;
			else if(!strcasecmp(v->value, "inband"))
				config->dtmf = DC_DTMF_SETTING_INBAND;
			else if(!strcasecmp(v->value, "relax"))
				config->dtmf = DC_DTMF_SETTING_RELAX;
			else
			ast_log(LOG_ERROR, "Invalid valie for 'dtmf': '%s', setting default 'relax'\n", v->value);
		}
		else if (!strcasecmp (v->name, "mindtmfgap"))
		{
			errno = 0;
			config->mindtmfgap = (int) strtol (v->value, (char**) NULL, 10);
			if ((config->mindtmfgap == 0 && errno == EINVAL) || config->mindtmfgap < 0)
			{
				ast_log(LOG_ERROR, "Invalid valie for 'mindtmfgap' '%s', setting default %d\n", v->value, DEFAULT_MINDTMFGAP);
				config->mindtmfgap = DEFAULT_MINDTMFGAP;
			}
		}
		else if (!strcasecmp (v->name, "mindtmfduration"))
		{
			errno = 0;
			config->mindtmfduration = (int) strtol (v->value, (char**) NULL, 10);
			if ((config->mindtmfduration == 0 && errno == EINVAL) || config->mindtmfduration < 0)
			{
				ast_log(LOG_ERROR, "Invalid valie for 'mindtmfgap' '%s', setting default %d\n", v->value, DEFAULT_MINDTMFDURATION);
				config->mindtmfduration = DEFAULT_MINDTMFDURATION;
			}
		}
		else if (!strcasecmp (v->name, "mindtmfinterval"))
		{
			errno = 0;
			config->mindtmfinterval = (int) strtol (v->value, (char**) NULL, 10);
			if ((config->mindtmfinterval == 0 && errno == EINVAL) || config->mindtmfinterval < 0)
			{
				ast_log(LOG_ERROR, "Invalid valie for 'mindtmfinterval' '%s', setting default %d\n", v->value, DEFAULT_MINDTMFINTERVAL);
				config->mindtmfduration = DEFAULT_MINDTMFINTERVAL;
			}
		}
	}
}

#/* */
EXPORT_DEF void dc_gconfig_fill(struct ast_config * cfg, const char * cat, struct dc_gconfig * config)
{
	struct ast_variable * v;
	int tmp;

	/* set default values */
	memcpy(&config->jbconf, &jbconf_default, sizeof(config->jbconf));
	config->discovery_interval = DEFAULT_DISCOVERY_INT;

	for (v = ast_variable_browse (cfg, cat); v; v = v->next)
	{
		/* handle jb conf */
		if (!ast_jb_read_conf (&config->jbconf, v->name, v->value))
		{
			continue;
		}

		if (!strcasecmp (v->name, "interval"))
		{
			errno = 0;
			tmp = (int) strtol (v->value, (char**) NULL, 10);
			if (tmp == 0 && errno == EINVAL)
			{
				ast_log (LOG_NOTICE, "Error parsing 'interval' in general section, using default value\n");
			}
			else
				config->discovery_interval = tmp;
		}
	}
}

#/* */
EXPORT_DEF int dc_config_fill(struct ast_config * cfg, const char * cat, const struct dc_sconfig * parent, struct pvt_config * config)
{
	/* try set unique first */
	int err = dc_uconfig_fill(cfg, cat, &config->unique);
	if(!err)
	{
		/* inherit from parent */
		memcpy(&config->shared, parent, sizeof(config->shared));

		/* overwrite local */
		dc_sconfig_fill(cfg, cat, &config->shared);
	}

	return err;
}
