#ifndef __VES_H
#define __VES_H

struct demod_function_struct
{
	void (*write_reg)(int reg, int val);
	void (*init)(void);
	void (*set_frontend)(struct frontend *front);
	void (*get_frontend)(struct frontend *front);
	int (*get_unc_packet)(uint32_t *uncp);
};

extern int register_demod(struct demod_function_struct *demod);
extern int unregister_demod(struct demod_function_struct *demod);

#endif
