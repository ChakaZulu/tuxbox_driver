#include "saa7126_calc.h"

void cyc2rgb( int cr, int y, int cb, int *r, int *g, int *b )
{
	*r = y + 1.3707*(cr-128);
	*g = y - 0.3365*(cb-128) - 0.6982*(cr-128);
	*b = y + 1.7324*(cb-128);
}

unsigned int fsc( float ffsc, float fiic )
{
	return((ffsc/fiic)*(2^32));
}
