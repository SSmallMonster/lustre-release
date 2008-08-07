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
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/lustre/docs/GPLv2.pdf
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
 * libcfs/libcfs/linux/linux-curproc.c
 *
 * Lustre curproc API implementation for Linux kernel
 *
 * Author: Nikita Danilov <nikita@clusterfs.com>
 */

#include <linux/sched.h>

#define DEBUG_SUBSYSTEM S_LNET

#include <libcfs/libcfs.h>

/*
 * Implementation of cfs_curproc API (see portals/include/libcfs/curproc.h)
 * for Linux kernel.
 */

uid_t  cfs_curproc_uid(void)
{
        return current->uid;
}

gid_t  cfs_curproc_gid(void)
{
        return current->gid;
}

uid_t  cfs_curproc_fsuid(void)
{
        return current->fsuid;
}

gid_t  cfs_curproc_fsgid(void)
{
        return current->fsgid;
}

pid_t  cfs_curproc_pid(void)
{
        return current->pid;
}

int    cfs_curproc_groups_nr(void)
{
        int nr;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)
        task_lock(current);
        nr = current->group_info->ngroups;
        task_unlock(current);
#else
        nr = current->ngroups;
#endif
        return nr;
}

void   cfs_curproc_groups_dump(gid_t *array, int size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)
        task_lock(current);
        size = min_t(int, size, current->group_info->ngroups);
        memcpy(array, current->group_info->blocks[0], size * sizeof(__u32));
        task_unlock(current);
#else
        LASSERT(size <= NGROUPS);
        size = min_t(int, size, current->ngroups);
        memcpy(array, current->groups, size * sizeof(__u32));
#endif
}


int    cfs_curproc_is_in_groups(gid_t gid)
{
        return in_group_p(gid);
}

mode_t cfs_curproc_umask(void)
{
        return current->fs->umask;
}

char  *cfs_curproc_comm(void)
{
        return current->comm;
}

cfs_kernel_cap_t cfs_curproc_cap_get(void)
{
        return current->cap_effective;
}

void cfs_curproc_cap_set(cfs_kernel_cap_t cap)
{
        current->cap_effective = cap;
}

EXPORT_SYMBOL(cfs_curproc_uid);
EXPORT_SYMBOL(cfs_curproc_pid);
EXPORT_SYMBOL(cfs_curproc_gid);
EXPORT_SYMBOL(cfs_curproc_fsuid);
EXPORT_SYMBOL(cfs_curproc_fsgid);
EXPORT_SYMBOL(cfs_curproc_umask);
EXPORT_SYMBOL(cfs_curproc_comm);
EXPORT_SYMBOL(cfs_curproc_groups_nr);
EXPORT_SYMBOL(cfs_curproc_groups_dump);
EXPORT_SYMBOL(cfs_curproc_is_in_groups);
EXPORT_SYMBOL(cfs_curproc_cap_get);
EXPORT_SYMBOL(cfs_curproc_cap_set);

/*
 * Local variables:
 * c-indentation-style: "K&R"
 * c-basic-offset: 8
 * tab-width: 8
 * fill-column: 80
 * scroll-step: 1
 * End:
 */
