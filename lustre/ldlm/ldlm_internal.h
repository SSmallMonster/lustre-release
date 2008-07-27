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
 */

#define MAX_STRING_SIZE 128

extern atomic_t ldlm_srv_namespace_nr;
extern atomic_t ldlm_cli_namespace_nr;
extern struct semaphore ldlm_srv_namespace_lock;
extern struct list_head ldlm_srv_namespace_list;
extern struct semaphore ldlm_cli_namespace_lock;
extern struct list_head ldlm_cli_namespace_list;

static inline atomic_t *ldlm_namespace_nr(ldlm_side_t client)
{
        return client == LDLM_NAMESPACE_SERVER ? 
                &ldlm_srv_namespace_nr : &ldlm_cli_namespace_nr;
}

static inline struct list_head *ldlm_namespace_list(ldlm_side_t client)
{
        return client == LDLM_NAMESPACE_SERVER ? 
                &ldlm_srv_namespace_list : &ldlm_cli_namespace_list;
}

static inline struct semaphore *ldlm_namespace_lock(ldlm_side_t client)
{
        return client == LDLM_NAMESPACE_SERVER ? 
                &ldlm_srv_namespace_lock : &ldlm_cli_namespace_lock;
}

/* ldlm_request.c */
typedef enum {
        LDLM_ASYNC,
        LDLM_SYNC,
} ldlm_sync_t;

/* Cancel lru flag, it indicates we cancel aged locks. */
enum {
        LDLM_CANCEL_AGED   = 1 << 0, /* Cancel aged locks (non lru resize). */
        LDLM_CANCEL_PASSED = 1 << 1, /* Cancel passed number of locks. */
        LDLM_CANCEL_SHRINK = 1 << 2, /* Cancel locks from shrinker. */
        LDLM_CANCEL_LRUR   = 1 << 3  /* Cancel locks from lru resize. */
};

int ldlm_cancel_lru(struct ldlm_namespace *ns, int nr, ldlm_sync_t sync, 
                    int flags);
int ldlm_cancel_lru_local(struct ldlm_namespace *ns, struct list_head *cancels,
                          int count, int max, int cancel_flags, int flags);
int ldlm_cancel_lru_estimate(struct ldlm_namespace *ns, int count, int max, 
                             int flags);
extern int ldlm_enqueue_min;
int ldlm_get_enq_timeout(struct ldlm_lock *lock);

/* ldlm_resource.c */
int ldlm_resource_putref_locked(struct ldlm_resource *res);
void ldlm_resource_insert_lock_after(struct ldlm_lock *original,
                                     struct ldlm_lock *new);
void ldlm_namespace_free_prior(struct ldlm_namespace *ns,
                               struct obd_import *imp, int force);
void ldlm_namespace_free_post(struct ldlm_namespace *ns);
/* ldlm_lock.c */

/* Number of blocking/completion callbacks that will be sent in
 * parallel (see bug 11301). */
#define PARALLEL_AST_LIMIT      200

struct ldlm_cb_set_arg {
        struct ptlrpc_request_set *set;
        atomic_t restart;
        __u32 type; /* LDLM_BL_CALLBACK or LDLM_CP_CALLBACK */
};

typedef enum {
        LDLM_WORK_BL_AST,
        LDLM_WORK_CP_AST,
        LDLM_WORK_REVOKE_AST
} ldlm_desc_ast_t; 

void ldlm_grant_lock(struct ldlm_lock *lock, struct list_head *work_list);
struct ldlm_lock *
ldlm_lock_create(struct ldlm_namespace *ns, const struct ldlm_res_id *,
                 ldlm_type_t type, ldlm_mode_t, ldlm_blocking_callback,
                 ldlm_completion_callback, ldlm_glimpse_callback, void *data,
                 __u32 lvb_len);
ldlm_error_t ldlm_lock_enqueue(struct ldlm_namespace *, struct ldlm_lock **,
                               void *cookie, int *flags);
void ldlm_lock_addref_internal(struct ldlm_lock *, __u32 mode);
void ldlm_lock_decref_internal(struct ldlm_lock *, __u32 mode);
void ldlm_add_ast_work_item(struct ldlm_lock *lock, struct ldlm_lock *new,
                                struct list_head *work_list);
int ldlm_reprocess_queue(struct ldlm_resource *res, struct list_head *queue,
                         struct list_head *work_list);
int ldlm_run_ast_work(struct list_head *rpc_list, ldlm_desc_ast_t ast_type);
int ldlm_lock_remove_from_lru(struct ldlm_lock *lock);
int ldlm_lock_remove_from_lru_nolock(struct ldlm_lock *lock);
void ldlm_lock_add_to_lru_nolock(struct ldlm_lock *lock);
void ldlm_lock_add_to_lru(struct ldlm_lock *lock);
void ldlm_lock_touch_in_lru(struct ldlm_lock *lock);
void ldlm_lock_destroy_nolock(struct ldlm_lock *lock);

/* ldlm_lockd.c */
int ldlm_bl_to_thread_lock(struct ldlm_namespace *ns, struct ldlm_lock_desc *ld,
                           struct ldlm_lock *lock);
int ldlm_bl_to_thread_list(struct ldlm_namespace *ns, struct ldlm_lock_desc *ld,
                           struct list_head *cancels, int count);

void ldlm_handle_bl_callback(struct ldlm_namespace *ns,
                             struct ldlm_lock_desc *ld, struct ldlm_lock *lock);

/* ldlm_plain.c */
int ldlm_process_plain_lock(struct ldlm_lock *lock, int *flags, int first_enq,
                            ldlm_error_t *err, struct list_head *work_list);

/* ldlm_extent.c */
int ldlm_process_extent_lock(struct ldlm_lock *lock, int *flags, int first_enq,
                             ldlm_error_t *err, struct list_head *work_list);
void ldlm_extent_add_lock(struct ldlm_resource *res, struct ldlm_lock *lock);
void ldlm_extent_unlink_lock(struct ldlm_lock *lock);

/* ldlm_flock.c */
int ldlm_process_flock_lock(struct ldlm_lock *req, int *flags, int first_enq,
                            ldlm_error_t *err, struct list_head *work_list);

/* ldlm_inodebits.c */
int ldlm_process_inodebits_lock(struct ldlm_lock *lock, int *flags,
                                int first_enq, ldlm_error_t *err,
                                struct list_head *work_list);

/* l_lock.c */
void l_check_ns_lock(struct ldlm_namespace *ns);
void l_check_no_ns_lock(struct ldlm_namespace *ns);

extern cfs_proc_dir_entry_t *ldlm_svc_proc_dir;
extern cfs_proc_dir_entry_t *ldlm_type_proc_dir;

struct ldlm_state {
        struct ptlrpc_service *ldlm_cb_service;
        struct ptlrpc_service *ldlm_cancel_service;
        struct ptlrpc_client *ldlm_client;
        struct ptlrpc_connection *ldlm_server_conn;
        struct ldlm_bl_pool *ldlm_bl_pool;
};

/* interval tree, for LDLM_EXTENT. */
extern cfs_mem_cache_t *ldlm_interval_slab; /* slab cache for ldlm_interval */
extern void ldlm_interval_attach(struct ldlm_interval *n, struct ldlm_lock *l);
extern struct ldlm_interval *ldlm_interval_detach(struct ldlm_lock *l);
extern struct ldlm_interval *ldlm_interval_alloc(struct ldlm_lock *lock);
extern void ldlm_interval_free(struct ldlm_interval *node);
/* this function must be called with res lock held */
static inline struct ldlm_extent *
ldlm_interval_extent(struct ldlm_interval *node)
{
        struct ldlm_lock *lock;
        LASSERT(!list_empty(&node->li_group));

        lock = list_entry(node->li_group.next, struct ldlm_lock, l_sl_policy);
        return &lock->l_policy_data.l_extent;
}

int ldlm_init(void);
void ldlm_exit(void);

enum ldlm_policy_res {
        LDLM_POLICY_CANCEL_LOCK,
        LDLM_POLICY_KEEP_LOCK
};

typedef enum ldlm_policy_res ldlm_policy_res_t;

#define LDLM_POOL_PROC_READER(var, type)                                    \
        static int lprocfs_rd_##var(char *page, char **start, off_t off,    \
                                    int count, int *eof, void *data)        \
        {                                                                   \
                struct ldlm_pool *pl = data;                                \
                type tmp;                                                   \
                                                                            \
                spin_lock(&pl->pl_lock);                                    \
                tmp = pl->pl_##var;                                         \
                spin_unlock(&pl->pl_lock);                                  \
                                                                            \
                return lprocfs_rd_uint(page, start, off, count, eof, &tmp); \
        }                                                                   \
        struct __##var##__dummy_read {;} /* semicolon catcher */

#define LDLM_POOL_PROC_WRITER(var, type)                                    \
        int lprocfs_wr_##var(struct file *file, const char *buffer,         \
                             unsigned long count, void *data)               \
        {                                                                   \
                struct ldlm_pool *pl = data;                                \
                type tmp;                                                   \
                int rc;                                                     \
                                                                            \
                rc = lprocfs_wr_uint(file, buffer, count, &tmp);            \
                if (rc) {                                                   \
                        CERROR("Can't parse user input, rc = %d\n", rc);    \
                        return rc;                                          \
                }                                                           \
                                                                            \
                spin_lock(&pl->pl_lock);                                    \
                pl->pl_##var = tmp;                                         \
                spin_unlock(&pl->pl_lock);                                  \
                                                                            \
                return rc;                                                  \
        }                                                                   \
        struct __##var##__dummy_write {;} /* semicolon catcher */
