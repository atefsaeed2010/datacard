/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "pdu.h"
#include "helpers.h"			/* dial_digit_code() */
#include "char_conv.h"			/* utf8_to_hexstr_ucs2() */

/* SMS-SUBMIT format
	SCA		1..12 octet(s)		Service Center Adress information element
	  octets
	        1		Length of Address (minimal 0)
	        2		Type of Address
	    3  12		Address

	PDU-type	1 octet			Protocol Data Unit Type
	  bits
	      1 0 MTI	Message Type Indicator Parameter describing the message type 00 means SMS-DELIVER 01 means SMS-SUBMIT
				0 0	SMS-DELIVER (SMSC ==> MS)
						or
					SMS-DELIVER REPORT (MS ==> SMSC, is generated automatically by the MOBILE, after receiving a SMS-DELIVER)

				0 1	SMS-SUBMIT (MS ==> SMSC)
						or
					SMS-SUBMIT REPORT (SMSC ==> MS)

				1 0	SMS-STATUS REPORT (SMSC ==> MS)
						or
					SMS-COMMAND (MS ==> SMSC)
				1 1	Reserved

		2 RD	Reject Duplicate
				0    Instruct the SMSC to accept an SMS-SUBMIT for an short message still
					held in the SMSC which has the same MR and
				1    Instruct the SMSC to reject an SMS-SUBMIT for an short message still
					held in the SMSC which has the same MR and DA as a previosly
					submitted short message from the same OA.

	      4 3 VPF	Validity Period Format	Parameter indicating whether or not the VP field is present
	      			0 0	VP field is not present
	      			0 1	Reserved
	      			1 0	VP field present an integer represented (relative)
	      			1 1	VP field present an semi-octet represented (absolute)

		5 SRR	Status Report Request Parameter indicating if the MS has requested a status report
				0	A status report is not requested
				1	A status report is requested

		6 UDHI	User Data Header Indicator Parameter indicating that the UD field contains a header
				0	The UD field contains only the short message
				1	The beginning of the UD field contains a header in addition of the short message
		7 RP	Reply Path Parameter indicating that Reply Path exists
				0	Reply Path parameter is not set in this PDU
				1	Reply Path parameter is set in this PDU

	MR		1 octet		Message Reference
						The MR field gives an integer (0..255) representation of a reference number of the SMSSUBMIT submitted to the SMSC by the MS.
						! notice: at the MOBILE the MR is generated automatically, -anyway you have to generate it a possible entry is for example ”00H” !
	DA		2-12 octets	Destination Address
	  octets
		1	Length of Address (of BCD digits!)
		2	Type of Address
	     3 12	Address
	PID		1 octet		Protocol Identifier
						The PID is the information element by which the Transport Layer either refers to the higher
						layer protocol being used, or indicates interworking with a certain type of telematic device.
						here are some examples of PID codings:
		00H: The PDU has to be treat as a short message
		41H: Replace Short Message Type1
		  ....
		47H: Replace Short Message Type7
					Another description:
		
		Bit7 bit6 (bit 7 = 0, bit 6 = 0)
		l 0 0 Assign bits 0..5, the values are defined as follows.
		l 1 0 Assign bits 0..5, the values are defined as follows.
		l 0 1 Retain
		l 1 1 Assign bits 0..5 for special use of SC
		Bit5 values:
		l 0: No interworking, but SME-to-SME protocol
		l 1: Telematic interworking (in this situation , value of bits4...0 is
		valid)
		Interface Description for HUAWEI EV-DO Data Card AT Commands
		All rights reserved Page 73 , Total 140
		Bit4...Bit0: telematic devices type identifier. If the value is 1 0 0 1 0, it
		indicates email. Other values are not supported currently.
		
		
	DCS		1 octet			Data Coding Scheme
		
	VP		0,1,7 octet(s)		Validity Period
	UDL		1 octet			User Data Length
	UD		0-140 octets		User Data
*/

#define NUMBER_TYPE_INTERNATIONAL	145

/* Message Type Indicator Parameter */
#define PDUTYPE_MTI_SMS_DELIVER			0x00
#define PDUTYPE_MTI_SMS_DELIVER_REPORT		0x00
#define PDUTYPE_MTI_SMS_SUBMIT			0x01
#define PDUTYPE_MTI_SMS_SUBMIT_REPORT		0x01
#define PDUTYPE_MTI_SMS_STATUS_REPORT		0x02
#define PDUTYPE_MTI_SMS_COMMAND			0x02
#define PDUTYPE_MTI_RESERVED			0x03

/* Reject Duplicate */
#define PDUTYPE_RD_ACCEPT			(0x00 << 2)
#define PDUTYPE_RD_REJECT			(0x01 << 2)

/* Validity Period Format */
#define PDUTYPE_VPF_NOT_PRESENT			(0x00 << 3)
#define PDUTYPE_VPF_RESERVED			(0x01 << 3)
#define PDUTYPE_VPF_RELATIVE			(0x02 << 3)
#define PDUTYPE_VPF_ABSOLUTE			(0x03 << 3)

/* Status Report Request */
#define PDUTYPE_SRR_NOT_REQUESTED		(0x00 << 5)
#define PDUTYPE_SRR_REQUESTED			(0x01 << 5)

/* User Data Header Indicator */
#define PDUTYPE_UDHI_ONLY_SMS			(0x00 << 6)
#define PDUTYPE_UDHI_HAS_HEADER			(0x01 << 6)

/* eply Path Parameter */
#define PDUTYPE_RP_IS_NOT_SET			(0x00 << 7)
#define PDUTYPE_RP_IS_SET			(0x01 << 7)

#define PDU_MESSAGE_REFERENCE			0x00		/* assigned by MS */
#define PDU_PID_SMS				0x00		/* bit5 No interworking, but SME-to-SME protocol */
#define PDU_PID_EMAIL				0x32		/* bit5 Telematic interworking, bits 4..0 0x 12 email */
#define PDU_DCS					0x08		/* UCS2, Not compressed, bits01 retained */



#define ROUND_UP2(x)		(((x) + 1) & (0xFFFFFFFF << 1))
#define LENGTH2BYTES(x)		(((x) + 1)/2)

#/* convert minutes to relative VP value */
static int get_relative_validity(unsigned minutes)
{
#define DIV_UP(x,y)	(((x)+(y)-1)/(y))
/*
	0 ... 143  (vp + 1) * 5 minutes				   5  ...   720		vp = m / 5 - 1
	144...167  12 hours + (vp - 143) * 30 minutes		 750  ...  1440		(m - 720) / 30 + 143
	168...196  (vp - 166) * 1 day				2880  ... 43200		m / 1440 + 166
	197...255  (vp - 192) * 1 week			       50400  ...635040		m / 10080 + 192
*/
	int validity;
	if(minutes <= 720)
		validity = DIV_UP(minutes, 5) - 1;
	else if(minutes <= 1440)
		validity = DIV_UP(minutes, 30) + 167;
	else if(minutes <= 43200)
		validity = DIV_UP(minutes, 1440) + 166;
	else if(minutes <= 635040)
		validity = DIV_UP(minutes, 10080) + 192;
	else
		validity = 0xFF;
	return validity;
#undef DIV_UP
}

/*!
 * \brief Store number in PDU 
 * \param buffer -- pointer to place where number will be stored, CALLER MUST be provide length + 2 bytes of buffer
 * \param number -- phone number w/o leading '+'
 * \param length -- length of number
 * \return number of bytes written to buffer
 */
static int store_number(char* buffer, const char* number, unsigned length)
{
	int i;
	for(i = 0; length > 1; length -=2, i +=2)
	{
		buffer[i] = dial_digit_code(number[i + 1]);
		buffer[i + 1] = dial_digit_code(number[i]);
	}
	
	if(length)
	{
		buffer[i] = 'F';
		buffer[i+1] = dial_digit_code(number[i]);
		i += 2;
	}
	return i;
}


/*!
 * \brief Build PDU text for SMS
 * \param buffer -- pointer to place where PDU will be stored
 * \param length -- length of buffer
 * \param csca -- number of SMS center may be with leading '+' in International format
 * \param dst -- destination number for SMS may be with leading '+' in International format
 * \param msg -- SMS message in utf-8
 * \param valid_minutes -- Validity period
 * \param srr -- Status Report Request
 * \param sca_len -- pointer where length of SCA header (in bytes) will be stored
 * \return number of bytes written to buffer w/o trailing 0x1A or 0, -1 if buffer too short, -2 on iconv recode errors
 */
EXPORT_DEF int build_pdu(char* buffer, unsigned length, const char* csca, const char* dst, const char* msg, unsigned valid_minutes, int srr, int* sca_len)
{
	char tmp;
	int len = 0;
	int data_len;

	int csca_toa = NUMBER_TYPE_INTERNATIONAL;
	int dst_toa = NUMBER_TYPE_INTERNATIONAL;
	int pdutype= PDUTYPE_MTI_SMS_SUBMIT | PDUTYPE_RD_ACCEPT | PDUTYPE_VPF_RELATIVE | PDUTYPE_SRR_NOT_REQUESTED | PDUTYPE_UDHI_ONLY_SMS | PDUTYPE_RP_IS_NOT_SET;
	
	/* TODO: check numbers */
	if(csca[0] == '+')
		csca++;

	if(dst[0] == '+') 
		dst++;

	/* count length of strings */
	unsigned csa_len = strlen(csca);
	unsigned dst_len = strlen(dst);
	unsigned msg_len = strlen(msg);

	/* check buffer has enougth space */
	if(length < ((csa_len == 0 ? 2 : 4 + ROUND_UP2(csa_len)) + 8 + ROUND_UP2(dst_len) + 8 + msg_len * 4 + 4))
		return -1;

	/* SCA Length */
	/* Type-of-address of the SMSC */
	/* Address of SMSC */
	if(csa_len)
	{
		len += snprintf(buffer + len, length - len, "%02X%02X", 1 + LENGTH2BYTES(csa_len), csca_toa);
		len += store_number(buffer + len, csca, csa_len);
		*sca_len = len;
	}
	else
	{
		buffer[len++] = '0';
		buffer[len++] = '0';
		*sca_len = 2;
	}

	if(srr)
		pdutype |= PDUTYPE_SRR_REQUESTED;

	/* PDU-type */
	/* TP-Message-Reference. The "00" value here lets the phone set the message reference number itself */
	/* Address-Length */
	/* Type-of-address of the sender number */
	len += snprintf(buffer + len, length - len, "%02X%02X%02X%02X", pdutype, PDU_MESSAGE_REFERENCE, dst_len, dst_toa);

	/*  Destination address */
	len += store_number(buffer + len, dst, dst_len);
	
	/* TP-PID. Protocol identifier  */
	/* TP-DCS. Data coding scheme */
	/* TP-Validity-Period */
	/* TP-User-Data-Length */
	/* TP-User-Data */
	data_len = utf8_to_hexstr_ucs2(msg, msg_len, buffer + len + 8, length - len - 11);
	if(data_len < 0)
	{
		return -2;
	}
	/* TODO: check message limit in 178 octet of TPDU (w/o SCA) */
	tmp = buffer[len + 8];
	len += snprintf(buffer + len, length - len, "%02X%02X%02X%02X", PDU_PID_SMS, PDU_DCS, get_relative_validity(valid_minutes), data_len / 2);
	buffer[len] = tmp;

	len += data_len;
	return len;
}

