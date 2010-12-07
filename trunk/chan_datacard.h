/* 
   Copyright (C) 2009 - 2010
   
   Artem Makhutov <artem@makhutov.org>
   http://www.makhutov.org
   
   Dmitry Vagin <dmitry2004@yandex.ru>
*/
#ifndef CHAN_DATACARD_H_INCLUDED
#define CHAN_DATACARD_H_INCLUDED

#include <asterisk.h>
#include <asterisk/frame.h>		/* AST_FRIENDLY_OFFSET */
#include <asterisk/lock.h>
#include <asterisk/linkedlists.h>

#include "ringbuffer.h"			/* struct ringbuffer */
#include "cpvt.h"			/* struct cpvt */
#include "export.h"			/* EXPORT_DECL EXPORT_DEF */
#include "dc_config.h"			/* pvt_config_t */

#define FRAME_SIZE		320
#define MODULE_DESCRIPTION	"Datacard Channel Driver"

struct at_queue_task;

typedef struct pvt_stat
{
	/* statictics / debugging */
	unsigned int		at_tasks;			/* number of active tasks in at_queue */
	unsigned int		at_cmds;			/* number of active commands in at_queue */

	uint64_t		read_bytes;			/*!< number of bytes of audio actually readed from device */
	uint64_t		write_bytes;			/*!< number of bytes of audio actually written to device */

	unsigned int		read_frames;			/*!< number of frames readed from device */
	unsigned int		read_sframes;			/*!< number of truncated frames readed from device */

	unsigned int		write_frames;			/*!< number of tries to frame write */
	unsigned int		write_tframes;			/*!< number of truncated frames to write */
	unsigned int		write_sframes;			/*!< number of silence frames to write */
	
	uint64_t		write_rb_overflow_bytes;	/*!< number of overflow bytes */
	unsigned int		write_rb_overflow;		/*!< number of times when a_write_rb overflowed */
} pvt_stat_t;
#define PVT_STAT_PUMP(name, expr)		pvt->stat.name expr

typedef struct pvt
{
	AST_LIST_ENTRY (pvt)	entry;				/*!< linked list pointers */

	ast_mutex_t		lock;				/*!< pvt lock */
	AST_LIST_HEAD_NOLOCK (, at_queue_task) at_queue;	/*!< queue for commands to modem */

	AST_LIST_HEAD_NOLOCK (, cpvt)		chans;		/*!< list of channels */
	unsigned short		chan_count[CALL_STATES_NUMBER];	/*!< channel number grouped by state */
	unsigned		chansno;			/*!< number of channels in channels list */
	struct cpvt		sys_chan;			/*!< system channel */
	struct cpvt		*last_dialed_cpvt;		/*!< channel what last call successfully set ATDnum; leave until ^ORIG received; need because real call idx of dialing call unknown until ^ORIG */
	
	pthread_t		monitor_thread;			/*!< monitor (at commands reader) thread handle */

	int			audio_fd;			/*!< audio descriptor */
	int			data_fd;			/*!< data descriptor */

	struct ast_dsp*		dsp;				/*!< silence/DTMF detector */
	struct ast_timer*	a_timer;			/*!< audio write timer */

	char			a_write_buf[FRAME_SIZE * 5];	/*!< audio write buffer */
	struct ringbuffer	a_write_rb;			/*!< audio ring buffer */

	char			a_read_buf[FRAME_SIZE + AST_FRIENDLY_OFFSET];	/*!< audio read buffer */
	struct ast_frame	a_read_frame;			/*!< readed frame buffer TODO: move as local to channel_read? */

	char			dtmf_digit;			/*!< last DTMF digit */
	struct timeval		dtmf_begin_time;		/*!< time of begin of last DTMF digit */
	struct timeval		dtmf_end_time;			/*!< time of end of last DTMF digit */

	int			timeout;			/*!< used to set the timeout for data */
#define DATA_READ_TIMEOUT	10000				/* 10 seconds */

	unsigned long		channel_instanse;		/*!< number of channels created on this device */
	unsigned int		rings;				/*!< ring/ccwa  number distributed to at_response_clcc() */

	/* device caps */
	unsigned int		use_ucs2_encoding:1;
	unsigned int		cusd_use_7bit_encoding:1;
	unsigned int		cusd_use_ucs2_decoding:1;

	int			gsm_reg_status;
	int			rssi;
	int			linkmode;
	int			linksubmode;
	char			provider_name[32];
	char			manufacturer[32];
	char			model[32];
	char			firmware[32];
	char			imei[17];
	char			imsi[17];
	char			subscriber_number[128];
	char			location_area_code[8];
	char			cell_id[8];
	char			sms_scenter[20];
	
	/* device state */
	unsigned int		connected:1;			/*!< do we have an connection to a device */
	unsigned int		initialized:1;			/*!< whether a service level connection exists or not */
	unsigned int		gsm_registered:1;		/*!< do we have an registration to a GSM */
	unsigned int		dialing;			/*!< HW state; true from ATD response OK until CEND or CONN for this call idx */
	unsigned int		ring:1;				/*!< HW state; true if has incoming call from first RING until CEND or CONN */
	unsigned int		cwaiting:1;			/*!< HW state; true if has incoming call waiting from first CCWA until CEND or CONN for */
	unsigned int		outgoing_sms:1;			/*!< outgoing sms */
	unsigned int		incoming_sms:1;			/*!< incoming sms */
	unsigned int		volume_sync_step:2;		/*!< volume synchronized stage */
#define VOLUME_SYNC_BEGIN	0
#define VOLUME_SYNC_DONE	3

	unsigned int		use_pdu:1;			/*!< PDU SMS mode in force */
	unsigned int		has_sms:1;			/*!< device has SMS support */
	unsigned int		has_voice:1;			/*!< device has voice call support */
	unsigned int		has_call_waiting:1;		/*!< call waiting enabled on device */

	unsigned int		group_last_used:1;		/*!< mark the last used device */
	unsigned int		prov_last_used:1;		/*!< mark the last used device */
	unsigned int		sim_last_used:1;		/*!< mark the last used device */
	unsigned int		d_read_result:1;		/*!< for response parsing, may move to reader thread stack */
	unsigned int		restarting:1;			/*!< if non-zero request to restart device */
	unsigned int		has_subscriber_number:1;	/*!< subscriber_number field is valid */

	pvt_config_t		settings;			/*!< all device settings from config file */
	pvt_stat_t		stat;				/*!< various statistics */
} pvt_t;
#define CONF_GLOBAL(name)	(gpublic->global_settings.name)
#define CONF_SHARED(pvt, name)	((pvt)->settings.shared.name)
#define CONF_UNIQ(pvt, name)	((pvt)->settings.unique.name)
#define PVT_ID(pvt)		((pvt)->settings.unique.id)

typedef struct public_state
{
	AST_RWLIST_HEAD(devices, pvt)	devices;
	ast_mutex_t			round_robin_mtx;
	struct pvt*			round_robin[256];
	struct dc_gconfig		global_settings;
} public_state_t;

EXPORT_DECL public_state_t * gpublic;

EXPORT_DECL int pvt_get_pseudo_call_idx(const struct pvt * pvt);
EXPORT_DECL int ready4voice_call(const struct pvt* pvt, const struct cpvt * ignore_cpvt, int opts);
EXPORT_DECL int is_dial_possible(const struct pvt * pvt, int opts, const struct cpvt * ignore_cpvt);
EXPORT_DECL const char* pvt_str_state(const struct pvt* pvt);
EXPORT_DECL struct ast_str* pvt_str_state_ex(const struct pvt* pvt);
EXPORT_DECL const char * GSM_regstate2str(int gsm_reg_status);
EXPORT_DECL const char * sys_mode2str(int sys_mode);
EXPORT_DECL const char * sys_submode2str(int sys_submode);
EXPORT_DECL char* rssi2dBm(int rssi, char* buf, unsigned len);

EXPORT_DECL void pvt_on_create_1st_channel(struct pvt* pvt);
EXPORT_DECL void pvt_on_remove_last_channel(struct pvt* pvt);

EXPORT_DECL struct ast_module* self_module();

#define PVT_NO_CHANS(pvt)		((pvt)->chansno == 0)

#endif /* CHAN_DATACARD_H_INCLUDED */
