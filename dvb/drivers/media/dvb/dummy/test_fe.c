#include <sys/ioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <sys/poll.h>

#define FRONT "/dev/dvb/adapter0/frontend0"

int FEGetInfo(int fd, struct dvb_frontend_info *info)
{

	int result; 
	
	if (!info)
		return -EINVAL;
	
	if ((result = ioctl(fd, FE_GET_INFO, info) < 0)) { 

		perror("FE GET INFO: ");
		
		return result;
		
	} 

	return 0;

}

int FEDumpInfo(struct dvb_frontend_info *info)
{

	if (!info)
		return -EINVAL;
		
	switch(info->type) {
	
		case FE_QAM:
		
			printf("FE Type: QAM\n");
			
			break;

		case FE_QPSK:
		
			printf("FE Type: QPSK\n");
			
			break;

		case FE_OFDM:
		
			printf("FE Type: OFDM\n");
			
			break;
			
		default:
		
			printf("FE Type: unknown!\n");
			
	}
	
	printf("FE Name: %s\n", info->name);

	return 0;

}

int FEDumpStatus(int fd)
{ 

	fe_status_t stat;
	int result; 
	
	if ((result = ioctl(fd, FE_READ_STATUS, &stat) < 0)) { 

		perror("FE READ STATUS: ");
		
		return result;
		
	} 
	
	printf("FE Status:");
	
	if (stat & FE_HAS_SIGNAL)
		printf(" FE_HAS_SIGNAL");

	if (stat & FE_HAS_VITERBI)
		printf(" FE_HAS_VITERBI");

	if (stat & FE_HAS_SYNC)
		printf(" FE_HAS_SYNC");

	if (stat & FE_HAS_CARRIER)
		printf(" FE_HAS_CARRIER");

	if (stat & FE_HAS_LOCK)
		printf(" FE_HAS_LOCK");
		
	printf("\n");
		
	return 0;
	
}

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
	
//	frequency = (uint32_t)freq;
//	symbolrate = (uint32_t)srate; 
	

}

int main(void)
{

	int front_fd;
	struct dvb_frontend_info front_info;

	if ((front_fd = open(FRONT, O_RDWR)) < 0) {
	
		perror("FRONTEND DEVICE");

		return -1; 
		
	}

	FEGetInfo(front_fd, &front_info);
	FEDumpInfo(&front_info);
	FEDumpStatus(front_fd);

//	set_qam_channel(front, 510000, 0x0001, 0x0002, 0x0003, 7000, 1);

	close(front_fd);

}	