/* -*- mode: c; c-file-style: "linux" -*- */
/* Run a micro benchmark on fib_lookup().
 *
 * It creates a /sys/kernel/kbench directory. By default, a scan is
 * done from 0.0.0.0 to 223.255.255.255 (considered as a linear
 * space).
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

#include <net/route.h>
#include <net/ip_fib.h>

#include <linux/timex.h>

#define DEFAULT_WARMUP_COUNT	100000
#define DEFAULT_LOOP_COUNT	5000
#define DEFAULT_BATCH_COUNT	1
#define DEFAULT_OIF		0
#define DEFAULT_IIF		0
#define DEFAULT_MARK		0x00000000
#define DEFAULT_TOS		0x00
#define DEFAULT_DST_IPADDR_S	0x00000000
#define DEFAULT_DST_IPADDR_E	0xdfffffff
#define DEFAULT_SRC_IPADDR	0x00000000

#define HIST_BUCKETS		15
#define HIST_WIDTH		50

static unsigned long	warmup_count	= DEFAULT_WARMUP_COUNT;
static unsigned long	loop_count	= DEFAULT_LOOP_COUNT;
static unsigned long	batch_count	= DEFAULT_BATCH_COUNT;
static int		flow_oif	= DEFAULT_OIF;
static int		flow_iif	= DEFAULT_IIF;
static u8		flow_tos	= DEFAULT_TOS;
static u32		flow_mark	= DEFAULT_MARK;
static u32		flow_dst_ipaddr_s = DEFAULT_DST_IPADDR_S;
static u32		flow_dst_ipaddr_e = DEFAULT_DST_IPADDR_E;
static u32		flow_src_ipaddr = DEFAULT_SRC_IPADDR;

static DEFINE_MUTEX(kb_lock);

/* Compatibility with older kernel versions */
#ifndef __ATTR_RW
# define __ATTR_RW(_name) __ATTR(_name,				\
				(S_IWUSR | S_IRUGO),		\
				_name##_show, _name##_store)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
# define my_fib_lookup(f, r) fib_lookup(&init_net, f, r)
#else
# define my_fib_lookup(f, r) fib_lookup(&init_net, f, r, 0)
#endif

/* We require flowi4 from 2.6.39 but also the kstrto* functions. We
 * assume the later have been backported. They are commit
 * 33ee3b2e2eb9. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
# define flowi4 flowi
#endif

/* Benchmark */

static int compare_ull(const void *lhs, const void *rhs) {
    unsigned long long lhs_integer = *(const unsigned long long *)(lhs);
    unsigned long long rhs_integer = *(const unsigned long long *)(rhs);

    return (lhs_integer < rhs_integer) ? -1 : (lhs_integer > rhs_integer);
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
	sort(dmedian, count, sizeof(unsigned long long), compare_ull, NULL);
	res = percentile(50, dmedian, count);
	kfree(dmedian);
	return res;
}

static unsigned long long average(unsigned long long *results,
				  unsigned count)
{
	unsigned long i;
	unsigned long long avg = 0, remainder = 0, remainder1;
	for (i = 0; i < count; i++) {
		avg += results[i] / count;
		remainder1 = results[i] % count;
		if (remainder >= count - remainder1) {
			avg++;
			remainder -= count - remainder1;
		} else {
			remainder += remainder1;
		}
	}
	return avg;		/* + remainder/N but that's 0 */
}

static void display_statistics(char *buf,
			       unsigned long long *results,
			       unsigned long total,
			       unsigned long batch,
			       int verbose)
{
	sort(results, total, sizeof(*results), compare_ull, NULL);
	if (total == 0) {
		scnprintf(buf, PAGE_SIZE, "msg=\"no match\"\n");
	} else {
		unsigned long i, j, count, count2;
		unsigned long long p95 = percentile(95, results, total);
		unsigned long long p90 = percentile(90, results, total);
		unsigned long long p50 = percentile(50, results, total);
		unsigned long long avg = average(results, total);
		scnprintf(buf, PAGE_SIZE,
			  "min=%llu max=%llu count=%lu batch=%lu "
			  "average=%llu 95th=%llu 90th=%llu 50th=%llu mad=%llu\n",
			  results[0],
			  results[total - 1],
			  total*batch,
			  batch,
			  avg,
			  p95,
			  p90,
			  p50,
			  mad(results, p50, total));
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
						      count*batch);
				count = 0;
				start += share;
				if (i >= total) break;
				if (results[i] > p95) break;
				if (hist_buf >= buf + PAGE_SIZE - HIST_WIDTH - 20) break;
			}
		}
	}
}

static int do_bench(char *buf, int verbose)
{
	unsigned long long *results = NULL;
	unsigned long long t1, t2;
	struct fib_result res;
	int err;
	unsigned long i, j, total, count, batch, delta = 0;
	u32 start;
	bool scan;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
	struct flowi fl4;
	struct flowi *batched_fl4 = NULL;
#else
	struct flowi4 fl4;
	struct flowi4 *batched_fl4 = NULL;
#endif


	mutex_lock(&kb_lock);
	total = warmup_count;
	count = loop_count;
	batch = batch_count;
	if (batch > 1)
		batched_fl4 = kmalloc(sizeof(*batched_fl4) * batch, GFP_KERNEL);
	start = flow_dst_ipaddr_s;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
	memset(&fl4, 0, sizeof(fl4));
	fl4.oif = flow_oif;
	fl4.iif = flow_iif;
	fl4.mark = flow_mark;
	fl4.fl4_tos = flow_tos;
	fl4.fl4_dst = flow_dst_ipaddr_s;
	fl4.fl4_src = flow_src_ipaddr;
#define flowi4_oif oif
#else
	memset(&fl4, 0, sizeof(fl4));
	fl4.flowi4_oif = flow_oif;
	fl4.flowi4_iif = flow_iif;
	fl4.flowi4_mark = flow_mark;
	fl4.flowi4_tos = flow_tos;
	fl4.daddr = flow_dst_ipaddr_s;
	fl4.saddr = flow_src_ipaddr;
#endif
	if (ntohl(flow_dst_ipaddr_s) < ntohl(flow_dst_ipaddr_e)) {
		scan = true;
		delta = ntohl(flow_dst_ipaddr_e) - ntohl(flow_dst_ipaddr_s);
	}
	results = kmalloc(sizeof(*results) * (count / batch + 1), GFP_KERNEL);
	mutex_unlock(&kb_lock);
	if (!results || (batch > 0 && !batched_fl4)) {
		kfree(batched_fl4);
		kfree(results);
		return scnprintf(buf, PAGE_SIZE, "msg=\"no memory\"\n");
	}

	/* Warmup */
	for (i = total; i > 0; --i) {
		err = my_fib_lookup(&fl4, &res);
		if (err && err != -ENETUNREACH && err != -ESRCH) {
			kfree(results);
			kfree(batched_fl4);
			return scnprintf(buf, PAGE_SIZE, "err=%d msg=\"lookup error\"\n", err);
		}
	}

	for (i = j = total = 0; i < count; i++) {
		if (scan) {
			__be32 daddr;
			if (delta < count)
				daddr = htonl(ntohl(start) + (i % delta));
			else
				daddr = htonl(ntohl(start) + delta / count * i);
			if (ipv4_is_loopback(daddr))
				continue;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
			fl4.fl4_dst = daddr;
#else
			fl4.daddr = daddr;
#endif
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
		/* There is a small overhead added to the measured
		 * function. It's easy to test, just remove the call
		 * to the function. On my test machine, I get an
		 * overhead of 28 instructions (which is less than 10
		 * ns). */
		/* There is also a risk for instructions to be
		 * reordered. On amd64, get_cycles() just use rtdsc()
		 * which just wraps RTDSC. Intel recommends using
		 * RTDSCP. On Linux, you also have rtdsc_ordered()
		 * which puts a fence. However, those methods increase
		 * overhead. */
		if (batch <= 1) {
			t1 = get_cycles();
			err = my_fib_lookup(&fl4, &res);
			t2 = get_cycles();
			if (err == -ENETUNREACH)
				continue;
			results[total] = t2 - t1;
			total++;
		} else {
			/* Ideally, we would like to test the result
			 * before putting it in the batch. We can't do
			 * that, this would warm the cache. */
			memcpy(&batched_fl4[j], &fl4, sizeof(fl4));
			if (++j != batch)
				continue;
			t1 = get_cycles();
			for (j = 0; j < batch; j++)
				/* Store the result in the flow directly to increase locality. */
				batched_fl4[j].flowi4_oif = my_fib_lookup(&batched_fl4[j], &res);
			t2 = get_cycles();
			for (err = 0, j = 0; j < batch; j++) {
				if (batched_fl4[j].flowi4_oif == -ENETUNREACH) {
					err = -ENETUNREACH;
					break;
				}
			}
			if (err == -ENETUNREACH) {
				j = 0;
				continue;
			}
			results[total] = (t2 - t1)/batch;
			total++;
			j = 0;
		}
	}

	/* Compute and display statistics */
	display_statistics(buf, results, total, batch, verbose);
	kfree(results);
	kfree(batched_fl4);
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

static ssize_t batch_count_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%lu\n", batch_count);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t batch_count_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned long val;
	int err = kstrtoul(buf, 0, &val);
	if (err < 0)
		return err;
	if (val < 1)
		return -EINVAL;
	mutex_lock(&kb_lock);
	batch_count = val;
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

static ssize_t flow_tos_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "0x%02x\n", (u32)flow_tos);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_tos_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	unsigned int val;
	int err = kstrtouint(buf, 0, &val);
	if (err < 0)
		return err;
	if (val < 0 || val > 255)
		return -EINVAL;
	mutex_lock(&kb_lock);
	flow_tos = val;
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
	res = scnprintf(buf, PAGE_SIZE, "%pI4\n", &flow_dst_ipaddr_s);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_dst_ipaddr_s_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	mutex_lock(&kb_lock);
	flow_dst_ipaddr_s = in_aton(buf);
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_dst_ipaddr_e_show(struct kobject *kobj, struct kobj_attribute *attr,
				      char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%pI4\n", &flow_dst_ipaddr_e);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_dst_ipaddr_e_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	mutex_lock(&kb_lock);
	flow_dst_ipaddr_e = in_aton(buf);
	mutex_unlock(&kb_lock);
	return count;
}

static ssize_t flow_src_ipaddr_show(struct kobject *kobj, struct kobj_attribute *attr,
				    char *buf)
{
	ssize_t res;
	mutex_lock(&kb_lock);
	res = scnprintf(buf, PAGE_SIZE, "%pI4\n", &flow_src_ipaddr);
	mutex_unlock(&kb_lock);
	return res;
}

static ssize_t flow_src_ipaddr_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	mutex_lock(&kb_lock);
	flow_src_ipaddr = in_aton(buf);
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
static struct kobj_attribute	batch_count_attr       = __ATTR_RW(batch_count);
static struct kobj_attribute	flow_oif_attr	       = __ATTR_RW(flow_oif);
static struct kobj_attribute	flow_iif_attr	       = __ATTR_RW(flow_iif);
static struct kobj_attribute	flow_tos_attr	       = __ATTR_RW(flow_tos);
static struct kobj_attribute	flow_mark_attr	       = __ATTR_RW(flow_mark);
static struct kobj_attribute	flow_dst_ipaddr_s_attr = __ATTR_RW(flow_dst_ipaddr_s);
static struct kobj_attribute	flow_dst_ipaddr_e_attr = __ATTR_RW(flow_dst_ipaddr_e);
static struct kobj_attribute	flow_src_ipaddr_attr   = __ATTR_RW(flow_src_ipaddr);
static struct kobj_attribute	run_attr	       = __ATTR_RO(run);
static struct kobj_attribute	run_verbose_attr       = __ATTR_RO(run_verbose);

static struct attribute *bench_attributes[] = {
	&warmup_count_attr.attr,
	&loop_count_attr.attr,
	&batch_count_attr.attr,
	&flow_oif_attr.attr,
	&flow_iif_attr.attr,
	&flow_tos_attr.attr,
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
