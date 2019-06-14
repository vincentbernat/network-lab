/*
 * livepatch-teamd-ab.c - Kernel Live Patching to make teamd active/backup accept traffic from inactive slaves
 *
 * Copyright (C) 2014 Seth Jennings <sjenning@redhat.com>
 * Copyright (C) 2019 Vincent Bernat
 * Copyright (C) 2011 Jiri Pirko <jpirko@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/livepatch.h>

#include <linux/if_team.h>
static rx_handler_result_t livepatch_team_handle_frame(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct team_port *port;
	struct team *team;
	struct team_pcpu_stats *pcpu_stats;

	skb = skb_share_check(skb, GFP_ATOMIC);
	if (!skb)
		return RX_HANDLER_CONSUMED;

	*pskb = skb;

	port = rcu_dereference(skb->dev->rx_handler_data);
	team = port->team;

	/* Don't check if the port is enabled. */
	pcpu_stats = this_cpu_ptr(team->pcpu_stats);
	u64_stats_update_begin(&pcpu_stats->syncp);
	pcpu_stats->rx_packets++;
	pcpu_stats->rx_bytes += skb->len;
	if (skb->pkt_type == PACKET_MULTICAST)
		pcpu_stats->rx_multicast++;
	u64_stats_update_end(&pcpu_stats->syncp);

	skb->dev = team->dev;
	return RX_HANDLER_ANOTHER;
}


static struct klp_func funcs[] = {
	{
		.old_name = "team_handle_frame",
		.new_func = livepatch_team_handle_frame,
	}, { }
};

static struct klp_object objs[] = {
	{
		/* This needs to be added if NET_TEAM_MODE_ACTIVEBACKUP=m */
		.name = "team_mode_activebackup",
		.funcs = funcs,
	}, { }
};

static struct klp_patch patch = {
	.mod = THIS_MODULE,
	.objs = objs,
};

static int livepatch_init(void)
{
	int ret;

	ret = klp_register_patch(&patch);
	if (ret)
		return ret;
	ret = klp_enable_patch(&patch);
	if (ret) {
		WARN_ON(klp_unregister_patch(&patch));
		return ret;
	}
	return 0;
}

static void livepatch_exit(void)
{
	WARN_ON(klp_unregister_patch(&patch));
}

module_init(livepatch_init);
module_exit(livepatch_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");
