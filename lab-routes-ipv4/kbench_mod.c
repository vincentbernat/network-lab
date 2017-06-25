/* -*- mode: c; c-file-style: "linux" -*- */
/* Run a micro benchmark on fib_lookup().
 *
 * It creates a /sys/kernel/kbench directory. By default, a scan is
 * done from 0.0.0.0 to 223.255.255.255 (considered as a linear
 * space).
 *
 * The module doesn't perform any kind of locking. It is not safe to
 * modify a setting while running the benchmark. Moreover, it only
 * acts on the initial network namespace.
 */

#define pr_fmt(fmt) "kbench: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/inet.h>
#include <linux/sort.h>
#include <linux/netdevice.h>

#include <net/route.h>
#include <net/ip_fib.h>

#include <linux/timex.h>

#define DEFAULT_WARMUP_COUNT	100000
#define DEFAULT_LOOP_COUNT	5000
#define DEFAULT_OIF		0
#define DEFAULT_IIF		0
#define DEFAULT_MARK		0x00000000
#define DEFAULT_TOS		0x00
#define DEFAULT_DST_IPADDR_S	0x00000000
#define DEFAULT_DST_IPADDR_E	0xdfffffff
#define DEFAULT_SRC_IPADDR	0x00000000

static unsigned long	warmup_count	= DEFAULT_WARMUP_COUNT;
static unsigned long	loop_count	= DEFAULT_LOOP_COUNT;
static int		flow_oif	= DEFAULT_OIF;
static int		flow_iif	= DEFAULT_IIF;
static u8		flow_tos	= DEFAULT_TOS;
static u32		flow_mark	= DEFAULT_MARK;
static u32		flow_dst_ipaddr_s = DEFAULT_DST_IPADDR_S;
static u32		flow_dst_ipaddr_e = DEFAULT_DST_IPADDR_E;
static u32		flow_src_ipaddr = DEFAULT_SRC_IPADDR;
static bool		filter_unreach	= true;

/* Benchmark */

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

static int do_bench(char *buf)
{
	unsigned long long *results;
	unsigned long long t1, t2, average;
	struct fib_result res;
	struct flowi4 fl4;
	int err, l;
	unsigned long i, total, delta = 0;
	bool scan;

	results = kmalloc(sizeof(*results) * loop_count, GFP_KERNEL);
	if (!results)
		return scnprintf(buf, PAGE_SIZE, "msg=\"no memory\"\n");

	memset(&fl4, 0, sizeof(fl4));
	fl4.flowi4_oif = flow_oif;
	fl4.flowi4_iif = flow_iif;
	fl4.flowi4_tos = flow_tos;
	fl4.flowi4_mark = flow_mark;
	fl4.daddr = flow_dst_ipaddr_s;
	fl4.saddr = flow_src_ipaddr;
	if (ntohl(flow_dst_ipaddr_s) < ntohl(flow_dst_ipaddr_e)) {
		scan = true;
		delta = ntohl(flow_dst_ipaddr_e) - ntohl(flow_dst_ipaddr_s);
	}

	for (i = 0; i < warmup_count; i++) {
		err = fib_lookup(&init_net, &fl4, &res, 0);
		if (err && err != -ENETUNREACH) {
			kfree(results);
			return scnprintf(buf, PAGE_SIZE, "err=%d msg=\"lookup error\"\n", err);
		}
	}

	average = 0;
	for (i = total = 0; i < loop_count; i++) {
		if (scan) {
			if (delta < loop_count)
				fl4.daddr = htonl(ntohl(flow_dst_ipaddr_s) + (i % delta));
			else
				fl4.daddr = htonl(ntohl(flow_dst_ipaddr_s) + delta / loop_count * i);
			if (ipv4_is_loopback(fl4.daddr))
				continue;
		}
		t1 = get_cycles();
		err = fib_lookup(&init_net, &fl4, &res, 0);
		t2 = get_cycles();
		if (err == -ENETUNREACH)
			continue;
		results[total] = t2 - t1;
		average += results[total];
		total++;
	}

	/* Compute percentiles */
	sort(results, total, sizeof(*results), compare, NULL);
	if (total == 0) {
		l = scnprintf(buf, PAGE_SIZE, "msg=\"no match\"\n");
	} else {
		l = scnprintf(buf, PAGE_SIZE,
			      "min=%llu max=%llu count=%lu average=%llu 50th=%llu 90th=%llu 95th=%llu\n",
			      results[0],
			      results[total - 1],
			      total,
			      average/total,
			      percentile(50, results, total),
			      percentile(90, results, total),
			      percentile(95, results, total));
	}
	kfree(results);
	return l;
}

/* Sysfs attributes */

static ssize_t warmup_count_show(struct kobject *kobj, struct kobj_attribute *attr,
				 char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", warmup_count);
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
	warmup_count = val;
	return count;
}

static ssize_t loop_count_show(struct kobject *kobj, struct kobj_attribute *attr,
			       char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", loop_count);
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
	loop_count = val;
	return count;
}

static ssize_t flow_oif_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	struct net_device *dev = dev_get_by_index(&init_net, flow_oif);
	if (!dev)
		return scnprintf(buf, PAGE_SIZE, "%d\n", flow_oif);
	return scnprintf(buf, PAGE_SIZE, "%s\n", dev->name);
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
		flow_oif = dev->ifindex;
		return count;
	}
	if (val < 0)
		return -EINVAL;
	flow_oif = val;
	return count;
}

static ssize_t flow_iif_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	struct net_device *dev = dev_get_by_index(&init_net, flow_iif);
	if (!dev)
		return scnprintf(buf, PAGE_SIZE, "%d\n", flow_iif);
	return scnprintf(buf, PAGE_SIZE, "%s\n", dev->name);
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
		flow_iif = dev->ifindex;
		return count;
	}
	if (val < 0)
		return -EINVAL;
	flow_iif = val;
	return count;
}

static ssize_t flow_tos_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%02x\n", (u32)flow_tos);
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
	flow_tos = val;
	return count;
}

static ssize_t flow_mark_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "0x%08x\n", flow_mark);
}

static ssize_t flow_mark_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	u32 val;
	int err = kstrtou32(buf, 0, &val);
	if (err < 0)
		return err;
	flow_mark = val;
	return count;
}

static ssize_t flow_dst_ipaddr_s_show(struct kobject *kobj, struct kobj_attribute *attr,
				      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%pI4\n", &flow_dst_ipaddr_s);
}

static ssize_t flow_dst_ipaddr_s_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	flow_dst_ipaddr_s = in_aton(buf);
	return count;
}

static ssize_t flow_dst_ipaddr_e_show(struct kobject *kobj, struct kobj_attribute *attr,
				      char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%pI4\n", &flow_dst_ipaddr_e);
}

static ssize_t flow_dst_ipaddr_e_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	flow_dst_ipaddr_e = in_aton(buf);
	return count;
}

static ssize_t flow_src_ipaddr_show(struct kobject *kobj, struct kobj_attribute *attr,
				    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%pI4\n", &flow_src_ipaddr);
}

static ssize_t flow_src_ipaddr_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	flow_src_ipaddr = in_aton(buf);
	return count;
}

static ssize_t filter_unreach_show(struct kobject *kobj, struct kobj_attribute *attr,
				   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", (int)filter_unreach);
}

static ssize_t filter_unreach_store(struct kobject *kobj, struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int val;
	int err = kstrtouint(buf, 0, &val);
	if (err < 0)
		return err;
	if (val < 0 || val > 1)
		return -EINVAL;
	filter_unreach = val;
	return count;
}

static ssize_t run_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return do_bench(buf);
}


static struct kobj_attribute	warmup_count_attr      = __ATTR_RW(warmup_count);
static struct kobj_attribute	loop_count_attr	       = __ATTR_RW(loop_count);
static struct kobj_attribute	flow_oif_attr	       = __ATTR_RW(flow_oif);
static struct kobj_attribute	flow_iif_attr	       = __ATTR_RW(flow_iif);
static struct kobj_attribute	flow_tos_attr	       = __ATTR_RW(flow_tos);
static struct kobj_attribute	flow_mark_attr	       = __ATTR_RW(flow_mark);
static struct kobj_attribute	flow_dst_ipaddr_s_attr = __ATTR_RW(flow_dst_ipaddr_s);
static struct kobj_attribute	flow_dst_ipaddr_e_attr = __ATTR_RW(flow_dst_ipaddr_e);
static struct kobj_attribute	flow_src_ipaddr_attr   = __ATTR_RW(flow_src_ipaddr);
static struct kobj_attribute	filter_unreach_attr    = __ATTR_RW(filter_unreach);
static struct kobj_attribute	run_attr	       = __ATTR_RO(run);

static struct attribute *bench_attributes[] = {
	&warmup_count_attr.attr,
	&loop_count_attr.attr,
	&flow_oif_attr.attr,
	&flow_iif_attr.attr,
	&flow_tos_attr.attr,
	&flow_mark_attr.attr,
	&flow_dst_ipaddr_s_attr.attr,
	&flow_dst_ipaddr_e_attr.attr,
	&flow_src_ipaddr_attr.attr,
	&filter_unreach_attr.attr,
	&run_attr.attr,
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
