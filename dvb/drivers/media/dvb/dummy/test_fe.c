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

		perror("FE_GET_INFO");
		
		return result;
		
	} 

	return 0;

}

int FEDumpBER(int fd)
{ 

	unsigned int ber;
	int result; 
	
	if ((result = ioctl(fd, FE_READ_BER, &ber) < 0)) { 

		perror("FE_READ_BER");
		
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

	unsigned short sig_str;
	int result; 
	
	if ((result = ioctl(fd, FE_READ_SIGNAL_STRENGTH, &sig_str) < 0)) { 

		perror("FE_READ_SIGNAL_STRENGTH");
		
		return result;
		
	} 
	
	printf("FE SIGNAL STRENGTH: %d\n", sig_str);

	return 0;
		
}

int FEDumpSNR(int fd)
{ 

	unsigned short snr;
	int result; 
	
	if ((result = ioctl(fd, FE_READ_BER, &snr) < 0)) { 

		perror("FE_READ_SNR");
		
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

		perror("FE_READ_STATUS");
		
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

		perror("FE_SET_FRONTEND");
		
		return result;
		
	} 

	return 0;
	
}

int FESetVoltage(int fd, fe_sec_voltage_t v)
{
	
	int result;
	
	if ((result = ioctl(fd, FE_SET_VOLTAGE, v) < 0)) {

		perror("FE_SET_VOLTAGE");

		return result;

	}
	
	return 0;

}

int FESetTone(int fd, fe_sec_tone_mode_t t)
{
	
	int result;
	
	if ((result = ioctl(fd, FE_SET_TONE, t) < 0)) {

		perror("FE_SET_TONE");

		return result;

	}

	return 0;

}

int main (void)
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
		fe_param.inversion = INVERSION_OFF;
		fe_param.u.qam.symbol_rate = 6900000;
		fe_param.u.qam.fec_inner = FEC_AUTO;
		fe_param.u.qam.modulation = QAM_64;
	
	} else if (fe_info.type == FE_QPSK) {

		fe_param.frequency = 12669500-10600000;
		fe_param.inversion = INVERSION_OFF;
		fe_param.u.qpsk.symbol_rate = 22000000;
		fe_param.u.qpsk.fec_inner = FEC_AUTO;

		FESetVoltage(fe_fd, SEC_VOLTAGE_13);
		FESetTone(fe_fd, SEC_TONE_ON);
	
	} else if (fe_info.type == FE_OFDM) {

		fe_param.frequency = 730000000;
		fe_param.inversion = INVERSION_OFF;
		fe_param.u.ofdm.bandwidth = BANDWIDTH_8_MHZ;
		fe_param.u.ofdm.code_rate_HP = FEC_2_3;
		fe_param.u.ofdm.code_rate_LP = FEC_1_2;
		fe_param.u.ofdm.constellation = QAM_16;
		fe_param.u.ofdm.transmission_mode = TRANSMISSION_MODE_2K;
		fe_param.u.ofdm.guard_interval = GUARD_INTERVAL_1_8;
		fe_param.u.ofdm.hierarchy_information = HIERARCHY_NONE;
		
	} else {

		printf("invalid fe_info.type\n");
		
		close(fe_fd);

		return -1;

	}
		
	
	FESetFrontend(fe_fd, &fe_param);
	FEDumpBER(fe_fd);
	FEDumpSNR(fe_fd);
	FEDumpSignalStrength(fe_fd);

	close(fe_fd);

	return 0;

}

