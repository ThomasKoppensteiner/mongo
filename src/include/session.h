/*-
 * Copyright (c) 2008-2014 WiredTiger, Inc.
 *	All rights reserved.
 *
 * See the file LICENSE for redistribution information.
 */

/*
 * WT_DATA_HANDLE_CACHE --
 *	Per-session cache of handles to avoid synchronization when opening
 *	cursors.
 */
struct __wt_data_handle_cache {
	WT_DATA_HANDLE *dhandle;

	SLIST_ENTRY(__wt_data_handle_cache) l;
};

/*
 * WT_HAZARD --
 *	A hazard pointer.
 */
struct __wt_hazard {
	WT_PAGE *page;			/* Page address */
#ifdef HAVE_DIAGNOSTIC
	const char *file;		/* File/line where hazard acquired */
	int	    line;
#endif
};

typedef	enum {
	WT_SERIAL_NONE=0,		/* No request */
	WT_SERIAL_FUNC=1,		/* Function, then return */
	WT_SERIAL_EVICT=2,		/* Function, then schedule evict */
} wq_state_t;

/* Get the connection implementation for a session */
#define	S2C(session) ((WT_CONNECTION_IMPL *)(session)->iface.connection)

/* Get the btree for a session */
#define	S2BT(session) ((WT_BTREE *)(session)->dhandle->handle)
#define	S2BT_SAFE(session) ((session)->dhandle == NULL ?  NULL : S2BT(session))

/*
 * WT_SESSION_IMPL --
 *	Implementation of WT_SESSION.
 */
struct __wt_session_impl {
	WT_SESSION iface;

	u_int active;			/* Non-zero if the session is in-use */

	const char *name;		/* Name */
	uint32_t id;			/* UID, offset in session array */

	WT_CONDVAR *cond;		/* Condition variable */

	WT_EVENT_HANDLER *event_handler;/* Application's event handlers */

	WT_DATA_HANDLE *dhandle;	/* Current data handle */

					/* Session handle reference list */
	SLIST_HEAD(__dhandles, __wt_data_handle_cache) dhandles;
#define	WT_DHANDLE_SWEEP_WAIT	60	/* Wait before discarding */
#define	WT_DHANDLE_SWEEP_PERIOD	20	/* Only sweep every 20 seconds */
	time_t last_sweep;		/* Last sweep for dead handles */

	WT_CURSOR *cursor;		/* Current cursor */
					/* Cursors closed with the session */
	TAILQ_HEAD(__cursors, __wt_cursor) cursors;

	WT_CURSOR_BACKUP *bkp_cursor;	/* Hot backup cursor */
	WT_COMPACT	 *compact;	/* Compact state */

	WT_BTREE *metafile;		/* Metadata file */
	void	*meta_track;		/* Metadata operation tracking */
	void	*meta_track_next;	/* Current position */
	void	*meta_track_sub;	/* Child transaction / save point */
	size_t	 meta_track_alloc;	/* Currently allocated */
	int	 meta_track_nest;	/* Nesting level of meta transaction */
#define	WT_META_TRACKING(session)	(session->meta_track_next != NULL)

	TAILQ_HEAD(__tables, __wt_table) tables;

	WT_ITEM	logrec_buf;		/* Buffer for log records */

	WT_ITEM	**scratch;		/* Temporary memory for any function */
	u_int	scratch_alloc;		/* Currently allocated */
#ifdef HAVE_DIAGNOSTIC
	/*
	 * It's hard to figure out from where a buffer was allocated after it's
	 * leaked, so in diagnostic mode we track them; DIAGNOSTIC can't simply
	 * add additional fields to WT_ITEM structures because they are visible
	 * to applications, create a parallel structure instead.
	 */
	struct __wt_scratch_track {
		const char *file;	/* Allocating file, line */
		int line;
	} *scratch_track;
#endif

	WT_TXN_ISOLATION isolation;
	WT_TXN	txn;			/* Transaction state */
	u_int	ncursors;		/* Count of active file cursors. */
	void	*lang_private;		/* Language specific private storage */

	WT_REF **excl;			/* Eviction exclusive list */
	u_int	 excl_next;		/* Next empty slot */
	size_t	 excl_allocated;	/* Bytes allocated */

	void	*block_manager;		/* Block-manager support */
	int	(*block_manager_cleanup)(WT_SESSION_IMPL *);
	void	*reconcile;		/* Reconciliation support */
	int	(*reconcile_cleanup)(WT_SESSION_IMPL *);

	int compaction;			/* Compaction did some work */
	int skip_schema_lock;		/* Another thread holds the schema lock
					 * on our behalf */

	/*
	 * The free-on-transactional generation" memory and hazard information
	 * persist past session close, because they are accessed by threads of
	 * control other than the thread owning the session.  They live at the
	 * end of the structure so it's somewhat easier to clear everything but
	 * the fields that persist.
	 */
#define	WT_SESSION_CLEAR_SIZE(s)					\
	(WT_PTRDIFF(&(s)->flags, s) + sizeof((s)->flags))
	uint32_t flags;

	/*
	 * Sessions can "free" memory that may still be in use, and we use a
	 * transactional generation to track it, that is, the session stores
	 * a reference to the memory and a current transaction ID; when the
	 * oldest transaction ID has moved beyond that point, the memory can
	 * be discarded for real.
	 */
	struct __wt_fotxn {
		uint64_t    txnid;	/* Transaction ID */
		void *p;		/* Memory, length */
		size_t	    len;
	} *fotxn;			/* Free-on-transaction array */
	size_t  fotxn_cnt;		/* Array entries */
	size_t  fotxn_size;		/* Array size */

	/*
	 * Hazard pointers.
	 * The number of hazard pointers that can be in use grows dynamically.
	 */
#define	WT_HAZARD_INCR		10
	uint32_t   hazard_size;		/* Allocated slots in hazard array. */
	uint32_t   nhazard;		/* Count of active hazard pointers */
	WT_HAZARD *hazard;		/* Hazard pointer array */
} WT_GCC_ATTRIBUTE((aligned(WT_CACHE_LINE_ALIGNMENT)));
