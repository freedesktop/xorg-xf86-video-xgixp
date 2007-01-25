/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/xgi/xg47_tv.h,v */

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

#ifndef _XG47_TV_H_
#define _XG47_TV_H_

#define HTOTAL                  0
#define VTOTAL                  1
#define TINT                    2
#define SATUR                   3
#define BRIGHT                  4
#define FLICKER                 5
#define TOTALCNT                (FLICKER+1)*2
#define SUPPORTMODE             7

#define INIT_TV_SCREEN                  30000
#define DETECT_TV_CONNECTION            30001
#define CONVERT_PAL_NTSC                30002
#define GET_TV_SCREEN_POSITION          30003
#define SET_TV_SCREEN_POSITION          30004
#define GET_TV_COLOR_ITEMS              30008
#define GET_TV_COLOR_INFORMATION        30009
#define SET_TV_COLOR_INFORMATION        30010
#define GET_TV_FLICKER_INFORMATION      30011
#define SET_TV_FLICKER_INFORMATION      30012
#define CAN_AUTO_DETECT_TV              30013
#define ENABLE_TV_DISPLAY               30014
#define DISABLE_TV_DISPLAY              30015
#define LINE_BEATING                    30016
#define GET_TV_FLICKER_INFORMATION_EX   30018
#define SET_TV_FLICKER_INFORMATION_EX   30019
#define SAVE_SETTING                    30100
#define LOAD_SETTING                    30101
#define SET_TV_DEFAULT                  30102
#define PASS_DEVHANDLE                  30103
#define SET_BYPASS_CAP                  30200
#define CLEAR_BYPASS_CAP                30201
#define GET_BYPASS_CAP                  30202
#define ENABLE_BYPASS                   30203
#define DISABLE_BYPASS                  30204
#define SET_MV7_PARAM                   30300
#define GET_MV7_INFORMATION             30301
#define SET_DOTCRAWL                    30302
#define GET_DOTCRAWL                    30303
#define CURRENT_1KMODE                  30304
#define GET_CURRENT_MV7_LEVLE           30305
#define INIT_VIDEO_MEMORY               30307
#define GET_NEW_TV_INTERFACE            30308
#define SAVE_PAL_NTSC_TO_CMOS           30500   //Save TV standard to CMOS

#define NON_TV_CONNECTION               0x00000000
#define CONNECT_COMPOSITE               0x00000001
#define CONNECT_SVIDEO                  0x00000002
#define CONNECT_BOTH_CS                 0x00000003

#define CHANGE_ON_HORIZONTAL            0x0001
#define CHANGE_ON_VERTICAL              0x0002
#define CHANGE_ON_BOTH_HV               0x0003
#define CHANGE_ON_LEFT                  0x0000
#define CHANGE_ON_UP                    0x0000
#define CHANGE_ON_RIGHT                 0x0001
#define CHANGE_ON_DOWN                  0x0001
#define SET_DEFAULT_POSITION            0x0004
#define CHANGE_ON_POS                   0x0001
#define CHANGE_ON_NEG                   0x0000
#define CHANGE_ON_ZOOMOUT               0x0001
#define CHANGE_ON_ZOOMIN                0x0000
#define CHANGE_ON_COLOR                 0x0001
#define CHANGE_ON_TINT                  0x0001
#define CHANGE_ON_SATURATION            0x0002
#define CHANGE_ON_TEXTENHANCE           0x0010

typedef struct {
    CARD16                 xRes;
    CARD16                 yRes;
    CARD16                 color;
    XGIDigitalTVInfoRec    nInfo[6];
    XGIDigitalTVInfoRec    pInfo[6];
} XGIDigitalTVModeRec, *XGIDigitalTVModePtr;

extern void XG47SetDefaultFlickerValue(XGIPtr pXGI, CARD16 i);
extern Bool XG47GetTVFlickerInformation(XGIPtr pXGI,
                                        XGIDigitalTVRec *pDtv, CARD32 index);
extern Bool XG47SetTVFlickerInformation(XGIPtr pXGI,
                                        XGIDigitalTVRec *pDtv, CARD32 index);
extern CARD16 XG47GetFlickerIndex();
extern CARD16 MV7SetRegisters(XGIPtr pXGI, CARD16 aps);
extern CARD16 MV7GetInformation();
extern CARD16 MV7GetLevel();
extern Bool XG47EnableTV(XGIPtr pXGI, unsigned long enableTV);
extern unsigned long XG47IsTVConnected(XGIPtr pXGI);
extern void XG47DelayTimer(XGIPtr pXGI, CARD16 loop);
extern void XG47SetExternalRegister(XGIPtr pXGI, CARD16 index, CARD16 value);
extern CARD16 XG47GetExternalRegister(XGIPtr pXGI, CARD16 index);
extern XGIDigitalTVInfoRec *XG47GetCurrentTable(XGIPtr pXGI,
                                                CARD16 xRes, CARD16 yRes, CARD16 color);
extern unsigned long XG47DetectTVConnection(XGIPtr pXGI);
extern void XG47InitTVScreen(XGIPtr pXGI, CARD16 xRes,
                             CARD16 yRes, CARD16 color, CARD16 attr);
extern void XG47ConvertTVMode(XGIPtr pXGI, CARD16 isPAL);
extern void XG47UpdateCMOS(XGIPtr pXGI, CARD16 tvMode);
extern void XG47LineBeating(XGIPtr pXGI, Bool ctrl);
extern void XG47ControlTVDisplay(XGIPtr pXGI, Bool ctrl);
extern CARD32 XG47GetTVColorItems();
extern Bool XG47GetTVColorInformation(XGIPtr pXGI,
                                      XGIDigitalTVRec *pDtv, CARD32 index);
extern Bool XG47SetTVColorInformation(XGIPtr pXGI,
                                      XGIDigitalTVRec *pDtv, CARD32 index);
extern Bool XG47GetTVScreenPosition(XGIPtr pXGI,
                                    XGIDigitalTVRec *pDtv, CARD32 index);
extern Bool XG47SetTVScreenPosition(XGIPtr pXGI,
                                    XGIDigitalTVRec *pDtv, CARD32 index);
extern void XG47ChipType();
extern void XGI47BiosAttachDTV(XGIPtr pXGI);

#endif
