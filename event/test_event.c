#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/poll.h>

#include <linux/types.h>
#include <linux/rtc.h>

#include <dbox/event.h>

#define EVENT_DEVICE "/dev/dbox/event0"

#define VERSION "0.1"

/** */
main(int argc, char **argv)
{
	int fd;
	int err;
	unsigned long arg;

	struct event_t event;

	if((fd = open(EVENT_DEVICE,O_RDWR)) < 0)
	{
		perror("open");
		return -1;
	}

	arg = EVENT_VCR_OFF | EVENT_VHSIZE_CHANGE | EVENT_ARATIO_CHANGE;

	if ( ioctl(fd,EVENT_SET_FILTER,arg) < 0 )
		perror("ioctl");
	else if ( read(fd,&event,sizeof(event_t)) <= 0 )
		perror("read");
	else
		printf("event: %d\n",event.event);

	close(fd);
	return 0;
}
