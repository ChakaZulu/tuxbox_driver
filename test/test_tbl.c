#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/poll.h>

#include <ost/dmx.h>
#include <ost/frontend.h>
#include <ost/sec.h>
#include <ost/video.h>

#define BSIZE 10000

int main(int argc, char **argv)
{
  int fd, r=BSIZE, i;
  struct dmxSctFilterParams flt; 
  char buffer[BSIZE];

  fd=open("/dev/ost/demux0", O_RDONLY);
  if (fd<0)
  {
    perror("/dev/ost/demux0");
    return -fd;
  }

  memset(&flt.filter.filter, 0, DMX_FILTER_SIZE);
  memset(&flt.filter.mask, 0, DMX_FILTER_SIZE);

  flt.pid=0;

  if (argc>=2)
  {
    int pid;
    sscanf(argv[1], "%x", &pid);
    flt.pid=pid;
    flt.filter.filter[0]=2;
  }

  if (argc>=3)
  {
    int filter;
    sscanf(argv[2], "%x", &filter);
    flt.filter.filter[0]=filter;
  }

  flt.filter.mask[0]  =0xFF;
  flt.timeout=10000;
  flt.flags=DMX_ONESHOT;

  flt.flags=0;
  if (ioctl(fd, DMX_SET_FILTER, &flt)<0)
  {
    perror("DMX_SET_FILTER");
    return 1;
  }
  
  ioctl(fd, DMX_START, 0);

  if ((r=read(fd, buffer, r))<=0)
  {
    perror("read");
    return 1;
  }
  
  printf("%d bytes.\n", r);
  
  if (!flt.pid)         // PAT
  {
    printf("TSID: %04x\n", (buffer[3]<<8)|buffer[4]);
    for (i=0; i<(r-8-4)/4; i++)
      printf("%04x PMT: %04x\n", (buffer[i*4+8]<<8)|(buffer[i*4+9]), ((buffer[i*4+10]&~0xE0)<<8)|(buffer[i*4+11]));
  } else
  {
    int pilen, dp;
    printf("Program: %04x\n", (buffer[3]<<8)|buffer[4]);
    printf("PCR-PID: %04x\n", ((buffer[8]&0x1F)<<8)|buffer[9]);
    pilen=((buffer[10]&0xF)<<8)|buffer[11];
    dp=12;
    while (dp<(pilen+12))
      printf("%02x ", buffer[dp++]);
    printf("\n");
    printf("%d bytes.\n", r-4-dp);
    while (dp<r-4)
    {
      int epid, esinfo, i;
      printf("---------\n");
      printf("stream type: %x\n", buffer[dp++]);
      epid=(buffer[dp++]&0x1F)<<8;
      epid|=buffer[dp++];
      printf("epid         %x\n", epid);
      esinfo=(buffer[dp++]&0xF)<<8;
      esinfo|=buffer[dp++];
      printf("%d bytes info: ", esinfo);
      for (i=0; i<esinfo; i++)
      {
        printf(" %02x", buffer[dp++]);
      }
      printf("\n");
    }
  }
  close(fd);
  return 0;
}
