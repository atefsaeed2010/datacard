/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>
*/

#ifndef ____RINGBUFFER_H__
#define ____RINGBUFFER_H__

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

struct iovec;

struct ringbuffer
{
	void*	buffer;			/*!< pointer to data buffer */
	size_t	size;			/*!< size of buffer */
	size_t	used;			/*!< number of bytes used */
	size_t	read;			/*!< read position */
	size_t	write;			/*!< write position */
};

EXPORT_DECL inline void rb_init (struct ringbuffer*, void*, size_t);

EXPORT_DECL size_t rb_used (const struct ringbuffer*);
EXPORT_DECL size_t rb_free (const struct ringbuffer*);
EXPORT_DECL int rb_memcmp (const struct ringbuffer*, const char*, size_t);

/*!< fill io vectors array with readed data (situable for writev()) and return number of io vectors updated  */
EXPORT_DECL int rb_read_all_iov (const struct ringbuffer* rb, struct iovec* iov);

/*!< fill io vectors array and return number of io vectors updated for reading len bytes */
EXPORT_DECL int rb_read_n_iov (const struct ringbuffer* rb, struct iovec* iov, size_t len);

EXPORT_DECL int rb_read_until_char_iov (const struct ringbuffer*, struct iovec*, char);
EXPORT_DECL int rb_read_until_mem_iov (const struct ringbuffer*, struct iovec*, const void*, size_t);

/*!< advice read position to len bytes */
EXPORT_DECL size_t rb_read_upd (struct ringbuffer* rb, size_t len);

/*!< fill io vectors array with free data (situable for readv()) and return number of io vectors updated  */
EXPORT_DECL int rb_write_iov (const struct ringbuffer*, struct iovec*);

/*!< advice write position to len bytes */
EXPORT_DECL size_t rb_write_upd (struct ringbuffer*, size_t);
EXPORT_DECL size_t rb_write (struct ringbuffer*, char*, size_t);

#endif /* ____RINGBUFFER_H__ */
