/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifdef BUILD_MANAGER
#ifndef CHAN_DATACARD_MANAGER_H_INCLUDED
#define CHAN_DATACARD_MANAGER_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

EXPORT_DECL void manager_register();
EXPORT_DECL void manager_unregister();

EXPORT_DECL void manager_event_message(const char * event, const char * devname, const char * message);
EXPORT_DECL void manager_event_new_ussd(const char * devname, char * message);
EXPORT_DECL void manager_event_new_sms(const char * devname, char * number, char * message);
EXPORT_DECL void manager_event_new_sms_base64 (const char * devname, char * number, char * message_base64);
EXPORT_DECL void manager_event_cend(const char * devname, int call_index, int duration, int end_status, int cc_cause);
EXPORT_DECL void manager_event_call_state_change(const char * devname, int call_index, const char * newstate);
EXPORT_DECL void manager_event_device_status(const char * devname, const char * newstatus);
EXPORT_DECL void manager_event_sent_notify(const char * devname, const char * type, const void * id, const char * result);

#endif /* CHAN_DATACARD_MANAGER_H_INCLUDED */
#endif /* BUILD_MANAGER */
