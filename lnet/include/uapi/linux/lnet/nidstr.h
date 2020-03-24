/*
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
 * http://www.gnu.org/licenses/gpl-2.0.html
 *
 * GPL HEADER END
 */
/*
 * Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
 * Use is subject to license terms.
 *
 * Copyright (c) 2015, 2017, Intel Corporation.
 */
#ifndef _LNET_NIDSTRINGS_H
#define _LNET_NIDSTRINGS_H

#include <linux/types.h>
#include <linux/lnet/lnet-types.h>

/**
 *  Lustre Network Driver types.
 */
enum {
	/* Only add to these values (i.e. don't ever change or redefine them):
	 * network addresses depend on them... */
	/*QSWLND	= 1, removed v2_7_50                 */
	SOCKLND		= 2,
	/*GMLND		= 3, removed v2_0_0-rc1a-16-gc660aac */
	/*PTLLND	= 4, removed v2_7_50                 */
	O2IBLND		= 5,
	/*CIBLND	= 6, removed v2_0_0-rc1a-175-gd2b8a0e */
	/*OPENIBLND	= 7, removed v2_0_0-rc1a-175-gd2b8a0e */
	/*IIBLND	= 8, removed v2_0_0-rc1a-175-gd2b8a0e */
	LOLND		= 9,
	/*RALND		= 10, removed v2_7_50_0-34-g8be9e41    */
	/*VIBLND	= 11, removed v2_0_0-rc1a-175-gd2b8a0e */
	/*MXLND		= 12, removed v2_7_50_0-34-g8be9e41    */
	GNILND		= 13,
	GNIIPLND	= 14,
	PTL4LND		= 15,

	NUM_LNDS
};

struct list_head;

#define LNET_NIDSTR_COUNT 1024	/* # of nidstrings */
#define LNET_NIDSTR_SIZE  32	/* size of each one (see below for usage) */

/* support decl needed by both kernel and user space */
char *libcfs_next_nidstring(void);
int libcfs_isknown_lnd(__u32 lnd);
char *libcfs_lnd2modname(__u32 lnd);
char *libcfs_lnd2str_r(__u32 lnd, char *buf, __kernel_size_t buf_size);
static inline char *libcfs_lnd2str(__u32 lnd)
{
	return libcfs_lnd2str_r(lnd, libcfs_next_nidstring(),
				LNET_NIDSTR_SIZE);
}
int libcfs_str2lnd(const char *str);
char *libcfs_net2str_r(__u32 net, char *buf, __kernel_size_t buf_size);
static inline char *libcfs_net2str(__u32 net)
{
	return libcfs_net2str_r(net, libcfs_next_nidstring(),
				LNET_NIDSTR_SIZE);
}
char *libcfs_nid2str_r(lnet_nid_t nid, char *buf, __kernel_size_t buf_size);
static inline char *libcfs_nid2str(lnet_nid_t nid)
{
	return libcfs_nid2str_r(nid, libcfs_next_nidstring(),
				LNET_NIDSTR_SIZE);
}
__u32 libcfs_str2net(const char *str);
lnet_nid_t libcfs_str2nid(const char *str);
int libcfs_str2anynid(lnet_nid_t *nid, const char *str);
char *libcfs_id2str(struct lnet_process_id id);
void cfs_free_nidlist(struct list_head *list);
int cfs_parse_nidlist(char *str, int len, struct list_head *list);
int cfs_print_nidlist(char *buffer, int count, struct list_head *list);
int cfs_match_nid(lnet_nid_t nid, struct list_head *list);
int cfs_expand_nidlist(struct list_head *nidlist, lnet_nid_t *lnet_nidlist,
		       int max_nids);
int cfs_ip_addr_parse(char *str, int len, struct list_head *list);
int cfs_ip_addr_match(__u32 addr, struct list_head *list);
int cfs_nidrange_find_min_max(struct list_head *nidlist, char *min_nid,
			       char *max_nid, __kernel_size_t nidstr_length);

struct netstrfns {
	__u32	nf_type;
	char	*nf_name;
	char	*nf_modname;
	void	(*nf_addr2str)(__u32 addr, char *str, __kernel_size_t size);
	int	(*nf_str2addr)(const char *str, int nob, __u32 *addr);
	int	(*nf_parse_addrlist)(char *str, int len,
				     struct list_head *list);
	int	(*nf_print_addrlist)(char *buffer, int count,
				     struct list_head *list);
	int	(*nf_match_addr)(__u32 addr, struct list_head *list);
	int	(*nf_min_max)(struct list_head *nidlist, __u32 *min_nid,
			      __u32 *max_nid);
	int	(*nf_expand_addrrange)(struct list_head *addrranges,
				       __u32 *addrs, int max_addrs);
};

#endif /* _LNET_NIDSTRINGS_H */
