/*
 *   stv6412.c - audio/video switch driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem htoa@gmx.net
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *   $Log: stv6412.c,v $
 *   Revision 1.1  2001/05/26 09:19:50  gillem
 *   - initial release
 *
 *
 *   $Revision: 1.1 $
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/malloc.h>
#include <linux/poll.h>
#include <linux/types.h>

#include <linux/i2c.h>

#include "dbox/avs_core.h"
#include "stv6412.h"

/* ---------------------------------------------------------------------- */

/*
 * cxa2092 data struct
 * thanks to sony for great support
 *
 */

typedef struct s_stv6412_data {
} s_stv6412_data;

#define STV6412_DATA_SIZE sizeof(s_stv6412_data)

/* ---------------------------------------------------------------------- */

#define dprintk     if (debug) printk

static struct s_stv6412_data stv6412_data;

/* ---------------------------------------------------------------------- */

int stv6412_set(struct i2c_client *client)
{
	if ( STV6412_DATA_SIZE != i2c_master_send(client, (char*)&stv6412_data, STV6412_DATA_SIZE))
	{
		return -EFAULT;
    }

	return 0;
}

/* ---------------------------------------------------------------------- */

int stv6412_command(struct i2c_client *client, unsigned int cmd, void *arg )
{
	int val=0;
	dprintk("[AVS]: command\n");
	
	if (cmd&AVSIOSET)
	{
		if ( get_user(val,(int*)arg) )
		{
			return -EFAULT;
        }

    	switch (cmd) {
			default:
				return -EINVAL;
		}
	} else
	{
		switch (cmd)
		{
			default:
				return -EINVAL;
		}

		return put_user(val,(int*)arg);
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

int stv6412_init(struct i2c_client *client)
{
	memset((void*)&stv6412_data,0,STV6412_DATA_SIZE);

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */
