/* -*- mode: c; c-file-style: "linux" -*- */
/* Run a micro benchmark on ip6_route_output().
 *
 * It creates a /sys/kernel/kbench directory. By default, a scan is
 * done in 2000::/3 (considered as a linear space).
 *
 * The module only acts on the initial network namespace.
 *
 * Copyright (C) 2017 Vincent Bernat
 * Based on https://git.kernel.org/pub/scm/linux/kernel/git/davem/net_test_tools.git/tree/kbench_mod.c
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#define pr_fmt(fmt) "kbench: " fmt

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/inet.h>
#include <linux/sort.h>
#include <linux/netdevice.h>
#include <linux/mutex.h>

#include <net/ip6_route.h>
#include <net/ip6_fib.h>

#include <linux/timex.h>

#define DEFAULT_WARMUP_COUNT	100000
#define DEFAULT_LOOP_COUNT	5000
#define DEFAULT_MAX_LOOP_COUNT	1000000
#define DEFAULT_OIF		0
#define DEFAULT_IIF		0
#define DEFAULT_MARK		0x00000000
#define DEFAULT_LABEL		0
#define DEFAULT_DST_IPADDR_S	{ .s6_addr = {0x20} }
#define DEFAULT_DST_IPADDR_E	{ .s6_addr = {0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, \
					      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff} }
#define DEFAULT_SRC_IPADDR	{ .s6_addr32 = {} }

#define HIST_BUCKETS		15
#define HIST_WIDTH		50

static unsigned long	warmup_count	= DEFAULT_WARMUP_COUNT;
static unsigned long	loop_count	= DEFAULT_LOOP_COUNT;
static unsigned long	max_loop_count	= DEFAULT_MAX_LOOP_COUNT;
static int		flow_oif	= DEFAULT_OIF;
static int		flow_iif	= DEFAULT_IIF;
static u32		flow_label	= DEFAULT_LABEL;
static u32		flow_mark	= DEFAULT_MARK;
static struct in6_addr	flow_dst_ipaddr_s = DEFAULT_DST_IPADDR_S;
static struct in6_addr	flow_dst_ipaddr_e = DEFAULT_DST_IPADDR_E;
static struct in6_addr	flow_src_ipaddr = DEFAULT_SRC_IPADDR;

static DEFINE_MUTEX(kb_lock);

/* Compatibility with older kernel versions */
#ifndef __ATTR_RW
# define __ATTR_RW(_name) __ATTR(_name,				\
				(S_IWUSR | S_IRUGO),		\
				_name##_show, _name##_store)
#endif

/* Helpers */

static int compare(const void *lhs, const void *rhs) {
    unsigned long long lhs_integer = *(const unsigned long long *)(lhs);
    unsigned long long rhs_integer = *(const unsigned long long *)(rhs);

    if (lhs_integer < rhs_integer) return -1;
    if (lhs_integer > rhs_integer) return 1;
    return 0;
}

static unsigned long long percentile(int p,
				     unsigned long long *sorted,
				     unsigned int count)
{
	int index = p * count / 100;
	int index2 = index + 1;
	if (p * count % 100 == 0)
		return sorted[index];
	if (index2 >= count)
		index2 = index - 1;
	if (index2 < 0)
		index2 = index;
	return (sorted[index] + sorted[index+1]) / 2;
}

static unsigned long long mad(unsigned long long *sorted,
			      unsigned long long median,
			      unsigned count)
{
	unsigned long long *dmedian = kmalloc(sizeof(unsigned long long) * count, GFP_KERNEL);
	unsigned long long res;
	unsigned i;
	for (i = 0; i < count; i++) {
		if (sorted[i] > median)
			dmedian[i] = sorted[i] - median;
		else
			dmedian[i] = median - sorted[i];
	}
	sort(dmedian, count, sizeof(unsigned long long), compare, NULL);
	res = percentile(50, dmedian, count);
	kfree(dmedian);
	return res;
}

static void lcg32(unsigned long *cur)
{
	*cur = *cur * 1664525 + 1013904223;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
#ifdef CONFIG_IPV6_SUBTREES
#define FWS_INIT FWS_S
#else
#define FWS_INIT FWS_L
#endif

static void collect_depth(struct fib6_node *root,
			  unsigned long *avgdepth, unsigned long *maxdepth)
{
	unsigned long totdepth, depth;
	unsigned int count;
	struct fib6_node *fn, *pn, *node;
	struct rt6_info *leaf;
	enum fib6_walk_state state;
	for (node = root, leaf = NULL,
		     depth = 0, totdepth = 0, count = 0, *maxdepth = 0, *avgdepth = 0,
		     state = FWS_INIT;;) {
		fn = node;
		if (!fn)
			goto end;
		switch (state) {
#ifdef CONFIG_IPV6_SUBTREES
		case FWS_S:
			if (FIB6_SUBTREE(fn)) {
				node = FIB6_SUBTREE(fn);
				continue;
			}
			state = FWS_L;
#endif
		case FWS_L:
			if (fn->left) {
				node = fn->left;
				depth++;
				state = FWS_INIT;
				continue;
			}
			state = FWS_R;
		case FWS_R:
			if (fn->right) {
				node = fn->right;
				depth++;
				state = FWS_INIT;
				continue;
			}
			state = FWS_C;
			leaf = fn->leaf;
		case FWS_C:
			if (leaf && fn->fn_flags & RTN_RTINFO) {
				totdepth += depth;
				count++;
				if (depth > *maxdepth)
					*maxdepth = depth;
				leaf = NULL;
				continue;
			}
			state = FWS_U;
		case FWS_U:
			if (fn == root) {
				goto end;
			}
			pn = fn->parent;
			node = pn;
			depth--;
#ifdef CONFIG_IPV6_SUBTREES
			if (FIB6_SUBTREE(pn) == fn) {
				WARN_ON(!(fn->fn_flags & RTN_ROOT));
				state = FWS_L;
				continue;
			}
#endif
			if (pn->left == fn) {
				state = FWS_R;
				continue;
			}
			if (pn->right == fn) {
				state = FWS_C;
				leaf = node->leaf;
				continue;
			}
		}
	}
end:
	if (count > 0) *avgdepth = totdepth*10 / count;
}
#endif

/* Benchmark */

static int do_bench(char *buf, int verbose)
{
	unsigned long long *results;
	unsigned long long t1, t2, average;
	unsigned long i, j, total, count, count2, carry;
	bool scan;
	unsigned long rnd = 0;
	struct flowi6 fl6;
	struct in6_addr delta = {};

	results = kmalloc(sizeof(*results) * loop_count, GFP_KERNEL);
	if (!results)
		return scnprintf(buf, PAGE_SIZE, "msg=\"no memory\"\n");

	mutex_lock(&kb_lock);
	memset(&fl6, 0, sizeof(fl6));
	fl6.flowi6_oif = flow_oif;
	fl6.flowi6_iif = flow_iif;
	fl6.flowi6_mark = flow_mark;
	fl6.flowi6_proto = NEXTHDR_TCP;
	fl6.flowlabel = flow_label;
	memcpy(&fl6.daddr, &flow_dst_ipaddr_s, sizeof(flow_dst_ipaddr_s));
	memcpy(&fl6.saddr, &flow_src_ipaddr, sizeof(flow_src_ipaddr));
	for (i = 0, carry = 0; i < 4; i++) {
		if ((unsigned long long)ntohl(flow_dst_ipaddr_s.s6_addr32[3 - i]) + carry <=
		    ntohl(flow_dst_ipaddr_e.s6_addr32[3 - i])) {
			delta.s6_addr32[3-i] = ntohl(flow_dst_ipaddr_e.s6_addr32[3 - i]) -
				ntohl(flow_dst_ipaddr_s.s6_addr32[3 - i]) -
				carry;
			carry = 0;
		} else {
			delta.s6_addr32[3-i] = ntohl(flow_dst_ipaddr_s.s6_addr32[3 - i]) + carry -
				ntohl(flow_dst_ipaddr_e.s6_addr32[3 - i]);
			carry = 1;
		}
	}
	if (carry == 0 &&
	    (delta.s6_addr32[0] != 0 || delta.s6_addr32[1] != 0 ||
	     delta.s6_addr32[2] != 0 || delta.s6_addr32[3] != 0)) {
		unsigned long rem;
		unsigned long long quo;
		scan = true;
		for (i = 0, rem = 0; i < 4; i++) {
			quo = delta.s6_addr32[i] + ((unsigned long long)rem << 32);
			rem = quo % loop_count;
			quo /= loop_count;
			delta.s6_addr32[i] = quo;
		}
		if (delta.s6_addr32[0] == 0 && delta.s6_addr32[1] == 0 &&
		    delta.s6_addr32[2] == 0 && delta.s6_addr32[3] == 0)
			delta.s6_addr32[3] = 1;
		for (i = 0; i < 4; i++)
			delta.s6_addr32[i] = htonl(delta.s6_addr32[i]);
	}

	for (i = 0; i < warmup_count; i++) {
		struct dst_entry *dst = ip6_route_output(&init_net, NULL, &fl6);
		if (dst->error && dst->error != -ENETUNREACH) {
			dst_release(dst);
			kfree(results);
			return scnprintf(buf, PAGE_SIZE, "err=%d msg=\"lookup error\"\n", dst->error);
		}
		dst_release(dst);
	}

	average = 0;
	for (i = total = 0; i < max_loop_count; i++) {
		struct dst_entry *dst;
		if (total >= loop_count)
			break;
		if (scan) {
			for (j = 0, carry = 0; j < 4; j++) {
				carry = ((unsigned long long)ntohl(fl6.daddr.s6_addr32[3-j]) +
					 ntohl(delta.s6_addr32[3-j]) +
					 carry > ULONG_MAX);
				fl6.daddr.s6_addr32[3-j] = htonl(ntohl(fl6.daddr.s6_addr32[3-j]) +
								 ntohl(delta.s6_addr32[3-j]) +
								 carry);
			}
			if (ntohl(fl6.daddr.s6_addr32[0]) > ntohl(flow_dst_ipaddr_e.s6_addr32[0]) ||
			    (ntohl(fl6.daddr.s6_addr32[0]) == ntohl(flow_dst_ipaddr_e.s6_addr32[0]) &&
			     (ntohl(fl6.daddr.s6_addr32[1]) > ntohl(flow_dst_ipaddr_e.s6_addr32[1]) ||
			      (ntohl(fl6.daddr.s6_addr32[1]) == ntohl(flow_dst_ipaddr_e.s6_addr32[1]) &&
			       (ntohl(fl6.daddr.s6_addr32[2]) > ntohl(flow_dst_ipaddr_e.s6_addr32[2]) ||
				(ntohl(fl6.daddr.s6_addr32[2]) == ntohl(flow_dst_ipaddr_e.s6_addr32[2]) &&
				 ntohl(fl6.daddr.s6_addr32[3]) > ntohl(flow_dst_ipaddr_e.s6_addr32[3]))))))) {
				memcpy(&fl6.daddr, &flow_dst_ipaddr_s, sizeof(flow_dst_ipaddr_s));
				/* Add a bit of (reproducible)
				 * randomness to the first step to
				 * avoid using the same routes. */
				for (j = 0, carry = 0; j < 4; j++) {
					unsigned long add = ntohl(delta.s6_addr32[3-j]);
					lcg32(&rnd);
					add &= rnd;
					carry = ((unsigned long long)ntohl(fl6.daddr.s6_addr32[3-j]) +
						 add +
						 carry > ULONG_MAX);
					fl6.daddr.s6_addr32[3-j] = htonl(ntohl(fl6.daddr.s6_addr32[3-j]) +
									 add +
									 carry);
				}
				schedule();
			}
		}
		/* Could use sched_clock() to get a number of
		 * nanoseconds instead. This would be the one used for
		 * ftrace. get_cycles() use RDTSC behind the scene and
		 * this instruction is virtualized with low-overhead
		 * (see cpu_has_vmx_rdtscp() for support, which can be
		 * checked with the following command-line: `sudo
		 * rdmsr 0x0000048b` (which is
		 * MSR_IA32_VMX_PROCBASED_CTLS2), and it should have
		 * 0x8 bit set (which is SECONDARY_EXEC_RDTSCP) in the
		 * high word. For example, if we have 0x7cff00000000,
		 * high word is 0x7cff, so 0x8 bit is set and it's
		 * OK. */
		t1 = get_cycles();
		dst = ip6_route_output(&init_net, NULL, &fl6);
		t2 = get_cycles();
		if (dst->error == -ENETUNREACH) {
			dst_release(dst);
			continue;
		}
		dst_release(dst);
		results[total] = t2 - t1;
		average += results[total];
		total++;
	}
	mutex_unlock(&kb_lock);

	/* Compute statistics */
	sort(results, total, sizeof(*results), compare, NULL);
	if (total == 0) {
		scnprintf(buf, PAGE_SIZE, "msg=\"no match\"\n");
	} else {
		unsigned long long p95 = percentile(95, results, total);
		unsigned long long p90 = percentile(90, results, total);
		unsigned long long p50 = percentile(50, results, total);
		average /= total;
		scnprintf(buf, PAGE_SIZE,
			  "min=%llu max=%llu count=%lu average=%llu 95th=%llu 90th=%llu 50th=%llu mad=%llu\n",
			  results[0],
			  results[total - 1],
			  total,
			  average,
			  p95,
			  p90,
			  p50,
			  mad(results, p50, total));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
		do {
			unsigned long avgdepth, maxdepth;
			struct fib6_table *table = init_net.ipv6.fib6_main_tbl;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
			spin_lock_bh(&table->tb6_lock);
#else
			read_lock_bh(&table->tb6_lock);
#endif
			collect_depth(&table->tb6_root, &avgdepth, &maxdepth);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
			spin_unlock_bh(&table->tb6_lock);
#else
			read_unlock_bh(&table->tb6_lock);
#endif
			scnprintf(buf + strnlen(buf, PAGE_SIZE), PAGE_SIZE - strnlen(buf, PAGE_SIZE),
				  "table=%u avgdepth=%lu.%lu maxdepth=%lu\n",
				  table->tb6_id, avgdepth/10, avgdepth%10, maxdepth);
		} while(0);
#endif
		if (verbose) {
			/* Display an histogram */
			unsigned long long share = (p95 - results[0]) / HIST_BUCKETS;
			unsigned long long start = results[0];
			int order = (ilog2(share) * 3 + 5) / 10;
			char *hist_buf = buf + strnlen(buf, PAGE_SIZE);

			if (order <= 0) order = 1;
			for (i = order, order = 1; i > 1; i--) {
				order *= 10;
			}
			share = share/order * order;
			if (share <= 0) share = 1;
			start = start/order * order;

			hist_buf += scnprintf(hist_buf, buf + PAGE_SIZE - hist_buf, " %8s │", "value");
			hist_buf += scnprintf(hist_buf, buf + PAGE_SIZE - hist_buf, "%*s┊%*s",
					      HIST_WIDTH/2, "",
					      HIST_WIDTH/2-1, "");
			hist_buf += scnprintf(hist_buf, buf + PAGE_SIZE - hist_buf, " %8s\n", "count");

			for (i = 0, count = 0, count2 = 0;;) {
				if (i < total &&
				    results[i] < start + share) {
					count++; count2++; i++;
					continue;
				}
				hist_buf += scnprintf(hist_buf, buf + PAGE_SIZE - hist_buf,
						      " %8llu │", start);
				for (j = 0; j < count * HIST_WIDTH / total; j++)
					hist_buf += scnprintf(hist_buf, buf + PAGE_SIZE - hist_buf,
							      "▒");
				for (; j < count2 * HIST_WIDTH / total; j++)
					hist_buf += scnprintf(hist_buf, buf + PAGE_SIZE - hist_buf,
							      "░");
				hist_buf += scnprintf(hist_buf, buf + PAGE_SIZE - hist_buf,
						      "%*s %8lu\n",
						      (int)(HIST_WIDTH - count2 * HIST_WIDTH / total), "",
						      count);
				count = 0;
				start += share;
				if (i >= total) break;
				if (results[i] > p95) break;
				if (hist_buf >= buf + PAGE_SIZE - HIST_WIDTH - 20) break;
			}
		}
	}

	kfree(results);
	return strnlen(buf, PAGE_SIZE);
}

/* Sysfs attributes */

static ssize_t warmup_count_show(struct kobject *kobj, struct kobj_attribute *attr,
				 char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%lu\n", warmup_count);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t warmup_count_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long val;
	int err = kstrtoul(buf, 0, &val);
	if (err < 0)
		return err;
	if (val < 1)
		return -EINVAL;
	mutex_lock(&kb_lock);
	warmup_count = val;
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t loop_count_show(struct kobject *kobj, struct kobj_attribute *attr,
			       char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%lu\n", loop_count);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t loop_count_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;
	int err = kstrtoul(buf, 0, &val);
	if (err < 0)
		return err;
	if (val < 1)
		return -EINVAL;
	mutex_lock(&kb_lock);
	loop_count = val;
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t max_loop_count_show(struct kobject *kobj, struct kobj_attribute *attr,
			       char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%lu\n", max_loop_count);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t max_loop_count_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;
	int err = kstrtoul(buf, 0, &val);
	if (err < 0)
		return err;
	if (val < 1)
		return -EINVAL;
	mutex_lock(&kb_lock);
	max_loop_count = val;
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_oif_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	ssize_t res;
	struct net_device *dev;
	mutex_lock(&kb_lock);
	dev = dev_get_by_index(&init_net, flow_oif);
	if (!dev)
		res = scnprintf(buf, PAGE_SIZE, "%d\n", flow_oif);
	else
		res = scnprintf(buf, PAGE_SIZE, "%s\n", dev->name);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_oif_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int val;
	int err = kstrtoint(buf, 0, &val);
	if (err < 0) {
		struct net_device *dev;
		char ifname[IFNAMSIZ] = {0, };
		sscanf(buf, "%15s", ifname);
		dev = dev_get_by_name(&init_net, ifname);
		if (!dev)
			return -ENODEV;
		mutex_lock(&kb_lock);
		flow_oif = dev->ifindex;
		mutex_unlock(&kb_lock);
		return count;
	}
	if (val < 0)
		return -EINVAL;
	mutex_lock(&kb_lock);
	flow_oif = val;
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_iif_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	ssize_t res;
	struct net_device *dev;
	mutex_lock(&kb_lock);
	dev = dev_get_by_index(&init_net, flow_iif);
	if (!dev)
		res = scnprintf(buf, PAGE_SIZE, "%d\n", flow_iif);
	else
		res = scnprintf(buf, PAGE_SIZE, "%s\n", dev->name);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_iif_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int val;
	int err = kstrtoint(buf, 0, &val);
	if (err < 0) {
		struct net_device *dev;
		char ifname[IFNAMSIZ] = {0, };
		sscanf(buf, "%15s", ifname);
		dev = dev_get_by_name(&init_net, ifname);
		if (!dev)
			return -ENODEV;
		mutex_lock(&kb_lock);
		flow_iif = dev->ifindex;
		mutex_unlock(&kb_lock);
		return count;
	}
	if (val < 0)
		return -EINVAL;
	mutex_lock(&kb_lock);
	flow_iif = val;
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_label_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "0x%08x\n", (u32)flow_label);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_label_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	u32 val;
	int err = kstrtou32(buf, 0, &val);
	if (err < 0)
		return err;
	mutex_lock(&kb_lock);
	flow_label = val;
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_mark_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "0x%08x\n", flow_mark);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_mark_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	u32 val;
	int err = kstrtou32(buf, 0, &val);
	if (err < 0)
		return err;
	mutex_lock(&kb_lock);
	flow_mark = val;
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_dst_ipaddr_s_show(struct kobject *kobj, struct kobj_attribute *attr,
				      char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%pI6c\n", &flow_dst_ipaddr_s);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_dst_ipaddr_s_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	const char *end;
	struct in6_addr parsed;
	if (!in6_pton(buf, count, parsed.s6_addr, -1, &end) || (*end != '\0' && *end != '\n'))
		return -EINVAL;
	mutex_lock(&kb_lock);
	memcpy(&flow_dst_ipaddr_s, &parsed, sizeof(parsed));
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_dst_ipaddr_e_show(struct kobject *kobj, struct kobj_attribute *attr,
				      char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%pI6c\n", &flow_dst_ipaddr_e);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_dst_ipaddr_e_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	const char *end;
	struct in6_addr parsed;
	if (!in6_pton(buf, count, parsed.s6_addr, -1, &end) || (*end != '\0' && *end != '\n'))
		return -EINVAL;
	mutex_lock(&kb_lock);
	memcpy(&flow_dst_ipaddr_e, &parsed, sizeof(parsed));
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_src_ipaddr_show(struct kobject *kobj, struct kobj_attribute *attr,
				    char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%pI6c\n", &flow_src_ipaddr);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_src_ipaddr_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	const char *end;
	struct in6_addr parsed;
	if (!in6_pton(buf, count, parsed.s6_addr, -1, &end) || (*end != '\0' && *end != '\n'))
		return -EINVAL;
	mutex_lock(&kb_lock);
	memcpy(&flow_src_ipaddr, &parsed, sizeof(parsed));
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t run_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return do_bench(buf, 0);
}

static ssize_t run_verbose_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return do_bench(buf, 1);
}


static struct kobj_attribute	warmup_count_attr      = __ATTR_RW(warmup_count);
static struct kobj_attribute	loop_count_attr	       = __ATTR_RW(loop_count);
static struct kobj_attribute	max_loop_count_attr    = __ATTR_RW(max_loop_count);
static struct kobj_attribute	flow_oif_attr	       = __ATTR_RW(flow_oif);
static struct kobj_attribute	flow_iif_attr	       = __ATTR_RW(flow_iif);
static struct kobj_attribute	flow_label_attr	       = __ATTR_RW(flow_label);
static struct kobj_attribute	flow_mark_attr	       = __ATTR_RW(flow_mark);
static struct kobj_attribute	flow_dst_ipaddr_s_attr = __ATTR_RW(flow_dst_ipaddr_s);
static struct kobj_attribute	flow_dst_ipaddr_e_attr = __ATTR_RW(flow_dst_ipaddr_e);
static struct kobj_attribute	flow_src_ipaddr_attr   = __ATTR_RW(flow_src_ipaddr);
static struct kobj_attribute	run_attr	       = __ATTR_RO(run);
static struct kobj_attribute	run_verbose_attr       = __ATTR_RO(run_verbose);

static struct attribute *bench_attributes[] = {
	&warmup_count_attr.attr,
	&loop_count_attr.attr,
	&max_loop_count_attr.attr,
	&flow_oif_attr.attr,
	&flow_iif_attr.attr,
	&flow_label_attr.attr,
	&flow_mark_attr.attr,
	&flow_dst_ipaddr_s_attr.attr,
	&flow_dst_ipaddr_e_attr.attr,
	&flow_src_ipaddr_attr.attr,
	&run_attr.attr,
	&run_verbose_attr.attr,
	NULL
};

static struct attribute_group bench_attr_group = {
	.attrs = bench_attributes,
};

static struct kobject *bench_kobj;

int init_module(void)
{
	int rc;
	bench_kobj = kobject_create_and_add("kbench", kernel_kobj);
	if (!bench_kobj)
		return -ENOMEM;

	rc = sysfs_create_group(bench_kobj, &bench_attr_group);
	if (rc) {
		kobject_put(bench_kobj);
		return rc;
	}

	return 0;
}

void cleanup_module(void)
{
	sysfs_remove_group(bench_kobj, &bench_attr_group);
	kobject_put(bench_kobj);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Micro-benchmark for fib_lookup()");
