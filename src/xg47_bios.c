/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/xgi/xg47_bios.c,v */

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xgi.h"
#include "xgi_regs.h"
#include "xgi_bios.h"
#include "xgi_mode.h"
#include "xg47_tv.h"

extern XGIPixelClockRec XG47ModeVClockTable[];
extern int XG47ModeVClockTableSize;
extern XGIPixelClockRec XG47ModeVClockTable2[];
extern int XG47ModeVClockTableSize2;
extern XGIModeRec XG47ModeTable[];
extern int XG47ModeTableSize;

extern CARD8       vclk18;
extern CARD8       vclk19;
extern CARD8       vclk28;

extern CARD8       GR3CE_45;
extern CARD8       GR3CE_45_SingleView;

extern CARD8       value[23];

/* Jong 11/08/2006 */
extern ScreenPtr g_pScreen;

/*
 * Definition of video mode refresh rate (!!! CAN NOT BE MODIFIED !!!).
 */
const CARD16 XG47RefreshTable[] = {
    0,
    44,                                     // VVMX_INDEX_REF44
    48,                                     // ZVMX_INDEX_REF48
    0xb2,                                   //50i
    0x132,                                  //50r
    50,                                     //50
    0xbc,                                   //60i
    0x13c,                                  //60r
    60,                                     // ZVMX_INDEX_REF60
    70,                                     // ZVMX_INDEX_REF70
    72,                                     // ZVMX_INDEX_REF72
    75,                                     // ZVMX_INDEX_REF75
    85,                                     // ZVMX_INDEX_REF85
    100,                                    // ZVMX_INDEX_REF90
    120,                                    // ZVMX_INDEX_REF95
};

/*
 * from bios dll: refresh.c
 */
/*
 * Convert refresh rate index to value.
 *
 * Entry : index, refresh rate index number.
 *         isInterlace, TRUE for interlace format.
 * Return: refresh rate value.
 */
CARD8 XG47ConvertRefIndexToValue(CARD8 index, Bool isInterlace)
{
    CARD8 value = XG47RefreshTable[index & 0x0F];

    return value;
}

/*
 * Convert refresh rate value to index.
 *
 * Entry : value, refresh rate value.
 * Return: refresh rate index number.
 */
CARD8 XG47ConvertRefValueToIndex(CARD16 value)
{
    CARD8 index;

    for (index = 0; index < VREF_MAX_NUMBER; index ++)
    {
        if (value == (CARD16)(XG47RefreshTable[index]))
        {
            return index;
        }
    }
    return 0;
}

/*
 * Convert refresh rate index to value.
 *
 * Entry : index, refresh rate index number.
 * Return: refresh rate value.
 */

CARD16 XG47GetRefreshRateByIndex(CARD8 index)
{
    return XG47RefreshTable[index];
}

/*
 * from bios dll: somecmd.c
 */
static CARD8 crtc59 = 0x80;

void XG47SetDeviceHotkeyLoop(XGIPtr pXGI, CARD8 yy)
{
    CARD8 temp;

    OUTW(0x3c4, 0x9211);

    OUTB(XGI_REG_GRX,0x36);
    temp = (CARD8)INB(XGI_REG_GRX+1);
    temp &= ~0x04;
    yy <<= 2;
    temp |= yy;
    OUTB(XGI_REG_GRX+1, temp);
}

CARD8 XG47GetDeviceHotkeyLoop(XGIPtr pXGI)
{
    OUTB(XGI_REG_GRX, 0x36);
    return ((INB(XGI_REG_GRX+1) & 0x04)>>2);
}

void XG47WaitLcdPowerSequence(XGIPtr pXGI, CARD8 bNew)
{
    CARD32 i;

    OUTB(XGI_REG_GRX, 0x24);
    if (bNew & 0x10)
    {
        for (i=0; i < 0xfffff; i++)
        {
            /*
             * It should check [3:0] here,
             * but in XP4 Rev. A, backlight control doesn't works
             */
            if((INB(XGI_REG_GRX+1) & 0x07) == 0x07)
                break;
        }
    }
    else
    {
        for (i=0; i < 0xfffff; i++)
        {
            if(!(INB(XGI_REG_GRX+1) & 0x07))
                break;
        }
    }

    OUTB(XGI_REG_GRX, 0x23);
    /* wait active */
    for (i=0; i < 0xfffff; i++)
    {
        if(INB(XGI_REG_GRX+1) & 0x10)
            break;
    }
    /* wait deactive */
    for (i=0; i < 0xfffff; i++)
    {
        if(!(INB(XGI_REG_GRX+1) & 0x10))
            break;
    }
}

void XG47CloseAllDevice(XGIPtr pXGI, CARD8 device2Close)
{
    ScrnInfoPtr pScrn = pXGI->pScrn;
    CARD8       curr, bNew;

    OUTB(XGI_REG_CRX, SOFT_PAD59_INDEX);
    if (pXGI->biosDllOperationFlag & DEVICE_CLOSED)
    {
        /*
         * if devices had been closed already, don't save 3x5.59.bit1
         * again, or you'll always get 3x5.59.bit1 as 1 (0x02)
         */
        crtc59 &= 0x02;                         /* get previos 3x5.59.bit1 */
        crtc59 |= (INB(XGI_REG_CRX+1) & 0xFD);  /* save for open_all (except bit1) */
    }
    else
        crtc59 = (INB(XGI_REG_CRX+1) & 0xFF);    /* save for open_all (all) */

    /* inhibit SMI switching */
    OUTB(XGI_REG_CRX+1, (CARD8)INB(XGI_REG_CRX+1) | 0x02);

    /* CRT display */
    if(device2Close & DEV_SUPPORT_CRT)
    {
        /* DAC power off */
        OUTB(XGI_REG_SRX,0x24);
        OUTB(XGI_REG_SRX+1, (CARD8)INB(XGI_REG_SRX+1) & 0xFE);
    }

    /* TV display */
    if(device2Close & DEV_SUPPORT_TV)
    {
        if((pXGI->biosDevSupport & DEV_SUPPORT_TV)
         && !(pXGI->biosDevSupport & SUPPORT_CURRENT_NO_TV)
         && (pXGI->pBiosDll->biosDtvCtrl))
            (*pXGI->pBiosDll->biosDtvCtrl)(pScrn,
                                           (CARD32)DISABLE_TV_DISPLAY,
                                           (CARD32 *)NULL,
                                           (CARD32 *)NULL);
    }

    /* LCD display */
    if(device2Close & DEV_SUPPORT_LCD)
    {
        OUTB(XGI_REG_GRX, 0x33);
        curr = (CARD8)INB(XGI_REG_GRX+1);
        if (curr & 0x10)
        {
            /*
             * Before close device, wait for VBlank,
             * otherwise sometimes dot noise will appear.
             */

            OUTB(XGI_REG_GRX, 0x5b);
            if(INB(XGI_REG_GRX+1) & 0x10)
                XGIWaitVerticalOnCRTC2(pXGI, 1);
            else
                XGIWaitVerticalOnCRTC1(pXGI, 1);

            /* Turn off the LCD interface logic */
            OUTB(XGI_REG_GRX, 0x33);
            bNew = (curr & ~0x10) | 0x20;
            OUTB(XGI_REG_GRX+1, bNew);

            XG47WaitLcdPowerSequence(pXGI, bNew);

            /* LVDS PD line off */
            OUTB(XGI_REG_GRX, 0x71);
            OUTB(XGI_REG_GRX+1, (CARD8)INB(XGI_REG_GRX+1) & ~0x02);
        }

        OUTB(XGI_REG_GRX,0x5b);
        if (INB(XGI_REG_GRX+1) & DEV_SUPPORT_LCD)
        {
            /* Shadow off */
            OUTB(XGI_REG_GRX, 0x30);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x81);
            OUTB(XGI_REG_GRX, 0x44);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x03);


            /*
             * Disable centering
             * Centering function should work if only shadow enabled. But h/w always works.
             * So we need disable if LCD is not on. BIOS already take care of most cases.
             * However we have issue if we change device from LCD to other device in Windows,
             * because BIOS skip shadow and LCD on/off control.
             * So we turn centering bit off here.
             */
            OUTB(XGI_REG_GRX, 0x69);      //Vertical
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x80);
            OUTB(XGI_REG_GRX, 0x6d);      //Horizontal
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x80);

            /* Disable scaling */
            OUTB(XGI_REG_GRX, 0xd1);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x03);
        }
    }

    OUTB(XGI_REG_GRX, 0x5B);
    curr = (CARD8)INB(XGI_REG_GRX+1);

    /* 2nd display */
    if(device2Close & DEV_SUPPORT_DVI)
    {
        OUTB(XGI_REG_CRX, 0x5A);
        if (INB(XGI_REG_CRX+1) & 0x04)
        {
            OUTB(XGI_REG_GRX, 0x5A);
            if((INB(XGI_REG_GRX+1) & 0x80)
               && (curr & DEV_SUPPORT_DVI)
               && !(curr & DEV_SUPPORT_LCD))
            {
                /* Shadow on */
                OUTB(XGI_REG_GRX, 0x30);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x81);
                OUTB(XGI_REG_GRX, 0x44);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x03);

                /* Enable scaling */
                OUTB(XGI_REG_GRX, 0xd1);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)&~0x03);
            }
        }

        /* 2nd DAC off */
        OUTB(XGI_REG_CRX, 0xd7);
        OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) | 0x40);

        /* disable TMDS */
        OUTB(XGI_REG_GRX, 0x3d);
        OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x01);
        OUTB(XGI_REG_GRX, 0x26);
        OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x30);
        /*
        OUTB(XGI_REG_GRX,0x27);
        OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x01);
        */
        if (curr & 0x80)
            XGIWaitVerticalOnCRTC2(pXGI, 20);
        else
            XGIWaitVerticalOnCRTC1(pXGI, 20);
    }

#ifdef TOPGUN
    OUTB(XGI_REG_GRX, 0x36);
    OUTB(XGI_REG_GRX+1, (CARD8)INB(XGI_REG_GRX+1) | 0x01);

    pXGI->pInt10->ax = 0x120d;
    pXGI->pInt10->bx = 0x0314;
    pXGI->pInt10->cx = (CARD16)device2Close;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    if(!(pXGI->pInt10->ax >> 8))
    {
        while(1)
        {
            OUTB(XGI_REG_GRX, 0x36);
            if(!((CARD8)INB(XGI_REG_GRX+1) & 0x01))
                break;
        }
    }
    OUTB(XGI_REG_GRX, 0x36);
    OUTB(XGI_REG_GRX+1,(CARD8)INB(XGI_REG_GRX+1) & ~0x01);
#endif

    pXGI->biosDllOperationFlag |= DEVICE_CLOSED; /* close complete. */
}

void XG47OpenAllDevice(XGIPtr pXGI, CARD8 device2Open)
{
    ScrnInfoPtr pScrn = pXGI->pScrn;
    CARD8 curr, bNew, bDPMS_status;

	/* Jong 09/14/2006; get DPMS status of devices */
    OUTB(XGI_REG_GRX,0x55);
    bDPMS_status = (CARD8)INB(XGI_REG_GRX+1);

	/* Jong 09/14/2006; check current available devices */
    OUTB(XGI_REG_GRX,0x5b);
    curr = (CARD8)INB(XGI_REG_GRX+1);

	/* Jong 09/14/2006; (curr & 0x0f) means current device status of single view */
	/* (curr >> 4) means current device status of multiple view */
	/* device2Open means devices which we want to open */
	/* ((curr & 0x0f) | (curr >> 4)) means devices which are available no matter they are in single view or multiple view */
	/* device2Open &= ((curr & 0x0f) | (curr >> 4)) means devices which are available and we want to open */
	/* Why we got curr=0x02 even CRT and DVI have connected displays ??? */
    /* device2Open &= ((curr & 0x0f) | (curr >> 4)); */

	/* Jong 09/14/2006; Force to open CRT and DVI for test */
	/* Work!!!! */
	device2Open=0x0A; 

    if(!device2Open)
        return;

    /* CRT display */
    if ((device2Open & DEV_SUPPORT_CRT)
        && (!(bDPMS_status & 0x03)))
    {
        /* DAC power on */
        OUTB(XGI_REG_SRX,0x24);
        OUTB(XGI_REG_SRX+1, (CARD8)INB(XGI_REG_SRX+1) | 0x01);
        /* CRT on */
        OUTB(XGI_REG_GRX, 0x33);
        OUTB(XGI_REG_GRX+1, (CARD8)INB(XGI_REG_GRX+1) | 0x20);
        //Sync on
        OUTB(XGI_REG_GRX,0x23);
        OUTB(XGI_REG_GRX+1,(CARD8)INB(XGI_REG_GRX+1) & ~0x03);
    }

    /* LCD display */
    if(device2Open & DEV_SUPPORT_LCD)
    {
        if(curr & DEV_SUPPORT_LCD)
        {
            /* Shadow on */
            OUTB(XGI_REG_GRX, 0x30);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)|0x81);
            OUTB(XGI_REG_GRX, 0x44);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)|0x03);

            //we already control the H/V DE control on/off in BIOS code
            /* Eable centering
            OUTB(XGI_REG_GRX, 0x5d);
            if ((INB(XGI_REG_GRX+1) & GRAF_EXPANSION) == 0)
            {
                OUTB(XGI_REG_GRX, 0x69);      //Vertical
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x80);
                OUTB(XGI_REG_GRX, 0x6d);      //Horizontal
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x80);
            }
            */

            /* Enable scaling */
            OUTB(XGI_REG_GRX, 0xd1);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)|0x03);
        }

        if (!(bDPMS_status & 0x10))
        {
            OUTB(XGI_REG_GRX, 0x33);
            curr = (CARD8)INB(XGI_REG_GRX+1);

            if (!(curr & 0x10))
            {
                OUTB(XGI_REG_GRX, 0x36);
                if ((INB(XGI_REG_GRX+1) & 0x8) == 0)
                {
                    /* Turn on the LCD interface logic */
                    bNew = curr | 0x10;
                    OUTB(XGI_REG_GRX, 0x33);
                    OUTB(XGI_REG_GRX+1, bNew);

                    XG47WaitLcdPowerSequence(pXGI, bNew);
                }

                /* LVDS PD line on */
                OUTB(XGI_REG_GRX, 0x71);
                OUTB(XGI_REG_GRX+1, (CARD8)INB(XGI_REG_GRX+1) | 0x02);

                /*
                 * XG47 Rev. A0 patch begin
                 * Reset dual channel LVDS even/odd sequence.
                 * Rev. A1 will fix.
                 */

                /* Wit V Sync. start */
                OUTB(XGI_REG_GRX,0x5b);
                curr = (CARD8)INB(XGI_REG_GRX+1);
                if(curr & DEV_SUPPORT_LCD)
                {
                    while(!(INB(0x3da) & 0x08));
                }
                else
                {
                    OUTB(XGI_REG_SRX, 0xDC);
                    while (!(INB(XGI_REG_SRX+1) & 0x01));
                }

                /* Power down LVDS and back on */
                OUTB(XGI_REG_GRX, 0x71);
                OUTB(XGI_REG_GRX+1,(CARD8)INB(XGI_REG_GRX+1) & ~0x02);
                OUTB(XGI_REG_GRX, 0x71);
                OUTB(XGI_REG_GRX+1,(CARD8)INB(XGI_REG_GRX+1) | 0x02);
                /* XG47 Rev. A0 patch end */
            }
        }
    }

	/* Jong 09/14/2006; why to call with second time */
    OUTB(XGI_REG_GRX, 0x5B);
    curr = (CARD8)INB(XGI_REG_GRX+1);

    /* 2nd display */
    if(device2Open & DEV_SUPPORT_DVI)
    {
		/* Jong 09/14/2006; System status flag 1 (3D5.5A)*/
		/*	3x5_5A:	7	6	5	4	3	2	1	0
					|	|	|	|	|	|	|	*	-	Digital monitor in 1st port
					|	|	|	|	|	|	*	-	-	1st monitor attached
					|	|	|	|	|	*	-	-	-	Digital monitor in 2nd port
					|	|	|	|	*	-	-	-	-	2nd monitor attached
					|	|	|	*	-	-	-	-	-	TV connected
					|	|	*	-	-	-	-	-	-	Copy of 3x5.2F.5
					|	*	-	-	-	-	-	-	-	Display control
					*	-	-	-	-	-	-	-	-	In-POST */

        OUTB(XGI_REG_CRX, 0x5A);

		/* Jong 09/14/2006; Digital monitor in 2nd port */
        if (INB(XGI_REG_CRX+1) & 0x04)
        {
            OUTB(XGI_REG_GRX, 0x5A);

			/* Jong 09/14/2006; why to check if it is fixed timing for digital display??? */
            if((INB(XGI_REG_GRX+1) & 0x80)
               && (curr & DEV_SUPPORT_DVI)
               && !(curr & DEV_SUPPORT_LCD))
            {
                /* Shadow on */
                OUTB(XGI_REG_GRX, 0x30);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)|0x81);
                OUTB(XGI_REG_GRX, 0x44);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)|0x03);

                /* Enable scaling */
                OUTB(XGI_REG_GRX, 0xd1);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1)|0x03);
            }

			/* Jong 09/14/2006; why to do this ??? */
            if (!(bDPMS_status & 0x0C))
            {
                /* Enable TMDS, RevB always on */
                OUTB(XGI_REG_GRX,0x3d);
                OUTB(XGI_REG_GRX+1,(int)INB(XGI_REG_GRX+1) | 0x01);
                OUTB(XGI_REG_GRX,0x26);
                OUTB(XGI_REG_GRX+1,(int)INB(XGI_REG_GRX+1) & ~0x30);
                if (curr & 0x80)
                    XGIWaitVerticalOnCRTC2(pXGI, 15);
                else
                    XGIWaitVerticalOnCRTC1(pXGI, 15);

                /*
                 * OUTB(XGI_REG_GRX,0x27);
                 * OUTB(XGI_REG_GRX+1,(int)INB(XGI_REG_GRX+1) & ~0x01);
                 */
            }
        }
 		/* Jong 09/14/2006; indicate analog display in 2nd port */
		/* Set 2nd DAC (Digital Alalog Converter) on */
       else
        {
            OUTB(XGI_REG_CRX, 0xd7);
            OUTB(XGI_REG_CRX+1, (INB(XGI_REG_CRX+1) & ~0x40) | 0x80);
            if (!(bDPMS_status & DEV_SUPPORT_DVI))
            {
                /* Program external de-mux */
                OUTB(XGI_REG_GRX, 0x27);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x02);
                OUTB(XGI_REG_GRX, 0x26);
                OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x30);
            }
        }
    }

    /*
     * call BIOS function DeviceSwitchPosHook to
     * let SBIOS know current device status
     */
    bNew = 0x0;

    OUTB(XGI_REG_CRX,0xc0);
    bNew |= (CARD8)INB(XGI_REG_CRX+1) & 0xe0;

    OUTB(XGI_REG_GRX,0x36);
    OUTB(XGI_REG_GRX+1,(CARD8)INB(XGI_REG_GRX+1) | 0x01);
    bDPMS_status |= ((bDPMS_status << 1) & 0x20);   /* Merge 2 CRT bits to 1bit and put in bit 1 */
    bDPMS_status &= ~0x01;
    bDPMS_status |= (bDPMS_status >> 4);            /* Move LCD flag to bit 0 */
    pXGI->pInt10->ax = 0x120d;
    pXGI->pInt10->bx = 0x0414;
    pXGI->pInt10->cx = (CARD16)bNew << 8 | (CARD16)(device2Open & (~bDPMS_status));
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    if(!(pXGI->pInt10->ax >> 8))
    {
        while(1)
        {
            OUTB(XGI_REG_GRX, 0x36);
            if(!((CARD8)INB(XGI_REG_GRX+1) & 0x01))
                break;
        }
    }
    OUTB(XGI_REG_GRX, 0x36);
    OUTB(XGI_REG_GRX+1,(CARD8)INB(XGI_REG_GRX+1) & ~0x01);

    /* allow SMI switching, Do not touch bit 6 and 3 */
    OUTB(XGI_REG_CRX, SOFT_PAD59_INDEX);
    OUTB(XGI_REG_CRX+1,(((CARD8)INB(XGI_REG_CRX+1) & 0x48) | (crtc59 & ~0x48)));

    /* turn on display */
	/* Jong 09/14/2006; XGI_REG_SRX=0x03C4; 0x01:Clocking Mode Register */ 
    OUTB(XGI_REG_SRX, 0x01);
	/* Jong 09/14/2006;  turn screen on and selects normal screen operation */
    OUTB(XGI_REG_SRX+1,(CARD8)INB(XGI_REG_SRX+1) & ~0x20);

    /* TV display */
    if(device2Open & DEV_SUPPORT_TV)
    {
        /* Program external de-mux */
        OUTB(XGI_REG_GRX,0x27);
        OUTB(XGI_REG_GRX+1,(int)INB(XGI_REG_GRX+1) & ~0x02);
        if((pXGI->biosDevSupport & DEV_SUPPORT_TV)
          && !(pXGI->biosDevSupport & SUPPORT_CURRENT_NO_TV)
          && (pXGI->pBiosDll->biosDtvCtrl))
            (*pXGI->pBiosDll->biosDtvCtrl)(pScrn,
                                           (CARD32)ENABLE_TV_DISPLAY,
                                           (CARD32 *)NULL,
                                           (CARD32 *)NULL);
    }

    pXGI->biosDllOperationFlag &= (~DEVICE_CLOSED); /* OPEN complete */
}

/*
 * from bios dll: modeset.c
 */

//Input
//  device:  Bit3~0: DVI/TV/CRT/LCD
//  index:   the index to the mode table: XG47ModeTable
//  modeNum: the BIOS mode number
//  colorIndex:   the color index
//    0x2 - 8  bpp
//    0x6 - 16 bpp
//    0x8 - 32 bpp A8R8G8B8
//    0xA - 32 bpp A2R10G10B10
//Modification
//  Update the mode table: myVideoModeInfoTable in first time call
//Return
//   refresh rates supported by specified video mode and device (See BIOS spec: Get refresh rate support for definition)
//
CARD16 XGIGetRefreshSupport(XGIPtr pXGI, CARD8 device, CARD8 index, CARD16 modeNum, CARD16 colorIndex)
{
    XGIModePtr  pModeTable = XG47ModeTable;
    CARD16      refresh;
    CARD8       i = index;

    switch (device)
    {
    case DEV_SUPPORT_LCD:
        if(pModeTable[i].refBIOS[0])
        {
            refresh = pModeTable[i].refBIOS[0];
        }
        else
        {
            pModeTable[i].refBIOS[0] = refresh = XGIGetRefreshRateCapability(pXGI, modeNum, colorIndex);
        }
        break;
    case DEV_SUPPORT_CRT:
        if(pModeTable[i].refBIOS[1])
        {
            refresh = pModeTable[i].refBIOS[1];
        }
        else
        {
            pModeTable[i].refBIOS[1] = refresh = XGIGetRefreshRateCapability(pXGI, modeNum, colorIndex);
        }
        break;
    case DEV_SUPPORT_TV:
        if(pModeTable[i].refBIOS[2])
        {
            refresh = pModeTable[i].refBIOS[2];
        }
        else
        {
            pModeTable[i].refBIOS[2] = refresh = XGIGetRefreshRateCapability(pXGI, modeNum, colorIndex);
        }
        break;
    case DEV_SUPPORT_DVI:
        if(pModeTable[i].refBIOS[3])
        {
            refresh = pModeTable[i].refBIOS[3];
        }
        else
        {
            pModeTable[i].refBIOS[3] = refresh = XGIGetRefreshRateCapability(pXGI, modeNum, colorIndex);
        }
        break;
    }
    return refresh;
}

/*
 * Check if the mode is supported in our chip
 * (???and get video mode extension based on given information???)
 *
 * Entry : The mode for primary view
 *         The mode for second view. NULL if in single view mode
 *
 * Return: TRUE  - The mode is supported
 *         FALSE - The mode is NOT supported
*/
Bool XG47GetValidMode(XGIPtr pXGI,
                      XGIAskModePtr pMode0,
                      XGIAskModePtr pMode1)
{
    XGIModePtr  pModeTable;
    CARD8       i;
    CARD8       tv_ntsc_pal, tv_ntsc_pal_org;
    CARD8       tv_3cf_5b, want_3cf_5a;
    CARD16      refSupport;
    INT8        refRateIndex = 0;
    CARD16      modeNo, modeSpec=0;
    CARD16      flag;
    CARD16      j;
    CARD8       k;
    unsigned long   ret_value;

    XGIGetSetChipSupportDevice(pXGI, 0, 0);

	/* Jong 09/20/2006; must be assigned for both pMode0 and pMode1 */
	pModeTable = XG47ModeTable; 

	/* Jong 09/15/2006; support dual view */
	if(pMode0)
	{
		/* Check with mode index table */
		pMode0->modeNo &= ~0x7F;    /* only clear mode number */

		/* Jong 09/20/2006; must be assigned for both pMode0 and pMode1 */
		/* pModeTable = XG47ModeTable; */

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-pMode0->modeNo=%d--\n", pMode0->modeNo);
#endif

		/* Jong 11/08/2006; find the mode number */
		for (i=0; i < XG47ModeTableSize; i++)
		{
			if ((pModeTable[i].width == pMode0->width)
			 && (pModeTable[i].height == pMode0->height))
			{
				pMode0->modeNo |= pModeTable[i].modeNo & 0x7F;

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-Found a supported mode-pMode0->modeNo=%d, pModeTable[i=%d]--\n", pMode0->modeNo, i);
#endif
				break;
			}
		}

		/* Jong 09/12/2006; why? */
		if (!(pMode0->modeNo & 0x7F))   return FALSE;

		/* display device */
		/* Jong 09/15/2006;
			#define ST_DISP_LCD         0x00000001
			#define ST_DISP_CRT         0x00000002
			#define ST_DISP_TV          0x00000004
			#define ST_DISP_DVI         0x00000008 */
		want_3cf_5a = (CARD8)(pMode0->condition & 0x0000000F);

		/*
		 * TV format
		 */
		OUTB(XGI_REG_CRX, 0xC0);
		tv_ntsc_pal = (CARD8)INB(XGI_REG_CRX+1) & 0xE0;

		OUTB(XGI_REG_GRX, 0x5A);
		tv_3cf_5b = (CARD8)INB(XGI_REG_GRX+1);

		if ((pMode0->condition & DEV_SUPPORT_TV))
		{
			tv_ntsc_pal = 0;    /* NTSC */
			if(pMode0->condition & ZVMX_ATTRIB_NTSCJ)
				tv_ntsc_pal = 0x20;
			if(pMode0->condition & ZVMX_ATTRIB_PALM)
				tv_ntsc_pal = 0x40;
			if(pMode0->condition & ZVMX_ATTRIB_PAL)
				tv_ntsc_pal = 0x80;
		}

		/* Check video BIOS support or not */

		/* Jong 09/14/2006; why? */
		OUTW(0x3C4, 0x9211);

		OUTB(XGI_REG_GRX, 0x5A);
		OUTB(XGI_REG_GRX+1, ((tv_3cf_5b & 0xF0) | want_3cf_5a));
		OUTB(XGI_REG_CRX, 0xC0);
		tv_ntsc_pal_org = (CARD8)INB(XGI_REG_CRX+1);
		OUTB(XGI_REG_CRX + 1, ((tv_ntsc_pal_org & 0x1F) | tv_ntsc_pal));

		modeNo = pMode0->modeNo & 0x7F;
		modeSpec |= XGIGetColorIndex(pMode0->pixelSize);
		if (modeSpec & 0x08)        /* 32 bit true color */
		{
			modeSpec |= (pMode0->modeNo & 0x0100) >> 7; /* 10 bits */
		}

		refSupport = XGIGetRefreshSupport(pXGI, (CARD8)(pMode0->condition & 0x0f), i, modeNo, modeSpec);
		/*
		 * CRT, DVI device
		 */
		/* Jong 09/14/2006; why? */
		OUTW(0x3C4, 0x9211);

		OUTB(XGI_REG_GRX, 0x5A);
		OUTB(XGI_REG_GRX+1, tv_3cf_5b);

		/* Jong 09/14/2006; TV Status flag 1 */
		OUTB(XGI_REG_CRX, 0xC0);
		OUTB(XGI_REG_CRX+1, tv_ntsc_pal_org);

		if((CARD8)(pMode0->condition & 0x0F) == DEV_SUPPORT_CRT)
			refSupport &= pModeTable[i].refSupport[1];
		if((CARD8)(pMode0->condition & 0x0F) == DEV_SUPPORT_DVI)
			refSupport &= pModeTable[i].refSupport[3];
		if(pMode0->condition & DEV_SUPPORT_TV)
			refSupport &= pModeTable[i].refSupport[2];
		if(pMode0->condition & DEV_SUPPORT_LCD)
			refSupport &= pModeTable[i].refSupport[0];

		pMode0->refSupport = refSupport;
		if (!refSupport) return FALSE;

		/*
		 * Refresh Rate
		 */
		flag = 1;
		refRateIndex = XG47ConvertRefValueToIndex(pMode0->refRate);

		if(!refRateIndex)
		{
			flag=0;
			for(refRateIndex = 0; refRateIndex < VREF_MAX_NUMBER; refRateIndex++)
			{
				if((refSupport >> refRateIndex) & 0x01) break;
			}
		}
		else
		{
			for(; refRateIndex >= 0; refRateIndex--)
			{
				if(XGICheckRefreshRateSupport(refSupport, (CARD8)refRateIndex))
					break;
			}
			if (refRateIndex < 0)
			{
				flag=0;
				for(refRateIndex = 0; refRateIndex<VREF_MAX_NUMBER; refRateIndex++)
				{
					if((refSupport >> refRateIndex) & 0x01)
					{
						refRateIndex++;
						break;
					}
				}
			}
		}
		pMode0->refRate = (CARD16)XG47GetRefreshRateByIndex((CARD8)refRateIndex);
	}

    /*
     *
     */
	/* Jong 09/14/2006; Check mode for dual view */
	/* Jong 09/16/2006; codes need to be modified because they use pMode0 = NULL */
    if(pMode1)
    {
 #ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-pMode1->modeNo=%d--\n", pMode1->modeNo);
#endif

		/* Check framebuffer size */
        if(pMode1->condition & 0x80000000)  /* MHS */
        {
			ret_value= (CARD32)(pMode1->width * pMode1->height * (pMode1->pixelSize/8) );

			/* Jong 09/16/2006; don't need to check total size of dual view but just check second view */
            /* ret_value  = (CARD32)pMode0->width * pMode0->height * pMode0->pixelSize/8;
            ret_value += (CARD32)pMode1->width * pMode1->height * pMode1->pixelSize/8; */

        }
        else
        {
			ret_value= (CARD32)(pMode1->width * pMode1->height * (pMode1->pixelSize/8) );

			/* Jong 09/16/2006; why comparing pMode0 and pMode1 ??? */
            /* if(pMode0->width < pMode1->width)
                ret_value = (CARD32)pMode1->width;
            else
                ret_value = (CARD32)pMode0->width;

            if(pMode0->height < pMode1->height)
                ret_value *= (CARD32)pMode1->height;
            else
                ret_value *= (CARD32)pMode0->height;

            ret_value *= (CARD32)pMode0->pixelSize;
            ret_value /= 8; */
        }

        /* Check memory size supportting by panel type. */
        if(ret_value > pXGI->freeFbSize)  return FALSE;

        pMode1->modeNo &= ~0x7F;
        //pMode1->modeNo |= (CARD16)XGIConvertResToModeNo(pMode1->width,
        //                                                pMode1->height,
        //                                                pMode1->pixelSize);

		/* Jong 09/16/2006; can't get valid mode from table !!! */
        for (k = 0; k < XG47ModeTableSize; k++)
        {
            if ((pModeTable[k].width == pMode1->width)
             && (pModeTable[k].height == pMode1->height))
            {
                pMode1->modeNo |= pModeTable[k].modeNo & 0x7F;
                break;
            }
        }

		/* Jong 09/16/2006; return FALSE for second view !!! */
        if(!(pMode1->modeNo & 0x7F))    return FALSE; 

        if (pMode1->condition & DEV_SUPPORT_LCD)
        {
            pMode1->refRate = pXGI->lcdRefRate;
            pMode1->refSupport =(CARD16)XG47ConvertRefValueToIndex(pXGI->lcdRefRate);
        }
        else
        {
            /*
             * Refresh Rate for second view.
             */
            CARD8 save;
            INT8  refRateIndex2 = 0;

            OUTW(0x3C4, 0x9211);
            OUTB(XGI_REG_GRX, 0x5A);
            save = INB(XGI_REG_GRX + 1);
            OUTB(XGI_REG_GRX + 1, ((save & 0xF0) | (pMode1->condition & 0x0F)));

            refSupport = XGIGetRefreshSupport(pXGI,
                                              (CARD8)(pMode1->condition & 0x0f), k,
                                              (CARD16)(pMode1->modeNo & 0x7F),
                                              XGIGetColorIndex(pMode1->pixelSize));
            OUTW(0x3C4, 0x9211);
            OUTB(XGI_REG_GRX, 0x5A);
            OUTB(XGI_REG_GRX+1, save);

            pMode1->refSupport = refSupport;
            if (!refSupport)    return FALSE;

            refRateIndex2 = XG47ConvertRefValueToIndex(pMode1->refRate);

            if (!refRateIndex2)
            {
                for (refRateIndex2 = 0; refRateIndex2 < VREF_MAX_NUMBER; refRateIndex2++)
                {
                    if ((refSupport >> refRateIndex2) & 0x01)
                    {
                        refRateIndex2++;
                        break;
                    }
                }
            }
            else
            {
                for (; refRateIndex2 >= 0; refRateIndex2--)
                {
                    if (XGICheckRefreshRateSupport(refSupport, (CARD8)refRateIndex2))
                        break;
                }
                if (refRateIndex2 < 0)
                {

                    for (refRateIndex2 = 0; refRateIndex2 < VREF_MAX_NUMBER; refRateIndex2++)
                    {
                        if ((refSupport >> refRateIndex2) & 0x01)
                        {
                            refRateIndex2++;
                            break;
                        }
                    }
                }
            }
            pMode1->refRate = (CARD16)XG47GetRefreshRateByIndex((CARD8)refRateIndex2);
        }

        /*
         * Check Refresh Rate & BW
         */
#if 0 /* Jong 09/16/2006 uncertainly; don't apply at this moment !!!! */
        for(j=0; j<VREF_MAX_NUMBER; j++)
        {
            if((pMode0->refSupport >> j) & 0x01)
            {
                if (XGICheckModeSupported(pXGI, pMode0, pMode1, (CARD16)XG47GetRefreshRateByIndex((CARD8)j)) == FALSE)
                    pMode0->refSupport &= ~((CARD16)0x0001 << j);
            }
        }

        if(!pMode0->refSupport) return FALSE;

        if(flag)               /* flag and refRateIndex come from pMode0. */
        {
            if (!XGICheckRefreshRateSupport(pMode0->refSupport, (CARD8)refRateIndex))
                return FALSE;
        }
        else
        {
            for(j=0; j<VREF_MAX_NUMBER; j++)
            {
                if((pMode0->refSupport >> j) & 0x01) break;
            }
            pMode0->refRate = (CARD16)XG47GetRefreshRateByIndex((CARD8)(j+1));
            if(pMode0->refRate == 0)
                return FALSE;
        }
#endif

    }
    else
    {
		/* Jong 09/15/2006; support dual view */
		if(pMode0 == NULL) return FALSE;

        for(j=0; j<VREF_MAX_NUMBER; j++)
        {
            if((pMode0->refSupport >> j) & 0x01)
            {
                if(XGICheckModeSupported(pXGI, pMode0, pMode1, (CARD16)XG47GetRefreshRateByIndex((CARD8)j+1))==FALSE)
                    pMode0->refSupport &= ~((CARD16)0x0001 << j);
            }
        }

        if (!(pMode0->refSupport & ((CARD16)0x0001 << (refRateIndex-1))))
        {
            /*
             * If the current refresh rate can not be supported,
             * find lowest one.
             */
            for (refRateIndex=0; refRateIndex<VREF_MAX_NUMBER; refRateIndex++)
            {
                if ((pMode0->refSupport >> refRateIndex) & 0x01)
                {
                    refRateIndex++;
                    break;
                }
            }
        }

        if (!pMode0->refRate) return FALSE;

#if 0 /* old codes */
        /* Check framebuffer size */
        if(pMode1->condition & 0x80000000)  /* MHS */
        {
            ret_value  = (CARD32)pMode0->width * pMode0->height * pMode0->pixelSize/8;
            ret_value += (CARD32)pMode1->width * pMode1->height * pMode1->pixelSize/8;

        }
        else
        {
            if(pMode0->width < pMode1->width)
                ret_value = (CARD32)pMode1->width;
            else
                ret_value = (CARD32)pMode0->width;

            if(pMode0->height < pMode1->height)
                ret_value *= (CARD32)pMode1->height;
            else
                ret_value *= (CARD32)pMode0->height;

            ret_value *= (CARD32)pMode0->pixelSize;
            ret_value /= 8;
        }

        /* Check memory size supportting by panel type. */
        if(ret_value > pXGI->freeFbSize)  return FALSE;

        pMode1->modeNo &= ~0x7F;
        //pMode1->modeNo |= (CARD16)XGIConvertResToModeNo(pMode1->width,
        //                                                pMode1->height,
        //                                                pMode1->pixelSize);
        for (k = 0; k < XG47ModeTableSize; k++)
        {
            if ((pModeTable[k].width == pMode1->width)
             && (pModeTable[k].height == pMode1->height))
            {
                pMode1->modeNo |= pModeTable[k].modeNo & 0x7F;
                break;
            }
        }
        if(!(pMode1->modeNo & 0x7F))    return FALSE;

        if (pMode1->condition & DEV_SUPPORT_LCD)
        {
            pMode1->refRate = pXGI->lcdRefRate;
            pMode1->refSupport =(CARD16)XG47ConvertRefValueToIndex(pXGI->lcdRefRate);
        }
        else
        {
            /*
             * Refresh Rate for second view.
             */
            CARD8 save;
            INT8  refRateIndex2 = 0;

            OUTW(0x3C4, 0x9211);
            OUTB(XGI_REG_GRX, 0x5A);
            save = INB(XGI_REG_GRX + 1);
            OUTB(XGI_REG_GRX + 1, ((save & 0xF0) | (pMode1->condition & 0x0F)));

            refSupport = XGIGetRefreshSupport(pXGI,
                                              (CARD8)(pMode1->condition & 0x0f), k,
                                              (CARD16)(pMode1->modeNo & 0x7F),
                                              XGIGetColorIndex(pMode1->pixelSize));
            OUTW(0x3C4, 0x9211);
            OUTB(XGI_REG_GRX, 0x5A);
            OUTB(XGI_REG_GRX+1, save);

            pMode1->refSupport = refSupport;
            if (!refSupport)    return FALSE;

            refRateIndex2 = XG47ConvertRefValueToIndex(pMode1->refRate);

            if (!refRateIndex2)
            {
                for (refRateIndex2 = 0; refRateIndex2 < VREF_MAX_NUMBER; refRateIndex2++)
                {
                    if ((refSupport >> refRateIndex2) & 0x01)
                    {
                        refRateIndex2++;
                        break;
                    }
                }
            }
            else
            {
                for (; refRateIndex2 >= 0; refRateIndex2--)
                {
                    if (XGICheckRefreshRateSupport(refSupport, (CARD8)refRateIndex2))
                        break;
                }
                if (refRateIndex2 < 0)
                {

                    for (refRateIndex2 = 0; refRateIndex2 < VREF_MAX_NUMBER; refRateIndex2++)
                    {
                        if ((refSupport >> refRateIndex2) & 0x01)
                        {
                            refRateIndex2++;
                            break;
                        }
                    }
                }
            }
            pMode1->refRate = (CARD16)XG47GetRefreshRateByIndex((CARD8)refRateIndex2);
        }

        /*
         * Check Refresh Rate & BW
         */
        for(j=0; j<VREF_MAX_NUMBER; j++)
        {
            if((pMode0->refSupport >> j) & 0x01)
            {
                if (XGICheckModeSupported(pXGI, pMode0, pMode1, (CARD16)XG47GetRefreshRateByIndex((CARD8)j)) == FALSE)
                    pMode0->refSupport &= ~((CARD16)0x0001 << j);
            }
        }

        if(!pMode0->refSupport) return FALSE;

        if(flag)               /* flag and refRateIndex come from pMode0. */
        {
            if (!XGICheckRefreshRateSupport(pMode0->refSupport, (CARD8)refRateIndex))
                return FALSE;
        }
        else
        {
            for(j=0; j<VREF_MAX_NUMBER; j++)
            {
                if((pMode0->refSupport >> j) & 0x01) break;
            }
            pMode0->refRate = (CARD16)XG47GetRefreshRateByIndex((CARD8)(j+1));
            if(pMode0->refRate == 0)
                return FALSE;
        }
    }
    else
    {
		/* Jong 09/15/2006; support dual view */
		if(pMode0 == NULL) return FALSE;

        for(j=0; j<VREF_MAX_NUMBER; j++)
        {
            if((pMode0->refSupport >> j) & 0x01)
            {
                if(XGICheckModeSupported(pXGI, pMode0, pMode1, (CARD16)XG47GetRefreshRateByIndex((CARD8)j+1))==FALSE)
                    pMode0->refSupport &= ~((CARD16)0x0001 << j);
            }
        }

        if (!(pMode0->refSupport & ((CARD16)0x0001 << (refRateIndex-1))))
        {
            /*
             * If the current refresh rate can not be supported,
             * find lowest one.
             */
            for (refRateIndex=0; refRateIndex<VREF_MAX_NUMBER; refRateIndex++)
            {
                if ((pMode0->refSupport >> refRateIndex) & 0x01)
                {
                    refRateIndex++;
                    break;
                }
            }
        }

        if (!pMode0->refRate) return FALSE;
#endif
    }

    return TRUE;
}

void XG47BiosValueInit(ScrnInfoPtr pScrn)
{
    XGIPtr      pXGI = XGIPTR(pScrn);
    CARD16      engineClock, memClock;

    /*
     * Default Display Device Info
     */
    pXGI->biosDevSupport    = SUPPORT_DEV_DVI          \
                            | SUPPORT_DEV_CRT          \
                            | SUPPORT_DEV_LCD          \
                            | SUPPORT_PANEL_CENTERING  \
                            | SUPPORT_PANEL_EXPANSION  \
                            | SUPPORT_DEV2_DVI         \
                            | SUPPORT_DEV2_CRT         \
                            | SUPPORT_DEV2_LCD;

    pXGI->biosOrgDevSupport = pXGI->biosDevSupport;

    /*
     * Display Device Info from BIOS
     */
    if (!(pXGI->biosCapability & BIOS_TV_CAPABILITY))
    {
        /* does not support TV */
        pXGI->biosDevSupport    |= SUPPORT_CURRENT_NO_TV;
        pXGI->biosOrgDevSupport |= SUPPORT_CURRENT_NO_TV;
    }
    else
    {
        /* support TV */
        pXGI->biosDevSupport    |= SUPPORT_TV_NATIVE   \
                                | SUPPORT_TV_OVERSCAN  \
                                | SUPPORT_TV_UNDERSCAN \
                                | SUPPORT_TV_PAL       \
                                | SUPPORT_TV_NTSC      \
                                | SUPPORT_DEV2_TV      \
                                | SUPPORT_DEV_TV;
        pXGI->biosOrgDevSupport |= pXGI->biosDevSupport;
    }

    if (pXGI->lcdWidth > (pXGI->lcdHeight/3*4))
    {
        /* wide panel */
        pXGI->biosDevSupport    |= SUPPORT_PANEL_V_EXPANSION;
        pXGI->biosOrgDevSupport |= SUPPORT_PANEL_V_EXPANSION;
    }

    if (!(pXGI->biosCapability & BIOS_LCD_SUPPORT))
    {
        pXGI->biosDevSupport    &=0xfeff00fe;
        pXGI->biosOrgDevSupport &=0xfeff00fe;
    }

    if (!(pXGI->biosCapability & BIOS_DVI_SUPPORT))
    {
        pXGI->biosDevSupport    &=0xf7fffff7;
        pXGI->biosOrgDevSupport &=0xf7fffff7;
    }

    if (!(pXGI->biosCapability & BIOS_CRT_SUPPORT))
    {
        pXGI->biosDevSupport    &=0xfdfffffd;
        pXGI->biosOrgDevSupport &=0xfdfffffd;
    }

    /*
     * Check TV
     */

    pXGI->pInt10->ax = 0x120E;
    pXGI->pInt10->bx = 0x0014;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

    if ((pXGI->pInt10->ax >> 8) != 0x0)               /* No TV */
    {
        pXGI->biosDevSupport |= SUPPORT_CURRENT_NO_TV;
    }
    else
    {
        if (!(pXGI->pInt10->ax & 0x0004))
        {
            pXGI->biosDevSupport    |= ZVMX_ATTRIB_PALM;
            pXGI->biosOrgDevSupport |= ZVMX_ATTRIB_PALM;
        }
        if (!(pXGI->pInt10->ax & 0x0002))
        {
            pXGI->biosDevSupport    |= ZVMX_ATTRIB_NTSCJ;
            pXGI->biosOrgDevSupport |= ZVMX_ATTRIB_NTSCJ;
        }
    }

    /*
     * Get LCD Timing Parameters
     */
    if(XGIReadBiosData(pXGI, value))
    {
        vclk18 = value[16];
        vclk19 = value[17];
        vclk28 = value[18] & 0x70;
        GR3CE_45 = value[19];
    }

    OUTB(XGI_REG_GRX, 0x45);
    GR3CE_45_SingleView = INB(XGI_REG_GRX + 1);

    /*
     * Calculate Max. available Memory Bandwidth
     */
    engineClock = XGIBiosCalculateClock(pXGI, 0x16, 0x17);
    memClock = XGIBiosCalculateClock(pXGI, 0x1E, 0x1F);
    pXGI->maxBandwidth = (engineClock <= memClock) ? engineClock : memClock;
    pXGI->maxBandwidth = pXGI->maxBandwidth * 8 * 2;    /* 64bit, DDR */
    OUTB(XGI_REG_CRX, 0x5D);
    if(INB(XGI_REG_CRX + 1) & 0x01 )
    {
        pXGI->maxBandwidth = pXGI->maxBandwidth * 8;    //32bit, DDR *4*2 --> *8
    }
    else
    {
        pXGI->maxBandwidth = pXGI->maxBandwidth * 16;   //64bit, DDR *8*2 --> *16
    }

    pXGI->maxBandwidth = (CARD16)(((CARD32)pXGI->maxBandwidth * 60) / 100);

    /*
     * Register Initialize
     */

    /* CRTC2 sync load enable */
    OUTB(XGI_REG_SRX, 0xBF);
    OUTB(XGI_REG_SRX+1,(int)INB(XGI_REG_SRX+1) | 0x0C);

    /* DV DSTN position */
    if(pXGI->lcdType != FPTYPE_TFT)
    {
        OUTB(XGI_REG_GRX, 0x49);
        OUTB(XGI_REG_GRX+1, ((pXGI->lcdHeight/2) & 0xFF));
        OUTB(XGI_REG_GRX, 0x4A);
        OUTB(XGI_REG_GRX+1, ((((pXGI->lcdHeight/2) >> 8) & 0xFF) | 0x80));
    }

    /* Anti-tearing sync select */
    OUTB(0x2450, (CARD16)INB(0x2450) & 0xFB); /* HW recommend 2450[bit 2]=1'b0 */

    /* W2 line buffer threshold */
    OUTB(0x2492,0x04);

    /* Set Flag to indicate the DLL is loaded. */
    OUTB(XGI_REG_CRX, 0x59);
    OUTB(XGI_REG_CRX+1, (INB(XGI_REG_CRX+1) | 0x80));
}

/*
 * Validate Video Mode.
 *
 * Entry : wBitsPixel,  bits/pixel of the display format.
 *         wSurfaceWidth, width of the screen surface in pixel.
 *         wSurfaceHeight, height of the screen surface in scanlines.
 * Return: video mode information if none zero value, otherwise does not
 *         support this mode.
 */
Bool XG47BiosValidMode(ScrnInfoPtr pScrn,
                       XGIAskModePtr pMode,
                       CARD32 dualView)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    XGIAskModePtr   pMode0, pMode1;
    CARD32          temp;

	/* Jong 09/12/2006; why to do this but don't get its return value? */
    XGIGetSetChipSupportDevice(pXGI, 1, 0);
    pMode0 = pMode;

	/* Jong 09/12/2006; test */
	CARD32 test=~0xF0;

	/* Jong 09/15/2006; single view or first view of dual view mode */
    if (!dualView || (pXGI->FirstView)) /* single view */
    {
        pMode1 = NULL;
        pMode0->condition &= test;
    }
    else /* dual view */
    {
		/* Jong 09/12/2006; pMode1 has wrong data; seems not to initialize */
        pMode1 = pMode0 + 1;
		/* Jong 09/12/2006; why not to &= ~0x0F */
        pMode1->condition &= test;

		pMode0=NULL;
    }

#if 0
    if (!dualView) /* single view */
    {
        pMode1 = NULL;
        pMode0->condition &= ~0xF0;
    }
    else /* dual view */
    {
        pMode1 = pMode0 + 1; 
        pMode1->condition &= ~0xF0;
    }
#endif

    /* get current all display device status and information */
    temp = XGIGetDisplayStatus(pXGI, 0);

	/* Jong 09/15/2006; support dual view */
	if(pMode0)
	{
		/* check 1st display device input data */
		if (!(pMode0->condition & 0x0F))
			pMode0->condition |= (temp & 0x0F);

		/* Check LCD information (Expansion/Centering) */
		if (!(pMode0->condition & 0x0001C00))
			pMode0->condition |= (temp & 0x0001C00);

		/* Check TV format information */
		if (!(pMode0->condition & (ZVMX_ATTRIB_NTSCJ | ZVMX_ATTRIB_PALM | ZVMX_ATTRIB_PAL | ZVMX_ATTRIB_NTSC)))
		{
			pMode0->condition |= (temp & (ZVMX_ATTRIB_NTSCJ | ZVMX_ATTRIB_PALM | ZVMX_ATTRIB_PAL | ZVMX_ATTRIB_NTSC));
		}
		/* Check TV Overscan/Underscan mode info */
		if (!(pMode0->condition & 0x00C0000))
			pMode0->condition |= (temp & 0x00C0000);

		/* check valid 1st display device */
		if ((pMode0->condition & 0x0F) != (pXGI->biosDevSupport & (pMode0->condition & 0x0F)))
			return FALSE;
	}

    if (pMode1)
    {
        if (!(pMode1->condition & 0x0000C00))
            pMode1->condition |= (temp & 0x0000C00);

        /* check valid 2nd display device */
        if ((pMode1->condition & 0x0F) != (pXGI->biosDevSupport & (pMode1->condition & 0x0F)))
            return FALSE;
    }

    return XG47GetValidMode(pXGI, pMode0, pMode1);
}

/*
 * Set video mode patch based on mode extension number.
 *
 * Entry : dwModeExtension, video mode extersion number.
 *         dwConditionMask, setting condition mask.
 * Return: LOWORD - TRUE for set video mode successful,
 *                  otherwise return FALSE.
 *         HIWORD - refresh rate value returned.
 */
Bool XG47BiosModeInit(ScrnInfoPtr pScrn,
                      XGIAskModePtr pMode,
                      CARD32 dualView)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    XGIAskModePtr   pMode0, pMode1;
    CARD8           want_3cf_5a, tv_ntsc_pal, b3c5_de;
    CARD16          w2_hzoom,w2_vzoom,w2_hstart,w2_hend,w2_vstart,w2_vend,w2_rowByte;
    CARD8           w2_sync;
    CARD16          modeinfo[4], ret, modeSpec=0, temp_x, disLen;
    CARD16          yres, xres, lineBuf;
    CARD32          condition;

	int				i;

	/* Jong 09/21/2006; frame buffer address for video window 2 */
	CARD32 W2fbAddr=0;
	CARD32 dwTemp=0;

    pMode0 = pMode1 = pMode;

	/* Jong 09/15/2006; single view or first view of dual view */
    if (!dualView || dualView == 0x01)
    {
        condition = pMode0->condition;

		/* Jong 09/20/2006; set pMode1 to invalid */
		pMode1=NULL;
    }
    else /* DualView */ /* Jong 09/15/2006; second view of dual view */
    {
        pMode1 = pMode + 1;
        condition = pMode1->condition;

		/* Jong 09/20/2006; set pMode0 to invalid */
		pMode0=NULL;
    }

	/* Jong 10/04/2006; TV Status flag 2 */
    OUTB(XGI_REG_CRX, 0xC2);
    OUTB(XGI_REG_CRX+1, (int)(INB(XGI_REG_CRX+1) & ~0x11));

    if(condition & SUPPORT_TV_NATIVE)
        OUTB(XGI_REG_CRX+1, (int)(INB(XGI_REG_CRX+1) | 0x10));
    if(condition & SUPPORT_TV_OVERSCAN)
        OUTB(XGI_REG_CRX+1, (int)(INB(XGI_REG_CRX+1) | 0x01));

    tv_ntsc_pal = 0;

	/* Jong 09/20/2006; check pMode0 */
	if(pMode0)
	{
		if(pMode0->condition & ZVMX_ATTRIB_NTSCJ)
			tv_ntsc_pal = 0x20;
		if(pMode0->condition & ZVMX_ATTRIB_PALM)
			tv_ntsc_pal = 0x40;
		if(pMode0->condition & ZVMX_ATTRIB_PAL)
			tv_ntsc_pal = 0x80;

		OUTB(XGI_REG_CRX, 0xC0);
		OUTB(XGI_REG_CRX+1, ((INB(XGI_REG_CRX+1) & 0x1f) | tv_ntsc_pal));
	}

	/* Jong 09/21/2006; single view */
	/* Jong 09/21/2006; dualView == 0x01 indicate first view? right? */ 
    if (!dualView || dualView == 0x01)
    {
        /* Changing MV to single device, need to modify some MV   */
        /* configurations. For single device change, this is only */
        /* dummy processing.                                      */

		/* Jong 09/21/2006; single view */
        if (!dualView)
        {
			/* Jong 09/21/2006; close second view (Video Window 2; W2) */
            XGICloseSecondaryView(pXGI);

			/* Jong 09/21/2006;  Selects normal screen operation */
            OUTB(XGI_REG_SRX, 0x01);
            OUTB(XGI_REG_SRX+1, INB(XGI_REG_SRX+1) & ~0x20);
        }

        /* Combinate & set mode condition. */

		/* Jong 09/21/2006; want_3cf_5a indicate which devices need to be enabled and decided by condition */
        want_3cf_5a = (CARD8)(condition & 0x0000000F);

		/* Jong 09/21/2006; what is this for ? */
        if(condition & ZVMX_ATTRIB_EXPANSION)
        {
            modeSpec |= BIOS_EXPANSION;
        }
        else if(condition & ZVMX_ATTRIB_V_EXPANSION)
        {
            modeSpec |= BIOS_V_EXPANSION;
        }

		/* Jong 09/21/2006; Protection Register; Set register 0x11 to 
		   92H to unprotect all extended registers without regarding 3C5.0E bit 7. */
        OUTW(0x3C4, 0x9211);

		/* Jong 09/21/2006; Change of the display device can be done 
		   by altering this scratch pad register and followed by the set mode function call. */
        OUTB(XGI_REG_GRX, 0x5A);
        OUTB(XGI_REG_GRX+1, ((INB(XGI_REG_GRX+1) & 0xF0) | want_3cf_5a));

		/* Jong 09/21/2006; LUT read write enable */
        /* Write enable LUT0 */
        OUTB(XGI_REG_SRX, 0xDD);
        OUTB(XGI_REG_SRX+1, 0xE0);

		/* Jong 09/21/2006; what is modeSpec for ? It is used by calling BIOS to set mode */
        modeSpec |= XGIGetColorIndex(pMode0->pixelSize);
        if (modeSpec & 0x08)        /* 32 bit true color */
        {
            modeSpec |= (pMode0->modeNo & 0x0100) >> 7;        /* 10 bits */
        }

		/* Jong 09/21/2006; Read information of LUT standby and gamma enable */
        OUTB(XGI_REG_SRX, 0xDE);
        b3c5_de = INB(XGI_REG_SRX+1);

        /* Jong 09/21/2006; call BIOS to set mode and refer to BIOSInterface.doc */
        /* call BIOS */
        pXGI->pInt10->ax = 0x1200;
        pXGI->pInt10->bx = 0x0014;

		/* Jong 10/04/2006; use code segment from "dualView == 0x02" */
        /* else if (condition & DEV_SUPPORT_DVI) */
		/* VGA extended mode number */
        {
            OUTB(XGI_REG_GRX, 0x5A);
            OUTB(XGI_REG_CRX, 0x5A);
            if ((INB(XGI_REG_GRX+1) & 0x80) && (INB(XGI_REG_CRX+1) & 0x04))  /* single sync digital */
            {
                pXGI->pInt10->bx |= (CARD16)pXGI->digitalModeNo << 8;

#ifdef XGI_DUMP_DUALVIEW
				ErrorF("Jong-Debug-pXGI->pInt10->bx=0x%x--0\n", pXGI->pInt10->bx);
#endif
            }
            else
            {
#ifdef XGI_DUMP_DUALVIEW
				ErrorF("Jong-Debug-pMode0->width=%d, pMode0->height=%d--1\n", pMode0->width, pMode0->height);
				ErrorF("Jong-Debug-pMode0->modeNo=%d--1\n", pMode0->modeNo);
				ErrorF("Jong-Debug-pXGI->pInt10->bx=0x%x--1\n", pXGI->pInt10->bx);
#endif

                pXGI->pInt10->bx |= ((CARD16)XGIConvertResToModeNo(pMode0->width, pMode0->height)
                                   | (pMode0->modeNo & 0x80)) << 8;

#ifdef XGI_DUMP_DUALVIEW
				ErrorF("Jong-Debug-pXGI->pInt10->bx=0x%x--1*\n", pXGI->pInt10->bx);
#endif
            }
        }

		/* Jong 10/04/2006; use code segment instead from "dualView == 0x02" */
        // pXGI->pInt10->bx |= ((CARD16)XGIConvertResToModeNo(pMode0->width, pMode0->height)
        //                     | (pMode0->modeNo & 0x80)) << 8;

        pXGI->pInt10->cx = (modeSpec << 8) | (pMode0->refRate & 0x01FF);

        if ((pXGI->pInt10->cx & 0xFF) == 44)
        {
            /* for interlace mode, driver pass to DLL 44i or 48i */
            pXGI->pInt10->cx = (pXGI->pInt10->cx & ~0xFF) | 0xD7;
#ifdef XGI_DUMP_DUALVIEW
				ErrorF("Jong-Debug-pXGI->pInt10->cx=0x%x--0\n", pXGI->pInt10->cx);
#endif
        }
        else if((pXGI->pInt10->cx & 0xFF) == 48)
        {
            pXGI->pInt10->cx = (pXGI->pInt10->cx & ~0xFF) | 0xE0;
#ifdef XGI_DUMP_DUALVIEW
				ErrorF("Jong-Debug-pXGI->pInt10->cx=0x%x--1\n", pXGI->pInt10->cx);
#endif
        }

        pXGI->pInt10->num = 0x10;

/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-before XG47BiosModeInit()-xf86ExecX86int10()-0--\n");
	XGIDumpRegisterValue(g_pScreen);
#endif

        xf86ExecX86int10(pXGI->pInt10);

		/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-after XG47BiosModeInit()-xf86ExecX86int10()-0--\n");
	XGIDumpRegisterValue(g_pScreen);
#endif

		/* Jong 09/21/2006; why need to restore b3c5_de which is saved before calling BIOS??? */
        OUTB(XGI_REG_SRX, 0xDE);
        OUTB(XGI_REG_SRX+1, b3c5_de);

        /* Set mode fail. */
        if(pXGI->pInt10->ax & 0xFF00)   return FALSE;

		/* Jong 09/21/2006; what is this for ??? */
        pMode0->refRate = pXGI->pInt10->cx;

		/* Jong 09/21/2006; what is this for ??? */
        /* Notify expansion status to system BIOS */
        pXGI->pInt10->ax = 0x120C;
        pXGI->pInt10->bx = 0x214;
        pXGI->pInt10->cx = 0;
        if (modeSpec & BIOS_EXPANSION)
        {
            pXGI->pInt10->cx = 1;
        }
        else if (modeSpec & BIOS_V_EXPANSION)
        {
            pXGI->pInt10->cx = 2;
        }
        pXGI->pInt10->num = 0x10;

/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-before XG47BiosModeInit()-xf86ExecX86int10()-1--\n");
	XGIDumpRegisterValue(g_pScreen);
#endif

        xf86ExecX86int10(pXGI->pInt10);

/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-after XG47BiosModeInit()-xf86ExecX86int10()-1--\n");
	XGIDumpRegisterValue(g_pScreen);
#endif

		/* Jong 09/21/2006; Working status register 0 */
		/* indicate intended status of display device and will take effect after calling set mode of BIOS */
        OUTB(XGI_REG_GRX, 0x5A);
		/* Jong 09/21/2006; System status flag 1 : indicate current status of output devices */
        OUTB(XGI_REG_CRX, 0x5A);

        if(((pMode0->width == 1920) || (pMode0->width == 2048))
           && !(((condition & DEV_SUPPORT_LCD) && (pXGI->lcdWidth <= 1600))
           || ((condition & DEV_SUPPORT_DVI) && (INB(XGI_REG_GRX+1) & 0x80)
           && (INB(XGI_REG_CRX+1) & 4) && (pXGI->lcdWidth <= 1600))))
        {
			/* Jong 09/21/2006; Disable Scaling Engine Control: Horizontal interpolation */
            OUTB(XGI_REG_GRX, 0xD3);
            OUTB(XGI_REG_GRX+1, (int)(INB(XGI_REG_GRX+1) & ~0x80));

			/* Jong 09/21/2006; Disable Scaling Engine Control: Vertical interpolation */
            OUTB(XGI_REG_GRX,0xD5);
            OUTB(XGI_REG_GRX+1, (int)(INB(XGI_REG_GRX+1) & ~0x80));
        }
    }

	/* Jong 09/20/2006; check whether it's a MHS mode */
    OUTB(XGI_REG_GRX, 0x36);
    if ((INB(XGI_REG_GRX+1) & 0x02))    /* if MHS */
    {
        if (pXGI->isInterpolation)
        {
			/* Jong 09/21/2006;  Enable vertical interpolation of video Window 2 */
            OUTB(0x24aa, (CARD16)INB(0x24aa) & ~0x02);
        }
        else
        {
			/* Jong 09/21/2006;  Disable vertical interpolation of video Window 2 */
            OUTB(0x24aa, (CARD16)INB(0x24aa) | 0x02);
        }
    }

	/* Jong 09/21/2006; dualView == 0x02 indicate second view? right? */ 
    if(dualView == 0x02)
    {
        //Moved to miniport
        //XGICloseSecondaryView(pXGI);

		/* Jong 09/21/2006; what is this for ??? */
        if(condition & SUPPORT_W2_CLOSE)
        {
			/* Jong 09/21/2006;  Selects normal screen operation */
            OUTB(XGI_REG_SRX, 0x01);
            OUTB(XGI_REG_SRX+1, INB(XGI_REG_SRX+1) & ~0x20);
            return TRUE;
        }

		/* Jong 09/21/2006; used to check 0x3E4-0x5B for dual view */
        want_3cf_5a = (CARD8)(condition & 0x0000000f) << 4;

		/* Jong 09/21/2006; set flag of 0x5B for dual view */
        /* set flag for dual view */
        OUTB(XGI_REG_GRX, 0x5B);
        OUTB(XGI_REG_GRX+1, (INB(XGI_REG_GRX+1) & ~0xF0) | want_3cf_5a);

		/* Jong 10/04/2006; debug different modes for dual view */
		//--------------------------------------------------------
        modeSpec |= XGIGetColorIndex(pMode1->pixelSize);
        if (modeSpec & 0x08)        /* 32 bit true color */
        {
            modeSpec |= (pMode1->modeNo & 0x0100) >> 7;        /* 10 bits */
        }

		/* Jong 10/04/2006; test with hard code */
		/* modeSpec = 0x8; */
		//--------------------------------------------------------

		/* Jong 09/21/2006; call BIOS to set mode */
        pXGI->pInt10->ax = 0x1200;
        pXGI->pInt10->bx = 0x0014;
        if (condition & DEV_SUPPORT_LCD)
        {
            pXGI->pInt10->bx |= (CARD16)pXGI->lcdModeNo << 8;
        }
        else if (condition & DEV_SUPPORT_DVI)
        {
            OUTB(XGI_REG_GRX, 0x5A);
            OUTB(XGI_REG_CRX, 0x5A);
            if ((INB(XGI_REG_GRX+1) & 0x80) && (INB(XGI_REG_CRX+1) & 0x04))  /* single sync digital */
            {
                pXGI->pInt10->bx |= (CARD16)pXGI->digitalModeNo << 8;
            }
            else
            {
                pXGI->pInt10->bx |= ((CARD16)XGIConvertResToModeNo(pMode1->width, pMode1->height)
                                   | (pMode1->modeNo & 0x80)) << 8;
            }
        }
        else if((condition & DEV_SUPPORT_TV) && (pMode1->width > 1024))
        {
            pXGI->pInt10->bx |= (MODE_1024x768 & 0xFF00);
        }
        else
        {
			/* Jong 10/04/2006; test with hard code */
			/* pXGI->pInt10->bx = 0x6214; */
            pXGI->pInt10->bx |= ((CARD16)XGIConvertResToModeNo(pMode1->width, pMode1->height)
                               | (pMode1->modeNo & 0x80)) << 8; 
        }

		/* Jong 10/04/2006; D7	1: Set CRTC2 timing only*/
        pXGI->pInt10->cx = 0x8000 | (modeSpec << 8) | (pMode1->refRate & 0x01FF); 

        pXGI->pInt10->num = 0x10;
        xf86ExecX86int10(pXGI->pInt10); 

        /*
         * w2 (CRTC 2) Zoom & Position.
         */

		/* Jong 09/21/2006; Get horisontal start and end position of display */
        /* Get Horisontal Display Position */
        OUTB(XGI_REG_SRX, 0xC8);
        w2_hstart = (CARD16)INB(XGI_REG_SRX+1);
        OUTB(XGI_REG_SRX, 0xC9);
        w2_hstart |= (CARD16)INB(XGI_REG_SRX+1) << 8;

        OUTB(XGI_REG_SRX,0xCA);
        w2_hend = (CARD16)INB(XGI_REG_SRX+1);
        OUTB(XGI_REG_SRX,0xCB);
        w2_hend |= (CARD16)INB(XGI_REG_SRX+1) << 8;

		/* Jng 09/21/2006; Get vertical start and end position of display */
        /* Get Vertical Display Position */
        OUTB(XGI_REG_SRX,0xC4);
        w2_vstart = (CARD16)INB(XGI_REG_SRX+1);
        OUTB(XGI_REG_SRX,0xC5);
        w2_vstart |= (CARD16)INB(XGI_REG_SRX+1) << 8;

        OUTB(XGI_REG_SRX,0xC6);
        w2_vend = (CARD16)INB(XGI_REG_SRX+1);
        OUTB(XGI_REG_SRX,0xC7);
        w2_vend |= (CARD16)INB(XGI_REG_SRX+1) << 8;

		/* Jong 09/21/2006; CRTC2 SYNC Pulse Width */
        OUTB(XGI_REG_SRX,0xCC);
        w2_sync = INB(XGI_REG_SRX+1);

        /* Adjust Overlay position
         * W2_Hstart = HDE start - 50 = (3C5.C9-C8) - 50
         * W2_Hend = HDE end - 51 = (3C5.CB-CA) - 51
         * W2_Vstart = VDE start - 6 = (3C5.C5-C4) - 4
         * W2_Vend = VDE end -2 = (3C5.C7-C6)
         * Note: 1. Flick disbled, 24B0.[2:0] = 0
         *      2. W2 HDE is 0, 24AA.[7:4] = 0
         */
        w2_hstart -= 0x032;
        w2_hend   -= 0x033; /* Keep as same as DE. */
        w2_vstart -= 0x04;

        xres = pMode1->width;
        yres = pMode1->height;

        if (condition & DEV_SUPPORT_LCD) /* Panel mode */
        {
            if(condition & ZVMX_ATTRIB_EXPANSION)
            {
                /* Horizontal Zoom */
                if (xres > pXGI->lcdWidth)
                {
                    xres = pXGI->lcdWidth;
                }

                w2_hzoom = (CARD16)((CARD32)(xres-1) * 1024 / (pXGI->lcdWidth - 1));
                temp_x = (CARD16)((xres-1) * 1024 % (pXGI->lcdWidth - 1));
                if (temp_x >= ((pXGI->lcdWidth - 1) >> 1))
                {
                    w2_hzoom++;
                    if (temp_x <= ((pXGI->lcdWidth - 1) * 9 / 10))
                        w2_hend--;
                }
            }
            else
            {
                w2_hzoom = 1024;
                disLen = (CARD16)(pXGI->lcdHeight/3)*4;
                if ((disLen < pXGI->lcdWidth) && (xres < disLen) && (condition & ZVMX_ATTRIB_V_EXPANSION))
                {
                    w2_hstart += (CARD16)((pXGI->lcdWidth - disLen)>>1);
                    w2_hend = (CARD16)(w2_hstart + disLen) -1;
                    w2_hzoom = (CARD16)((CARD32)(xres-1) * 1024 / (disLen-1));
                    temp_x = (CARD16)((xres-1) * 1024 % (disLen-1));
                    if (temp_x >= ((disLen-1) >> 1))
                    {
                        w2_hzoom++;
                    }
                    else
                    {
                        w2_hend++;
                    }
                }
                else if (xres < pXGI->lcdWidth)
                {
                    w2_hstart += (CARD16)((pXGI->lcdWidth - xres)>>1);
                    w2_hend   -= (CARD16)((pXGI->lcdWidth - xres)>>1);
                }
            }

            if(condition & (ZVMX_ATTRIB_EXPANSION | ZVMX_ATTRIB_V_EXPANSION))
            {
                /* Vertical Zoom */
                if (yres > pXGI->lcdHeight)
                    yres = pXGI->lcdHeight;

                w2_vzoom = (CARD16)((CARD32)(yres-1) * 1024 / (pXGI->lcdHeight-1));
                if ((CARD32)(yres-1) * 1024 % (pXGI->lcdHeight-1))
                    w2_vzoom++;
                if (((CARD32)(yres-1) * 1024 % (pXGI->lcdHeight-1)) &&
                     ((CARD32)(yres-1) * 1024 % (pXGI->lcdHeight-1)) < (CARD32)((pXGI->lcdHeight-1) >> 1))
                    w2_vend--;
            }
            else
            {
                if (yres < pXGI->lcdHeight)
                {
                    w2_vstart += (CARD16)((pXGI->lcdHeight - yres)>>1);
                    w2_vend   -= (CARD16)((pXGI->lcdHeight - yres)>>1);
                }
                w2_vzoom = 1024;
            }
        }
        else if (condition & DEV_SUPPORT_DVI) /* DVI mode */
        {
            OUTB(XGI_REG_GRX,0x5a);
            OUTB(XGI_REG_CRX,0x5a);
            if((INB(XGI_REG_GRX+1) & 0x80) && (INB(XGI_REG_CRX+1) & 0x04))  // single sync digital
            {
                w2_hend--;
                /* Horizontal Zoom */
                if (xres > pXGI->digitalWidth)
                    xres = pXGI->digitalWidth;

                w2_hzoom = (CARD16)((CARD32)(xres-1) * 1024 / (pXGI->digitalWidth - 1));
                if (((CARD32)(xres-1) * 1024 % (pXGI->digitalWidth - 1)) >= (CARD32)((pXGI->digitalWidth - 1) >> 1))
                {
                    w2_hzoom++;
                    w2_hend--;
                }

                /* Vertical Zoom */
                if (yres > pXGI->digitalHeight)
                    yres = pXGI->digitalHeight;

                w2_vzoom = (CARD16)((CARD32)(yres-1) * 1024 / (pXGI->digitalHeight -1));
                if ((CARD32)(yres-1) * 1024 % (pXGI->digitalHeight - 1))
                    w2_vzoom++;
                if (((CARD32)(yres-1) * 1024 % (pXGI->digitalHeight - 1)) &&
                     ((CARD32)(yres-1) * 1024 % (pXGI->digitalHeight - 1)) < (CARD32)((pXGI->digitalHeight - 1) >> 1))
                    w2_vend--;
            }
            else
            {
                if (xres == 320 || xres == 400 || xres == 512)
                {
                    w2_hzoom = 512;
                    w2_vzoom = 512;
                }
                else if (xres == 720) /* scale up to 800x600 */
                {
                    w2_hzoom = 0x399;
                    if(yres == 576)
                        w2_vzoom = 0x3d7;
                    else  /* 480 */
                        w2_vzoom = 0x333;
                }
                else
                {
                    w2_hzoom = 1024;
                    w2_vzoom = 1024;
                }
            }
        }
        else if (condition & DEV_SUPPORT_CRT) /* CRT mode */
        {
            if (xres == 320 || xres == 400 || xres == 512)
            {
                w2_hzoom = 512;
                w2_vzoom = 512;
            }
            else
            {
                w2_hzoom = 1024;
                w2_vzoom = 1024;
            }
        }
        else /* TV */
        {
            if (xres > 1024)
            {
                xres = 1024;
                yres = 768;
            }

            if (xres == 320 || xres == 400 || xres == 512)
            {
                w2_hzoom = 512;
                w2_vzoom = 512;
            }
            else
            {
                w2_hzoom = 1024;
                w2_vzoom = 1024;
            }

            w2_hstart += 0x01A;
            w2_hend   += 0x019;

            w2_sync = (w2_sync>>1) + 8;
            OUTB(XGI_REG_SRX,0xcc);
            OUTB(XGI_REG_SRX+1, w2_sync);

            OUTB(XGI_REG_CRX,0xc0);
            if(!(INB(XGI_REG_CRX+1) & 0x80))
            {
                w2_vstart -= 0x0c;
                w2_vend -= 0x0c;
            }
        }

        temp_x = (CARD16)xres & 0xfff0;
        if(((CARD16)xres & 0x000f)!=0) temp_x+=0x10;
        w2_rowByte = (CARD16)(temp_x * (pMode1->pixelSize >> 3)) >> 4;

        lineBuf = xres >> 3;

        if(pMode1->pixelSize == 8)
        {
            lineBuf >>= 1;
        }
        else if(pMode1->pixelSize == 16)
        {
            /* Disable dithering */
            OUTB(XGI_REG_GRX, 0x42);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x08);
        }
        else if(pMode1->pixelSize == 32)
            lineBuf <<= 1;
        lineBuf++;
        if(lineBuf > 0x3FF) lineBuf = 0x3FF;    /* Overflow */

        OUTW(0x3C4, 0x9211);

        /* W2 starting address default = 0 */
		/* Jong 09/21/2006; Window 2 Starting address of (Y) frame buffer */
		/* We need to give an different address than (0,0) if MHS is required */
		/* Otherwise; (0,0) is correct for simultaneous and content mode */

		/* Jong 09/26/2006; not necessary */
		W2fbAddr=pXGI->fbAddr; /* total 25 bits is effective */
		/* W2fbAddr=(pXGI->fbAddr) & 0x01FFFFFF; */ /* total 25 bits is effective */
		/* dwTemp=(CARD32)INB(0x2483) & 0xFE;
		W2fbAddr=(dwTemp<<24) | W2fbAddr; */

		/* Jong 09/26/2006; test */
		/* W2fbAddr=0x2000000;*/ /* 32MB */ /* Jong 10/02/2006; use pXGI->fbAddr instead */
		XG47SetW2ViewBaseAddr(pScrn, (unsigned long)W2fbAddr); 

/* Jong 09/26/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-After calling- XG47SetW2ViewBaseAddr()-W2fbAddr=0x%x\n", W2fbAddr);
	XGIDumpRegisterValue(pScrn);
#endif

#if 0
        OUTW(0x2480, 0x0);
        OUTB(0x2482, 0x0);
		/* Jong 09/21/2006; total 25 bits for address and set them all to zero */
        OUTB(0x2483, (CARD16)INB(0x2483) & ~0x01);
#endif

        /* W2 zooming factor */
        OUTW(0x249c, w2_hzoom);
        OUTW(0x24a0, w2_vzoom);

        /* W2 start/end */
        OUTW(0x2494, w2_hstart);
        OUTW(0x2496, w2_hend);
        OUTW(0x2498, w2_vstart);
        OUTW(0x249a, w2_vend);

        /* W2 row byte */
        OUTW(0x248c, w2_rowByte);

        /* Line buffer */
        OUTW(0x2490, lineBuf);

        /* disable color key mask for video */
        OUTW(0x24bc, 0x0);
        OUTW(0x24be, 0x0);

        OUTB(XGI_REG_GRX, 0x5D);
        modeSpec = (CARD8)INB(XGI_REG_GRX+1); /* LCD Expansion/Centering */

		ErrorF("Jong-Debug-3CE-0x5D-modeSpec=0x%x\n", modeSpec);

		/* Jong 09/21/2006; set to RGB mode */
		/* 0x24a9:[4]: W2_CSCPASS
                    1: window2 RGB format
                    *0: window2 YUV format
		*/
        OUTB(0x24a9, ((CARD16)INB(0x24a9) & ~0x0F) | 0x1A); /* CSCPASS, RGB WINMD */

        if ((!(modeSpec & GRAF_EXPANSION))||(pMode1->pixelSize == 8))
        {
            OUTB(0x24aa, (CARD16)INB(0x24aa) | 0x01); /* HINTEN, disable */
        }
        else
        {
            OUTB(0x24aa, (CARD16)INB(0x24aa) & ~0x01); /* HINTEN, enable */
        }

        OUTB(0x24aa, (CARD16)INB(0x24aa) | 0x02);
        if ((condition & DEV_SUPPORT_LCD) && (pMode1->width < pXGI->lcdWidth))
        {
            if(pXGI->isInterpolation)
                OUTB(0x24aa, INB(0x24aa) & ~0x02);
        }

        if(pMode1->pixelSize == 8)
        {
            OUTB(0x24a8, (INB(0x24a8) & ~0x07) | 0x01);
        }
        else if(pMode1->pixelSize == 16)
        {
            OUTB(0x24a8, (INB(0x24a8) & ~0x07) | 0x02);
        }
        else
        {
            if (pMode0->modeNo & 0x0100)    /* 10 bits */
            {
                OUTB(0x24a8, INB(0x24a8) | 0x07);
            }
            else
            {
                OUTB(0x24a8, (INB(0x24a8) & ~0x07) | 0x04);
            }
        }

        OUTB(0x24aa, (INB(0x24aa) & ~0x70)); /* HDE Adjust */

        /* sharpness */
        OUTB(0x24ab, 0x10);

        /*disable sub-picture, and will correct 2nd view color */
        OUTB(0x2470, (CARD16)INB(0x2470) & ~0x06);

        OUTB(XGI_REG_GRX, 0x81);
        OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x80);
        /* Enable Window 2 */
        OUTB(XGI_REG_GRX, 0x81);
        OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x01);

        /* Turn on video engine memory clock */
        OUTB(XGI_REG_GRX, 0xDA);
        OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x10);

        /* Turn on video engine pixel clock */
        OUTB(XGI_REG_CRX, 0xBE);
        OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) | 0x04);

        /* Turn on video engine clock for w2 */
        OUTB(XGI_REG_SRX, 0x52);
        OUTB(XGI_REG_SRX+1, INB(XGI_REG_SRX+1) | 0x40);

        if (want_3cf_5a & (DEV_SUPPORT_LCD << 4))
        {
            /* W2 on LCD */
            OUTB(XGI_REG_SRX, 0xBE);
            OUTB(XGI_REG_SRX+1,(int)INB(XGI_REG_SRX+1) | 0x08);

            /* Set DE delay for CRTC2 */
            OUTB(XGI_REG_GRX, 0x45);
            OUTB(XGI_REG_GRX+1, (INB(XGI_REG_GRX+1) & ~0x07)|(GR3CE_45 & 0x07));
        }

        if (want_3cf_5a & (DEV_SUPPORT_TV << 4))
        {
            /* W2 on TV */
            OUTB(XGI_REG_CRX, 0xD6);
            OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) | 0x04);

            /* UV order */
            OUTB(XGI_REG_SRX, 0xD8);
            OUTB(XGI_REG_SRX+1, 0x11);

            /* Adjust VDHLOAD */
            OUTB(XGI_REG_CRX, 0xD2);
            temp_x = (CARD16)INB(XGI_REG_CRX+1);
            OUTB(XGI_REG_CRX, 0xD3);
            temp_x |= (CARD16)(INB(XGI_REG_CRX+1) & 0x0F) << 8;

            OUTB(XGI_REG_CRX, 0xC0);
            if(INB(XGI_REG_CRX+1) & 0x80)
                temp_x += 0x14;
            //else
            //    temp_x += 0x04;

            OUTB(XGI_REG_CRX, 0xD2);
            OUTB(XGI_REG_CRX+1, temp_x);
            OUTB(XGI_REG_CRX, 0xD3);
            OUTB(XGI_REG_CRX+1, (INB(XGI_REG_CRX+1) & 0xF0) | (temp_x >> 8));

        }

        if (want_3cf_5a & (DEV_SUPPORT_DVI << 4))
        {
            /* W2 on DVI, driving strength */

            /* TMDS Power, disable internal TMDS */
            OUTB(XGI_REG_GRX, 0x3D);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x01);

            /* LCD2 HS Delay */
            OUTB(XGI_REG_GRX, 0x46);
            OUTB(XGI_REG_GRX+1, (INB(XGI_REG_GRX+1) & ~0x38) | 0x28);

            /* LCD2 clock polarity
            OUTB(XGI_REG_GRX, 0x43);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) & ~0x08);
            */

            OUTB(XGI_REG_CRX, 0xD6);
            OUTB(XGI_REG_CRX+1, INB(XGI_REG_CRX+1) & ~0x10);

            OUTB(XGI_REG_GRX, 0x2A);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x40);
        }

		/* Jong 09/21/2006; set second view to CRT1/DVI2 with 0x3CE/0x3CF-0x2C:[6]*/
		/* DEV_SUPPORT_CRT = 0x0002; (DEV_SUPPORT_CRT << 4) =  0x0020 */
		/* why to check want_3cf_5a but not want_3cf_5b ??? */
        if (want_3cf_5a & (DEV_SUPPORT_CRT << 4))
        {
            OUTB(XGI_REG_GRX, 0x2C);
            OUTB(XGI_REG_GRX+1, INB(XGI_REG_GRX+1) | 0x40);
        }

        OUTB(0x24c2,(CARD16)INB(0x24c2) | 0x20);  /* MC5 */
    }

    /*
     * TV
     */
    if((pXGI->biosDevSupport & SUPPORT_DEV_TV)
        && !(pXGI->biosDevSupport & SUPPORT_CURRENT_NO_TV)
        && (condition & DEV_SUPPORT_TV))
    {
        if (dualView == 0x02)
        {
            modeinfo[0] = (CARD16)pMode1->width;
            modeinfo[1] = (CARD16)pMode1->height;
            modeinfo[2] = (CARD16)pMode1->pixelSize;
        }
        else
        {
            modeinfo[0] = (CARD16)pMode0->width;
            modeinfo[1] = (CARD16)pMode0->height;
            modeinfo[2] = (CARD16)pMode0->pixelSize;
        }

        if(condition & SUPPORT_TV_NATIVE)
        {
            if (condition & ZVMX_ATTRIB_PAL)
            {
                if (modeinfo[0] >= 800 && modeinfo[1] > 514)
                {
                    modeinfo[0] = 800;
                    modeinfo[1] = 514;
                }
            }
            else
            {
                if (modeinfo[0] >= 640 && modeinfo[1] > 432)
                {
                    modeinfo[0] = 640;
                    modeinfo[1] = 432;
                }
            }
        }
        else
        {
            if (modeinfo[0] >= 1024 && modeinfo[1] >= 768)
            {
                modeinfo[0] = 1024;
                modeinfo[1] = 768;
            }
        }
        modeinfo[3] = 1;                    //only one true color format(32bit)

        if (pXGI->pBiosDll->biosDtvCtrl)
            (*pXGI->pBiosDll->biosDtvCtrl)(pScrn,
                                          (unsigned long)INIT_TV_SCREEN,
                                          (unsigned long *)&modeinfo[0],
                                           NULL);

    }

    return TRUE;
}


int XG47BiosSpecialFeature(ScrnInfoPtr pScrn,
                           unsigned long cmd,
                           unsigned long *pInBuf,
                           unsigned long *pOutBuf)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    XGIAskModePtr   pMode;
    CARD16          *x;
    CARD32          *xd;

    switch(cmd)
    {
    case GET_FLAT_PANEL_INFORMATION:
        x = (CARD16 *)pOutBuf;
        //x[0] = wPanelSize;
        x[1] = pXGI->lcdWidth;
        x[2] = pXGI->lcdHeight;
        x[3] = pXGI->lcdType;
        x[4] = pXGI->lcdRefRate;
        return TRUE;

    case GET_PANEL_LID_INFORMATION:
        xd = (CARD32 *)pOutBuf;
        pXGI->pInt10->ax = 0x1205;
        pXGI->pInt10->bx = 0x0014;
        pXGI->pInt10->num = 0x10;
        xf86ExecX86int10(pXGI->pInt10);

        if((pXGI->pInt10->ax & 0xFF00)!=0)
            xd[0] = LCD_LID_STATUS_UNKNOWN;
        else if (pXGI->pInt10->cx & 0x02)
            xd[0] = LCD_LID_CLOSE;
        else
            xd[0] = LCD_LID_OPEN;
        return TRUE;

    case GET_DEV_SUPPORT_INFORMATION:
        xd = (CARD32 *)pOutBuf;
        xd[0] = XGIGetSetChipSupportDevice(pXGI, 0x03, 0x0);
        return TRUE;

    case CLOSE_ALL_DEVICE:
        x = (CARD16 *)pInBuf;
        OUTW(0x3C4, 0x9211);
        XG47CloseAllDevice(pXGI, (CARD8)x[0]);
        return TRUE;

    case OPEN_ALL_DEVICE:
        x = (CARD16 *)pInBuf;
        OUTW(0x3C4, 0x9211);
        XG47OpenAllDevice(pXGI, (CARD8)x[0]);
        return TRUE;

    case GET_TV_INFORMATION:

        {
            CARD32 dwNewIF = 0;

            // Query DTV to know new interface or not.
            if (pXGI->pBiosDll->biosDtvCtrl)
            {
                (*pXGI->pBiosDll->biosDtvCtrl)(pScrn, (CARD32)GET_NEW_TV_INTERFACE, 0, (unsigned long*)&dwNewIF);
            }
            xd = (CARD32*)pOutBuf;
            xd[0] = pXGI->dtvInfo;
            if (dwNewIF) xd[0] |= TV_NEW_INTERFACE;
        }
        return TRUE;

    case GET_TV_PHYSICAL_SIZE:
        xd=(CARD32 *)pInBuf;
        pXGI->pInt10->ax = 0x1280;
        pXGI->pInt10->bx = 0x0214;
        if(xd[0]==0) pXGI->pInt10->bx |= 0x1000;   //PAL
        if(xd[1]!=0) pXGI->pInt10->bx |= 0x0100;   //TV Native mode
        if(xd[2]!=0) pXGI->pInt10->bx |=0x4000;   //multiview
        pXGI->pInt10->num = 0x10;
        xf86ExecX86int10(pXGI->pInt10);

        if((pXGI->pInt10->ax & 0xFF00)!=0) return FALSE;
        if(pXGI->pInt10->bx <= 50) return FALSE;  //For old BIOS !!!
        xd=(CARD32 *)pOutBuf;
        xd[0]=(CARD32)pXGI->pInt10->bx;        //H-Size
        xd[1]=(CARD32)pXGI->pInt10->cx;        //V-Size
        return TRUE;

    // Zdu, 10/8/98, Close second view for DOS FULL Screen
    case CLOSE_SECOND_VIEW:
        XGICloseSecondaryView(pXGI);
        return TRUE;

    case GET_AVAILABLE_BANDWIDTH:
        x = (CARD16 *)pOutBuf;
        x[0] = pXGI->maxBandwidth;
        return TRUE;

    case GET_MODE_VCLOCK:
        pMode = (XGIAskModePtr)pInBuf;
        x = (CARD16 *)pOutBuf;
        if(pMode->condition & DEV_SUPPORT_LCD)
            x[0] = XGIGetVClock_BandWidth(pXGI,
                                          pXGI->lcdWidth,
                                          pXGI->lcdHeight,
                                          pMode->pixelSize,
                                          (CARD16)(pXGI->lcdRefRate), 1);
        else
            x[0] = XGIGetVClock_BandWidth(pXGI,
                                          pMode->width,
                                          pMode->height,
                                          pMode->pixelSize,
                                          (CARD16)(pMode->refRate),
                                          (CARD8)(pMode->condition & 0x0F));
        return TRUE;

    default:
            break;
    }

    if((pXGI->biosOrgDevSupport & 0x000F0000) && cmd >= INIT_TV_SCREEN)
    {
        return -1; /* Which means let DTV do it! */
    }
    else
        return FALSE;
}

/*
 * Get Frame buffer size.
 */
CARD32 XG47BiosGetFreeFbSize(ScrnInfoPtr pScrn, Bool isAvailable)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

    if(isAvailable)
        return pXGI->freeFbSize;
    else
        return pXGI->biosFbSize;
}

Bool XG47BiosDTVControl(ScrnInfoPtr pScrn,
                        unsigned long cmd,
                        unsigned long * pInBuf,
                        unsigned long * pOutBuf)
{
    XGIPtr      pXGI = XGIPTR(pScrn);

    XGIDigitalTVRec *z, *w;
    CARD32          *x, *y, *w1;
    CARD16          *piWord, *poWord;

    x  = (CARD32*)pInBuf;
    z  = (XGIDigitalTVRec *)(x+1);

    y = (CARD32*)pOutBuf;
    w1 = y+1;
    w  = (XGIDigitalTVRec *)(y+1);

    piWord = (CARD16*)pInBuf;
    poWord = (CARD16*)pOutBuf;

    switch(cmd)
    {
    case ENABLE_TV_DISPLAY:
        XG47ControlTVDisplay(pXGI, TRUE);
        break;
    case DISABLE_TV_DISPLAY:
        XG47ControlTVDisplay(pXGI, FALSE);
        break;
    case INIT_TV_SCREEN:
        XG47InitTVScreen(pXGI, piWord[0],piWord[1],piWord[2],piWord[3]);
        break;
    case DETECT_TV_CONNECTION:
        *y  = 0x01;
        *w1 = XG47DetectTVConnection(pXGI);
        break;
    case CONVERT_PAL_NTSC:
        *y = 0x01;
        XG47ConvertTVMode(pXGI, (CARD16)x[0]);
        break;
    case GET_TV_SCREEN_POSITION:
        *y = 0x01;
        if(!XG47GetTVScreenPosition(pXGI, w, x[0]))
            return FALSE;
        break;
    case SET_TV_SCREEN_POSITION:
        *y = (CARD32)XG47SetTVScreenPosition(pXGI, z, x[0]);
        break;
    case GET_TV_COLOR_INFORMATION:
        *y = 0x01;
        if(!XG47GetTVColorInformation(pXGI, w, x[0]))
            return FALSE;
        break;
    case SET_TV_COLOR_INFORMATION:
        *y = (CARD32)XG47SetTVColorInformation(pXGI, z, x[0]);
        break;
    case GET_NEW_TV_INTERFACE:
        y[0] = 1; // Support here.
        break;
    case LINE_BEATING:
        XG47LineBeating(pXGI, (Bool)x[0]);
        break;
    case SAVE_PAL_NTSC_TO_CMOS:
        *y = 0x01;
        XG47UpdateCMOS(pXGI, (CARD16)x[0]);
        break;
    default:
        return FALSE;
    }

    return TRUE;
}
