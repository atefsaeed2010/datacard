/*
   Copyright (C) 2011 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_PDISCOVERY_H_INCLUDED
#define CHAN_DATACARD_PDISCOVERY_H_INCLUDED

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

typedef struct pdiscovery {
} pdiscovery_t;


/* return non-zero if found */
EXPORT_DECL int pdiscovery_lookup(struct pdiscovery * pdisc, const char * imei, const char * imsi, char ** dport, char ** aport);

#endif /* CHAN_DATACARD_PDISCOVERY_H_INCLUDED */
