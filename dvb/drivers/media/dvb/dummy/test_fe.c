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
#include <asm/types.h>

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

int FEDumpBER(int fd)
{ 

	unsigned int ber;
	int result; 
	
	if ((result = ioctl(fd, FE_READ_BER, &ber) < 0)) { 

		perror("FE READ BER: ");
		
		return result;
		
	} 
	
	printf("FE BER: %d\n", ber);

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

int FEDumpSignalStrength(int fd)
{ 

	signed int sig_str;
	int result; 
	
	if ((result = ioctl(fd, FE_READ_SIGNAL_STRENGTH, &sig_str) < 0)) { 

		perror("FE READ SIGNAL STRENGTH: ");
		
		return result;
		
	} 
	
	printf("FE SIGNAL STRENGTH: %d\n", sig_str);

	return 0;
		
}

int FEDumpSNR(int fd)
{ 

	signed int snr;
	int result; 
	
	if ((result = ioctl(fd, FE_READ_BER, &snr) < 0)) { 

		perror("FE READ SNR: ");
		
		return result;
		
	} 
	
	printf("FE SNR: %d\n", snr);

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

int FESetFrontend(int fd, struct dvb_frontend_parameters *parameters)
{

	int result; 

	if (!parameters)
		return -EINVAL;

	if ((result = ioctl(fd, FE_SET_FRONTEND, parameters) < 0)) { 

		perror("FE SET FRONTEND: ");
		
		return result;
		
	} 

	return 0;
	
}

int main(void)
{

	int fe_fd;
	struct dvb_frontend_info fe_info;
	struct dvb_frontend_parameters fe_param;

	if ((fe_fd = open(FRONT, O_RDWR)) < 0) {
	
		perror("FRONTEND DEVICE");

		return -1; 
		
	}

	FEGetInfo(fe_fd, &fe_info);
	FEDumpInfo(&fe_info);
	FEDumpStatus(fe_fd);
	
	if (fe_info.type == FE_QAM) {

		fe_param.frequency = 394000000;
		fe_param.u.qam.symbol_rate = 6900000;
		fe_param.u.qam.modulation = QAM_64;
		fe_param.inversion = INVERSION_AUTO;
	
	} else if (fe_info.type == FE_QPSK) {
	
		//FIXME
	
	} else if (fe_info.type == FE_OFDM) {
	
		//FIXME
	
	}
	
	FESetFrontend(fe_fd, &fe_param);
	FEDumpBER(fe_fd);
	FEDumpSNR(fe_fd);
	FEDumpSignalStrength(fe_fd);

	close(fe_fd);

}	