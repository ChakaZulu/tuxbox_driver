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
 *   Revision 1.2  2001/05/26 13:46:59  gillem
 *   - add stv6411 data struct
 *
 *   Revision 1.1  2001/05/26 09:19:50  gillem
 *   - initial release
 *
 *
 *   $Revision: 1.2 $
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
 * stv6412 data struct (stv6411)
 *
 */

typedef struct s_stv6412_data {
 /* Data 0 */
 unsigned char res1				: 3;
 unsigned char tv_gain			: 1;
 unsigned char tv_mono			: 1;
 unsigned char tv_aoc			: 3;
 /* Data 1 */
 unsigned char res2				: 3;
 unsigned char cinch_gain		: 1;
 unsigned char res3				: 1;
 unsigned char cinch_aoc		: 3;
 /* Data 2 */
 unsigned char res4				: 1;
 unsigned char vcr_mono			: 1;
 unsigned char res5				: 3;
 unsigned char vcr_aoc			: 3;
 /* Data 3 */
 unsigned char res6				: 1;
 unsigned char tv_chroma_mute	: 1;
 unsigned char tv_chroma_oc		: 3;
 unsigned char tv_rf_oc			: 2;
 unsigned char tv_rc_oc			: 1;
 /* Data 4 */
 unsigned char tv_rgb_oc		: 2;
 unsigned char tv_fb_oc			: 2;
 unsigned char rgb_gain			: 2;
 unsigned char res7				: 1;
 unsigned char rcs_encode_clamp	: 1;
 /* Data 5 */
 unsigned char res8				: 4;
 unsigned char vcr_chroma_mute	: 1;
 unsigned char chroma_oc		: 3;
 /* Data 6 */
 unsigned char res9				: 1;
 unsigned char res10			: 1;
 unsigned char res11			: 2;
 unsigned char vcr_sb			: 2;
 unsigned char tv_sb			: 2;
 /* Data 7 */
 unsigned char vcr_off			: 1;
 unsigned char res12			: 1;	/* set to 1 !!! */
 unsigned char tv_off			: 1;
 unsigned char encode_clamp_off	: 1;
 unsigned char tv_clamp_off		: 1;
 unsigned char astb_clamp_off	: 1;
 unsigned char vcr_clamp_off	: 1;
 unsigned char rgb_clamp_off	: 1;
 /* Data 8 */
 unsigned char res13			: 6;
 unsigned char rfm_off			: 1;
 unsigned char cinch_off		: 1;
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

	stv6412_data.res12 = 1;

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */
