/***************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			   *
 *									   *
 * All Rights Reserved.							   *
 *									   *
 * Permission is hereby granted, free of charge, to any person obtaining   *
 * a copy of this software and associated documentation files (the	   *
 * "Software"), to deal in the Software without restriction, including	   *
 * without limitation on the rights to use, copy, modify, merge,	   *
 * publish, distribute, sublicense, and/or sell copies of the Software,	   *
 * and to permit persons to whom the Software is furnished to do so,	   *
 * subject to the following conditions:					   *
 *									   *
 * The above copyright notice and this permission notice (including the	   *
 * next paragraph) shall be included in all copies or substantial	   *
 * portions of the Software.						   *
 *									   *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	   *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	   *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		   *
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			   *
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,	   *
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,	   *
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER	   *
 * DEALINGS IN THE SOFTWARE.						   *
 ***************************************************************************/

#ifndef _XGI_MODE_H_
#define _XGI_MODE_H_

typedef struct {
    CARD16  modeNo;             /* video mode number. */
    CARD16  width;              /* width of the graphic mode in pixel. */
    CARD16  height;             /* height of the graphic mode in scanline.*/
    //CARD16  pixelSize;          /* bits/pixel of the graphic mode. */
    CARD16  refSupport[4];
    CARD16  refBIOS[4];
    CARD16  refReserved[1];
} XGIModeRec, *XGIModePtr;

#define MODE_320x200     0x30
#define MODE_320x240     0x31
#define MODE_400x300     0x32
#define MODE_512x384     0x33
#define MODE_640x400     0x60
#define MODE_640x432     0x61
#define MODE_640x480     0x61
#define MODE_720x480     0x38
#define MODE_720x576     0x3A
#define MODE_800x514     0x62
#define MODE_800x600     0x62
#define MODE_848X480     0x45      // 848x480.
#define MODE_1024x600    0x3B
#define MODE_1024x768    0x63
#define MODE_1152x864    0x68
#define MODE_1280x600    0x3D
#define MODE_1280X720    0x43
#define MODE_1280x768    0x3E
#define MODE_1280x800    0x3F
#define MODE_1280x960    0x69
#define MODE_1280x1024   0x64
#define MODE_1440x900    0x40
#define MODE_1400x1050   0x3C
#define MODE_1600x1200   0x65
#define MODE_1680x1050   0x6B
#define MODE_1920X1080   0x44      // 1920x1080.
#define MODE_1920x1200   0x42
#define MODE_1920x1440   0x66
#define MODE_2048x1536   0x67
#define MAX_MODE_INT     0xFFFF
#define MAX_MODE_NO      0xFF

/* xgi_mode.c */
extern void XGIModeSave(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg);
extern void XGIModeRestore(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg);
extern void XGILoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
                           LOCO *colors, VisualPtr pVisual);
extern void XGISetOverscan(ScrnInfoPtr pScrn, int overscan);
extern unsigned int XGIDDCRead(ScrnInfoPtr pScrn);

#endif
