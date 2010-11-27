/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_AT_PARSE_H_INCLUDED
#define CHAN_DATACARD_AT_PARSE_H_INCLUDED

#include <sys/types.h>			/* size_t */

#include "export.h"		/* EXPORT_DECL EXPORT_DEF */

struct pvt;

//EXPORT_DECL char* at_parse_clip (char* str, unsigned len);
EXPORT_DECL char* at_parse_cnum (char* str, unsigned len);
EXPORT_DECL char* at_parse_cops (char* str, unsigned len);
EXPORT_DECL int at_parse_creg (char* str, unsigned len, int* gsm_reg, int* gsm_reg_status, char** lac, char** ci);
EXPORT_DECL int at_parse_cmti (struct pvt* pvt, const char* str);
EXPORT_DECL int at_parse_cmgr (char* str, unsigned len, char** number, char** text);
EXPORT_DECL int at_parse_cusd (char* str, unsigned len, char** cusd, unsigned char* dcs);
EXPORT_DECL int at_parse_cpin (struct pvt* pvt, char* str, unsigned len);
EXPORT_DECL int at_parse_csq (struct pvt* pvt, const char* str, int* rssi);
EXPORT_DECL int at_parse_rssi (struct pvt* pvt, const char* str);
EXPORT_DECL int at_parse_mode (struct pvt* pvt, char* str, unsigned len, int* mode, int* submode);

#endif /* CHAN_DATACARD_AT_PARSE_H_INCLUDED */
