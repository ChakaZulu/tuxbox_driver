#ifndef REGS_H
#define REGS_H

#define ENX_TCR1   0x03C8 /* Transparent Color Register */
#define ENX_BDST   0x0580 /* Blitter Destination Register */
#define ENX_BMR    0x0584 /* Blitter Mask Register */
#define ENX_BDR    0x0588 /* Blitter Data Register */
#define ENX_BCLR01 0x05A0 /* Blitter Color Register 0/1 */
#define ENX_BCLR23 0x05A4 /* Blitter Color Register 2/3 */
#define ENX_BPW    0x058C /* Blitter Pixel Width Register */
#define ENX_BPO    0x058E /* Blitter Pixel Offset Register */
#define ENX_GCSRC  0x05C0 /* Graphics Copy Source Address Register */
#define ENX_GCDST  0x05C4 /* Graphics Copy Destination Address Register */
#define ENX_GCCMD  0x05C8 /* Graphics Copy Command Register */

#define GTX_TCR    0x0008 /* Transparent Color Register */
#define GTX_BDST   0x00B0 /* Blitter Destination Register */
#define GTX_BMR    0x00B4 /* Blitter Mask Register */
#define GTX_BDR    0x00B6 /* Blitter Data Register */
#define GTX_BCLR   0x00B8 /* Blitter Color Register 0/1/2/3 */
#define GTX_BPW    0x00BC /* Blitter Pixel Width Register */
#define GTX_BPO    0x00BE /* Blitter Pixel Offset Register */
#define GTX_CSA    0x0220 /* Graphics Copy Source Address Register */
#define GTX_CDA    0x0224 /* Graphics Copy Destination Address Register */
#define GTX_CCOM   0x0228 /* Graphics Copy Command Register */

#endif
