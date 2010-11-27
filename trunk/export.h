/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_EXPORT_H_INCLUDED
#define CHAN_DATACARD_EXPORT_H_INCLUDED

#ifdef BUILD_SINGLE

#define EXPORT_DEF		static
#define EXPORT_DECL		static

#else /* BUILD_SINGLE */

#define EXPORT_DEF
#define EXPORT_DECL		extern

#endif /* BUILD_SINGLE */
#endif /* CHAN_DATACARD_EXPORT_H_INCLUDED */
