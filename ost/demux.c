/*
 * demux.c
 *
 * Copyright (C) 2000 Ralph  Metzler <ralph@convergence.de>
 *		  & Marcus Metzler <marcus@convergence.de>
		      for convergence integrated media GmbH
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
 */

#include <ost/demux.h>

#include <linux/module.h>
#include <linux/string.h>

#if LINUX_VERSION_CODE < 0x020300
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)
#endif

LIST_HEAD(dmx_muxs);

int dmx_register_demux(dmx_demux_t *demux)
{
	struct list_head *pos, *head=&dmx_muxs;

	if (!(demux->id && demux->vendor && demux->model))
		return -EINVAL;
	list_for_each(pos, head)
	{
		if (!strcmp(DMX_DIR_ENTRY(pos)->id, demux->id))
			return -EEXIST;
	}

	demux->users=0;
	list_add(&(demux->reg_list), head);
	MOD_INC_USE_COUNT;
	return 0;
}


int dmx_unregister_demux(dmx_demux_t* demux)
{
	struct list_head *pos, *head=&dmx_muxs;

	list_for_each(pos, head)
	{
		if (DMX_DIR_ENTRY(pos)==demux)
		{
			if (demux->users!=0)
				return -EINVAL;
			list_del(pos);
			MOD_DEC_USE_COUNT;
			return 0;
		}
	}
	return -ENODEV;
}


struct list_head *dmx_get_demuxes(void)
{
	if (list_empty(&dmx_muxs))
		return NULL;

	return &dmx_muxs;
}

#ifdef MODULE
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
EXPORT_SYMBOL(dmx_register_demux);
EXPORT_SYMBOL(dmx_unregister_demux);
EXPORT_SYMBOL(dmx_get_demuxes);
#endif
