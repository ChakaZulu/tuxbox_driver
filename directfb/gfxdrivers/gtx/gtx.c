#include <stdio.h>
#include <unistd.h>

#include <linux/fb.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/gfxcard.h>
#include <core/state.h>
#include <core/surfaces.h>
#include <core/graphics_driver.h>

#include <gfx/convert.h>


DFB_GRAPHICS_DRIVER( gtx );

#include "regs.h"
#include "mmio.h"
#include "gtx.h"


#define GTX_DRAWING_FLAGS \
               (DSDRAW_NOFX)

#define GTX_DRAWING_FUNCTIONS \
               (DFXL_FILLRECTANGLE)

#define GTX_BLITTING_FLAGS \
               (DSBLIT_NOFX)

#define GTX_BLITTING_FUNCTIONS \
               (DFXL_BLIT)


typedef struct {
  /* misc stuff */
  long  mem_offset;
  __u32 orig_tcr;

  /* state validation */
  int   v_color;
  int   v_src;
  int   v_dst;

  /* stored values */
  int   pixelwidth;
  int   src_offset;
  int   src_pitch;
  int   dst_offset;
  int   dst_pitch;
} GTXDeviceData;

typedef struct {
  volatile __u8 *mmio_base;
} GTXDriverData;

static __u8 gtx = 0;
static __u8 enx = 0;

static inline void
gtx_validate_color (GTXDriverData *gdrv,
                    GTXDeviceData *gdev, CardState *state)
{
  if (gdev->v_color)
    return;

  switch (state->destination->format)
    {
    case DSPF_LUT8:
      if (gtx)
        gtx_out16 (gdrv->mmio_base, GTX_BCLR, state->color_index);
//    else if (enx)
	  
      break;
    case DSPF_RGB332:
      if (gtx)
        gtx_out16 (gdrv->mmio_base, GTX_BCLR, PIXEL_RGB332 (state->color.r,
                                                            state->color.g,
                                                            state->color.b));
//    else if (enx)
//      gtx_out16 (gdrv->mmio_base, ENX_BCLR, PIXEL_RGB332 (state->color.r,
//                                                           state->color.g,
//                                                           state->color.b));
      break;
    case DSPF_ARGB1555:
      if (gtx)
        gtx_out16 (gdrv->mmio_base, GTX_BCLR, PIXEL_ARGB1555 (state->color.a,
        		                          					  state->color.r,
                                                              state->color.g,
                                                              state->color.b));
//      else if (enx)
//        gtx_out16 (gdrv->mmio_base, ENX_BCLR, PIXEL_ARGB1555 (state->color.a,
//        							state->color.r,
//                                                          state->color.g,
//                                                          state->color.b));
      break;
    case DSPF_RGB16:
      if (gtx)
        gtx_out16 (gdrv->mmio_base, GTX_BCLR, PIXEL_RGB16 (state->color.r,
                                                           state->color.g,
                                                           state->color.b));
//	  else if (enx)
//        gtx_out16 (gdrv->mmio_base, ENX_BCLR, PIXEL_RGB16 (state->color.r,
//                                                           state->color.g,
//                                                           state->color.b));
    default:
      BUG ("unexpected pixelformat");
      break;
    }

  gdev->v_color = 1;
}

static inline void
gtx_validate_dst (GTXDeviceData *gdev, CardState *state)
{
  CoreSurface *dest = state->destination;

  if (gdev->v_dst)
    return;

  gdev->pixelwidth = DFB_BYTES_PER_PIXEL (dest->format);
  gdev->dst_offset = dest->front_buffer->video.offset;
  gdev->dst_pitch  = dest->front_buffer->video.pitch;

  gdev->v_dst = 1;
}

static inline void
gtx_validate_src (GTXDeviceData *gdev, CardState *state)
{
  CoreSurface *source = state->source;

  if (gdev->v_src)
    return;

  gdev->src_offset = source->front_buffer->video.offset;
  gdev->src_pitch  = source->front_buffer->video.pitch;

  gdev->v_src = 1;
}


static void
gtxEngineSync (void *drv, void *dev)
{
//  gtx_waitidle( gdrv, gdev );
}

static void
gtxCheckState (void *drv, void *dev,
	       CardState *state, DFBAccelerationMask accel)
{
  switch (state->destination->format)
    {
    case DSPF_LUT8:
    case DSPF_RGB332:
    case DSPF_ARGB1555:
    case DSPF_RGB16:
      break;
    default:
      return;
    }

  if (accel & 0xFFFF)
    {
      if (state->drawingflags & ~GTX_DRAWING_FLAGS)
	return;

      state->accel |= GTX_DRAWING_FUNCTIONS;
    }
  else
    {
      if (state->source->format != state->destination->format)
	return;

      if (state->blittingflags & ~GTX_BLITTING_FLAGS)
	return;

      state->accel |= GTX_BLITTING_FUNCTIONS;
    }
}

static void
gtxSetState (void *drv, void *dev,
	     GraphicsDeviceFuncs *funcs,
	     CardState *state, DFBAccelerationMask accel)
{
  GTXDriverData *gdrv = (GTXDriverData*) drv;
  GTXDeviceData *gdev = (GTXDeviceData*) dev;

  if (state->modified)
    {
      if (state->modified & SMF_DESTINATION)
        gdev->v_color = gdev->v_dst = 0;
      else if (state->modified & SMF_COLOR)
        gdev->v_color = 0;

      if (state->modified & SMF_SOURCE)
        gdev->v_src = 0;
    }

  switch (accel)
    {
    case DFXL_FILLRECTANGLE:
      gtx_validate_dst (gdev, state);
      gtx_validate_color (gdrv, gdev, state);

      state->set |= GTX_DRAWING_FUNCTIONS;
      break;

    case DFXL_BLIT:
      gtx_validate_dst (gdev, state);
      gtx_validate_src (gdev, state);

      state->set |= GTX_BLITTING_FUNCTIONS;
      break;

    default:
      BUG ("unexpected drawing/blitting function!");
      break;
    }

  state->modified = 0;
}

static void
gtxFillRectangle (void *drv, void *dev, DFBRectangle *rect)
{
  GTXDriverData *gdrv = (GTXDriverData*) drv;
  GTXDeviceData *gdev = (GTXDeviceData*) dev;
  volatile __u8 *mmio = gdrv->mmio_base;

  int   pw     = gdev->pixelwidth;
  int   pitch  = gdev->dst_pitch;
  int   width  = rect->w * pw;
  int   height = rect->h;
  __u16 data   = pw == 2 ? 0x5555 : 0xFFFF;
  __u32 addr   = gdev->mem_offset + gdev->dst_offset + rect->x * pw + rect->y * pitch;

  if (gtx) {
  
    gtx_out16 (mmio, GTX_BPO, 0x8000); /* Set Mode */

    while (height--)
      {
        int w,i;

        gtx_out32 (mmio, GTX_BDST, addr); /* Set destination address */

        /* If the rectangle is greater than 240 pixels wide, break it up
         * into a series of 240-pixel wide blits
         */
        for (w = width; w > 240; w -= 240)
    	{
	    gtx_out16 (mmio, GTX_BPW, 240);

	    /* do 16 pixels per iteration */
  	    for (i = (239 >> 4) + 1; i != 0; --i)
              {
              gtx_out16 (mmio, GTX_BDR, data);
            }
   		}

        /* Do the remainder of the last block */
        gtx_out16 (mmio, GTX_BPW, w);
        for (w = ((w - 1) >> 4) + 1; w != 0; --w)
        {
          gtx_out16 (mmio, GTX_BDR, data);
        }

        /* Flush the blitter internal registers by writing 16 masked pixels */
        gtx_out32 (mmio, GTX_BMR, 0);

        addr += pitch; /* Do next scanline */
    }
  }
}

static void
gtxBlit (void *drv, void *dev,
	 DFBRectangle *rect, int dx, int dy )
{
  GTXDriverData *gdrv = (GTXDriverData*) drv;
  GTXDeviceData *gdev = (GTXDeviceData*) dev;
  volatile __u8 *mmio = gdrv->mmio_base;

  int   odd1, odd2;
  int   pw     = gdev->pixelwidth;
  int   width  = rect->w * pw;
  int   height = rect->h;

  __u32 cda    = gdev->mem_offset +
                 gdev->dst_offset +      dx * pw +      dy * gdev->dst_pitch;
  __u32 csa    = gdev->mem_offset +
                 gdev->src_offset + rect->x * pw + rect->y * gdev->src_pitch;

  odd1 = (cda & 1) | (csa & 1);
  odd2 = (cda & 1) & (csa & 1);

  while (height--)
    {
      int odd = odd1, w = width;

      if (gtx) 
        {
          gtx_out32 (mmio, GTX_CDA, cda); /* Set destination address */
          gtx_out32 (mmio, GTX_CSA, csa); /* Set source address */
        } 
//    else if (enx) 
//    {
//    }

      while (w > 0)
        {
          int cw = (w > 15) ? (16 - odd) : w;

          if (gtx)
            gtx_out16 (mmio, GTX_CCOM, 0x300 | (cw - 1));

          /* if both addresses started odd byte then they are even byte now */
          if (odd2)
            odd = 0;

          w -= cw;
        }

      cda += gdev->dst_pitch;
      csa += gdev->src_pitch;
    }
}

static int
driver_probe (GraphicsDevice *device)
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_CCUBE_AVIA_GTX:
               gtx = 1;
               return 1;
          case FB_ACCEL_CCUBE_AVIA_ENX:
               enx = 1;
               return 1;
     }
     return 0;
}

static void
driver_get_info (GraphicsDevice     *device,
                 GraphicsDriverInfo *info)
{
  /* fill driver info structure */
  snprintf( info->name,
	    DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
	    "Avia eNX/GTX Driver" );

  snprintf( info->vendor,
	    DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
	    "Thomas Klamm" );

  info->version.major = 0;
  info->version.minor = 3;

  info->driver_data_size = sizeof (GTXDriverData);
  info->device_data_size = sizeof (GTXDeviceData);
}


static DFBResult
driver_init_driver (GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data)
{
  GTXDriverData *gdrv = (GTXDriverData*) driver_data;

  gdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
  if (!gdrv->mmio_base)
    return DFB_IO;

  funcs->EngineSync    = gtxEngineSync;
  funcs->CheckState    = gtxCheckState;
  funcs->SetState      = gtxSetState;
  funcs->FillRectangle = gtxFillRectangle;
  funcs->Blit          = gtxBlit;

  return DFB_OK;
}


static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
  GTXDriverData *gdrv = (GTXDriverData*) driver_data;
  GTXDeviceData *gdev = (GTXDeviceData*) device_data;
  volatile __u8 *mmio = gdrv->mmio_base;

  (void) gdrv;
  (void) mmio;

  /* calculate framebuffer offset within GTX DRAM */
  gdev->mem_offset = dfb_gfxcard_memory_physical (NULL,0) & 0xFFFFFF;
  
  /* fill device info */
  snprintf( device_info->name,
	    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Avia eNX/GTX" );

  snprintf( device_info->vendor,
	    DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "CCube" );

  /* set hardware capabilities */
  device_info->caps.flags    = 0;
  device_info->caps.accel    = GTX_DRAWING_FUNCTIONS |
                               GTX_BLITTING_FUNCTIONS;
  device_info->caps.drawing  = GTX_DRAWING_FLAGS;
  device_info->caps.blitting = GTX_BLITTING_FLAGS;

  /* set limitations */
  device_info->limits.surface_byteoffset_alignment = 4;
  device_info->limits.surface_pixelpitch_alignment = 2;

  /* set color key of graphics layer to DirectFB's standard bg color */
  if (gtx) {
    gdev->orig_tcr = gtx_in16 (gdrv->mmio_base, GTX_TCR);
    gtx_out16 (gdrv->mmio_base, GTX_TCR, 0x9153);
  }else if (enx) {
    gdev->orig_tcr = gtx_in32 (gdrv->mmio_base, ENX_TCR1);
	gtx_out32 (gdrv->mmio_base, ENX_TCR1, 0x00010101); //FIXME
  }

  return DFB_OK;
}

static void
driver_close_device (GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data)
{
  GTXDriverData *gdrv = (GTXDriverData*) driver_data;
  GTXDeviceData *gdev = (GTXDeviceData*) device_data;

  (void) gdrv;
  (void) gdev;
  
  /*restore TCR */
  if (gtx)
    gtx_out16 (gdrv->mmio_base, GTX_TCR, gdev->orig_tcr & 0xFFFF);
  else if (enx)
    gtx_out32 (gdrv->mmio_base, ENX_TCR1, gdev->orig_tcr);
}

static void
driver_close_driver (GraphicsDevice *device,
                     void           *driver_data)
{
  GTXDriverData *gdrv = (GTXDriverData*) driver_data;

  dfb_gfxcard_unmap_mmio( device, gdrv->mmio_base, -1 );
}
