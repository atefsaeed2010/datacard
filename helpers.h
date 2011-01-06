/* 
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_HELPERS_H_INCLUDED
#define CHAN_DATACARD_HELPERS_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */
#include "dc_config.h"			/* call_waiting_t */

#define ITEMS_OF(x)				(sizeof(x)/sizeof((x)[0]))
#define STRLEN(string)				(sizeof(string)-1)

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

typedef enum {
	DEV_STATE_STOPPED	= 0,
	DEV_STATE_RESTARTED,
	DEV_STATE_REMOVED,
	DEV_STATE_STARTED,
} dev_state_t;

typedef enum {
	RESTATE_TIME_NOW	= 0,
	RESTATE_TIME_GRACEFULLY,
	RESTATE_TIME_CONVENIENT,
} restate_time_t;

INLINE_DECL const char * dev_state2str(dev_state_t state)
{
	static const char * states[] = { "stop", "restart", "remove", "start" };
	return states[state];
}

INLINE_DECL const char * dev_state2str_msg(dev_state_t state)
{
	static const char * states[] = { "Stop scheduled", "Restart scheduled", "Removal scheduled", "Start scheduled" };
	return states[state];
}

EXPORT_DECL struct pvt* find_device (const char* name);
EXPORT_DECL struct pvt* find_device_ext (const char* name, const char ** reason);
EXPORT_DECL int get_at_clir_value (struct pvt* pvt, int clir);

/* return status string of sending, status arg is optional */
EXPORT_DECL const char* send_ussd(const char* dev_name, const char* ussd, int * status);
EXPORT_DECL const char* send_sms(const char* dev_name, const char* number, const char* message, const char * validity, const char * report, int * status);
EXPORT_DECL const char* send_reset(const char* dev_name, int * status);
EXPORT_DECL const char* send_ccwa_set(const char* dev_name, call_waiting_t enable, int * status);
EXPORT_DECL const char* send_at_command(const char* dev_name, const char* command);
EXPORT_DECL const char* schedule_restart_event(dev_state_t event, restate_time_t when, const char* dev_name, int * status);
EXPORT_DECL int is_valid_phone_number(const char* number);




#endif /* CHAN_DATACARD_HELPERS_H_INCLUDED */
