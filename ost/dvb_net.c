/* 
 * dvb_net.c
 *
 * Copyright (C) 2001 Convergence integrated media GmbH
 *                    Ralph Metzler <ralph@convergence.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 *   $Log: dvb_net.c,v $
 *   Revision 1.5  2001/07/03 19:34:23  gillem
 *   - sync with cvs (linuxtv)
 *
 *   Revision 1.4  2001/06/26 18:24:23  gillem
 *   - change dvb_net init
 *
 *   Revision 1.3  2001/06/25 20:31:48  gillem
 *   - fix return value
 *
 *   Revision 1.2  2001/06/25 20:23:39  gillem
 *   - bugfix (set pointer)
 *   - add debug output
 *   - add pointercheck
 *
 *
 */

#include <ost/demux.h>
#include "dvb_net.h"

/* external stuff in dvb.c */
int register_dvbnet(dvb_net_t *dvbnet);
int unregister_dvbnet(dvb_net_t *dvbnet);

/* internal stuff */
dvb_net_t dvb_net;

/*
 *	Determine the packet's protocol ID. The rule here is that we 
 *	assume 802.3 if the type field is short enough to be a length.
 *	This is normal practice and works for any 'now in use' protocol.
 *
 *  stolen from eth.c out of the linux kernel, hacked for dvb-device
 *  by Michael Holzt <kju@debian.org>
 */
 
unsigned short my_eth_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	struct ethhdr *eth;
	unsigned char *rawp;
	
	printk("dvb_net: (my_eth_type_trans) %x %x\n",skb,dev);

	skb->mac.raw=skb->data;
	skb_pull(skb,dev->hard_header_len);
	eth= skb->mac.ethernet;
	
	if(*eth->h_dest&1)
	{
		if(memcmp(eth->h_dest,dev->broadcast, ETH_ALEN)==0)
			skb->pkt_type=PACKET_BROADCAST;
		else
			skb->pkt_type=PACKET_MULTICAST;
	}
	
	if (ntohs(eth->h_proto) >= 1536)
		return eth->h_proto;
		
	rawp = skb->data;
	
	/*
	 *	This is a magic hack to spot IPX packets. Older Novell breaks
	 *	the protocol design and runs IPX over 802.3 without an 802.2 LLC
	 *	layer. We look for FFFF which isn't a used 802.2 SSAP/DSAP. This
	 *	won't work for fault tolerant netware but does for the rest.
	 */
	if (*(unsigned short *)rawp == 0xFFFF)
		return htons(ETH_P_802_3);
		
	/*
	 *	Real 802.2 LLC
	 */
	return htons(ETH_P_802_2);
}

static void 
dvb_net_sec(struct net_device *dev, u8 *pkt, int pkt_len)
{
        u8 *eth;
        struct sk_buff *skb;

		printk("dvb_net: (dvb_net_sec) %x %x %x\n",dev,pkt,pkt_len);

        if (!pkt_len)
                return;
        skb = dev_alloc_skb(pkt_len+2);
        if (skb == NULL) {
                printk(KERN_NOTICE "%s: Memory squeeze, dropping packet.\n",
                       dev->name);
                ((dvb_net_priv_t *)dev->priv)->stats.rx_dropped++;
                return;
        }
        eth=(u8 *) skb_put(skb,pkt_len+2);
        memcpy(eth+14, (void*)pkt+12, pkt_len-12);

        eth[0]=pkt[0x0b];
        eth[1]=pkt[0x0a];
        eth[2]=pkt[0x09];
        eth[3]=pkt[0x08];
        eth[4]=pkt[0x04];
        eth[5]=pkt[0x03];
        eth[6]=eth[7]=eth[8]=eth[9]=eth[10]=eth[11]=0;
        eth[12]=0x08; eth[13]=0x00;

	skb->protocol=my_eth_type_trans(skb,dev);
        skb->dev=dev;
        
        ((dvb_net_priv_t *)dev->priv)->stats.rx_packets++;
        ((dvb_net_priv_t *)dev->priv)->stats.rx_bytes+=skb->len;
        sti();
        netif_rx(skb);
}
 
static int 
dvb_net_callback(__u8 *buffer1, size_t buffer1_len,
		 __u8 *buffer2, size_t buffer2_len,
		 dmx_section_filter_t *filter,
		 dmx_success_t success)
{
	struct net_device *dev;

	if (!filter || !dev)
	{
		printk("dvb_net: warning filter null pointer\n");
		return -1;
	}

	dev = (struct net_device *) filter->priv;

	if(!dev)
	{
		printk("dvb_net: warning device null pointer\n");
		return -1;
	}

	printk("dvb_net: (dvb_net_callback) %s\n",dev->name);

	/* FIXME: this only works if exactly one complete section is
	          delivered in buffer1 only */
	dvb_net_sec(dev, buffer1, buffer1_len);
	return 0;
}

static int
dvb_net_tx(struct sk_buff *skb, struct net_device *dev)
{
	return 0;
}

static void
dvb_net_set_multi(struct net_device *dev)
{
	if (!dev)
	{
		printk("dvb_net: warning device null pointer\n");
		return;
	}

	printk("%s: set_multi\n", dev->name);

	if (dev->flags&IFF_PROMISC)
	{
		/* Enable promiscuous mode */
	}
	else if((dev->flags&IFF_ALLMULTI))
	{
		/* Disable promiscuous mode, use normal mode. */
          //hardware_set_filter(NULL);

	}
	else if(dev->mc_count)
	{
		int mci=0;

		struct dev_mc_list *mc;

		for (mc=dev->mc_list; (mc!=NULL) &&
			(mci<DVB_NET_MULTICAST_MAX);
			 mc=mc->next, mci++) {
                  //set_mc_filter(dev, );
		}
	}
	else
		;
}

static int
dvb_net_set_config(struct net_device *dev, struct ifmap *map)
{
	if ((!dev) || (!map))
	{
		printk("dvb_net: warning null pointer %x %x\n",dev,map);
		return -1;
	}

	printk("%s set_config\n",dev->name);

	if (netif_running(dev))
	{
		return -EBUSY;
	}

	//printk("dvb_net: PID=%04x\n", map->base_addr);
	return 0;
}

#define MASK 0xFF

static int
dvb_net_filter_set(struct net_device *dev, unsigned char *mac)
{
	int ret;
	dvb_net_priv_t *priv;
	dmx_demux_t *demux;

	if(!dev)
	{
		printk("dvb_net: warning null pointer %x %x\n",dev,mac);
		return -1;
	}

	priv  = (dvb_net_priv_t *)dev->priv;
	demux = priv->demux;

	printk("%s: filter_set\n", dev->name);

	priv->secfeed=0;
	priv->secfilter=0;

	if (!(mac[0]|mac[1]|mac[2]|mac[3]|mac[4]|mac[5]))
	        return 0;

	ret=demux->allocate_section_feed(demux, &priv->secfeed, 
					 dvb_net_callback);
	if (ret<0)
		return ret;
	ret=priv->secfeed->set(priv->secfeed, priv->pid, 32768, 0, 0);
	if (ret<0)
	        return ret;
	ret=priv->secfeed->allocate_filter(priv->secfeed, 
					   &priv->secfilter);
	if (ret<0)
	        return ret;
	priv->secfilter->priv=(void *) dev;

	memset(priv->secfilter->filter_value, 0, DMX_MAX_FILTER_SIZE);
	memset(priv->secfilter->filter_mask , 0, DMX_MAX_FILTER_SIZE);


	priv->secfilter->filter_value[0]=0x3e;
	priv->secfilter->filter_mask[0]=MASK;

	priv->secfilter->filter_value[3]=mac[5];
	priv->secfilter->filter_mask[3]=MASK;
	priv->secfilter->filter_value[4]=mac[4];
	priv->secfilter->filter_mask[4]=MASK;
	priv->secfilter->filter_value[8]=mac[3];
	priv->secfilter->filter_mask[8]=MASK;
	priv->secfilter->filter_value[9]=mac[2];
	priv->secfilter->filter_mask[9]=MASK;
	priv->secfilter->filter_value[10]=mac[1];
	priv->secfilter->filter_mask[10]=MASK;
	priv->secfilter->filter_value[11]=mac[0];
	priv->secfilter->filter_mask[11]=MASK;

	priv->secfeed->start_filtering(priv->secfeed);
	MOD_INC_USE_COUNT;
	return 0;
}

static void
dvb_net_filter_free(struct net_device *dev)
{
	dvb_net_priv_t *priv;

	if (!dev)
	{
		printk("dvb_net: warning device null pointer\n");
		return;
	}
	
	priv = (dvb_net_priv_t *)dev->priv;

	if (!priv)
	{
		printk("dvb_net: warning priv null pointer\n");
		return;
	}

	printk("%s: filter_free\n", dev->name);

	if (priv->secfeed) {
		printk("%s: filter_free %x\n", dev->name, priv);
		if (priv->secfeed->is_filtering)
			priv->secfeed->stop_filtering(priv->secfeed);
		if (priv->secfilter)
			priv->secfeed->
				release_filter(priv->secfeed,
				priv->secfilter);
		priv->demux->
			release_section_feed(priv->demux, priv->secfeed);
		priv->secfeed=0;
		MOD_DEC_USE_COUNT;
	}
}

static int
dvb_net_set_mac(struct net_device *dev, void *p)
{
	struct sockaddr *addr;
	unsigned char *mac;

	if ((!dev) || (!p))
	{
		printk("dvb_net: warning null pointer %x %x\n",dev,p);
		return -1;
	}

	addr = p;

	printk("%s: set_mac\n", dev->name);

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	mac=(unsigned char *) dev->dev_addr;
	if (netif_running(dev)) {
		dvb_net_filter_free(dev);
		if (dvb_net_filter_set(dev, mac))
			printk("dvb_net_filter_set failed\n");
	}

	return 0;
}


static int
dvb_net_open(struct net_device *dev)
{
	if (!dev)
	{
		printk("dvb_net: warning device null pointer\n");
		return -1;
	}

	if (dvb_net_filter_set(dev, dev->dev_addr))
		printk("dvb_net_filter_set failed\n");

	printk("dvb_net: open %x\n",dev);

	return 0;
}

static int
dvb_net_stop(struct net_device *dev)
{
	if (!dev)
	{
		printk("dvb_net: warning device null pointer\n");
		return -1;
	}

	printk("dvb_net: stop\n");
    dvb_net_filter_free(dev);

	return 0;
}

static struct net_device_stats *
dvb_net_get_stats(struct net_device *dev)
{
	if (!dev)
	{
		printk("dvb_net: warning device null pointer\n");
		return 0;
	}

	printk("dvb_net: get_stats\n");
	return &((dvb_net_priv_t *)dev->priv)->stats;
}


static int
dvb_net_init_dev(struct net_device *dev)
{
	if (!dev)
	{
		printk("dvb_net: warning device null pointer\n");
		return -1;
	}

	printk("dvb_net: dvb_net_init_dev\n");

	dev->open		= dvb_net_open;
	dev->stop		= dvb_net_stop;
	dev->hard_start_xmit	= dvb_net_tx;
	dev->hard_header	= 0;
	dev->get_stats		= dvb_net_get_stats;
	dev->set_multicast_list = dvb_net_set_multi;
	dev->set_config         = dvb_net_set_config;
	ether_setup(dev);
	dev->set_mac_address    = dvb_net_set_mac;
	dev->mtu		= 4096;
	
	return 0;
}

static int 
get_if(dvb_net_t *dvbnet)
{
	int i;

	if (!dvbnet)
	{
		printk("dvb_net: warning dvbnet null pointer\n");
		return -1;
	}

	printk("dvb_net: get_if\n");

	for (i=0; i<dvbnet->dev_num; i++)
		if (!dvbnet->state[i])
			break;
	if (i==dvbnet->dev_num)
		return -1;
	dvbnet->state[i]=1;
	return i;
}


int 
dvb_net_add_if(dvb_net_t *dvbnet, u16 pid)
{
	struct net_device *net;
	dmx_demux_t *demux;
	int result;
	int if_num;
 
	if (!dvbnet)
	{
		printk("dvb_net: warning dvbnet null pointer\n");
		return -1;
	}

	printk("dvb_net: net_add_if pid: %x\n",pid);

	if_num=get_if(dvbnet);
	if (if_num<0)
		return -EINVAL;

	net=&dvbnet->device[if_num];
	demux=dvbnet->demux;
	
	net->base_addr = 0;
	net->irq       = 0;
	net->dma       = 0;
	net->mem_start = 0;

	memcpy(net->name, "dvb0_0", 7);
	net->name[3]=dvbnet->card_num+0x30;
	net->name[5]=if_num+0x30;
	net->next      = NULL;
	net->init      = dvb_net_init_dev;
	net->priv      = kmalloc(sizeof(dvb_net_priv_t), GFP_KERNEL);

	if (net->priv == NULL)
			return -ENOMEM;

	memset(&((dvb_net_priv_t *)net->priv)->stats,
               0, sizeof(struct net_device_stats));

	((dvb_net_priv_t *)net->priv)->demux=demux;
	((dvb_net_priv_t *)net->priv)->pid=pid;
	((dvb_net_priv_t *)net->priv)->secfeed = 0;
	((dvb_net_priv_t *)net->priv)->secfilter = 0;

	net->base_addr=pid;
                
	if ((result = register_netdev(net)) < 0)
		return result;
	return if_num;
}

int 
dvb_net_remove_if(dvb_net_t *dvbnet, int num)
{
	if (!dvbnet)
	{
		printk("dvb_net: warning dvbnet null pointer\n");
		return -1;
	}

	printk("dvb_net: net_remove_if %x\n",num);

	if (!dvbnet->state[num])
		return -EINVAL;
	dvb_net_stop(&dvbnet->device[num]);
	kfree(dvbnet->device[num].priv);
	unregister_netdev(&dvbnet->device[num]);
	dvbnet->state[num]=0;
	return 0;
}

void
dvb_net_release(dvb_net_t *dvbnet)
{
	int i;

	if (!dvbnet)
	{
		printk("dvb_net: warning dvbnet null pointer\n");
		return;
	}

	printk("net_release\n");
	for (i=0; i<dvbnet->dev_num; i++) {
		if (!dvbnet->state[i])
			continue;
		dvb_net_remove_if(dvbnet, i);
	}
}

int
dvb_net_init(dvb_net_t *dvbnet, dmx_demux_t *demux)
{
	int i;

	if ((!dvbnet) || (!demux))
	{
		printk("dvb_net: warning null pointer %x %x\n",dvbnet,demux);
		return -1;
	}

	printk("dvb_net: net_init\n");

	dvbnet->demux=demux;
	dvbnet->dev_num=DVB_NET_DEVICES_MAX;

	for (i=0; i<dvbnet->dev_num; i++)
		dvbnet->state[i]=0;
	return 0;
}

/* ---------------------------------------------------------------------- */

#ifdef MODULE

MODULE_DESCRIPTION("DVB-NET driver");

int
init_module (void)
{
	printk("DVB-NET: $Id: dvb_net.c,v 1.5 2001/07/03 19:34:23 gillem Exp $\n");

	dvb_net.dvb_net_release   = dvb_net_release;
	dvb_net.dvb_net_init      = dvb_net_init;
	dvb_net.dvb_net_add_if    = dvb_net_add_if;
	dvb_net.dvb_net_remove_if = dvb_net_remove_if;

	return register_dvbnet(&dvb_net);
}

void
cleanup_module(void)
{
	unregister_dvbnet(&dvb_net);
}

#endif
