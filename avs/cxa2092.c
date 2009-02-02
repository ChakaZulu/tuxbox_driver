/*
 *   cxa2092.c - audio/video switch driver (dbox-II-project)
 *
 *   Homepage: http://www.tuxbox.org
 *
 *   Copyright (C) 2000-2002 Gillem gillem@berlios.de
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/types.h>

#include <linux/i2c.h>

#include "dbox/avs_core.h"
#include "cxa2092.h"

/* ---------------------------------------------------------------------- */

/*
 * cxa2092 data struct
 * thanks to sony for great support
 *
 */

typedef struct s_cxa2092_data {
 /* Data 1 */
 unsigned char evc 	: 3;
 unsigned char evf 	: 3;
 unsigned char tvmute1 	: 1;
 unsigned char zcd 	: 1;
 /* Data 2 */
 unsigned char fblk 	: 2;
 unsigned char vsw1 	: 3;
 unsigned char asw1 	: 3;
 /* Data 3 */
 unsigned char fnc 	: 2;
 unsigned char vsw2 	: 3;
 unsigned char asw2 	: 3;
 /* Data 4 */
 unsigned char ycm 	: 1;
 unsigned char res1 	: 1;
 unsigned char vsw3 	: 3;
 unsigned char asw3 	: 3;
 /* Data 5 */
 unsigned char tvmute2 	: 1;
 unsigned char res2 	: 3;
 unsigned char log1 	: 1;
 unsigned char log2 	: 1;
 unsigned char log3 	: 1;
 unsigned char log4 	: 1;
} s_cxa2092_data;

#define CXA2092_DATA_SIZE sizeof(s_cxa2092_data)

/* ---------------------------------------------------------------------- */

#define dprintk     if (debug) printk

static struct s_cxa2092_data cxa2092_data;

/* ---------------------------------------------------------------------- */

int cxa2092_set(struct i2c_client *client)
{
	if ( CXA2092_DATA_SIZE != i2c_master_send(client, (char*)&cxa2092_data, CXA2092_DATA_SIZE))
	{
		return -EFAULT;
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

int cxa2092_set_volume( struct i2c_client *client, int vol )
{
	if ((vol > 63) || (vol < 0))
		return -EINVAL;
	
	cxa2092_data.evc = (vol >> 3) & 7;
	cxa2092_data.evf = vol & 7;

	return cxa2092_set(client);
}

inline int cxa2092_set_mute( struct i2c_client *client, int type )
{
	if ((type<0) || (type>1))
	{
		return -EINVAL;
	}

	cxa2092_data.tvmute1 = cxa2092_data.tvmute2 = type&1;

	return cxa2092_set(client);
}

inline int cxa2092_set_zcd( struct i2c_client *client, int type )
{
	if ((type<0) || (type>1))
	{
		return -EINVAL;
	}

	cxa2092_data.zcd = type&1;

	return cxa2092_set(client);
}

inline int cxa2092_set_fblk( struct i2c_client *client, int type )
{
	if (type<0 || type>3)
	{
		return -EINVAL;
	}

	cxa2092_data.fblk = type;

	return cxa2092_set(client);
}

inline int cxa2092_set_fnc( struct i2c_client *client, int type )
{
	if (type<0 || type>3)
	{
		return -EINVAL;
	}

	cxa2092_data.fnc = type;

	return cxa2092_set(client);
}

inline int cxa2092_set_vsw( struct i2c_client *client, int sw, int type )
{
	if (type<0 || type>7)
	{
		return -EINVAL;
	}

	switch(sw)
	{
		case 0:
			cxa2092_data.vsw1 = type;
			break;
		case 1:
			cxa2092_data.vsw2 = type;
			break;
		case 2:
			cxa2092_data.vsw3 = type;
			break;
		default:
			return -EINVAL;
	}

	return cxa2092_set(client);
}

inline int cxa2092_set_asw( struct i2c_client *client, int sw, int type )
{
	if (type<0 || type>7)
	{
		return -EINVAL;
	}

	switch(sw)
	{
		case 0:
			cxa2092_data.asw1 = type;
			break;
		case 1:
			cxa2092_data.asw2 = type;
			break;
		case 2:
			cxa2092_data.asw3 = type;
			break;
		default:
			return -EINVAL;
	}

	return cxa2092_set(client);
}

int cxa2092_get_volume(void)
{
	return ((cxa2092_data.evc*8)+cxa2092_data.evf);
}

inline int cxa2092_get_mute(void)
{
	return cxa2092_data.tvmute1;
}

inline int cxa2092_get_zcd(void)
{
	return cxa2092_data.zcd;
}

inline int cxa2092_get_fblk(void)
{
	return cxa2092_data.fblk;
}

inline int cxa2092_get_fnc(void)
{
	return cxa2092_data.fnc;
}

inline int cxa2092_get_vsw( int sw )
{
	switch(sw)
	{
		case 0:
			return cxa2092_data.vsw1;
			break;
		case 1:
			return cxa2092_data.vsw2;
			break;
		case 2:
			return cxa2092_data.vsw3;
			break;
		default:
			return -EINVAL;
	}

	return -EINVAL;
}

inline int cxa2092_get_asw( int sw )
{
	switch(sw)
	{
		case 0:
			return cxa2092_data.asw1;
			break;
		case 1:
			return cxa2092_data.asw2;
			break;
		case 2:
			return cxa2092_data.asw3;
			break;
		default:
			return -EINVAL;
	}

	return -EINVAL;
}

/* ---------------------------------------------------------------------- */

inline int cxa2092_set_ycm( struct i2c_client *client, int type )
{
	if (type<0 || type>1)
	{
		return -EINVAL;
	}

	cxa2092_data.ycm = type;

	return cxa2092_set(client);
}

inline int cxa2092_set_logic( struct i2c_client *client, int sw, int type )
{
	if(type<0 || type>1)
	{
		return -EINVAL;
	}

	switch(sw)
	{
		case 0:
			cxa2092_data.log1 = type;
			break;
		case 1:
			cxa2092_data.log2 = type;
			break;
		case 2:
			cxa2092_data.log3 = type;
			break;
		case 3:
			cxa2092_data.log4 = type;
			break;
		default:
			return -EINVAL;
	}

	return cxa2092_set(client);
}

inline int cxa2092_get_ycm(void)
{
	return cxa2092_data.ycm;
}

inline int cxa2092_get_logic( int sw )
{
	switch(sw)
	{
		case 0:
			return cxa2092_data.log1;
			break;
		case 1:
			return cxa2092_data.log2;
			break;
		case 2:
			return cxa2092_data.log3;
			break;
		case 3:
			return cxa2092_data.log4;
			break;
		default:
			return -EINVAL;
	}

	return -EINVAL;
}

/* ---------------------------------------------------------------------- */

int cxa2092_get_status(struct i2c_client *client)
{
	unsigned char byte[1];
	int i;
	byte[0] = 0;

	i = i2c_master_recv(client,byte,1);

	if (1 != i)
	{
		printk("[AVS] i2c error %d\n",i);
		return -1;
	}

	return byte[0];
}

/* ---------------------------------------------------------------------- */

int cxa2092_command(struct i2c_client *client, unsigned int cmd, void *arg )
{
	int val;

	dprintk("[AVS]: command %x\n",cmd);
	
	if (cmd&AVSIOSET)
	{
		if ( copy_from_user(&val,arg,sizeof(val)) )
		{
			return -EFAULT;
        	}

		switch (cmd)
		{
			/* set video */
			case AVSIOSVSW1:
				return cxa2092_set_vsw(client,0,val);
			case AVSIOSVSW2:
				return cxa2092_set_vsw(client,1,val);
			case AVSIOSVSW3:
				return cxa2092_set_vsw(client,2,val);
			/* set audio */
			case AVSIOSASW1:
				return cxa2092_set_asw(client,0,val);
			case AVSIOSASW2:
				return cxa2092_set_asw(client,1,val);
			case AVSIOSASW3:
				return cxa2092_set_asw(client,2,val);
			/* set vol & mute */
			case AVSIOSVOL:
				return cxa2092_set_volume(client,val);
			case AVSIOSMUTE:
				return cxa2092_set_mute(client,val);
			/* set video fast blanking */
			case AVSIOSFBLK:
				return cxa2092_set_fblk(client,val);
			/* set video function switch control */
			case AVSIOSFNC:	
			case AVSIOSSCARTPIN8:
				return cxa2092_set_fnc(client,val);
			/* set output throgh vout 8 */
			case AVSIOSYCM:
				return cxa2092_set_ycm(client,val);
			/* set zero cross detector */
			case AVSIOSZCD:
				return cxa2092_set_zcd(client,val);
			/* set logic outputs */
			case AVSIOSLOG1:
				return cxa2092_set_logic(client,1,val);
			case AVSIOSLOG2:
				return cxa2092_set_logic(client,2,val);
			case AVSIOSLOG3:
				return cxa2092_set_logic(client,3,val);
			case AVSIOSLOG4:
				return cxa2092_set_logic(client,4,val);
			default:
				return -EINVAL;
		}
	}
	else
	{
		switch (cmd)
		{
			/* get video */
			case AVSIOGVSW1:
                                val = cxa2092_get_vsw(0);
                                break;
			case AVSIOGVSW2:
                                val = cxa2092_get_vsw(1);
                                break;
			case AVSIOGVSW3:
                                val = cxa2092_get_vsw(2);
                                break;
			/* get audio */
			case AVSIOGASW1:
                                val = cxa2092_get_asw(0);
                                break;
			case AVSIOGASW2:
                                val = cxa2092_get_asw(1);
                                break;
			case AVSIOGASW3:
                                val = cxa2092_get_asw(2);
                                break;
			/* get vol & mute */
			case AVSIOGVOL:
                                val = cxa2092_get_volume();
                                break;
			case AVSIOGMUTE:
                                val = cxa2092_get_mute();
                                break;
			/* get video fast blanking */
			case AVSIOGFBLK:
                                val = cxa2092_get_fblk();
                                break;
			/* get video function switch control */
			case AVSIOGFNC:
			case AVSIOGSCARTPIN8:
                                val = cxa2092_get_fnc();
                                break;
			/* get output throgh vout 8 */
			case AVSIOGYCM:
                                val = cxa2092_get_ycm();
                                break;
			/* get zero cross detector */
			case AVSIOGZCD:
                                val = cxa2092_get_zcd();
                                break;
			/* get logic outputs */
			case AVSIOGLOG1:
                                val = cxa2092_get_logic(1);
                                break;
			case AVSIOGLOG2:
                                val = cxa2092_get_logic(2);
                                break;
			case AVSIOGLOG3:
                                val = cxa2092_get_logic(3);
                                break;
			case AVSIOGLOG4:
                                val = cxa2092_get_logic(4);
                                break;
			case AVSIOGSTATUS:
                                // TODO: error handling
                                val = cxa2092_get_status(client);
                                break;
			default:
                                return -EINVAL;
		}

		return put_user(val,(int*)arg);
	}

	return 0;
}

/* ---------------------------------------------------------------------- */

int cxa2092_init(struct i2c_client *client)
{
	memset((void*)&cxa2092_data,0,CXA2092_DATA_SIZE);

	/* default values */
	cxa2092_data.asw1 = 0; // start with audio outputs disabled
	cxa2092_data.vsw1 = 5;

	cxa2092_data.vsw2 = 1;
	cxa2092_data.asw2 = 0; // start with audio outputs disabled

	cxa2092_data.fnc  = 2;
	cxa2092_data.asw3 = 0; // start with audio outputs disabled

	return cxa2092_set(client);
}

/* ---------------------------------------------------------------------- */
