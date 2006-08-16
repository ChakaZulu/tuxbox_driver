#include <linux/kernel.h>
#include <asm/time.h>
#include <asm/uaccess.h>

#define TRACE_WITH_CALLSTACK 1

#define TRACE_DEPTH 32

#ifdef TRACE_WITH_CALLSTACK
#define STACK_DEPTH 4
#endif

extern void m8xx_wdt_reset(void);

extern char * dboxide_trace_msg[];

typedef struct IDETraceData {
  unsigned long time;
  unsigned int typ;
  unsigned int a;
  unsigned int b;

#ifdef TRACE_WITH_CALLSTACK
  unsigned long stack[STACK_DEPTH];
#endif

} IDETraceData;

static int traceIdx;
static IDETraceData trace[TRACE_DEPTH];

static void log_backtrace(IDETraceData * td)
{
#ifdef TRACE_WITH_CALLSTACK
        int cnt = 0;
        unsigned long i;
	unsigned long *sp;

        asm("mr %0,1" : "=r" (sp));

        while (sp) {
                if (__get_user( i, &sp[1] ))
                        break;

		if (cnt>0) td->stack[cnt-1] = i;
                cnt++;

                if (cnt > STACK_DEPTH) break;

                if (__get_user(sp, (unsigned long **)sp))
                        break;
        }
#endif
}

static void print_callstack(IDETraceData * t)
{
#ifdef TRACE_WITH_CALLSTACK
  int j;
  for (j=0; j<STACK_DEPTH; j++)
  {
    if (t->stack[j]==0) break;

    printk("%08lx ", t->stack[j] );
    
    t->stack[j] = 0;
  }
  printk("\n");
#endif
}

void dboxide_log_trace(unsigned int typ, unsigned int a, unsigned int b)
{
  IDETraceData * t= &trace[traceIdx];
  traceIdx = (traceIdx + 1)&((TRACE_DEPTH)-1);

  t->time = get_tbl(); /* timebase: fast, free running counter */
  t->typ = typ;
  t->a = a;
  t->b = b;

  log_backtrace( t );
}

void dboxide_print_trace(void)
{
  int i;
  IDETraceData * t;
  printk("dboxide: trace\n");
  for (i=TRACE_DEPTH-1; i>0; i--)
  {
    int idx = (traceIdx + i) & ((TRACE_DEPTH)-1);
    t = &trace[idx]; 
   
    if (t->typ==0) continue;

    printk("%08lx: %s %8x %8x\n", t->time, dboxide_trace_msg[t->typ], t->a, t->b);

    print_callstack(t);

    // this can take a while and when not printing to console the watchdog is not served
    if ((i&7)==0) 
      m8xx_wdt_reset();

    t->typ = 0;
  }

  printk("trace end.\n");
}

