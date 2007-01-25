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

/* Jong 09/15/2006; support dual view; default is single mode */
int		g_DualViewMode=0;

/* Jong 10/04/2006; support different modes for dual view */
XGIAskModeRec	g_ModeOfFirstView;

/*
 * Pixel(Video) clock information.
 */
XGIPixelClockRec XG47ModeVClockTable[]={
    {320, 200, 60, 13},
    {320, 200, 70, 13},
    {320, 240, 60, 13},
    /*{320, 240, 72, 16},*/
    {320, 240, 75, 16},
    {320, 240, 85, 18},
    {400, 300, 60, 20},
    {400, 300, 75, 25},
    {400, 300, 85, 28},
    /*{512, 384, 44, 23},*/
    {512, 384, 60, 33},
    /*{512, 384, 70, 38},*/
    {512, 384, 75, 39},
    {512, 384, 85, 48},
    {640, 400, 60, 25},
    {640, 400, 70, 25},
    {640, 400, 85, 31},
    {640, 432, 60, 25},
    {640, 480, 60, 25},
    /*{640, 480, 72, 31},*/
    {640, 480, 75, 32},
    {640, 480, 85, 36},
    {640, 480,100, 43},
    {640, 480,120, 52},
    {720, 480, 60, 28},
    {720, 540, 60, 30},
    {720, 576, 60, 32},
    {800, 514, 60, 40},
    {800, 600, 60, 40},
    {800, 600, 75, 50},
    {800, 600, 85, 56},
    {800, 600,100, 68},
    {800, 600,120, 84},
    {848, 480, 60, 31},
    {1024, 600, 60, 51},
    {1024, 768, 44, 45},
    {1024, 768, 60, 65},
    {1024, 768, 70, 75},
    {1024, 768, 75, 79},
    {1024, 768, 85, 95},
    {1024, 768,100,113},
    {1024, 768,120,139},
    {1152, 864, 60, 81},
    {1152, 864, 75,108},
    {1152, 864, 85,119},
    {1280, 600, 60,108},
    {1280, 768, 60, 74},
    {1280, 768, 60, 79},
    {1280, 800, 60,101},
    {1280, 960, 60,101},
    {1280, 960, 75,130},
    {1280, 960, 85,148},
    {1280,1024, 44, 78},
    {1280,1024, 60,108},
    {1280,1024, 75,135},
    {1280,1024, 85,158},
    {1280,1024,100,191},
    {1280,1024,120,234},
    {1400,1050, 60,122},
    {1400,1050, 75,155},
    {1400,1050, 85,179},
    {1440, 900, 60,129},
    {1600,1200, 48,130},
    {1600,1200, 0x13C,130},
    {1600,1200, 60,162},
    {1600,1200, 75,203},
    {1600,1200, 85,230},
    {1600,1200,100,281},
    {1680,1050, 60,178},
    {1920,1080, 0xbc,74},
    {1920,1080, 60,148},
    {1920,1200, 0x13C,154},
    {1920,1200, 60,234},
    {1920,1440, 60,234},
    {1920,1440, 75,297},
    {2048,1536, 60,267},
    {2048,1536, 75,340}
};

int XG47ModeVClockTableSize = sizeof(XG47ModeVClockTable) / sizeof(XGIPixelClockRec);

XGIPixelClockRec XG47ModeVClockTable2[]={
    {1600,1200,60,130},
    {1920,1200,60,154}
};
int XG47ModeVClockTableSize2 = sizeof(XG47ModeVClockTable2) / sizeof(XGIPixelClockRec);

/*
 * XG47 mode table
 */
XGIModeRec XG47ModeTable[] = {
{ MODE_320x200, 320, 200,
  { VMODE_REF_75Hz+VMODE_REF_60Hz,
    VMODE_REF_70Hz,
    VMODE_REF_60Hz,
    VMODE_REF_70Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_320x240, 320, 240,
  { VMODE_REF_75Hz+VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_75Hz+VMODE_REF_60Hz,
    VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_75Hz+VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_400x300, 400, 300,
  { VMODE_REF_75Hz+VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_75Hz+VMODE_REF_60Hz,
    VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_75Hz+VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_512x384, 512, 384,
  { VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_75Hz+VMODE_REF_60Hz+VMODE_REF_44Hz,
    VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_75Hz+VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_640x400, 640, 400,
  { VMODE_REF_75Hz+VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_70Hz,
    VMODE_REF_60Hz,
    VMODE_REF_85Hz+VMODE_REF_70Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_640x432, 640, 432,
  { 0x0000, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_640x480, 640, 480,
  { VMODE_REF_75Hz+VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },    /* Open All */
  { 0,0,0,0 }, { 0 }
},
{ MODE_720x480, 720, 480,
  { VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_720x576, 720, 576,
  { VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_800x514, 800, 514,
  { 0x0000, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_800x600, 800, 600,
  { VMODE_REF_75Hz+VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },   /* Open All */
  { 0,0,0,0 }, { 0 }
},
{ MODE_848X480, 848, 480,
  { 0x0000, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1024x600, 1024, 600,
  { VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1024x768, 1024, 768,
  { VMODE_REF_75Hz+VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },    /* Open All */
  { 0,0,0,0 }, { 0 }
},
{ MODE_1152x864,  1152, 864,
  { VMODE_REF_60Hz, VMODE_REF_60Hz+VMODE_REF_75Hz+VMODE_REF_85Hz,
    VMODE_REF_60Hz, VMODE_REF_60Hz+VMODE_REF_75Hz+VMODE_REF_85Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1280x600, 1280, 600,
  { VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1280X720, 1280, 720,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1280x768,  1280, 768,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },   /* Open All */
  { 0,0,0,0 }, { 0 }
},
{ MODE_1280x800,  1280, 800,
  { VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1280x960,  1280, 960,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },   /* Open All */
  { 0,0,0,0 }, { 0 }
},
{ MODE_1280x1024, 1280,1024,
  { VMODE_REF_75Hz+VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },    /* Open All */
  { 0,0,0,0 }, { 0 }
},
{ MODE_1440x900,  1440, 900,
  { VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz, VMODE_REF_60Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1400x1050, 1440, 1050,
  { VMODE_REF_60Hz, VMODE_REF_60Hz+VMODE_REF_75Hz+VMODE_REF_85Hz,
    VMODE_REF_60Hz, VMODE_REF_60Hz+VMODE_REF_75Hz+VMODE_REF_85Hz },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1600x1200, 1600,1200,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },   /* Open All */
  { 0,0,0,0 }, { 0 }
},
{ MODE_1680x1050, 1680, 1050,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1920X1080,  1920, 1080,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1920x1200, 1920, 1200,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },
  { 0,0,0,0 }, { 0 }
},
{ MODE_1920x1440, 1920,1440,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },
  { 0,0,0,0 }, { 0 }
},
{ MODE_2048x1536, 2048,1536,
  { VMODE_REF_60Hz, 0xffff, VMODE_REF_60Hz, 0xffff },
  { 0,0,0,0 }, { 0 }
},
{ MAX_MODE_NO, MAX_MODE_INT, MAX_MODE_INT,
  { 0xFFFF,0xFFFF,0xFFFF,0xFFFF },
  { 0xFFFF,0xFFFF,0xFFFF,0xFFFF }, { 0xFFFF }
},
{ MAX_MODE_NO, MAX_MODE_INT, MAX_MODE_INT,
  { 0xFFFF,0xFFFF,0xFFFF,0xFFFF },
  { 0xFFFF,0xFFFF,0xFFFF,0xFFFF }, { 0xFFFF }
},
{ MAX_MODE_NO, MAX_MODE_INT, MAX_MODE_INT,
  { 0xFFFF,0xFFFF,0xFFFF,0xFFFF },
  { 0xFFFF,0xFFFF,0xFFFF,0xFFFF }, { 0xFFFF }
},
};

int XG47ModeTableSize = sizeof(XG47ModeTable) / sizeof(XGIModeRec);

void XG47LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies,
                     LOCO *colors, VisualPtr pVisual)
{
    vgaHWPtr pVgaHW = VGAHWPTR(pScrn);
    XGIPtr   pXGI = XGIPTR(pScrn);
    int      i, index;

    for(i = 0; i < numColors; i++)
    {
        index = indicies[i];
        OUTB(0x3C6, 0xFF);
        DACDelay(pVgaHW);
        OUTB(0x3c8, index);
        DACDelay(pVgaHW);
        OUTB(0x3c9, colors[index].red);
        DACDelay(pVgaHW);
        OUTB(0x3c9, colors[index].green);
        DACDelay(pVgaHW);
        OUTB(0x3c9, colors[index].blue);
        DACDelay(pVgaHW);
    }
}

void XG47SetOverscan(ScrnInfoPtr pScrn, int overscan)
{
    vgaHWPtr pVgaHW = VGAHWPTR(pScrn);

    if (overscan < 0 || overscan > 255)
        return;

    pVgaHW->enablePalette(pVgaHW);
    pVgaHW->writeAttr(pVgaHW, OVERSCAN, overscan);
    pVgaHW->disablePalette(pVgaHW);
}

unsigned int XG47DDCRead(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);
    CARD8 temp;

    /* Define SDA as input */
    OUTW(0x3D4, (0x04 << 8) | 0x37);

    /* Wait until vertical retrace is in progress. */
    while (INB(0x3DA) & 0x08);
    while (!(INB(0x3DA) & 0x08));

    /* Get the result */
    OUTB(0x3D4, 0x37);
    return ( INB(0x3D5) & 0x01 );
}

void XG47ModeSave(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
    /*int     i = 0;*/
    CARD8   modeNo;

/*
    for ( i = 0; i < 0x100; i++)
    {
        pXGIReg->regs3C5[i] = IN3C5B(i);
        pXGIReg->regs3X5[i] = IN3X5B(i);
        pXGIReg->regs3CF[i] = IN3CFB(i);
    }
*/
    pXGI->textModeNo = 0;

    modeNo = IN3CFB(0x5C);
    if (modeNo > MAX_VGA_MODE_NO)
    {
        pXGI->pInt10->ax = 0x4F03;
        pXGI->pInt10->num = 0x10;
        xf86ExecX86int10(pXGI->pInt10);
        pXGI->isVGAMode = FALSE;
        pXGI->textModeNo = pXGI->pInt10->bx;
    }
    else
    {
        pXGI->isVGAMode  = TRUE;
        pXGI->textModeNo = modeNo;
    }
}

void XG47ModeRestore(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

	/* Jong 09/28/2006; restore to single view */
    XGICloseSecondaryView(pXGI);

    if (pXGI->isVGAMode)
    {
        pXGI->pInt10->ax = pXGI->textModeNo;
        pXGI->pInt10->num = 0x10;
        xf86ExecX86int10(pXGI->pInt10);
    }
    else
    {
        pXGI->pInt10->ax = 0x4F02;
        pXGI->pInt10->bx = pXGI->textModeNo;
        pXGI->pInt10->num = 0x10;
        xf86ExecX86int10(pXGI->pInt10);
    }
/*
    int     i = 0;
    for (i = 0; i < 0x100; i++)
    {
        OUT3C5B(i, pXGIReg->regs3C5[i]);
        OUT3X5B(i, pXGIReg->regs3X5[i]);
        OUT3CFB(i, pXGIReg->regs3CF[i]);
    }
*/
}

void XG47SetCRTCViewStride(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    unsigned long   screenStride = 0;

    /* displayWidth * BPP / 8 */
    screenStride = ((pScrn->displayWidth * (pScrn->bitsPerPixel >> 3)) >> 3);

    Out3x5(0x13, (CARD8) screenStride);
    Out3x5(0x8B, (In3x5(0x8B) & 0xC0) | (CARD8)((screenStride >> 8) & 0x3F));
}

void XG47SetCRTCViewBaseAddr(ScrnInfoPtr pScrn, unsigned long startAddr)
{
    XGIPtr       pXGI = XGIPTR(pScrn);

    /* Set base address for CRTC
     *
     * It should be guaranteed that the base address is aligned with 32 (8 pixels x 4 bpp) bytes.
     * so we needn't to set offset.
     */

    if (!pXGI->noAccel) pXGI->pXaaInfo->Sync(pScrn);

    startAddr >>= 2;

    Out3x5(0x0d, (CARD8)startAddr);
    Out3x5(0x0c, (CARD8)(startAddr >> 0x08));
    Out3x5(0x8C, (CARD8)(startAddr >> 0x10));
    Out3x5(0x8D, (CARD8)(startAddr >> 0x18));
}

void XG47SetW2ViewStride(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    unsigned long   screenStride = 0;

    screenStride = (screenStride + 0x07) >> 4;
    Out(0x248C, (CARD8)(screenStride));
    Out(0x248D, (CARD8)((In(0x248D) & 0xF0) | ((screenStride >> 8) & 0xF)));
}

void XG47SetW2ViewBaseAddr(ScrnInfoPtr pScrn, unsigned long startAddr)
{
    XGIPtr      pXGI = XGIPTR(pScrn);

    /* Set Second View starting Address. */
    startAddr = (startAddr + 0x07) >> 4;
    Out (0x2480, (CARD8)(startAddr & 0xFF));
    Out (0x2481, (CARD8)((startAddr >> 8) & 0xFF));
    Out (0x2482, (CARD8)((startAddr >> 16) & 0xFF));
    Out (0x2483, (CARD8)((In(0x2483) & 0xFE) |((startAddr >> 24) & 0x1)));
}

Bool XG47ModeInit(ScrnInfoPtr pScrn, DisplayModePtr dispMode)
{
	/* Jong 09/15/2006; support dual view */
    XGIAskModeRec   askMode[2];
    /* XGIAskModeRec   askMode = {0, 0}; */

    XGIPtr          pXGI = XGIPTR(pScrn);
    vgaHWPtr        pVgaHW = VGAHWPTR(pScrn);

	int				i;

	/* Jong 09/15/2006; index of display mode */
	int				index=0;
	/* Jong 11/14/2006; use bIn3x5(0x5A) to check whether device is attached */
	CARD8			ConnectedDevices=0x00;

	/* Jong 11/08/2006; initialize modeNo */
	askMode[0].modeNo=0x00;
	askMode[1].modeNo=0x00;

    vgaHWUnlock(pVgaHW);

    /* Initialise the ModeReg values */
    if (!vgaHWInit(pScrn, dispMode))
        return FALSE;

	/* Jong 07/1/42006; write OK! */
	/*----------------------------*/
	/*
	for(i=0; i< 256; i++)
		*(((CARD8*)(pXGI->fbBase))+i)=0x32; 

	ErrorF("Jong-07142006-dump memory of FB-(32)\n");
	XGIDumpMemory(pXGI->fbBase,  256); */
	/*----------------------------*/

    pScrn->vtSema = TRUE;

	/* Jong 09/15/2006; Use askMode[0] for single view or first view of dual view mode */
	/* Use askMode[1] for second view of dual view mode */
#ifdef XGIDUALVIEW
	if(!g_DualViewMode || (pXGI->FirstView) )
		index=0;
	else
		index=1;
#endif

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-index=%d--\n", index);
#endif

	/* Jong 10/04/2006; hard code for test */
    /* dispMode->HDisplay   = 1024;
    dispMode->VDisplay      = 768; */

    askMode[index].width       = dispMode->HDisplay;
    askMode[index].height      = dispMode->VDisplay; 

    askMode[index].pixelSize   = pScrn->bitsPerPixel;

	/* Jong 11/09/2006; pXGI->displayDevice == 0 and then go to default */
    switch(pXGI->displayDevice) /* Jong 11/09/2006; Be used to force opening devices */
    {
		case ST_DISP_LCD:
			askMode[index].condition   = ST_DISP_LCD;
			break;
		case ST_DISP_CRT:
			askMode[index].condition   = ST_DISP_CRT;
			break;
		case ST_DISP_TV:
			askMode[index].condition   = ST_DISP_TV;
			break;
		case ST_DISP_DVI:
			askMode[index].condition   = ST_DISP_DVI;
			break;
		/* Jong 11/14/2006; add for MV(multiple view) */
		case ST_DISP_LCD_MV:
			askMode[index].condition   = ST_DISP_LCD_MV;
			break;
		case ST_DISP_CRT_MV:
			askMode[index].condition   = ST_DISP_CRT_MV;
			break;
		case ST_DISP_TV_MV:
			askMode[index].condition   = ST_DISP_TV_MV;
			break;
		case ST_DISP_DVI_MV:
			askMode[index].condition   = ST_DISP_DVI_MV;
			break;
		default : /* Jong 11/09/2006; follow current detection status to open device */
			/* Jong 11/14/2006; 0x3cf-0x5B is a scratch register */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-XG47ModeInit-bIn3cf(0x5B)=0x%x--\n", bIn3cf(0x5B));
	ErrorF("Jong-Debug-XG47ModeInit-bIn3x5(0x5A)=0x%x--\n", bIn3x5(0x5A));
#endif
			/* Jong 11/14/2006; use bIn3x5(0x5A) to check whether device is attached */
			/* 0x22 : single CRT; 0x2e : DVI + CRT , 0x2f : single DVI ... */
			/* How about LCD ??? */
			ConnectedDevices=bIn3x5(0x5A);

			if (ConnectedDevices & 0x01) /* single DVI */
			{
				askMode[index].condition = ST_DISP_DVI;
			}
			else
			{
				if (ConnectedDevices & 0x0E == 0x0E) /* DVI + CRT */
				{
					askMode[index].condition = ST_DISP_CRT | ST_DISP_DVI;
				}
				else if (ConnectedDevices & 0x02) /* single CRT */
				{
					askMode[index].condition = ST_DISP_CRT;
				}
				else if(ConnectedDevices & 0x10) /* TV : need to be verified more */
				{
					askMode[index].condition = ST_DISP_TV;
				}
			}

			/*
			if (bIn3cf(0x5B) & ST_DISP_LCD)
			{
				askMode[index].condition = ST_DISP_LCD;
			}
			else if (bIn3cf(0x5B) & ST_DISP_CRT)
			{
				askMode[index].condition = ST_DISP_CRT;
			}
			else if (bIn3cf(0x5B) & ST_DISP_TV)
			{
				askMode[index].condition = ST_DISP_TV;
			}
			else if (bIn3cf(0x5B) & ST_DISP_DVI)
			{
				askMode[index].condition = ST_DISP_DVI;
			} */
			/* Jong 11/14/2006; add for MV(multiple view) */
			/* 
			else if (bIn3cf(0x5B) & ST_DISP_LCD_MV)
			{
				askMode[index].condition = ST_DISP_LCD_MV;
			}
			else if (bIn3cf(0x5B) & ST_DISP_CRT_MV)
			{
				askMode[index].condition = ST_DISP_CRT_MV;
			}
			else if (bIn3cf(0x5B) & ST_DISP_TV_MV)
			{
				askMode[index].condition = ST_DISP_TV_MV;
			}
			else if (bIn3cf(0x5B) & ST_DISP_DVI_MV)
			{
				askMode[index].condition = ST_DISP_DVI_MV;
			} */

			break;
    }

    if (dispMode->VRefresh <= 0.0)
    {
        dispMode->VRefresh = (dispMode->SynthClock * 1000.0) /
                             (dispMode->CrtcHTotal * dispMode->CrtcVTotal);
    }

    if (((int)dispMode->VRefresh) >= 85)
    {
        askMode[index].refRate = 85;
    }
    else if (((int)dispMode->VRefresh) >= 75)
    {
        askMode[index].refRate = 75;
    }
    else
    {
        askMode[index].refRate = 60;
    }

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-0-askMode[index].modeNo=0x%x--\n", askMode[index].modeNo);
#endif

	/* Jong 09/15/2006; support dual view */
	if(g_DualViewMode)
	{
		/* Jong 10/04/2006; support different modes for dual view */
		if(pXGI->FirstView)
			g_ModeOfFirstView=askMode[index];

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-Before calling XGIBiosModeInit()-0-askMode[index].modeNo=0x%x--\n", askMode[index].modeNo);
#endif
		if (!XGIBiosModeInit(pScrn, &askMode, 1, 0)) return FALSE;
	}
	else
	{
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-Before calling XGIBiosModeInit()-1-askMode[index].modeNo=0x%x--\n", askMode[index].modeNo);
#endif
		if (!XGIBiosModeInit(pScrn, &askMode, 0, 0)) return FALSE;
	}

#if 0
    askMode.width       = dispMode->HDisplay;
    askMode.height      = dispMode->VDisplay;
    askMode.pixelSize   = pScrn->bitsPerPixel;

    switch(pXGI->displayDevice)
    {
    case ST_DISP_LCD:
        askMode.condition   = ST_DISP_LCD;
        break;
    case ST_DISP_CRT:
        askMode.condition   = ST_DISP_CRT;
        break;
    case ST_DISP_TV:
        askMode.condition   = ST_DISP_TV;
        break;
    case ST_DISP_DVI:
        askMode.condition   = ST_DISP_DVI;
        break;
    default :
        if (bIn3cf(0x5B) & ST_DISP_LCD)
        {
            askMode.condition = ST_DISP_LCD;
        }
        else if (bIn3cf(0x5B) & ST_DISP_CRT)
        {
            askMode.condition = ST_DISP_CRT;
        }
        else if (bIn3cf(0x5B) & ST_DISP_TV)
        {
            askMode.condition = ST_DISP_TV;
        }
        else if (bIn3cf(0x5B) & ST_DISP_DVI)
        {
            askMode.condition = ST_DISP_DVI;
        }
        break;
    }

    if (dispMode->VRefresh <= 0.0)
    {
        dispMode->VRefresh = (dispMode->SynthClock * 1000.0) /
                             (dispMode->CrtcHTotal * dispMode->CrtcVTotal);
    }

    if (((int)dispMode->VRefresh) >= 85)
    {
        askMode.refRate = 85;
    }
    else if (((int)dispMode->VRefresh) >= 75)
    {
        askMode.refRate = 75;
    }
    else
    {
        askMode.refRate = 60;
    }

	/* Jong 09/15/2006; Force to dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-Before calling XGIBiosModeInit()-2-askMode[index].modeNo=0x%x--\n", askMode[index].modeNo);
#endif

    if (!XGIBiosModeInit(pScrn, &askMode, 1, 0)) return FALSE;
    /* if (!XGIBiosModeInit(pScrn, &askMode, 0, 0)) return FALSE;*/
#endif

	/* Jong 10/05/2006; fix bug of supporting different modes for each view of dual view mode */
	/* don't set offset register when 2nd view */
	if(!g_DualViewMode || (pXGI->FirstView) )
	{
#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

		XG47SetCRTCViewStride(pScrn);

#ifdef XGI_DUMP
	ErrorF("Jong-Debug-After-XG47SetCRTCViewStride()\n");
    XGIDumpRegisterValue(pScrn);
#endif
	}

    return TRUE;
}

/* Jong 09/14/2006; check one mode per time from xf86ValidateModes()-xf86InitialCheckModeForDriver() */
/* dispMode is mode information to be checked from X */
/* Why to check modes at the same time with askMode[0] and askMode[1]? */
/* It seems not feasible to get more information (dispMode) of second view when checking first view */
/* We might need to check modes for each view individually because */
/* xf86ValidateModes() will be called twice in different XGIPreInit()-XGIPreInitModes() */
/* Of course, we must have the ability to identify which device in attached with each view (screen=pScrn) */
int XG47ValidMode(ScrnInfoPtr pScrn, DisplayModePtr dispMode)
{
	/* Jong 09/12/2006; support dual view */
	CARD8			DeviceStatus=0;

	/* Jong 11/14/2006; use bIn3x5(0x5A) to check whether device is attached */
	CARD8			ConnectedDevices=0x00;

    XGIAskModeRec   askMode[2];
    XGIPtr          pXGI = XGIPTR(pScrn);

/* Jong 09/12/2006; support dual view */
/* will cause support mode not found even if single view */
/* #ifdef XGIDUALVIEW
    CARD32          dualView = 1;
#else
    CARD32          dualView = 0;
#endif */
    CARD32          dualView = 0;

    askMode[0].width       = dispMode->HDisplay;
    askMode[0].height      = dispMode->VDisplay;
    askMode[0].pixelSize   = pScrn->bitsPerPixel;

    switch(pXGI->displayDevice)
    {
		case ST_DISP_LCD:
			askMode[0].condition   = ST_DISP_LCD;
			break;
		case ST_DISP_CRT:
			askMode[0].condition   = ST_DISP_CRT;
			break;
		case ST_DISP_TV:
			askMode[0].condition   = ST_DISP_TV;
			break;
		case ST_DISP_DVI:
			askMode[0].condition   = ST_DISP_DVI;
			break;
		/* Jong 11/14/2006; add for MV(multiple view) */
		case ST_DISP_LCD_MV: 
			askMode[0].condition   = ST_DISP_LCD_MV;
			break;
		case ST_DISP_CRT_MV:
			askMode[0].condition   = ST_DISP_CRT_MV;
			break;
		case ST_DISP_TV_MV:
			askMode[0].condition   = ST_DISP_TV_MV;
			break;
		case ST_DISP_DVI_MV:
			askMode[0].condition   = ST_DISP_DVI_MV;

		/* Jong 09/11/2006; get current device status */
		default :
			/* Jong 09/12/2006; support dual view */
			DeviceStatus=bIn3cf(0x5B); /* CRT:0x02; DVI:0x08 */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-XG47ModeInit-bIn3cf(0x5B)=0x%x--\n", DeviceStatus);
	ErrorF("Jong-Debug-XG47ModeInit-bIn3x5(0x5A)=0x%x--\n", bIn3x5(0x5A));
#endif

			/* Jong 11/14/2006; use bIn3x5(0x5A) to check whether device is attached */
			/* 0x22 : single CRT; 0x2e : DVI + CRT , 0x2f : single DVI ... */
			/* How about LCD ??? */
			ConnectedDevices=bIn3x5(0x5A);

			if (ConnectedDevices & 0x01) /* single DVI */
			{
				askMode[0].condition = ST_DISP_DVI;
			}
			else
			{
				if (ConnectedDevices & 0x0E) /* DVI + CRT */
				{
					askMode[0].condition = ST_DISP_CRT | ST_DISP_DVI;
				}
				else if (ConnectedDevices & 0x02) /* single CRT */
				{
					askMode[0].condition = ST_DISP_CRT;
				}
				else if(ConnectedDevices & 0x10) /* TV : need to be verified more */
				{
					askMode[0].condition = ST_DISP_TV;
				}
			}

			/*
			if (DeviceStatus & ST_DISP_LCD)
			{
				askMode[0].condition   = ST_DISP_LCD;
			}
			else if (DeviceStatus & ST_DISP_CRT)
			{
				askMode[0].condition   = ST_DISP_CRT;
			}
			else if (DeviceStatus & ST_DISP_TV)
			{
				askMode[0].condition   = ST_DISP_TV;
			}
			else if (DeviceStatus & ST_DISP_DVI)
			{
				askMode[0].condition   = ST_DISP_DVI;
			} */
			/* Jong 11/14/2006; add for MV(multiple view) */
			/*
			else if (DeviceStatus & ST_DISP_LCD_MV)
			{
				askMode[0].condition   = ST_DISP_LCD_MV;
			}
			else if (DeviceStatus & ST_DISP_CRT_MV)
			{
				askMode[0].condition   = ST_DISP_CRT_MV;
			}
			else if (DeviceStatus & ST_DISP_TV_MV)
			{
				askMode[0].condition   = ST_DISP_TV_MV;
			}
			else if (DeviceStatus & ST_DISP_DVI_MV)
			{
				askMode[0].condition   = ST_DISP_DVI_MV;
			} */

/* #if 0
        if (bIn3cf(0x5B) & ST_DISP_LCD)
        {
            askMode[0].condition   = ST_DISP_LCD;
        }
        else if (bIn3cf(0x5B) & ST_DISP_CRT)
        {
            askMode[0].condition   = ST_DISP_CRT;
        }
        else if (bIn3cf(0x5B) & ST_DISP_TV)
        {
            askMode[0].condition   = ST_DISP_TV;
        }
        else if (bIn3cf(0x5B) & ST_DISP_DVI)
        {
            askMode[0].condition   = ST_DISP_DVI;
        }
#endif */

        break;
    }
    /*askMode[0].condition   = ST_DISP_CRT;*/

    if (dispMode->VRefresh <= 0.0)
    {
        dispMode->VRefresh = (dispMode->SynthClock * 1000.0) /
                             (dispMode->CrtcHTotal * dispMode->CrtcVTotal);
    }

    if (((int)dispMode->VRefresh) >= 85)
    {
        askMode[0].refRate = 85;
    }
    else if (((int)dispMode->VRefresh) >= 75)
    {
        askMode[0].refRate = 75;
    }
    else
    {
        askMode[0].refRate = 60;
    }

	/* Jong 09/12/2006; Both askMode[0] and askMode[1] will be used in XG47BiosValidMode() */
	/* pXGI->pBiosDll->biosValidMode() is XG47BiosValidMode() */
    if (!pXGI->pBiosDll->biosValidMode(pScrn, askMode, dualView))
    {
        return (MODE_BAD);
    }

    /* judge the mode on LCD */
    if (pXGI->displayDevice & ST_DISP_LCD
        || (bIn3cf(0x5B) & ST_DISP_LCD))
    {
        if ((pXGI->lcdWidth == 1600) && (dispMode->HDisplay == 1400))
        {
            return(MODE_BAD);
        }

        if (dispMode->HDisplay > pXGI->lcdWidth)
        {
            return(MODE_BAD);
        }
    }

    /* judge the mode on CRT, DVI */
    if ((pXGI->displayDevice & ST_DISP_CRT) || (bIn3cf(0x5B) & ST_DISP_CRT)
         || (pXGI->displayDevice & ST_DISP_DVI) || (bIn3cf(0x5B) & ST_DISP_DVI))
    {
        if (dispMode->HDisplay == 1400)
        {
            return(MODE_BAD);
        }
    }

    return (MODE_OK);
}
