/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>
*/

#include <asterisk.h>
#include <asterisk/logger.h>			/* ast_debug() */
#include <asterisk/timing.h>			/* ast_timer_set_rate() */
#include <asterisk/pbx.h>			/* ast_pbx_start() */

#include "at_response.h"
#include "helpers.h"				/* STRLEN() */
#include "at_queue.h"
#include "chan_datacard.h"
#include "at_parse.h"
#include "char_conv.h"
#include "manager.h"
#include "channel.h"				/* channel_queue_hangup() channel_queue_control() */

#define DEF_STR(str)	str,STRLEN(str)

#define CCWA_STATUS_NOT_ACTIVE	0
#define CCWA_STATUS_ACTIVE	1

#define CLCC_CALL_TYPE_VOICE	0
#define CLCC_CALL_TYPE_DATA	1
#define CLCC_CALL_TYPE_FAX	2

/* magic!!! must be in same order as elements of enums in at_res_t */
static const at_response_t at_responses_list[] = {
	{ RES_PARSE_ERROR,"PARSE ERROR", 0, 0 },
	{ RES_UNKNOWN,"UNKNOWN", 0, 0 },

	{ RES_BOOT,"^BOOT",DEF_STR("^BOOT:") },
	{ RES_BUSY,"BUSY",DEF_STR("BUSY\r") },
	{ RES_CEND,"^CEND",DEF_STR("^CEND:") },
//	{ RES_CLIP,"+CLIP",DEF_STR("+CLIP:") },

	{ RES_CMGR, "+CMGR",DEF_STR("+CMGR:") },
	{ RES_CMS_ERROR, "+CMS ERROR",DEF_STR("+CMS ERROR:") },
	{ RES_CMTI, "+CMTI",DEF_STR("+CMTI:") },
	{ RES_CNUM, "+CNUM",DEF_STR("+CNUM:") },		// and "ERROR+CNUM:"

	{ RES_CONF,"^CONF",DEF_STR("^CONF:") },
	{ RES_CONN,"^CONN",DEF_STR("^CONN:") },
	{ RES_COPS,"+COPS",DEF_STR("+COPS:") },
	{ RES_CPIN,"+CPIN",DEF_STR("+CPIN:") },

	{ RES_CREG,"+CREG",DEF_STR("+CREG:") },
	{ RES_CSQ,"+CSQ",DEF_STR("+CSQ:") },
	{ RES_CSSI,"+CSSI",DEF_STR("+CSSI:") },
	{ RES_CSSU,"+CSSU",DEF_STR("+CSSU:") },

	{ RES_CUSD,"+CUSD",DEF_STR("+CUSD:") },
	{ RES_ERROR,"ERROR",DEF_STR("ERROR\r") },		// and "COMMAND NOT SUPPORT\r"
	{ RES_MODE,"^MODE",DEF_STR("^MODE:") },
	{ RES_NO_CARRIER,"NO CARRIER",DEF_STR("NO CARRIER\r") },

	{ RES_NO_DIALTONE,"NO DIALTONE",DEF_STR("NO DIALTONE\r") },
	{ RES_OK,"OK",DEF_STR("OK\r") },
	{ RES_ORIG,"^ORIG",DEF_STR("^ORIG:") },
	{ RES_RING,"RING",DEF_STR("RING\r") },

	{ RES_RSSI,"^RSSI",DEF_STR("^RSSI:") },
	{ RES_SMMEMFULL,"^SMMEMFULL",DEF_STR("^SMMEMFULL:") },
	{ RES_SMS_PROMPT,"> ",DEF_STR("> ") },
	{ RES_SRVST,"^SRVST",DEF_STR("^SRVST:") },

	{ RES_CVOICE,"^CVOICE",DEF_STR("^CVOICE:") },
	{ RES_CMGS,"+CMGS",DEF_STR("+CMGS:") },
	{ RES_CMGS,"+CPMS",DEF_STR("+CPMS:") },
	{ RES_CSCA,"+CSCA",DEF_STR("+CSCA:") },

	{ RES_CLCC,"+CLCC", DEF_STR("+CLCC:") },
	{ RES_CCWA,"+CCWA", DEF_STR("+CCWA:") },

	// duplicated response undef other id
	{ RES_CNUM, "+CNUM",DEF_STR("ERROR+CNUM:") },
	{ RES_ERROR,"ERROR",DEF_STR("COMMAND NOT SUPPORT\r") },
	};
#undef DEF_STR

EXPORT_DEF const at_responses_t at_responses = { at_responses_list, 2, ITEMS_OF(at_responses_list), RES_MIN, RES_MAX};

/*!
 * \brief Get the string representation of the given AT response
 * \param res -- the response to process
 * \return a string describing the given response
 */

EXPORT_DEF const char* at_res2str (at_res_t res)
{
	if((int)res >= at_responses.name_first && (int)res <= at_responses.name_last)
		return at_responses.responses[res - at_responses.name_first].name;
	return "UNDEFINED";
}

/*!
 * \brief Handle OK response
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_ok (struct pvt* pvt, at_res_t res)
{
	const at_queue_task_t * task = at_queue_head_task (pvt);
	const at_queue_cmd_t * ecmd = at_queue_task_cmd(task);

	if ((ecmd) && (ecmd->res == RES_OK || ecmd->res == RES_CMGR))
	{
		switch (ecmd->cmd)
		{
			case CMD_AT:
			case CMD_AT_Z:
			case CMD_AT_E:
			case CMD_AT_U2DIAG:
			case CMD_AT_CGMI:
			case CMD_AT_CGMM:
			case CMD_AT_CGMR:
			case CMD_AT_CMEE:
			case CMD_AT_CGSN:
			case CMD_AT_CIMI:
			case CMD_AT_CPIN:
			case CMD_AT_CCWA_STATUS:
			case CMD_AT_CHLD_2:
			case CMD_AT_CSCA:
			case CMD_AT_CLCC:
			case CMD_AT_CLIR:
				ast_debug (1, "[%s] %s sent successfully\n", pvt->id, at_cmd2str (ecmd->cmd));
				break;

			case CMD_AT_COPS_INIT:
				ast_debug (1, "[%s] Operator select parameters set\n", pvt->id);
				break;

			case CMD_AT_CREG_INIT:
				ast_debug (1, "[%s] registration info enabled\n", pvt->id);
				break;

			case CMD_AT_CREG:
				ast_debug (1, "[%s] registration query sent\n", pvt->id);
				break;

			case CMD_AT_CNUM:
				ast_debug (1, "[%s] Subscriber phone number query successed\n", pvt->id);
				break;

			case CMD_AT_CVOICE:
				ast_debug (1, "[%s] Datacard has voice support\n", pvt->id);

				pvt->has_voice = 1;
				break;

			case CMD_AT_CLIP:
				ast_debug (1, "[%s] Calling line indication disabled\n", pvt->id);
				break;

			case CMD_AT_CSSN:
				ast_debug (1, "[%s] Supplementary Service Notification enabled successful\n", pvt->id);
				break;

			case CMD_AT_CMGF:
				pvt->use_pdu = pvt->send_sms_as_pdu;
				ast_debug (1, "[%s] SMS operation mode set to %s\n", pvt->id, pvt->use_pdu ? "PDU" : "TEXT");
				break;

			case CMD_AT_CSCS:
				ast_debug (1, "[%s] UCS-2 text encoding enabled\n", pvt->id);

				pvt->use_ucs2_encoding = 1;
				break;

			case CMD_AT_CPMS:
				ast_debug (1, "[%s] SMS storage location is established\n", pvt->id);
				break;

			case CMD_AT_CNMI:
				ast_debug (1, "[%s] SMS new message indication enabled\n", pvt->id);
				ast_debug (1, "[%s] Datacard has sms support\n", pvt->id);

				pvt->has_sms = 1;

				if (!pvt->initialized)
				{
					pvt->timeout = DATA_READ_TIMEOUT;
					pvt->initialized = 1;
					ast_verb (3, "Datacard %s initialized and ready\n", pvt->id);
				}
				break;

			case CMD_AT_D:
				pvt->dialing = 1;
				pvt->last_dialed_cpvt = task->cpvt;
				/* passthrow */

			case CMD_AT_A:
			case CMD_AT_CHLD_2x:
/* not work, ^CONN: appear before OK for CHLD_ANSWER */
//				task->cpvt->answered = 1;
//				task->cpvt->needhangup = 1;
				CPVT_SET_FLAGS(task->cpvt, CALL_FLAG_NEED_HANGUP);
				ast_debug (1, "[%s] %s sent successfully for call id %d\n", pvt->id, at_cmd2str (ecmd->cmd), task->cpvt->call_idx);
				break;

			case CMD_AT_CFUN:
				/* in case of reset */
				pvt->ring = 0;
				pvt->dialing = 0;
				pvt->cwaiting = 0;
				break;
			case CMD_AT_DDSETEX:
				ast_debug (1, "[%s] %s sent successfully\n", pvt->id, at_cmd2str (ecmd->cmd));
				if (!pvt->initialized)
				{
					pvt->timeout = DATA_READ_TIMEOUT;
					pvt->initialized = 1;
					ast_verb (3, "Datacard %s initialized and ready\n", pvt->id);
				}
				break;
			case CMD_AT_CHUP:
			case CMD_AT_CHLD_1x:
//				task->cpvt->needhangup = 0;
				CPVT_RESET_FLAG(task->cpvt, CALL_FLAG_NEED_HANGUP);
				ast_debug (1, "[%s] Successful hangup for call idx ?\n", pvt->id);
				break;

			case CMD_AT_CMGS:
				ast_debug (1, "[%s] Sending sms message in progress\n", pvt->id);
				break;

			case CMD_AT_SMSTEXT:
				pvt->outgoing_sms = 0;
				ast_log (LOG_NOTICE, "[%s] Successfully sent sms message\n", pvt->id);
				ast_debug (1, "[%s] Successfully sent sms message\n", pvt->id);
				break;

			case CMD_AT_DTMF:
				ast_debug (1, "[%s] DTMF sent successfully for call idx %d\n", pvt->id, task->cpvt->call_idx);
				break;

			case CMD_AT_CUSD:
				ast_log (LOG_NOTICE, "[%s] Successfully sent sms USSD\n", pvt->id);
				ast_debug (1, "[%s] CUSD code sent successfully\n", pvt->id);
				break;

			case CMD_AT_COPS:
				ast_debug (1, "[%s] Provider query successfully\n", pvt->id);
				break;

			case CMD_AT_CMGR:
				ast_debug (1, "[%s] SMS message see later\n", pvt->id);
				break;

			case CMD_AT_CMGD:
				ast_debug (1, "[%s] SMS message deleted successfully\n", pvt->id);
				break;

			case CMD_AT_CSQ:
				ast_debug (1, "[%s] Got signal strength result\n", pvt->id);
				break;
			case CMD_AT_CCWA_SET:
//				ast_log (LOG_NOTICE, "Call-Waiting successfully enabled or disabled on device %s\n", pvt->id);
				break;

			case CMD_AT_CLVL:
				pvt->volume_sync_step++;
				if(pvt->volume_sync_step == VOLUME_SYNC_DONE)
				{
					ast_debug (1, "[%s] Volume level synchronized\n", pvt->id);
					pvt->volume_sync_step = VOLUME_SYNC_BEGIN;
				}
				break;

			default:
				ast_log (LOG_ERROR, "[%s] Received 'OK' for unhandled command '%s'\n", pvt->id, at_cmd2str (ecmd->cmd));
				break;
		}

		at_queue_handle_result (pvt, res);
	}
	else if (ecmd)
	{
		ast_log (LOG_ERROR, "[%s] Received 'OK' when expecting '%s', ignoring\n", pvt->id, at_res2str (ecmd->res));
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Received unexpected 'OK'\n", pvt->id);
	}

	return 0;

//e_return:
//	at_queue_handle_result (pvt, res);

//	return -1;
}

/*!
 * \brief Handle ERROR response
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_error (struct pvt* pvt, at_res_t res)
{
	const at_queue_task_t * task = at_queue_head_task(pvt);
	const at_queue_cmd_t * ecmd = at_queue_task_cmd(task);

	if (ecmd && (ecmd->res == RES_OK || ecmd->res == RES_CMGR || ecmd->res == RES_SMS_PROMPT))
	{
		switch (ecmd->cmd)
		{
        		/* initilization stuff */
			case CMD_AT:
			case CMD_AT_Z:
			case CMD_AT_E:
			case CMD_AT_U2DIAG:
			case CMD_AT_CLCC:
				ast_log (LOG_ERROR, "[%s] Command '%s' failed\n", pvt->id, at_cmd2str (ecmd->cmd));
				goto e_return;

			case CMD_AT_CGMI:
				ast_log (LOG_ERROR, "[%s] Getting manufacturer info failed\n", pvt->id);
				goto e_return;

			case CMD_AT_CGMM:
				ast_log (LOG_ERROR, "[%s] Getting model info failed\n", pvt->id);
				goto e_return;

			case CMD_AT_CGMR:
				ast_log (LOG_ERROR, "[%s] Getting firmware info failed\n", pvt->id);
				goto e_return;

			case CMD_AT_CMEE:
				ast_log (LOG_ERROR, "[%s] Setting error verbosity level failed\n", pvt->id);
				goto e_return;

			case CMD_AT_CGSN:
				ast_log (LOG_ERROR, "[%s] Getting IMEI number failed\n", pvt->id);
				goto e_return;

			case CMD_AT_CIMI:
				ast_log (LOG_ERROR, "[%s] Getting IMSI number failed\n", pvt->id);
				goto e_return;

			case CMD_AT_CPIN:
				ast_log (LOG_ERROR, "[%s] Error checking PIN state\n", pvt->id);
				goto e_return;

			case CMD_AT_COPS_INIT:
				ast_log (LOG_ERROR, "[%s] Error setting operator select parameters\n", pvt->id);
				goto e_return;

			case CMD_AT_CREG_INIT:
				ast_log (LOG_ERROR, "[%s] Error enableling registration info\n", pvt->id);
				goto e_return;

			case CMD_AT_CREG:
				ast_debug (1, "[%s] Error getting registration info\n", pvt->id);
				break;

			case CMD_AT_CNUM:
				ast_log (LOG_ERROR, "[%s] Error checking subscriber phone number\n", pvt->id);
				ast_verb (3, "Datacard %s needs to be reinitialized. The SIM card is not ready yet\n", pvt->id);
				goto e_return;

			case CMD_AT_CVOICE:
				ast_debug (1, "[%s] Datacard has NO voice support\n", pvt->id);

				pvt->has_voice = 0;

				if (!pvt->initialized)
				{
					/* continue initialization in other job at cmd CMD_AT_CMGF */
					if (at_enque_initialization(task->cpvt, CMD_AT_CMGF))
					{
						ast_log (LOG_ERROR, "[%s] Error schedule initialization commands\n", pvt->id);
						goto e_return;
					}
				}
				break;

			case CMD_AT_CLIP:
				ast_log (LOG_ERROR, "[%s] Error enabling calling line indication\n", pvt->id);
				goto e_return;

			case CMD_AT_CSSN:
				ast_log (LOG_ERROR, "[%s] Error Supplementary Service Notification activation failed\n", pvt->id);
				goto e_return;

			case CMD_AT_CMGF:
			case CMD_AT_CPMS:
			case CMD_AT_CNMI:
				ast_debug (1, "[%s] Command '%s' failed\n", pvt->id, at_cmd2str (ecmd->cmd));
				ast_debug (1, "[%s] No SMS support\n", pvt->id);

				pvt->has_sms = 0;

				if (!pvt->initialized)
				{
					if (pvt->has_voice)
					{
						/* continue initialization in other job at cmd CMD_AT_CSQ */
						if (at_enque_initialization(task->cpvt, CMD_AT_CSQ))
						{
							ast_log (LOG_ERROR, "[%s] Error querying signal strength\n", pvt->id);
							goto e_return;
						}

						pvt->timeout = DATA_READ_TIMEOUT;
						pvt->initialized = 1;
						ast_verb (3, "Datacard %s initialized and ready\n", pvt->id);
					}

					goto e_return;
				}
				break;

			case CMD_AT_CSCS:
				ast_debug (1, "[%s] No UCS-2 encoding support\n", pvt->id);

				pvt->use_ucs2_encoding = 0;
				break;

				
			case CMD_AT_A:
			case CMD_AT_CHLD_2x:
				ast_log (LOG_ERROR, "[%s] Answer failed for call idx %d\n", pvt->id, task->cpvt->call_idx);
//				task->cpvt->answered = 0;
				channel_queue_hangup (task->cpvt, 0);
				break;

			case CMD_AT_CLIR:
				ast_log (LOG_ERROR, "[%s] Setting CLIR failed\n", pvt->id);
				break;
				

			case CMD_AT_CHLD_2:
				if(!CPVT_TEST_FLAG(task->cpvt, CALL_FLAG_HOLD_OTHER) || task->cpvt->state != CALL_STATE_INIT)
					break;
				/* passthru */
			case CMD_AT_D:
				ast_log (LOG_ERROR, "[%s] Dial failed\n", pvt->id);
				channel_queue_control (task->cpvt, AST_CONTROL_CONGESTION);
				break;

			case CMD_AT_DDSETEX:
				ast_log (LOG_ERROR, "[%s] AT^DDSETEX failed\n", pvt->id);
				break;

			case CMD_AT_CHUP:
			case CMD_AT_CHLD_1x:
				ast_log (LOG_ERROR, "[%s] Error sending hangup for call idx %d\n", pvt->id, task->cpvt->call_idx);
				goto e_return;

			case CMD_AT_CMGR:
				pvt->incoming_sms = 0;
				ast_log (LOG_ERROR, "[%s] Error reading SMS message\n", pvt->id);
				break;

			case CMD_AT_CMGD:
				pvt->incoming_sms = 0;
				ast_log (LOG_ERROR, "[%s] Error deleting SMS message\n", pvt->id);
				break;

			case CMD_AT_CMGS:
			case CMD_AT_SMSTEXT:
				ast_log (LOG_ERROR, "[%s] Error sending SMS message\n", pvt->id);
				pvt->outgoing_sms = 0;
				break;

			case CMD_AT_DTMF:
				ast_log (LOG_ERROR, "[%s] Error sending DTMF\n", pvt->id);
				break;

			case CMD_AT_COPS:
				ast_debug (1, "[%s] Could not get provider name\n", pvt->id);
				break;

			case CMD_AT_CLVL:
				ast_debug (1, "[%s] Audio level synchronization failed at step %d/%d\n", pvt->id, pvt->volume_sync_step, VOLUME_SYNC_DONE-1);
				pvt->volume_sync_step = VOLUME_SYNC_BEGIN;
				break;

			case CMD_AT_CUSD:
				ast_log (LOG_ERROR, "[%s] Could not send USSD code\n", pvt->id);
				break;

			default:
				ast_log (LOG_ERROR, "[%s] Received 'ERROR' for unhandled command '%s'\n", pvt->id, at_cmd2str (ecmd->cmd));
				break;
		}
		at_queue_handle_result (pvt, res);
	}
	else if (ecmd)
	{
		ast_log (LOG_ERROR, "[%s] Received 'ERROR' when expecting '%s', ignoring\n", pvt->id, at_res2str (ecmd->res));
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Received unexpected 'ERROR'\n", pvt->id);
	}

	return 0;

e_return:
	at_queue_handle_result (pvt, res);

	return -1;
}

/*!
 * \brief Handle ^RSSI response Here we get the signal strength.
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_rssi (struct pvt* pvt, const char* str)
{
	if ((pvt->rssi = at_parse_rssi (pvt, str)) == -1)
	{
		return -1;
	}

	return 0;
}

/*!
 * \brief Handle ^MODE response Here we get the link mode (GSM, UMTS, EDGE...).
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_mode (struct pvt* pvt, char* str, size_t len)
{
	return at_parse_mode (pvt, str, len, &pvt->linkmode, &pvt->linksubmode);
}

static void request_clcc(struct pvt* pvt)
{
	if (at_enque_clcc(&pvt->sys_chan))
	{
		ast_log (LOG_ERROR, "[%s] Error enque List Current Calls request\n", pvt->id);
	}
}

/*!
 * \brief Handle ^ORIG response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_orig (struct pvt* pvt, const char* str)
{
	int call_index;
	int call_type;
	struct cpvt * cpvt = pvt->last_dialed_cpvt;
	
	pvt->last_dialed_cpvt = NULL;
	if(!cpvt)
	{
		ast_log (LOG_ERROR, "[%s] ^ORIG '%s' for unknown ATD\n", pvt->id, str);
		return 0;
	}
	
	
	/*
	 * parse ORIG info in the following format:
	 * ^ORIG:<call_index>,<call_type>
	 */

	if (sscanf (str, "^ORIG:%d,%d", &call_index, &call_type) != 2)
	{
		ast_log (LOG_ERROR, "[%s] Error parsing ORIG event '%s'\n", pvt->id, str);
		return -1;
	}

	ast_debug (1, "[%s] ORIG Received call_index: %d call_type %d\n", pvt->id, call_index, call_type);

	if (call_type == CLCC_CALL_TYPE_VOICE)
	{
		
		if(call_index >= MIN_CALL_IDX && call_index <= MAX_CALL_IDX)
		{
			/* assign call idx */
//			cpvt->dir = CALL_DIR_OUTGOING;
			cpvt->call_idx = call_index;
			channel_change_state(cpvt, CALL_STATE_DIALING, 0);
/* TODO: move to CONN ? */
			if(pvt->volume_sync_step == VOLUME_SYNC_BEGIN)
			{
				pvt->volume_sync_step = VOLUME_SYNC_BEGIN;
				if (at_enque_volsync (cpvt))
				{
					ast_log (LOG_ERROR, "[%s] Error synchronize audio level\n", pvt->id);
				}
				else
					pvt->volume_sync_step++;
			}

			request_clcc(pvt);
		}
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] ORIG event for non-voice call type '%d' index %d\n", pvt->id, call_type, call_index);
// FIXME: and reset call if no-voice, bad call_index !
	}
	return 0;
}

/*!
 * \brief Handle ^CONF response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_conf (struct pvt* pvt, const char* str)
{
	int call_index;
	struct cpvt * cpvt;
	
	/*
	 * parse CONF info in the following format:
	 * ^CONF: <call_index>
	 */

	if (sscanf (str, "^CONF:%d", &call_index) != 1)
	{
		ast_log (LOG_ERROR, "[%s] Error parsing ORIG event '%s'\n", pvt->id, str);
		return -1;
	}

	ast_debug (1, "[%s] CONF Received call_index %d\n", pvt->id, call_index);

	cpvt = pvt_find_cpvt(pvt, call_index);
	if(cpvt)
	{
		channel_change_state(cpvt, CALL_STATE_ALERTING, 0);
	}

	return 0;
}



/*!
 * \brief Handle ^CEND response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cend (struct pvt* pvt, const char* str)
{
	int call_index = 0;
	int duration   = 0;
	int end_status = 0;
	int cc_cause   = 0;
	struct cpvt * cpvt;

	request_clcc(pvt);

	/*
	 * parse CEND info in the following format:
	 * ^CEND:<call_index>,<duration>,<end_status>[,<cc_cause>]
	 */

	if (sscanf (str, "^CEND:%d,%d,%d,%d", &call_index, &duration, &end_status, &cc_cause) != 4)
	{
		ast_debug (1, "[%s] Could not parse all CEND parameters\n", pvt->id);
	}

	ast_debug (1,	"[%s] CEND: call_index %d duration %d end_status %d cc_cause %d Line disconnected\n"
				, pvt->id, call_index, duration, end_status, cc_cause);

	cpvt = pvt_find_cpvt(pvt, call_index);
	if (cpvt)
	{
		if (cpvt->dir == CALL_DIR_INCOMING)
		{
			switch(cpvt->state)
			{
				case CALL_STATE_INCOMING:
					pvt->ring = 0;
					break;
				case CALL_STATE_WAITING:
					pvt->cwaiting = 0;
					break;
				default:;
			}
		}
		else
		{
			switch(cpvt->state)
			{
				case CALL_STATE_INIT:
				case CALL_STATE_DIALING:
				case CALL_STATE_ALERTING:
					pvt->dialing = 0;
				default:;
			}
		}
//		cpvt->needhangup = 0;
		CPVT_RESET_FLAG(cpvt, CALL_FLAG_NEED_HANGUP);
		channel_change_state(cpvt, CALL_STATE_RELEASED, cc_cause);
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] CEND event for unknown call idx '%d'\n", pvt->id, call_index);
	}

	return 0;
}

/*!
 * \brief Handle +CSCA response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */
static int at_response_csca (struct pvt* pvt, const char* str)
{
	char csca[201];

	/*
	 * parse CSCA info in the following format:
	 * CSCA: <SCA>,<TOSCA>
	 */
	if (sscanf (str, "+CSCA: \"%200[+0-9*#]\",%*d", csca) != 1)
	{
		ast_debug (1, "[%s] Could not parse all CSCA parameters\n", pvt->id);
		return 0;
	}
	ast_copy_string (pvt->sms_scenter, csca, sizeof (pvt->sms_scenter));

	ast_debug (1, "[%s] CSCA: %s\n", pvt->id, pvt->sms_scenter);
	return 0;
}


/*!
 * \brief Handle ^CONN response
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_conn (struct pvt* pvt, const char* str)
{
	int call_index;
	int call_type;
	struct cpvt * cpvt;

	pvt->ring = 0;
	pvt->dialing = 0;
	pvt->cwaiting = 0;
	
	request_clcc(pvt);
	
	/*
	 * parse CONN info in the following format:
	 * ^CONN:<call_index>,<call_type>
	 */
	if (sscanf (str, "^CONN:%d,%d", &call_index, &call_type) != 2)
	{
		ast_log (LOG_ERROR, "[%s] Error parsing CONN event '%s'\n", pvt->id, str);
		return -1;
	}

	ast_debug (1, "[%s] CONN Received call_index %d call_type %d\n", pvt->id, call_index, call_type);

	if (call_type == CLCC_CALL_TYPE_VOICE)
	{
		cpvt = pvt_find_cpvt(pvt, call_index);
		if(cpvt)
		{
// FIXME: delay until CLCC handle; as is create time when both channels in active state and read/write one device
			channel_change_state(cpvt, CALL_STATE_ACTIVE, 0);
			if (cpvt->dir == CALL_DIR_OUTGOING)
			{
				pvt->dialing = 0;
			}
			else if(cpvt->state == CALL_STATE_WAITING)
			{
				pvt->cwaiting = 0;
			}
		}
		else
		{
			at_enque_hangup(&pvt->sys_chan, call_index);
			ast_log (LOG_ERROR, "[%s] CONN event for non-existing call idx %d, hangup!\n", pvt->id, call_index);
		}
	}
	else
		ast_log (LOG_ERROR, "[%s] CONN event for non-voice call type '%d' index %d!\n", pvt->id, call_type, call_index);
	return 0;
}


static int start_pbx(struct pvt* pvt, const char * number, int call_idx, call_state_t state)
{
	struct cpvt* cpvt;
	struct ast_channel* channel = channel_new (pvt, AST_STATE_RING, number, call_idx, CALL_DIR_INCOMING, state);

	if (!channel)
	{
		ast_log (LOG_ERROR, "[%s] Unable to allocate channel for incoming call\n", pvt->id);

		if (at_enque_hangup (&pvt->sys_chan, call_idx))
		{
			ast_log (LOG_ERROR, "[%s] Error sending AT+CHUP command\n", pvt->id);
		}

		return -1;
	}
	cpvt = channel->tech_pvt;
//	cpvt->needchup = 1;
	CPVT_SET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);

	if (ast_pbx_start (channel))
	{
		ast_log (LOG_ERROR, "[%s] Unable to start pbx on incoming call\n", pvt->id);
		channel_ast_hangup (cpvt);

		return -1;
	}

	return 0;
}

/*!
 * \brief Handle +CLCC response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_clcc (struct pvt* pvt, const char* str)
{
	struct cpvt * cpvt;
	unsigned call_idx, dir, state, mode, mpty, type;
	unsigned all = 0;
	unsigned held = 0;
	char number[201];
	char *p;

	/*
	 * +CLCC:<id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]\r\n
	 *  ...
	 * +CLCC:<id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>[,<priority>]]]\r\n
	 */

	if (pvt->initialized)
	{
		for(;;)
		{
			if(sscanf(str, "+CLCC:%u,%u,%u,%u,%u,\"%200[+0-9*#]\",%u", &call_idx, &dir, &state, &mode, &mpty, number, &type) == 7)
			{
				ast_debug (3, "[%s] CLCC callidx %u dir %u state %u mode %u mpty %u number %s type %u\n",  pvt->id, call_idx, dir, state, mode, mpty, number, type);
				if(mode == CLCC_CALL_TYPE_VOICE && state <= CALL_STATE_WAITING)
				{
					cpvt = pvt_find_cpvt(pvt, call_idx);
					if(cpvt)
					{
						if(dir == cpvt->dir)
						{
							if(dir == CALL_DIR_INCOMING && (state == CALL_STATE_INCOMING || state == CALL_STATE_WAITING))
							{
								if(cpvt->channel)
								{
									cpvt->channel->rings += pvt->rings;
									pvt->rings = 0;
								}
							}
							if(state != cpvt->state)
							{
								channel_change_state(cpvt, state, 0);
							}
						}
						else
						{
							ast_log (LOG_ERROR, "[%s] CLCC listed call idx %d with different dir %d/%d\n", pvt->id, cpvt->call_idx, dir, cpvt->dir);
						}
					}
					else if(dir == CALL_DIR_INCOMING && (state == CALL_STATE_INCOMING || state == CALL_STATE_WAITING))
					{
						return start_pbx(pvt, number, call_idx, state);
					}

					all++;
					switch(state)
					{
						case CALL_STATE_WAITING:
							pvt->cwaiting = 1;
//							pvt->ring = 0;
//							pvt->dialing = 0;
							break;
						case CALL_STATE_ONHOLD:
							held++;
							break;
						case CALL_STATE_DIALING:
						case CALL_STATE_ALERTING:
							pvt->dialing = 1;
							pvt->cwaiting = 0;
							pvt->ring = 0;
							break;
						case CALL_STATE_INCOMING:
							pvt->ring = 1;
							pvt->dialing = 0;
							pvt->cwaiting = 0;
							break;
						default:;
					}
				}
			}
			else
			{
				ast_log (LOG_ERROR, "[%s] can't parse CLCC line '%s'\n", pvt->id, str);
			}
			p = strchr(str, '\r');
			if(p)
			{
				++p;
				if(p[0] == '\n')
					++p;
				if(p[0])
				{
					str = p;
					continue;
				}
			}
			break;
		}
		/* unhold first held call */
		if(all == held)
		{
/* HW BUG 2: when no active call exist not way to enable voice again on activated from hold call
	call will be activated but no voice
*/
			ast_debug (1, "[%s] all %u call held, try activate some\n",  pvt->id, all);
			if(at_enque_flip_hold(&pvt->sys_chan))
			{
				ast_log (LOG_ERROR, "[%s] can't flip active and hold/waiting calls \n", pvt->id);
			}
		}
	}
	return 0;
}

/*!
 * \brief Handle +CCWA response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_ccwa(struct pvt* pvt, const char* str)
{
	int status, class, n;
	
	/* 
	 * CCWA may be in form:
	 *	in response of AT+CCWA=?
	 *		+CCWA: (0,1)
	 *	in response of AT+CCWA=?
	 *		+CCWA: <n>
	 *	in response of "AT+CCWA=[<n>[,<mode>[,<class>]]]"
	 *		+CCWA: <status>,<class1>
	 *	unsolicited result code
	 *		+CCWA: <number>,<type>,<class>,[<alpha>][,<CLI validity>[,<subaddr>,<satype>[,<priority>]]]
	 *
	 */
	if (sscanf(str, "+CCWA: (%u-%u)", &status, &class) == 2)
		return 0;

	n = sscanf (str, "+CCWA:%d,%d", &status, &class);
	if(n == 1)
		return 0;
	else if (n == 2)
	{
		if ((class & CCWA_CLASS_VOICE) && (status == CCWA_STATUS_NOT_ACTIVE || status == CCWA_STATUS_ACTIVE))
		{
			pvt->has_call_waiting = status == CCWA_STATUS_ACTIVE ? 1 : 0;
			ast_log (LOG_NOTICE, "Call waiting is %s on device %s\n", status ? "enabled" : "disabled", pvt->id);
		}
		return 0;
	}

	if (pvt->initialized)
	{
		if (sscanf (str, "+CCWA: \"%*[+0-9*#]\",%*d,%d", &class) == 1)
		{
			if (pvt->call_waiting != CALL_WAITING_DISALLOWED && class == CCWA_CLASS_VOICE)
			{
				pvt->rings++;
				pvt->cwaiting = 1;
				request_clcc(pvt);
			}
		}
		else
			ast_log (LOG_ERROR, "[%s] can't parse CCWA line '%s'\n", pvt->id, str);
	}
	return 0;
}

/*!
 * \brief Handle RING response
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_ring (struct pvt* pvt)
{
	if (pvt->initialized)
	{
		pvt->ring = 1;
		pvt->dialing = 0;
		pvt->cwaiting = 0;

		pvt->rings++;

		request_clcc(pvt);
		/* We only want to syncronize volume on the first ring and if no channels yes */
		if (pvt->volume_sync_step == VOLUME_SYNC_BEGIN && PVT_NO_CHANS(pvt))
		{
			if (at_enque_volsync (&pvt->sys_chan))
			{
				ast_log (LOG_ERROR, "[%s] Error synchronize audio level\n", pvt->id);
			}
			else
				pvt->volume_sync_step++;
		}
	}

	return 0;
}

/*!
 * \brief Handle +CMTI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cmti (struct pvt* pvt, const char* str)
{
	int index = at_parse_cmti (pvt, str);

	if (index > -1)
	{
		ast_debug (1, "[%s] Incoming SMS message\n", pvt->id);

		if (pvt->disablesms)
		{
			ast_log (LOG_WARNING, "[%s] SMS reception has been disabled in the configuration.\n", pvt->id);
		}
		else
		{
			if (at_enque_retrive_sms (&pvt->sys_chan, index, pvt->auto_delete_sms))
			{
				ast_log (LOG_ERROR, "[%s] Error sending CMGR to retrieve SMS message\n", pvt->id);
				return -1;
			}
			else
			    pvt->incoming_sms = 1;
		}

		return 0;
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Error parsing incoming sms message alert, disconnecting\n", pvt->id);
		return -1;
	}
}

/*!
 * \brief Handle +CMGR response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cmgr (struct pvt* pvt, char* str, size_t len)
{
	ssize_t		res;
	char		sms_utf8_str[4096];
	char		from_number_utf8_str[1024];
	char		channel_name[1024];

	char*		from_number = NULL;
	char*		text = NULL;
	char		text_base64[16384];

	const struct at_queue_cmd * ecmd = at_queue_head_cmd (pvt);

	if (ecmd && ecmd->res == RES_CMGR)
	{
		at_queue_handle_result (pvt, RES_CMGR);

		if (at_parse_cmgr (str, len, &from_number, &text))
		{
			ast_log (LOG_ERROR, "[%s] Error parsing SMS message, disconnecting\n", pvt->id);
			return -1;
		}

		ast_debug (1, "[%s] Successfully read SMS message\n", pvt->id);

		pvt->incoming_sms = 0;

		if (pvt->use_ucs2_encoding)
		{
			res = hexstr_ucs2_to_utf8 (text, strlen (text), sms_utf8_str, sizeof (sms_utf8_str));
			if (res > 0)
			{
				text = sms_utf8_str;
			}
			else
			{
				ast_log (LOG_ERROR, "[%s] Error parsing SMS (convert UCS-2 to UTF-8): %s\n", pvt->id, text);
			}

			res = hexstr_ucs2_to_utf8 (from_number, strlen (from_number), from_number_utf8_str, sizeof (from_number_utf8_str));
			if (res > 0)
			{
				from_number = from_number_utf8_str;
			}
			else
			{
				ast_log (LOG_ERROR, "[%s] Error parsing SMS from_number (convert UCS-2 to UTF-8): %s\n", pvt->id, from_number);
			}
		}

		ast_base64encode (text_base64, (unsigned char*)text, strlen(text), sizeof(text_base64));
		ast_verb (1, "[%s] Got SMS from %s: '%s'\n", pvt->id, from_number, text);

#ifdef __MANAGER__
		manager_event_new_sms (pvt, from_number, text);
		manager_event_new_sms_base64(pvt, from_number, text_base64);
#endif

#ifdef __MANAGER__
		struct ast_channel* channel;

		snprintf (channel_name, sizeof (channel_name), "sms@%s", pvt->context);

		channel = channel_local_request (pvt, channel_name, pvt->id, from_number, "en");
		if (channel)
		{
			pbx_builtin_setvar_helper (channel, "SMS", text);
			pbx_builtin_setvar_helper (channel, "SMS_BASE64", text_base64);

			if (ast_pbx_start (channel))
			{
				ast_hangup (channel);
				ast_log (LOG_ERROR, "[%s] Unable to start pbx on incoming sms\n", pvt->id);
			}
		}
#endif
	}
	else if (ecmd)
	{
		ast_log (LOG_ERROR, "[%s] Received '+CMGR' when expecting '%s' response to '%s', ignoring\n", pvt->id,
				at_res2str (ecmd->res), at_cmd2str (ecmd->cmd));
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Received unexpected '+CMGR'\n", pvt->id);
	}

	return 0;
}

/*!
 * \brief Send an SMS message from the queue.
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_sms_prompt (struct pvt* pvt)
{
	const struct at_queue_cmd* ecmd;

// HERE
	if ((ecmd = at_queue_head_cmd (pvt)) && ecmd->res == RES_SMS_PROMPT)
	{
		at_queue_handle_result (pvt, RES_SMS_PROMPT);
	}
	else if (ecmd)
	{
		ast_log (LOG_ERROR, "[%s] Received sms prompt when expecting '%s' response to '%s', ignoring\n", pvt->id,
				at_res2str (ecmd->res), at_cmd2str (ecmd->cmd));
	}
	else
	{
		ast_log (LOG_ERROR, "[%s] Received unexpected sms prompt\n", pvt->id);
	}

	return 0;
}

/*!
 * \brief Handle CUSD response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cusd (struct pvt* pvt, char* str, size_t len)
{
	ssize_t		res;
	char*		cusd;
	unsigned char	dcs;
	char		cusd_utf8_str[1024];
	char		text_base64[16384];
	char		channel_name[1024];

	if (at_parse_cusd (str, len, &cusd, &dcs))
	{
		ast_verb (1, "[%s] Error parsing CUSD: '%.*s'\n", pvt->id, (int) len, str);
		return 0;
	}

	if ((dcs == 0 || dcs == 15) && !pvt->cusd_use_ucs2_decoding)
	{
		res = hexstr_7bit_to_char (cusd, strlen (cusd), cusd_utf8_str, sizeof (cusd_utf8_str));
		if (res > 0)
		{
			cusd = cusd_utf8_str;
		}
		else
		{
			ast_log (LOG_ERROR, "[%s] Error parsing CUSD (convert 7bit to ASCII): %s\n", pvt->id, cusd);
			return -1;
		}
	}
	else
	{
		res = hexstr_ucs2_to_utf8 (cusd, strlen (cusd), cusd_utf8_str, sizeof (cusd_utf8_str));
		if (res > 0)
		{
			cusd = cusd_utf8_str;
		}
		else
		{
			ast_log (LOG_ERROR, "[%s] Error parsing CUSD (convert UCS-2 to UTF-8): %s\n", pvt->id, cusd);
			return -1;
		}
	}

	ast_verb (1, "[%s] Got USSD response: '%s'\n", pvt->id, cusd);
	ast_base64encode (text_base64, (unsigned char*)cusd, strlen(cusd), sizeof(text_base64));

#ifdef __MANAGER__
	manager_event_new_ussd (pvt, cusd);
	manager_event_new_ussd_base64 (pvt, text_base64);
#endif

#ifdef __MANAGER__
	struct ast_channel* channel;

	snprintf (channel_name, sizeof (channel_name), "ussd@%s", pvt->context);

	channel = channel_local_request (pvt, channel_name, pvt->id, "ussd", "en");
	if (channel)
	{
		pbx_builtin_setvar_helper (channel, "USSD", cusd);
		pbx_builtin_setvar_helper (channel, "USSD_BASE64", text_base64);

		if (ast_pbx_start (channel))
		{
			ast_hangup (channel);
			ast_log (LOG_ERROR, "[%s] Unable to start pbx on incoming ussd\n", pvt->id);
		}
	}
#endif

	return 0;
}

/*!
 * \brief Handle +CPIN response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cpin (struct pvt* pvt, char* str, size_t len)
{
	return at_parse_cpin (pvt, str, len);
}

/*!
 * \brief Handle ^SMMEMFULL response This event notifies us, that the sms storage is full
 * \param pvt -- pvt structure
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_smmemfull (struct pvt* pvt)
{
	ast_log (LOG_ERROR, "[%s] SMS storage is full\n", pvt->id);
	return 0;
}

/*!
 * \brief Handle +CSQ response Here we get the signal strength and bit error rate
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */
static int at_response_csq (struct pvt* pvt, const char* str)
{
	return at_parse_csq (pvt, str, &pvt->rssi);
}

/*!
 * \brief Handle +CNUM response Here we get our own phone number
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cnum (struct pvt* pvt, char* str, size_t len)
{
	char* number = at_parse_cnum (str, len);

	if (number)
	{
		ast_copy_string (pvt->number, number, sizeof (pvt->number));
		return 0;
	}

	ast_copy_string (pvt->number, "Unknown", sizeof (pvt->number));

	return -1;
}

/*!
 * \brief Handle +COPS response Here we get the GSM provider name
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cops (struct pvt* pvt, char* str, size_t len)
{
	char* provider_name = at_parse_cops (str, len);

	if (provider_name)
	{
		ast_copy_string (pvt->provider_name, provider_name, sizeof (pvt->provider_name));
		return 0;
	}

	ast_copy_string (pvt->provider_name, "NONE", sizeof (pvt->provider_name));

	return -1;
}

/*!
 * \brief Handle +CREG response Here we get the GSM registration status
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_creg (struct pvt* pvt, char* str, size_t len)
{
	int	d;
	char*	lac;
	char*	ci;

	if (at_enque_cops (&pvt->sys_chan))
	{
		ast_log (LOG_ERROR, "[%s] Error sending query for provider name\n", pvt->id);
	}

	if (at_parse_creg (str, len, &d, &pvt->gsm_reg_status, &lac, &ci))
	{
		ast_verb (1, "[%s] Error parsing CREG: '%.*s'\n", pvt->id, (int) len, str);
		return 0;
	}

	if (d)
	{
		pvt->gsm_registered = 1;
	}
	else
	{
		pvt->gsm_registered = 0;
	}

	if (lac)
	{
		ast_copy_string (pvt->location_area_code, lac, sizeof (pvt->location_area_code));
	}

	if (ci)
	{
		ast_copy_string (pvt->cell_id, ci, sizeof (pvt->cell_id));
	}

	return 0;
}

/*!
 * \brief Handle AT+CGMI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgmi (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->manufacturer, str, sizeof (pvt->manufacturer));

	return 0;
}

/*!
 * \brief Handle AT+CGMM response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgmm (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->model, str, sizeof (pvt->model));
	
	if (!strcmp (pvt->model, "E1550") || !strcmp (pvt->model, "E1750") || !strcmp (pvt->model, "E160X"))
	{
		pvt->cusd_use_7bit_encoding = 1;
		pvt->cusd_use_ucs2_decoding = 0;
	}

	return 0;
}

/*!
 * \brief Handle AT+CGMR response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgmr (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->firmware, str, sizeof (pvt->firmware));

	return 0;
}

/*!
 * \brief Handle AT+CGSN response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cgsn (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->imei, str, sizeof (pvt->imei));

	return 0;
}

/*!
 * \brief Handle AT+CIMI response
 * \param pvt -- pvt structure
 * \param str -- string containing response (null terminated)
 * \param len -- string lenght
 * \retval  0 success
 * \retval -1 error
 */

static int at_response_cimi (struct pvt* pvt, const char* str)
{
	ast_copy_string (pvt->imsi, str, sizeof (pvt->imsi));

	return 0;
}

static void at_response_busy(struct pvt* pvt, enum ast_control_frame_type control)
{
	const struct at_queue_task * task = at_queue_head_task (pvt);
	struct cpvt* cpvt = task->cpvt;
	
	if(cpvt == &pvt->sys_chan)
		cpvt = pvt->last_dialed_cpvt;

	if(cpvt)
	{
//		cpvt->needchup = 1;
		CPVT_SET_FLAGS(cpvt, CALL_FLAG_NEED_HANGUP);
		channel_queue_control (cpvt, control);
	}
}

/*!
 * \brief Do response
 * \param pvt -- pvt structure
 * \param iovcnt -- number of elements array pvt->d_read_iov
 * \param at_res -- result type
 * \retval  0 success
 * \retval -1 error
 */

int at_response (struct pvt* pvt, const struct iovec iov[2], int iovcnt, at_res_t at_res)
{
//	char		parse_buf[1024];
	char*		str;
	size_t		len;
	const struct at_queue_cmd*	ecmd;

//	if (iovcnt > 0)
//	{
		len = iov[0].iov_len + iov[1].iov_len - 1;

		if (iovcnt == 2)
		{
			ast_debug (5, "[%s] iovcnt == 2\n", pvt->id);
			
			str = alloca(len + 1);
			if (!str)
			{
				ast_debug (1, "[%s] buffer overflow\n", pvt->id);
				return -1;
			}
			memcpy (str,                  iov[0].iov_base, iov[0].iov_len);
			memcpy (str + iov[0].iov_len, iov[1].iov_base, iov[1].iov_len);
		}
		else
		{
			str = iov[0].iov_base;
		}
		str[len] = '\0';

//		ast_debug (5, "[%s] [%.*s]\n", pvt->id, (int) len, str);

		switch (at_res)
		{
			case RES_BOOT:
			case RES_CSSI:
			case RES_CSSU:
			case RES_SRVST:
			case RES_CVOICE:
			case RES_CMGS:
			case RES_CPMS:
				return 0;

			case RES_OK:
				return at_response_ok (pvt, at_res);

			case RES_RSSI:
				return at_response_rssi (pvt, str);
			case RES_CONF:
				return at_response_conf(pvt, str);
			case RES_MODE:
				/* An error here is not fatal. Just keep going. */
				at_response_mode (pvt, str, len);
				return 0;

			case RES_ORIG:
				return at_response_orig (pvt, str);

			case RES_CEND:
				return at_response_cend (pvt, str);

			case RES_CONN:
				return at_response_conn (pvt, str);

			case RES_CREG:
				/* An error here is not fatal. Just keep going. */
				at_response_creg (pvt, str, len);
				return 0;

			case RES_COPS:
				/* An error here is not fatal. Just keep going. */
				at_response_cops (pvt, str, len);
				return 0;

			case RES_CSQ:
				return at_response_csq (pvt, str);

			case RES_CMS_ERROR:
			case RES_ERROR:
				return at_response_error (pvt, at_res);

			case RES_RING:
				return at_response_ring (pvt);

			case RES_SMMEMFULL:
				return at_response_smmemfull (pvt);
/*
			case RES_CLIP:
				return at_response_clip (pvt, str, len);
*/
			case RES_CMTI:
				return at_response_cmti (pvt, str);

			case RES_CMGR:
				return at_response_cmgr (pvt, str, len);

			case RES_SMS_PROMPT:
				return at_response_sms_prompt (pvt);

			case RES_CUSD:
				return at_response_cusd (pvt, str, len);

			case RES_CLCC:
				return at_response_clcc (pvt, str);

			case RES_CCWA:
				return at_response_ccwa (pvt, str);

			case RES_BUSY:
				ast_log (LOG_ERROR, "[%s] Receive BUSY\n", pvt->id);
				at_response_busy(pvt, AST_CONTROL_BUSY);
				break;

			case RES_NO_DIALTONE:
				ast_log (LOG_ERROR, "[%s] Receive NO DIALTONE\n", pvt->id);
				at_response_busy(pvt, AST_CONTROL_CONGESTION);
				break;
			case RES_NO_CARRIER:
				ast_log (LOG_ERROR, "[%s] Receive NO CARRIER\n", pvt->id);
				at_response_busy(pvt, AST_CONTROL_CONGESTION);
				break;
			case RES_CPIN:
				return at_response_cpin (pvt, str, len);

			case RES_CNUM:
				/* An error here is not fatal. Just keep going. */
				at_response_cnum (pvt, str, len);
				return 0;

			case RES_CSCA:
				at_response_csca (pvt, str);
				return 0;

			case RES_PARSE_ERROR:
				ast_log (LOG_ERROR, "[%s] Error parsing result\n", pvt->id);
				return -1;

			case RES_UNKNOWN:
				ecmd = at_queue_head_cmd(pvt);
				if (ecmd)
				{
					switch (ecmd->cmd)
					{
						case CMD_AT_CGMI:
							ast_debug (1, "[%s] Got AT_CGMI data (manufacturer info)\n", pvt->id);
							return at_response_cgmi (pvt, str);

						case CMD_AT_CGMM:
							ast_debug (1, "[%s] Got AT_CGMM data (model info)\n", pvt->id);
							return at_response_cgmm (pvt, str);

						case CMD_AT_CGMR:
							ast_debug (1, "[%s] Got AT+CGMR data (firmware info)\n", pvt->id);
							return at_response_cgmr (pvt, str);

						case CMD_AT_CGSN:
							ast_debug (1, "[%s] Got AT+CGSN data (IMEI number)\n", pvt->id);
							return at_response_cgsn (pvt, str);

						case CMD_AT_CIMI:
							ast_debug (1, "[%s] Got AT+CIMI data (IMSI number)\n", pvt->id);
							return at_response_cimi (pvt, str);
						default:
							break;
					}
				}
				ast_debug (1, "[%s] Ignoring unknown result: '%.*s'\n", pvt->id, (int) len, str);

				break;
		}
//	}

	return 0;
}
