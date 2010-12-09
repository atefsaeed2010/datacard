/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_CPVT_H_INCLUDED
#define CHAN_DATACARD_CPVT_H_INCLUDED

#include <asterisk.h>
#include <asterisk/linkedlists.h>

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */

typedef enum {
	CALL_STATE_MIN		= 0,

/* values from CLCC */
	CALL_STATE_ACTIVE	= CALL_STATE_MIN,
	CALL_STATE_ONHOLD,
	CALL_STATE_DIALING,
	CALL_STATE_ALERTING,
	CALL_STATE_INCOMING,
	CALL_STATE_WAITING,

	CALL_STATE_RELEASED,
	CALL_STATE_INIT,
	CALL_STATE_MAX		= CALL_STATE_INIT
} call_state_t;
#define CALL_STATES_NUMBER	(CALL_STATE_MAX - CALL_STATE_MIN + 1)

typedef enum {
	CALL_FLAG_NONE		= 0,
	CALL_FLAG_HOLD_OTHER	= 1,				/* external from dial() hold other calls and dial here */
	CALL_FLAG_NEED_HANGUP	= 2,				/* internal */
	CALL_FLAG_ACTIVATED	= 4,				/* fds attached to channel */
	CALL_FLAG_ALIVE		= 8,				/* listed in CLCC */
	CALL_FLAG_CONFERENCE	= 16,				/* extenal from dial() begin conference after activate this call */
} call_flag_t;


/* */
typedef struct cpvt {
	AST_LIST_ENTRY (cpvt)	entry;				/*!< linked list pointers */
	
	struct ast_channel*	channel;			/*!< Channel pointer */
	struct pvt		*pvt;				/*!< pointer to device structure */

	short			call_idx;			/*!< device call ID */
#define MIN_CALL_IDX		0
#define MAX_CALL_IDX		31

	call_state_t		state;				/*!< see also call_state_t */
	int			flags;				/*!< see also call_flag_t */

/* TODO: join with flags */
	unsigned int		dir:1;				/*!< call direction */
#define CALL_DIR_OUTGOING	0
#define CALL_DIR_INCOMING	1
} cpvt_t;

#define CPVT_SET_FLAGS(cpvt, flag)	do { (cpvt)->flags |= (flag); } while(0)
#define CPVT_RESET_FLAG(cpvt, flag)	do { (cpvt)->flags &= ~((int)flag); } while(0)
#define CPVT_TEST_FLAG(cpvt, flag)	((cpvt)->flags & (flag))
#define CPVT_TEST_FLAGS(cpvt, flag)	(((cpvt)->flags & (flag)) == (flag))

#define CPVT_IS_ACTIVE(cpvt)		((cpvt)->state == CALL_STATE_ACTIVE)
#define CPVT_IS_SOUND_SOURCE(cpvt)	((cpvt)->state == CALL_STATE_ACTIVE || (cpvt)->state == CALL_STATE_DIALING || (cpvt)->state == CALL_STATE_ALERTING)


EXPORT_DECL struct cpvt * cpvt_alloc(struct pvt * pvt, int call_idx, unsigned dir, call_state_t statem);
EXPORT_DECL void cpvt_free(struct cpvt* cpvt);

EXPORT_DECL struct cpvt * pvt_find_cpvt(struct pvt * pvt, int call_idx);
EXPORT_DECL const char * call_state2str(call_state_t state);

#endif /* CHAN_DATACARD_CPVT_H_INCLUDED */
