/*
 * $Id: dvb_i2c_bridge.h,v 1.1 2003/01/04 11:05:31 Jolt Exp $
 *
 * DVB I2C bridge
 *
 * Copyright (C) 2002 Florian Schirmer <schirmer@taytron.net>
 * Copyright (C) 2002 Felix Domke <tmbinc@elitedvb.net>
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

#ifndef DVB_I2C_BRIDGE_H
#define DVB_I2C_BRIDGE_H

int dvb_i2c_bridge_register(struct dvb_adapter *dvb_adap);
void dvb_i2c_bridge_unregister(struct dvb_adapter *dvb_adap);

#endif
