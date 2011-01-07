/*
   Copyright (C) 2010,2011 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_MUTILS_H_INCLUDED
#define CHAN_DATACARD_MUTILS_H_INCLUDED

#include "export.h"

#define ITEMS_OF(x)				(sizeof(x)/sizeof((x)[0]))
#define STRLEN(string)				(sizeof(string)-1)

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

INLINE_DECL const char * enum2str_def(unsigned value, const char * const names[], unsigned items, const char * def)
{
	const char * name;
	if(value < items)
		name = names[value];
	else
		name = def;
	return name;
}

INLINE_DECL const char * enum2str(unsigned value, const char * const names[], unsigned items)
{
	return enum2str_def(value, names, items, "unknown");
}

#endif /* CHAN_DATACARD_MUTILS_H_INCLUDED */
