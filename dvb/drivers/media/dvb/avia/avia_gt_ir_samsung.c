/*
 * Treiber für Samsung Infrarot-Tastatur SEM-CIR am AVIA GTX/ENX
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/input.h>

#include "avia_gt.h"

#define MAX_PULSES 48

#define MAX_RECV 16

#define BREAK_KEY_TIMEOUT 13

#define dprintk(...)
//#define dprintk(fmt,args...) printk(fmt,## args)

/*
 * Keyboard-Mapping
 */

const unsigned char keyboard_mapping[128] =
{
/* 00-07 */		KEY_RESERVED, KEY_RESERVED, KEY_B, KEY_TAB, KEY_RIGHT, KEY_SPACE, KEY_APOSTROPHE, KEY_RESERVED,
/* 08-0F */		KEY_MINUS, KEY_LEFTBRACE, KEY_RESERVED, KEY_ENTER, KEY_INSERT, KEY_LEFT, KEY_RESERVED, KEY_RESERVED,
/* 10-17 */		KEY_RESERVED, KEY_RESERVED, KEY_A, KEY_S, KEY_D, KEY_F, KEY_J, KEY_RESERVED,
/* 18-1F */		KEY_K, KEY_L, KEY_VOLUMEUP, KEY_SEMICOLON, KEY_PAGEDOWN, KEY_END, KEY_SLEEP, KEY_RESERVED,
/* 20-27 */		KEY_RESERVED, KEY_RESERVED, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_U, KEY_RESERVED,
/* 28-2F */		KEY_I, KEY_O, KEY_RESERVED, KEY_P, KEY_RIGHTBRACE, KEY_WWW, KEY_RESERVED, KEY_RESERVED,
/* 30-37 */		KEY_RESERVED, KEY_RESERVED, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_M, KEY_RESERVED,
/* 38-3F */		KEY_COMMA, KEY_DOT, KEY_RESERVED, KEY_SLASH, KEY_RESERVED, KEY_N, KEY_RESERVED, KEY_RESERVED,
/* 40-47 */		KEY_RESERVED, KEY_DELETE, KEY_RECORD, KEY_HOMEPAGE, KEY_PLAYPAUSE, KEY_5, KEY_6, KEY_RESERVED,
/* 48-4F */		KEY_STOPCD, KEY_REWIND, KEY_RESERVED, KEY_EQUAL, KEY_BACKSPACE, KEY_HOME, KEY_RESERVED, KEY_RESERVED,
/* 50-57 */		KEY_RESERVED, KEY_RESERVED, KEY_HELP, KEY_ESC, KEY_PLAYCD, KEY_G, KEY_H, KEY_RESERVED,
/* 58-5F */		KEY_PREVIOUSSONG, KEY_FORWARD, KEY_RESERVED, KEY_BACKSLASH, KEY_UP, KEY_PAGEUP, KEY_RESERVED, KEY_RESERVED,
/* 60-67 */		KEY_RESERVED, KEY_RESERVED, KEY_1, KEY_2, KEY_3, KEY_4, KEY_7, KEY_RESERVED,
/* 68-6F */		KEY_8, KEY_9, KEY_RESERVED, KEY_0, KEY_DOWN, KEY_WAKEUP, KEY_RESERVED, KEY_RESERVED,
/* 70-77 */		KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_CAPSLOCK, KEY_VOLUMEDOWN, KEY_T, KEY_Y, KEY_RESERVED,
/* 78-7F */		KEY_CYCLEWINDOWS, KEY_NEXTSONG, KEY_RESERVED, KEY_MUTE, KEY_RESERVED, KEY_RESERVED, KEY_RESERVED, KEY_102ND
};

const unsigned short __init extra_keyboard[] =
{
	KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_LEFTALT, KEY_RIGHTALT, KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_F1, KEY_F2,
	KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, BTN_LEFT, BTN_RIGHT
};

/*
 * Tastatur-Statusbits:
 * Im allerersten Byte sind offenbar auch noch ein paar Flags versteckt:
 * 0x01 Kleiner/Größer
 * 0x02 Off
 * 0x04 On
 *
 * Definitionen für das 2. Byte:
 */

#define BREAK		0x80
#define FN			0x40
#define LEFTSHIFT	0x20
#define RIGHTSHIFT	0x10
#define LEFTALT		0x08
#define RIGHTALT	0x04
#define LEFTCTRL	0x02
#define RIGHTCTRL	0x01

/*
 * Maus-Button-Stati:
 */

#define MOUSE_LEFT	0x01
#define MOUSE_RIGHT	0x02
// Vermutlich...
#define	MOUSE_MID	0x04

/*
 * globale Variablen
 */

static sAviaGtInfo *gt_info;

static unsigned char *receive_buffer;

static unsigned received[MAX_RECV];
static unsigned read_ptr = 0;
static unsigned write_ptr = 0;
static struct input_dev idev;

/*
 * Tasklet
 */

void samsung_task(unsigned long unused);
DECLARE_TASKLET(samsung_tlet,samsung_task,0);

/*
 * Timer
 */

static struct timer_list key_timeout;

/*
 * Queue-Routinen
 */

/* NO LOCKING, CALLED FROM IRQ */

static int write_to_queue(unsigned value)
{
	unsigned new_ptr = (write_ptr + 1) & (MAX_RECV - 1);

	if ( new_ptr == read_ptr )
	{
		return -1;
	}

	received[write_ptr] = value;
	write_ptr = new_ptr;
	return 0;
}

/* IRQs DISABLED */

static unsigned read_from_queue(void)
{
	unsigned long flags;
	unsigned wert = -1;

	save_flags(flags);
	cli();
	if (read_ptr != write_ptr)
	{
		wert = received[read_ptr];
		read_ptr = (read_ptr + 1) & (MAX_RECV - 1);
	}
	restore_flags(flags);
	return wert;
}

/* NO LOCKS NEEDED */

static unsigned queue_data_avail(void)
{
	return read_ptr != write_ptr;
}

/*
 * Break-Code nach Timeout
 */

static void break_missed(unsigned long data)
{
	dprintk(KERN_INFO "timeout reached, code: %d, value: 0\n",data);
	input_report_key(&idev,data,0);
}

/*
 * Report Key mit Timeout
 */

void do_input_report_key(unsigned int code, int value)
{
	if (!value)
	{
		del_timer(&key_timeout);
		dprintk(KERN_INFO "1 code: %d, value: 0\n",code);
		input_report_key(&idev,code,0);
	}
	else
	{
		if (timer_pending(&key_timeout))
		{
			if (code != key_timeout.data)
			{
				input_report_key(&idev,key_timeout.data,0);
				dprintk(KERN_INFO "2 code: %d, value: 0\n",key_timeout.data);
				input_report_key(&idev,code,1);
				key_timeout.data = code;
				dprintk(KERN_INFO "3 code: %d, value: 1\n",code);
			}
		}
		else
		{
			input_report_key(&idev,code,1);
			key_timeout.data = code;
			dprintk(KERN_INFO "4 code: %d, value: 1\n",code);
		}
		mod_timer(&key_timeout,jiffies + BREAK_KEY_TIMEOUT);
	}
}

/*
 * Auswertung der übermittelten Tstendrücke
 */

void samsung_task(unsigned long unused)
{
	unsigned longkey;
	unsigned char *key = (unsigned char *) &longkey;
	signed char *mouse = (signed char *) &longkey;
	unsigned char changed_status;

	static unsigned char old_status = 0;
	static unsigned char old_mouse_status = 0xF8;

	while (queue_data_avail())
	{
		longkey = read_from_queue();

		dprintk(KERN_INFO "longkey: 0x%08X\n",jiffies,longkey);

		/*
		 * Mouse ?
		 */

		if ((key[0]  & 0xF8) == 0xF8)
		{
			changed_status = old_mouse_status ^ key[0];
			if (changed_status & MOUSE_LEFT)
			{
				input_report_key(&idev,BTN_LEFT,(key[0] & MOUSE_LEFT));
			}
			if (changed_status & MOUSE_RIGHT)
			{
				input_report_key(&idev,BTN_RIGHT,(key[0] & MOUSE_RIGHT));
			}
			old_mouse_status = key[0];
			if (mouse[1])
			{
				input_report_rel(&idev,REL_X,mouse[1]);
			}
			if (mouse[2])
			{
				input_report_rel(&idev,REL_Y,-mouse[2]);
			}
			continue;
		}

		/*
		 * Not keyboard ?
		 */

		if (key[0] & 0xF8)
		{
			continue;
		}

		/*
		 * shift, alt...
		 */

		changed_status = key[1] ^ old_status;

		if ( changed_status & LEFTSHIFT )
		{
			dprintk(KERN_INFO "LEFTSHIFT %d\n",(key[1] & LEFTSHIFT));
			input_report_key(&idev,KEY_LEFTSHIFT,(key[1] & LEFTSHIFT));
		}
		if ( changed_status & RIGHTSHIFT )
		{
			dprintk(KERN_INFO "RIGHTSHIFT %d\n",(key[1] & RIGHTSHIFT));
			input_report_key(&idev,KEY_RIGHTSHIFT,(key[1] & RIGHTSHIFT));
		}
		if ( changed_status & LEFTALT )
		{
			dprintk(KERN_INFO "LEFTALT %d\n",(key[1] & LEFTALT));
			input_report_key(&idev,KEY_LEFTALT,(key[1] & LEFTALT));
		}
		if ( changed_status & RIGHTALT )
		{
			dprintk(KERN_INFO "RIGHTALT %d\n",(key[1] & RIGHTALT));
			input_report_key(&idev,KEY_RIGHTALT,(key[1] & RIGHTALT));
		}
		if (changed_status & LEFTCTRL )
		{
			dprintk(KERN_INFO "LEFTCTRL %d\n",(key[1] & LEFTCTRL));
			input_report_key(&idev,KEY_LEFTCTRL,(key[1] & LEFTCTRL));
		}
		if (changed_status & RIGHTCTRL )
		{
			dprintk(KERN_INFO "RIGHTCTRL %d\n",(key[1] & RIGHTCTRL));
			input_report_key(&idev,KEY_RIGHTCTRL,(key[1] & RIGHTCTRL));
		}

		old_status = key[1];

		if (key[2] >= sizeof(keyboard_mapping))
		{
			continue;
		}

		/*
		 * Fn ?
		 */

		if (old_status & FN)
		{
			switch (key[2]) {
				case 0x62:
					dprintk(KERN_INFO "F1 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F1, !(old_status & BREAK));
					break;
				case 0x63:
					dprintk(KERN_INFO "F2 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F2, !(old_status & BREAK));
					break;
				case 0x64:
					dprintk(KERN_INFO "F3 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F3, !(old_status & BREAK));
					break;
				case 0x65:
					dprintk(KERN_INFO "F4 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F4, !(old_status & BREAK));
					break;
				case 0x45:
					dprintk(KERN_INFO "F5 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F5, !(old_status & BREAK));
					break;
				case 0x46:
					dprintk(KERN_INFO "F6 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F6, !(old_status & BREAK));
					break;
				case 0x66:
					dprintk(KERN_INFO "F7 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F7, !(old_status & BREAK));
					break;
				case 0x68:
					dprintk(KERN_INFO "F8 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F8, !(old_status & BREAK));
					break;
				case 0x69:
					dprintk(KERN_INFO "F9 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F9, !(old_status & BREAK));
					break;
				case 0x6B:
					dprintk(KERN_INFO "F10 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F10, !(old_status & BREAK));
					break;
				case 0x08:
					dprintk(KERN_INFO "F11 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F11, !(old_status & BREAK));
					break;
				case 0x4B:
					dprintk(KERN_INFO "F12 %d\n",!(old_status & BREAK));
					do_input_report_key(KEY_F12, !(old_status & BREAK));
					break;
				case 0x60:
					/* Fn alone */
					break;
				default:
					break;
			}
			continue;
		}

		if (keyboard_mapping[key[2]] != KEY_RESERVED)
		{
			dprintk(KERN_INFO "0x%02X %d\n",keyboard_mapping[key[2]],!(old_status & BREAK));
			do_input_report_key(keyboard_mapping[key[2]], !(old_status & BREAK));
		}
	}
}

/*
 * Decodier-Routinen
 */

static unsigned char chip2value(unsigned char chip)
{
	switch (chip)
	{
		case 8:
			return 0;
		case 4:
			return 1;
		case 2:
			return 2;
		case 1:
			return 3;
		default:
			dprintk(KERN_INFO "illegal chip\n");
			return 255;
	}
}

void samsung_ir_decode(unsigned char *recv, unsigned samples)
{
	unsigned char *idx = recv;
	unsigned zaehler;

	unsigned char bits[36];	/* Begrenzung in Empfangsroutine (>71) */
	unsigned char *bier = bits;
	unsigned char bit = 7;
	unsigned char time;
	unsigned wert = 0;
	unsigned char value;
	unsigned summe;

	/*
	 * Startsequenz OK ?
	 */

	if ( (recv[0] != 1) || (recv[1] != 3) || (recv[2] != 3) )
	{
		return;
	}

	/*
	 * Bitmusker rekonstruieren
	 */

	memset(bits,0,sizeof(bits));

	for (zaehler = 0; zaehler < samples; zaehler++)
	{
		time = *(idx++);
		bier += time >> 8;
		if ( (time & 7) > bit )
		{
			bier++;
			bit = bit + 8 - (time & 7);
		}
		else
		{
			bit -= time;
		}

		time = *(idx++);
		while (time--)
		{
			*bier |= 1 << bit;
			if (!bit)
			{
				bier++;
				bit = 7;
			}
			else
			{
				bit--;
			}
		}
	}

	/*
	 * Dekodieren
	 */

	bier = bits + 1;

	for (zaehler = 0; zaehler < 8; zaehler++)
	{
		if ( (value = chip2value(*bier >> 4)) == 255)
		{
			return;
		}


		wert |= value << (30 - (4 * zaehler));

		if ( (value = chip2value(*bier & 15)) == 255)
		{
			return;
		}

		wert |= value << (28 - (4 * zaehler));

		bier++;
	}

	/*
	 * Prüfsumme.
	 */

	bier = (unsigned char *) &wert;
	summe = 0;

	for (zaehler = 0; zaehler < 3; zaehler++)
	{
		summe += (bier[zaehler] >> 4) + (bier[zaehler] & 0x0F);
	}

	summe += bier[3] >> 4;

	if ( (bier[3] & 0x0F) != (summe & 0x0F) )
	{
		dprintk(KERN_INFO "Checksum error\n");
		return;
	}

	/*
	 * In Queue schreiben
	 */

	if (!write_to_queue(wert))
	{
		tasklet_schedule(&samsung_tlet);
	}
}

/*
 * Receive-Interrupt
 */

static void samsung_ir_receive_irq(u16 irq)
{
	static unsigned char receive_buffer_start_offset = 0;
	static unsigned char lbuf[2 * MAX_PULSES];
	static unsigned char *lbufidx = lbuf;
	static unsigned char lanz = 0;
	static unsigned rec_time = 0;
	static unsigned char extra_len = 0;

	unsigned receive_buffer_end_offset;
	unsigned char pulse_on, pulse_len, pulse_off;
	unsigned char next_irq_at;

	receive_buffer_end_offset = (avia_gt_chip(ENX) ? enx_reg_16(IRRO) : gtx_reg_16(IRRO)) & 0x7F;

//	printk(KERN_INFO "on entry: start: %d, end: %d\n",receive_buffer_start_offset,receive_buffer_end_offset);


	while (receive_buffer_start_offset != (receive_buffer_end_offset = (avia_gt_chip(ENX) ? enx_reg_16(IRRO) : gtx_reg_16(IRRO)) & 0x7F) )
	{
		pulse_on = receive_buffer[receive_buffer_start_offset << 1];
		pulse_len = receive_buffer[(receive_buffer_start_offset << 1) + 1] + extra_len;
		extra_len = 0;
		pulse_off = pulse_len - pulse_on;

		receive_buffer_start_offset = (receive_buffer_start_offset + 1) & 0x7F;

#if 0
		/*
		 * Entstörfilter
		 */

		if (pulse_on < 2)
		{
			extra_len = pulse_len;
			if (extra_len > 63)
			{
				extra_len = 63;
			}
			dprintk(KERN_INFO "Störung\n");
			continue;
		}
#endif
		dprintk(KERN_INFO "Off: %d, On: %d, Len: %d\n",pulse_off,pulse_on,pulse_len);

		/*
		 * Ist die Pause zwischen den Impulsen zu groß oder zu klein ?
		 */

		if ( (pulse_off < 4) || ( (pulse_off > 50) && lanz) )
		{
			rec_time = 0;
			lanz = 0;
			lbufidx = lbuf;
//			printk(KERN_INFO "off: %d\n",pulse_off);
			continue;
		}

		if (!lanz && (pulse_len < 50) )
		{
//			printk(KERN_INFO "off: too short for start: %d\n",pulse_len);
			continue;
		}

		/*
		 * Ist der Impuls zu kurz oder zu lang?
		 */

		if ( !lanz && ( (pulse_on < 10 ) || (pulse_on > 18) ) )
		{
//			printk(KERN_INFO "start on-range: %d\n",pulse_on);
			continue;
		}

		/*
		 * Maximal 2 * High hintereinander nach Start
		 */

		if ( lanz && (pulse_on > 14) )
		{
//			printk(KERN_INFO "on-range: %d\n",pulse_on);
			rec_time = 0;
			lanz = 0;
			lbufidx = lbuf;
			continue;
		}

		/*
		 * Normalisierung
		 */

		pulse_on = (pulse_on + 3) / 6;
		pulse_len = (pulse_len + 3) / 6;
		pulse_off = pulse_len - pulse_on;

		dprintk(KERN_INFO "off: %d, on: %d\n",pulse_off,pulse_on);
		/*
		 * Speichern
		 */

		if (lanz)
		{
			rec_time += pulse_len;

			*(lbufidx++) = pulse_off;
			*(lbufidx++) = pulse_on;
		}
		else
		{
			rec_time += pulse_on;
			*(lbufidx++) = 1;
			*(lbufidx++) = pulse_on;
		}
		lanz++;

		/*
		 * Sequenz zu lang ?
		 */

		if (rec_time > 71)
		{
			dprintk(KERN_INFO "too long: %d\n",rec_time);
			rec_time = 0;
			lanz = 0;
			lbufidx = lbuf;
			continue;
		}

		/*
		 * Fertig ?
		 */

		if (rec_time >= 68)
		{
			samsung_ir_decode(lbuf,lanz);
			rec_time = 0;
			lanz = 0;
			lbufidx = lbuf;
			continue;
		}

		/*
		 * Pufferüberlauf ?
		 */

		if (lanz >= MAX_PULSES)
		{
			dprintk(KERN_INFO "buffer\n");
			rec_time = 0;
			lanz = 0;
			lbufidx = lbuf;
		}
	}

	if (!lanz)
	{
		next_irq_at = (receive_buffer_end_offset + 12) & 0x7F;
	}
	else
	{
		next_irq_at = receive_buffer_end_offset;
	}

	if (avia_gt_chip(ENX))
	{
		enx_reg_16(IRRE) = 0x8000 + next_irq_at;
	}
	else
	{
		gtx_reg_16(IRRE) = 0x8000 + next_irq_at;
	}

	dprintk(KERN_INFO "on exit: %d, next: %d\n",receive_buffer_end_offset,next_irq_at);

}

/*
 * Modul-Init
 */

static int __init samsung_ir_init(void)
{
	unsigned zaehler;

	gt_info = avia_gt_get_info();

	if ( !gt_info || (!avia_gt_chip(ENX) && !avia_gt_chip(GTX)) )
	{
		printk(KERN_ERR "dbox_samsung: Unsupported chip type.\n");
		return -EIO;
	}

	receive_buffer = gt_info->mem_addr + AVIA_GT_MEM_IR_OFFS;

	memset(&idev,0,sizeof(idev));
	idev.name = "Samsung infrared keyboard SEM-CIR";
	set_bit(EV_KEY,idev.evbit);
	set_bit(EV_REP,idev.evbit);
	set_bit(EV_REL,idev.evbit);
	set_bit(REL_X,idev.relbit);
	set_bit(REL_Y,idev.relbit);

	for (zaehler = 0; zaehler < sizeof(keyboard_mapping); zaehler++)
	{
		if (keyboard_mapping[zaehler] != KEY_RESERVED)
		{
			set_bit(keyboard_mapping[zaehler],idev.keybit);
		}
	}

	for (zaehler = 0; zaehler < sizeof(extra_keyboard) / 2; zaehler++)
	{
		set_bit(extra_keyboard[zaehler],idev.keybit);
	}

	input_register_device(&idev);

	init_timer(&key_timeout);
	key_timeout.function = break_missed;

	if (avia_gt_chip(ENX))
	{
		enx_reg_set(RSTR0,IR,1);		// Reset
		if (avia_gt_alloc_irq(ENX_IRQ_IR_RX,samsung_ir_receive_irq))
		{
			printk(KERN_ERR "dbox_samsung: Cannot allocate irq.\n");
			input_unregister_device(&idev);
			return -EIO;
		}

		enx_reg_set(RSTR0,IR,0);	// Reset off
		enx_reg_32(IRQA) = AVIA_GT_MEM_IR_OFFS;
		enx_reg_16(RTP) = 2830;		// divider 50 MHz
		enx_reg_16(RFR) = 0;		// Polarity, start pulse length-values
		enx_reg_16(RTC) = 0;		// no start pulse
		enx_reg_16(IRRO) = 0;		// Reset receive queue
		enx_reg_16(IRRE) = 0x8001;	// irq after 1 received value, dma
		enx_reg_16(IRTE) = 0;		// no transmits
	}
	else
	{
		gtx_reg_set(RR0,IR,1);		// Reset

		if (avia_gt_alloc_irq(GTX_IRQ_IR_RX,samsung_ir_receive_irq))
		{
			printk(KERN_ERR "dbox_samsung: Cannot allocate irq.\n");
			input_unregister_device(&idev);
			return -EIO;
		}

		gtx_reg_set(RR0,IR,0);		// Reset off
		gtx_reg_32(IRQA) = AVIA_GT_MEM_IR_OFFS;
		gtx_reg_16(RTP) = 2292;		// divider 40.5 MHz
		gtx_reg_16(RFR) = 0;		// Polarity, start pulse length-values
		gtx_reg_16(RTC) = 0;		// no start pulse
		gtx_reg_16(IRRO) = 0;		// Reset receive queue
		gtx_reg_16(IRRE) = 0x8001;	// irq after 1 received value, dma
		gtx_reg_16(IRTE) = 0;		// no transmits
	}

	return 0;
}

/*
 * Modul-Exit
 */

static void __exit samsung_ir_exit(void)
{

	if (avia_gt_chip(ENX))
	{
		enx_reg_set(RSTR0,IR,1);	// Reset
		avia_gt_free_irq(ENX_IRQ_IR_RX);
	}
	else
	{
		gtx_reg_set(RR0,IR,1);		// Reset
		avia_gt_free_irq(GTX_IRQ_IR_RX);
	}

	if (timer_pending(&key_timeout))
	{
		del_timer(&key_timeout);
		input_report_key(&idev,key_timeout.data,0);
	}

	input_unregister_device(&idev);

}

/*
 * Modul-Kram
 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wolfram Joost");
MODULE_DESCRIPTION("Driver for samsung infrared keyboard SEM-CIR on dbox2.");
module_init(samsung_ir_init);
module_exit(samsung_ir_exit);
