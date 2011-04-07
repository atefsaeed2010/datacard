/*
   Copyright (C) 2011 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_PDISCOVERY_H_INCLUDED
#define CHAN_DATACARD_PDISCOVERY_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

/* return non-zero if found */
EXPORT_DECL int pdiscovery_lookup(const char * device, const char * imei, const char * imsi, char ** dport, char ** aport);
EXPORT_DECL void pdiscovery_init();
EXPORT_DECL void pdiscovery_fini();

#endif /* CHAN_DATACARD_PDISCOVERY_H_INCLUDED */
