#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <sys/poll.h>

#define FRONT "/dev/dvb/adapter0/frontend0"

/* routine for checking if we have a signal and other status information*/
int FEReadStatus(int fd, fe_status_t *stat)
{ 

	int ans; 
	
	if ((ans = ioctl(fd, FE_READ_STATUS, stat) < 0)) { 

		perror("FE READ STATUS: ");
		
		return -1;
		
	} 
	
	if (*stat & FE_HAS_SIGNAL)
		printf("FE HAS SIGNAL\n");
		
	return 0;
	
}

/* tune qpsk */
/* freq: frequency of transponder */
/* vpid, apid, tpid: PIDs of video, audio and teletext TS packets */
/* diseqc: DiSEqC address of the used LNB */
/* pol: Polarisation */
/* srate: Symbol Rate */
/* fec. FEC */
/* lnb_lof1: local frequency of lower LNB band */
/* lnb_lof2: local frequency of upper LNB band */
/* lnb_slof: switch frequency of LNB */

int set_qpsk_channel(int fd, int freq, int vpid, int apid, int tpid, int diseqc, int pol, 
					 int srate, int fec, int lnb_lof1, int lnb_lof2, int lnb_slof) 
{

//	struct secCommand scmd;
//	struct secCmdSequence scmds;
//	struct dmxPesFilterParams pesFilterParams;
//	FrontendParameters frp;
//	struct pollfd pfd[1];
//	FrontendEvent event;
//	int demux1, dmeux2, demux3, front;
	
//	frequency = (uint32_t)freq;
//	symbolrate = (uint32_t)srate; 
	
/*	if ((sec = open(SEC,O_RDWR)) < 0) {
	
		perror("SEC DEVICE: ");
		
		return -1;
		
	}
*/	

}

/* tune qpsk */
/* freq: frequency of transponder */
/* vpid, apid, tpid: PIDs of video, audio and teletext TS packets */
/* srate: Symbol Rate */
/* fec. FEC */

int set_qam_channel(int fd, int freq, int vpid, int apid, int tpid, int srate, int fec) 
{

//	struct dmxPesFilterParams pesFilterParams;
	struct dvb_frontend_parameters frp;
	struct pollfd pfd[1];
	struct dvb_frontend_event event;
//	int demux1, dmeux2, demux3;
	fe_status_t stat;
	
//	frequency = (uint32_t)freq;
//	symbolrate = (uint32_t)srate; 
	
	FEReadStatus(fd, &stat);

}

int main(void)
{

	int front;

	if ((front = open(FRONT, O_RDWR)) < 0) {
	
		perror("FRONTEND DEVICE");

		return -1; 
		
	}

	set_qam_channel(front, 510000, 0x0001, 0x0002, 0x0003, 7000, 1);

}	