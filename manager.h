/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifdef __MANAGER__
#ifndef CHAN_DATACARD_MANAGER_H_INCLUDED
#define CHAN_DATACARD_MANAGER_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

EXPORT_DECL void manager_register();
EXPORT_DECL void manager_unregister();
EXPORT_DECL void manager_event_new_ussd (struct pvt* pvt, char* message);
EXPORT_DECL void manager_event_new_ussd_base64 (struct pvt* pvt, char* message);
EXPORT_DECL void manager_event_new_sms (struct pvt* pvt, char* number, char* message);
EXPORT_DECL void manager_event_new_sms_base64 (struct pvt* pvt, char* number, char* message_base64);

#endif /* CHAN_DATACARD_MANAGER_H_INCLUDED */
#endif /* __MANAGER__ */
