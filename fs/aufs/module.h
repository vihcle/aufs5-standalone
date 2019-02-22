/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2005-2019 Junjiro R. Okajima
 */

/*
 * module initialization and module-global
 */

#ifndef __AUFS_MODULE_H__
#define __AUFS_MODULE_H__

#ifdef __KERNEL__

#include <linux/slab.h>
#include "debug.h"
#include "dentry.h"
#include "inode.h"

struct path;
struct seq_file;

/* module parameters */
extern int sysaufs_brs;

/* ---------------------------------------------------------------------- */

void *au_krealloc(void *p, unsigned int new_sz, gfp_t gfp, int may_shrink);
void *au_kzrealloc(void *p, unsigned int nused, unsigned int new_sz, gfp_t gfp,
		   int may_shrink);

/*
 * Comparing the size of the object with sizeof(struct rcu_head)
 * case 1: object is always larger
 *	--> au_kfree_rcu() or au_kfree_do_rcu()
 * case 2: object is always smaller
 *	--> au_kfree_small()
 * case 3: object can be any size
 *	--> au_kfree_try_rcu()
 */

static inline void au_kfree_do_rcu(const void *p)
{
	struct {
		struct rcu_head rcu;
	} *a = (void *)p;

	kfree_rcu(a, rcu);
}

#define au_kfree_rcu(_p) do {						\
		typeof(_p) p = (_p);					\
		BUILD_BUG_ON(sizeof(*p) < sizeof(struct rcu_head));	\
		if (p)							\
			au_kfree_do_rcu(p);				\
	} while (0)

#define au_kfree_do_sz_test(sz)	(sz >= sizeof(struct rcu_head))
#define au_kfree_sz_test(p)	(p && au_kfree_do_sz_test(ksize(p)))

static inline void au_kfree_try_rcu(const void *p)
{
	if (!p)
		return;
	if (au_kfree_sz_test(p))
		au_kfree_do_rcu(p);
	else
		kfree(p);
}

static inline void au_kfree_small(const void *p)
{
	if (!p)
		return;
	AuDebugOn(au_kfree_sz_test(p));
	kfree(p);
}

static inline int au_kmidx_sub(size_t sz, size_t new_sz)
{
#ifndef CONFIG_SLOB
	return kmalloc_index(sz) - kmalloc_index(new_sz);
#else
	return -1; /* SLOB is untested */
#endif
}

int au_seq_path(struct seq_file *seq, struct path *path);

/* ---------------------------------------------------------------------- */

/* kmem cache */
enum {
	AuCache_DINFO,
	AuCache_ICNTNR,
	AuCache_Last
};

extern struct kmem_cache *au_cache[AuCache_Last];

#define AuCacheFlags		(SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD)
#define AuCache(type)		KMEM_CACHE(type, AuCacheFlags)
#define AuCacheCtor(type, ctor)	\
	kmem_cache_create(#type, sizeof(struct type), \
			  __alignof__(struct type), AuCacheFlags, ctor)

#define AuCacheFuncs(name, index)					\
	static inline struct au_##name *au_cache_alloc_##name(void)	\
	{ return kmem_cache_alloc(au_cache[AuCache_##index], GFP_NOFS); } \
	static inline void au_cache_free_##name##_norcu(struct au_##name *p) \
	{ kmem_cache_free(au_cache[AuCache_##index], p); }		\
									\
	static inline void au_cache_free_##name##_rcu_cb(struct rcu_head *rcu) \
	{ void *p = rcu;						\
		p -= offsetof(struct au_##name, rcu);			\
		kmem_cache_free(au_cache[AuCache_##index], p); }	\
	static inline void au_cache_free_##name##_rcu(struct au_##name *p) \
	{ BUILD_BUG_ON(sizeof(struct au_##name) < sizeof(struct rcu_head)); \
		call_rcu(&p->rcu, au_cache_free_##name##_rcu_cb); }	\
									\
	static inline void au_cache_free_##name(struct au_##name *p)	\
	{ /* au_cache_free_##name##_norcu(p); */			\
		au_cache_free_##name##_rcu(p); }

AuCacheFuncs(dinfo, DINFO);
AuCacheFuncs(icntnr, ICNTNR);

#endif /* __KERNEL__ */
#endif /* __AUFS_MODULE_H__ */
