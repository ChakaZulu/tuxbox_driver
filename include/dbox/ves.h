#ifndef __VES_H
#define __VES_H

void ves_write_reg(int reg, int val);
void ves_init(void);
void ves_set_frontend(struct frontend *front);
void ves_get_frontend(struct frontend *front);
int ves_get_unc_packet(uint32_t *uncp);
#endif