/*
 * tuxbox_hardware.h - TuxBox hardware info
 *
 * Copyright (C) 2003 Florian Schirmer <jolt@tuxbox.org>
 *                    Bastian Blank <waldi@tuxbox.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id: tuxbox_hardware.h,v 1.1 2003/02/19 16:39:00 waldi Exp $
 */

#ifndef TUXBOX_HARDWARE_H
#define TUXBOX_HARDWARE_H

#include <tuxbox/tuxbox_info.h>

#ifdef __KERNEL__
#define TUXBOX_VERSION				KERNEL_VERSION(2,0,2)
#endif

extern tuxbox_capabilities_t tuxbox_capabilities;
extern tuxbox_model_t tuxbox_model;
extern tuxbox_submodel_t tuxbox_submodel;
extern tuxbox_vendor_t tuxbox_vendor;

int tuxbox_hardware_read (void);

#endif
