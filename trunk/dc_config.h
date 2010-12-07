/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_DC_CONFIG_H_INCLUDED
#define CHAN_DATACARD_DC_CONFIG_H_INCLUDED

#include <asterisk.h>
#include <asterisk/channel.h>		/* AST_MAX_CONTEXT MAX_LANGUAGE */

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

#define CONFIG_FILE		"datacard.conf"

typedef enum {
	CALL_WAITING_DISALLOWED = 0,
	CALL_WAITING_ALLOWED,
	CALL_WAITING_AUTO
} call_waiting_t;

/* Global inherited (shared) settings */
typedef struct dc_sconfig
{
	char			context[AST_MAX_CONTEXT];	/*!< the context for incoming calls; 'default '*/
	char			exten[AST_MAX_EXTENSION];	/*!< exten, not overwrite valid subscriber_number */
	char			language[MAX_LANGUAGE];		/*!< default language 'en' */
	int			group;				/*!< group number for group dialling 0 */
	int			rxgain;				/*!< increase the incoming volume 0 */
	int			txgain;				/*!< increase the outgoint volume 0 */
	int			u2diag;				/*!< -1 */
	int			callingpres;			/*!< calling presentation */

	unsigned int		usecallingpres:1;		/*! -1 */
	unsigned int		auto_delete_sms:1;		/*! 0 */
	unsigned int		reset_datacard:1;		/*! 1 */
	unsigned int		disablesms:1;			/*! 0 */
	unsigned int		smsaspdu:1;			/*! 0 */
	unsigned int		disable:1;			/*! 0 */

	call_waiting_t		call_waiting;			/*!< enable/disable/auto call waiting CALL_WAITING_AUTO */

	int			mindtmfgap;			/*!< minimal time in ms from end of previews DTMF and begining of next */
#define DEFAULT_MINDTMFGAP	45

	int			mindtmfduration;		/*!< minimal DTMF duration in ms */
#define DEFAULT_MINDTMFDURATION	80

	int			mindtmfinterval;		/*!< minimal DTMF interval beetween ends in ms, applied only on same digit */
#define DEFAULT_MINDTMFINTERVAL	200
} dc_sconfig_t;

/* Global settings */
typedef struct dc_gconfig
{
	struct ast_jb_conf	jbconf;				/*!< jitter buffer settings, disabled by default */
	int			discovery_interval;		/*!< The device discovery interval */
#define DEFAULT_DISCOVERY_INT	60
} dc_gconfig_t;

/* Local required (unique) settings */
typedef struct dc_uconfig
{
	/* unique settings */
	char			id[31];				/*!< id from datacard.conf */
	char			audio_tty[256];			/*!< tty for audio connection */
	char			data_tty[256];			/*!< tty for AT commands */
} dc_uconfig_t;

/* all Config settings join in one place */
typedef struct pvt_config
{
	dc_uconfig_t		unique;				/*!< unique settings */
	dc_sconfig_t		shared;				/*!< possible inherited settings */
} pvt_config_t;

EXPORT_DECL void dc_sconfig_fill_defaults(struct dc_sconfig * config);
EXPORT_DECL void dc_sconfig_fill(struct ast_config * cfg, const char * cat, struct dc_sconfig * config);
EXPORT_DECL void dc_gconfig_fill(struct ast_config * cfg, const char * cat, struct dc_gconfig * config);
EXPORT_DECL int dc_config_fill(struct ast_config * cfg, const char * cat, const struct dc_sconfig * parent, struct pvt_config * config);

#endif /* CHAN_DATACARD_DC_CONFIG_H_INCLUDED */
