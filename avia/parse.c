#include <stdio.h>
#include <stdlib.h>
#include "riscram.c"

#define sw(x) ( ((x>>8)&0xFF) | ((x&0xFF)<<8))

void main(void)
{
  int i;

  printf("pid search table:\n");

  for (i=0; i<32; i++)
  {
    unsigned short r=sw(*(unsigned short*)(riscram+0x700+i*2));
    // pusi valid reservered pid
    printf("%02d %s %s %s #%d (%x)\n", i, (r&(1<<15))?"WAIT_PUSI":"         ",
           (r&(1<<14))?"invalid":" valid", (r&(1<<13))?"res":"   ",
           r&8191, r&8191);
  }
  printf("\npid parsing control table:\n");
  printf("type quid fork cw_offset cc reserve start_up pec reserver reserve\n");
  for (i=0; i<32; i++)
  {
    unsigned char *buffer=riscram+0x740+i*4;
    printf("%04d %04d %04d %09d %02d %07d %08d %03d %08d %d\n",
      buffer[0]>>5, buffer[0]&0x1F, buffer[1]>>7, (buffer[1]>>4)&7,
      buffer[1]&0xF,  buffer[2]>>7, !!(buffer[2]>>6), buffer[2]>>5, buffer[21]&0xF,
      buffer[3]);
  }
}