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
 *   Revision 1.3  2001/07/03 19:24:46  gillem
 *   - some changes ... bugreport !
 *
 *   Revision 1.2  2001/05/26 13:46:59  gillem
 *   - add stv6411 data struct
 *
 *   Revision 1.1  2001/05/26 09:19:50  gillem
 *   - initial release
 *
 *
 *   $Revision: 1.3 $
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
 * stv6412 data struct
 *
 */

typedef struct s_stv6412_data {
 /* Data 0 */
 unsigned char t_stereo			: 1;
 unsigned char t_vol_x			: 1;
 unsigned char t_vol_c			: 5;
 unsigned char svm				: 1;
 /* Data 1 */
 unsigned char v_stereo			: 1;
 unsigned char v_asc			: 2;
 unsigned char c_ag				: 1;
 unsigned char tc_asc			: 3;
 /* Data 2 */
 unsigned char v_cm				: 1;
 unsigned char v_vsc			: 3;
 unsigned char t_cm				: 1;
 unsigned char t_vsc			: 3;
 /* Data 3 */
 unsigned char rgb_tri			: 1;
 unsigned char rgb_gain			: 3;
 unsigned char rgb_sc			: 2;
 unsigned char fblk				: 2;
 /* Data 4 */
 unsigned char it_enable		: 1;
 unsigned char slb				: 1;
 unsigned char res2				: 1;
 unsigned char v_coc			: 1;
 unsigned char r_tfc			: 1;
 unsigned char r_ac				: 1;
 unsigned char t_rcos			: 1;
 /* Data 5 */
 unsigned char v_sb				: 2;
 unsigned char t_sb				: 2;
 unsigned char e_aig			: 2;
 unsigned char v_rcsc			: 1;
 unsigned char e_rcsc			: 1;
 /* Data 6 */
 unsigned char r_out			: 1;
 unsigned char t_out			: 1;
 unsigned char c_out			: 1;
 unsigned char v_out			: 1;
 unsigned char a_in				: 1;
 unsigned char t_in				: 1;
 unsigned char v_in				: 1;
 unsigned char e_in				: 1;
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

int stv6412_set_volume( struct i2c_client *client, int vol )
{
	int c=0;

	c = vol;

	if ( c <= 62 )
	{
		if ( c > 0 )
		{
			c /= 2;
		}
	} else
	{
		return -EINVAL;
	}

    stv6412_data.t_vol_c = c;

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */

inline int stv6412_set_mute( struct i2c_client *client, int type )
{
	if ((type<0) || (type>1))
	{
		return -EINVAL;
	}

	stv6412_data.c_ag = type&1;

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */

inline int stv6412_set_vsw( struct i2c_client *client, int sw, int type )
{
	if (type<0 || type>4)
	{
		return -EINVAL;
    }

    switch(sw)
	{
        case 0:
            stv6412_data.v_vsc = type;
            break;
        case 2:
            stv6412_data.t_vsc = type;
            break;
        default:
    		return -EINVAL;
    }

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */

inline int stv6412_set_asw( struct i2c_client *client, int sw, int type )
{
    switch(sw)
	{
        case 0:
			if (type<0 || type>3)
			{
				return -EINVAL;
		    }

            stv6412_data.v_asc = type;
            break;
        case 1:
        case 2:
			if (type<0 || type>4)
			{
				return -EINVAL;
		    }

            stv6412_data.tc_asc = type;
            break;
        default:
    		return -EINVAL;
    }

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */

inline int stv6412_set_fblk( struct i2c_client *client, int type )
{
	if (type<0 || type>3)
	{
		return -EINVAL;
    }

    stv6412_data.fblk = type;

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */

static int stv6412_getstatus(struct i2c_client *client)
{
	unsigned char byte;

	byte = 0;

	if (1 != i2c_master_recv(client,&byte,1))
	{
		return -1;
    }

	return byte;
}

/* ---------------------------------------------------------------------- */

int stv6412_get_volume(void)
{
	int c;

	c = stv6412_data.t_vol_c;

	if (c)
	{
		c *= 2;
	}

    return c;
}

/* ---------------------------------------------------------------------- */

inline int stv6412_get_mute(void)
{
    return stv6412_data.c_ag;
}

/* ---------------------------------------------------------------------- */

inline int stv6412_get_fblk(void)
{
    return stv6412_data.fblk;
}

/* ---------------------------------------------------------------------- */

inline int stv6412_get_vsw( int sw )
{
    switch(sw)
	{
        case 0:
            return stv6412_data.v_vsc;
            break;
        case 2:
			return stv6412_data.t_vsc;
            break;
        default:
            return -EINVAL;
    }

    return -EINVAL;
}

/* ---------------------------------------------------------------------- */

inline int stv6412_get_asw( int sw )
{
    switch(sw)
	{
        case 0:
            return stv6412_data.v_asc;
        case 1:
        case 2:
			return stv6412_data.tc_asc;
            break;
        default:
    		return -EINVAL;
    }

    return -EINVAL;
}

/* ---------------------------------------------------------------------- */

int stv6412_command(struct i2c_client *client, unsigned int cmd, void *arg )
{
	int val=0;
	dprintk("[AVS]: command\n");
	
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
						return stv6412_set_vsw(client,0,val);
			case AVSIOSVSW2:
						return stv6412_set_vsw(client,1,val);
			case AVSIOSVSW3:
						return stv6412_set_vsw(client,2,val);
			/* set audio */
			case AVSIOSASW1:
						return stv6412_set_asw(client,0,val);
			case AVSIOSASW2:
						return stv6412_set_asw(client,1,val);
			case AVSIOSASW3:
						return stv6412_set_asw(client,2,val);
			/* set vol & mute */
			case AVSIOSVOL:
						return stv6412_set_volume(client,val);
			case AVSIOSMUTE:
						return stv6412_set_mute(client,val);
			/* set video fast blanking */
			case AVSIOSFBLK:
						return stv6412_set_fblk(client,val);

			default:
						return -EINVAL;
		}
	} else
	{
		switch (cmd)
		{
			/* get video */
			case AVSIOGVSW1:
                                val = stv6412_get_vsw(0);
                                break;
			case AVSIOGVSW2:
                                val = stv6412_get_vsw(1);
                                break;
			case AVSIOGVSW3:
                                val = stv6412_get_vsw(2);
                                break;
			/* get audio */
			case AVSIOGASW1:
                                val = stv6412_get_asw(0);
                                break;
			case AVSIOGASW2:
                                val = stv6412_get_asw(1);
                                break;
			case AVSIOGASW3:
                                val = stv6412_get_asw(2);
                                break;
			/* get vol & mute */
			case AVSIOGVOL:
                                val = stv6412_get_volume();
                                break;
			case AVSIOGMUTE:
                                val = stv6412_get_mute();
                                break;
			/* get video fast blanking */
			case AVSIOGFBLK:
                                val = stv6412_get_fblk();
                                break;
			/* get status */
            case AVSIOGSTATUS:
                                // TODO: error handling
                                val = stv6412_getstatus(client);
                                break;
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

	stv6412_data.tc_asc = 1;
	stv6412_data.c_ag   = 1;
	stv6412_data.v_asc  = 1;

	stv6412_data.t_vsc  = 1;
	stv6412_data.t_cm   = 1;
	stv6412_data.v_vsc  = 1;
	stv6412_data.v_cm   = 1;

	stv6412_data.fblk     = 1;
	stv6412_data.rgb_sc   = 1;
	stv6412_data.rgb_gain = 2;
	stv6412_data.rgb_tri  = 1;

	stv6412_data.v_coc = 1;

	stv6412_data.e_aig = 1;
	stv6412_data.t_sb  = 3;

	stv6412_data.a_in  = 1;
	stv6412_data.r_out = 1;

	return stv6412_set(client);
}

/* ---------------------------------------------------------------------- */
