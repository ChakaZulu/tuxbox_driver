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

#define BSIZE 1024

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

  flt.pid=0;
  memset(&flt.filter.filter, 0, DMX_FILTER_SIZE);
  memset(&flt.filter.mask, 0, DMX_FILTER_SIZE);
  flt.filter.mask[0]  =0xFF;
  flt.filter.filter[0]=0;                 // PAT
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
  
  printf("TSID: %04x\n", (buffer[3]<<8)|buffer[4]);
  for (i=0; buffer[i*4+9]!=0xFF; i++)
    printf("%04x PMT: %04x\n", (buffer[i*4+9]<<8)|(buffer[i*4+10]), ((buffer[i*4+11]&~0xE0)<<8)|(buffer[i*4+12]));
  
  close(fd);
  return 0;
}
