/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/xgi/xg47_bios.h,v */

/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.						*
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining	*
 * a copy of this software and associated documentation files (the			*
 * "Software"), to deal in the Software without restriction, including		*
 * without limitation on the rights to use, copy, modify, merge,			*
 * publish, distribute, sublicense, and/or sell copies of the Software,		*
 * and to permit persons to whom the Software is furnished to do so,		*
 * subject to the following conditions:										*
 *																			*
 * The above copyright notice and this permission notice (including the		*
 * next paragraph) shall be included in all copies or substantial			*
 * portions of the Software.												*
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,			*
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF		*
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND					*
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR							*
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		*
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		*
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			*
 * DEALINGS IN THE SOFTWARE.												*
 ***************************************************************************/

#ifndef _XG47_BIOS_H_
#define _XG47_BIOS_H_

/* xg47_bios.c */
extern Bool XG47BiosValidMode(ScrnInfoPtr pScrn,
                              XGIAskModePtr pMode,
                              CARD32 dualView);
extern Bool XG47BiosModeInit(ScrnInfoPtr pScrn,
                             XGIAskModePtr pMode,
                             CARD32 dualView);
extern void XG47BiosValueInit(ScrnInfoPtr pScrn);
extern int  XG47BiosSpecialFeature(ScrnInfoPtr pScrn,
                                   unsigned long cmd,
                                   unsigned long *pInBuf,
                                   unsigned long *pOutBuf);
extern CARD32 XG47BiosGetFreeFbSize(ScrnInfoPtr pScrn, Bool isAvailable);
extern Bool   XG47BiosDTVControl(ScrnInfoPtr pScrn,
                                 unsigned long cmd,
                                 unsigned long * pInBuf,
                                 unsigned long * pOutBuf);

extern void XG47CloseAllDevice(XGIPtr pXGI, CARD8 device2Close);
extern void XG47OpenAllDevice(XGIPtr pXGI, CARD8 device2Open);
extern CARD16 XG47GetRefreshRateByIndex(CARD8 index);

#endif
