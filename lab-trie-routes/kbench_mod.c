/* Run a micro benchmark on fib_lookup(). This is heavily derived from
 * kbench_mod.c from
 * https://git.kernel.org/pub/scm/linux/kernel/git/davem/net_test_tools.git/. */

#define pr_fmt(fmt) "kbench: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/inet.h>
#include <linux/sort.h>

#include <net/route.h>
#include <net/ip_fib.h>

#include <linux/timex.h>

#define DEFAULT_WARMUP_COUNT 100000
#define DEFAULT_COUNT 100

#define DEFAULT_DST_IP_ADDR	0x4a800001
#define DEFAULT_SRC_IP_ADDR	0x00000000
#define DEFAULT_OIF		0
#define DEFAULT_IIF		0
#define DEFAULT_MARK		0x00000000
#define DEFAULT_TOS		0x00

static int flow_oif = DEFAULT_OIF;
static int flow_iif = DEFAULT_IIF;
static u32 flow_mark = DEFAULT_MARK;
static u32 flow_dst_ip_addr = DEFAULT_DST_IP_ADDR;
static u32 flow_src_ip_addr = DEFAULT_SRC_IP_ADDR;
static int flow_tos = DEFAULT_TOS;

static char dst_string[64];
static char src_string[64];

module_param_string(dst, dst_string, sizeof(dst_string), 0);
MODULE_PARM_DESC(dst, "Destination IP address");
module_param_string(src, src_string, sizeof(src_string), 0);
MODULE_PARM_DESC(src, "Source IP address");

static void __init flow_setup(void)
{
	if (dst_string[0])
		flow_dst_ip_addr = in_aton(dst_string);
	if (src_string[0])
		flow_src_ip_addr = in_aton(src_string);
}

module_param_named(oif, flow_oif, int, 0);
MODULE_PARM_DESC(oif, "Output interface index");
module_param_named(iif, flow_iif, int, 0);
MODULE_PARM_DESC(iif, "Input interface index");
module_param_named(mark, flow_mark, uint, 0);
MODULE_PARM_DESC(mark, "Firewall mark");
module_param_named(tos, flow_tos, int, 0);
MODULE_PARM_DESC(tos, "TOS field");

static int warmup_count = DEFAULT_WARMUP_COUNT;
module_param_named(warmup, warmup_count, int, 0);
MODULE_PARM_DESC(warmup, "Warmup iterations before benchmark");

static int count = DEFAULT_COUNT;
module_param_named(count, count, int, 0);
MODULE_PARM_DESC(count, "Benchmark iterations");


static void flow_init(struct flowi4 *fl4)
{
	memset(fl4, 0, sizeof(*fl4));
	fl4->flowi4_oif = flow_oif;
	fl4->flowi4_iif = flow_iif;
	fl4->flowi4_mark = flow_mark;
	fl4->flowi4_tos = flow_tos;
	fl4->daddr = flow_dst_ip_addr;
	fl4->saddr = flow_src_ip_addr;
}

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

static void do_bench(void)
{
	unsigned long long *results;
	unsigned long long t1, t2, average;
	struct fib_result res;
	struct flowi4 fl4;
	int err, i;

	if (count < 1) count = 1;
	results = kmalloc(sizeof(*results) * count, GFP_KERNEL);
	if (!results)
		return;

	flow_init(&fl4);

	for (i = 0; i < warmup_count; i++) {
		err = fib_lookup(&init_net, &fl4, &res, 0);
		if (err) {
			pr_info("fib_lookup: err=%d\n", err);
			return;
		}
	}

	average = 0;
	for (i = 0; i < count; i++) {
		t1 = get_cycles();
		err = fib_lookup(&init_net, &fl4, &res, 0);
		t2 = get_cycles();
		results[i] = t2 - t1;
		average += results[i];
	}

	/* Compute percentiles */
	sort(results, count, sizeof(*results), compare, NULL);
	pr_info("fib_lookup: min=%llu max=%llu average=%llu 50th=%llu 90th=%llu 95th=%llu\n",
		results[0],
		results[count - 1],
		average/count,
		percentile(50, results, count),
		percentile(90, results, count),
		percentile(95, results, count));
}

static int __init kbench_init(void)
{
	flow_setup();

	pr_info("flow [IIF(%d),OIF(%d),MARK(0x%08x),D(%pI4),S(%pI4),TOS(0x%02x)]\n",
		flow_iif, flow_oif, flow_mark,
		&flow_dst_ip_addr,
		&flow_src_ip_addr, flow_tos);

#if defined(CONFIG_X86)
	if (!boot_cpu_has(X86_FEATURE_TSC)) {
		pr_err("X86 TSC is required, but is unavailable.\n");
		return -EINVAL;
	}
#endif

	do_bench();

	return -ENODEV;
}

static void __exit kbench_exit(void)
{
}

module_init(kbench_init);
module_exit(kbench_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Micro-benchmark for fib_lookup()");
