/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_AT_PARSE_H_INCLUDED
#define CHAN_DATACARD_AT_PARSE_H_INCLUDED

#include <sys/types.h>			/* size_t */

#include "export.h"		/* EXPORT_DECL EXPORT_DECL */
#include "char_conv.h"		/* str_encoding_t */
struct pvt;

EXPORT_DECL char* at_parse_cnum (char* str, unsigned len);
EXPORT_DECL char* at_parse_cops (char* str, unsigned len);
EXPORT_DECL int at_parse_creg (char* str, unsigned len, int* gsm_reg, int* gsm_reg_status, char** lac, char** ci);
EXPORT_DECL int at_parse_cmti (struct pvt* pvt, const char* str);
EXPORT_DECL const char* at_parse_cmgr (char** str, size_t len, char* oa, size_t oa_len, str_encoding_t* oa_enc, char** msg, str_encoding_t* msg_enc);
EXPORT_DECL int at_parse_cusd (char* str, size_t len, char** cusd, unsigned char* dcs);
EXPORT_DECL int at_parse_cpin (struct pvt* pvt, char* str, size_t len);
EXPORT_DECL int at_parse_csq (struct pvt* pvt, const char* str, int* rssi);
EXPORT_DECL int at_parse_rssi (struct pvt* pvt, const char* str);
EXPORT_DECL int at_parse_mode (struct pvt* pvt, char* str, size_t len, int* mode, int* submode);



#endif /* CHAN_DATACARD_AT_PARSE_H_INCLUDED */
