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

#include "xf86.h"
#include "xf86_OSproc.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "miline.h"

#include "xaarop.h"
#include "xaalocal.h"

#include "xgi.h"
#include "xgi_regs.h"
#include "xg47_regs.h"
#include "xgi_driver.h"
#include "xg47_cmdlist.h"
#include "xg47_accel.h"
#include "xgi_debug.h"
#include "xgi_misc.h"

#define CMDBUF_SIZE      0x100000
#define CMDBATCH_SIZE    0x2000

/* Jong 09/16/2006; support dual view */
extern int		g_DualViewMode;

typedef struct XG47_ACCEL_INFO
{
    unsigned long engineCmd;
    unsigned long engineFlag;

    int clp_x1, clp_y1, clp_x2, clp_y2;
    Bool clp_enable;

    int rop;

    unsigned int planemask;
    int color_depth;
    int trans_color;
    CARD32 pitch;
    int fg_color;
    int bg_color;

    Bool discardCmd;
}XG47_accel_info;

static XG47_accel_info  accel_info;


/* inner functions */
/*
static void XG47EnableGE(XGIPtr pXGI);
static void XG47DisableGE(XGIPtr pXGI);
static void XG47WaitForIdle(XGIPtr pXGI);
*/

static Bool XG47InitCmdList();
static void XG47BeginCmdList(CARD32 size);
static void XG47EndCmdList();
static inline void XG47SetGECmd(CARD32 addr, CARD32 cmd);
static void ResetClip();
static void SetClip(CARD32 left, CARD32 top, CARD32 right, CARD32 bottom);

static inline void SetColorDepth(int color);
static inline void SetROP3(int rop);
static inline void SetFGColor(CARD32 color);
static inline void SetPatFGColor(CARD32 color);
static inline void SetPatBKColor(CARD32 color);
static inline void SetDstColorKey(CARD32 color);
static inline void SetDstBasePitch(CARD32 pitch, CARD32 base);
static inline void SetSrcBasePitch(CARD32 pitch, CARD32 base);
static inline void SetDstXY(CARD32 x, CARD32 y, CARD32 addr);
static inline void SetSrcXY(CARD32 x, CARD32 y, CARD32 addr);
static inline void SetDstSize(CARD32 w, CARD32 h);
static inline void SetDrawingCommand(CARD32 cmd, CARD32 flag);

/* XAA functions */
/* Sync */
static void XG47Sync(ScrnInfoPtr pScrn);

/* Clipper */
static void XG47SetClippingRectangle(ScrnInfoPtr pScrn,
                                     int left, int top, int right, int bottom);
static void XG47DisableClipping(ScrnInfoPtr pScrn);

/* Blits */
static void XG47SetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                           int xdir, int ydir,
                                           int rop,
                                           unsigned int planemask,
                                           int trans_color);

static void XG47SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                         	 int xsrc, int ysrc,
                                         	 int xdst, int ydst,
                                             int w, int h);
/* Solid fills */
static void XG47SetupForSolidFill(ScrnInfoPtr pScrn,
                                  int color,
                                  int rop,
                                  unsigned int planemask);

static void XG47SubsequentSolidFillRect(ScrnInfoPtr pScrn,
                                        int x, int y, int w, int h);

/* Screen to screen color expansion - HW not Support */
/*
static void XG47SetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                      int fg, int bg,
                                                      int rop,
                                                      unsigned int planemask);

static void XG47SubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                        int x, int y,
                                                        int w, int h,
                                                        int srcx, int srcy,
                                                        int skipleft);
*/

/* CPU to Screen Color expansion - similiar to XAAImageWrite() */
/*
static void XG47SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                   int fg, int bg,
                                                   int rop,
                                                   unsigned int planemask);

static void XG47SubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                     int x, int y, int w, int h,
                                                     int skipleft);
*/

/* 8x8 mono pattern fills */

static void XG47SetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
                                           int patx, int paty,
                                           int fg, int bg,
                                           int rop,
                                           unsigned int planemask);

static void XG47SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
                                                 int patx, int paty,
                                                 int x, int y, int w, int h);

/* 8x8 color pattern fills */
static void XG47SubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn,
                                                  int patx, int paty,
                                                  int x, int y, int w, int h);

static void XG47SetupForColor8x8PatternFill(ScrnInfoPtr pScrn,
                                            int patx, int paty,
                                            int rop,
                                            unsigned int planemask,
                                            int transparency_color);


/* Solid lines */
static void XG47SetupForSolidLine(ScrnInfoPtr pScrn,
                                  int color,
                                  int rop,
                                  unsigned int planemask);

/*
static void XG47SubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
                                            int xa, int ya, int xb, int yb,
                                            int flags);
*/
static void XG47SubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
                                             int x, int y, int absmaj, int absmin,
                                             int err, int len, int octant);

static void XG47SubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                                           int x, int y, int len, int dir);
/*
static void XG47SetupForDashedLine(ScrnInfoPtr pScrn,
                                   int fg, int bg,
                                   int rop,
                                   unsigned int planemask,
                                   int length,
                                   unsigned char *pattern);

static void XG47SubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
                                             int xa, int ya, int xb, int yb,
                                             int flags, int phase);

static void XG47SubsequentDashedBresenhamLine(ScrnInfoPtr pScrn,
                                              int x, int y,
                                              int absmaj, int absmin,
                                              int err, int len,
                                              int flags, int phase);
*/

/* Jong 09/16/2006; support dual view */
#ifdef XGIDUALVIEW
static void XGIRestoreAccelState(ScrnInfoPtr pScrn);
#endif

/* Called from outside */
void XG47EngineInit(ScrnInfoPtr pScrn)
{
    unsigned char temp;

    XGIPtr  pXGI = XGIPTR(pScrn);

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47EngineInit()\n");

    /* unprotect all register except which protected by 3c5.0e.7 */
    bOut3c5(0x11,0x92);

    /* -------> copy from OT2D
     * PCI Retry Control Register.
     * disable PCI read retry & enable write retry in mem. (10xx xxxx)b
     */
    temp = bIn3x5(0x55);
    bOut3x5(0x55, (temp & 0xbf) | 0x80);

    XG47EnableGE(pXGI);

    /* Enable linear addressing of the card. */
    temp = bIn3x5(0x21);
    bOut3x5(0x21, temp | 0x20);

    /* Enable 32-bit internal data path */
    temp = bIn3x5(0x2A);
    bOut3x5(0x2A, temp | 0x40);

    /* Enable PCI burst write ,disable burst read and enable MMIO. */
    /*
     * 0x3D4.39 Enable PCI burst write, disable burst read and enable MMIO.
     * 7 ---- Pixel Data Format 1:  big endian 0:  little endian
     * 6 5 4 3---- Memory Data with Big Endian Format, BE[3:0]#  with Big Endian Format
     * 2 ---- PCI Burst Write Enable
     * 1 ---- PCI Burst Read Enable
     * 0 ---- MMIO Control
     */
    temp = bIn3x5(0x39);
    bOut3x5(0x39, (temp | 0x05) & 0xfd);

    /* enable GEIO decode */
    /*
        bOut3x5(0x29,bIn3x5(0x29)|0x08);
    */

    /*Init MEMORY again*/

    /* Patch For BIOS*/

    /* Enable graphic engine I/O PCI retry function*/
    /*
        temp = bIn3x5(0x62);
        bOut3x5(0x62, temp | 0x50);
    */

    /* protect all register except which protected by 3c5.0e.7 */
    /*
        bOut3c5(0x11,0x87);
    */
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47EngineInit()\n");
}

Bool XG47AccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr   pXaaInfo;
    ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];

    XGIPtr pXGI = XGIPTR(pScrn);

    PDEBUG(ErrorF("*-*Jong-XG47AccelInit()\n"));	
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47AccelInit\n");

    XGIDebug(DBG_CMDLIST, "[DBG] XG47AccelInit() Enable cmdlist\n");

    /*pXG47->InitializeAccelerator = XG47EngineInit;*/
    XG47EngineInit(pScrn);

    if (pXGI->noAccel)
        return FALSE;

    pXGI->pXaaInfo = pXaaInfo = XAACreateInfoRec();
    if (!pXaaInfo) return FALSE;

    pXaaInfo->Flags             = PIXMAP_CACHE          |
                                  OFFSCREEN_PIXMAPS     |
                                  LINEAR_FRAMEBUFFER;

    pXaaInfo->Sync = XG47Sync;


    pXaaInfo->SetClippingRectangle = XG47SetClippingRectangle;
    pXaaInfo->DisableClipping      = XG47DisableClipping;


    if (ENABLE_HW_SRC2SRC)
    {
		PDEBUG(ErrorF("*-*Jong-[INFO]Enable HW Screen to Screen Copy\n"));
        XGIDebug(DBG_INFO, "[INFO]Enable HW Screen to Screen Copy\n");

		/* Jong 07/03/2006; it seems improve performance of window moving to right */
        pXaaInfo->ScreenToScreenCopyFlags      = NO_PLANEMASK                  |
                                                 NO_TRANSPARENCY; 

        /* pXaaInfo->ScreenToScreenCopyFlags      = ONLY_TWO_BITBLT_DIRECTIONS    |
                                                 NO_PLANEMASK                  |
                                                 NO_TRANSPARENCY; */

        pXaaInfo->SetupForScreenToScreenCopy   = XG47SetupForScreenToScreenCopy;
        pXaaInfo->SubsequentScreenToScreenCopy = XG47SubsequentScreenToScreenCopy;
    }

    if (ENABLE_HW_SOLIDFILL)
    {
		PDEBUG(ErrorF("*-*Jong-[INFO]Enable HW SolidFill\n"));
        XGIDebug(DBG_INFO, "[INFO]Enable HW SolidFill\n");
        pXaaInfo->SolidFillFlags            = NO_PLANEMASK | NO_TRANSPARENCY;
        pXaaInfo->SetupForSolidFill         = XG47SetupForSolidFill;
        pXaaInfo->SubsequentSolidFillRect   = XG47SubsequentSolidFillRect;
    }

    if (ENABLE_HW_SOLIDLINE)
    {
		PDEBUG(ErrorF("*-*Jong-[INFO]Enable HW SolidLine\n"));
        XGIDebug(DBG_INFO, "[INFO]Enable HW SolidLine\n");
        pXaaInfo->SolidLineFlags             = NO_PLANEMASK | NO_TRANSPARENCY;
        pXaaInfo->SetupForSolidLine          = XG47SetupForSolidLine;
        pXaaInfo->SubsequentSolidHorVertLine = XG47SubsequentSolidHorVertLine;
        pXaaInfo->SubsequentSolidBresenhamLine = XG47SubsequentSolidBresenhamLine;
    }

    if (ENABLE_HW_8X8PATTERN)
    {
		PDEBUG(ErrorF("*-*Jong-[INFO]Enable HW Color 8x8 color pattern fill\n"));
        XGIDebug(DBG_INFO, "[INFO]Enable HW Color 8x8 color pattern fill\n");
        pXaaInfo->Color8x8PatternFillFlags = HARDWARE_PATTERN_SCREEN_ORIGIN |
                                             BIT_ORDER_IN_BYTE_MSBFIRST;
        pXaaInfo->SetupForColor8x8PatternFill = XG47SetupForColor8x8PatternFill;
        pXaaInfo->SubsequentColor8x8PatternFillRect =
                                          XG47SubsequentColor8x8PatternFillRect;
    }

    if (ENABLE_HW_8X8MONOPAT)
    {
		PDEBUG(ErrorF("*-*Jong-[INFO]Enable HW Color 8x8 Mono pattern fill\n"));
        XGIDebug(DBG_INFO, "[INFO]Enable HW Color 8x8 Mono pattern fill\n");
        pXaaInfo->Mono8x8PatternFillFlags =  NO_PLANEMASK | NO_TRANSPARENCY |
                                            BIT_ORDER_IN_BYTE_MSBFIRST |
                                            HARDWARE_PATTERN_SCREEN_ORIGIN |
                                            HARDWARE_PATTERN_PROGRAMMED_BITS;

        pXaaInfo->SetupForMono8x8PatternFill = XG47SetupForMono8x8PatternFill;
        pXaaInfo->SubsequentMono8x8PatternFillRect = XG47SubsequentMono8x8PatternFillRect;
    }

    if (ENABLE_HW_IMAGEWRITE)
    {
    }

    pXaaInfo->ImageWriteBase  = pXGI->IOBase + 0x10000;
    pXaaInfo->ImageWriteRange = 0x8000;

/* Jong 09/16/2006; RestoreAccelState() is required if PCI entity has IS_SHARED_ACCEL */
/* Otherwise; it will cause failed of XAAInit() */
#ifdef XGIDUALVIEW
	      if(g_DualViewMode) 
		  {
			pXaaInfo->RestoreAccelState = XGIRestoreAccelState;
	      }
#endif

    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-Before XG47InitCmdList\n");
    if (XG47InitCmdList(pScrn) == FALSE)
    {
    	return FALSE;
    }
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After XG47InitCmdList\n");

    /* We always use patten bank0 since HW can not handle patten bank swapping corretly! */
    accel_info.engineFlag   = FER_EN_PATTERN_FLIP;
    /* ENINT; 32bpp */
    accel_info.engineCmd    = 0x8200;

    accel_info.clp_x1 = 0;
    accel_info.clp_y1 = 0;
    /*accel_info.clp_x2 = pScrn->displayWidth;*/
    accel_info.clp_x2 = pScrn->virtualX;    /* Virtual width */
    accel_info.clp_y2 = pScrn->virtualY;

    accel_info.discardCmd = FALSE;

    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-Before XAAInit()\n");
    return(XAAInit(pScreen, pXaaInfo));

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47AccelInit\n");
}

void XG47EnableGE(XGIPtr pXGI)
{
    CARD32 iWait;
    /* this->vAcquireRegIOProctect();*/
    /* Save and close dynamic gating */
    CARD8 bOld3cf2a = bIn3cf(0x2a);

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47EnableGE\n");

    bOut3cf(0x2a, bOld3cf2a & 0xfe);

    /* Reset both 3D and 2D engine */
    bOut3x5(0x36, 0x84);
    iWait = 10;
    while (iWait--)
    {
        INB(0x36);
    }

    bOut3x5(0x36, 0x94);
    iWait = 10;
    while (iWait--)
    {
        INB(0x36);
    }
    bOut3x5(0x36, 0x84);
    iWait = 10;
    while (iWait--)
    {
        INB(0x36);
    }
    /* Enable 2D engine only */
    bOut3x5(0x36, 0x80);

    /* Enable 2D+3D engine */
    bOut3x5(0x36, 0x84);

    /* Restore dynamic gating */
    bOut3cf(0x2a, bOld3cf2a);

    /* this->vReleaseRegIOProctect();
    m_b3DGEOn = FALSE;*/

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47EnableGE\n");
}

void XG47DisableGE(XGIPtr pXGI)
{
    CARD32 iWait;

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47DisableGE\n");
    /* this->vAcquireRegIOProctect();*/
    /* Reset both 3D and 2D engine */
    bOut3x5(0x36, 0x84);
    iWait = 10;
    while (iWait--)
    {
        INB(0x36);
    }
    bOut3x5(0x36, 0x94);
    iWait = 10;
    while (iWait--)
    {
        INB(0x36);
    }
    bOut3x5(0x36, 0x84);
    iWait = 10;
    while (iWait--)
    {
        INB(0x36);
    }
    /* Disable 2D engine only */
    bOut3x5(0x36, 0);
    /* this->vReleaseRegIOProctect();
     m_b3DGEOn = FALSE;*/
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47DisableGE\n");
}

/*
Function:
    1. Wait for GE
    2. Reset GE
*/
static void XG47Sync(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47Sync\n");

    if (accel_info.clp_enable)
    {
        XG47DisableClipping(pScrn);
    }

    XG47WaitForIdle(pXGI);
    /*ResetGE()*/

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47Sync\n");
}

void XG47WaitForIdle(XGIPtr pXGI)
{
    int	idleCount = 0 ;
    unsigned int busyCount = 0;
    unsigned long reg;

    int enablewait = 1;

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47WaitForIdle\n");
    if (enablewait == 1)
    {
        while (idleCount < 5)
        {
            reg = INB(0x2800);
            if (0 == (reg & IDLE_MASK))
            {
                idleCount++ ;
                busyCount = 0;
            }
            else
            {
                idleCount = 0 ;
                busyCount++;
                if (busyCount >= 5000000)
                {
                    /* vGE_Reset(); */
                    busyCount = 0;
                }
            }
        }
    }

    XGIDebug(DBG_WAIT_FOR_IDLE, "[DEBUG] [2800]=%08x\n", INDW(2800));
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47WaitForIdle\n");
}

void XG47StopCmd(Bool flag)
{
    accel_info.discardCmd = flag;
}

static void XG47SetClippingRectangle(ScrnInfoPtr pScrn,
                                     int left, int top, int right, int bottom)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetClippingRectangle\n");
    accel_info.clp_x1 = left;
    accel_info.clp_y1 = top;
    accel_info.clp_x2 = right;
    accel_info.clp_y2 = bottom;
    accel_info.clp_enable = TRUE;
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SetClippingRectangle\n");
}

static void XG47DisableClipping(ScrnInfoPtr pScrn)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47DisableClipping\n");

    ResetClip();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47DisableClipping\n");
}

static void XG47SetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                           int xdir, int ydir,
                                           int rop,
                                           unsigned int planemask,
                                           int trans_color)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetupForScreenToScreenCopy\n");

    accel_info.color_depth = pScrn->bitsPerPixel >> 3;
    accel_info.pitch = pScrn->displayWidth * accel_info.color_depth;
    accel_info.rop = rop;
    accel_info.planemask = planemask;
    accel_info.trans_color = trans_color;

/* Jong 07/05/2006 */
#ifdef _SO_
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) | ((xgiG2_ALUConv[rop] & 0xff) << 24);
#else
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) | ((XAACopyROP[rop] & 0xff) << 24);
#endif

    SetColorDepth(accel_info.color_depth);

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetupForScreenToScreenCopy-1\n");
    /* LEAVEFUNC(XG47SetupForScreenToScreenCopy); */
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SetupForScreenToScreenCopy\n");

/*#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong09272006-XGI_DUMP-XG47SetupForScreenToScreenCopy()----\n");
    XGIDumpRegisterValue(pScrn);
#endif*/
}

static void XG47SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                             int x1, int y1,
                                             int x2, int y2,
                                             int w, int h)
{
    XGIPtr          pXGI = XGIPTR(pScrn);

	/* Jong 09/22/2006; test for dual view */
	/* if(pScrn->fbOffset == 0) return; */

    XGIDebug(DBG_FUNCTION, "[DBG-Jong-05292006] Enter XG47SubsequentScreenToScreenCopy\n");
	/*return; */

    XG47BeginCmdList(CMDBATCH_SIZE);

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-0\n");
	
	/* Jong 09/22/2006; support dual view */
    SetDstBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetDstBasePitch(accel_info.pitch, 0); */
	
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("XG47SubsequentScreenToScreenCopy()-FirstView=%d-pScrn->fbOffset=%d\n", pXGI->FirstView, pScrn->fbOffset); 
#endif

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-1\n");
	/* Jong 09/22/2006; support dual view */
    SetSrcBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetSrcBasePitch(accel_info.pitch, 0);*/ 

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-2\n");

    if (accel_info.clp_enable == TRUE)
    {
        SetClip(accel_info.clp_x1, accel_info.clp_y1, accel_info.clp_x2, accel_info.clp_y2);
    }

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-3\n");

    SetSrcXY(x1, y1, 0);
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-3-1\n");

    SetDstXY(x2, y2, 0);
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-3-2\n");
    SetDstSize(w, h);
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-3-3\n");
    SetDrawingCommand(FER_CMD_BITBLT, FER_SOURCE_IN_VRAM);
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentScreenToScreenCopy-4\n");

    XG47EndCmdList();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SubsequentScreenToScreenCopy\n");

/* Jong 06/29/2006 */
/*#ifdef XGI_DUMP
	ErrorF("Jong06292006-XGI_DUMP-XG47SubsequentScreenToScreenCopy()----\n");
    XGIDumpRegisterValue(pScrn);
#endif*/
}

void XG47SetupForSolidFill(ScrnInfoPtr pScrn,
                           int color,
                           int rop,
                           unsigned int planemask)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetupForSolidFill\n");
    /* return; */

    accel_info.color_depth = pScrn->bitsPerPixel >> 3;
    accel_info.pitch = pScrn->displayWidth * accel_info.color_depth;
    accel_info.rop = rop;
    accel_info.planemask = planemask;
    accel_info.fg_color = color;

/* Jong 07/05/2006 */
#ifdef _SO_
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((xgiG2_PatALUConv[rop] & 0xff) << 24);
#else
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((XAAPatternROP[rop] & 0xff) << 24);
#endif

    SetColorDepth(accel_info.color_depth);

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SetupForSolidFill\n");

/*#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong09272006-XGI_DUMP-XG47SetupForSolidFill()----\n");    XGIDumpRegisterValue(pScrn);
#endif*/
}

void XG47SubsequentSolidFillRect(ScrnInfoPtr pScrn,
                                 int x, int y, int w, int h)
{
    XGIPtr          pXGI = XGIPTR(pScrn);

	/* Jong 09/22/2006; test for dual view */
	/* if(pScrn->fbOffset == 0) return; */

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentSolidFillRect(%d,%d,%d,%d)\n", x, y, w, h);
    /* return; */

    XG47BeginCmdList(CMDBATCH_SIZE);

    SetPatFGColor(accel_info.fg_color);
    /* SetPatFGColor(accel_info.fg_color); */

	/* Jong 09/22/2006; support dual view */
    SetDstBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetDstBasePitch(accel_info.pitch, 0); */

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("XG47SubsequentSolidFillRect()-FirstView=%d-pScrn->fbOffset=%d\n", pXGI->FirstView, pScrn->fbOffset); 
#endif

    if (accel_info.clp_enable == TRUE)
    {
        SetClip(accel_info.clp_x1, accel_info.clp_y1, accel_info.clp_x2, accel_info.clp_y2);
    }

    SetDstXY(x, y, 0);
    SetDstSize(w, h);
    SetDrawingCommand(FER_CMD_BITBLT, EBP_SOLID_BRUSH);

    XG47EndCmdList();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SubsequentSolidFillRect");
}

/*
static void XG47SetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                      int fg, int bg,
                                                      int rop,
                                                      unsigned int planemask)
{

}

static void XG47SubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                        int x, int y,
                                                        int w, int h,
                                                        int srcx, int srcy,
                                                        int skipleft)
{

}

*/

static void XG47SetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
                                           int patx, int paty,
                                           int fg, int bg,
                                           int rop,
                                           unsigned int planemask)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetupForMono8x8PatternFill\n");
    /* return; */

    accel_info.color_depth = pScrn->bitsPerPixel >> 3;
    accel_info.pitch = pScrn->displayWidth * accel_info.color_depth;
    accel_info.rop = rop;
    accel_info.planemask = planemask;
    accel_info.fg_color = fg;
    accel_info.bg_color = bg;

/* Jong 07/05/2006 */
#ifdef _SO_
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((xgiG2_PatALUConv[rop] & 0xff) << 24);
#else
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((XAAPatternROP[rop] & 0xff) << 24);
#endif

    SetColorDepth(accel_info.color_depth);

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SetupForMono8x8PatternFill\n");

/*#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong09272006-XGI_DUMP-XG47SetupForMono8x8PatternFill()----\n");
    XGIDumpRegisterValue(pScrn);
#endif*/
}

static void XG47SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
                                                 int patx, int paty,
                                                 int x, int y, int w, int h)
{
	/* Jong 09/22/2006; test for dual view */
	/* if(pScrn->fbOffset == 0) return; */

    XGIPtr pXGI = XGIPTR(pScrn);

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentMono8x8PatternFillRect(%d,%d,%d,%d)\n", x, y, w, h);
    /* return; */

    XG47BeginCmdList(CMDBATCH_SIZE);

    /* PATCH ?? */
/*
    XG47SetGECmd(ENG_DRAWINGFLAG, 0x00004000);
    XG47SetGECmd(ENG_LENGTH, 0x00010001);
    XG47SetGECmd(ENG_COMMAND, 0xf0400101);
*/
    XG47SetGECmd(ENG_DRAWINGFLAG, FER_EN_PATTERN_FLIP);

    XG47SetGECmd(ENG_PATTERN, patx);
    XG47SetGECmd(ENG_PATTERN1, paty);

/*
    XG47SetGECmd(ENG_DRAWINGFLAG, 0x00004000);
    XG47SetGECmd(ENG_LENGTH, 0x00010001);
    XG47SetGECmd(ENG_COMMAND, 0xf0400101);
*/
    XG47SetGECmd(ENG_DRAWINGFLAG, FER_EN_PATTERN_FLIP | FER_PATTERN_BANK1);
    XG47SetGECmd(ENG_PATTERN, patx);
    XG47SetGECmd(ENG_PATTERN1, paty);

    SetPatFGColor(accel_info.fg_color);
    SetPatBKColor(accel_info.bg_color);

	/* Jong 09/22/2006; support dual view */
    SetDstBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetDstBasePitch(accel_info.pitch, 0); */

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("XG47SubsequentMono8x8PatternFillRect()-FirstView=%d-pScrn->fbOffset=%d\n", pXGI->FirstView, pScrn->fbOffset); 
#endif

	/* Jong 09/22/2006; support dual view */
    SetSrcBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetSrcBasePitch(accel_info.pitch, 0); */

    if (accel_info.clp_enable == TRUE)
    {
        SetClip(accel_info.clp_x1, accel_info.clp_y1, accel_info.clp_x2, accel_info.clp_y2);
    }

    SetDstXY(x, y, 0);
    SetDstSize(w, h);

    SetDrawingCommand(FER_CMD_BITBLT, FER_MONO_PATTERN_DPRO | 0x2);

    XG47EndCmdList();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SubsequentMono8x8PatternFillRect\n");
}



static void XG47SetupForColor8x8PatternFill(ScrnInfoPtr pScrn,
                                            int patx, int paty,
                                            int rop,
                                            unsigned int planemask,
                                            int transparency_color)
{
    XGIPtr pXGI = XGIPTR(pScrn);
    unsigned long pattern_offset;
    unsigned long* ptr;
    int i;

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetupForColor8x8PatternFill\n");
    /* return; */

    accel_info.color_depth = pScrn->bitsPerPixel >> 3;
    accel_info.pitch = pScrn->displayWidth * accel_info.color_depth;
    accel_info.rop = rop;
    accel_info.planemask = planemask;
    accel_info.trans_color = transparency_color;

/* Jong 07/05/2006 */
#ifdef _SO_
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((xgiG2_PatALUConv[rop] & 0xff) << 24);
#else
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((XAAPatternROP[rop] & 0xff) << 24);
#endif

    SetColorDepth(accel_info.color_depth);

    XG47BeginCmdList(CMDBATCH_SIZE);

    pattern_offset = ((paty * pScrn->displayWidth * pScrn->bitsPerPixel / 8) +
                      (patx * pScrn->bitsPerPixel / 8));

    ptr = (unsigned long *)(pXGI->fbBase + pattern_offset);
    for(i = 0; i < pScrn->bitsPerPixel*2; i ++, ptr++ )
    {
        XG47SetGECmd(ENG_PATTERN +((i & ~0x20)<<2), *ptr);
    }

    XG47EndCmdList();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SetupForColor8x8PatternFill\n");

/*#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong09272006-XGI_DUMP-XG47SetupForColor8x8PatternFill()----\n");
    XGIDumpRegisterValue(pScrn);
#endif*/
}

static void XG47SubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn,
                                                  int patx, int paty,
                                                  int x, int y, int w, int h)
{
	/* Jong 09/22/2006; test for dual view */
	/* if(pScrn->fbOffset == 0) return; */

    XGIPtr pXGI = XGIPTR(pScrn);
    unsigned long pattern_offset;
    unsigned long trans_draw = 0;
    unsigned long* ptr;
    int i;

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentColor8x8PatternFillRect(%d,%d,%d,%d)\n", x, y, w, h);
    /* return; */

    XG47BeginCmdList(CMDBATCH_SIZE);

/*
    pattern_offset = ((paty * pScrn->displayWidth * pScrn->bitsPerPixel / 8) +
                      (patx * pScrn->bitsPerPixel / 8));

    ptr = (unsigned long *)(pXGI->fbBase + pattern_offset);
    for(i = 0; i < pScrn->bitsPerPixel*2; i ++, ptr++ )
    {
        XG47SetGECmd(ENG_PATTERN +((i & ~0x20)<<2), *ptr);
    }
*/

	/* Jong 09/22/2006; support dual view */
    SetDstBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetDstBasePitch(accel_info.pitch, 0); */

	/* Jong 09/22/2006; support dual view */
    SetSrcBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetSrcBasePitch(accel_info.pitch, 0); */

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("XG47SubsequentColor8x8PatternFillRect()-pScrn->fbOffset=%d\n", pScrn->fbOffset); 
#endif

    if (accel_info.trans_color != -1)
    {
        SetDstColorKey(accel_info.trans_color & 0xffffff);
        trans_draw = EBP_TRANSPARENT_MODE;
    }

    if (accel_info.clp_enable == TRUE)
    {
        SetClip(accel_info.clp_x1, accel_info.clp_y1, accel_info.clp_x2, accel_info.clp_y2);
    }

    SetDstXY(x, y, 0);
    SetDstSize(w, h);

    SetDrawingCommand(FER_CMD_BITBLT, trans_draw);

    XG47EndCmdList();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SubsequentColor8x8PatternFillRect\n");
}

static void XG47SetupForSolidLine(ScrnInfoPtr pScrn,
                                  int color,
                                  int rop,
                                  unsigned int planemask)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetupForSolidLine\n");
    /* return; */

    accel_info.color_depth = pScrn->bitsPerPixel >> 3;
    accel_info.pitch = pScrn->displayWidth * accel_info.color_depth;
    accel_info.rop = rop;
    accel_info.planemask = planemask;
    accel_info.fg_color = color;
    accel_info.bg_color = 0xffffffff;
 
/* Jong 07/05/2006 */
#ifdef _SO_
	accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((xgiG2_ALUConv[rop] & 0xff) << 24);
#else
	accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) |
                           ((XAAPatternROP[rop] & 0xff) << 24);
#endif

    SetColorDepth(accel_info.color_depth);

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SetupForSolidLine\n");

/*#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong09272006-XGI_DUMP-XG47SetupForSolidLine()----\n");
    XGIDumpRegisterValue(pScrn);
#endif*/
}

static void XG47SubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                                           int x, int y, int len, int dir)
{
	/* Jong 09/22/2006; test for dual view */
	/* if(pScrn->fbOffset == 0) return; */

    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SubsequentSolidHorVertLine(%d,%d,%d,%d)\n", x, y, len, dir);
    /* return; */

    XG47BeginCmdList(CMDBATCH_SIZE);

    SetPatFGColor(accel_info.fg_color);
    SetPatBKColor(accel_info.bg_color);

	/* Jong 09/22/2006; support dual view */
    SetDstBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetDstBasePitch(accel_info.pitch, 0); */

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("XG47SubsequentSolidHorVertLine()-pScrn->fbOffset=%d\n", pScrn->fbOffset); 
#endif

    if (accel_info.clp_enable == TRUE)
    {
        SetClip(accel_info.clp_x1, accel_info.clp_y1, accel_info.clp_x2, accel_info.clp_y2);
    }

    SetDstXY(x, y, 0);
    if (dir == DEGREES_0)
        SetDstSize(len, 1);
    else
        SetDstSize(1, len);

    SetDrawingCommand(FER_CMD_BITBLT,  EBP_SOLID_BRUSH);

    XG47EndCmdList();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SubsequentSolidHorVertLine\n");

}

static void XG47SubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
                                             int x, int y, int absmaj, int absmin,
                                             int err, int len, int octant)
{
	/* Jong 09/22/2006; test for dual view */
	/* if(pScrn->fbOffset == 0) return; */

    register int direction = 0;
    int axial, diagonal, error;

    XGIDebug(DBG_FUNCTION,
        "[DBG] Enter XG47SubsequentSolidBresenhamLine(%d,%d,%d,%d,%d,%d,%d)\n",
        x, y, absmaj, absmin, err, len, octant);
    /* return; */

    XG47BeginCmdList(CMDBATCH_SIZE);

    /* convert bresenham line interface style */
    axial = (absmin>>1)&0xFFF;
    diagonal = ((absmin - absmaj)>>1)&0xFFF;
    error = absmin -(absmaj>>1);
    if(error &0x01)
    {
        if(octant & YDECREASING)
            error++;
        else
            error--;
    }

    /* set direction */
    if(octant & YMAJOR) direction |= EBP_Y_MAJOR;
    if(octant & XDECREASING) direction |= EBP_OCT_X_DEC;
    if(octant & YDECREASING) direction |= EBP_OCT_Y_DEC;

    SetPatFGColor(accel_info.fg_color);
    SetPatBKColor(accel_info.bg_color);

	/* Jong 09/22/2006; support dual view */
    SetDstBasePitch(accel_info.pitch, pScrn->fbOffset);
    /* SetDstBasePitch(accel_info.pitch, 0); */

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("XG47SubsequentSolidBresenhamLine()-pScrn->fbOffset=%d\n", pScrn->fbOffset); 
#endif

    if (accel_info.clp_enable == TRUE)
    {
        SetClip(accel_info.clp_x1, accel_info.clp_y1, accel_info.clp_x2, accel_info.clp_y2);
    }

    SetSrcXY(diagonal, axial, 0);
    SetDstXY(x, y, 0);
    SetDstSize(error, len);

    SetDrawingCommand(FER_CMD_LINEDRAW, EBP_MONO_SOURCE | direction);

    XG47EndCmdList();

    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SubsequentSolidBresenhamLine\n");
}

/*
static void XG47SubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
                                            int xa, int ya, int xb, int yb,
                                            int flags)
{
}
*/

static void SetColorDepth(int color)
{
    /* 2125[0,1] = screen color depth */
    unsigned long mode = 0;

    if (color == 3)
    {
        color = 6;
    }
    /*
        color   1   2   3   4
        mode    0   1   3   2
    */
    mode = color >> 1;

    accel_info.engineCmd = (accel_info.engineCmd & ~0x300) | (mode << 8);
}



static void SetROP3(int rop)
{
    /* 2127[0,7] = rop */
/* Jong 07/05/2006; for .so */
#ifdef _SO_
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) | ((xgiG2_ALUConv[rop] & 0xff) << 24);
#else
    accel_info.engineCmd = (accel_info.engineCmd & 0xffffff) | ((XAACopyROP[rop] & 0xff) << 24);
#endif
}

static void SetFGColor(CARD32 color)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetFGColor()\n");
    XG47SetGECmd(ENG_FGCOLOR, color);
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetFGColor()\n");
}

static void SetPatFGColor(CARD32 color)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetPatFGColor()\n");
    XG47SetGECmd(ENG_PATTERN_FG, color);
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetPatFGColor()\n");
}

static void SetPatBKColor(CARD32 color)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetPatBKColor()\n");
    XG47SetGECmd(ENG_PATTERN_BG, color);
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetPatBKColor()\n");
}

static void SetDstColorKey(CARD32 color)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetDstColorKey()\n");
    XG47SetGECmd(ENG_DEST_COLORKEY, color);
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetDstColorKey()\n");
}

static void SetDstBasePitch(CARD32 pitch, CARD32 base)
{
    /* copy to onscreen, HW start address is zero */
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetDstBasePitch()\n");

	/* Jong 11/08/2006; seems have a bug for base=64MB=0x4000000 */
	/* base >> 4 = 0x400000; 0x400000 & 0x3fffff = 0x0 */
	/* Thus, base address must be less than 64MB=0x4000000 */
    XG47SetGECmd(ENG_DST_BASE, ((pitch & 0x3ff0) << 18) | ((base >> 4) & 0x3fffff)); 

#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-SetDstBasePitch-((base >> 4) & 0x7fffff)=0x%x-\n", ((base >> 4) & 0x7fffff));
#endif

    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetDstBasePitch()\n");
}

static void SetSrcBasePitch(CARD32 pitch, CARD32 base)
{
    /* copy from onscreen, HW start address is zero */
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetSrcBasePitch()\n");

	/* Jong 11/08/2006; seems have a bug for base=64MB=0x4000000 */
	/* base >> 4 = 0x400000; 0x400000 & 0x3fffff = 0x0 */
	/* Thus, base address must be less than 64MB=0x4000000 */
    XG47SetGECmd(ENG_SRC_BASE, ((pitch & 0x3ff0) << 18) | ((base >> 4) & 0x3fffff)); 

    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetSrcBasePitch()\n");
}

static void SetDstXY(CARD32 x, CARD32 y, CARD32 addr)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetDstXY()\n");
    XG47SetGECmd(ENG_DESTXY, (((x & 0x1fff)<<16) | (y & 0x1fff) | (addr & 0xe0000000) | ((addr & 0x1c000000) >> 13)));
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetDstXY()\n");
}

static void SetSrcXY(CARD32 x, CARD32 y, CARD32 addr)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetSrcXY()\n");
    XG47SetGECmd(ENG_SRCXY, (((x & 0xfff)<<16) | (y & 0xfff)   | (addr & 0xe0000000) | ((addr & 0x1c000000) >> 13)));
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetSrcXY()\n");
}

static void SetDstSize(CARD32 w, CARD32 h)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetDstSize()\n");
    XG47SetGECmd(ENG_DIMENSION, (((w & 0xfff)<<16) | (h & 0xfff)));
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetDstSize()\n");
}

static void SetDrawingCommand(CARD32 cmd, CARD32 flag)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetDrawingCommand()\n");
    XG47SetGECmd(ENG_DRAWINGFLAG, (accel_info.engineFlag | flag));
    XG47SetGECmd(ENG_COMMAND, (accel_info.engineCmd | cmd));
    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetDrawingCommand()\n");
}

static void SetClip(CARD32 left, CARD32 top, CARD32 right, CARD32 bottom)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter SetClip()\n");

    accel_info.engineCmd |= FER_CLIP_ON;
    XG47SetGECmd(ENG_CLIPSTARTXY, (((left & 0xfff)<<16) |
                                   (top & 0xfff)));
    XG47SetGECmd(ENG_CLIPENDXY, (((right & 0xfff)<<16) |
                                 ((bottom>0xfff?0xfff:bottom) & 0xfff)));

    XGIDebug(DBG_FUNCTION, "[DBG] Leave SetClip()\n");
}

static void ResetClip()
{
    accel_info.clp_enable = FALSE;
    accel_info.engineCmd &= ~FER_CLIP_ON;
}

int testRWPCIE(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);

    unsigned long hd_addr;
    unsigned long *virt_addr;

	/* Jong 06/01/2006; test */
    unsigned long sz_test = 0x1000;
    /* unsigned long sz_test = 0x2000; */

    unsigned long res;
    int ret = 0;

	int i;

    PDEBUG(ErrorF("\n\t[2D]begin test rw in kernel\n"));
    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl-05292006][2D]begin test rw in kernel\n");

    if (!XGIPcieMemAllocate(pScrn,
                            sz_test, /* 20 byte */
                            0,
                            &hd_addr,
                            (unsigned long *)(&virt_addr)))
    {
        ErrorF("alloc memory for test kd write error\n");
    }
    else
    {
        ErrorF("alloc memory for test kd write correctly\n");
    }

	/* Jong 05/30/2006 */
	/* sleep(3); */

    PDEBUG(ErrorF("[Jong-2d] Initial value is [0x%8x]=0x%8x\n", hd_addr, *virt_addr));
    PDEBUG(ErrorF("[Jong-2d] virt_addr=0x%8x\n", virt_addr));

    res = 0xff0ff000;
    *virt_addr = res;

    PDEBUG(ErrorF("[Jong-2d] Initial value is [0x%8x]=0x%8x\n", hd_addr, *virt_addr));
    PDEBUG(ErrorF("[Jong-2d] virt_addr=0x%8x\n", virt_addr));

	/*
	for(i=0; i<16 ; i++)
		PDEBUG(ErrorF("[Jong06012006-2d] virt_addr[%d]=%x\n", i, *(virt_addr+i)));

	for(i=16; i<32 ; i++)
	{
	    *(virt_addr+i) = 0x123456;
		PDEBUG(ErrorF("[Jong06012006-2d] virt_addr[%d]=%x\n", i, *(virt_addr+i)));
	}
	*/

	/* Jong 05/30/2006 */
	/* sleep(3); */

    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] testRWPCIE()-1\n");
    ret = ioctl(pXGI->fd, XGI_IOCTL_TEST_RWINKERNEL, &hd_addr);
    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] testRWPCIE()-2\n");

	/* Jong 05/30/2006 */
	/* sleep(3); */

    if ( ret == -1)
    {
        ErrorF("[2D] ioctl XGI_IOCTL_TEST_RWINKERNEL error \n");
    }
    else
    {
        ErrorF("[2D] call ioctl XGI_IOCTL_TEST_RWINKERNEL = %d. \n", ret);
    }

    res = *virt_addr;

	/*
	for(i=0; i<32 ; i++)
		PDEBUG(ErrorF("[Jong06012006-2d] virt_addr[%d]=%x\n", i, *(virt_addr+i)));
	*/

    if( *virt_addr == 0x00f00fff)
    {
        ErrorF("[2D] kd write right: %x\n", *virt_addr);
    }
    else
    {
        ErrorF("[2D] kd write error: %x\n", *virt_addr);
    }

    XGIPcieMemFree(pScrn, sz_test, 0, hd_addr, virt_addr);

    ErrorF("\n\t[2D]End test rw in kernel.\n");

    return 1;
}

static Bool XG47InitCmdList(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);

    XGIDebug(DBG_FUNCTION, "[DBG]Enter XG47InitCmdList() - XGI_CMDLIST_ENABLE(1)\n");

	/* Jong 05/24/2006 */
    testRWPCIE(pScrn); 

    pXGI->cmdList.cmdBufSize = CMDBUF_SIZE;
    pXGI->cmdList.mmioBase   = (CARD32*) pXGI->IOBase;
    pXGI->cmdList.cmdBufLinearStartAddr = NULL;
    pXGI->cmdList.scratchPadLinearAddr = NULL;

    if (!XGIPcieMemAllocate(pScrn,
                            pXGI->cmdList.cmdBufSize * sizeof(CARD32),
                            0,
                            (CARD32*)(&(pXGI->cmdList.cmdBufHWStartAddr)),
                            (CARD32*)(&(pXGI->cmdList.cmdBufLinearStartAddr))))
    {
        XGIDebug(DBG_ERROR, "[DBG Error]Allocate CmdList buffer error!\n");

        XAADestroyInfoRec(pXGI->pXaaInfo);
        pXGI->pXaaInfo = NULL;

        return FALSE;
    }

    XGIDebug(DBG_CMDLIST, "[Malloc]cmdBuf VAddr=0x%X  HAddr=0x%X buffsize=0x%X\n",
            (CARD32)(pXGI->cmdList.cmdBufLinearStartAddr),
            (CARD32)(pXGI->cmdList.cmdBufHWStartAddr),
            (CARD32)(pXGI->cmdList.cmdBufSize));

    if (!XGIPcieMemAllocate(pScrn,
                            4*1024,
                            0,
                            (CARD32*)(&(pXGI->cmdList.scracthPadHWAddr)),
                            (CARD32*)(&(pXGI->cmdList.scratchPadLinearAddr))))
    {
        XGIDebug(DBG_ERROR, "[DBG ERROR]Allocate Scratch Pad error!\n");

        XGIPcieMemFree(pScrn,
                       pXGI->cmdList.cmdBufSize * sizeof(CARD32),
                       0,
                       (CARD32)(pXGI->cmdList.cmdBufHWStartAddr),
                       pXGI->cmdList.cmdBufLinearStartAddr);

        XGIDebug(DBG_ERROR, "[Error]Free cmdBuf. VAddr=0x%x  HAddr=0x%x\n",
                pXGI->cmdList.cmdBufLinearStartAddr, pXGI->cmdList.cmdBufHWStartAddr);

        pXGI->cmdList.cmdBufLinearStartAddr = NULL;

        XAADestroyInfoRec(pXGI->pXaaInfo);
        pXGI->pXaaInfo = NULL;

        return FALSE;
    }


    XGIDebug(DBG_CMDLIST, "[Malloc]Scratch VAddr=0x%x HAddr=0x%x\n",
           pXGI->cmdList.scratchPadLinearAddr, pXGI->cmdList.scracthPadHWAddr);

	XGIDebug(DBG_FUNCTION, "[DBG-Jong] XG47InitCmdList()-Before calling Initialize-pXGI->fd=%d\n", pXGI->fd);

    Initialize(pXGI->cmdList.cmdBufLinearStartAddr,
               pXGI->cmdList.cmdBufHWStartAddr,
               pXGI->cmdList.cmdBufSize,
               pXGI->cmdList.scratchPadLinearAddr,
               pXGI->cmdList.scracthPadHWAddr,
               pXGI->cmdList.mmioBase,
               pXGI->fd);

    XGIDebug(DBG_FUNCTION, "Leave XG47InitCmdList\n");

    return TRUE;
}

void XG47AccelExit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn   = xf86Screens[pScreen->myNum];
    XGIPtr      pXGI = XGIPTR(pScrn);

    XGIDebug(DBG_FUNCTION, "Enter XG47AccelExit\n");

    XG47StopCmd(TRUE);

    XG47WaitForIdle(pXGI);

    XG47DisableGE(pXGI);

    if (pXGI->cmdList.scratchPadLinearAddr != NULL)
    {
        XGIDebug(DBG_CMDLIST, "[DBG Free]Scratch VAddr=0x%x HAddr=0x%x\n",
               pXGI->cmdList.scratchPadLinearAddr, pXGI->cmdList.scracthPadHWAddr);

        XGIPcieMemFree(pScrn,
                       4*1024,
                       0,
                       (CARD32)(pXGI->cmdList.scracthPadHWAddr),
                       pXGI->cmdList.scratchPadLinearAddr);

        pXGI->cmdList.scratchPadLinearAddr = NULL;
    }

    if (pXGI->cmdList.cmdBufLinearStartAddr != NULL)
    {
        XGIDebug(DBG_CMDLIST, "[DBG Free]cmdBuf VAddr=0x%x  HAddr=0x%x\n",
               pXGI->cmdList.cmdBufLinearStartAddr, pXGI->cmdList.cmdBufHWStartAddr);

        XGIPcieMemFree(pScrn,
                       pXGI->cmdList.cmdBufSize * sizeof(CARD32),
                       0,
                       (CARD32)(pXGI->cmdList.cmdBufHWStartAddr),
                       pXGI->cmdList.cmdBufLinearStartAddr);

        pXGI->cmdList.cmdBufLinearStartAddr = NULL;
    }

    XGIDebug(DBG_FUNCTION, "Leave XG47AccelExit\n");
}


static void XG47BeginCmdList(CARD32 size)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47BeginCmdList() - XGI_CMDLIST_ENABLE(1)\n");
    if (accel_info.discardCmd == FALSE)
    {
        BeginCmdList(size);
    }
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47BeginCmdList()\n");
}

static void XG47EndCmdList()
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47EndCmdList() - XGI_CMDLIST_ENABLE(1)\n");
    EndCmdList();
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47EndCmdList()\n");
}


static void XG47SetGECmd(CARD32 addr, CARD32 cmd)
{
    XGIDebug(DBG_FUNCTION, "[DBG] Enter XG47SetGECmd() - XGI_CMDLIST_ENABLE(1)\n");
    SendGECommand(addr, cmd);
    XGIDebug(DBG_FUNCTION, "[DBG] Leave XG47SetGECmd()\n");
}

/* Jong 09/16/2006; support dual view */
#ifdef XGIDUALVIEW
static void
XGIRestoreAccelState(ScrnInfoPtr pScrn)
{
	XGIPtr pXGI = XGIPTR(pScrn);
	XG47WaitForIdle(pXGI);
}
#endif
