#ifndef __FP_H
#define __FP_H

#define FP_MAJOR            60
#define FP_MINOR            0
#define RC_MINOR            1

#define FP_IOCTL_GETID          0
#define FP_IOCTL_POWEROFF       1
#define FP_IOCTL_REBOOT         2

int fp_set_tuner_dword(int type, u32 tw);
int fp_set_polarisation(int pol);

#define P_HOR           0
#define P_VERT          1

int fp_do_reset(int type);

#define T_UNKNOWN       0
#define T_QAM           1
#define T_QPSK          2

int fp_send_diseqc(u32 dw);


 // mehr fallen mir jetzt nicht ein.


#endif