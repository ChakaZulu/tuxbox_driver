/*
 *   enx_accel.c - AViA eNX accelerator driver (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2002 Florian Schirmer (jolt@tuxbox.org)
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
 *   $Log: avia_gt_accel.c,v $
 *   Revision 1.2  2002/06/07 17:53:45  Jolt
 *   GCC31 fixes 2nd shot - sponsored by Frankster (THX!)
 *
 *   Revision 1.1  2002/04/01 22:29:11  Jolt
 *   HW accelerated functions for eNX
 *
 *
 *
 *   $Revision: 1.2 $
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include <dbox/enx.h>

#define ENX_MAX_TRANSCATION_SIZE 64

unsigned int enx_crc32(void *buffer, unsigned int buffer_size)
{

    printk("enx_accel: crc32 (buffer=0x%X, buffer_size=%d)\n", (unsigned int)buffer, buffer_size);

    //enx_reg_s(CPCCRCSRC2)->CRC.CRC = 0;
    enx_reg_32(CPCCRCSRC2) = 0xFFFFFFFF;
    enx_reg_set(CPCSRC1, Addr, (unsigned int)buffer);
    
    /*enx_reg_set(CPCCMD, W, 0);
    enx_reg_set(CPCCMD, C, 1);
    enx_reg_set(CPCCMD, P, 0);
    enx_reg_set(CPCCMD, N, 0);
    enx_reg_set(CPCCMD, T, 0);
    enx_reg_set(CPCCMD, D, 0);*/

    while (buffer_size) {
    
	if (buffer_size > ENX_MAX_TRANSCATION_SIZE) {
	
	    //enx_reg_set(CPCCMD, Len, ENX_MAX_TRANSCATION_SIZE);
	    enx_reg_16(CPCCMD) = (1 << 14) | (ENX_MAX_TRANSCATION_SIZE - 1);
	    buffer_size -= ENX_MAX_TRANSCATION_SIZE;
	    
	} else {
	
	    //enx_reg_set(CPCCMD, Len, buffer_size);
	    enx_reg_16(CPCCMD) = (1 << 14) | (buffer_size - 1);
	    buffer_size = 0;
	    
	}
	
	//enx_reg_set(CPCCMD, Cmd, 0);
    
    }
    
    //return enx_reg_s(CPCCRCSRC2)->CRC.CRC;
    return enx_reg_32(CPCCRCSRC2);
}

static int enx_accel_init(void)
{

    printk("enx_accel: $Id: avia_gt_accel.c,v 1.2 2002/06/07 17:53:45 Jolt Exp $\n");
    
    enx_reg_set(RSTR0, COPY, 0);

    return 0;
    
}

static void __exit enx_accel_cleanup(void)
{

    printk("enx_accel: cleanup\n");

    enx_reg_set(RSTR0, COPY, 1);

}

#ifdef MODULE
module_init(enx_accel_init);
module_exit(enx_accel_cleanup);
#endif
