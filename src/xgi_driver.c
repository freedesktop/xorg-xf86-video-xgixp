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

/* X and server generic header files */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86RAC.h"
#include "xf86Resources.h"
#include "xf86cmap.h"
#include "xf86xv.h"
#include "vbe.h"

/* PCI vendor/device definitions */
#include "xf86PciInfo.h"

/* fbdevhw & vgahw */
#include "fbdevhw.h"
#include "vgaHW.h"
#include "dixstruct.h"

/* mi banking wrapper */
#include "mibank.h"
/* initialise a SW cursor */
#include "mipointer.h"
/* implementing backing store */
#include "mibstore.h"

/* colormap initialization */
#include "micmap.h"

/* VGA or Hercules monochrome screens */
#include "xf1bpp.h"
/* VGA or EGC 16-colour screens */
#include "xf4bpp.h"

#include "xf86Version.h"

#include "xgi_debug.h"

#include "fb.h"

/* Driver data structures */
#include "xgi.h"
#include "xgi_regs.h"
#include "xgi_version.h"
#include "xgi_option.h"
#include "xgi_misc.h"
#include "xgi_bios.h"
#include "xgi_mode.h"
#include "xgi_dga.h"
#include "xgi_accel.h"
#include "xgi_cursor.h"
#include "xgi_shadow.h"
#include "xgi_video.h"
#include "xgi_misc.h"
#include "xgi_driver.h"
#include "xgi_hwmc.h"
#include "xg47_mode.h"
#include "xg47_misc.h"
#include "xg47_accel.h"
#include "xg47_cursor.h"
#include "xg47_regs.h"
#include "xg47_cmdlist.h"

/* Jong 09/20/2006; support dual view */
extern int		g_DualViewMode;

/* Jong 10/04/2006; support different resolutions for dual view */
/* DisplayModePtr g_pCurrentModeOfFirstView=0; */

/* Jong 09/06/2006; support dual view */
#ifdef XGIDUALVIEW
static int	XGIEntityIndex = -1;
#endif

/* Jong 05/25/2006 */
#define DBG_FLOW        1

#define FB_MANAGED_BY_X 12*1024*1024

/* Jong 05/23/2006 */
/* #define OPENGL_HW_ACCEL */

#define XGI_XVMC

static void     XGIIdentify(int flags);
static Bool     XGIProbe(DriverPtr drv, int flags);
static Bool     XGIPreInit(ScrnInfoPtr pScrn, int flags);
static Bool     XGIScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);
static Bool     XGIEnterVT(int scrnIndex, int flags);
static void     XGILeaveVT(int scrnIndex, int flags);
static Bool     XGICloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool     XGISaveScreen(ScreenPtr pScreen, int mode);
/* Optional functions */
static void     XGIFreeScreen(int scrnIndex, int flags);
static int      XGIValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose,
                             int flags);

/* Internally used functions */
static Bool     XGIMapMMIO(ScrnInfoPtr pScrn);
static void XGIUnmapMMIO(ScrnInfoPtr pScrn);
static Bool     XGIMapFB(ScrnInfoPtr pScrn);
static void XGIUnmapFB(ScrnInfoPtr pScrn);
static void XGIUnmapMem(ScrnInfoPtr pScrn);

static void     XGISave(ScrnInfoPtr pScrn);
static void     XGIRestore(ScrnInfoPtr pScrn);
static Bool     XGIModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);

static void     XGIBlockHandler(int, pointer, pointer, pointer);

static void     XGIEnableMMIO(ScrnInfoPtr pScrn);
static void     XGIDisableMMIO(ScrnInfoPtr pScrn);

/* Jong 07/03/2006 */
ScreenPtr g_pScreen=NULL;

/*
static Bool     XGIPreInitDRI(ScrnInfoPtr pScrn);
*/
static const char *vgahwSymbols[] = {
    "vgaHWBlankScreen",
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIOBase",
    "vgaHWGetIndex",
    "vgaHWInit",
    "vgaHWLock",
    "vgaHWMapMem",
    "vgapVgaHWrotect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    "vgaHWSetMmioFuncs",
    "vgaHWUnlock",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86SetDDCproperties",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

static const char *xaaSymbols[] = {
/* Jong 07/05/2006; fail when using .so */
#ifndef _SO_
    "XAACopyROP", 
#endif
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAFillSolidRects",
    "XAAInit",
/* Jong 07/05/2006; fail when using .so */
#ifndef _SO_
    "XAAPatternROP", 
#endif
    "XAAScreenIndex",
    "XAAStippleScanlineFuncLSBFirst",
    "XAAOverlayFBfuncs",
    "XAACachePlanarMonoStipple",
    "XAAHelpPatternROP",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    "vbeFree",
    NULL
};

static const char *int10Symbols[] = {
    "xf86ExecX86int10",
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

static const char *shadowSymbols[] = {
    "shadowInit",
    NULL
};

static const char *fbdevHWSymbols[] = {
    "fbdevHWInit",
    "fbdevHWUseBuildinMode",
    "fbdevHWGetLineLength",
    "fbdevHWGetVidmem",
    "fbdevHWDPMSSet",
    /* colormap */
    "fbdevHWLoadPalette",
    /* ScrnInfo hooks */
    "fbdevHWAdjustFrame",
    "fbdevHWEnterVT",
    "fbdevHWLeaveVT",
    "fbdevHWModeInit",
    "fbdevHWRestore",
    "fbdevHWSave",
    "fbdevHWSwitchMode",
    "fbdevHWValidMode",
    "fbdevHWMapMMIO",
    "fbdevHWMapVidmem",
    "fbdevHWUnmapMMIO",
    "fbdevHWUnmapVidmem",
    NULL
};

static SymTabRec XGIChipsets[] = {
    { XG47,         "Volari 8300"   },
    { -1,           NULL            }
};

static PciChipsets XGIPciChipsets[] = {
    { XG47,             PCI_CHIP_0047,    RES_SHARED_VGA },
    { -1,               -1,               RES_UNDEFINED  }
};

/* Clock Limits */
static int PixelClockLimit8bpp[] = {
    267000, /* Volari 8300 */
};

static int PixelClockLimit16bpp[] = {
    267000, /* Volari 8300 */
};

static int PixelClockLimit24bpp[] = {
    267000, /* Volari 8300 */
};

static int PixelClockLimit32bpp[] = {
    267000, /* Volari 8300 */
};

/*
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

_X_EXPORT DriverRec XGI = {
    XGI_VERSION_CURRENT,
    XGI_DRIVER_NAME,
    XGIIdentify,
    XGIProbe,
    XGIAvailableOptions,
    NULL,
    0
};

static void XGILoaderRefSymLists(void)
{

    /*
     * Tell the loader about symbols from other modules that this module might
     * refer to.
     */
    xf86LoaderRefSymLists(vgahwSymbols,
                          ddcSymbols,
                          i2cSymbols,
                          fbSymbols,
                          xaaSymbols,
                          ramdacSymbols,
                          vbeSymbols,
                          int10Symbols,
                          shadowSymbols,
                          NULL);
}


static MODULESETUPPROTO(XGISetup);

/* Module loader interface for subsidiary driver module */
static XF86ModuleVersionInfo XGIVersionRec =
{
    XGI_DRIVER_NAME,
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    XGI_VERSION_MAJOR, XGI_VERSION_MINOR, XGI_VERSION_PATCH,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0,0,0,0}
};

_X_EXPORT XF86ModuleData xgixpModuleData =
{
    &XGIVersionRec,
    XGISetup,
    NULL
};

/*
 * XGISetup --
 *
 * This function is called every time the module is loaded.
 */

static pointer XGISetup(pointer module,
                        pointer options,
                        int *errorMajor,
                        int *errorMinor)
{
    static Bool isInited = FALSE;
    pointer result;

    /* This module should be loaded only once, but check to be sure. */

    if (!isInited)
    {
        /*
         * Modules that this driver always requires may be loaded
         * here by calling LoadSubModule().
         */
        isInited = TRUE;
        xf86AddDriver(&XGI, module, 0);
        LoaderRefSymLists(vgahwSymbols, fbSymbols, i2cSymbols, vbeSymbols,
                          ramdacSymbols, int10Symbols, xaaSymbols, shadowSymbols,
                          fbdevHWSymbols, NULL);

        /*
         * The return value must be non-NULL on success even though
         * there is no TearDownProc.
         */
        result = (pointer)TRUE;
    }
    else
    {
        if (errorMajor)
            *errorMajor = LDR_ONCEONLY;
        result = NULL;
    }

    return result;
}


static void XGIIdentify(int flags)
{
    xf86PrintChipsets(XGI_NAME, "driver for XGI chipsets", XGIChipsets);
}

static Bool XGIProbe(DriverPtr drv, int flags)
{
    int     i;
    int     *usedChips = NULL;
    int     numDevSections;
    int     numUsed;
    Bool    foundScreen = FALSE;
    GDevPtr *devSections;

    /*
     * Find the config file Device sections that match this
     * driver, and return if there are none.
     */
    if ((numDevSections = xf86MatchDevice(XGI_DRIVER_NAME, &devSections)) <= 0)
    {
        return FALSE;
    }

    /*
     * While we're VGA-dependent, can really only have one such instance, but
     * we'll ignore that.
    */

    /*
     * We need to probe the hardware first.  We then need to see how this
     * fits in with what is given in the config file, and allow the config
     * file info to override any contradictions.
    */

    /*
     * Since this is a PCI card, "probing" just amounts to checking
     * the PCI data that the server has already collected.  If there
     * is none, return.
     *
     * Although the config file is allowed to override things, it
     * is reasonable to not allow it to override the detection
     * of no PCI video cards.
     *
     * The provided xf86MatchPciInstances() helper takes care of
     * the details.
     */

    if (xf86GetPciVideoInfo() == NULL)
    {
        return FALSE;
    }
    numUsed = xf86MatchPciInstances(XGI_NAME, PCI_VENDOR_XGI,
                                    XGIChipsets, XGIPciChipsets, devSections,
                                    numDevSections, drv, &usedChips);

    if (numUsed <=0)
        return FALSE;

	/* Jong 09/28/2006; support dual view mode */
    if (numUsed >= 2)
		g_DualViewMode=1;

    if (numUsed > 0)
    {
        if (flags & PROBE_DETECT)
            foundScreen = TRUE;
        else for (i = 0; i < numUsed; i++)
        {
            ScrnInfoPtr pScrn = NULL;
            /* ScrnInfoPtr pScrn = xf86AllocateScreen(drv, 0); */

            if ((pScrn = xf86ConfigPciEntity(pScrn, flags, usedChips[i],
                                             XGIPciChipsets, NULL, NULL,
                                             NULL, NULL, NULL)))
            {
                /* Allocate a ScrnInfoRec */
                /* Fill in what we can of the ScrnInfoRec */
                pScrn->driverVersion = XGI_VERSION_CURRENT;
                pScrn->driverName    = XGI_DRIVER_NAME;
                pScrn->name          = XGI_NAME;
                pScrn->Probe         = XGIProbe;
                pScrn->PreInit       = XGIPreInit;
                pScrn->ScreenInit    = XGIScreenInit;
                pScrn->SwitchMode    = XGISwitchMode;
                pScrn->AdjustFrame   = XGIAdjustFrame;
                pScrn->EnterVT       = XGIEnterVT;
                pScrn->LeaveVT       = XGILeaveVT;
                pScrn->FreeScreen    = XGIFreeScreen;
                pScrn->ValidMode     = XGIValidMode;
                foundScreen = TRUE;
                /* add screen to entity */
            }

/* Jong 09/06/2006; support dual view */
#ifdef XGIDUALVIEW
			if(g_DualViewMode == 1)
			{
			   XGIEntityPtr pXGIEntity = NULL;
			   DevUnion  *pEntityPrivate;

			   xf86SetEntitySharable(usedChips[i]); 

			   if(XGIEntityIndex < 0) {
					XGIEntityIndex = xf86AllocateEntityPrivateIndex();
			   }

			   pEntityPrivate = xf86GetEntityPrivate(pScrn->entityList[0], XGIEntityIndex);

			   if(!pEntityPrivate->ptr) {
				  pEntityPrivate->ptr = xnfcalloc(sizeof(XGIEntityRec), 1);
				  pXGIEntity = pEntityPrivate->ptr;
				  memset(pXGIEntity, 0, sizeof(XGIEntityRec));
				  pXGIEntity->lastInstance = -1;
			   } else {
				  pXGIEntity = pEntityPrivate->ptr;
			   }

			   pXGIEntity->lastInstance++;
			   xf86SetEntityInstanceForScreen(pScrn, pScrn->entityList[0],
							pXGIEntity->lastInstance);
#endif /* XGIDUALVIEW */
			}
        }
        xfree(usedChips);
    }

    xfree(devSections);
    return foundScreen;
}

static XGIPtr XGIGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an XGIRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate == NULL) {
	XGIPtr pXGI = xnfcalloc(sizeof(XGIRec), 1);

	/* Initialise it */
	pXGI->pBiosDll = xnfcalloc(sizeof(XGIBiosDllRec), 1);

	pScrn->driverPrivate = pXGI;
	pXGI->pScrn = pScrn;
    }

    return (XGIPtr) pScrn->driverPrivate;
}

static void XGIFreeRec(ScrnInfoPtr pScrn)
{

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (pScrn->driverPrivate == NULL)
    {
        return;
    }
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

/* XGI Display Power Management Set */
static void XGIDPMSSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
	ErrorF("XGI-XGIDPMSSet()...\n");

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (pXGI->isFBDev)
    {
	    fbdevHWDPMSSet(pScrn, PowerManagementMode, flags);
	    return;
    }

    switch(pXGI->chipset)
    {
    case XG47:
		XG47DPMSSet(pScrn, PowerManagementMode, flags); /* Jong 07/24/2006 */
		break;

    default:
        break;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

/**
 * Memory map the MMIO region.  Used during pre-init.
 */
static Bool XGIMapMMIO(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!pXGI->IOBase) {
        if (pXGI->isFBDev) {
            pXGI->IOBase = fbdevHWMapMMIO(pScrn);
        }
        else {
            /* Map a virtual address IOBase from physical address IOAddr
             * for MMIO
             */
            pXGI->IOBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO,
                                         pXGI->pciTag, pXGI->IOAddr, 
                                         XGI_MMIO_SIZE);
        }
    }


    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "IO Map at 0x%p\n", pXGI->IOBase);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return pXGI->IOBase != NULL;
}


/**
 * Unmap the MMIO region.
 * 
 * \sa XGIUnmapMem, XGIUnmapFB
 */
static void XGIUnmapMMIO(ScrnInfoPtr pScrn)
{
    XGIPtr      pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!pXGI->IOBase)
    {
        if (pXGI->isFBDev) {
            fbdevHWUnmapMMIO(pScrn);
        }
        else {
            xf86UnMapVidMem(pScrn->scrnIndex, pXGI->IOBase, XGI_MMIO_SIZE);
        }

	pXGI->IOBase = NULL;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif
}

/**
 * Memory map the frame buffer.  Used by pre-init.
 */
static Bool XGIMapFB(ScrnInfoPtr pScrn)
{
    XGIPtr      pXGI = XGIPTR(pScrn);


#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!pXGI->fbBase) {
        if (pXGI->isFBDev) {
            pXGI->fbBase = fbdevHWMapVidmem(pScrn);
        }
        else {
            /* Make sure that the fbSize has been set (after
             * XGIPreInitMemory has been called) before attempting the
             * mapping.
             */
            if (pXGI->fbSize != 0) {
                pXGI->fbBase = xf86MapPciMem(pScrn->scrnIndex,
                                             VIDMEM_FRAMEBUFFER,
                                             pXGI->pciTag,
                                             pXGI->fbAddr,
                                             pXGI->fbSize); 

                xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                           "Frame Buffer Map at 0x%p\n", pXGI->fbBase);
            }
        }
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return pXGI->fbBase != NULL;
}


/**
 * Unmap the frame buffer from process address space.
 *
 * \sa XGIUnmapMem, XGIUnmapMMIO
 */
static void XGIUnmapFB(ScrnInfoPtr pScrn)
{
    XGIPtr      pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!pXGI->fbBase) {
        if (pXGI->isFBDev) {
            fbdevHWUnmapVidmem(pScrn);
        }
        else {
            xf86UnMapVidMem(pScrn->scrnIndex, pXGI->fbBase, pXGI->fbSize);
        }

	pXGI->fbBase = NULL;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif
}


/*
 * Unmap the MMIO region and the frame buffer.
 */
static void XGIUnmapMem(ScrnInfoPtr pScrn)
{
#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    XGIUnmapMMIO(pScrn);
    XGIUnmapFB(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif
}

static void XGIEnableMMIO(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    switch(pXGI->chipset)
    {
    case XG47:
        XG47EnableMMIO(pScrn);
        break;
    default:
        break;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

static void XGIDisableMMIO(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (pXGI->IOBase == 0) {
#if DBG_FLOW
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif
	return;
    }

    switch(pXGI->chipset)
    {
    case XG47:
        XG47DisableMMIO(pScrn);
        break;
    default:
        break;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

/*
 * Blank screen.
 */
static void XGIBlank(ScrnInfoPtr pScrn)
{
    /* XGIPtr      pXGI = XGIPTR(pScrn); */
}

/*
 * Unblank screen.
 */
static void XGIUnblank(ScrnInfoPtr pScrn)
{
    /* XGIPtr      pXGI = XGIPTR(pScrn); */
}

/*
 * Compute log base 2 of val
 */
int XGIMinBits(int val)
{
    int  bits;

    if (!val) return 1;
    for (bits = 0; val; val >>= 1, ++bits);
    return bits;
}

/*
 * Compute n/d with rounding.
 */
static int XGIDiv(int n, int d)
{
    return (n + (d / 2)) / d;
}

/*
 * This is called by XGIPreInit to set up the default visual.
 */
static Bool XGIPreInitVisual(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!xf86SetDepthBpp(pScrn, 8, 8, 8, (Support24bppFb
                                        | Support32bppFb
                                        | SupportConvert24to32)))
    {
        return FALSE;
    }

    /* Check that the returned depth is one we support */
    switch (pScrn->depth)
    {
    case 1:
    case 4:
    case 8:
        if (pScrn->bitsPerPixel != pScrn->depth)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Given depth (%d)/ fbbpp (%d) is not supported by this driver\n",
                       pScrn->depth, pScrn->bitsPerPixel);
            return FALSE;
        }
        break;
    case 15:
    case 16:
        if (pScrn->bitsPerPixel != 16)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Given depth (%d)/ fbbpp (%d) is not supported by this driver\n",
                       pScrn->depth, pScrn->bitsPerPixel);
            return FALSE;
        }
        break;
    case 24:
        if ((pScrn->bitsPerPixel != 24) && (pScrn->bitsPerPixel != 32))
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Given depth (%d)/ fbbpp (%d) is not supported by this driver\n",
                       pScrn->depth, pScrn->bitsPerPixel);
            return FALSE;
        }
        break;
    case 32:
        if (pScrn->bitsPerPixel != 32)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "Given depth (%d)/ fbbpp (%d) is not supported by this driver\n",
                       pScrn->depth, pScrn->bitsPerPixel);
            return FALSE;
        }
        break;
    default:
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Given depth (%d) is not supported by this driver\n",
                   pScrn->depth);
        return FALSE;
    }

    /* Print out the depth/bpp that was set */
    xf86PrintDepthBpp(pScrn);

    /* Get pixmap format */
    pXGI->pix24bpp = xf86GetBppFromDepth(pScrn, pScrn->depth);

    pXGI->currentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    pXGI->currentLayout.depth        = pScrn->depth;
    pXGI->currentLayout.bytesPerPixel= pScrn->bitsPerPixel/8;

    /* Set the default visual. */
    if (!xf86SetDefaultVisual(pScrn, -1))   return FALSE;

    /* We don't currently support DirectColor at > 8bpp */
    if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
                   " (%s) is not supported at depth %d\n",
                   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
        return FALSE;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

/*
 * This is called by XGIPreInit to handle all color weight issues.
 */
static Bool XGIPreInitWeight(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /*
     * Save flag for 6 bit DAC to use for
     * setting CRTC registers.  Otherwise use
     * an 8 bit DAC, even if xf86SetWeight sets
     * pScrn->rgbBits to some value other than 8.
     */
    pXGI->isDac8bits = FALSE;

    if (pScrn->depth > 8)
    {
        /* The defaults are OK for us */
        rgb defaultWeight = {0, 0, 0};
        rgb defaultMask = {0, 0, 0};

        if (!xf86SetWeight(pScrn, defaultWeight, defaultMask))
        {
            return FALSE;
        }
    }
    else
    {
        pScrn->rgbBits = 6;
        if (xf86ReturnOptValBool(pXGI->pOptionInfo, OPTION_DAC_8BIT, FALSE))
        {
            pScrn->rgbBits = 8;
            pXGI->isDac8bits = TRUE;
        }
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "Using %d bits per RGB (%d bit DAC)\n",
               pScrn->rgbBits, pXGI->isDac8bits ? 8 : 6);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

static Bool XGIPreInitInt10(ScrnInfoPtr pScrn)
{
    XGIPtr   pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

#if !defined(__alpha__)
    /* int10 is broken on some Alphas */
#ifdef VBE_INFO
    if (pXGI->pVbeModes)
    {
        pXGI->pVbe = VBEInit(NULL, pXGI->pEnt->index);
        pXGI->pInt10 = pXGI->pVbe->pInt10;
    }
    else
#endif
    {
        if(!pXGI->pInt10)
        {
            if (xf86LoadSubModule(pScrn, "int10"))
            {
                xf86LoaderReqSymLists(int10Symbols, NULL);
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Initializing int10\n");
                pXGI->pInt10 = xf86InitInt10(pXGI->pEnt->index);
            }
        }
    }
#endif

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

/*
 * This is called by XGIPreInit to handle config file overrides for things
 * like chipset and memory regions.  Also determine memory size and type.
 * If memory type ever needs an override, put it in this routine.
 */
static Bool XGIPreInitConfig(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    EntityInfoPtr   pEnt = pXGI->pEnt;
    GDevPtr         pDev  = pEnt->device;
    MessageType     from;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /* Chipset */
    from = X_PROBED;
    /*
    if (pDev->chipset && *pDev->chipset)
    {
        pXGI->chipset  = xf86StringToToken(XGIChipsets, pDev->chipset);
        from           = X_CONFIG;
    }
    else if (pDev->chipID >= 0)
    {
        pXGI->chipset  = pDev->chipID;
        from           = X_CONFIG;
    }
    else
    {
        pXGI->chipset = pXGI->pPciInfo->chipType;
    }
    pScrn->chipset = (char *)xf86TokenToString(XGIChipsets, pXGI->chipset);
    */
    if (pScrn->chipset == NULL)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "ChipID 0x%04X is not recognised\n", pXGI->chipset);
        return FALSE;
    }

    if (pXGI->chipset < 0)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "chipset \"%s\" is not recognised\n", pScrn->chipset);
        return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, from,
               "chipset: \"%s\" (chipID = 0x%04x)\n",
                pScrn->chipset,
                pXGI->chipset);

    /* Determine Frame Buffer base address */
	/* Jong 07/07/2006; base address of frame buffer from PCI configuration space */
	/* It's a physical address */
    from = X_PROBED;

    pXGI->fbAddr = pXGI->pPciInfo->memBase[0] & 0xFFFFFFF0; 

    if (pDev->MemBase)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "Linear address override, using 0x%08x instead of 0x%08x\n",
                    pDev->MemBase,
                    pXGI->fbAddr);
        pXGI->fbAddr = pDev->MemBase;
        from = X_CONFIG;
    }
    else if (!pXGI->fbAddr)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "No valid linear framebuffer address\n");
        return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lx\n",
               pXGI->fbAddr);

    /* MMIO registers */
    from = X_PROBED;

	/* Jong 07/07/2006; base address of MMIO from PCI configuration space */
	/* It's a physical address */
    pXGI->IOAddr = pXGI->pPciInfo->memBase[1] & 0xFFFFFFF0;

    if (pDev->IOBase)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "MMIO address override, using 0x%08x instead of 0x%08x\n",
                   pDev->IOBase,
                   pXGI->IOAddr);
        pXGI->IOAddr = pDev->IOBase;
        from = X_CONFIG;
    }
    else if (!pXGI->IOAddr)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid MMIO address\n");
        return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,"IO registers at 0x%lx\n", pXGI->IOAddr);


    /* BIOS */
    from = X_PROBED;

	/* Jong 07/07/2006; base address of VBIOS from PCI configuration space */
	/* It's a physical address */
	/* Is it the "Expansion ROM Base Address" at 30H ? */
    pXGI->BIOSAddr = pXGI->pPciInfo->biosBase & 0xFFFFFFF0;

    if (pDev->BiosBase)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "BIOS address override, using 0x%08x instead of 0x%08x\n",
                    pDev->BiosBase,
                    pXGI->BIOSAddr);
        pXGI->BIOSAddr = pDev->BiosBase;
        from = X_CONFIG;
    }

    if (pXGI->BIOSAddr)
    {
        xf86DrvMsg(pScrn->scrnIndex, from,
                   "BIOS at 0x%08lx\n", pXGI->BIOSAddr);
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

static Bool XGIPreInitDDC(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    vbeInfoPtr      pVbe;
    xf86MonPtr      pMon;
    Bool            result;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

#if defined(__powerpc__) || defined(__alpha__)
    /* Int10 is broken on PPC and some Alphas */
    return TRUE;
#else
    if (xf86LoadSubModule(pScrn, "vbe"))
    {
        xf86LoaderReqSymLists(vbeSymbols,NULL);
        pVbe = VBEInit(NULL, pXGI->pEnt->index);
        if (!pVbe) return FALSE;

        pMon = vbeDoEDID(pVbe, NULL);

#ifdef VBE_INFO
        {
            VbeInfoBlock* vbeInfoBlockPtr;
            if ((vbeInfoBlockPtr = VBEGetVBEInfo(pVbe)))
            {
                pXGI->pVbeModes = VBEBuildVbeModeList(pVbe,vbeInfoBlockPtr);
                VBEFreeVBEInfo(vbeInfoBlockPtr);
            }
        }

        vbeFree(pVbe);
#endif
        if (!xf86LoadSubModule(pScrn, "ddc")) return FALSE;

		PDEBUG(ErrorF("Jong-Before-xf86LoaderReqSymLists(ddcSymbols, NULL)\n"));
        xf86LoaderReqSymLists(ddcSymbols, NULL);
		PDEBUG(ErrorF("Jong-After-xf86LoaderReqSymLists(ddcSymbols, NULL)\n"));
        xf86SetDDCproperties(pScrn, xf86PrintEDID(pMon));
		PDEBUG(ErrorF("Jong-After-xf86SetDDCproperties(pScrn, xf86PrintEDID(pMon))\n"));

        result =  TRUE;
    }
    else
    {
        result = FALSE;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return result;

#endif
}

/*
 * This is called by XGIPreInit to initialize gamma correction.
 */
static Bool XGIPreInitGamma(ScrnInfoPtr pScrn)
{
    Gamma defaultGamma = {0.0, 0.0, 0.0};

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!xf86SetGamma(pScrn, defaultGamma)) return FALSE;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

/*
 * This is called by XGIPreInit to initialize I2C bus.
 */
static void XGII2CGetBits(I2CBusPtr b, int *clock, int *data)
{
    XGIPtr pXGI = ((XGIPtr)b->DriverPrivate.ptr);
    unsigned long val;

    OUTB(0x3D4, 0x37);
    val = INB(0x3D5);
    *clock = (val & 0x02) != 0;
    *data  = (val & 0x01) != 0;
}

static void XGII2CPutBits(I2CBusPtr b, int clock, int data)
{
    XGIPtr pXGI = ((XGIPtr)b->DriverPrivate.ptr);
    unsigned long val = 0x0C;

    if(clock)
    {
        val |= 2;
    }
    if(data)
    {
        val |= 1;
    }
    OUTB(0x3D4, 0x37);
    OUTB(0x3D5, val);
}

static Bool XGIPreInitI2c(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (xf86LoadSubModule(pScrn, "i2c"))
    {
        xf86LoaderReqSymLists(i2cSymbols,NULL);
    }
    else
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Failed to load i2c module\n");
        return FALSE;
    }

    pXGI->pI2C = xf86CreateI2CBusRec();
    if(!pXGI->pI2C) return FALSE;

    pXGI->pI2C->BusName    = "DDC";
    pXGI->pI2C->scrnIndex  = pScrn->scrnIndex;
    /* pXGI->ddcReg = ; */
    pXGI->pI2C->I2CPutBits = XGII2CPutBits;
    pXGI->pI2C->I2CGetBits = XGII2CGetBits;
    pXGI->pI2C->AcknTimeout = 5;

    if (!xf86I2CBusInit(pXGI->pI2C))
    {
        return FALSE;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

static Bool XGIPreInitLcdSize(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

    CARD16  lcdWidth = 0;
    CARD16  lcdHeight = 0;

    pXGI->lcdActive = FALSE;
    pXGI->lcdWidth = lcdWidth;
    pXGI->lcdHeight = lcdHeight;

    /* get LCD size if display on LCD */
    if ((pXGI->displayDevice & ST_DISP_LCD)
        || (IN3CFB(0x5B) & ST_DISP_LCD))
    {
        XGIGetLcdSize(pScrn, &lcdWidth, &lcdHeight);
        pXGI->lcdActive = TRUE;
        pXGI->lcdWidth = lcdWidth;
        pXGI->lcdHeight = lcdHeight;
    }

    return TRUE;
}

/* This is called by XGIPreInit to validate modes
 * and compute parameters for all of the valid modes.
 */
static Bool XGIPreInitModes(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    ClockRangePtr   pClockRange;

    int             modesFound;
    char            *mod = NULL;
    MessageType     from = X_PROBED;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /* there is programmable clock */
    pScrn->progClock = TRUE;

    /* Set the min pixel clock */
    pXGI->minClock = 12000;      /* XXX Guess, need to check this */
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock is %d MHz\n", pXGI->minClock / 1000);

    /*
     * If the user has specified ramdac speed in the XF86Config
     * file, we respect that setting.
     */
    if (pXGI->pEnt->device->dacSpeeds[0])
    {
        int speed = 0;

        switch (pScrn->bitsPerPixel)
        {
        case 8:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP8];
            break;
        case 16:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP16];
            break;
        case 24:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP24];
            break;
        case 32:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP32];
            break;
        }
        if (speed == 0)
            pXGI->maxClock = pXGI->pEnt->device->dacSpeeds[0];
        else
            pXGI->maxClock = speed;
        from = X_CONFIG;
    }
    else
    {
        switch (pScrn->bitsPerPixel)
        {
        case 16:
            pXGI->maxClock = PixelClockLimit16bpp[pXGI->chipset];
            break;
        case 24:
            pXGI->maxClock = PixelClockLimit24bpp[pXGI->chipset];
            break;
        case 32:
            pXGI->maxClock = PixelClockLimit32bpp[pXGI->chipset];
            break;
        default:
            pXGI->maxClock = PixelClockLimit8bpp[pXGI->chipset];
            break;
        }
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Max pixel clock is %d MHz\n", pXGI->maxClock / 1000);

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    pClockRange = xnfcalloc(sizeof(ClockRange), 1);
    pClockRange->next = NULL;

    pClockRange->minClock = pXGI->minClock;
    pClockRange->maxClock = pXGI->maxClock;
    pClockRange->clockIndex = -1;                /* programmable */
    pClockRange->interlaceAllowed = TRUE;
    pClockRange->doubleScanAllowed = FALSE;      /* XXX check this */

    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set. Since our XGIValidMode() already takes
     * care of this, we don't worry about setting them here.
     */

    if ((pScrn->depth < 8) || (pScrn->bitsPerPixel == 24))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                   "Disabling Engine due to 24bpp or < 8bpp.\n");
        pXGI->noAccel = TRUE;
    }

    /* Select valid modes from those available */
    modesFound = xf86ValidateModes(pScrn,
                                   pScrn->monitor->Modes,
                                   pScrn->display->modes,
                                   pClockRange,
                                   NULL,                /* linePitches */
                                   8 * 64,              /* minPitch */
                                   8 * 1024,            /* maxPitch */
                                   pScrn->bitsPerPixel, /* pitchInc */
                                   128,                 /* minHeight */
                                   4096,                /* maxHeight */
                                   pScrn->display->virtualX,
                                   pScrn->display->virtualY,
                                   pXGI->fbSize,
                                   LOOKUP_BEST_REFRESH);

    if (modesFound < 1 && pXGI->isFBDev)
    {
        fbdevHWUseBuildinMode(pScrn);
        pScrn->displayWidth = fbdevHWGetLineLength(pScrn)/(pScrn->bitsPerPixel/8);
        modesFound = 1;
    }

    if (modesFound == -1) return FALSE;

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    /* If no valid modes, return */
    if (!modesFound || !pScrn->modes)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
        return FALSE;
    }

    /*
     * Initialise the CRTC fields for the modes.  This driver expects
     * vertical values to be halved for interlaced modes.
     */
    xf86SetCrtcForModes(pScrn, 0);

    /* Set the current mode to the first in the list. */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used. */
    xf86PrintModes(pScrn);

    /* Set the DPI */
    xf86SetDpi(pScrn, 0, 0);

    /* Get ScreenInit function */
    mod = "fb";
    if (mod && !xf86LoadSubModule(pScrn, mod)) return FALSE;
    xf86LoaderReqSymLists(fbSymbols, NULL);

    pXGI->currentLayout.displayWidth = pScrn->displayWidth;
    pXGI->currentLayout.mode = pScrn->currentMode;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

/*
 * This is called by XGIPreInit to initialize the hardware cursor.
 */
static Bool XGIPreInitCursor(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!xf86ReturnOptValBool(pXGI->pOptionInfo, OPTION_SW_CURSOR, FALSE))
    {
        if (!xf86LoadSubModule(pScrn, "ramdac")) return FALSE;
        xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

/*
 * This is called by XGIPreInit to initialize hardware acceleration.
 */
static Bool XGIPreInitAccel(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

	PDEBUG(ErrorF("Jong-07052006-before xf86ReturnOptValBool()\n"));
    if (!xf86ReturnOptValBool(pXGI->pOptionInfo, OPTION_NOACCEL, FALSE))
    {
		PDEBUG(ErrorF("Jong-07052006-before xf86LoadSubModule('xaa')\n"));

		/* Jong 09/16/2006; will cause segmentation fault when loading at second time */
		/* FirstView =0 when single view */
		/* if(pXGI->FirstView) */
		if(!g_DualViewMode || (pXGI->FirstView) )
			if (!xf86LoadSubModule(pScrn, "xaa")) return FALSE;

        xf86LoaderReqSymLists(xaaSymbols, NULL);
		PDEBUG(ErrorF("Jong-07052006-After xf86LoaderReqSymLists(xaaSymbols)\n"));
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

static Bool XGIPreInitMemory(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    char            *ramType = NULL;
    char            *chipset = NULL;
    double          real;
    MessageType     from;

	/* Jong 09/11/2006; support dual view */
#ifdef XGIDUALVIEW
	/* FB base of first view starts right from base address */
	/* got from PCI config space or config file				*/
	/* its FB size is set pXGI->freeFbSize / 2				*/

	/* Second view need aan offset; first view keep pXGI->fbAddr */
	/* Jong 09/20/2006; only dual view takes effect */
	/* if(!pXGI->FirstView) */
	if(g_DualViewMode && !pXGI->FirstView) 
	{
		/* Jong 09/21/2006; 32MB: offset of second view starting address is  0x2000000 (0x4000000=64MB is total size) */
		/* but starting address of W2 is limited to 25 bits long */
		pXGI->freeFbSize /= 2;
		/* Jong 09/26/2006; try 32MB */
		/* pXGI->fbAddr += pXGI->freeFbSize - 4096;*/ /* 0x1FFF000 */
		/* pXGI->freeFbSize += 4096; */

		/* Jong 10/02/2006; use pXGI->freeFbSize /= 2 instead */
		/* Jong 11/09/2006; pXGI->fbAddr must be less than 0x4000000 (64MB) */
		if(pXGI->freeFbSize >= 0x4000000)
			pXGI->fbAddr += pXGI->freeFbSize - 1024; 
		else
			pXGI->fbAddr += pXGI->freeFbSize; 

		/* pXGI->fbAddr += pXGI->freeFbSize; */ /* 32MB ; but need to be calculated with freeFbSize */
		/* pXGI->fbAddr += 0x2000000; */ /* 32MB ; but need to be calculated with freeFbSize */
    }
	else
	{
		/* Use half of the memory available for each view */
		pXGI->freeFbSize /= 2;
		/* Jong 09/21/2006; starting address of W2 is limited to 25 bits long */
		/* pXGI->freeFbSize -= 4096; */
	}
#endif

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /* Determine RAM Type */
    switch (pXGI->chipset)
    {
    case XG47:
        pXGI->isDDRAM = TRUE;
        pXGI->frequency = NTSC;
        chipset = "XG47";
        break;
    default:
        if (!chipset)
        {
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No support for \"%s\"\n", pScrn->chipset);
            return FALSE;
        }
        break;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Found %s chip\n", chipset);

    if (ramType)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "RAM type is %s\n", ramType);
    }

    /* Determine video memory */
    from = X_PROBED;

	/* Jong 09/11/2006; got from config file */
    if (pXGI->pEnt->device->videoRam != 0)
    {
        pScrn->videoRam = pXGI->pEnt->device->videoRam;

        from = X_CONFIG;
    }
    else if (pXGI->chipset == XG47)
    {
		/* Jong 09/11/2006; FB size from VBIOS */
        pScrn->videoRam = (pXGI->biosFbSize/1024); /* get from bios */
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "VideoRAM: %x KByte\n",
               pScrn->videoRam);

    /* memory clock */
    pXGI->memClock = 0;
    pXGI->overrideMemClock = 0;
    pXGI->memClock = XGICalculateMemoryClock(pScrn);
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Memory Clock is %3.2f MHz\n", pXGI->memClock);
    if (xf86GetOptValFreq(pXGI->pOptionInfo, OPTION_SETMCLK, OPTUNITS_MHZ, &real))
    {
        pXGI->overrideMemClock = (int)(real * 1000.0);
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                   "Setting new Memory Clock to %3.2f MHz\n",
                   (float)(pXGI->overrideMemClock / 1000));
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}
static void XGIPreInitClock(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    MessageType     from = X_PROBED;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /* Set the min pixel clock */
    pXGI->minClock = 12000;      /* XXX Guess, need to check this */
    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock is %d MHz\n", pXGI->minClock / 1000);

    /*
     * If the user has specified ramdac speed in the XF86Config
     * file, we respect that setting.
     */
    if (pXGI->pEnt->device->dacSpeeds[0])
    {
        int speed = 0;

        switch (pScrn->bitsPerPixel)
        {
        case 8:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP8];
            break;
        case 16:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP16];
            break;
        case 24:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP24];
            break;
        case 32:
            speed = pXGI->pEnt->device->dacSpeeds[DAC_BPP32];
            break;
        }
        if (speed == 0)
            pXGI->maxClock = pXGI->pEnt->device->dacSpeeds[0];
        else
            pXGI->maxClock = speed;
        from = X_CONFIG;
    }
    else
    {
        switch (pScrn->bitsPerPixel)
        {
        case 16:
            pXGI->maxClock = PixelClockLimit16bpp[pXGI->chipset];
            break;
        case 24:
            pXGI->maxClock = PixelClockLimit24bpp[pXGI->chipset];
            break;
        case 32:
            pXGI->maxClock = PixelClockLimit32bpp[pXGI->chipset];
            break;
        default:
            pXGI->maxClock = PixelClockLimit8bpp[pXGI->chipset];
            break;
        }
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Max pixel clock is %d MHz\n", pXGI->maxClock / 1000);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

static Bool XGIPreInitShadow(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /* Load shadow if needed */
    if (pXGI->isShadowFB)
    {
        if (!xf86LoadSubModule(pScrn, "shadow"))
        {
            XGIFreeRec(pScrn);
            return FALSE;
        }
        xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

static Bool XGIPreInitFD(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    pXGI->fd = open("/dev/xgi", O_RDWR); 

    if (pXGI->fd < 0)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Open /dev/xgi failed !\n");
        return FALSE;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

static void XGIProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (xf86LoadSubModule(pScrn, "vbe"))
    {
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
        vbeFree(pVbe);
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif
}

/* XGIPreInit is called once at server startup. */
Bool XGIPreInit(ScrnInfoPtr pScrn, int flags)
{
    XGIPtr           pXGI;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

/* Jong 11/07/2006 */
/* #ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-XGIPreInit\n");
	XGIDumpRegisterValue(g_pScreen);
#endif */

	/* Jong 11/07/2006; test */
    /* OUTB(XGI_REG_GRX, 0x5D);
    OUTB(XGI_REG_GRX+1, 0x29); */

    /* Check the number of entities registered for the screen against the expected
     * number (most drivers expect only one).The entity information for
     * each of them should be retrieved by calling xf86GetEntityInfo()).
     * And check for the correct bus type and that none of the sharable
     * resources registered during the Probe phase was rejected.
     */

    /* Access to resources for the entities that can be controlled in a
     * device-independent way are enabled before this function is called.
     * If the driver needs to access any resources that it has disabled in
     * an EntityInit() function that it registered, then it may enable them
     * here providing that it disables them before this function returns.
     */
    if (pScrn->numEntities != 1) return FALSE;

    /* Allocate the XGIRec driverPrivate */

    pXGI = XGIGetRec(pScrn);
    if (pXGI == NULL) return FALSE;


    /* Get the entity, and make sure it is PCI. */
    pXGI->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

    if (pXGI->pEnt->resources) return FALSE;
    if (pXGI->pEnt->location.type != BUS_PCI)   goto fail;

/* Jong 09/06/2006; support dual view */
/* pXGI->pEntityPrivate hasn't been defined */
#ifdef XGIDUALVIEW
    if(xf86IsEntityShared(pScrn->entityList[0])) 
	{
		pXGI->pEntityPrivate = xf86GetEntityPrivate(pScrn->entityList[0], XGIEntityIndex)->ptr;

		if(!xf86IsPrimInitDone(pScrn->entityList[0])) 
		{
			pXGI->FirstView = TRUE; /* Jong 09/15/2006; assume it's DVI */
		}
		else
		{
			pXGI->FirstView = FALSE; /* Jong 09/15/2006; assume it's CRT */
		}
    }
#endif


    pXGI->pPciInfo = xf86GetPciInfoForEntity(pXGI->pEnt->index);
    pXGI->pciTag = pciTag(pXGI->pPciInfo->bus,
                          pXGI->pPciInfo->device,
                          pXGI->pPciInfo->func);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "PCI bus %d card %d func %d\n",
               pXGI->pPciInfo->bus,
               pXGI->pPciInfo->device,
               pXGI->pPciInfo->func);

    pXGI->chipset = pXGI->pEnt->chipset;
    pScrn->chipset = (char *)xf86TokenToString(XGIChipsets, pXGI->pEnt->chipset);

    pXGI->isFBDev = FALSE;

    /* Fill in the monitor field, just Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    if (flags & PROBE_DETECT)
    {
        XGIProbeDDC(pScrn, pXGI->pEnt->index);
        return TRUE;
    }

    if (!xf86LoadSubModule(pScrn, "vgahw")) return FALSE;
    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    if (!vgaHWGetHWRec(pScrn))
    {
        XGIFreeRec(pScrn);
        return FALSE;
    }

    /* All additional resources that the screen needs must be registered here.
     * This should be done with xf86RegisterResources(). If some of the fixed resources
     * registered in the Probe phase are not needed or not decoded by the hardware
     * when in the OPERATING server state, their status should be updated with
     * xf86SetOperatingState().
     */

    /* Register the PCI-assigned resources. */
    if (xf86RegisterResources(pXGI->pEnt->index, NULL, ResExclusive))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86RegisterResources() found resource conflicts\n");
        return FALSE;
    }

    /*
     * Set the depth/bpp.  Use the globally preferred depth/bpp.  If the driver
     * has special default depth/bpp requirements, the defaults should be specified
     * here explicitly. Our default depth is 8, so pass it to the helper function.
     * We support both 24bpp and 32bpp framebuffer layouts. Our preference for
     * depth 24 is 24bpp, so tell it that too. This sets pScrn->display also.
     */
    if (!XGIPreInitVisual(pScrn))   goto fail;

    /*
     * xf86SetWeight() and xf86SetDefaultVisual() must be called to
     * set weight/mask/offset for depth > 8 after pScrn->display is initialised.
     */
    if (!XGIPreInitWeight(pScrn))           goto fail;

    if (!XGIPreInitGamma(pScrn))            goto fail;

    if (!XGIProcessOptions(pScrn))          goto fail;

    if (!XGIPreInitConfig(pScrn))           goto fail;
    if (pXGI->isFBDev)
    {

	/* Jong 07/07/2006; has error - _dl_catch_error() from /lib/ld-linux.so.2 ???? */
        if (!xf86LoadSubModule(pScrn, "fbdevhw")) return FALSE;
        xf86LoaderReqSymLists(fbdevHWSymbols, NULL);

        /* check for linux framebuffer device */
        if (!fbdevHWInit(pScrn, pXGI->pPciInfo, NULL)) return FALSE;
        /*pScrn->SwitchMode   = fbdevHWSwitchMode;
        pScrn->AdjustFrame  = fbdevHWAdjustFrame;
        pScrn->ValidMode    = fbdevHWValidMode;*/
    }
    if (!XGIPreInitFD(pScrn))               goto fail;

    /* Enable MMIO */
    if (!pXGI->noMMIO)
    {
        if (!XGIMapMMIO(pScrn)) {
            goto fail;
        }

        XGIEnableMMIO(pScrn);
    }

    if (!pXGI->isFBDev && !XGIPreInitInt10(pScrn)) {
        goto fail;
    }

    if (!XGIBiosDllInit(pScrn))             goto fail;
    if (!XGIPreInitMemory(pScrn))           goto fail;

    /* pScrn->videoRam is determined by XGIPreInitMemory() */
    pXGI->fbSize = pScrn->videoRam * 1024;

    if (!XGIMapFB(pScrn))                   goto fail;

    /* Don't fail on this one */
    if (!XGIPreInitDDC(pScrn))              goto fail;

    if (!XGIPreInitLcdSize(pScrn))          goto fail;

    if (!XGIPreInitModes(pScrn))            goto fail;

    if (!XGIPreInitCursor(pScrn))           goto fail;

    if (!XGIPreInitAccel(pScrn))            goto fail;

    if(!XGIPreInitI2c(pScrn))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "I2C initialization failed!\n");
    }

    if(!XGIPreInitShadow(pScrn))
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "shadow initialization failed!\n");
    }

/*
#ifdef XF86DRI
    if (!XGIPreInitDRI(pScrn))                goto fail;
#endif
*/

    /* Free the video bios (if applicable) */
    if (pXGI->biosBase)
    {
        xfree(pXGI->biosBase);
        pXGI->biosBase = NULL;
    }

    /* Free int10 info
    if (pXGI->pInt10) xf86FreeInt10(pXGI->pInt10);
    */

    /* Decide which operations need to be placed under resource access control.
     * The classes of operations are the frame buffer operations (RAC_FB),
     * the pointer operations (RAC_CURSOR), the viewport change operations (RAC_VIEWPORT)
     * and the colormap operations (RAC_COLORMAP). Any operation that requires resources
     * which might be disabled during OPERATING state should be set to use RAC.
     * This can be specified separately for memory and IO resources (the racMemFlags
     * and racIoFlags fields of the ScrnInfoRec respectively).
     */

    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

    if (pXGI->isMmioOnly)
    {
        pScrn->racIoFlags = 0;
    }
    else
    {
        pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
    }

/* Jong 09/11/2006; support dual view */
#ifdef XGIDUALVIEW
    xf86SetPrimInitDone(pScrn->entityList[0]);
#endif

    return TRUE;
fail:
    if (!pXGI->noMMIO)
    {
        XGIDisableMMIO(pScrn);
    }
    XGIUnmapMem(pScrn);
    /* Free the video bios (if applicable) */
    if (pXGI->biosBase)
    {
      xfree(pXGI->biosBase);
      pXGI->biosBase = NULL;
    }

    if (pXGI->pVbe)
    {
      vbeFree(pXGI->pVbe);
      pXGI->pVbe = NULL;
    }

    /* Free int10 info */
    if (pXGI->pInt10)
    {
      xf86FreeInt10(pXGI->pInt10);
      pXGI->pInt10 = NULL;
    }
    vgaHWFreeHWRec(pScrn);

    XGIFreeRec(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return FALSE;
}

static void XGIBlockHandler(int i, pointer pBlockData, pointer pTimeout, pointer pReadmask)
{
    ScreenPtr      pScreen = screenInfo.screens[i];
    ScrnInfoPtr    pScrn = xf86Screens[i];
    XGIPtr         pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    /* xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); */
#endif

    pScreen->BlockHandler = pXGI->BlockHandler;
    (*pScreen->BlockHandler)(i, pBlockData, pTimeout, pReadmask);
    pScreen->BlockHandler = XGIBlockHandler;

#ifdef XvExtension
    if(pXGI->VideoTimerCallback)
    {
        UpdateCurrentTime();
        (*pXGI->VideoTimerCallback)(pScrn, currentTime.milliseconds);
    }
#endif

#if DBG_FLOW
    /* xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__); */
#endif

}

/* XGISave:Save the current video state. This could be called from ChipScreenInit() and
 * (possibly) ChipEnterVT().This will only be saving pre-server states or states before
 * returning to the server. There is only one current saved state per screen and it is
 * stored in private storage in the screen.
 */
static void XGISave(ScrnInfoPtr pScrn)
{
    vgaHWPtr    pVgaHW = VGAHWPTR(pScrn);
    vgaRegPtr   pVgaReg = &VGAHWPTR(pScrn)->SavedReg;
    XGIPtr      pXGI = XGIPTR(pScrn);
    XGIRegPtr   pXGIReg = &pXGI->savedReg;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (pXGI->isFBDev)
    {
    	fbdevHWSave(pScrn);
    	return;
    }

    /*vgaHWUnlock(pVgaHW);*/
    vgaHWSave(pScrn, pVgaReg, VGA_SR_MODE | VGA_SR_CMAP |
                              (IsPrimaryCard ? VGA_SR_FONTS : 0));
    /*vgaHWLock(pVgaHW);*/

    XGIModeSave(pScrn, pXGIReg);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

/*
 * Restore the initial (text) mode.
 */
/* Restore the original video state. This could be called from the ChipLeaveVT()
 * and ChipCloseScreen() functions.
 * XGIRestore: Restores the saved state from the private storage.
 * Usually only used for restoring text modes.
 */

static void XGIRestore(ScrnInfoPtr pScrn)
{
    vgaHWPtr    pVgaHW = VGAHWPTR(pScrn);
    XGIPtr      pXGI = XGIPTR(pScrn);
    vgaRegPtr   pVgaReg = &pVgaHW->SavedReg;
    XGIRegPtr   pXGIReg = &pXGI->savedReg;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    XGIModeRestore(pScrn, pXGIReg);
/*
    vgaHWUnlock(pVgaHW);

    vgaHWRestore(pScrn, pVgaReg, VGA_SR_MODE | VGA_SR_CMAP |
                                 (IsPrimaryCard ? VGA_SR_FONTS : 0));
    vgaHWLock(pVgaHW);
*/
#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

Bool XGIPutScreenInfo(ScrnInfoPtr pScrn)
{
    XGIPtr              pXGI = XGIPTR(pScrn);
    XGIScreenInfoRec    scrnInfo;
    int                 ret;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    scrnInfo.scrnStart  = 0;
    scrnInfo.scrnWidth  = pXGI->currentLayout.mode->HDisplay;
    scrnInfo.scrnHeight = pXGI->currentLayout.mode->VDisplay;
    scrnInfo.scrnBpp    = pScrn->bitsPerPixel >> 3;
    scrnInfo.scrnPitch  = scrnInfo.scrnWidth * scrnInfo.scrnBpp;
    /*scrnInfo.scrnPitch  = pScrn->displayWidth * scrnInfo.scrnBpp;*/

    ret = ioctl(pXGI->fd, XGI_IOCTL_PUT_SCREEN_INFO, &scrnInfo);
    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIPutScreenInfo()-1\n");

    if (ret < 0)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "failed to get screen info !\n");
        return FALSE;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

Bool XGIFBManagerInit(ScreenPtr pScreen)
{
    ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];
    XGIPtr          pXGI = XGIPTR(pScrn);
    BoxRec          availFBArea;
    CARD16          temp;
    Bool            ret;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    availFBArea.x1 = 0;
    availFBArea.y1 = 0;
    availFBArea.x2 = pScrn->displayWidth;

    temp = (pXGI->fbSize - XGI_CURSOR_BUF_SIZE) / (pScrn->displayWidth * pScrn->bitsPerPixel / 8);

    if (temp > 2047)
    {
        availFBArea.y2 = 2047;
    }
    else
    {
        availFBArea.y2 = temp;
    }

    /* Let 12M FB to be managed by XServer */
/*
    temp = FB_MANAGED_BY_X / (pScrn->displayWidth * pScrn->bitsPerPixel / 8);

    if (temp > 2047)
    {
        availFBArea.y2 = 2048;
    }
    else
    {
        availFBArea.y2 = 2048;
    }
*/
    /* XAA uses FB manager for its pixmap cahe */
    ret = xf86InitFBManager(pScreen, &availFBArea);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return ret;
}

#ifdef OPENGL_HW_ACCEL
static Bool (* OHAScreenInit)(ScreenPtr pScreen);
static Bool (* OHACleanup)(ScreenPtr pScreen);
#endif

/* Called at the start of each server generation. */
Bool XGIScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    /* First Get the ScrnInfoRec */
    ScrnInfoPtr pScrn  = xf86Screens[pScreen->myNum];
    XGIPtr      pXGI = XGIPTR(pScrn);
    vgaHWPtr    pVgaHW = VGAHWPTR(pScrn);
    VisualPtr   pVisual;

    int         width, height, displayWidth;
    Bool        retValue;
    CARD8       *pFBStart;
    int         visualMask;

	int			i; /* Jong 07/14/2006 */

	/* Jong 10/04/2006; debug different resolutions for dual view */
	/* if(pScreen->myNum == 0)
		g_pCurrentModeOfFirstView=pScrn->currentMode; */

	/* Jong 07/03/2006 */
	g_pScreen=pScrn;
	ErrorF("XGI-XGIScreenInit()...\n");

	/* Jong 11/07/2006; test */
	/* XGIDumpRegisterValue(g_pScreen); */
    /* OUTB(XGI_REG_GRX, 0x5D);
    OUTB(XGI_REG_GRX+1, 0x29); */
	/* ErrorF("XGI-XGIScreenInit()-After set 3CE-0x5D..\n");
	XGIDumpRegisterValue(g_pScreen); */

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    XGITRACE(("XGIScreenInit %x %d\n", pScrn->memPhysBase, pScrn->fbOffset));

#ifdef OPENGL_HW_ACCEL
    OHAScreenInit = LoaderSymbol("OHAScreenInit");
    OHACleanup = LoaderSymbol("OHACleanup");
#endif

    pScrn->memPhysBase = pXGI->fbAddr;

	/* Jong 09/22/2006; to save offset of frame buffer for */
	/* setting Des and Src base in acceleration function */
	/* pScrn->fbOffset = 0; */
	if(!g_DualViewMode || (pXGI->FirstView) )
	    pScrn->fbOffset = 0;
	else
	{
		/* Jong 11/09/2006; base address must be less than 64MB=0x4000000 */
		pScrn->fbOffset = pXGI->fbAddr - (pXGI->pPciInfo->memBase[0] & 0xFFFFFFF0);
		/* pScrn->fbOffset = pXGI->fbAddr - (pXGI->pPciInfo->memBase[0] & 0xFFFFFFF0); */

		if(pScrn->fbOffset >= 0x4000000) pScrn->fbOffset = 0x4000000 - 1024;
	}

    /*
     * If using the vgahw module, its data structures and related
     * things are typically initialised/mapped here.
     * Map the VGA memory and get the VGA IO base
     */
    if (IsPrimaryCard)
    {
        if (!vgaHWMapMem(pScrn))
            return FALSE;
    }

    if (!pXGI->noMMIO)
    {
        if (!XGIMapMMIO(pScrn)) {
            goto fail;
        }

        XGIEnableMMIO(pScrn);

        /* Initialize the MMIO vgahw functions */
        vgaHWSetMmioFuncs(pVgaHW, pXGI->IOBase, 0);
    }

    if (!XGIMapFB(pScrn))           goto fail;

    if (!XGIPreInitInt10(pScrn))    goto fail;


    /*
     * Save the current video card state. Enough state must be
     * saved so that the original state can later be restored.
     */
    XGISave(pScrn);

#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif
    /*
     * Initialise the first video mode.
     * The ScrnInfoRec's vtSema field should be set to TRUE
     * just prior to changing the video hardware's state.
     */
    pXGI->isNeedCleanBuf = TRUE;

    if (pXGI->isFBDev)
    {
        if (!fbdevHWModeInit(pScrn, pScrn->currentMode))    goto fail;
    }
    else
    {
        if (!XGIModeInit(pScrn, pScrn->currentMode))        goto fail;
    }

    if (!XGIPutScreenInfo(pScrn))                           goto fail;

    XGISaveScreen(pScreen, SCREEN_SAVER_ON);
    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default choices
     * for things like visual layouts and bits per RGB are OK, this may be
     * as simple as calling the framebuffer's ScreenInit() function.
     * If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     *
     * For most PC hardware at depths >= 8, the defaults that cfb uses
     * are not appropriate.  In this driver, we fixup the visuals after.
     */

    /* Reset the visual list */
    miClearVisualTypes();

    /*
     * Setup the visuals we supported.  This driver only supports
     * TrueColor for bpp > 8, so the default set of visuals isn't
     * acceptable.  To deal with this, call miSetVisualTypes with
     * the appropriate visual mask.
     */
    if (pScrn->bitsPerPixel > 8)
        visualMask = miGetDefaultVisualMask(pScrn->depth);
    else
        visualMask = TrueColorMask;

    if (!miSetVisualTypes(pScrn->depth, visualMask, pScrn->rgbBits,
                          pScrn->defaultVisual))
    {
        goto fail;
    }

    if (!miSetPixmapDepths())   goto fail;

    /* FIXME - we don't do shadowfb for < 4 */
    displayWidth = pScrn->displayWidth;
    if (pXGI->rotate)
    {
        height = pScrn->virtualX;
        width = pScrn->virtualY;
    }
    else
    {
        width = pScrn->virtualX;
        height = pScrn->virtualY;
    }

    if (pXGI->isShadowFB)
    {
        pXGI->shadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pXGI->pShadow = xalloc(pXGI->shadowPitch * height);
        displayWidth = pXGI->shadowPitch / (pScrn->bitsPerPixel >> 3);
        pFBStart = pXGI->pShadow;
    }
    else
    {
        pXGI->isShadowFB = FALSE;
        pXGI->pShadow = NULL;
        pFBStart = pXGI->fbBase;
    }
    /*
     * Initialise the framebuffer.
     * Call the framebuffer layer's ScreenInit function,
     * and fill in other pScreen fields.
     */

    retValue = fbScreenInit(pScreen, pFBStart,
                            pScrn->virtualX, pScrn->virtualY,
                            pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth,
                            pScrn->bitsPerPixel);

    if (!retValue)  goto fail;
    /*
     * Set initial black & white colourmap indices.
     */
    xf86SetBlackWhitePixels(pScreen);

    /* Override the default mask/offset settings */
    if (pScrn->bitsPerPixel > 8)
    {
        /* Fixup RGB ordering */
        pVisual = pScreen->visuals + pScreen->numVisuals;
        while (--pVisual >= pScreen->visuals)
        {
            if ((pVisual->class | DynamicClass) == DirectColor)
            {
                pVisual->offsetRed = pScrn->offset.red;
                pVisual->offsetGreen = pScrn->offset.green;
                pVisual->offsetBlue = pScrn->offset.blue;
                pVisual->redMask = pScrn->mask.red;
                pVisual->greenMask = pScrn->mask.green;
                pVisual->blueMask = pScrn->mask.blue;
            }
        }
    }

    /* must be after RGB order fixed */
    fbPictureInit(pScreen, 0, 0);

    if (!XGIFBManagerInit(pScreen))
    {
        xf86DrvMsg(scrnIndex, X_ERROR, "FB Manager init failed \n");
    }

    if (!pXGI->isShadowFB)
    {
        XGIDGAInit(pScreen);
    }

    /* 2D accel Initialize */
    if (!pXGI->noAccel)
    {
        if (XGIAccelInit(pScreen))
        {
            xf86DrvMsg(scrnIndex, X_INFO, "Acceleration enabled\n");
		    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-Acceleration enabled\n");
            pXGI->noAccel = FALSE;
        }
        else
        {
            xf86DrvMsg(scrnIndex, X_ERROR, "Acceleration initialization failed\n");
            xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
            pXGI->noAccel = TRUE;
        }
    }
    else
    {
        xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
        pXGI->noAccel = TRUE;
    }

    /*
     * If banking is needed, initialise an miBankInfoRec (defined in
     * "mibank.h"), and call miInitializeBanking().

    if (!miInitializeBanking(pScreen, pScrn->virtualX, pScrn->virtualY,
                             pScrn->displayWidth, pBankInfo))
        return FALSE;
     */

    /* If backing store is to be supported (as is usually the case), initialise it. */
    miInitializeBackingStore(pScreen);
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After miInitializeBackingStore()\n");

    xf86SetBackingStore(pScreen);
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After xf86SetBackingStore()\n");

    /* Set Silken Mouse */
    xf86SetSilkenMouse(pScreen);
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After xf86SetSilkenMouse()\n");

    /*
     * Hardware cursor setup.  This example is for
     * the mi software cursor.
     */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After miDCInitialize()\n");

    if (pXGI->isHWCursor)
    {
        if (XGIHWCursorInit(pScreen))
        {
            int width, height;

            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                       "Using hardware cursor (scanline %d)\n",
                       pXGI->cursorStart / pScrn->displayWidth);
            if (xf86QueryLargestOffscreenArea(pScreen, &width, &height,
                                              0, 0, 0))
            {
                xf86DrvMsg(scrnIndex, X_INFO,
                           "Largest offscreen area available: %d x %d\n",
                           width, height);
            }
        }
        else
        {
            xf86DrvMsg(scrnIndex, X_ERROR,
                       "Hardware cursor initialization failed\n");
            xf86DrvMsg(scrnIndex, X_INFO, "Using software cursor\n");
        }
    }
    else
    {
        pXGI->cursorStart = 0;
        xf86DrvMsg(scrnIndex, X_INFO, "Using software cursor\n");
    }

    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After pXGI->isHWCursor()\n");

    /*
     * Initialise the default colourmap
     * If using the vgahw module,
     * vgaHandleColormaps would usually be called here.
     */
    if (!miCreateDefColormap(pScreen)) return FALSE;
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After miCreateDefColormap()\n");

    if (!xf86HandleColormaps(pScreen, 256, pXGI->isDac8bits ? 8 : 6,
                             XGILoadPalette,
                             XGISetOverscan,
                             CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR))
    {
        return FALSE;
    }
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After xf86HandleColormaps()\n");

    /* shadow frame buffer */
    if (pXGI->isShadowFB)
    {
        if (pXGI->rotate)
        {
            if (!pXGI->PointerMoved)
            {
                pXGI->PointerMoved = pScrn->PointerMoved;
                pScrn->PointerMoved = XGIPointerMoved;
            }
            switch (pScrn->bitsPerPixel)
            {
                case 8:    pXGI->RefreshArea = XGIRefreshArea8; break;
                case 16:   pXGI->RefreshArea = XGIRefreshArea16; break;
                case 24:   pXGI->RefreshArea = XGIRefreshArea24; break;
                case 32:   pXGI->RefreshArea = XGIRefreshArea32; break;
            }
        }
        else
        {
            pXGI->RefreshArea = XGIRefreshArea;
        }
        shadowInit(pScreen, XGIShadowUpdate, 0);
    }
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After pXGI->isShadowFB\n");

    /* DPMS setup */
#ifdef DPMSExtension
	ErrorF("XGI-Define DPMSExtension...\n");

        xf86DPMSInit(pScreen, (DPMSSetProcPtr)XGIDPMSSet, 0);
		ErrorF("XGI-xf86DPMSInit(pScreen, (DPMSSetProcPtr)XGIDPMSSet, 0)...\n");
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After DPMSExtension\n");
#endif

    /* XV extension */
#ifdef XvExtension
    XGIInitVideo(pScreen); 

    PDEBUG(ErrorF("*-*Jong-XGIInitVideo-End\n"));
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After XvExtension\n");
#endif
#ifdef XGI_XVMC
    PDEBUG(ErrorF("*-*Jong-XGIInitMC-Begin\n"));
    XGIInitMC(pScreen);
    PDEBUG(ErrorF("*-*Jong-XGIInitMC-End\n"));
    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After XGI_XVMC\n");
#endif

    /* Provide SaveScreen */
    pScreen->SaveScreen  = XGISaveScreen;

    /* Wrap CloseScreen */
    pXGI->CloseScreen    = pScreen->CloseScreen;
    pScreen->CloseScreen = XGICloseScreen;

    PDEBUG(ErrorF("*-*Jong-After-XGIInitMC-1\n"));
    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1)
    {
        xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    PDEBUG(ErrorF("Jong-After-XGIInitMC-2\n"));
    pXGI->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = XGIBlockHandler;

    PDEBUG(ErrorF("Jong-After-XGIInitMC-3\n"));
#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

    PDEBUG(ErrorF("Jong-After-XGIInitMC-4\n"));
#ifdef OPENGL_HW_ACCEL
    if (!OHAScreenInit(pScreen))
    {
        ErrorF("OHAScreenInit failed.\n");
        goto fail;
    }
    PDEBUG(ErrorF("Jong-After-XGIInitMC-5\n"));
#endif
    PDEBUG(ErrorF("Jong-After-XGIInitMC-5-1\n"));

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    PDEBUG(ErrorF("Jong-After-XGIInitMC-6\n"));

	/* Jong 07/1/42006; write OK! */
	/*----------------------------*/
	/*
	for(i=0; i< 256; i++)
		*(((CARD8*)(pXGI->fbBase))+i)=0x06; 

	ErrorF("Jong-07142006-dump memory of FB-(06)\n");
	XGIDumpMemory(pXGI->fbBase,  256);
	*/
	/*----------------------------*/

/* Jong 09/21/2006; support dual view */
#ifdef XGI_DUMP_DUALVIEW
	ErrorF("Jong-Debug-DualView-XGIScreenInit-End-%d--\n", pXGI->FirstView);
	XGIDumpRegisterValue(g_pScreen);
#endif

    return TRUE;
fail:
    if (!pXGI->noMMIO)
    {
        XGIDisableMMIO(pScrn);
    }
    PDEBUG(ErrorF("Jong-After-XGIInitMC-7\n"));
    XGIUnmapMem(pScrn);
    PDEBUG(ErrorF("Jong-After-XGIInitMC-8\n"));
    /* Free the video bios (if applicable) */
    if (pXGI->biosBase)
    {
        xfree(pXGI->biosBase);
        pXGI->biosBase = NULL;
    }

    PDEBUG(ErrorF("Jong-After-XGIInitMC-9\n"));
    if (pXGI->pVbe)
    {
        vbeFree(pXGI->pVbe);
        pXGI->pVbe = NULL;
    }

    PDEBUG(ErrorF("Jong-After-XGIInitMC-10\n"));
    /* Free int10 info */
    if (pXGI->pInt10)
    {
        xf86FreeInt10(pXGI->pInt10);
        pXGI->pInt10 = NULL;
    }
    PDEBUG(ErrorF("Jong-After-XGIInitMC-11\n"));
    vgaHWFreeHWRec(pScrn);

    PDEBUG(ErrorF("Jong-After-XGIInitMC-12\n"));
    XGIFreeRec(pScrn);

    PDEBUG(ErrorF("Jong-After-XGIInitMC-13\n"));
    return FALSE;
}

/* Usually mandatory */
/* When a SwitchMode event is received, XGISwitchMode() is called (when it exists).
 * Initialises the new mode for the screen identified by index.
 * The viewport may need to be adjusted also.
 */
Bool XGISwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    XGIPtr      pXGI = XGIPTR(pScrn);
    Bool        retVal = FALSE;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /*
     * Initialise the first video mode.
     * The ScrnInfoRec's vtSema field should be set to TRUE
     * just prior to changing the video hardware's state.
     */
    pXGI->isNeedCleanBuf = FALSE;

    if (pXGI->isFBDev)
    {
        if (!fbdevHWModeInit(xf86Screens[scrnIndex], mode))
            retVal = FALSE;
        else
            retVal = TRUE;
    }
    else
    {
        if (!XGIModeInit(xf86Screens[scrnIndex], mode))
            retVal = FALSE;
        else
            retVal = TRUE;
    }
/*
    if (!XGIPutScreenInfo(pScrn))
    {
        retVal = FALSE;
        xf86DrvMsg(scrnIndex, X_ERROR, "XGIPutScreenInfo failed\n");
    }
    else
    {
        retVal = TRUE;
        xf86DrvMsg(scrnIndex, X_INFO, "XGIPutScreenInfo Successfully\n");
    }
*/

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return retVal;
}

/*
 * Adjust viewport into virtual desktop such that (0,0) in
 * viewport space is (x,y) in virtual space.
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* When a Change Viewport event is received, ChipAdjustFrame() is called (when it exists).
 * Changes the viewport for the screen identified by index.
 * It should be noted that many chipsets impose restrictions on where
 * the viewport may be placed in the virtual resolution, either for alignment reasons,
 * or to prevent the start of the viewport from being positioned within a pixel (as
 * can happen in a 24bpp mode). After calculating the value the chipset's panning
 * registers need to be set to for non-DGA modes, this function should recalculate
 * the ScrnInfoRec's frameX0, frameY0, frameX1 and frameY1 fields to correspond to
 * that value. If this is not done, switching to another mode might cause the position
 * of a hardware cursor to change.
 */
void XGIAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    XGIPtr      pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (pXGI->isFBDev)
    {
	    fbdevHWAdjustFrame(scrnIndex, x, y, flags);
	    return;
    }

    switch(pXGI->chipset)
    {
    case XG47:
        XG47AdjustFrame(scrnIndex, x, y, flags);
        break;
    default:
        break;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

/*
 * This is called when VT switching back to the X server. Its job is
 * to reinitialise the video mode.
 */
static Bool XGIEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    XGIPtr      pXGI = XGIPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!pXGI->noMMIO)
        XGIEnableMMIO(pScrn);

#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

    pXGI->isNeedCleanBuf = TRUE;

    if (pXGI->isFBDev)
    {
        if (!fbdevHWEnterVT(scrnIndex,flags)) return FALSE;
    }
    else
    {
        /* Should we re-save the text mode on each VT enter? */
        if (!XGIModeInit(pScrn, pScrn->currentMode))
            return FALSE;
    }

#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

    if (!pXGI->noAccel)
    {
        if(pXGI->chipset == XG47)
        {
            int retIoctl;
            XGIStateInfoRec stateInfo;

            stateInfo._fromState    = 0;    /* console */
            stateInfo._toState      = 1;    /* graphic */

           /* reset KD cmdlist status */
            retIoctl = ioctl(pXGI->fd, XGI_IOCTL_STATE_CHANGE, &stateInfo);
		    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIEnterVT()-1\n");
            if (retIoctl == -1)
            {
                return FALSE;
            }

            /* reset 2D cmdlist status */
            xg47_Reset(pXGI->cmdList);

            /* reset 3D cmdlist status */
            /* notify 3D to start run */
        }

        XGIEngineInit(pScrn);
    }

/*    if (pXGI->InitializeAccelerator)
        pXGI->InitializeAccelerator(pScrn);*/

    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 *
 * We may wish to remap video/MMIO memory too.
 */

/* Mandatory */
static void XGILeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    XGIPtr      pXGI = XGIPTR(pScrn);
    vgaHWPtr    pVgaHW = VGAHWPTR(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (pXGI == NULL) {
#if DBG_FLOW
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif
	return;
    }

	/* Jong 11/09/2006; only call once */
	if((g_DualViewMode == 1) & (pXGI->FirstView == 0)) 
	{
		ErrorF("--Leave XGILeaveVT()---\n");
		return;
	}

    /*
    XGISavePalette(pScrn, save);
    */
#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

  /* Jong 11/09/2006; support dual view */
  /* Do only one time; otherwise will cause system hang */
  if((g_DualViewMode == 0 ) || (pXGI->FirstView == 1)) 
  {
    if (!pXGI->noAccel)
    {
        if(pXGI->chipset == XG47)
        {
            int retIoctl;
            XGIStateInfoRec stateInfo;

            stateInfo._fromState    = 1;    /* console */
            stateInfo._toState      = 0;    /* graphic */

            /* notify 3D to stop */

            /* notify 2D to stop */

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() - Before XG47WaitForIdle()\n", __FUNCTION__);
#endif

            /* flush cmdlist */
            XG47WaitForIdle(pXGI);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() - After XG47WaitForIdle()\n", __FUNCTION__);
#endif

            /* disable GE end out */
            XG47DisableGE(pXGI);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() - After XG47DisableGE()\n", __FUNCTION__);
#endif

            /* no use, just for kernel test */
            /* reset KD cmdlist status */
            retIoctl = ioctl(pXGI->fd, XGI_IOCTL_STATE_CHANGE, &stateInfo);
		    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGILeaveVT()-1\n");
            if (retIoctl == -1)
            {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Notify kernel to change state (G==>C)\n");
                return;
            }
        }
    }
  }
  
    pXGI->isNeedCleanBuf = TRUE;

    if (pXGI->isFBDev)
    {
        fbdevHWLeaveVT(scrnIndex,flags);
    }
    else
    {
#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() - Before XGIRestore()\n", __FUNCTION__);
#endif
        XGIRestore(pScrn);
#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() - After XGIRestore()\n", __FUNCTION__);
#endif
    }

#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

    if (!pXGI->noMMIO)
        XGIDisableMMIO(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool XGICloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    vgaHWPtr    pVgaHW = VGAHWPTR(pScrn);
    XGIPtr      pXGI = XGIPTR(pScrn);
    Bool        result;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (pXGI->pCursorInfo)
    {
        if (pXGI->chipset == XG47)
        {
            XG47HWCursorCleanup(pScreen);
        }

        xf86DestroyCursorInfoRec(pXGI->pCursorInfo);
        pXGI->pCursorInfo = NULL;
    }

	/* Jong 09/28/2006; support dual view */
	/* Do only one time; otherwise will cause system hang */
	/* if(pScreen->myNum == 0) */ /* cause system hang */
	if((g_DualViewMode == 0 ) || (pScreen->myNum == 1)) 
		if (pXGI->pXaaInfo)
		{
			if (pXGI->chipset == XG47)
			{
				XG47AccelExit(pScreen);
			}

			XAADestroyInfoRec(pXGI->pXaaInfo);

			pXGI->pXaaInfo = NULL;
		}

    /* Restore the saved video state and unmap the memory regions. */
    if (pScrn->vtSema)
    {
        XGIRestore(pScrn);
    }

    vgaHWLock(pVgaHW);
    if (!pXGI->noMMIO)
    {
        XGIDisableMMIO(pScrn);
    }

    XGIUnmapMem(pScrn);

    if (pXGI->pShadow)
    {
        xfree(pXGI->pShadow);
        pXGI->pShadow = NULL;
    }

    if (pXGI->pDgaModes)
    {
        xfree(pXGI->pDgaModes);
        pXGI->pDgaModes = NULL;
    }

    if (pXGI->pAdaptor)
    {
        /* xfree(pXGI->pAdaptor->pPortPrivates[0].ptr); */
        xf86XVFreeVideoAdaptorRec(pXGI->pAdaptor);
        pXGI->pAdaptor = NULL;
    }
    /* The ScrnInfoRec's vtSema field should be set to FALSE
     * once the video HW state has been restored.
     */
    pScrn->vtSema = FALSE;

    if (pXGI->BlockHandler)
        pScreen->BlockHandler = pXGI->BlockHandler;

    if (pXGI->pVbe)
    {
        vbeFree(pXGI->pVbe);
        pXGI->pVbe = NULL;
    }

    if (pXGI->pInt10)
    {
        xf86FreeInt10(pXGI->pInt10);
        pXGI->pInt10 = NULL;
    }

#ifdef OPENGL_HW_ACCEL
    if (!OHACleanup(pScreen))
    {
         xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Close OHA failed !\n");
    }
#endif

    if (close(pXGI->fd) < 0)
    {
         xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Close /dev/xgi failed !\n");
    }

    /* Before freeing the per-screen driver data the saved CloseScreen
     * value should be restored to pScreen->CloseScreen, and that function
     * should be called after freeing the data.
     */
    pScreen->CloseScreen = pXGI->CloseScreen;

    result = (*pScreen->CloseScreen)(scrnIndex, pScreen);

#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return result;
}

static Bool XGISaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    Bool        unblank;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    unblank = xf86IsUnblank(mode);

    if (unblank)
        SetTimeSinceLastInputEvent();

    if ((pScrn != NULL) && pScrn->vtSema)
    {
        /*
        if (unblank)
            XGIUnblank(pScrn);
        else
            XGIBlank(pScrn);
        */
        vgaHWBlankScreen(pScrn, unblank);
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return TRUE;
}

/* Optional */
/* Free up any per-generation data structures */
/* Free any driver-allocated data that may have been allocated up to
 * and including an unsuccessful ChipScreenInit() call. This would predominantly
 * be data allocated by ChipPreInit() that persists across server generations.
 * It would include the driverPrivate, and any ``privates'' entries that modules
 * may have allocated.
 */
static void XGIFreeScreen(int scrnIndex, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    XGITRACE(("XGIFreeScreen\n"));
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
    {
        vgaHWFreeHWRec(pScrn);
    }
    XGIFreeRec(pScrn);

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

}

/* Optional */
/* Checks if a mode is suitable for the selected chipset. */
/* This function may also modify the effective timings and clock of the passed mode.
 * These have been stored in the mode's Crtc* and SynthClock elements, and have
 * already been adjusted for interlacing, doublescanning, multiscanning and clock
 * multipliers and dividers. The function should not modify any other mode field,
 * unless it wants to modify the mode timings reported to the user by xf86PrintModes().

 * The function is called once for every mode in the XF86Config Monitor section
 * assigned to the screen, with flags set to MODECHECK_INITIAL. It is subsequently
 * called for every mode in the XF86Config Display subsection assigned to the screen,
 * with flags set to MODECHECK_FINAL. In the second case, the mode will have successfully
 * passed all other tests. In addition, the ScrnInfoRec's virtualX, virtualY and
 * displayWidth fields will have been set as if the mode to be validated were to
 * be the last mode accepted.

 * In effect, calls with MODECHECK_INITIAL are intended for checks that do not
 * depend on any mode other than the one being validated, while calls with
 * MODECHECK_FINAL are intended for checks that may involve more than one mode.
 */
static int XGIValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    ScrnInfoPtr     pScrn = xf86Screens[scrnIndex];
    XGIPtr          pXGI = XGIPTR(pScrn);
    int             ret = 0;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    if (!pXGI->pInt10)
    {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "have not loade int10 module successfully!\n");
        return (MODE_BAD);
    }

    switch(pXGI->chipset)
    {
    case XG47:
        ret = XG47ValidMode(pScrn, mode);
        break;
    default:
        break;
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return ret;
}

/*
 * Initialise a new mode.  This is currently still using the old "initialise
 * struct, restore/write struct to HW" model.  That could be changed.
 * This could be called from the ChipScreenInit(), ChipSwitchMode() and
 * ChipEnterVT() functions.Programs the hardware for the given video mode.
 */
static Bool XGIModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    XGIPtr      pXGI = XGIPTR(pScrn);
    Bool        ret = FALSE;

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    switch(pXGI->chipset)
    {
    case XG47:
        ret = XG47ModeInit(pScrn, mode);
        break;
    default:
        break;
    }

    pXGI->currentLayout.mode = mode;

#ifdef XGI_DUMP
    XGIDumpRegisterValue(pScrn);
#endif

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    return ret;
}
