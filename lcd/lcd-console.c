/*
    lcd-console.c - console driver for lcd display (dbox-II-project)

    Copyright (C) 2000 Gillem htoa@gmx.net

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Version: 0.03

    Last update: 15.12.2000
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/smp_lock.h>
#include <linux/delay.h>

#include "lcd-console.h"
#include "lcd-font.h"
#include "lcd-ks0713.h"

#define MAX_COL 15
#define MAX_ROW 8

#define INITSTRING "@lcd:\x0a"

int row,col;

///////////////////////////////////////////////////////////////////////////////

void lcd_init_console(void)
{
	lcd_init_font(0);
	lcd_console_clear();

	lcd_console_put_data(INITSTRING,strlen(INITSTRING));
}

///////////////////////////////////////////////////////////////////////////////

void lcd_console_clear(void)
{
	static unsigned char d[LCD_BUFFER_SIZE];

	row = 0;
	col = 0;

	memset(d,0xFF,LCD_BUFFER_SIZE);
	lcd_write_dram(d);
}

///////////////////////////////////////////////////////////////////////////////

void lcd_console_put_data( unsigned char *data, int len )
{
	int i;

	for(i=0;i<len;i++)
	{
//		printk("%02X\n",data[i]);

		switch(data[i])
		{
			case 0x0A:	lcd_console_new_line();
									continue;
									break;
			default:		break;
		}

		if (col == MAX_COL)
		{
			col=0;
			row++;
		}

		if ( row == MAX_ROW )
		{
			lcd_console_new_line();
			row--;
		}

		lcd_console_put_char( data[i] );
		col++;
	}
}

///////////////////////////////////////////////////////////////////////////////

void lcd_console_put_char( unsigned char data )
{
	int i;
	unsigned char b[8];

	// load font to b
	lcd_convert_to_font( b, &data, 1 );

	lcd_set_pos( row, col*8 );

	for(i=0;i<8;i++)
	{
		lcd_write_byte( b[i] );
	}
}

///////////////////////////////////////////////////////////////////////////////

void lcd_console_new_line()
{
//	printk("NL: %02X %02X\n",row,col);

	for(;col<=MAX_COL;col++)
	{
		lcd_console_put_char(0x20);
	}

	if ( row == MAX_ROW )
	{
		row--;
		lcd_console_scroll_down( 1 );

		for(col=0;col<=MAX_COL;col++)
		{
			lcd_console_put_char(0x20);
		}

	}

	row++;
	col=0;
}

///////////////////////////////////////////////////////////////////////////////

void lcd_console_scroll_down( int i )
{
	static unsigned char d[LCD_BUFFER_SIZE+LCD_COLS];

	lcd_read_dram(d);
	memset( d+LCD_BUFFER_SIZE, 0x00, LCD_COLS );
	lcd_write_dram(d+LCD_COLS);
}

///////////////////////////////////////////////////////////////////////////////
