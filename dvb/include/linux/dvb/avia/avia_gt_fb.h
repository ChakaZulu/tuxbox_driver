#ifndef __FB_H__
#define __FB_H__

typedef struct {

	u32 sx;	/* screen-relative */
	u32 sy;
	u32 width;
	u32 height;
	u32 dx;
	u32 dy;
	
} fb_copyarea;

#define AVIA_GT_GV_SET_BLEV	0	/* blend level */
#define AVIA_GT_GV_SET_POS	1	/* position of graphics frame */
#define AVIA_GT_GV_HIDE		2	/* hide framebuffer */
#define AVIA_GT_GV_SHOW		3	/* show framebuffer */
#define AVIA_GT_GV_COPYAREA	4	/* copy area */

#endif /* __FB_H__ */
