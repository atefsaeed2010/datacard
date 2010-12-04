/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <asterisk.h>
#include <asterisk/utils.h>

#include "cpvt.h"
#include "chan_datacard.h"			/* struct pvt */
#include "at_queue.h"				/* struct at_queue_task */
#include "helpers.h"				/* ITEMS_OF() */

#/* */
EXPORT_DEF struct cpvt * cpvt_alloc(struct pvt * pvt, int call_idx, unsigned dir, call_state_t state)
{
	struct cpvt* cpvt = ast_malloc (sizeof (*cpvt));
	if(cpvt)
	{
		cpvt->entry.next = NULL;
		cpvt->channel = NULL;
		cpvt->pvt = pvt;
		cpvt->call_idx = call_idx;
		cpvt->dir = dir;
		cpvt->state = state;
		cpvt->flags = 0;

		AST_LIST_INSERT_TAIL(&pvt->chans, cpvt, entry);
		if(PVT_NO_CHANS(pvt))
			pvt_on_create_1st_channel(pvt);
		pvt->chansno++;
		pvt->chan_count[cpvt->state]++;
		ast_debug (3, "[%s] create cpvt for call_idx %d dir %d state '%s'\n",  PVT_ID(pvt), call_idx, dir, call_state2str(state));
	}

	return cpvt;
}

#/* */
EXPORT_DEF struct cpvt * pvt_find_cpvt(struct pvt * pvt, int call_idx)
{
	struct cpvt * cpvt;
	AST_LIST_TRAVERSE(&pvt->chans, cpvt, entry) {
		if(call_idx == cpvt->call_idx)
			return cpvt;
	}
	
	return 0;
}

#/* */
EXPORT_DEF void cpvt_free(struct cpvt* cpvt)
{
	pvt_t * pvt = cpvt->pvt;
	struct cpvt * found;

	ast_debug (3, "[%s] destroy cpvt for call_idx %d dir %d state '%s' flags %d has%s channel\n",  PVT_ID(pvt), cpvt->call_idx, cpvt->dir, call_state2str(cpvt->state), cpvt->flags, cpvt->channel ? "" : "'t");
	AST_LIST_TRAVERSE_SAFE_BEGIN(&pvt->chans, found, entry) {
		if(found == cpvt)
		{
			AST_LIST_REMOVE_CURRENT(entry);
			pvt->chan_count[cpvt->state]--;
			pvt->chansno--;
			break;
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;

	if(PVT_NO_CHANS(pvt))
		pvt_on_remove_last_channel(pvt);

	/* also remove task from queue */
/* BUG: bad idea remove command when response to which now expected
	AST_LIST_TRAVERSE_SAFE_BEGIN(&pvt->at_queue, task, entry) {
		if(task->cpvt == cpvt)
		{
			AST_LIST_REMOVE_CURRENT(entry);
			at_queue_free(entry);
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;
*/
	/* drop last_dialed_cpvt if need */
	if(pvt->last_dialed_cpvt == cpvt)
		pvt->last_dialed_cpvt = 0;

	ast_free(cpvt);
}

#/* */
EXPORT_DEF const char * call_state2str(call_state_t state)
{
	static const char * const states[] = {
	/* on device states */
		"active",
		"held",
		"dialing",
		"alerting",
		"incoming",
		"waiting",

	/* pseudo states */
		"released",
		"initialize"
	};
	
	int idx = state;
	
	if(idx >= 0 && idx < (int)ITEMS_OF(states))
		return states[idx];
	return "Unknown";
}
