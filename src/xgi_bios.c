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

#include <assert.h>

#include "xgi.h"
#include "xgi_regs.h"
#include "xgi_bios.h"
#include "xgi_mode.h"
#include "xg47_bios.h"
#include "xg47_mode.h"

static void XGIBiosGetFramebufferSize(XGIPtr pXGI);

extern XGIPixelClockRec XG47ModeVClockTable;
extern int XG47ModeVClockTableSize;
extern XGIPixelClockRec XG47ModeVClockTable2;
extern int XG47ModeVClockTableSize2;

CARD8       vclk18;
CARD8       vclk19;
CARD8       vclk28;
CARD8       GR3CE_45;
CARD8       GR3CE_45_SingleView;

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
                           const XGIAskModeRec *pMode0,
                           const XGIAskModeRec *pMode1,
                           unsigned refRate)
{
    unsigned crtbw = 0;
    unsigned w2bw = 0;
    Bool flag = TRUE;


    assert(pMode0 != NULL);

    if (pMode1 != NULL) {
        if (pMode1->condition & DEV_SUPPORT_LCD) {
            w2bw = XGIGetVClock_BandWidth(pXGI,
                                          pXGI->lcdWidth, pXGI->lcdHeight,
                                          pMode1->pixelSize,
                                          pMode1->refRate,
                                          ST_DISP_LCD);

            /* Enable interpolation if the panel does not support centering
             * and the native resolution of the LCD, in either dimension, is
             * larger than the resolution selected.
             */
            pXGI->isInterpolation = 
                (((pMode1->condition & SUPPORT_PANEL_CENTERING) == 0)
                 && ((pXGI->lcdWidth > pMode1->width) 
                     || (pXGI->lcdHeight > pMode1->height)));
        } else {
            /* DVI on W2 */
            w2bw = XGIGetVClock_BandWidth(pXGI,
                                          pMode1->width, pMode1->height,
                                          pMode1->pixelSize,
                                          pMode1->refRate,
                                          (pMode0->condition & 0x0F));
        }

        /* XG47 W2 can not support above 1920 modes with 32bits color under
         * 250/250 275/275; only don't support 20x15x32 under 300/300.
         */
        flag = (((pXGI->lcdWidth < 1920) && (pMode1->width < 1920)) 
                || (pMode1->pixelSize < 32));
    }

    crtbw = XGIGetVClock_BandWidth(pXGI,
                                   pMode0->width, pMode0->height,
                                   pMode0->pixelSize, refRate,
                                   (pMode0->condition & 0x0F))
        * (pMode0->pixelSize / 8);

    if (!flag || ((crtbw == 0) && (w2bw == 0))) {
        return FALSE;
    }

    /* If there is sufficient bandwidth to drive both displays, we win.
     */
    if ((crtbw + w2bw) < pXGI->maxBandwidth) {
        return TRUE;
    }

    /* Check bandwidth again with interpolation disabled on LCD display. */
    if (((crtbw+w2bw/2) < pXGI->maxBandwidth)
        && (pMode1->condition & DEV_SUPPORT_LCD) && pXGI->isInterpolation) {
        pXGI->isInterpolation = FALSE;
        return TRUE;
    }

    return FALSE;
}

#ifndef NATIVE_MODE_SETTING
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

void XGIBiosGetFramebufferSize(XGIPtr pXGI)
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
    const XGIModeRec *const pMode = XG47GetModeFromRes(width, height);

    return (pMode == NULL) ? 0 : pMode->modeNo;
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
void XGIGetSetChipSupportDevice(XGIPtr pXGI, Bool reset_to_original)
{
    /* Jong 09/12/2006; XGI_REG_CRX is 0x03D4 */
    /* 0xC2 : TV Status flag 2 */
    OUTB(XGI_REG_CRX, 0xC2);

    if ((INB(XGI_REG_CRX+1)) & 0x40) /* Jong 09/12/2006; Yes */
        pXGI->biosDevSupport    &= ~SUPPORT_CURRENT_NO_TV;
    else
        pXGI->biosDevSupport    |= SUPPORT_CURRENT_NO_TV;

    if (reset_to_original) {
        /* Reset orignal support. */
        pXGI->biosDevSupport = pXGI->biosOrgDevSupport;
    }

    return;
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

    /* If success, ah = 0 */
    if ((pXGI->pInt10->ax >> 8) || (pXGI->biosBase != NULL)) {
        return FALSE;
    }

    memcpy(array, (pXGI->biosBase + pXGI->pInt10->di), pXGI->pInt10->cx);
    return TRUE;
}

/*
 * from bios dll: modeset.c
 */
void XGICloseSecondaryView(XGIPtr pXGI)
{
    CARD16    i;

    vAcquireRegIOProtect(pXGI);

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
#endif

/*
 * from bios dll: initial.c
 */
unsigned XGIBiosCalculateClock(XGIPtr pXGI, unsigned low, unsigned high)
{
    const unsigned N = IN3C5B(low);
    const unsigned tmp = IN3C5B(high);
    const unsigned K = (tmp & 0xC0) >> 6;
    const unsigned M = (tmp & 0x3F);

    return ((1431818 * (N + 8)) / ((M + 1) << K)) / 100000;
}

#ifndef NATIVE_MODE_SETTING
/*
 * below is bios call
 */
static Bool XGIBiosDllPunt(XGIPtr pXGI, unsigned long cmd,
                           const unsigned long *pInBuf)
{
    ScrnInfoPtr pScrn = pXGI->pScrn;

    if (pXGI->pBiosDll->biosSpecialFeature == NULL) {
        return FALSE;
    }

    return (*pXGI->pBiosDll->biosSpecialFeature)(pScrn, cmd, pInBuf);
}
#endif

/*
 * Init bios dll
 */
Bool XGIBiosDllInit(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
    CARD8   idxbak;

    (void) memset(pXGI->pBiosDll, 0, sizeof(*pXGI->pBiosDll));

    switch (pXGI->chipset) {
    case XG47:
#ifndef NATIVE_MODE_SETTING
        pXGI->pBiosDll->biosValidMode           = XG47BiosValidMode;
        pXGI->pBiosDll->biosModeInit            = XG47BiosModeInit;
        pXGI->pBiosDll->biosSpecialFeature      = XG47BiosSpecialFeature;
        pXGI->pBiosDll->biosValueInit           = XG47BiosValueInit;
#endif

        pXGI->lcdRefRate = 0x003C;    /* 60Hz */

        pXGI->lcdType = FPTYPE_UNKNOWN;
        pXGI->lcdWidth = FPSIZE_UNKNOWN;
        pXGI->lcdHeight = FPSIZE_UNKNOWN;

        pXGI->digitalWidth = FPSIZE_UNKNOWN;
        pXGI->digitalHeight = FPSIZE_UNKNOWN;

        pXGI->biosDllOperationFlag  = 0;

        XG47GetFramebufferSize(pXGI);
        break;
    default:
#ifndef NATIVE_MODE_SETTING
        XGIBiosGetFramebufferSize(pXGI);
#endif
        break;
    }


    /* check BIOS capability */
    pXGI->biosCapability = 0;

#ifndef NATIVE_MODE_SETTING
    pXGI->pInt10->ax = 0x1290;
    pXGI->pInt10->bx = 0x0014;
    pXGI->pInt10->dx = 0;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    pXGI->biosCapability = ((CARD32)pXGI->pInt10->bx << 16) | pXGI->pInt10->dx;


    pXGI->pInt10->ax = 0x1280;
    pXGI->pInt10->bx = 0x0014;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    pXGI->pInt10->bx &= 0x7FFF;         /* Clear Master/Slave mode flag. */
    switch(pXGI->pInt10->bx)
    {
    case 0x0003: /* TV_TVX2 */
        pXGI->pBiosDll->dtvType = DTV_TVEXPRESS_XP4E; /* TV_TVX internal */
        pXGI->dtvInfo = TV_TVXI;

        XGI47BiosAttachDTV(pXGI);
        break;
    default:
        pXGI->dtvInfo = 0xFFFFFFFF;
        break;
    }


    /*
     * Get display device
     */
    XGIGetFlatPanelType(pXGI);
    XGIGetFlatPanelSize(pXGI);

    XGIBiosValueInit(pXGI);
#else
    pXGI->dtvInfo = 0xFFFFFFFF;
#endif

    return TRUE;
}


#ifndef NATIVE_MODE_SETTING
/* Jong 1109/2006; pMode[]->condition indicate which device needs to be open */
Bool XGIBiosModeInit(ScrnInfoPtr pScrn, XGIAskModePtr pMode, Bool dualView)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    unsigned long   devices;
    Bool success;
    ModeStatus status;


    /* Ask BIOS if the mode is supported */
    status = (*pXGI->pBiosDll->biosValidMode)(pScrn, pMode, dualView);
    if (status != MODE_OK) {
	xf86DrvMsg(pXGI->pScrn->scrnIndex, X_ERROR,
		   "%s:%u: mode failed %d\n", __func__, __LINE__, status);
        return FALSE;
    }

    /* close all device before set mode */
    devices = 0xF;
    XGIBiosCloseAllDevice(pXGI, &devices);

    if (!dualView) {
        /* Single View mode. */
        success = (*pXGI->pBiosDll->biosModeInit)(pScrn, pMode, 0);

        /* Enable device pMode[0].condition: 0x02:CRT; 0x08:DVI 
         */
        devices = pMode[0].condition;
    } else {
        /* argument 3 : 1 means first view of dual view mode, 2 means
         * second view of dual view mode.
         */
        success = (*pXGI->pBiosDll->biosModeInit)(pScrn, pMode,
                                                  (pXGI->FirstView) ? 1 : 2);
    }

    if (!success) {
        return FALSE;
    }

    XGIBiosOpenAllDevice(pXGI, &devices);
    return TRUE;
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

Bool XGIBiosCloseAllDevice(XGIPtr pXGI, unsigned long* pDevices)
{
    return XGIBiosDllPunt(pXGI, CLOSE_ALL_DEVICE, pDevices);
}

Bool XGIBiosOpenAllDevice(XGIPtr pXGI, unsigned long* pDevices)
{
    return XGIBiosDllPunt(pXGI, OPEN_ALL_DEVICE, pDevices);
}
#endif
