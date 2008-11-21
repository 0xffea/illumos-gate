/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2000 by Cisco Systems, Inc.  All rights reserved.
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _ISCSI_H
#define	_ISCSI_H

/*
 * Block comment which describes the contents of this file.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/scsi/scsi.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/socket.h>
#include <sys/kstat.h>
#include <sys/sunddi.h>
#include <sys/sunmdi.h>
#include <sys/mdi_impldefs.h>
#include <sys/time.h>
#include <sys/nvpair.h>
#include <sys/sdt.h>

#include <sys/scsi/adapters/iscsi_if.h>
#include <sys/scsi/adapters/iscsi_protocol.h>
#include <iscsiAuthClient.h>
#include <iscsi_stats.h>
#include <iscsi_thread.h>
#include <nvfile.h>

#ifndef MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef TRUE
#define	TRUE 1
#endif

#ifndef FALSE
#define	FALSE 0
#endif

#define	LOGIN_PDU_BUFFER_SIZE	(16 * 1024)	/* move somewhere else */

/*
 * Name Format of the different Task Queues
 */
#define	ISCSI_SESS_IOTH_NAME_FORMAT		"io_thrd_%d.%d"
#define	ISCSI_SESS_WD_NAME_FORMAT		"wd_thrd_%d.%d"
#define	ISCSI_SESS_LOGIN_TASKQ_NAME_FORMAT	"login_taskq_%d.%d"
#define	ISCSI_CONN_RXTH_NAME_FORMAT		"rx_thrd_%d.%d.%d"
#define	ISCSI_CONN_TXTH_NAME_FORMAT		"tx_thrd_%d.%d.%d"

/*
 * The iSCSI driver will not build scatter/gather lists (iovec) longer
 * than the value defined here. Asserts have been include in the code
 * to check.
 */
#define	ISCSI_MAX_IOVEC		5

#define	ISCSI_DEFAULT_MAX_STORM_DELAY		32

/*
 * The SNDBUF and RCVBUF size parameters for the sockets are just a
 * guess for the time being (I think it is the values used by CISCO
 * or UNH).  Testing will have to be done to figure * out the impact
 * of these values on performance.
 */
#define	ISCSI_SOCKET_SNDBUF_SIZE		(256 * 1024)
#define	ISCSI_SOCKET_RCVBUF_SIZE		(256 * 1024)
#define	ISCSI_TCP_NODELAY_DEFAULT		0
#define	ISCSI_TCP_CNOTIFY_THRESHOLD_DEFAULT	2000
#define	ISCSI_TCP_CABORT_THRESHOLD_DEFAULT	10000
#define	ISCSI_TCP_ABORT_THRESHOLD_DEFAULT	(30 * 1000) /* milliseconds */
#define	ISNS_TCP_ABORT_THRESHOLD_DEFAULT	(3 * 1000) /* milliseconds */

/*
 * timeout value in seconds that we will poll and wait for data to be return
 * from the device
 */
#define	ISCSI_RX_TIMEOUT_VALUE			60

/*
 * Convenient short hand defines
 */
#define	TARGET_PROP	"target"
#define	LUN_PROP	"lun"
#define	MDI_GUID	"wwn"
#define	NDI_GUID	"client-guid"

#define	ISCSI_SIG_CMD	0x11111111
#define	ISCSI_SIG_LUN	0x22222222
#define	ISCSI_SIG_CONN	0x33333333
#define	ISCSI_SIG_SESS	0x44444444
#define	ISCSI_SIG_HBA	0x55555555

#define	SENDTARGETS_DISCOVERY	"SENDTARGETS_DISCOVERY"

#define	ISCSI_LUN_MASK_MSB	0x00003f00
#define	ISCSI_LUN_MASK_LSB	0x000000ff
#define	ISCSI_LUN_BYTE_COPY(lun, report_lun_data) \
	lun[0] = (report_lun_data & ISCSI_LUN_MASK_MSB) >> 8; \
	lun[1] = (report_lun_data & ISCSI_LUN_MASK_LSB);
/*
 * Not defined by iSCSI, but used in the login code to
 * determine when to send the initial Login PDU
 */
#define	ISCSI_INITIAL_LOGIN_STAGE	-1

typedef enum iscsi_status {
	/* Success */
	ISCSI_STATUS_SUCCESS = 0,
	/* Driver / Kernel / Code error */
	ISCSI_STATUS_INTERNAL_ERROR,
	/* ITT table is already full, unable to reserve slot */
	ISCSI_STATUS_ITT_TABLE_FULL,
	/* Login on connection failed */
	ISCSI_STATUS_LOGIN_FAILED,
	/* No connections are in the LOGGED_IN state */
	ISCSI_STATUS_NO_CONN_LOGGED_IN,
	/* TCP Transfer Error */
	ISCSI_STATUS_TCP_TX_ERROR,
	/* TCP Receive Error */
	ISCSI_STATUS_TCP_RX_ERROR,
	/* iSCSI packet RCV timeout */
	ISCSI_STATUS_RX_TIMEOUT,
	/* iSCSI Header Digest CRC error */
	ISCSI_STATUS_HEADER_DIGEST_ERROR,
	/* iSCSI Data Digest CRC error */
	ISCSI_STATUS_DATA_DIGEST_ERROR,
	/* kmem_alloc failure */
	ISCSI_STATUS_ALLOC_FAILURE,
	/* cmd (tran_abort/reset) failed */
	ISCSI_STATUS_CMD_FAILED,
	/* iSCSI protocol error */
	ISCSI_STATUS_PROTOCOL_ERROR,
	/* iSCSI protocol version mismatch */
	ISCSI_STATUS_VERSION_MISMATCH,
	/* iSCSI login negotiation failed */
	ISCSI_STATUS_NEGO_FAIL,
	/* iSCSI login authentication failed */
	ISCSI_STATUS_AUTHENTICATION_FAILED,
	/* iSCSI login redirection failed */
	ISCSI_STATUS_REDIRECTION_FAILED,
	/* iSCSI uscsi status failure */
	ISCSI_STATUS_USCSI_FAILED,
	/* data received would have overflowed given buffer */
	ISCSI_STATUS_DATA_OVERFLOW,
	/* session/connection needs to shutdown */
	ISCSI_STATUS_SHUTDOWN,
	/* logical unit in use */
	ISCSI_STATUS_BUSY
} iscsi_status_t;
#define	ISCSI_SUCCESS(status) (status == ISCSI_STATUS_SUCCESS)

/* SNA32 check value used on increment of CmdSn values */
#define	ISCSI_SNA32_CHECK 2147483648UL /* 2**31 */

/*
 * This is the maximum number of commands that can be outstanding
 * on a iSCSI session at anyone point in time.
 */
#define	ISCSI_CMD_TABLE_SIZE		1024

/* Used on connections thread create of receiver thread */
extern pri_t minclsyspri;

/*
 * Callers of iscsid_config_one/all must hold this
 * semaphore across the calls.  Otherwise a ndi_devi_enter()
 * deadlock in the DDI layer may occur.
 */
extern ksema_t iscsid_config_semaphore;

extern kmutex_t iscsi_oid_mutex;
extern uint32_t iscsi_oid;
extern void *iscsi_state;

/*
 * NOP delay is used to send a iSCSI NOP (ie. ping) across the
 * wire to see if the target is still alive.  NOPs are only
 * sent when the RX thread hasn't received anything for the
 * below amount of time.
 */
#define	ISCSI_DEFAULT_NOP_DELAY			5 /* seconds */
extern int	iscsi_nop_delay;
/*
 * If we haven't received anything in a specified period of time
 * we will stop accepting IO via tran start.  This will enable
 * upper level drivers to see we might be having a problem and
 * in the case of scsi_vhci will start to route IO down a better
 * path.
 */
#define	ISCSI_DEFAULT_RX_WINDOW			20 /* seconds */
extern int	iscsi_rx_window;
/*
 * If we haven't received anything in a specified period of time
 * we will stop accepting IO via tran start.  This the max limit
 * when encountered we will start returning a fatal error.
 */
#define	ISCSI_DEFAULT_RX_MAX_WINDOW		180 /* seconds */
extern int	iscsi_rx_max_window;

/*
 * +--------------------------------------------------------------------+
 * | iSCSI Driver Structures						|
 * +--------------------------------------------------------------------+
 */

/*
 * iSCSI Auth Information
 */
typedef struct iscsi_auth {
	IscsiAuthStringBlock    auth_recv_string_block;
	IscsiAuthStringBlock    auth_send_string_block;
	IscsiAuthLargeBinary    auth_recv_binary_block;
	IscsiAuthLargeBinary    auth_send_binary_block;
	IscsiAuthClient		auth_client_block;
	int			num_auth_buffers;
	IscsiAuthBufferDesc	auth_buffers[5];

	/*
	 * To indicate if bi-directional authentication is enabled.
	 * 0 means uni-directional authentication.
	 * 1 means bi-directional authentication.
	 */
	int			bidirectional_auth;

	/* Initiator's authentication information. */
	char			username[iscsiAuthStringMaxLength];
	uint8_t			password[iscsiAuthStringMaxLength];
	int			password_length;

	/* Target's authentication information. */
	char			username_in[iscsiAuthStringMaxLength];
	uint8_t			password_in[iscsiAuthStringMaxLength];
	int			password_length_in;
} iscsi_auth_t;

/*
 * iSCSI Task
 */
typedef struct iscsi_task {
	void			*t_arg;
	boolean_t		t_blocking;
} iscsi_task_t;

/*
 * These are all the iscsi_cmd types that we use to track our
 * commands between queues and actions.
 */
typedef enum iscsi_cmd_type {
	ISCSI_CMD_TYPE_SCSI = 1,	/* scsi cmd */
	ISCSI_CMD_TYPE_R2T,		/* r2t */
	ISCSI_CMD_TYPE_NOP,		/* nop / ping */
	ISCSI_CMD_TYPE_ABORT,		/* abort */
	ISCSI_CMD_TYPE_RESET,		/* reset */
	ISCSI_CMD_TYPE_LOGOUT,		/* logout */
	ISCSI_CMD_TYPE_LOGIN,		/* login */
	ISCSI_CMD_TYPE_TEXT		/* text */
} iscsi_cmd_type_t;

/*
 * iscsi_cmd_state - (reference iscsi_cmd.c for state diagram)
 */
typedef enum iscsi_cmd_state {
	ISCSI_CMD_STATE_FREE,
	ISCSI_CMD_STATE_PENDING,
	ISCSI_CMD_STATE_ACTIVE,
	ISCSI_CMD_STATE_ABORTING,
	ISCSI_CMD_STATE_COMPLETED
} iscsi_cmd_state_t;

/*
 * iscsi command events
 */
typedef enum iscsi_cmd_event {
	ISCSI_CMD_EVENT_E1,
	ISCSI_CMD_EVENT_E2,
	ISCSI_CMD_EVENT_E3,
	ISCSI_CMD_EVENT_E4,
	ISCSI_CMD_EVENT_E6,
	ISCSI_CMD_EVENT_E7,
	ISCSI_CMD_EVENT_E8
} iscsi_cmd_event_t;

/*
 * iscsi text command stages - these stages are used by iSCSI text
 * processing to manage long resonses.
 */
typedef enum iscsi_cmd_text_stage {
	ISCSI_CMD_TEXT_INITIAL_REQ,
	ISCSI_CMD_TEXT_CONTINUATION,
	ISCSI_CMD_TEXT_FINAL_RSP
} iscsi_cmd_text_stage_t;

/*
 * iSCSI cmd/pkt Structure
 */
typedef struct iscsi_cmd {
	uint32_t		cmd_sig;
	struct iscsi_cmd	*cmd_prev;
	struct iscsi_cmd	*cmd_next;
	struct iscsi_conn	*cmd_conn;

	iscsi_cmd_type_t	cmd_type;
	iscsi_cmd_state_t	cmd_state;
	iscsi_cmd_state_t	cmd_prev_state;
	clock_t			cmd_lbolt_pending;
	clock_t			cmd_lbolt_active;
	clock_t			cmd_lbolt_aborting;
	clock_t			cmd_lbolt_timeout;
	boolean_t		cmd_internal;
	boolean_t		cmd_free;

	union {
		/* ISCSI_CMD_TYPE_SCSI */
		struct {
			struct scsi_pkt		*pkt;
			struct buf		*bp;
			int			cmdlen;
			int			statuslen;
			size_t			data_transferred;

			uint32_t		lun;

			/*
			 * If SCSI_CMD_TYPE is in ABORTING_STATE
			 * then the abort_icmdp field will be a pointer
			 * to the abort command chasing this one.
			 */
			struct iscsi_cmd	*abort_icmdp;
			/*
			 * pointer to the r2t associated with this
			 * command (if any)
			 */
			struct iscsi_cmd	*r2t_icmdp;
		} scsi;
		/* ISCSI_CMD_TYPE_ABORT */
		struct {
			/* pointer to original iscsi_cmd, for abort */
			struct iscsi_cmd	*icmdp;
		} abort;
		/* ISCSI_CMD_TYPE_RESET */
		struct {
			int			level;
		} reset;
		/* ISCSI_CMD_TYPE_NOP */
		struct {
			int rsvd;
		} nop;
		/* ISCSI_CMD_TYPE_R2T */
		struct {
			struct iscsi_cmd	*icmdp;
			uint32_t		offset;
			uint32_t		length;
		} r2t;
		/* ISCSI_CMD_TYPE_LOGIN */
		struct {
			int rvsd;
		} login;
		/* ISCSI_CMD_TYPE_LOGOUT */
		struct {
			int rsvd;
		} logout;
		/* ISCSI_CMD_TYPE_TEXT */
		struct {
			char			*buf;
			int			buf_len;
			uint32_t		offset;
			uint32_t		data_len;
			uint32_t		total_rx_len;
			uint32_t		ttt;
			uint8_t			lun[8];
			iscsi_cmd_text_stage_t	stage;
		} text;
	} cmd_un;

	struct iscsi_lun	*cmd_lun; /* associated lun */

	uint32_t		cmd_itt;
	uint32_t		cmd_ttt;

	/*
	 * If a data digest error is seem on a data pdu.  This flag
	 * will get set.  We don't abort the cmd immediately because
	 * we want to read in all the data to get it out of the
	 * stream.  Once the completion for the cmd is received we
	 * we will abort the cmd and state no sense data was available.
	 */
	boolean_t		cmd_crc_error_seen;

	/*
	 * Used to block and wake up caller until action is completed.
	 * This is for ABORT, RESET, and PASSTHRU cmds.
	 */
	int			cmd_result;
	int			cmd_completed;
	kmutex_t		cmd_mutex;
	kcondvar_t		cmd_completion;

} iscsi_cmd_t;


/*
 * iSCSI LUN Structure
 */
typedef struct iscsi_lun {
	uint32_t		lun_sig;
	int			lun_state;

	struct iscsi_lun	*lun_next;	/* next lun on this sess. */
	struct iscsi_sess	*lun_sess;	/* parent sess. for lun */
	dev_info_t		*lun_dip;
	mdi_pathinfo_t		*lun_pip;

	uint16_t		lun_num;	/* LUN */
	uint8_t			lun_addr_type;	/* LUN addressing type */
	uint32_t		lun_oid;	/* OID */
	char			*lun_guid;	/* GUID */
	int			lun_guid_size;	/* GUID allocation size */
	char			*lun_addr;	/* sess,lun */
	time_t			lun_time_online;

	uchar_t			lun_cap;	/* bitmap of scsi caps */

	uchar_t			lun_vid[ISCSI_INQ_VID_BUF_LEN];	/* Vendor ID */
	uchar_t			lun_pid[ISCSI_INQ_PID_BUF_LEN];	/* Product ID */
} iscsi_lun_t;

#define	ISCSI_LUN_STATE_OFFLINE	0
#define	ISCSI_LUN_STATE_ONLINE	1

#define	ISCSI_LUN_CAP_RESET   0x01

/*
 * iscsi_conn_state - (reference iscsi_conn.c for state diagram)
 */
typedef enum iscsi_conn_state {
	ISCSI_CONN_STATE_FREE,
	ISCSI_CONN_STATE_IN_LOGIN,
	ISCSI_CONN_STATE_LOGGED_IN,
	ISCSI_CONN_STATE_IN_LOGOUT,
	ISCSI_CONN_STATE_FAILED,
	ISCSI_CONN_STATE_POLLING
} iscsi_conn_state_t;

#define	ISCSI_CONN_STATE_FULL_FEATURE(state) \
	((state == ISCSI_CONN_STATE_LOGGED_IN) || \
	(state == ISCSI_CONN_STATE_IN_LOGOUT))

/*
 * iscsi connection events - (reference iscsi_conn.c for state diagram)
 */
typedef enum iscsi_conn_event {
	ISCSI_CONN_EVENT_T1,
	ISCSI_CONN_EVENT_T5,
	ISCSI_CONN_EVENT_T7,
	ISCSI_CONN_EVENT_T8,
	ISCSI_CONN_EVENT_T9,
	ISCSI_CONN_EVENT_T11,
	ISCSI_CONN_EVENT_T12,
	ISCSI_CONN_EVENT_T13,
	ISCSI_CONN_EVENT_T14,
	ISCSI_CONN_EVENT_T15,
	ISCSI_CONN_EVENT_T17,
	ISCSI_CONN_EVENT_T30
} iscsi_conn_event_t;

/*
 *
 *
 */
typedef struct iscsi_queue {
	iscsi_cmd_t	*head;
	iscsi_cmd_t	*tail;
	int		count;
	kmutex_t	mutex;
} iscsi_queue_t;

#define	ISCSI_CONN_DEFAULT_LOGIN_MIN		0
#define	ISCSI_CONN_DEFAULT_LOGIN_MAX		180
#define	ISCSI_CONN_DEFAULT_LOGIN_REDIRECT	10

typedef union iscsi_sockaddr {
	struct sockaddr		sin;
	struct sockaddr_in	sin4;
	struct sockaddr_in6	sin6;
} iscsi_sockaddr_t;

#define	SIZEOF_SOCKADDR(so)	((so)->sa_family == AF_INET ? \
	sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6))

/*
 * iSCSI Connection Structure
 */
typedef struct iscsi_conn {
	uint32_t		conn_sig;
	struct iscsi_conn	*conn_next;	/* next conn on this sess. */
	struct iscsi_sess	*conn_sess;	/* parent sess. for conn. */

	iscsi_conn_state_t	conn_state;	/* cur. conn. driver state */
	iscsi_conn_state_t	conn_prev_state; /* prev. conn. driver state */
	clock_t			conn_state_lbolt;
	/* protects the session state and synchronizes the state machine */
	kmutex_t		conn_state_mutex;
	kcondvar_t		conn_state_change;
	boolean_t		conn_state_destroy;

	struct sonode		*conn_socket;	/* aka. kernel net. socket */

	/* base connection information */
	iscsi_sockaddr_t	conn_base_addr;

	/* current connection information, may have been redirected */
	iscsi_sockaddr_t	conn_curr_addr;

	/* current connection information, may have been redirected */
	boolean_t		conn_bound;
	iscsi_sockaddr_t	conn_bound_addr;

	uint32_t		conn_cid;	/* CID */
	uint32_t		conn_oid;	/* OID */

	int			conn_current_stage;	/* iSCSI login stage */
	int			conn_next_stage;	/* iSCSI login stage */
	int			conn_partial_response;

	/*
	 * The active queue contains iscsi_cmds that have already
	 * been sent on this connection.  Any future responses to
	 * these cmds require alligence to this connection.  If there
	 * are issues with these cmds the command may need aborted
	 * depending on the command type, and must be put back into
	 * the session's pending queue or aborted.
	 */
	iscsi_queue_t		conn_queue_active;

	/* lbolt from the last receive, used for nop processing */
	clock_t			conn_rx_lbolt;
	clock_t			conn_nop_lbolt;

	iscsi_thread_t		*conn_rx_thread;
	iscsi_thread_t		*conn_tx_thread;

	/*
	 * The expstatsn is the command status sn that is expected
	 * next from the target.  Command status is carried on a number
	 * of iSCSI PDUs (ex.  SCSI Cmd Response, SCSI Data IN with
	 * S-Bit set, ...), not all PDUs.  If our expstatsn is different
	 * than the received statsn.  Something got out of sync we need to
	 * recover.
	 */
	uint32_t		conn_expstatsn;
	uint32_t		conn_laststatsn;

	/* active login parameters */
	iscsi_login_params_t	conn_params;

	/* Statistics */
	struct {
		kstat_t			*ks;
		iscsi_conn_stats_t	ks_data;
	} stats;

	/*
	 * login min and max identify the amount of time
	 * in lbolt that iscsi_start_login() should attempt
	 * to log into a target portal.  The login will
	 * delay until the min lbolt has been reached and
	 * will end once max time has been reached.  These
	 * values are normally set to the default but can
	 * are also altered by async commands received from
	 * the targetlogin.
	 */
	clock_t			conn_login_min;
	clock_t			conn_login_max;
} iscsi_conn_t;


/*
 * iscsi_conn_state - (reference iscsi_sess.c for state diagram)
 */
typedef enum iscsi_sess_state {
	ISCSI_SESS_STATE_FREE,
	ISCSI_SESS_STATE_LOGGED_IN,
	ISCSI_SESS_STATE_FAILED,
	ISCSI_SESS_STATE_IN_FLUSH,
	ISCSI_SESS_STATE_FLUSHED
} iscsi_sess_state_t;

#define	ISCSI_SESS_STATE_FULL_FEATURE(state) \
	((state == ISCSI_SESS_STATE_LOGGED_IN) || \
	(state == ISCSI_SESS_STATE_IN_FLUSH))


typedef enum iscsi_sess_event {
	ISCSI_SESS_EVENT_N1,
	ISCSI_SESS_EVENT_N3,
	ISCSI_SESS_EVENT_N5,
	ISCSI_SESS_EVENT_N6,
	ISCSI_SESS_EVENT_N7
} iscsi_sess_event_t;

typedef enum iscsi_sess_type {
	ISCSI_SESS_TYPE_NORMAL,
	ISCSI_SESS_TYPE_DISCOVERY
} iscsi_sess_type_t;

#define	SESS_ABORT_TASK_MAX_THREADS	1

/* Sun's initiator session ID */
#define	ISCSI_SUN_ISID_0    0x40    /* ISID - EN format */
#define	ISCSI_SUN_ISID_1    0x00    /* Sec B */
#define	ISCSI_SUN_ISID_2    0x00    /* Sec B */
#define	ISCSI_SUN_ISID_3    0x2A    /* Sec C - 42 = Sun's EN */
/*
 * defines 4-5 are the reserved values.  These reserved values
 * are used as the ISID for an initiator-port in MP-API and used
 * for the send targets discovery sessions.  Byte 5 is overridden
 * for full feature sessions.  The default values of byte 5 for a
 * full feature session is 0.  When MS/T is enabled with more than
 * one session this byte 5 will increment > 0 up to
 * ISCSI_MAX_CONFIG_SESSIONS.
 */
#define	ISCSI_SUN_ISID_4    0x00
#define	ISCSI_SUN_ISID_5    0xFF

#define	ISCSI_DEFAULT_SESS_BOUND	B_FALSE
#define	ISCSI_DEFAULT_SESS_NUM		1

/*
 * iSCSI Session(Target) Structure
 */
typedef struct iscsi_sess {
	uint32_t		sess_sig;

	iscsi_sess_state_t	sess_state;
	iscsi_sess_state_t	sess_prev_state;
	clock_t			sess_state_lbolt;
	/* protects the session state and synchronizes the state machine */
	kmutex_t		sess_state_mutex;

	/*
	 * Associated target OID.
	 */
	uint32_t		sess_target_oid;

	/*
	 * Session OID.  Used by IMA, interfaces and exported as
	 * TARGET_PROP which is checked by the NDI.  In addition
	 * this is used in our tran_lun_init function.
	 */
	uint32_t		sess_oid;

	struct iscsi_sess	*sess_next;
	struct iscsi_hba	*sess_hba;

	/* list of all luns relating to session */
	struct iscsi_lun	*sess_lun_list;
	krwlock_t		sess_lun_list_rwlock;

	/* list of all connections relating to session */
	struct iscsi_conn	*sess_conn_list;
	struct iscsi_conn	*sess_conn_list_last_ptr;
	/* pointer to active connection in session */
	struct iscsi_conn	*sess_conn_act;
	krwlock_t		sess_conn_list_rwlock;

	/* Connection ID for next connection to be added to session */
	uint32_t		sess_conn_next_cid;

	/*
	 * last time any connection on this session received
	 * data from the target.
	 */
	clock_t			sess_rx_lbolt;

	clock_t			sess_failure_lbolt;

	int			sess_storm_delay;

	/*
	 * sess_cmdsn_mutex protects the cmdsn and itt table/values
	 * Cmdsn isn't that big of a problem yet since we only have
	 * one connection but in the future we will need to ensure
	 * this locking is working so keep the sequence numbers in
	 * sync on the wire.
	 *
	 * We also use this lock to protect the ITT table and it's
	 * values.  We need to make sure someone doesn't assign
	 * a duplicate ITT value or cell to a command.  Also we
	 * need to make sure when someone is looking up an ITT
	 * that the command is still in that correct queue location.
	 */
	kmutex_t		sess_cmdsn_mutex;

	/*
	 * iSCSI command sequencing / windowing.  The next
	 * command to be sent via the pending queue will
	 * get the sess_cmdsn.  If the maxcmdsn is less
	 * than the next cmdsn then the iSCSI window is
	 * closed and this command cannot be sent yet.
	 * Most iscsi cmd responses from the target carry
	 * a new maxcmdsn.  If this new maxcmdsn is greater
	 * than the sess_maxcmdsn we will update it's value
	 * and set a timer to fire in one tick and reprocess
	 * the pending queue.
	 *
	 * The expcmdsn.   Is the value the target expects
	 * to be sent for my next cmdsn.  If the expcmdsn
	 * and the cmdsn get out of sync this could denote
	 * a communication problem.
	 */
	uint32_t		sess_cmdsn;
	uint32_t		sess_expcmdsn;
	uint32_t		sess_maxcmdsn;

	/* Next Initiator Task Tag (ITT) to use */
	uint32_t		sess_itt;
	/*
	 * The session iscsi_cmd table is used to a fast performance
	 * lookup of an ITT to a iscsi_cmd when we receive an iSCSI
	 * PDU from the wire.  To reserve a location in the sess_cmd_table
	 * we try the sess_itt % ISCSI_CMD_TABLE_SIZE if this cmd table
	 * cell is already full.  Then increament the sess_itt and
	 * try to get the cell position again, repeat until an empty
	 * cell is found.  Once an empty cell is found place your
	 * scsi_cmd point into the cell to reserve the location.  This
	 * selection process should be done while holding the session's
	 * mutex.
	 */
	struct iscsi_cmd	*sess_cmd_table[ISCSI_CMD_TABLE_SIZE];
	int			sess_cmd_table_count;

	/*
	 * The pending queue contains all iscsi_cmds that require an
	 * open MaxCmdSn window to be put on the wire and haven't
	 * been placed on the wire.  Once placed on the wire they
	 * will be moved to a connections specific active queue.
	 */
	iscsi_queue_t		sess_queue_pending;

	iscsi_error_t		sess_last_err;

	iscsi_queue_t		sess_queue_completion;
	/* configured login parameters */
	iscsi_login_params_t	sess_params;

	/* general iSCSI protocol/session info */
	uchar_t			sess_name[ISCSI_MAX_NAME_LEN];
	int			sess_name_length;
	char			sess_alias[ISCSI_MAX_NAME_LEN];
	int			sess_alias_length;
	iSCSIDiscoveryMethod_t	sess_discovered_by;
	iscsi_sockaddr_t	sess_discovered_addr;
	uchar_t			sess_isid[ISCSI_ISID_LEN]; /* Session ID */
	uint16_t		sess_tsid; /* Target ID */
	/*
	 * If the target portal group tag(TPGT) is equal to ISCSI_DEFAULT_TPGT
	 * then the initiator will accept a successful login with any TPGT
	 * specified by the target.  If a none default TPGT is configured
	 * then we will only successfully accept a login with that matching
	 * TPGT value.
	 */
	int			sess_tpgt_conf;
	/* This field records the negotiated TPGT value, preserved for dtrace */
	int			sess_tpgt_nego;

	/*
	 * Authentication information.
	 *
	 * DCW: Again IMA seems to take a session view at this
	 * information.
	 */
	iscsi_auth_t		sess_auth;

	/* Statistics */
	struct {
		kstat_t			*ks;
		iscsi_sess_stats_t	ks_data;
		kstat_t			*ks_io;
		kstat_io_t		ks_io_data;
		kmutex_t		ks_io_lock;
	} stats;

	iscsi_thread_t		*sess_ic_thread;
	boolean_t		sess_window_open;
	iscsi_sess_type_t	sess_type;

	boolean_t		sess_enum_in_progress;

	ddi_taskq_t		*sess_taskq;

	iscsi_thread_t		*sess_wd_thread;

} iscsi_sess_t;


/*
 * iscsi_network
 */
typedef struct iscsi_network {
	void* (*socket)(int domain, int, int);
	int (*bind)(void *, struct sockaddr *, int, int, int);
	int (*connect)(void *, struct sockaddr *, int, int, int);
	int (*listen)(void *, int);
	void* (*accept)(void *, struct sockaddr *, int *);
	int (*getsockname)(void *);
	int (*getsockopt)(void *, int, int, void *, int *, int);
	int (*setsockopt)(void *, int, int, void *, int);
	int (*shutdown)(void *, int);
	void (*close)(void *);

	size_t (*poll)(void *, clock_t);
	size_t (*sendmsg)(void *, struct msghdr *);
	size_t (*recvmsg)(void *, struct msghdr *, int);

	iscsi_status_t (*sendpdu)(void *, iscsi_hdr_t *, char *, int);
	iscsi_status_t (*recvdata)(void *, iscsi_hdr_t *, char *,
	    int, int, int);
	iscsi_status_t (*recvhdr)(void *, iscsi_hdr_t *, int, int, int);

	struct {
		int			sndbuf;
		int			rcvbuf;
		int			nodelay;
		int			conn_notify_threshold;
		int			conn_abort_threshold;
		int			abort_threshold;
	} tweaks;
} iscsi_network_t;

#define	ISCSI_NET_HEADER_DIGEST	0x00000001
#define	ISCSI_NET_DATA_DIGEST	0x00000002

extern iscsi_network_t *iscsi_net;

/*
 * If we get bus_config requests in less than 5 seconds
 * apart skip the name services re-discovery and just
 * complete the requested logins.  This protects against
 * bus_config storms from stale /dev links.
 */
#define	ISCSI_CONFIG_STORM_DELAY_DEFAULT    5

/*
 * iSCSI HBA Structure
 */
typedef struct iscsi_hba {
	uint32_t		hba_sig;
	dev_info_t		*hba_dip;	/* dev info ptr */
	scsi_hba_tran_t		*hba_tran;	/* scsi tran ptr */

	struct iscsi_sess	*hba_sess_list;	/* sess. list for hba */
	krwlock_t		hba_sess_list_rwlock; /* protect sess. list */

	/* lbolt of the last time we received a config request */
	clock_t			hba_config_lbolt;
	/* current number of seconds to protect against bus config storms */
	int			hba_config_storm_delay;

	/* general iSCSI protocol hba/initiator info */
	uchar_t			hba_name[ISCSI_MAX_NAME_LEN];
	int			hba_name_length;
	uchar_t			hba_alias[ISCSI_MAX_NAME_LEN];
	int			hba_alias_length;

	/* Default SessionID for HBA */
	uchar_t			hba_isid[ISCSI_ISID_LEN];

	/* Default HBA wide settings */
	iscsi_login_params_t	hba_params;

	/*
	 * There's only one HBA and it's set to ISCSI_INITIATOR_OID
	 * (value of 1) at the beginning of time.
	 */
	uint32_t		hba_oid;

	/*
	 * Keep track of which events have been sent. User daemons request
	 * this information so they don't wait for events which they won't
	 * see.
	 */
	kmutex_t		hba_discovery_events_mutex;
	iSCSIDiscoveryMethod_t  hba_discovery_events;
	boolean_t		hba_discovery_in_progress;

	boolean_t		hba_mpxio_enabled; /* mpxio-enabled */

	/*
	 * Ensures only one SendTargets operation occurs at a time
	 */
	ksema_t			hba_sendtgts_semaphore;

	/*
	 * Statistics
	 */
	struct {
		kstat_t			*ks;
		iscsi_hba_stats_t	ks_data;
	} stats;
} iscsi_hba_t;

/*
 * +--------------------------------------------------------------------+
 * | iSCSI prototypes							|
 * +--------------------------------------------------------------------+
 */

/* iscsi_io.c */
int iscsi_sna_lte(uint32_t n1, uint32_t n2);
char *iscsi_get_next_text(char *data, int data_length, char *curr_text);

void iscsi_ic_thread(iscsi_thread_t *thread, void *arg);
void iscsi_tx_thread(iscsi_thread_t *thread, void *arg);
void iscsi_rx_thread(iscsi_thread_t *thread, void *arg);
void iscsi_wd_thread(iscsi_thread_t *thread, void *arg);

iscsi_status_t iscsi_tx_cmd(iscsi_sess_t *isp, iscsi_cmd_t *icmdp);

void iscsi_handle_abort(void *arg);
iscsi_status_t iscsi_handle_reset(iscsi_sess_t *isp, int level,
    iscsi_lun_t *ilp);
iscsi_status_t iscsi_handle_logout(iscsi_conn_t *icp);
iscsi_status_t iscsi_handle_passthru(iscsi_sess_t *isp, uint16_t lun,
    struct uscsi_cmd *ucmdp);
iscsi_status_t iscsi_handle_text(iscsi_conn_t *icp,
    char *buf, uint32_t buf_len, uint32_t data_len, uint32_t *rx_data_len);

void iscsi_iodone(iscsi_sess_t *isp, iscsi_cmd_t *icmdp);

/* iscsi_crc.c */
uint32_t iscsi_crc32c(void *address, unsigned long length);
uint32_t iscsi_crc32c_continued(void *address, unsigned long length,
    uint32_t crc);

/* iscsi_queue.c */
void iscsi_init_queue(iscsi_queue_t *queue);
void iscsi_destroy_queue(iscsi_queue_t *queue);
void iscsi_enqueue_pending_cmd(iscsi_sess_t *isp, iscsi_cmd_t *icmdp);
void iscsi_dequeue_pending_cmd(iscsi_sess_t *isp, iscsi_cmd_t *icmdp);
void iscsi_enqueue_active_cmd(iscsi_conn_t *icp, iscsi_cmd_t *icmdp);
void iscsi_dequeue_active_cmd(iscsi_conn_t *icp, iscsi_cmd_t *icmdp);
void iscsi_enqueue_completed_cmd(iscsi_sess_t *isp, iscsi_cmd_t *icmdp);
iscsi_status_t iscsi_dequeue_cmd(iscsi_cmd_t **, iscsi_cmd_t **, iscsi_cmd_t *);
void iscsi_move_queue(iscsi_queue_t *src_queue, iscsi_queue_t *dst_queue);
void iscsi_enqueue_cmd_head(iscsi_cmd_t **, iscsi_cmd_t **,
    iscsi_cmd_t *);

/* iscsi_login.c */
iscsi_status_t iscsi_login_start(void *arg);

/* iscsi_stats.c */
boolean_t iscsi_hba_kstat_init(struct iscsi_hba	*ihp);
boolean_t iscsi_hba_kstat_term(struct iscsi_hba	*ihp);
boolean_t iscsi_sess_kstat_init(struct iscsi_sess *isp);
boolean_t iscsi_sess_kstat_term(struct iscsi_sess *isp);
boolean_t iscsi_conn_kstat_init(struct iscsi_conn	*icp);
void iscsi_conn_kstat_term(struct iscsi_conn *icp);

/* iscsi_net.c */
void iscsi_net_init();
void iscsi_net_fini();

/* iscsi_sess.c */
iscsi_sess_t *iscsi_sess_create(iscsi_hba_t *ihp,
    iSCSIDiscoveryMethod_t method, struct sockaddr *addr_dsc,
    char *target_name, int tpgt, uchar_t isid_lsb,
    iscsi_sess_type_t type, uint32_t *oid);
void iscsi_sess_online(iscsi_sess_t *isp);
int iscsi_sess_get(uint32_t oid, iscsi_hba_t *ihp, iscsi_sess_t **ispp);
iscsi_status_t iscsi_sess_destroy(iscsi_sess_t *isp);
void iscsi_sess_state_machine(iscsi_sess_t *isp, iscsi_sess_event_t event);
char *iscsi_sess_state_str(iscsi_sess_state_t state);
boolean_t iscsi_sess_set_auth(iscsi_sess_t *isp);
iscsi_status_t iscsi_sess_reserve_itt(iscsi_sess_t *isp, iscsi_cmd_t *icmdp);
void iscsi_sess_release_itt(iscsi_sess_t *isp, iscsi_cmd_t *icmdp);
void iscsi_sess_redrive_io(iscsi_sess_t *isp);
int iscsi_sess_get_by_target(uint32_t target_oid, iscsi_hba_t *ihp,
	iscsi_sess_t **ispp);


/* iscsi_conn.c */
iscsi_status_t iscsi_conn_create(struct sockaddr *addr, iscsi_sess_t *isp,
    iscsi_conn_t **icpp);
iscsi_status_t iscsi_conn_offline(iscsi_conn_t *icp);
iscsi_status_t iscsi_conn_destroy(iscsi_conn_t *icp);
iscsi_status_t iscsi_conn_state_machine(iscsi_conn_t *icp,
    iscsi_conn_event_t event);
char *iscsi_conn_state_str(iscsi_conn_state_t state);
void iscsi_conn_set_login_min_max(iscsi_conn_t *icp, int min, int max);
iscsi_status_t iscsi_conn_sync_params(iscsi_conn_t *icp);

/* iscsi_lun.c */
iscsi_status_t iscsi_lun_create(iscsi_sess_t *isp, uint16_t lun_num,
    uint8_t lun_addr_type, struct scsi_inquiry *inq, char *guid);
iscsi_status_t iscsi_lun_destroy(iscsi_hba_t *ihp,
    iscsi_lun_t *ilp);
void iscsi_lun_online(iscsi_hba_t *ihp,
    iscsi_lun_t *ilp);
iscsi_status_t iscsi_lun_offline(iscsi_hba_t *ihp,
    iscsi_lun_t *ilp, boolean_t lun_free);

/* iscsi_cmd.c */
void iscsi_cmd_state_machine(iscsi_cmd_t *icmdp,
    iscsi_cmd_event_t event, void *arg);
iscsi_cmd_t	*iscsi_cmd_alloc(iscsi_conn_t *icp, int km_flags);
void		iscsi_cmd_free(iscsi_cmd_t *icmdp);

/* iscsi_ioctl.c */
int iscsi_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
void * iscsi_ioctl_copyin(caddr_t arg, int mode, size_t size);
int iscsi_ioctl_copyout(void *data, size_t size, caddr_t arg, int mode);
iscsi_conn_list_t *iscsi_ioctl_conn_oid_list_get_copyin(caddr_t, int);
int iscsi_ioctl_conn_oid_list_get_copyout(iscsi_conn_list_t *, caddr_t, int);
boolean_t iscsi_ioctl_conn_oid_list_get(iscsi_hba_t *ihp,
    iscsi_conn_list_t *cl);
boolean_t iscsi_ioctl_conn_props_get(iscsi_hba_t *ihp, iscsi_conn_props_t *cp);
int iscsi_ioctl_sendtgts_get(iscsi_hba_t *ihp, iscsi_sendtgts_list_t *stl);
int iscsi_target_prop_mod(iscsi_hba_t *, iscsi_property_t *, int cmd);
int iscsi_set_params(iscsi_param_set_t *, iscsi_hba_t *, boolean_t);
int iscsi_get_persisted_param(uchar_t *, iscsi_param_get_t *,
    iscsi_login_params_t *);
void iscsi_set_default_login_params(iscsi_login_params_t *params);
int iscsi_ioctl_get_config_sess(iscsi_hba_t *ihp,
    iscsi_config_sess_t *ics);
int iscsi_ioctl_set_config_sess(iscsi_hba_t *ihp,
    iscsi_config_sess_t *ics);
/* ioctls  prototypes */
int iscsi_get_param(iscsi_login_params_t *params,
    boolean_t valid_flag,
    iscsi_param_get_t *ipgp);

/* iscsid.c */
boolean_t iscsid_init(iscsi_hba_t *ihp, boolean_t restart);
void iscsid_fini();
void iscsid_props(iSCSIDiscoveryProperties_t *props);
boolean_t iscsid_enable_discovery(iscsi_hba_t *ihp,
    iSCSIDiscoveryMethod_t idm, boolean_t poke);
boolean_t iscsid_disable_discovery(iscsi_hba_t *ihp,
    iSCSIDiscoveryMethod_t idm);
void iscsid_poke_discovery(iscsi_hba_t *ihp, iSCSIDiscoveryMethod_t method);
void iscsid_do_sendtgts(entry_t *discovery_addr);
void iscsid_do_isns_query_one_server(
    iscsi_hba_t *ihp, entry_t *isns_addr);
void iscsid_do_isns_query(iscsi_hba_t *ihp);
void iscsid_config_one(iscsi_hba_t *ihp,
    char *name, boolean_t protect);
void iscsid_config_all(iscsi_hba_t *ihp, boolean_t protect);
void iscsid_unconfig_one(iscsi_hba_t *ihp, char *name);
void iscsid_unconfig_all(iscsi_hba_t *ihp);
void isns_scn_callback(void *arg);
boolean_t iscsid_del(iscsi_hba_t *ihp, char *target_name,
    iSCSIDiscoveryMethod_t method, struct sockaddr *addr_dsc);
boolean_t iscsid_login_tgt(iscsi_hba_t *ihp, char *target_name,
    iSCSIDiscoveryMethod_t method, struct sockaddr *addr_dsc);
void iscsid_addr_to_sockaddr(int src_insize, void *src_addr, int src_port,
    struct sockaddr *dst_addr);

extern void bcopy(const void *s1, void *s2, size_t n);
extern void bzero(void *s, size_t n);
/*
 * Here we need a contract for inet_ntop() and inet_pton()
 * in uts/common/inet/ip/inet_ntop.c
 */
extern char *inet_ntop(int af, const void *addr, char *buf, int addrlen);
extern int inet_pton(int af, char *inp, void *outp);

#ifdef __cplusplus
}
#endif

#endif /* _ISCSI_H */