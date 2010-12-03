/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE			/* vasprintf() in asterisk/utils.h */
#endif /* #ifndef _GNU_SOURCE */

#include <sys/types.h>
#include <errno.h>

#include <asterisk.h>
#include <asterisk/channel.h>		/* ast_waitfor_n_fd() */
#include <asterisk/logger.h>		/* ast_debug() */

#include "chan_datacard.h"
#include "at_read.h"
#include "ringbuffer.h"


/*!
 * \brief Wait for activity on an socket
 * \param pvt -- pvt struct
 * \param ms  -- pointer to an int containing a timeout in ms
 * \return 0 on timeout and the socket fd (non-zero) otherwise
 * \retval 0 timeout
 */

EXPORT_DEF int at_wait (struct pvt* pvt, int* ms)
{
	int exception, outfd;

	outfd = ast_waitfor_n_fd (&pvt->data_fd, 1, ms, &exception);

	if (outfd < 0)
	{
		outfd = 0;
	}

	return outfd;
}

EXPORT_DEF int at_read (struct pvt* pvt, struct ringbuffer* rb)
{
	struct iovec	iov[2];
	int		iovcnt;
	ssize_t		n;

	/* TODO: read until major error */
	iovcnt = rb_write_iov (rb, iov);

	if (iovcnt > 0)
	{
		n = readv (pvt->data_fd, iov, iovcnt);

		if (n < 0)
		{
			if (errno != EINTR && errno != EAGAIN)
			{
				ast_debug (1, "[%s] readv() error: %d\n", PVT_ID(pvt), errno);
				return -1;
			}

			return 0;
		}
		else if (n == 0)
		{
			return -1;
		}

		rb_write_upd (rb, n);

		ast_debug (5, "[%s] receive %zu byte, used %zu, free %zu, read %zu, write %zu\n", 
				PVT_ID(pvt), n, rb_used (rb), rb_free (rb), rb->read, rb->write);

		iovcnt = rb_read_all_iov (rb, iov);

		if (iovcnt > 0)
		{
			if (iovcnt == 2)
			{
				ast_debug (5, "[%s] [%.*s%.*s]\n", PVT_ID(pvt),
						(int) iov[0].iov_len, (char*) iov[0].iov_base,
							(int) iov[1].iov_len, (char*) iov[1].iov_base);
			}
			else
			{
				ast_debug (5, "[%s] [%.*s]\n", PVT_ID(pvt),
						(int) iov[0].iov_len, (char*) iov[0].iov_base);
			}
		}

		return 0;
	}

	ast_log (LOG_ERROR, "[%s] at cmd receive buffer overflow\n", PVT_ID(pvt));

	return -1;
}

EXPORT_DEF int at_read_result_iov (struct pvt* pvt, struct ringbuffer* rb, struct iovec iov[2])
{
	int	iovcnt = 0;
	int	res;
	size_t	s;

// FIXME: +CME ERROR:
	s = rb_used (rb);
	if (s > 0)
	{
//		ast_debug (5, "[%s] d_read_result %d len %d input [%.*s]\n", PVT_ID(pvt), pvt->d_read_result, s, MIN(s, rb->size - rb->read), (char*)rb->buffer + rb->read);
		
		if (pvt->d_read_result == 0)
		{
			res = rb_memcmp (rb, "\r\n", 2);
			if (res == 0)
			{
				rb_read_upd (rb, 2);
				pvt->d_read_result = 1;

				return at_read_result_iov (pvt, rb, iov);
			}
			else if (res > 0)
			{
				if (rb_memcmp (rb, "\n", 1) == 0)
				{
					ast_debug (5, "[%s] multiline response\n", PVT_ID(pvt));
					rb_read_upd (rb, 1);

					return at_read_result_iov (pvt, rb, iov);
				}

				if (rb_read_until_char_iov (rb, iov, '\r') > 0)
				{
					s = iov[0].iov_len + iov[1].iov_len + 1;
				}

				rb_read_upd (rb, s);

				return at_read_result_iov (pvt, rb, iov);
			}

			return 0;
		}
		else
		{
			if (rb_memcmp (rb, "+CSSI:", 6) == 0)
			{
				iovcnt = rb_read_n_iov (rb, iov, 8);
				if (iovcnt > 0)
				{
					pvt->d_read_result = 0;
				}

				return iovcnt;
			}
			else if (rb_memcmp (rb, "\r\n+CSSU:", 8) == 0 || rb_memcmp (rb, "\r\n+CMS ERROR:", 13) == 0 ||  rb_memcmp (rb, "\r\n+CMGS:", 8) == 0)
			{
				rb_read_upd (rb, 2);
				return at_read_result_iov (pvt, rb, iov);
			}
			else if (rb_memcmp (rb, "> ", 2) == 0)
			{
				pvt->d_read_result = 0;
				return rb_read_n_iov (rb, iov, 2);
			}
			else if (rb_memcmp (rb, "+CMGR:", 6) == 0 || rb_memcmp (rb, "+CNUM:", 6) == 0 || rb_memcmp (rb, "ERROR+CNUM:", 11) == 0 || rb_memcmp (rb, "+CLCC:", 6) == 0)
			{
				iovcnt = rb_read_until_mem_iov (rb, iov, "\n\r\nOK\r\n", 7);
				if (iovcnt > 0)
				{
					pvt->d_read_result = 0;
				}

				return iovcnt;
			}
			else
			{
				iovcnt = rb_read_until_mem_iov (rb, iov, "\r\n", 2);
				if (iovcnt > 0)
				{
					pvt->d_read_result = 0;
					s = iov[0].iov_len + iov[1].iov_len + 1;

					return rb_read_n_iov (rb, iov, s);
				}
			}
		}
	}

	return 0;
}

EXPORT_DEF at_res_t at_read_result_classification (struct ringbuffer * rb, unsigned len)
{
	at_res_t at_res = RES_UNKNOWN;
	unsigned idx;
	
	for(idx = at_responses.ids_first; idx < at_responses.ids; idx++)
	{
		if (rb_memcmp (rb, at_responses.responses[idx].id, at_responses.responses[idx].idlen) == 0)
		{
			at_res = at_responses.responses[idx].res;
			break;
		}
	}

	switch (at_res)
	{
		case RES_SMS_PROMPT:
			len = 2;
			break;

		case RES_CMGR:
			len += 7;
			break;

		case RES_CSSI:
			len = 8;
			break;
		default:
			len += 1;
			break;
	}
	
	rb_read_upd (rb, len);

//	ast_debug (5, "receive result '%s'\n", at_res2str (at_res));

	return at_res;
}
