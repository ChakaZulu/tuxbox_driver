/*
** I2C interface
** =============
** (C) 2000 by Paolo Scaffardi (arsenio@tin.it)
** AIRVENT SAM s.p.a - RIMINI(ITALY)
**
*/

#ifndef _I2C_H_
#define _I2C_H_

void i2c_init(int speed);
void i2c_send( unsigned char address,
              unsigned char secondary_address,
              int enable_secondary,
              unsigned short size, unsigned char dataout[] );
void i2c_receive(unsigned char address,
		unsigned char secondary_address,
		int enable_secondary,
                unsigned short size_to_expect, unsigned char datain[] );

#endif
