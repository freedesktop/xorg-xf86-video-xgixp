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
#include "xgi_version.h"
#include "xgi_regs.h"

void XG47EnableMMIO(ScrnInfoPtr pScrn)
{
    XGIPtr      pXGI = XGIPTR(pScrn);
    CARD8       protect = 0, temp = 0, temp1 = 0;

    /* Unprotect registers */
    outb(0x3C4, 0x11);
    protect = inb(0x3C5);
    outb(0x3C5, 0x92);

    /* Go to new mode */
    outb(0x3C4, 0xB);
    temp1 = inb(0x3C5);

    /*
     * in order to enable write configuration port (3C5.0CH, 3C5.0FH, 3X5.28H,
     * 3X5.29H, 3X5.2AH and bit [6:4] of 3C5.0EH), bit7 of control Register
     * (3C5.0EH) must be set 1 in the New Mode firstly. Bit1 of (3C5.0EH)
     * should be inverted when performing write.
     */
    outb(0x3C4, 0xE);
    temp = inb(0x3C5);
    if (temp & 0x2)
    {
        outb(0x3C5, ((temp | 0x80) & 0xFC));
    }
    else
    {
        outb(0x3C5, ((temp | 0x80) | 0x2));
    }

    /*
     * 3D4/3D5	3A
     * <5>: BOTH_IO
     *      1: Enable both pure-IO and MMIO
     *      0: No MMIO
     */
    outb(0x3D4, 0x3A);
    outb(0x3D5, inb(0x3D5) | 0x20);

    /* Enable MMIO */
    outb(0x3D4, 0x39);
    outb(0x3D5, inb(0x3D5) | 0x01);

    /* Go to old mode */
    OUTB(0x3C4, 0xB);
    OUTB(0x3C5, temp1);

    /* Protect registers */
    OUTB(0x3C4, 0x11);
    OUTB(0x3C5, protect);
}

void XG47DisableMMIO(ScrnInfoPtr pScrn)
{
    CARD8   temp = 0, protect = 0;
    XGIPtr  pXGI = XGIPTR(pScrn);

    if (pXGI->IOBase == 0) {
	return;
    }

    /* Protect registers */
    OUTB(0x3C4, 0x11);
    protect = INB(0x3C5);
    OUTB(0x3C5, 0x92);

    /* Disable MMIO access */
    OUTB(0x3D4, 0x39);
    OUTB(0x3D5, INB(0x3D5) & 0xFE);

    /* Protect registers */
    outb(pXGI->PIOBase + 0x3C4, 0x11);
    outb(pXGI->PIOBase + 0x3C5, protect);
}

void XG47AdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr     pScrn = xf86Screens[scrnIndex];
    XGIPtr          pXGI = XGIPTR(pScrn);

    unsigned long   base = y * pScrn->displayWidth + x;

#if DBG_FLOW
    ErrorF("(II) XGI (0) ++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    switch (pScrn->bitsPerPixel)
    {
    case 8:
        if (pScrn->progClock)
        {
            base = (base & 0xFFFFFFF8) >> 2;
        }
        else
        {
            base = (base & 0xFFFFFFF8) >> 3;
        }
        break;
    case 16:
        base >>= 1;
        break;
    case 24:
        base = (((base + 1) & ~0x03) * 3) >> 2;
        break;
    case 32:
        break;
    }

    /* CRT bits 0-15 */
    OUTW(0x3D4, (base & 0x00FF00) | 0x0C);
    OUTW(0x3D4, ((base & 0x0000FF) << 8) | 0x0D);

    /* CRT bits 16-23 */
    OUTW(0x3D4, ((base & 0xFF0000) >> 8) | 0x8C);

    /* CRT bits 24-26 */
    OUTW(0x3D4, ((base & 0xFF000000) >> 16) | 0x8D);

#if DBG_FLOW
    ErrorF("(II) XGI (0) -- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

    /*
    unsigned long   startAddr = (y * pScrn->displayWidth + x)
                             * (pScrn->bitsPerPixel >> 3);
    XG47SetCRTCViewBaseAddr(pScrn, startAddr);

    startAddr >>= 2;
    OUT3X5B(0x0d, (CARD8)startAddr);
    OUT3X5B(0x0c, (CARD8)(startAddr >> 0x08));
    OUT3X5B(0x8C, (CARD8)(startAddr >> 0x10));
    OUT3X5B(0x8D, (CARD8)(startAddr >> 0x18));
    */
}

/* Jong 07/24/2006; implement DPMS */
void XG47DPMSSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
	ErrorF("XGI-XG47DPMSSet()...\n");

	/* Get DPMS Capability */
    pXGI->pInt10->ax = 0x4F10;
    pXGI->pInt10->bx = 0x0000;
    pXGI->pInt10->cx = 0x0000;
    pXGI->pInt10->num = 0x10;
    xf86ExecX86int10(pXGI->pInt10);

	/* don't support DPMS power saving */
	if(((pXGI->pInt10->bx) & 0xFF00) == 0)
	{
		ErrorF("XGI Don't support DPMS ..... \n");
		return;
	}

    pXGI->pInt10->ax = 0x4F10;
    pXGI->pInt10->bx = 0x0001;
    pXGI->pInt10->cx = 0x0000;
    pXGI->pInt10->num = 0x10;
    switch (PowerManagementMode)
    {
		case DPMSModeOn:
			/* Screen: On, HSync: On, VSync: On */
			    pXGI->pInt10->bx |= 0x0000;
				ErrorF("XGI-XG47DPMSSet()-On...\n");
			break;

		case DPMSModeStandby:
			/* Screen: Off, HSync: Off, VSync: On */
			    pXGI->pInt10->bx |= 0x0100;
				ErrorF("XGI-XG47DPMSSet()-Standby...\n");
			break;

		case DPMSModeSuspend:
			/* Screen: Off, HSync: On, VSync: Off */
			    pXGI->pInt10->bx |= 0x0200;
				ErrorF("XGI-XG47DPMSSet()-Suspend...\n");
			break;

		case DPMSModeOff:
			/* Screen: Off, HSync: Off, VSync: Off */
			    pXGI->pInt10->bx |= 0x0400;
				ErrorF("XGI-XG47DPMSSet()-Off...\n");
			break;

		default:
			break;
    }

    xf86ExecX86int10(pXGI->pInt10);
}


/**
 * Examine hardware to determine the size of installed memory.
 */
void XG47GetFramebufferSize(XGIPtr pXGI)
{
    /* See documentation on page 9-8 of "Volari XP10 non-3D SPG v1.0.pdf"
     */
    const uint8_t biascntl = IN3X5B(0x60);
    static const unsigned s[8] = {
	256, 128, 64, 32, 16, 0, 0, 0
    };


    pXGI->biosFbSize = s[biascntl & 0x07] * 0x100000;
    pXGI->freeFbSize = pXGI->biosFbSize;
}
