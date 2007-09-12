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

#ifndef _XG47_MODE_H_
#define _XG47_MODE_H_

/* xg47_mode.c */
extern void XG47ModeSave(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg);
extern void XG47ModeRestore(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg);
extern void XG47LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
                            LOCO *colors, VisualPtr pVisual);
extern void XG47SetOverscan(ScrnInfoPtr pScrn, int overscan);
extern unsigned int XG47DDCRead(ScrnInfoPtr pScrn);
extern Bool XG47ModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
extern int XG47ValidMode(ScrnInfoPtr pScrn, DisplayModePtr dispMode);

extern void XG47SetCRTCViewStride(ScrnInfoPtr pScrn);
extern void XG47SetCRTCViewBaseAddr(ScrnInfoPtr pScrn, unsigned long startAddr);
extern void XG47SetW2ViewStride(ScrnInfoPtr pScrn);
extern void XG47SetW2ViewBaseAddr(ScrnInfoPtr pScrn, unsigned long startAddr);

extern XGIModePtr XG47GetModeFromRes(unsigned width, unsigned height);

#endif
