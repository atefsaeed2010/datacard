/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_CHAR_CONV_H_INCLUDED
#define CHAN_DATACARD_CHAR_CONV_H_INCLUDED

#include <sys/types.h>			/* ssize_t size_t */

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

EXPORT_DECL ssize_t utf8_to_hexstr_ucs2 (const char* in, size_t in_length, char* out, size_t out_size);
EXPORT_DECL ssize_t char_to_hexstr_7bit (const char* in, size_t in_length, char* out, size_t out_size);
EXPORT_DECL ssize_t hexstr_ucs2_to_utf8 (const char* in, size_t in_length, char* out, size_t out_size);
EXPORT_DECL ssize_t hexstr_7bit_to_char (const char* in, size_t in_length, char* out, size_t out_size);

#endif /* CHAN_DATACARD_CHAR_CONV_H_INCLUDED */
