/*
 * $Id: dbox2_fp_core.h,v 1.1 2002/10/21 11:38:59 obi Exp $
 *
 * Copyright (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef __KERNEL__
#ifndef __dbox2_fp_core_h__
#define __dbox2_fp_core_h__

#include <linux/i2c.h>
#include <linux/types.h>
#include <dbox/info.h>

struct fp_data
{
	int fpID;
	int fpVCR;
	struct i2c_client * client;
};

/* internal kernel api */
int fp_cmd (struct i2c_client * client, u8 cmd, u8 * res, u32 size);
int fp_sendcmd (struct i2c_client * client, u8 b0, u8 b1);
struct dbox_info_struct * fp_get_info (void);
struct i2c_client * fp_get_i2c (void);

#endif /* __dbox2_fp_core_h__ */
#endif /* __KERNEL__ */

