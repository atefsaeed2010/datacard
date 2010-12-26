/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <fcntl.h>

#include <asterisk.h>
#include <asterisk/utils.h>

#include "cpvt.h"
#include "chan_datacard.h"			/* struct pvt */
#include "at_queue.h"				/* struct at_queue_task */
#include "helpers.h"				/* ITEMS_OF() */

#/* return 0 on success */
// TODO: move to activation time, save resouces
static int init_pipe(int filedes[2])
{
	int x;
	int rv = pipe(filedes);
	if(rv == 0) {
		for(x = 0; x < 2; ++x) {
			rv = fcntl(filedes[x], F_GETFL);
			if(rv == -1 || (rv = fcntl(filedes[x], F_SETFL, O_NONBLOCK | rv)) == -1)
				goto bad;
			}
		return 0;
bad:
		close(filedes[0]);
		close(filedes[1]);
	}
	return rv;
}

#/* */
EXPORT_DEF struct cpvt * cpvt_alloc(struct pvt * pvt, int call_idx, unsigned dir, call_state_t state)
{
	int filedes[2];
	struct cpvt* cpvt = NULL;
	
	if(init_pipe(filedes) == 0)
	{
		cpvt = ast_malloc (sizeof (*cpvt));
		if(cpvt)
		{
			cpvt->entry.next = NULL;
			cpvt->channel = NULL;
			cpvt->pvt = pvt;
			cpvt->call_idx = call_idx;
			cpvt->state = state;
			cpvt->flags = 0;
			cpvt->dir = dir;
			cpvt->rd_pipe[0] = filedes[0];
			cpvt->rd_pipe[1] = filedes[1];

//			rb_init (&cpvt->a_write_rb, cpvt->a_write_buf, sizeof (cpvt->a_write_buf));

			AST_LIST_INSERT_TAIL(&pvt->chans, cpvt, entry);
			if(PVT_NO_CHANS(pvt))
				pvt_on_create_1st_channel(pvt);
			pvt->chansno++;
			pvt->chan_count[cpvt->state]++;
			ast_debug (3, "[%s] create cpvt for call_idx %d dir %d state '%s'\n",  PVT_ID(pvt), call_idx, dir, call_state2str(state));
			return cpvt;
		}
		close(filedes[0]);
		close(filedes[1]);
	}
	
	return cpvt;
}

#/* */
EXPORT_DEF void cpvt_free(struct cpvt* cpvt)
{
	pvt_t * pvt = cpvt->pvt;
	struct cpvt * found;
	struct at_queue_task * task;

	close(cpvt->rd_pipe[1]);
	close(cpvt->rd_pipe[0]);

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

	/* relink task to sys_chan */
	AST_LIST_TRAVERSE(&pvt->at_queue, task, entry) {
		if(task->cpvt == cpvt)
		{
			task->cpvt = &pvt->sys_chan;
		}
	}
	/* drop last_dialed_cpvt if need */
	if(pvt->last_dialed_cpvt == cpvt)
		pvt->last_dialed_cpvt = NULL;

	if(PVT_NO_CHANS(pvt))
		pvt_on_remove_last_channel(pvt);

	ast_free(cpvt);
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