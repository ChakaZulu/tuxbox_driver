#ifndef __FB_H
#define __FB_H

#define CCUBEFB_BLEV0	0	
#define CCUBEFB_BLEV1	1	

#define GTXFB_BLEV0	0	/* Blend level for pixels with Transparency class 0 */
#define GTXFB_BLEV1	1	/* Blend level for pixels with Transparency class 1 */

#define ENXFB_BLEV10	0	/* Blend level for pixels with Transparency class 0 Plane 1 */
#define ENXFB_BLEV11  	1       /* Blend level for pixels with Transparency class 1 Plane 1 */
#define ENXFB_BLEV20  	2	/* Blend level for pixels with Transparency class 0 Plane 2 */
#define ENXFB_BLEV21  	3	/* Blend level for pixels with Transparency class 1 Plane 2 */

#define CCUBEFB_XPOS	4	/* X position of graphics frame (scaled to gfx coordinates) */
#define	CCUBEFB_YPOS	5	/* Y position of graphics frame (lines) */

#define CCBUBEFB_FBCON_BLACK 6  /* schwarzer consolen hintergrund */
 
#endif
