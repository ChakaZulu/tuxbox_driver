/*
 *   lcd-ks0713.h - lcd driver for KS0713 and compatible (dbox-II-project)
 *
 *   Homepage: http://dbox2.elxsi.de
 *
 *   Copyright (C) 2000-2001 Gillem (htoa@gmx.net)
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
 *   $Log: lcd-ks0713.h,v $
 *   Revision 1.5  2001/01/06 10:06:35  gillem
 *   cvs check
 *
 *   $Revision: 1.5 $
 *
 */

#define LCD_STAT_BUSY			0x80
#define LCD_STAT_ADC			0x40
#define LCD_STAT_ON				0x20
#define LCD_STAT_RESETB		0x10

#define LCD_POWERC_VC			0x04
#define LCD_POWERC_VR			0x02
#define LCD_POWERC_VF			0x01

#define	LCD_MODE_ASC			0
#define	LCD_MODE_BIN			1

#define LCD_IOCTL_STATUS			1
#define LCD_IOCTL_ON					2
#define	LCD_IOCTL_EON					3
#define LCD_IOCTL_REVERSE			4
#define	LCD_IOCTL_BIAS				5
#define	LCD_IOCTL_ADC					6
#define	LCD_IOCTL_SHL					7
#define	LCD_IOCTL_RESET				8
#define	LCD_IOCTL_IDL					9
#define	LCD_IOCTL_SRV					10
#define	LCD_IOCTL_SMR					11
#define	LCD_IOCTL_RMR					12
#define	LCD_IOCTL_POWERC			13
#define LCD_IOCTL_SEL_RES			14
#define LCD_IOCTL_SIR					15

#define LCD_IOCTL_SPAGE				17
#define LCD_IOCTL_SCOLUMN			18
#define LCD_IOCTL_SET_ADDR		19
#define LCD_IOCTL_READ_BYTE		20
#define LCD_IOCTL_WRITE_BYTE	21

#define LCD_IOCTL_ASC_MODE		22

#define LCD_ROWS							8
#define LCD_COLS							120
#define LCD_BUFFER_SIZE				( LCD_ROWS * LCD_COLS )

static void lcd_set_pos( int row, int col );
static void lcd_write_byte( int data );
static void lcd_read_dram( unsigned char * dest );
static void lcd_write_dram( unsigned char * dest );
