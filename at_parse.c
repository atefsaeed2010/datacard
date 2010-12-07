/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>

   bg <bg_one@mail.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "memmem.h"

#include <asterisk.h>
#include <asterisk/logger.h>		/* ast_debug() */
#include <asterisk/utils.h>		/* ast_test_flag() */

#include <stdio.h>			/* NULL */
#include <errno.h>			/* errno */
#include <stdlib.h>			/* strtol */

#include "at_parse.h"
#include "helpers.h"			/* ITEMS_OF() */
#include "chan_datacard.h"
#include "pdu.h"			/* pdu_parse() */

/*!
 * \brief Parse a CNUM response
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the CNUM message is parsed
 * \return NULL on error (parse error) or a pointer to the subscriber number
 */

EXPORT_DEF char* at_parse_cnum (char* str, unsigned len)
{
	unsigned	i;
	int	state;
	char*	number = NULL;

	/*
	 * parse CNUM response in the following format:
	 * +CNUM: "<name>","<number>",<type>
	 */

	for (i = 0, state = 0; i < len && state < 5; i++)
	{
		switch (state)
		{
			case 0: /* search for start of the name (") */
				if (str[i] == '"')
				{
					state++;
				}
				break;

			case 1: /* search for the end of the name (") */
				if (str[i] == '"')
				{
					state++;
				}
				break;

			case 2: /* search for the start of the number (") */
				if (str[i] == '"')
				{
					state++;
				}
				break;

			case 3: /* mark the number */
				number = &str[i];
				state++;
				/* fall through */

			case 4: /* search for the end of the number (") */
				if (str[i] == '"')
				{
					str[i] = '\0';
					state++;
				}
				break;
		}
	}

	if (state != 5)
	{
		return NULL;
	}

	return number;
}

/*!
 * \brief Parse a COPS response
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the COPS message is parsed
 * \return NULL on error (parse error) or a pointer to the provider name
 */

EXPORT_DEF char* at_parse_cops (char* str, unsigned len)
{
	unsigned	i;
	int	state;
	char*	provider = NULL;

	/*
	 * parse COPS response in the following format:
	 * +COPS: <mode>[,<format>,<oper>]
	 */

	for (i = 0, state = 0; i < len && state < 3; i++)
	{
		switch (state)
		{
			case 0: /* search for start of the provider name (") */
				if (str[i] == '"')
				{
					state++;
				}
				break;

			case 1: /* mark the provider name */
				provider = &str[i];
				state++;
				/* fall through */

			case 2: /* search for the end of the provider name (") */
				if (str[i] == '"')
				{
					str[i] = '\0';
					state++;
				}
				break;
		}
	}

	if (state != 3)
	{
		return NULL;
	}

	return provider;
}

/*!
 * \brief Parse a CREG response
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \param gsm_reg -- a pointer to a int
 * \param gsm_reg_status -- a pointer to a int
 * \param lac -- a pointer to a char pointer which will store the location area code in hex format
 * \param ci  -- a pointer to a char pointer which will store the cell id in hex format
 * @note str will be modified when the CREG message is parsed
 * \retval  0 success
 * \retval -1 parse error
 */

EXPORT_DEF int at_parse_creg (char* str, unsigned len, int* gsm_reg, int* gsm_reg_status, char** lac, char** ci)
{
	unsigned	i;
	int	state;
	char*	p1 = NULL;
	char*	p2 = NULL;
	char*	p3 = NULL;
	char*	p4 = NULL;

	*gsm_reg = 0;
	*gsm_reg_status = -1;
	*lac = NULL;
	*ci  = NULL;

	/*
	 * parse CREG response in the following format:
	 * +CREG: [<p1>,]<p2>[,<p3>,<p4>]
	 */

	for (i = 0, state = 0; i < len && state < 8; i++)
	{
		switch (state)
		{
			case 0:
				if (str[i] == ':')
				{
					state++;
				}
				break;

			case 1:
				if (str[i] != ' ')
				{
					p1 = &str[i];
					state++;
				}
				/* fall through */

			case 2:
				if (str[i] == ',')
				{
					str[i] = '\0';
					state++;
				}
				break;

			case 3:
				p2 = &str[i];
				state++;
				/* fall through */

			case 4:
				if (str[i] == ',')
				{
					str[i] = '\0';
					state++;
				}
				break;

			case 5:
				if (str[i] != ' ')
				{
					p3 = &str[i];
					state++;
				}
				/* fall through */

			case 6:
				if (str[i] == ',')
				{
					str[i] = '\0';
					state++;
				}
				break;

			case 7:
				if (str[i] != ' ')
				{
					p4 = &str[i];
					state++;
				}
				break;
		}
	}

	if (state < 2)
	{
		return -1;
	}

	if ((p2 && !p3 && !p4) || (p2 && p3 && p4))
	{
		p1 = p2;
	}

	if (p1)
	{
		errno = 0;
		*gsm_reg_status = (int) strtol (p1, (char**) NULL, 10);
		if (*gsm_reg_status == 0 && errno == EINVAL)
		{
			*gsm_reg_status = -1;
			return -1;
		}

		if (*gsm_reg_status == 1 || *gsm_reg_status == 5)
		{
			*gsm_reg = 1;
		}
	}

	if (p2 && p3 && !p4)
	{
		*lac = p2;
		*ci  = p3;
	}
	else if (p3 && p4)
	{
		*lac = p3;
		*ci  = p4;
	}

	return 0;
}

/*!
 * \brief Parse a CMTI notification
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the CMTI message is parsed
 * \return -1 on error (parse error) or the index of the new sms message
 */

EXPORT_DEF int at_parse_cmti (struct pvt* pvt, const char* str)
{
	int index = -1;

	/*
	 * parse cmti info in the following format:
	 * +CMTI: <mem>,<index> 
	 */

	if (sscanf (str, "+CMTI: %*[^,],%d", &index) != 1)
	{
		ast_debug(2, "[%s] Error parsing CMTI event '%s'\n", PVT_ID(pvt), str);
		return -1;
	}

	return index;
}


static const char*  parse_cmgr_text (char** str, size_t len, char* oa, size_t oa_len, str_encoding_t* oa_enc, char** msg, str_encoding_t* msg_enc)
{
	/*
	 * parse cmgr info in the following TEXT format:
	 * +CMGR: "<msg status>","+123456789",,timestamp<CR><LF>
	 * <message text><CR><LF><CR><LF>
	 * OK<CR><LF>
	 *	or
	 * +CMGR: "<msg status>","002B....",,timestamp<CR><LF>
	 * <message text><CR><LF><CR><LF>
	 * OK<CR><LF>
	 */
	
	static const char char_search[] = {',', '"', '"', '\n', '\r'};
	char* sym;
	char* number = NULL;
	unsigned i;
	
	for(i = 0; i < ITEMS_OF(char_search); ++i)
	{
		sym = memchr(*str, char_search[i], len);
		if(!sym)
			break;
		len -= sym + 1 - *str;
		*str = sym + 1;
		switch(i)
		{
			case 1:
				number = *str;
				break;
			case 2:
				(*str)[-1] = 0;
				/* TODO: check encoding */
				*oa_enc = STR_ENCODING_UNKNOWN;
				if(oa_len < (size_t)(*str - number))
					return "Not enought space for store number";
				memcpy(oa, number, *str - number);
				break;
			case 3:
				*msg = *str;
				/* TODO: check encoding */
				*msg_enc = STR_ENCODING_UNKNOWN;
				return 0;
		}
	}
	return "Can't parse CMGR text";
}

static const char* parse_cmgr_pdu (char** str, size_t len, char* oa, size_t oa_len, str_encoding_t* oa_enc, char** msg, str_encoding_t* msg_enc)
{
	/*
	 * parse cmgr info in the following PDU format
	 * +CMGR: message_status,[address_text],TPDU_length<CR><LF>
	 * SMSC_number_and_TPDU<CR><LF><CR><LF>
	 * OK<CR><LF>
	 *
	 *	sample
	 * +CMGR: 1,,31
	 * 07911234567890F3040B911234556780F20008012150220040210C041F04400438043204350442<CR><LF><CR><LF>
	 * OK<CR><LF>
	 */

	static const char char_search[] = { ',', ',', '\n'};
	char * sym;
	char * end;
	unsigned i;
	int tpdu_length = 0;

	for(i = 0; i < ITEMS_OF(char_search); ++i)
	{
		sym = memchr(*str, char_search[i], len);
		if(!sym)
			break;
		len -= sym + 1 - *str;
		*str = sym + 1;
		switch(i)
		{
			case 1:
				tpdu_length = strtol(*str, &end, 10);
				if(tpdu_length <= 0 || end[0] != '\r')
					return "Invalid TPDU length in CMGR PDU status line";
				break;
			case 2:
				return pdu_parse(str, tpdu_length, oa, oa_len, oa_enc, msg, msg_enc);
		}
	}
	return "Can't parse CMGR text";
}

/*!
 * \brief Parse a CMGR message
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \param number -- a pointer to a char pointer which will store the from number
 * \param text -- a pointer to a char pointer which will store the message text
 * @note str will be modified when the CMGR message is parsed
 * \retval  0 success
 * \retval -1 parse error
 */

EXPORT_DEF const char* at_parse_cmgr (char** str, size_t len, char* oa, size_t oa_len, str_encoding_t* oa_enc, char** msg, str_encoding_t* msg_enc)
{
	const char* rv = "Can't parse CMGR first line";

	/* skip "+CMGR:" */
	*str += 6;
	len -= 6;

	/* skip leading spaces */
	while(len > 0 && str[0][0] == ' ')
	{
		(*str)++;
		len--;
	}

	if(len > 0)
	{
		/* check PDU or TEXT mode */
		const char* (*fptr)(char** str, size_t len, char* num, size_t num_len, str_encoding_t * oa_enc, char** msg, str_encoding_t * msg_enc);
		fptr = *str[0] == '"' ? parse_cmgr_text : parse_cmgr_pdu;
		rv = (*fptr)(str, len, oa, oa_len, oa_enc, msg, msg_enc);
	}

	return rv;
}

 /*!
 * \brief Parse a CUSD answer
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * @note str will be modified when the CUSD string is parsed
 * \retval  0 success
 * \retval -1 parse error
 */

EXPORT_DEF int at_parse_cusd (char* str, size_t len, char** cusd, unsigned char* dcs)
{
	size_t	i;
	int	state;
	char*	p = NULL;

	*cusd = NULL;
	*dcs  = 0;

	/*
	 * parse cusd message in the following format:
	 * +CUSD: 0,"100,00 EURO, valid till 01.01.2010, you are using tariff "Mega Tariff". More informations *111#.",15
	 */

	for (i = 0, state = 0; i < len && state < 5; i++)
	{
		switch (state)
		{
			case 0:
				if (str[i] == '"')
				{
					state++;
				}
				break;

			case 1:
				*cusd = &str[i];
				state++;
				break;

			case 2:
				if (str[i] == '"')
				{
					str[i] = '\0';
					state++;
				}
				break;

			case 3:
				if (str[i] == ',')
				{
					state++;
				}
				break;

			case 4:
				p = &str[i];
				state++;
				break;
		}
	}

	if (state != 5)
	{
		return -1;
	}

	errno = 0;
	*dcs = (unsigned char) strtol (p, (char**) NULL, 10);
	if (errno == EINVAL)
	{
		return -1;
	}

	return 0;
}

/*!
 * \brief Parse a CPIN notification
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \return  2 if PUK required
 * \return  1 if PIN required
 * \return  0 if no PIN required
 * \return -1 on error (parse error) or card lock
 */

EXPORT_DEF int at_parse_cpin (struct pvt* pvt, char* str, size_t len)
{
	if (memmem (str, len, "READY", 5))
	{
		return 0;
	}
	if (memmem (str, len, "SIM PIN", 7))
	{
		ast_log (LOG_ERROR, "Datacard %s needs PIN code!\n", PVT_ID(pvt));
		return 1;
	}
	if (memmem (str, len, "SIM PUK", 7))
	{
		ast_log (LOG_ERROR, "Datacard %s needs PUK code!\n", PVT_ID(pvt));
		return 2;
	}

	ast_log (LOG_ERROR, "[%s] Error parsing +CPIN message: %s\n", PVT_ID(pvt), str);

	return -1;
}

/*!
 * \brief Parse +CSQ response
 * \param pvt -- pvt struct
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

EXPORT_DEF int at_parse_csq (struct pvt* pvt, const char* str, int* rssi)
{
	/*
	 * parse +CSQ response in the following format:
	 * +CSQ: <RSSI>,<BER>
	 */

	*rssi = -1;

	if (sscanf (str, "+CSQ: %2d,", rssi) != 1)
	{
		ast_debug (2, "[%s] Error parsing +CSQ result '%s'\n", PVT_ID(pvt), str);
		return -1;
	}

	return 0;
}

/*!
 * \brief Parse a ^RSSI notification
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \return -1 on error (parse error) or the rssi value
 */

EXPORT_DEF int at_parse_rssi (struct pvt* pvt, const char* str)
{
	int rssi = -1;

	/*
	 * parse RSSI info in the following format:
	 * ^RSSI:<rssi>
	 */

	if (sscanf (str, "^RSSI:%d", &rssi) != 1)
	{
		ast_debug (2, "[%s] Error parsing RSSI event '%s'\n", PVT_ID(pvt), str);
		return -1;
	}

	return rssi;
}

/*!
 * \brief Parse a ^MODE notification (link mode)
 * \param pvt -- pvt structure
 * \param str -- string to parse (null terminated)
 * \param len -- string lenght
 * \return -1 on error (parse error) or the the link mode value
 */

EXPORT_DEF int at_parse_mode (struct pvt* pvt, char* str, size_t len, int* mode, int* submode)
{
	/*
	 * parse RSSI info in the following format:
	 * ^MODE:<mode>,<submode>
	 */

	*mode    = -1;
	*submode = -1;

	if (!sscanf (str, "^MODE:%d,%d", mode, submode))
	{
		ast_debug (2, "[%s] Error parsing MODE event '%.*s'\n", PVT_ID(pvt), (int) len, str);
		return -1;
	}

	return 0;
}
