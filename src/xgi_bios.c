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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xgi.h"
#include "xgi_regs.h"
#include "xgi_bios.h"
#include "xgi_mode.h"
#include "xg47_bios.h"

extern XGIPixelClockRec XG47ModeVClockTable;
extern int XG47ModeVClockTableSize;
extern XGIPixelClockRec XG47ModeVClockTable2;
extern int XG47ModeVClockTableSize2;
extern XGIModeRec XG47ModeTable;
extern int XG47ModeTableSize;

CARD8       vclk18;
CARD8       vclk19;
CARD8       vclk28;
CARD8       GR3CE_45;
CARD8       GR3CE_45_SingleView;
CARD8       value[23];

/* Jong 09/21/2006; support dual view */
extern ScreenPtr g_pScreen;

/* Jong 10/04/2006; support different modes for dual view */
extern XGIAskModeRec	g_ModeOfFirstView;

/*
 * from bios dll: Basefunc.c
 */
CARD16 XGIGetVClock_BandWidth(XGIPtr pXGI,
                              CARD16 xres,
                              CARD16 yres,
                              CARD16 depth,
                              CARD16 refRate,
                              CARD8 flag)
{
    XGIPixelClockPtr    pPixelClk;
    int                 i;

    if(flag & DEV_SUPPORT_LCD)
    {
        /* In reduced blanking mode, pixel clock is lower then normal. */
        if(pXGI->lcdWidth == 1280 && pXGI->lcdHeight == 600)
            return(65);
        else if(pXGI->lcdWidth == 1280 && pXGI->lcdHeight == 800)
            return(71);
        else if(pXGI->lcdWidth == 1400 && pXGI->lcdHeight == 1050)
            return(108);
        else if(pXGI->lcdWidth == 1440 && pXGI->lcdHeight == 900)
            return(96);
        else if(pXGI->lcdWidth == 1920 && pXGI->lcdHeight == 1200)
            return(154);
    }

    if ((((IN3CFB(0xB8) & 0x80) == 0x80) && ((IN3X5B(0x5A) & 0x03) == 0x03)
                                         && (flag & DEV_SUPPORT_CRT))
     || (((IN3X5B(0x5A) & 0x0C) == 0x0c) && ((IN3CFB(0xB8) & 0x80) == 0x80)
                                         && (flag &  DEV_SUPPORT_DVI)))
    {
        /*
         * if digital monitor && enabled reduced blank timing support
         * we may need seperate flag of enable reduced blank timing
         * support for two monitors.
         */
        pPixelClk = &XG47ModeVClockTable2;

        for (i = 0; i < XG47ModeVClockTableSize2; i++)
        {
            if (pPixelClk[i].xres == xres
             && pPixelClk[i].yres == yres
             && pPixelClk[i].vref == refRate)

                return(pPixelClk[i].vclk);
        }
    }

    pPixelClk = &XG47ModeVClockTable;
    for (i = 0; i < XG47ModeVClockTableSize; i++)
    {
        if (pPixelClk[i].xres == xres
             && pPixelClk[i].yres == yres
             && pPixelClk[i].vref == refRate)

                return(pPixelClk[i].vclk);
    }
    return 0;
}

Bool XGICheckModeSupported(XGIPtr pXGI,
                           XGIAskModePtr pMode0,
                           XGIAskModePtr pMode1,
                           CARD16 refRate)
{
    CARD16          crtbw, w2bw;
    CARD8           flag;

    crtbw = XGIGetVClock_BandWidth(pXGI,
                                   pMode0->width, pMode0->height,
                                   pMode0->pixelSize, refRate,
                                   (CARD8)(pMode0->condition & 0x0F));

    if (crtbw == 0) return FALSE;

    flag = 1;
    if (pMode1)
    {
        if (pMode1->condition & DEV_SUPPORT_LCD)
        {
            w2bw = XGIGetVClock_BandWidth(pXGI,
                                          pXGI->lcdWidth,
                                          pXGI->lcdHeight,
                                          pMode1->pixelSize,
                                          (CARD16)(pMode1->refRate),
                                          1);

            if ((pMode1->condition & SUPPORT_PANEL_CENTERING)
                 || ((pXGI->lcdWidth <= pMode1->width) && (pXGI->lcdHeight <= pMode1->height)))
            {
                /* NO expansion or interpoplation */
                pXGI->isInterpolation = FALSE;
            }
            else
            {
                /* expansion and interpolation */
                pXGI->isInterpolation = TRUE;
            }
        }
        else    /* DVI on W2 */
        {

            w2bw = XGIGetVClock_BandWidth(pXGI,
                                          pMode1->width, pMode1->height,
                                          pMode1->pixelSize,
                                          (CARD16)(pMode1->refRate),
                                          (CARD8)(pMode0->condition & 0x0F));
            /* XG47's TMDS may support up to 165Mhz
            if(w2bw >= 135) flag = 0;
            */
        }
        /* XG47 W2 can not support above 1920 modes with 32bits color under 250/250 275/275; 
	 * only don't support 20x15x32 under 300/300
	 */
        if (((pXGI->lcdWidth >= 1920) || (pMode1->width >= 1920)) && (pMode1->pixelSize == 32))
        {
            flag = 0;
        }
    }
    else
        w2bw = 0;

    if (pMode0)
        crtbw *= (pMode0->pixelSize/8);

    if (flag == 0) return FALSE;
    if ((crtbw == 0) && (w2bw == 0)) return FALSE;

    if ((crtbw + w2bw) < pXGI->maxBandwidth)
        return TRUE;
    /* Check bandwidth again with interpolation disabled on LCD display. */
    else if (((crtbw+w2bw/2) < pXGI->maxBandwidth)
             && (pMode1->condition & DEV_SUPPORT_LCD)
             && pXGI->isInterpolation)
    {
        pXGI->isInterpolation = FALSE;
        return TRUE;
    }
    return FALSE;
}

/*
 *
 */
void XGIGetFlatPanelSize(XGIPtr pXGI)
{
    CARD8   index_save;

    pXGI->pInt10->ax = 0x120C;
    pXGI->pInt10->bx = 0x0014;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    pXGI->lcdRefRate = (CARD16)XG47GetRefreshRateByIndex((CARD8)((pXGI->pInt10->cx>>8) & 0x0F));
    /*pXGI->lcdRefRate |= (pXGI->pInt10->cx & 0x0F00);*/   /* Used for BW Check later. */

    pXGI->lcdWidth = pXGI->pInt10->bx;
    pXGI->lcdHeight = pXGI->pInt10->dx;

    pXGI->lcdModeNo = XGIConvertResToModeNo(pXGI->lcdWidth, pXGI->lcdHeight);

    /* digital(single sync) display */

    index_save = INB(XGI_REG_GRX);
    OUTB(XGI_REG_GRX, 0x41);
    switch (INB(XGI_REG_GRX+1) & 0xF0 >> 4)
    {
        case 1:
            pXGI->digitalWidth = MONITOR_X640;
            pXGI->digitalHeight = MONITOR_Y480;
            break;

        case 2:
            pXGI->digitalWidth = MONITOR_X800;
            pXGI->digitalHeight = MONITOR_Y600;
            break;

        case 3:
            pXGI->digitalWidth = MONITOR_X1024;
            pXGI->digitalHeight = MONITOR_Y768;
            break;

        case 5:
            pXGI->digitalWidth = MONITOR_X1280;
            pXGI->digitalHeight = MONITOR_Y1024;
            break;

        case 8:
            pXGI->digitalWidth = MONITOR_X1600;
            pXGI->digitalHeight = MONITOR_Y1200;
            break;
    }

    pXGI->digitalModeNo = XGIConvertResToModeNo(pXGI->digitalWidth, pXGI->digitalHeight);
    OUTB(XGI_REG_GRX, index_save);
}

void XGIGetFlatPanelType(XGIPtr pXGI)
{
    pXGI->lcdType = FPTYPE_TFT;
}

void XGIGetFramebufferSize(XGIPtr pXGI)
{
    pXGI->pInt10->ax = 0x1200;
    pXGI->pInt10->bx = 0x0012;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    pXGI->biosFbSize = pXGI->pInt10->ax;

    if(pXGI->biosFbSize ==0) pXGI->biosFbSize = 1;

    pXGI->biosFbSize *= 0x100000;

    pXGI->freeFbSize = pXGI->biosFbSize;
}

CARD8 XGIConvertResToModeNo(CARD16 width, CARD16 height)
{
    XGIModePtr  pMode;
    int         i;

    pMode = &XG47ModeTable;
    for (i = 0; i < XG47ModeTableSize; i++)
    {
        if (pMode[i].width == width && pMode[i].height == height)
            return(CARD8)(pMode[i].modeNo & 0x7F);
    }
    return 0;
}

void XGIWaitVerticalOnCRTC1(XGIPtr pXGI, CARD16 count)
{
    CARD16 i, loop;

    for(loop = 0; loop < count; loop++)
    {
        for(i=0; i < 0xffff; i++)
            if (!((CARD8)INB(0x3DA) & 0x08)) break;
        for(i=0; i < 0xffff; i++)
            if ((CARD8)INB(0x3DA) & 0x08) break;
    }
}

void XGIWaitVerticalOnCRTC2(XGIPtr pXGI, CARD16 count)
{
    CARD16 i, loop;

    for(loop = 0; loop < count; loop++)
    {
        for(i = 0; i < 0xffff; i++)
        {
            OUTB(XGI_REG_SRX, 0xDC);
            if (!(INB(XGI_REG_SRX+1) & 0x01)) break;
        }
        for(i = 0; i < 0xffff; i++)
        {
            OUTB(XGI_REG_SRX, 0xDC);
            if (INB(XGI_REG_SRX+1) & 0x01) break;
        }
    }
}

/*
 * from bios dll: refresh.c
 */

/*
 * Check refresh rates supported by specified video mode.
 *
 * modeNo - BIOS mode number  (See BIOS spec for definition)
 * spec  -  Mode Specifier[15:8]  (See BIOS spec: Set mode with refresh rate for definition)
 * Return:
 *   refresh rates supported by specified video mode (See BIOS spec: Get refresh rate support for definition)
 */

CARD16 XGIGetRefreshRateCapability(XGIPtr pXGI, CARD16 modeNo, CARD16 spec)
{
    CARD16  refRateCaps = 0;

    pXGI->pInt10->ax = 0x1201;
    pXGI->pInt10->bx = (modeNo << 8) + 0x14;
    pXGI->pInt10->cx = spec << 8;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    if ((pXGI->pInt10->ax & 0xFF00) == 0)
        refRateCaps = pXGI->pInt10->bx;

    return refRateCaps;
}

/*
 * Check specified refresh rate index and support capability.
 *
 * Entry : refCaps, refresh rate support capability bits.
 *         refIndex, refresh rate index.
 * Return: TRUE/FALSE.
 */
Bool XGICheckRefreshRateSupport(CARD16 refCaps, CARD8 refIndex)
{
    refIndex--;
    refCaps >>= refIndex & 0x0F;
    return (Bool)(refCaps & 0x0001);
}

/*
 * from bios dll: status.c
 */

/*
 * Get current display attributes.
 *
 * Entry : displayDevice - bit3~0: DVI/TV/CRT/LCD
 * Bit
 *     [23:16]  TV information
 *     [15:8]   panel type and configuration
 *      [7:0]   device type
 * Return:
 *     current display attribute for specified device
 *     (See the definition of the condition member in ModeInformation structure)
 */
CARD32 XGIGetDisplayAttributes(XGIPtr pXGI, CARD32 displayDevice)
{
    CARD8    reg0;

    /* Check LCD additional information.*/
    if (displayDevice & DEV_SUPPORT_LCD)
    {
        /* Read LCD Type. */
		/* Jong 09/12/2006; XGI_REG_GRX is 0x03CE; 0x42 : TFT Panel Type Control */
        OUTB(XGI_REG_GRX, 0x42);
        reg0 = (CARD8)INB(XGI_REG_GRX + 1);
        if(reg0 & 0x80)
        {
            displayDevice &= ~ZVMX_ATTRIB_DSTN;
            displayDevice |= ZVMX_ATTRIB_TFT;
        }
        else
        {
            displayDevice &= ~ZVMX_ATTRIB_TFT;
            displayDevice |= ZVMX_ATTRIB_DSTN;
        }

        /* Centering & Expension */
		/* Jong 09/12/2006; 0x5D : Miscellaneous register */
        OUTB(XGI_REG_GRX, 0x5D);
        reg0 = (CARD8)INB(XGI_REG_GRX + 1);
        if ((reg0 & GRAF_EXPANSION))
        {
            displayDevice &= ~(ZVMX_ATTRIB_V_EXPANSION+ZVMX_ATTRIB_CENTERING);
            displayDevice |= ZVMX_ATTRIB_EXPANSION;
        }
        else if (reg0 & GRAF_V_EXPANSION)
        {
            displayDevice &= ~(ZVMX_ATTRIB_EXPANSION+ZVMX_ATTRIB_CENTERING);
            displayDevice |= ZVMX_ATTRIB_V_EXPANSION;
        }
        else
        {
            displayDevice &= ~(ZVMX_ATTRIB_V_EXPANSION+ZVMX_ATTRIB_EXPANSION);
                displayDevice |= ZVMX_ATTRIB_CENTERING;
        }
    }

    /* Check TV additional information. */
    if (displayDevice & DEV_SUPPORT_TV)
    {
        /* Check TV overscan & underscan. */
        OUTB(XGI_REG_CRX, 0xC2);
        reg0 = (CARD8)INB(XGI_REG_CRX + 1);
        if ((reg0 & 0x01))
        {
            displayDevice &= ~ZVMX_ATTRIB_UNDER;
            displayDevice |= ZVMX_ATTRIB_OVER;
        }
        else
        {
            displayDevice &= ~ZVMX_ATTRIB_OVER;
            displayDevice |= ZVMX_ATTRIB_UNDER;
        }

        if ((reg0 & 0x10))
        {
            displayDevice |= ZVMX_ATTRIB_NATIVE;
        }
        else
        {
            displayDevice &= ~ZVMX_ATTRIB_NATIVE;
        }


        /* Check TV PAL & NTSC. */
        OUTB(XGI_REG_CRX, 0xC0);
        reg0 = (CARD8)INB(XGI_REG_CRX+1) & 0xE0;
        if(reg0 == 0x80)
        {
            displayDevice &= ~ZVMX_ATTRIB_NTSC;    /*PAL*/
            displayDevice &= ~ZVMX_ATTRIB_NTSCJ;
            displayDevice |= ZVMX_ATTRIB_PAL;
            displayDevice &= ~ZVMX_ATTRIB_PALM;
        }
        else if(reg0 == 0x40)
        {
            displayDevice &= ~ZVMX_ATTRIB_PAL;    /*PAL_M*/
            displayDevice &= ~ZVMX_ATTRIB_NTSC;
            displayDevice &= ~ZVMX_ATTRIB_NTSCJ;
            displayDevice |= ZVMX_ATTRIB_PALM;
        }
        else if(reg0 == 0x20)
        {
            displayDevice &= ~ZVMX_ATTRIB_PAL;    /*NTSC-J*/
            displayDevice &= ~ZVMX_ATTRIB_NTSC;
            displayDevice |= ZVMX_ATTRIB_NTSCJ;
            displayDevice &= ~ZVMX_ATTRIB_PALM;
        }
        else
        {
            displayDevice &= ~ZVMX_ATTRIB_PAL;    /*NTSC*/
            displayDevice |= ZVMX_ATTRIB_NTSC;
            displayDevice &= ~ZVMX_ATTRIB_NTSCJ;
            displayDevice &= ~ZVMX_ATTRIB_PALM;
        }
    }
    return displayDevice;
}

/*
 * Get current display attributes.
 *
 * Input:
 *   no - 0: return display attributes of all devices (TV/LCD/CRT/DVI)
 *            (Wu Yun: CRT/DVI currently does not have display attributes)
 *   no - 1: return display attributes of LCD device
 *
 * Return:
 *    current display attributes
 *    (See the definition of the condition member in ModeInformation structure)
 *
 */
CARD32 XGIGetDisplayStatus(XGIPtr pXGI, CARD8 no)
{
    CARD8    reg0;
    CARD32   displayDevice;

    if(no)
    {
        displayDevice = DEV_SUPPORT_LCD;
        displayDevice |= XGIGetDisplayAttributes(pXGI, displayDevice);
    }
    else
    {
		/* Jong 09/12/2006; collect information of all devices */
		/* Call XGIGetDisplayAttributes() to fill device attributes defined in xgi_bios.h */
        displayDevice = 0x0000000F;
        displayDevice |= XGIGetDisplayAttributes(pXGI, displayDevice);

        displayDevice &= 0x00FFFF00;

		/* Jong 09/14/2006; Working status register of device; CRT:0x02; DVI:0x08 */
        OUTB(XGI_REG_GRX, 0x5B);
        reg0 = INB(XGI_REG_GRX+1);

        displayDevice |= (CARD32)(reg0 & 0x0F);

		/* Jong 09/14/2006; why? shitft to reserved bis */
        displayDevice |= (CARD32)(reg0 & 0xF0) << 20;

		/* Jong 09/14/2006; check if MHS (Multiple head support) */
        OUTB(XGI_REG_GRX, 0x36);
        if(INB(XGI_REG_GRX+1) & 0x02)
        {
			/* Jong 09/14/2006; Extended/Standard set mode method*/
            displayDevice |= 0x80000000;
        }
    }
    return displayDevice;
}

/*
 * Get support device status.
 *
 * Entry : NONE.
 * Return: BIOS support display status.
 *
 *  DEVICE(Sec) TV       LCD   DEVICE(Pri)
 * |--------'--------|--------'--------|
 *  |||||||| |||||||| |||||||| ||||||||
 *  |||||||| |||||||| |||||||| |||||||'- LCD only.
 *  |||||||| |||||||| |||||||| ||||||'-- CRT only.
 *  |||||||| |||||||| |||||||| |||||'--- TV.
 *  |||||||| |||||||| |||||||| ||||'---- DVI.
 *  |||||||| |||||||| |||||||| |||'-----
 *  |||||||| |||||||| |||||||| ||'------
 *  |||||||| |||||||| |||||||| |'-------
 *  |||||||| |||||||| |||||||| '--------
 *  |||||||| |||||||| |||||||'----------
 *  |||||||| |||||||| ||||||'-----------
 *  |||||||| |||||||| |||||'------------ Expansion.
 *  |||||||| |||||||| ||||'------------- Centering.
 *  |||||||| |||||||| ''''--------------
 *  |||||||| |||||||'------------------- NTSC.
 *  |||||||| ||||||'-------------------- PAL.
 *  |||||||| |||||'--------------------- underscan.
 *  |||||||| ||||'---------------------- overscan.
 *  |||||||| |||`----------------------- native mode.
 *  |||||||| ||`------------------------ TV chip not available.
 *  |||||||| |`------------------------- PAL-M
 *  |||||||| `-------------------------- NTSC-J
 *  |||||||'---------------------------- LCD.
 *  ||||||'-----------------------------
 *  |||||'------------------------------
 *  ||||`------------------------------- DVI.
 *  |```-------------------------------- reserved.
 *  `----------------------------------- 0: Contain, 1:MHS
 *
 * Current device(Primary/Secondary) status use same definition
 */
CARD32 XGIGetCurrentDeviceStatus(XGIPtr pXGI)
{
    CARD8    reg0;
    CARD32   retValue;

    retValue = 0x0;
    OUTB(XGI_REG_GRX, 0x5B);
    reg0 = INB(XGI_REG_GRX+1);

    retValue = (CARD32) ((reg0 & 0x0F) | ((reg0 & 0xF0) >> 4));
    retValue |= XGIGetDisplayAttributes(pXGI, retValue);

    retValue &= 0x00FFFF00;
    retValue |= (CARD32)(reg0 & 0x0F);
    retValue |= (CARD32)(reg0 & 0xF0) << 20;

    OUTB(XGI_REG_GRX, 0x36);
    if(INB(XGI_REG_GRX+1) & 0x02)
    {
        retValue |= 0x80000000;
    }

    return retValue;
}

/*
 * Get support device status.
 *
 * Entry : NONE.
 * Return: BIOS support display status.
 *
 * Hardware & BIOS support status
 *                                       1:Support  0:Non-support
 *   DEVICE     TV       LCD    DEVICE
 * |--------'--------|--------'--------|
 *  |||||||| |||||||| |||||||| ||||||||
 *  |||||||| |||||||| |||||||| |||||||'- LCD.
 *  |||||||| |||||||| |||||||| ||||||'-- CRT.
 *  |||||||| |||||||| |||||||| |||||'--- TV.
 *  |||||||| |||||||| |||||||| ||||'---- DVI.
 *  |||||||| |||||||| |||||||| |||'-----
 *  |||||||| |||||||| |||||||| ||'------
 *  |||||||| |||||||| |||||||| |'-------
 *  |||||||| |||||||| |||||||| '--------
 *  |||||||| |||||||| |||||||'----------
 *  |||||||| |||||||| ||||||'-----------
 *  |||||||| |||||||| |||||'------------ Expansion.
 *  |||||||| |||||||| ||||'------------- Centering.
 *  |||||||| |||||||| ''''--------------
 *  |||||||| |||||||'------------------- NTSC.
 *  |||||||| ||||||'-------------------- PAL.
 *  |||||||| |||||'--------------------- underscan.
 *  |||||||| ||||'---------------------- overscan.
 *  |||||||| |||`----------------------- native mode.
 *  |||||||| ||`------------------------ TV chip is not available.
 *  |||||||| |`------------------------- Support PAL-M.
 *  |||||||| `-------------------------- Support NTSC-J.
 *  |||||||'---------------------------- LCD.
 *  ||||||'----------------------------- CRT.
 *  |||||'------------------------------ TV.
 *  ||||`------------------------------- DVI.
 *  ````-------------------------------- reserved.
 *
 * Current device(Primary/Secondary) status use same definition
 */
CARD32 XGIGetSetChipSupportDevice(XGIPtr pXGI, CARD8 no, CARD32 device)
{
	/* Jong 09/12/2006; XGI_REG_CRX is 0x03D4 */
	/* 0xC2 : TV Status flag 2 */
    OUTB(XGI_REG_CRX, 0xC2);

    if ((INB(XGI_REG_CRX+1)) & 0x40) /* Jong 09/12/2006; Yes */
        pXGI->biosDevSupport    &= ~SUPPORT_CURRENT_NO_TV;
    else
        pXGI->biosDevSupport    |= SUPPORT_CURRENT_NO_TV;

    switch(no)
    {
    case 0x0:  /* Reset orignal support. */
         pXGI->biosDevSupport = pXGI->biosOrgDevSupport;
         return pXGI->biosOrgDevSupport;
    case 0x2:
    case 0x4:  /* Set support */
         if(device & 0x000C0000)
             device |= 0x000C0000;
         pXGI->biosDevSupport &= 0x00ffff00;
         pXGI->biosDevSupport |= (device & 0xff0000ff);
         if(!(device & DEV_SUPPORT_TV))
         {
             pXGI->biosDevSupport  &= 0xff00ffff;
         }
         if(!(device & DEV_SUPPORT_LCD))
         {
             pXGI->biosDevSupport  &= 0xffff00ff;
         }
         return pXGI->biosDevSupport;
    case 0x3:  /* Get support */
    case 0x1:
    default:
         return pXGI->biosDevSupport;
    }
}

/*
 * from bios dll: modeset.c
 */
CARD16 XGIGetColorIndex(CARD16 depth)
{
    switch(depth)
    {
    case 8:
        return 0x02;
    case 16:
        return 0x06;
    case 32:
        return 0x08;
    default:
        return 0;
    }
}

/*
 * from bios dll: biosdata.c
 */
Bool XGIReadBiosData(XGIPtr pXGI, CARD8 *array)
{
    pXGI->pInt10->ax = 0x1290;
    pXGI->pInt10->bx = 0x0014;
    pXGI->pInt10->dx = 0;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    if (pXGI->pInt10->ax >> 8)           /* If success, ah = 0 */
        return FALSE;

    if (pXGI->biosBase)
    {
        memcpy(array, (pXGI->biosBase + pXGI->pInt10->di), pXGI->pInt10->cx);
        return TRUE;
    }
    else
        return FALSE;
}

/*
 * from bios dll: modeset.c
 */
void XGICloseSecondaryView(XGIPtr pXGI)
{
    CARD16    i;

    OUTW(0x3C4, 0x9211);

    /* clear flag */
	/* Jong 09/20/2006; Working status register 1; 0xF0 is for MV */
    if (IN3CFB(0x5B) & 0xF0)
    {
        if (IN3CFB(0x5B) & (DEV_SUPPORT_DVI << 4))
        {
            if (!(IN3X5B(SOFT_PAD59_INDEX) & 0x02))
            {
                XG47CloseAllDevice(pXGI, DEV_SUPPORT_DVI);

                OUT3X5B(SOFT_PAD59_INDEX, IN3X5B(SOFT_PAD59_INDEX) & ~0x02);
            }

            /* driving strength */
            OUTB(XGI_REG_GRX, 0x98);
            OUTB(XGI_REG_GRX+1, 0x55);
            OUTB(XGI_REG_GRX, 0x7B);
            OUTB(XGI_REG_GRX+1, 0x55);

            /* disable internal TMDS */
            OUTB(XGI_REG_GRX, 0x3d);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x01);
            /*OUTB(XGI_REG_GRX, 0x43);*/
            /*OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x08);*/
            OUTB(XGI_REG_GRX, 0x46);
            OUTB(XGI_REG_GRX+1, (INB(XGI_REG_GRX+1) & ~0x38) | 0x08);

            /* TV VSync input buffer */
            OUTB(XGI_REG_CRX, 0xD6);
            OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) | 0x10);

            OUTB(XGI_REG_GRX, 0x2A);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x40);

            /* Turn back VCLK2 */
            OUTB(XGI_REG_SRX,0x1A);
            OUTB(XGI_REG_SRX+1, vclk18);
            OUTB(XGI_REG_SRX,0x1B);
            OUTB(XGI_REG_SRX+1, vclk19);
            OUTB(XGI_REG_SRX,0x28);
            OUTB(XGI_REG_SRX+1, (INB(XGI_REG_SRX+1) & ~0x70) | vclk28);

        }

        if (IN3CFB(0x5B) & (DEV_SUPPORT_TV << 4))
        {
            if (!(IN3X5B(SOFT_PAD59_INDEX) & 0x02))
            {
                XG47CloseAllDevice(pXGI, DEV_SUPPORT_TV);

                OUTB(XGI_REG_CRX, SOFT_PAD59_INDEX);
                OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) & ~0x02);
            }
            OUTB(XGI_REG_CRX,0xD6);
            OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) & ~0x04);
        }

        if (IN3CFB(0x5B) & (DEV_SUPPORT_CRT << 4))
        {
            if (!(IN3X5B(SOFT_PAD59_INDEX) & 0x02))
            {
                XG47CloseAllDevice(pXGI, DEV_SUPPORT_CRT);

                OUTB(XGI_REG_CRX, SOFT_PAD59_INDEX);
                OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) & ~0x02);
            }
            OUTB(XGI_REG_GRX,0x2C);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x40);

            /* Turn back VCLK2 */
            OUTB(XGI_REG_SRX,0x1a);
            OUTB(XGI_REG_SRX+1, vclk18);
            OUTB(XGI_REG_SRX,0x1b);
            OUTB(XGI_REG_SRX+1, vclk19);
            OUTB(XGI_REG_SRX,0x28);
            OUTB(XGI_REG_SRX+1, INB(XGI_REG_SRX+1) & ~0x70 | vclk28);
        }

        if (IN3CFB(0x5B) & (DEV_SUPPORT_LCD << 4))
        {
            if (!(IN3X5B(SOFT_PAD59_INDEX) & 0x02))
            {
                XG47CloseAllDevice(pXGI, DEV_SUPPORT_LCD);

                OUTB(XGI_REG_CRX, SOFT_PAD59_INDEX);
                OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) & ~0x02);
            }
            OUTB(XGI_REG_SRX,0xBE);
            OUTB(XGI_REG_SRX+1, INB(XGI_REG_SRX+1) & ~0x08);

            /* Change LCD DE delay back for CRTC1 */
            OUTB(XGI_REG_GRX, 0x45);
            OUTB((XGI_REG_GRX+1), (INB(XGI_REG_GRX+1) & ~0x07) | (GR3CE_45_SingleView & 0x07));
        }

        /* Enable dithering */
        OUTB(XGI_REG_GRX, 0x42);
        OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x08);

        OUTB(XGI_REG_SRX, 0xCE);
        OUTB(XGI_REG_SRX+1, INB(XGI_REG_SRX+1) & ~0x02);

        /* Disable dual view */
        OUTB(XGI_REG_SRX, 0xDC);
        for(i=0; i<0xFFFF; i++)
        {
            if ((INB(XGI_REG_SRX+1) & 0x01))
                break;
        }
        OUTB(XGI_REG_GRX, 0x81);
        OUTB(XGI_REG_GRX+1, (int)INB(XGI_REG_GRX+1) & ~0x01);

        /*
         * Please keep the order of below 2 I/O
         *start to turn off video engine pixel clock
         */
        OUTB(XGI_REG_CRX,0xBE);
        OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) & ~0x04);

        /* turn off video engine memory clock */
        OUTB(XGI_REG_GRX,0xDA);
        OUTB(XGI_REG_GRX+1, (int)INB(XGI_REG_GRX+1) | 0x10);
        /* end to turn off video engine pixel clock */

        /* Turn off video engine clock for w2 */
        OUTB(XGI_REG_SRX,0x52);
        OUTB(XGI_REG_SRX+1, INB(XGI_REG_SRX+1) & ~0x40);

        /* clear 2nd display */
        OUTB(XGI_REG_GRX, 0x5B);
        OUTB(XGI_REG_GRX+1, (INB(XGI_REG_GRX+1) & 0x0f));

        OUTB(0x24c2, (CARD16)INB(0x24c2) & ~0x20);
        OUTB(0x24a9, (CARD16)INB(0x24a9) & ~0x0a);
        OUTB(0x24a8, (CARD16)INB(0x24a8) & ~0x07);
    }

    /*
     * Zdu, 9/1/2000, Seems TSB machine did not wait LCD panel to be stabel to do
     * further operation leading a hang when undocking switching device, now we added
     * this wait and hang up disappeared.
     */
    {
        while(INB(0x3DA) & 0x8);
        for(i=0; i < 0xffff; i++)
        {
            if(!(INB(0x3DA) & 0x8))   break;
        }
        while(INB(0x3DA) & 0x8);
        for(i=0; i < 0xffff; i++)
        {
            if(!(INB(0x3DA) & 0x8))   break;
        }
    }
}

/*
 * from bios dll: initial.c
 */
CARD16 XGIBiosCalculateClock(XGIPtr pXGI, CARD8 low, CARD8 high)
{
    CARD16 clock;
    CARD16 K, M, N;

    OUTB(XGI_REG_SRX, low);
    clock = INB(XGI_REG_SRX+1);
    OUTB(XGI_REG_SRX, high);
    clock += INB(XGI_REG_SRX+1) << 8;

    K = 1 << ((clock & 0xC000) >> 14);
    M = (clock & 0x3F00) >> 8;
    N = (clock & 0xFF);
    clock = (CARD16) ((1431818 * (CARD32)(N+8)) / (CARD32)(K*(M+1)) / 100000);

    return clock;
}

/*
 * below is bios call
 */
Bool XGIBiosDllPunt(XGIPtr pXGI,
                    unsigned long cmd,
                    unsigned long *pInBuf,
                    unsigned long *pOutBuf)
{
    ScrnInfoPtr pScrn = pXGI->pScrn;
    Bool        result = FALSE;

    if (pXGI->pBiosDll->biosSpecialFeature)
    {
        if ((*pXGI->pBiosDll->biosSpecialFeature)(pScrn, cmd, pInBuf, pOutBuf) < 0)
        {
            /* DTV functions needed */
            if (pXGI->pBiosDll->biosDtvCtrl)
                return (*pXGI->pBiosDll->biosDtvCtrl)(pScrn, cmd, pInBuf, pOutBuf);
            else /* No DTV support */
                return FALSE;
        }
        else
        {
            /* Already dealt in BiosDll */
            result = TRUE;
        }
    }
    return result;
}

/*
 * Init bios dll
 */
/*Bool XGIBiosDllInit(XGIPtr pXGI)*/
Bool XGIBiosDllInit(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
    CARD8   idxbak;

    switch (pXGI->chipset)
    {
    case XG47:
        pXGI->pBiosDll->biosValidMode           = XG47BiosValidMode;
        pXGI->pBiosDll->biosModeInit            = XG47BiosModeInit;
        pXGI->pBiosDll->biosSpecialFeature      = XG47BiosSpecialFeature;
        pXGI->pBiosDll->biosGetFreeFbSize       = XG47BiosGetFreeFbSize;
        pXGI->pBiosDll->biosValueInit           = XG47BiosValueInit;
        pXGI->pBiosDll->biosDtvCtrl             = NULL;

        pXGI->lcdRefRate = 0x003C;    /* 60Hz */

        pXGI->lcdType = FPTYPE_UNKNOWN;
        pXGI->lcdWidth = FPSIZE_UNKNOWN;
        pXGI->lcdHeight = FPSIZE_UNKNOWN;

        pXGI->digitalWidth = FPSIZE_UNKNOWN;
        pXGI->digitalHeight = FPSIZE_UNKNOWN;

        pXGI->freeFbSize =(((CARD32)0x1)<<23);      /* 8M */
        pXGI->biosFbSize =(((CARD32)0x1)<<23);      /* 8M */

        pXGI->biosDllOperationFlag  = 0;
        break;
    default:
        pXGI->pBiosDll->biosValidMode           = NULL;
        pXGI->pBiosDll->biosModeInit            = NULL;
        pXGI->pBiosDll->biosSpecialFeature      = NULL;
        pXGI->pBiosDll->biosGetFreeFbSize       = NULL;
        pXGI->pBiosDll->biosValueInit           = NULL;
        pXGI->pBiosDll->biosDtvCtrl             = NULL;
        break;
    }

    /*
     * Get Revision ID and Chip ID
     */
    idxbak = (CARD8)INB(0x3C4);
    OUTB(0x3C4, 0xB);
    pXGI->chipID = INB(0x3C5);
    OUTB(0x3C4, 0x9);
    pXGI->chipRev = INB(0x3C5);
    OUTB(0x3C4, idxbak);

    /* check BIOS capability */
    pXGI->biosCapability = 0;

    pXGI->pInt10->ax = 0x1290;
    pXGI->pInt10->bx = 0x0014;
    pXGI->pInt10->dx = 0;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    pXGI->biosCapability = ((CARD32)pXGI->pInt10->bx << 16) | pXGI->pInt10->dx;

    {
        pXGI->pInt10->ax = 0x1280;
        pXGI->pInt10->bx = 0x0014;
        pXGI->pInt10->num = 0x10;
        xf86ExecX86int10(pXGI->pInt10);

        pXGI->pInt10->bx &= 0x7FFF;         /* Clear Master/Slave mode flag. */
        switch(pXGI->pInt10->bx)
        {
        case 0x0003: /* TV_TVX2 */

            pXGI->pBiosDll->biosDtvCtrl = XG47BiosDTVControl;

            pXGI->pBiosDll->dtvType = DTV_TVEXPRESS_XP4E; /* TV_TVX internal */
            pXGI->dtvInfo = TV_TVXI;
            break;
        default:
            pXGI->dtvInfo = 0xFFFFFFFF;
            break;
        }
    }

    switch (pXGI->pBiosDll->dtvType)
    {
    case DTV_TVEXPRESS_XP4E:
        XGI47BiosAttachDTV(pXGI);
        pXGI->pBiosDll->biosDtvCtrl = XG47BiosDTVControl;
        break;
    default:
        pXGI->pBiosDll->biosDtvCtrl = NULL;
        break;
    }

    /*
     * Get display device
     */
    XGIGetFlatPanelType(pXGI);
    XGIGetFlatPanelSize(pXGI);
    XGIGetFramebufferSize(pXGI);

    XGIBiosValueInit(pXGI);

    return TRUE;
}

Bool XGIBiosValidMode(ScrnInfoPtr pScrn,
                      XGIAskModePtr pAskMode,
                      CARD32 dualView)
{
    XGIPtr      pXGI = XGIPTR(pScrn);

    return (pXGI->pBiosDll->biosValidMode)(pScrn, pAskMode, dualView);
}

/* Jong 1109/2006; pMode[]->condition indicate which device needs to be open */
Bool XGIBiosModeInit(ScrnInfoPtr pScrn,
                     XGIAskModePtr pMode,
                     CARD32 dualView,
                     unsigned long device)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    unsigned long   devices = 0;
	int				i;

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-XGIBiosModeInit()-0-pMode->modeNo=0x%x--\n", pMode->modeNo);
#endif

    /* Ask BIOS if the mode is supported */
    if (!XGIBiosValidMode(pScrn, pMode, dualView))   /* not supported */
    {
        return FALSE;
    }

    /* close all device before set mode */
    devices = 0xF;
    XGIBiosCloseAllDevice(pXGI, &devices);

	/* Jong 09/15/2006; single view */
    if (!dualView)
    {
        if (!pXGI->isNeedCleanBuf)
        {
            pMode->modeNo |= 0x80;
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-XGIBiosModeInit()-1-pMode->modeNo=0x%x--\n", pMode->modeNo);
#endif
        }

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-DualView-before (biosModeInit)(pScrn, pMode, 0)----\n");
	XGIDumpRegisterValue(g_pScreen);
#endif
        /* Single View mode. */
		/* Jong 09/14/2006; biosModeInit() is XG47BiosModeInit() */
        if (!(*pXGI->pBiosDll->biosModeInit)(pScrn, pMode, 0))
        {
            return FALSE;
        }
/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-DualView-after (biosModeInit)(pScrn, pMode, 0)----\n");
	XGIDumpRegisterValue(g_pScreen);
#endif

		/* Jong 09/14/2006; enable device pMode[0].condition */
		/* 0x02:CRT; 0x08:DVI */
        devices = pMode[0].condition;

		/* Jong 09/14/2006; why not calling when dual view? */
        XGIBiosOpenAllDevice(pXGI, &devices);
    }
	/* Jong 09/15/2006; dual view */
    else
    {
		/* Jong 09/15/2006; initialize first view; assume DVI */
		if(pXGI->FirstView)
		{
			/* Jong 09/15/2006; ignor condition checking for test */
			/* if (device & pMode->condition) */
			{
				/* Jong 09/14/2006; biosModeInit() is XG47BiosModeInit() */
				/* argument 3 : 1 means first view of dual view mode */
				if (!(*pXGI->pBiosDll->biosModeInit)(pScrn, pMode, 1))
				{
					return FALSE;
				}
/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-DualView-(biosModeInit)(pScrn, pMode, 1)----\n");
	XGIDumpRegisterValue(g_pScreen);
#endif
			}
		}
		/* Jong 09/15/2006; initialize second view; assume CRT */
		else
		{
			/* Jong 09/15/2006; ignor condition checking for test */
			/* if (device & (pMode + 1)->condition) */
			{
				/* Jong 10/04/2006; support different modes for dual view */
				/* if (!(*pXGI->pBiosDll->biosModeInit)(pScrn, &g_ModeOfFirstView, 1))
				{
					return FALSE;
				} */

				/* Jong 09/14/2006; biosModeInit() is XG47BiosModeInit() */
				/* argument 3 : 2 means second view of dual view mode */
				if (!(*pXGI->pBiosDll->biosModeInit)(pScrn, pMode, 2))
				{
					return FALSE;
				} 

/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-DualView-(biosModeInit)(pScrn, pMode, 2)----\n");
	XGIDumpRegisterValue(g_pScreen);
#endif
			}
		}

		/* Jong 09/15/2006; Force to open CRT and DVI for dual view in XG47OpenAllDevice() */
		/* devices = ???; might need an accurate device open */
        XGIBiosOpenAllDevice(pXGI, &devices);
    }

    /* Return with successful status */
    return TRUE;
}

/*
 * get max band width from bios
 */
CARD32 XGIBiosGetMaxBandWidth(XGIPtr pXGI)
{
    unsigned long maxBandwidth;

    if (XGIBiosDllPunt(pXGI,
                      (CARD32)GET_AVAILABLE_BANDWIDTH,
                       NULL,
                      (unsigned long *)&maxBandwidth))
    {
        maxBandwidth &= 0x0000FFFF;
    }
    else
    {
        maxBandwidth  = 0x0000FFFF;     /* Maximum */
    }
    return maxBandwidth;
}

Bool XGIBiosSetMultiViewLoop(XGIPtr pXGI, unsigned long multiLoop)
{
    return XGIBiosDllPunt(pXGI,
                          SET_HOTKEY_LOOP,
                          &multiLoop, NULL);
}

void XGIBiosGetMultiViewLoop(XGIPtr pXGI, unsigned long *pMultiLoop)
{
    unsigned long arrMultiLoop[2];

    arrMultiLoop[0] = GET_HOTKEY_LOOP;
    arrMultiLoop[1] = 0;

    XGIBiosDllPunt(pXGI,
                   GET_HOTKEY_LOOP,
                   NULL,
                   (unsigned long *)&arrMultiLoop[0]);
    *pMultiLoop = arrMultiLoop[1];
}


Bool XGIBiosValidModeBaseRefRate(XGIPtr pXGI,
                                 XGIAskModePtr pAskMode)
{
    ScrnInfoPtr pScrn = pXGI->pScrn;
    Bool        result;
    CARD16      refreshRate = pAskMode->refRate;

    result = XGIBiosValidMode(pScrn, pAskMode, 0);

    if (result &&
        (pAskMode->refRate & 0x00FF) != (CARD16)refreshRate)
    {
        result = FALSE;
    }
    return result;
}

/*
 * Used in Win2K driver for resuming from Hibernation/Standby,
 */
unsigned long XGIBiosValueInit(XGIPtr pXGI)
{
    ScrnInfoPtr pScrn = pXGI->pScrn;
    if (pXGI->pBiosDll->biosValueInit)
        (pXGI->pBiosDll->biosValueInit)(pScrn);
    return TRUE;
}
/*
 * this function is just for DSTN pannel
 */
CARD32 XGIBiosGetFreeFbSize(XGIPtr pXGI, Bool isAvailable)
{
    ScrnInfoPtr pScrn = pXGI->pScrn;
    if (pXGI->pBiosDll->biosGetFreeFbSize)
        return (pXGI->pBiosDll->biosGetFreeFbSize)(pScrn, isAvailable);
    return FALSE;
}

unsigned long XGIBiosGetSupportDevice(XGIPtr pXGI)
{
    unsigned long devSupportInfo = 0;

    if (!XGIBiosDllPunt(pXGI,
                        GET_DEV_SUPPORT_INFORMATION,
                        0,
                        (unsigned long *)&devSupportInfo))
    {
        devSupportInfo = 0xFF;
    }
    return devSupportInfo;
}

void XGIBiosGetCurrentTime(XGIPtr pXGI,
                           unsigned long * pStartTime)
{
    XGIBiosDllPunt(pXGI, GET_CURRENT_TIME, NULL, pStartTime);
}

Bool XGIBiosGetLCDInfo(XGIPtr pXGI,
                       unsigned long * pBuf)
{
    return XGIBiosDllPunt(pXGI,
                          GET_FLAT_PANEL_INFORMATION,
                          0,
                          pBuf);
}

Bool XGIBiosGetLidInfo(XGIPtr pXGI, unsigned long * pBuf)
{
    return XGIBiosDllPunt(pXGI,
                          GET_PANEL_LID_INFORMATION,
                          0,
                          pBuf);
}

Bool XGIBiosGetTVInfo(XGIPtr pXGI,
                      unsigned long * pInBuf,
                      unsigned long * pOutBuf)
{
    return XGIBiosDllPunt(pXGI,
                          GET_TV_PHYSICAL_SIZE,
                          pInBuf,
                          pOutBuf);
}

Bool XGIBiosCloseSecondView(XGIPtr pXGI)
{
    unsigned long canCallBios = 1;
    return XGIBiosDllPunt(pXGI,
                          CLOSE_SECOND_VIEW,
                          (unsigned long*)&canCallBios,
                          NULL);
}

Bool XGIBiosCloseAllDevice(XGIPtr pXGI,
                           unsigned long* pDevices)
{
    return XGIBiosDllPunt(pXGI,
                          CLOSE_ALL_DEVICE,
                          pDevices,
                          NULL);
}

Bool XGIBiosOpenAllDevice(XGIPtr pXGI,
                          unsigned long* pDevices)
{
    return XGIBiosDllPunt(pXGI,
                          OPEN_ALL_DEVICE,
                          pDevices,
                          NULL);
}

Bool XGIBiosSetLineBeating(XGIPtr pXGI,
                           unsigned long* pLineBeating)
{
    return XGIBiosDllPunt(pXGI,
                          LINE_BEATING,
                          pLineBeating,
                          NULL);
}

Bool XGIBiosGetModeVClock(XGIPtr pXGI,
                          unsigned long* pModeVClock,
                          unsigned long  displayDevice,
                          CARD16 screenWidth,
                          CARD16 screenHeight,
                          CARD16 bitsPerPlane,
                          CARD16 frequency)
{
    XGIAskModeRec modeInfo;

    modeInfo.width          = screenWidth;
    modeInfo.height         = screenHeight;
    modeInfo.pixelSize      = bitsPerPlane;
    modeInfo.refRate        = frequency;
    modeInfo.condition      = displayDevice;

    return XGIBiosDllPunt(pXGI,
                          GET_MODE_VCLOCK,
                          (unsigned long*)&modeInfo,
                          pModeVClock);
}

Bool XGIBiosGetMV7Info(XGIPtr pXGI,
                       unsigned long* pOutBuf)
{
    unsigned long inBuf[2];
    return XGIBiosDllPunt(pXGI,
                          GET_MV7,
                          (unsigned long*)&inBuf,
                          pOutBuf);
}
Bool XGIBiosSetMV7Info(XGIPtr pXGI,
                       unsigned long* pInBuf)
{
    unsigned long outBuf = 0;
    return XGIBiosDllPunt(pXGI,
                          SET_MV7,
                          pInBuf,
                          (unsigned long*)&outBuf);
}

/*
 *  Digital TV Information.
 */
static unsigned long XGIBiosGetGradValue(XGIDigitalTVRec* pDtvInfo,
                                         unsigned long maxValue)
{
    unsigned long grad;
    unsigned long val;

    grad = pDtvInfo->maxLevel1 + pDtvInfo->maxLevel2; /* The Grade.*/
    if (0 == grad) return 0; /* No grades. */

    if (0 == pDtvInfo->direction)
        val = pDtvInfo->maxLevel1 - pDtvInfo->delta;
    else
        val = pDtvInfo->maxLevel1 + pDtvInfo->delta;

    if (maxValue != INVALID_VALUE)
        return (maxValue * val) / grad;
    else
        return val;
}

/*
 * Set the grade value to DTV.
 *        maxValue!=INVALID_VALUE: [0, maxValue] --> [0, grad]
 *        maxValue==INVALID_VALUE: [0, grad]
 */
static unsigned long XGIBiosSetGradValue(XGIDigitalTVRec* pDtvInfo,
                                         unsigned long maxValue,
                                         unsigned long setValue)
{
    unsigned long grad;
    unsigned long val;
    unsigned long currVal;

    grad = pDtvInfo->maxLevel1 + pDtvInfo->maxLevel2; /* Flicker Reduction Grad number.*/
    if (0 == grad) return FALSE; /* No filter grades. */

    if (0 == pDtvInfo->direction)
        currVal = pDtvInfo->maxLevel1 - pDtvInfo->delta;
    else
        currVal = pDtvInfo->maxLevel1 + pDtvInfo->delta;

    if (maxValue != INVALID_VALUE)
        val  = (grad * setValue) / maxValue +                   /* Interger part. */
              ((grad * setValue) % maxValue >=maxValue / 2);    /* Fraction part, round in. */
    else
        val  = setValue;

    val  = (val>grad) ? grad : val; /* Out of range? */
    if (val > currVal)
    {
        pDtvInfo->delta     = (CARD16)(val - currVal);
        pDtvInfo->direction = 1;
    }
    else
    {
        pDtvInfo->delta     = (CARD16)(currVal - val);
        pDtvInfo->direction = 0;
    }
    return TRUE;
}

Bool XGIBiosGetTVColorInfo(XGIPtr pXGI, unsigned long *pBrightness, unsigned long *pContrast)
{
    XGIDigitalTVColorRec dtvColor;
    unsigned long        input = 0;

    /* Get three color information. */
    XGIBiosDllPunt(pXGI,
                   GET_TV_COLOR_INFORMATION,
                   (unsigned long *)&input,
                   (unsigned long *)&dtvColor);

    if (dtvColor.ret == 0) return FALSE; /* Calling failure. */

    *pBrightness = XGIBiosGetGradValue((XGIDigitalTVRec*)&(dtvColor.info[1]), 100); /* Brigheness: 0..100. */
    *pContrast   = XGIBiosGetGradValue((XGIDigitalTVRec*)&(dtvColor.info[0]), 100); /* Contrast:   0..100. */
    return TRUE;
}

/*
 *  Set the TV color information.
 */
Bool XGIBiosSetTVColorInfo(XGIPtr pXGI, unsigned long brightness, unsigned long contrast)
{
    XGIDigitalTVColorRec dtvColor;
    unsigned long        output;
    unsigned long        input = 0;
    Bool                 ret;

    /* Read three color information first. */
    ret = XGIBiosDllPunt(pXGI,
                         GET_TV_COLOR_INFORMATION,
                         (unsigned long*)&input,
                         (unsigned long*)&dtvColor);

    if (0 == dtvColor.ret || !ret) return FALSE; /* Calling failure. */

    if (brightness <= 100)
    {
        XGIBiosSetGradValue(&dtvColor.info[1], 100, brightness);
    }
    if (contrast <= 100)
    {
        XGIBiosSetGradValue(&dtvColor.info[0], 100, brightness);
    }
    ret = XGIBiosDllPunt(pXGI,
                         SET_TV_COLOR_INFORMATION,
                         (unsigned long*)&dtvColor,
                         (unsigned long*)&output);
    return ret;
}

Bool XGIBiosGetTVPosition(XGIPtr pXGI, unsigned long *pdwX, unsigned long* pdwY)
{
    XGIDigitalTVPositionRec    dtvPos;
    unsigned long              input;
    Bool                       ret;

    /* Get (x,y) position. */
    input = CHANGE_ON_HORIZONTAL | CHANGE_ON_VERTICAL;
    ret = XGIBiosDllPunt(pXGI,
                         GET_TV_SCREEN_POSITION,
                         (unsigned long*)&input,
                         (unsigned long*)&dtvPos);

    if (!ret || dtvPos.ret == 0) return FALSE; /* Calling failure. */

    *pdwX = XGIBiosGetGradValue(&dtvPos.info[0], INVALID_VALUE);
    *pdwY = XGIBiosGetGradValue(&dtvPos.info[1], INVALID_VALUE);
    return ret;
}

/*
 *  Set the TV Position.
 */
Bool XGIBiosSetTVPosition(XGIPtr pXGI, unsigned long dwX, unsigned long dwY)
{
    XGIDigitalTVPositionRec     dtvPos;
    unsigned long               input;
    unsigned long               output;
    Bool                        ret;

    /* Get (x,y) position. */
    input = CHANGE_ON_HORIZONTAL | CHANGE_ON_VERTICAL;
    ret = XGIBiosDllPunt(pXGI,
                         GET_TV_SCREEN_POSITION,
                         (unsigned long*)&input,
                         (unsigned long*)&dtvPos);

    if (!ret || dtvPos.ret == 0) return FALSE; /* Calling failure. */

    XGIBiosSetGradValue(&dtvPos.info[0], INVALID_VALUE, dwX);
    XGIBiosSetGradValue(&dtvPos.info[1], INVALID_VALUE, dwY);
    dtvPos.ret = CHANGE_ON_HORIZONTAL | CHANGE_ON_VERTICAL;
    ret = XGIBiosDllPunt(pXGI,
                         SET_TV_SCREEN_POSITION,
                         (unsigned long*)&dtvPos,
                         (unsigned long*)&output);
    return ret;
}

/*
 *  Get the TV flicker filter.
 */
unsigned long XGIBiosGetTVFlickerFilter(XGIPtr pXGI)
{
    XGIDigitalTVCallRec     dtvCall;
    unsigned long           input;

    XGIBiosDllPunt(pXGI,
                   GET_TV_FLICKER,
                   (unsigned long*)&input,
                   (unsigned long*)&dtvCall);
    if (!dtvCall.ret) return 0; /* Calling failure. */

    return XGIBiosGetGradValue(&dtvCall.info, 1000); /* Flicker filter: 0..1000. */
}

/*
 *  Set the TV flicker filter.
 */
Bool XGIBiosSetTVFlickerFilter(XGIPtr pXGI, unsigned long dwFlickerFilter)
{
    XGIDigitalTVCallRec     dtvCall;
    unsigned long           output;
    Bool                    ret;

    /* Read the flicker info first. */
    ret = XGIBiosDllPunt(pXGI,
                         GET_TV_FLICKER,
                         (unsigned long*)&output,
                         (unsigned long*)&dtvCall);
    if (!ret || 0 == dtvCall.ret) return FALSE; /* Calling failure. */

    XGIBiosSetGradValue(&dtvCall.info, 1000, dwFlickerFilter); /* Flicker filter: 0..1000 */
    ret = XGIBiosDllPunt(pXGI,
                         SET_TV_FLICKER,
                         (unsigned long*)&dtvCall,
                         (unsigned long*)&output);
    return ret;
}
