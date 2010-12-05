/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_PDU_H_INCLUDED
#define CHAN_DATACARD_PDU_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

EXPORT_DECL int build_pdu(char* buffer, unsigned length, const char* csca, const char* dst, const char* msg, unsigned valid_minutes, int srr, int* sca_len);

#endif /* CHAN_DATACARD_PDU_H_INCLUDED */
