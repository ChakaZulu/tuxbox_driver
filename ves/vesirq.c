/*

    $Id: vesirq.c,v 1.2 2002/02/24 15:32:07 woglinde Exp $
    
    $Log: vesirq.c,v $
    Revision 1.2  2002/02/24 15:32:07  woglinde
    new tuner-api now in HEAD, not only in branch,
    to check out the old tuner-api should be easy using
    -r and date

    Revision 1.1.1.1.4.1  2002/01/22 22:58:04  fnbrd
    Id und Log bei manchen Quellen eingefuegt.

    
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/malloc.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>

#define VES_INTERRUPT   SIU_IRQ9

#ifdef MODULE
MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>");
MODULE_DESCRIPTION("Frontend Interrupt Handler");
#endif

static void ves_interrupt(int irq, void *dev, struct pt_regs * regs);
static int vesirq_init(void);
static void vesirq_close(void);

static void ves_interrupt(int irq, void *dev, struct pt_regs * regs)
{
  printk("VES_INTERRUPT!\n");
}

static int vesirq_init(void)
{
  if (request_8xxirq(VES_INTERRUPT, ves_interrupt, 0, "ves", 0) != 0)
    panic("Could not allocate VES IRQ!");
  return 0;
}

static void vesirq_close(void)
{
  free_irq(VES_INTERRUPT, 0);
}

#ifdef MODULE

int init_module(void)
{
  return vesirq_init();
}
void cleanup_module(void)
{
  vesirq_close();
}
#endif
