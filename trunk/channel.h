/*
   Copyright (C) 2010 bg <bg_one@mail.ru>
*/
#ifndef CHAN_DATACARD_CHANNEL_H_INCLUDED
#define CHAN_DATACARD_CHANNEL_H_INCLUDED

#include <asterisk.h>
#include <asterisk/frame.h>		/* enum ast_control_frame_type */

#include "export.h"			/* EXPORT_DECL EXPORT_DEF */


typedef struct channel_var
{
	const char* 	name;
	char*		value;
} channel_var_t;

struct pvt;
struct cpvt;

EXPORT_DECL const struct ast_channel_tech channel_tech;

EXPORT_DECL struct ast_channel* channel_new (struct pvt* pvt, int ast_state, const char* cid_num, int call_idx, unsigned dir, unsigned state);
EXPORT_DECL int channel_queue_control (struct cpvt * cpvt, enum ast_control_frame_type control);
EXPORT_DECL int channel_queue_hangup (struct cpvt * cpvt, int hangupcause);
//EXPORT_DECL struct ast_channel* channel_local_request (struct pvt* pvt, const char* devicename, const char* cid_name, const char* cid_num);
EXPORT_DECL void channel_local_start (struct pvt* pvt, const char* exten, const char* number, channel_var_t * vars);
EXPORT_DECL void channel_change_state(struct cpvt * cpvt, unsigned newstate, int cause);


#endif /* CHAN_DATACARD_CHANNEL_H_INCLUDED */
