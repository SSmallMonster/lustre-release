/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see [sun.com URL with a
 * copy of GPLv2].
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright  2008 Sun Microsystems, Inc. All rights reserved
 * Use is subject to license terms.
 */
/*
 * This file is part of Lustre, http://www.lustre.org/
 * Lustre is a trademark of Sun Microsystems, Inc.
 *
 * lnet/selftest/selftest.h
 *
 * Author: Isaac Huang <isaac@clusterfs.com>
 */
#ifndef __SELFTEST_SELFTEST_H__
#define __SELFTEST_SELFTEST_H__

#define LNET_ONLY

#ifndef __KERNEL__

/* XXX workaround XXX */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/* TODO: remove these when libcfs provides proper primitives for userspace
 *
 * Dummy implementations of spinlock_t and atomic_t work since userspace
 * selftest is completely single-threaded, even using multi-threaded usocklnd.
 */
typedef struct { } spinlock_t;
static inline void spin_lock(spinlock_t *l) {return;}
static inline void spin_unlock(spinlock_t *l) {return;}
static inline void spin_lock_init(spinlock_t *l) {return;}

typedef struct { volatile int counter; } atomic_t;
#define atomic_read(a) ((a)->counter)
#define atomic_set(a,b) do {(a)->counter = b; } while (0)
#define atomic_dec_and_test(a) ((--((a)->counter)) == 0)
#define atomic_inc(a)  (((a)->counter)++)
#define atomic_dec(a)  do { (a)->counter--; } while (0)

#endif
#include <libcfs/libcfs.h>
#include <lnet/lnet.h>
#include <lnet/lib-lnet.h>
#include <lnet/lib-types.h>
#include <lnet/lnetst.h>

#include "rpc.h"
#include "timer.h"

#ifndef MADE_WITHOUT_COMPROMISE
#define MADE_WITHOUT_COMPROMISE
#endif


#define SWI_STATE_NEWBORN                  0
#define SWI_STATE_REPLY_SUBMITTED          1
#define SWI_STATE_REPLY_SENT               2
#define SWI_STATE_REQUEST_SUBMITTED        3
#define SWI_STATE_REQUEST_SENT             4
#define SWI_STATE_REPLY_RECEIVED           5
#define SWI_STATE_BULK_STARTED             6
#define SWI_STATE_BULK_ERRORED             7
#define SWI_STATE_DONE                     10

/* forward refs */
struct swi_workitem;
struct srpc_service;
struct sfw_test_unit;
struct sfw_test_instance;

/*
 * A workitems is deferred work with these semantics:
 * - a workitem always runs in thread context.
 * - a workitem can be concurrent with other workitems but is strictly
 *   serialized with respect to itself.
 * - no CPU affinity, a workitem does not necessarily run on the same CPU
 *   that schedules it. However, this might change in the future.
 * - if a workitem is scheduled again before it has a chance to run, it
 *   runs only once.
 * - if a workitem is scheduled while it runs, it runs again after it
 *   completes; this ensures that events occurring while other events are
 *   being processed receive due attention. This behavior also allows a
 *   workitem to reschedule itself.
 *
 * Usage notes:
 * - a workitem can sleep but it should be aware of how that sleep might
 *   affect others.
 * - a workitem runs inside a kernel thread so there's no user space to access.
 * - do not use a workitem if the scheduling latency can't be tolerated.
 *
 * When wi_action returns non-zero, it means the workitem has either been
 * freed or reused and workitem scheduler won't touch it any more.
 */
typedef int (*swi_action_t) (struct swi_workitem *);
typedef struct swi_workitem {
        struct list_head wi_list;        /* chain on runq */
        int              wi_state;
        swi_action_t     wi_action;
        void            *wi_data;
        unsigned int     wi_running:1;
        unsigned int     wi_scheduled:1;
} swi_workitem_t;

static inline void
swi_init_workitem (swi_workitem_t *wi, void *data, swi_action_t action)
{
        CFS_INIT_LIST_HEAD(&wi->wi_list);

        wi->wi_running   = 0;
        wi->wi_scheduled = 0;
        wi->wi_data      = data;
        wi->wi_action    = action;
        wi->wi_state     = SWI_STATE_NEWBORN;
}

#define SWI_RESCHED    128         /* # workitem scheduler loops before reschedule */

/* services below SRPC_FRAMEWORK_SERVICE_MAX_ID are framework
 * services, e.g. create/modify session.
 */
#define SRPC_SERVICE_DEBUG              0
#define SRPC_SERVICE_MAKE_SESSION       1
#define SRPC_SERVICE_REMOVE_SESSION     2
#define SRPC_SERVICE_BATCH              3
#define SRPC_SERVICE_TEST               4
#define SRPC_SERVICE_QUERY_STAT         5
#define SRPC_SERVICE_JOIN               6
#define SRPC_FRAMEWORK_SERVICE_MAX_ID   10
/* other services start from SRPC_FRAMEWORK_SERVICE_MAX_ID+1 */
#define SRPC_SERVICE_BRW                11
#define SRPC_SERVICE_PING               12
#define SRPC_SERVICE_MAX_ID             12

#define SRPC_REQUEST_PORTAL             50
/* a lazy portal for framework RPC requests */
#define SRPC_FRAMEWORK_REQUEST_PORTAL   51
/* all reply/bulk RDMAs go to this portal */
#define SRPC_RDMA_PORTAL                52

static inline srpc_msg_type_t
srpc_service2request (int service)
{
        switch (service) {
        default:
                LBUG ();
        case SRPC_SERVICE_DEBUG:
                return SRPC_MSG_DEBUG_REQST;

        case SRPC_SERVICE_MAKE_SESSION:
                return SRPC_MSG_MKSN_REQST;

        case SRPC_SERVICE_REMOVE_SESSION:
                return SRPC_MSG_RMSN_REQST;

        case SRPC_SERVICE_BATCH:
                return SRPC_MSG_BATCH_REQST;

        case SRPC_SERVICE_TEST:
                return SRPC_MSG_TEST_REQST;

        case SRPC_SERVICE_QUERY_STAT:
                return SRPC_MSG_STAT_REQST;

        case SRPC_SERVICE_BRW:
                return SRPC_MSG_BRW_REQST;

        case SRPC_SERVICE_PING:
                return SRPC_MSG_PING_REQST;

        case SRPC_SERVICE_JOIN:
                return SRPC_MSG_JOIN_REQST;
        }
}

static inline srpc_msg_type_t
srpc_service2reply (int service)
{
        return srpc_service2request(service) + 1;
}

typedef enum {
        SRPC_BULK_REQ_RCVD   = 0, /* passive bulk request(PUT sink/GET source) received */
        SRPC_BULK_PUT_SENT   = 1, /* active bulk PUT sent (source) */
        SRPC_BULK_GET_RPLD   = 2, /* active bulk GET replied (sink) */
        SRPC_REPLY_RCVD      = 3, /* incoming reply received */
        SRPC_REPLY_SENT      = 4, /* outgoing reply sent */
        SRPC_REQUEST_RCVD    = 5, /* incoming request received */
        SRPC_REQUEST_SENT    = 6, /* outgoing request sent */
} srpc_event_type_t;

/* RPC event */
typedef struct {
        srpc_event_type_t ev_type;   /* what's up */
        lnet_event_kind_t ev_lnet;   /* LNet event type */
        int               ev_fired;  /* LNet event fired? */
        int               ev_status; /* LNet event status */
        void             *ev_data;   /* owning server/client RPC */
} srpc_event_t;

typedef struct {
        int              bk_len;  /* len of bulk data */
        lnet_handle_md_t bk_mdh;
        int              bk_sink; /* sink/source */
        int              bk_niov; /* # iov in bk_iovs */
#ifdef __KERNEL__
        lnet_kiov_t      bk_iovs[0];
#else
        cfs_page_t     **bk_pages;
        lnet_md_iovec_t  bk_iovs[0];
#endif
} srpc_bulk_t; /* bulk descriptor */

typedef struct srpc_peer {
        struct list_head stp_list;     /* chain on peer hash */
        struct list_head stp_rpcq;     /* q of non-control RPCs */
        struct list_head stp_ctl_rpcq; /* q of control RPCs */
        spinlock_t       stp_lock;     /* serialize */
        lnet_nid_t       stp_nid;
        int              stp_credits;  /* available credits */
} srpc_peer_t;

/* message buffer descriptor */
typedef struct {
        struct list_head     buf_list; /* chain on srpc_service::*_msgq */
        srpc_msg_t           buf_msg;
        lnet_handle_md_t     buf_mdh;
        lnet_nid_t           buf_self;
        lnet_process_id_t    buf_peer;
} srpc_buffer_t;

/* server-side state of a RPC */
typedef struct srpc_server_rpc {
        struct list_head     srpc_list;    /* chain on srpc_service::*_rpcq */
        struct srpc_service *srpc_service;
        swi_workitem_t       srpc_wi;
        srpc_event_t         srpc_ev;      /* bulk/reply event */
        lnet_nid_t           srpc_self;
        lnet_process_id_t    srpc_peer;
        srpc_msg_t           srpc_replymsg;
        lnet_handle_md_t     srpc_replymdh;
        srpc_buffer_t       *srpc_reqstbuf;
        srpc_bulk_t         *srpc_bulk;

        int                  srpc_status;
        void               (*srpc_done)(struct srpc_server_rpc *);
} srpc_server_rpc_t;

/* client-side state of a RPC */
typedef struct srpc_client_rpc {
        struct list_head     crpc_list;   /* chain on user's lists */
        struct list_head     crpc_privl;  /* chain on srpc_peer_t::*rpcq */
        spinlock_t           crpc_lock;   /* serialize */
        int                  crpc_service;
        atomic_t             crpc_refcount;
        int                  crpc_timeout; /* # seconds to wait for reply */
        stt_timer_t          crpc_timer;
        swi_workitem_t       crpc_wi;
        lnet_process_id_t    crpc_dest;
        srpc_peer_t         *crpc_peer;

        void               (*crpc_done)(struct srpc_client_rpc *);
        void               (*crpc_fini)(struct srpc_client_rpc *);
        int                  crpc_status;    /* completion status */
        void                *crpc_priv;      /* caller data */

        /* state flags */
        unsigned int         crpc_aborted:1; /* being given up */
        unsigned int         crpc_closed:1;  /* completed */

        /* RPC events */
        srpc_event_t         crpc_bulkev;    /* bulk event */
        srpc_event_t         crpc_reqstev;   /* request event */
        srpc_event_t         crpc_replyev;   /* reply event */

        /* bulk, request(reqst), and reply exchanged on wire */
        srpc_msg_t           crpc_reqstmsg;
        srpc_msg_t           crpc_replymsg;
        lnet_handle_md_t     crpc_reqstmdh;
        lnet_handle_md_t     crpc_replymdh;
        srpc_bulk_t          crpc_bulk;
} srpc_client_rpc_t;

#define srpc_client_rpc_size(rpc)                                       \
offsetof(srpc_client_rpc_t, crpc_bulk.bk_iovs[(rpc)->crpc_bulk.bk_niov])

#define srpc_client_rpc_addref(rpc)                                     \
do {                                                                    \
        CDEBUG(D_NET, "RPC[%p] -> %s (%d)++\n",                         \
               (rpc), libcfs_id2str((rpc)->crpc_dest),                  \
               atomic_read(&(rpc)->crpc_refcount));                     \
        LASSERT(atomic_read(&(rpc)->crpc_refcount) > 0);                \
        atomic_inc(&(rpc)->crpc_refcount);                              \
} while (0)

#define srpc_client_rpc_decref(rpc)                                     \
do {                                                                    \
        CDEBUG(D_NET, "RPC[%p] -> %s (%d)--\n",                         \
               (rpc), libcfs_id2str((rpc)->crpc_dest),                  \
               atomic_read(&(rpc)->crpc_refcount));                     \
        LASSERT(atomic_read(&(rpc)->crpc_refcount) > 0);                \
        if (atomic_dec_and_test(&(rpc)->crpc_refcount))                 \
                srpc_destroy_client_rpc(rpc);                           \
} while (0)

#define srpc_event_pending(rpc)   ((rpc)->crpc_bulkev.ev_fired == 0 ||  \
                                   (rpc)->crpc_reqstev.ev_fired == 0 || \
                                   (rpc)->crpc_replyev.ev_fired == 0)

typedef struct srpc_service {
        int                sv_id;            /* service id */
        const char        *sv_name;          /* human readable name */
        int                sv_nprune;        /* # posted RPC to be pruned */
        int                sv_concur;        /* max # concurrent RPCs */

        spinlock_t         sv_lock;
        int                sv_shuttingdown;
        srpc_event_t       sv_ev;            /* LNet event */
        int                sv_nposted_msg;   /* # posted message buffers */
        struct list_head   sv_free_rpcq;     /* free RPC descriptors */
        struct list_head   sv_active_rpcq;   /* in-flight RPCs */
        struct list_head   sv_posted_msgq;   /* posted message buffers */
        struct list_head   sv_blocked_msgq;  /* blocked for RPC descriptor */

        /* Service callbacks:
         * - sv_handler: process incoming RPC request
         * - sv_bulk_ready: notify bulk data
         */
        int                (*sv_handler) (srpc_server_rpc_t *);
        int                (*sv_bulk_ready) (srpc_server_rpc_t *, int);
} srpc_service_t;

#define SFW_POST_BUFFERS         8
#define SFW_SERVICE_CONCURRENCY  (SFW_POST_BUFFERS/2)

typedef struct {
        struct list_head  sn_list;    /* chain on fw_zombie_sessions */
        lst_sid_t         sn_id;      /* unique identifier */
        unsigned int      sn_timeout; /* # seconds' inactivity to expire */
        int               sn_timer_active;
        stt_timer_t       sn_timer;
        struct list_head  sn_batches; /* list of batches */
        char              sn_name[LST_NAME_SIZE];
        atomic_t          sn_brw_errors;
        atomic_t          sn_ping_errors;
} sfw_session_t;

#define sfw_sid_equal(sid0, sid1)     ((sid0).ses_nid == (sid1).ses_nid && \
                                       (sid0).ses_stamp == (sid1).ses_stamp)

typedef struct {
        struct list_head  bat_list;      /* chain on sn_batches */
        lst_bid_t         bat_id;        /* batch id */
        int               bat_error;     /* error code of batch */
        sfw_session_t    *bat_session;   /* batch's session */
        atomic_t          bat_nactive;   /* # of active tests */
        struct list_head  bat_tests;     /* test instances */
} sfw_batch_t;

typedef struct {
        int  (*tso_init)(struct sfw_test_instance *tsi); /* intialize test client */
        void (*tso_fini)(struct sfw_test_instance *tsi); /* finalize test client */
        int  (*tso_prep_rpc)(struct sfw_test_unit *tsu,
                             lnet_process_id_t dest,
                             srpc_client_rpc_t **rpc);   /* prep a tests rpc */
        void (*tso_done_rpc)(struct sfw_test_unit *tsu,
                             srpc_client_rpc_t *rpc);    /* done a test rpc */
} sfw_test_client_ops_t;

typedef struct sfw_test_instance {
        struct list_head        tsi_list;         /* chain on batch */
        int                     tsi_service;      /* test type */
        sfw_batch_t            *tsi_batch;        /* batch */
        sfw_test_client_ops_t  *tsi_ops;          /* test client operations */

        /* public parameter for all test units */
        int                     tsi_is_client:1;     /* is test client */
        int                     tsi_stoptsu_onerr:1; /* stop tsu on error */
        int                     tsi_concur;          /* concurrency */
        int                     tsi_loop;            /* loop count */

        /* status of test instance */
        spinlock_t              tsi_lock;         /* serialize */
        int                     tsi_stopping:1;   /* test is stopping */
        atomic_t                tsi_nactive;      /* # of active test unit */
        struct list_head        tsi_units;        /* test units */
        struct list_head        tsi_free_rpcs;    /* free rpcs */
        struct list_head        tsi_active_rpcs;  /* active rpcs */

        union {
                test_bulk_req_t bulk;             /* bulk parameter */
                test_ping_req_t ping;             /* ping parameter */
        } tsi_u;
} sfw_test_instance_t;

/* XXX: trailing (CFS_PAGE_SIZE % sizeof(lnet_process_id_t)) bytes at
 * the end of pages are not used */
#define SFW_MAX_CONCUR     LST_MAX_CONCUR
#define SFW_ID_PER_PAGE    (CFS_PAGE_SIZE / sizeof(lnet_process_id_t))
#define SFW_MAX_NDESTS     (LNET_MAX_IOV * SFW_ID_PER_PAGE)
#define sfw_id_pages(n)    (((n) + SFW_ID_PER_PAGE - 1) / SFW_ID_PER_PAGE)

typedef struct sfw_test_unit {
        struct list_head        tsu_list;         /* chain on lst_test_instance */
        lnet_process_id_t       tsu_dest;         /* id of dest node */
        int                     tsu_loop;         /* loop count of the test */
        sfw_test_instance_t    *tsu_instance;     /* pointer to test instance */
        void                   *tsu_private;      /* private data */
        swi_workitem_t          tsu_worker;       /* workitem of the test unit */
} sfw_test_unit_t;

typedef struct {
        struct list_head        tsc_list;         /* chain on fw_tests */
        srpc_service_t         *tsc_srv_service;  /* test service */
        sfw_test_client_ops_t  *tsc_cli_ops;      /* ops of test client */
} sfw_test_case_t;


srpc_client_rpc_t *
sfw_create_rpc(lnet_process_id_t peer, int service, int nbulkiov, int bulklen,
               void (*done) (srpc_client_rpc_t *), void *priv);
int sfw_create_test_rpc(sfw_test_unit_t *tsu, lnet_process_id_t peer,
                        int nblk, int blklen, srpc_client_rpc_t **rpc);
void sfw_abort_rpc(srpc_client_rpc_t *rpc);
void sfw_post_rpc(srpc_client_rpc_t *rpc);
void sfw_client_rpc_done(srpc_client_rpc_t *rpc);
void sfw_unpack_message(srpc_msg_t *msg);
void sfw_free_pages(srpc_server_rpc_t *rpc);
void sfw_add_bulk_page(srpc_bulk_t *bk, cfs_page_t *pg, int i);
int sfw_alloc_pages(srpc_server_rpc_t *rpc, int npages, int sink);

srpc_client_rpc_t *
srpc_create_client_rpc(lnet_process_id_t peer, int service,
                       int nbulkiov, int bulklen,
                       void (*rpc_done)(srpc_client_rpc_t *),
                       void (*rpc_fini)(srpc_client_rpc_t *), void *priv);
void srpc_post_rpc(srpc_client_rpc_t *rpc);
void srpc_abort_rpc(srpc_client_rpc_t *rpc, int why);
void srpc_free_bulk(srpc_bulk_t *bk);
srpc_bulk_t *srpc_alloc_bulk(int npages, int sink);
int srpc_send_rpc(swi_workitem_t *wi);
int srpc_send_reply(srpc_server_rpc_t *rpc);
int srpc_add_service(srpc_service_t *sv);
int srpc_remove_service(srpc_service_t *sv);
void srpc_shutdown_service(srpc_service_t *sv);
int srpc_finish_service(srpc_service_t *sv);
int srpc_service_add_buffers(srpc_service_t *sv, int nbuffer);
void srpc_service_remove_buffers(srpc_service_t *sv, int nbuffer);
void srpc_get_counters(srpc_counters_t *cnt);
void srpc_set_counters(const srpc_counters_t *cnt);

void swi_kill_workitem(swi_workitem_t *wi);
void swi_schedule_workitem(swi_workitem_t *wi);
void swi_schedule_serial_workitem(swi_workitem_t *wi);
int swi_startup(void);
int sfw_startup(void);
int srpc_startup(void);
void swi_shutdown(void);
void sfw_shutdown(void);
void srpc_shutdown(void);

static inline void
srpc_destroy_client_rpc (srpc_client_rpc_t *rpc)
{
        LASSERT (rpc != NULL);
        LASSERT (!srpc_event_pending(rpc));
        LASSERT (list_empty(&rpc->crpc_privl));
        LASSERT (atomic_read(&rpc->crpc_refcount) == 0);
#ifndef __KERNEL__
        LASSERT (rpc->crpc_bulk.bk_pages == NULL);
#endif

        if (rpc->crpc_fini == NULL) {
                LIBCFS_FREE(rpc, srpc_client_rpc_size(rpc));
        } else {
                (*rpc->crpc_fini) (rpc);
        }

        return;
}

static inline void
srpc_init_client_rpc (srpc_client_rpc_t *rpc, lnet_process_id_t peer,
                      int service, int nbulkiov, int bulklen,
                      void (*rpc_done)(srpc_client_rpc_t *),
                      void (*rpc_fini)(srpc_client_rpc_t *), void *priv)
{
        LASSERT (nbulkiov <= LNET_MAX_IOV);

        memset(rpc, 0, offsetof(srpc_client_rpc_t,
                                crpc_bulk.bk_iovs[nbulkiov]));

        CFS_INIT_LIST_HEAD(&rpc->crpc_list);
        CFS_INIT_LIST_HEAD(&rpc->crpc_privl);
        swi_init_workitem(&rpc->crpc_wi, rpc, srpc_send_rpc);
        spin_lock_init(&rpc->crpc_lock);
        atomic_set(&rpc->crpc_refcount, 1); /* 1 ref for caller */

        rpc->crpc_dest         = peer;
        rpc->crpc_priv         = priv;
        rpc->crpc_service      = service;
        rpc->crpc_bulk.bk_len  = bulklen;
        rpc->crpc_bulk.bk_niov = nbulkiov;
        rpc->crpc_done         = rpc_done;
        rpc->crpc_fini         = rpc_fini;
        rpc->crpc_reqstmdh     =
        rpc->crpc_replymdh     =
        rpc->crpc_bulk.bk_mdh  = LNET_INVALID_HANDLE;

        /* no event is expected at this point */
        rpc->crpc_bulkev.ev_fired  =
        rpc->crpc_reqstev.ev_fired =
        rpc->crpc_replyev.ev_fired = 1;

        rpc->crpc_reqstmsg.msg_magic   = SRPC_MSG_MAGIC;
        rpc->crpc_reqstmsg.msg_version = SRPC_MSG_VERSION;
        rpc->crpc_reqstmsg.msg_type    = srpc_service2request(service);
        return;
}

static inline const char *
swi_state2str (int state)
{
#define STATE2STR(x) case x: return #x
        switch(state) {
                default:
                        LBUG();
                STATE2STR(SWI_STATE_NEWBORN);
                STATE2STR(SWI_STATE_REPLY_SUBMITTED);
                STATE2STR(SWI_STATE_REPLY_SENT);
                STATE2STR(SWI_STATE_REQUEST_SUBMITTED);
                STATE2STR(SWI_STATE_REQUEST_SENT);
                STATE2STR(SWI_STATE_REPLY_RECEIVED);
                STATE2STR(SWI_STATE_BULK_STARTED);
                STATE2STR(SWI_STATE_BULK_ERRORED);
                STATE2STR(SWI_STATE_DONE);
        }
#undef STATE2STR
}

#define UNUSED(x)       ( (void)(x) )

#ifndef __KERNEL__

int stt_poll_interval(void);
int sfw_session_removed(void);

int stt_check_events(void);
int swi_check_events(void);
int srpc_check_event(int timeout);

int lnet_selftest_init(void);
void lnet_selftest_fini(void);
int selftest_wait_events(void);

#else

#define selftest_wait_events()    cfs_pause(cfs_time_seconds(1))

#endif

#define lst_wait_until(cond, lock, fmt, a...)                           \
do {                                                                    \
        int __I = 2;                                                    \
        while (!(cond)) {                                               \
                __I++;                                                  \
                CDEBUG(((__I & (-__I)) == __I) ? D_WARNING :            \
                                                 D_NET,     /* 2**n? */ \
                       fmt, ## a);                                      \
                spin_unlock(&(lock));                                   \
                                                                        \
                selftest_wait_events();                                 \
                                                                        \
                spin_lock(&(lock));                                     \
        }                                                               \
} while (0)

static inline void
srpc_wait_service_shutdown (srpc_service_t *sv)
{
        int i = 2;

        spin_lock(&sv->sv_lock);
        LASSERT (sv->sv_shuttingdown);
        spin_unlock(&sv->sv_lock);

        while (srpc_finish_service(sv) == 0) {
                i++;
                CDEBUG (((i & -i) == i) ? D_WARNING : D_NET,
                        "Waiting for %s service to shutdown...\n",
                        sv->sv_name);
                selftest_wait_events();
        }
}

#endif /* __SELFTEST_SELFTEST_H__ */
