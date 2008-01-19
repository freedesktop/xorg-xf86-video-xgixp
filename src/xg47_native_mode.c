/*
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.
 * (c) Copyright IBM Corporation 2007
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xgi.h"

#ifdef NATIVE_MODE_SETTING
#include "xgi_driver.h"
#include "xgi_regs.h"
#include "xgi_bios.h"
#include "xgi_mode.h"
#include "xg47_mode.h"

#if 0
#define FREQ_FROM_NMK(_n, _m, _k) \
    ((1431818 * (_n + 8) / ((_m + 1) << (_k))) / 100)

/**
 * \param clk   Desired pixel clock, in kHz.
 */
static Bool FindNMK(unsigned clk, unsigned *n, unsigned *m, unsigned *k)
{
    unsigned best_diff = clk;
    unsigned i;
    unsigned j;
    unsigned l;
    const unsigned max_i = 255;
    const unsigned max_j = 63;
    const unsigned max_l = 2;


    for (l = max_l; l > 0; l--) {
        for (i = 0; i <= max_i; i++) {
            for (j = 1; j <= max_j; j++) {
                const unsigned curr_freq = FREQ_FROM_NMK(i, j, l);
                const unsigned diff = (clk > curr_freq) 
                    ? (clk - curr_freq) : (curr_freq - clk);

                if (diff < best_diff) {
                    best_diff = diff;

                    *n = i;
                    *m = j;
                    *k = l;
                }
            }
        }
    }


    /* VESA says that the actual clock must be within 0.5% of target clock.
     */
    return (best_diff / (clk / 1000)) <= 5;
}


static void SetVCLK(XGIPtr pXGI, DisplayModePtr disp_mode)
{
    unsigned n = 0;
    unsigned m = 0;
    unsigned k = 0;


    /* Select primary VCLK
     */
    OUT3C5B(0x29, IN3C5B(0x29) & ~0x30);


    FindNMK(disp_mode->Clock, &n, &m, &k);
    xf86DrvMsg(pXGI->pScrn->scrnIndex, X_INFO, "%d -> 0x%x 0x%x (%d)\n",
	       disp_mode->Clock, n, ((k << 6) & 0xc0) | (m & 0x3f),
               FREQ_FROM_NMK(n, m, k));

    OUT3C5B(0x28, (IN3C5B(0x28) & ~0x07) | 0x04);
    OUT3C5B(0x18, n);
    OUT3C5B(0x19, ((k << 6) & 0xc0) | (m & 0x3f));
}
#else
struct vclk_tab {
	unsigned freq;
	uint8_t  sr18;
	uint8_t  sr19;
	uint8_t  p;
};

static const struct vclk_tab clk[] = {
	{ 25175, 0xCB, 0x9D, 0x00 },
	{ 28322, 0xAE, 0x96, 0x00 },
	{ 31500, 0x24, 0x84, 0x00 },
	{ 32668, 0x8A, 0x8F, 0x00 },
	{ 36000, 0xAE, 0xC8, 0x22 },
	{ 40000, 0x73, 0x8A, 0x66 },
	{ 43163, 0xC5, 0x90, 0x66 },
	{ 44900, 0x82, 0x8A, 0x66 },
	{ 49500, 0x4B, 0x85, 0x66 },
	{ 50350, 0xCB, 0x8E, 0x44 },
	{ 51000, 0x31, 0x83, 0x44 },
	{ 52406, 0xC5, 0x8D, 0x44 },
	{ 56250, 0x66, 0x86, 0x44 },
	{ 65000, 0x65, 0x85, 0x44 },
	{ 68179, 0xF0, 0x8C, 0x44 },
	{ 71000, 0x6F, 0x85, 0x33 },
	{ 74160, 0x89, 0x86, 0x33 },
	{ 75000, 0xDE, 0x8A, 0x33 },
	{ 78750, 0x24, 0x81, 0x22 },
	{ 79500, 0x67, 0x84, 0x44 },
	{ 81750, 0x81, 0x85, 0x22 },
	{ 83462, 0xCA, 0x88, 0x33 },
	{ 83950, 0xCB, 0x88, 0x22 },
	{ 90000, 0xA8, 0x86, 0x33 },
	{ 94500, 0x7C, 0x84, 0x33 },
	{ 96307, 0x8C, 0x4A, 0x66 },
	{ 99000, 0x4B, 0x82, 0x33 },
	{ 101250, 0xBE, 0x86, 0x33 },
	{ 106472, 0x6F, 0x83, 0x33 },
	{ 108000, 0xAD, 0x85, 0x22 },
	{ 112500, 0xD4, 0x86, 0x22 },
	{ 113309, 0x57, 0x82, 0x22 },
	{ 119651, 0xE2, 0x86, 0x22 },
	{ 122500, 0xA3, 0x84, 0x22 },
	{ 124090, 0x60, 0x82, 0x22 },
	{ 129000, 0xAC, 0x84, 0x22 },
	{ 130295, 0xAE, 0x84, 0x22 },
	{ 133000, 0xD7, 0x4B, 0x44 },
	{ 135000, 0x7C, 0x46, 0x44 },
	{ 139054, 0xE1, 0x85, 0x22 },
	{ 146250, 0x87, 0x46, 0x44 },
	{ 148250, 0xC7, 0x84, 0x22 },
	{ 154250, 0xE5, 0x4A, 0x33 },
	{ 155750, 0xA6, 0x83, 0x22 },
	{ 157500, 0xA8, 0x83, 0x22 },
	{ 160000, 0x7E, 0x82, 0x22 },
	{ 162000, 0xDA, 0x84, 0x22 },
	{ 164000, 0xF4, 0x4A, 0x33 },
	{ 178000, 0xA6, 0x46, 0x33 },
	{ 179250, 0x8E, 0x82, 0x22 },
	{ 181000, 0xA9, 0x46, 0x22 },
	{ 189000, 0x7C, 0x44, 0x22 },
	{ 190960, 0x48, 0x42, 0x33 },
	{ 193000, 0x49, 0x42, 0x33 },
	{ 202500, 0xBE, 0x46, 0x22 },
	{ 229500, 0x38, 0x41, 0x22 },
	{ 234000, 0x5A, 0x42, 0x22 },
	{ 267000, 0x68, 0x42, 0x22 },
	{ 280637, 0xBC, 0x44, 0x22 },
	{ 297000, 0x4B, 0x41, 0x22 },
	{ 340477, 0x57, 0x41, 0x22 },
};

static void SetVCLK(XGIPtr pXGI, DisplayModePtr disp_mode)
{
	unsigned base;
	unsigned count;
	const unsigned num_vclk = sizeof(clk) / sizeof(clk[0]);


	/* Select primary VCLK
	 */
	OUT3C5B(0x29, IN3C5B(0x29) & ~0x30);


	base = 0;
	count = num_vclk;
	while (count > 1) {
		const unsigned i = base + (count / 2);
		if (clk[i].freq <= disp_mode->Clock) {
			base = i;
			count -= count / 2;
		} else {
			count = count / 2;
		}
	}

	xf86DrvMsg(pXGI->pScrn->scrnIndex, X_INFO,
		"VCLK %d -> 0x%x 0x%x 0x%x (%d)\n",
		disp_mode->Clock, clk[base].p, clk[base].sr18, clk[base].sr19,
		clk[base].freq);

	OUT3C5B(0x28, (IN3C5B(0x28) & ~0x07) | (clk[base].p & 0x07));
	OUT3C5B(0x18, clk[base].sr18);
	OUT3C5B(0x19, clk[base].sr19);
}
#endif


static void FillExtendedRegisters(XGIPtr pXGI, DisplayModePtr disp_mode)
{
    uint8_t    val_0f;
    uint8_t    v3x5_2f, v3x5_5a, v3x5_38, v3x5_2a;
    uint8_t    v3x5_25;


    /* Disable character clock divider, compressed chain 4 mode for CPU (bit
     * 4 and bit 1), and source segment address register (bit 0).
     */
    val_0f = IN3CFB(0x0F) & ~(0x48 | 0x13);

    /* Enable alternative bank and clock select registers.
     */
    val_0f |= 0x04;

    /* For modes where each pixel is at least a byte, enable compressed chain
     * 4 mode for CPU.
     */
    if (pXGI->pScrn->bitsPerPixel >= 8) {
        val_0f |= 0x12;
    }

    OUT3CFB(0x0F, val_0f);


    SetVCLK(pXGI, disp_mode);


    /* Low resolution (i.e., less than 640 horizontal) modes on LCDs need to
     * use a special clock divider mode.
     */
    if (disp_mode->HDisplay < 640) {
        OUTB(0x3DB, (INB(0x3DB) & ~0xe0) | 0x20);
    } else {
        OUTB(0x3DB, (INB(0x3DB) & ~0xe0));
    }


    /* Disable linear addressing.
     */
    OUT3X5B(0x21, IN3X5B(0x21) & ~0x20);

    /* Disable input path and command arbitration path in memory controller.
     * 
     * Why?
     */
    OUT3X5B(0x5d, IN3X5B(0x5d) & ~0x18);


    /* For an extended VGA mode blank the display.
     *
     * Why?
     *
     * 3x5.5a.5 is a readable copy of 3x5.2f.5 for software to access.
     */
    v3x5_5a = IN3X5B(0x5a) & ~0x20;
    v3x5_2f = IN3X5B(0x2f) & ~0xF0;
    OUT3X5B(0x2f, v3x5_2f | 0x20);
    OUT3X5B(0x5a, v3x5_5a | 0x20);


    /* See Pixel Bus Mode Register on page 9-11 of "Volari XP10 non-3D SPG
     * v1.0".
     */
    switch (pXGI->pScrn->bitsPerPixel) {
    case 4:
        v3x5_38 = 0x10;
        break;
    default:
    case 8:
        v3x5_38 = 0;
        break;
    case 15:
    case 16:
        v3x5_38 = 0x05;
        break;
    case 24:
    case 32:
        v3x5_38 = 0x29;
        break;
    case 30:
        v3x5_38 = 0xA8;
        break;
    }
    OUT3X5B(0x38, v3x5_38);


    /* Interface select register.  Only enable internal 32-bit path (bit 6)
     * if a video mode is selected that uses more than one byte per pixel.
     */
    v3x5_2a = IN3X5B(0x2a) & ~0x40;
    if (pXGI->pScrn->bitsPerPixel >= 8) {
        v3x5_2a |= 0x40;
    }
    OUT3X5B(0x2a, v3x5_2a);


    /* Turn off the hardware cursor while display modes are being changed.
     */
    OUT3X5B(0x50, IN3X5B(0x50) & ~0x80);


    /* PCI retry defaults to disabled.  Enable and set maximum re-try count
     * if the selected mode is a non-VGA mode with more than 256 colors.
     */
    if (pXGI->pScrn->bitsPerPixel >= 8) {
        /* Set count first, then enable retry.  In some cases the system
         * may hang-up if set both same time.
         */
        OUT3X5B(0x55, 0x1f);
        OUT3X5B(0x55, 0xff);
    } else {
        OUT3X5B(0x55, 0x00);
    }

    /* Clear alternate destination / source segment address.
     */
    OUTB(0x3d8, 0x00);
    OUTB(0x3d9, 0x00);


    /* Define refresh cycles.
     * 
     * Not exactly sure what is going on here.
     */
    OUT3CFB(0x5D, IN3CFB(0x5D) & ~0x80);


    /* Always enable "line compare bit 10" (bit 3).  The other 10th bit
     * values are only set if the mode requires 10 bits to represent the
     * values.
     */
    OUT3X5B(0x27, (IN3X5B(0x27) & 0x07) | 0x08
            | (((disp_mode->CrtcVDisplay - 1)    & 0x400) >> 6)
            | (((disp_mode->CrtcVSyncStart)      & 0x400) >> 5)
            | (((disp_mode->CrtcVBlankStart - 1) & 0x400) >> 4)
            | (((disp_mode->CrtcVTotal - 2)      & 0x400) >> 3));


    /* Horizontal parameters overflow register.
     */
    OUT3X5B(0x2B, ((((disp_mode->CrtcHTotal >> 3) - 5) & 0x100) >> 8)
            | ((((disp_mode->CrtcHBlankStart >> 3) - 1) & 0x100) >> 5));


    if (disp_mode->Flags & V_INTERLACE) {
        /* CRT interlace control register.
         */
        OUT3X5B(0x19, 0x4a);


        /* Enable interlacing and access to display memory above 256KiB.
         */
        OUT3X5B(0x1E, (IN3X5B(0x1E) & 0x3B) | 0x84);
    } else {
        OUT3X5B(0x19, 0x4a);
        OUT3X5B(0x1E, (IN3X5B(0x1E) & 0x3B) | 0x80);
    }
    

    /* Write 0 to enable I/O buffers of PCLK and P[7:0] tri-state.
     */
    v3x5_25 = IN3X5B(0x25) | 0x80;
    if (disp_mode->VDisplay <= 800) {
        v3x5_25 &= ~0x80;
    }
    OUT3X5B(0x25, v3x5_25);


    /* Enable CRTC horizontal blanking end 7-bits function.  Highest bit is
     * at 3d5.03.7
     */
    OUT3X5B(0x33, IN3X5B(0x33) | 0x08);
}


/**
 * Calculate logical line width according to color depth.
 *
 * \return
 * Width per scan line in words (2-byte).
 */
static unsigned CalLogicalWidth(ScrnInfoPtr pScrn)
{
    return (pScrn->displayWidth * (pScrn->bitsPerPixel / 8)) / 8;
}


/**
 * Calculate logical line width according to color depth and CRTC13
 *
 * Calculate logical line width according to color depth and CRTC13 (offset
 * in words). Then write back to CRTC13 for later use.
 */
static void SetLogicalWidth(XGIPtr pXGI)
{
    const unsigned width = CalLogicalWidth(pXGI->pScrn);

    /* On XP10 the upper 6 bits of the CRTC offset address (CRTC13) are
     * stored at 0x8B.
     */
    OUT3X5B(0x13, width & 0x0FF);
    OUT3X5B(0x8B, ((IN3X5B(0x8B) & ~0x3f) | ((width >> 8) & 0x3f)));
}


void SetModeCRTC1(XGIPtr pXGI, DisplayModePtr disp_mode)
{
    vgaHWRestore(pXGI->pScrn, &VGAHWPTR(pXGI->pScrn)->ModeReg,
                 VGA_SR_MODE | VGA_SR_FONTS | VGA_SR_CMAP);
    FillExtendedRegisters(pXGI, disp_mode);
    SetLogicalWidth(pXGI);

    /* Enable PCI linear memory access
     */
    OUT3X5B(0x21, IN3X5B(0x21) | 0x20);
}


static void SetColorDAC(XGIPtr pXGI, unsigned color_depth)
{
    unsigned mode;

    switch (color_depth) {
    case 16:
        /* Use XGA color mode, 16-bit direct.
         */
        mode = 0x30;
        break;

    case 32:
    case 30:
        /* Use true color mode, 24-bit direct.
         */
        mode = 0xD0;
        break;

    default:
        /* Pseudo-color mode.
         */
        mode = 0x00;
        break;
    }

    INB(0x3c6);
    INB(0x3c6);
    INB(0x3c6);
    INB(0x3c6);
    OUTB(0x3c6, mode);
}


Bool XG47_NativeModeInit(ScrnInfoPtr pScrn, DisplayModePtr disp_mode)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
    vgaHWPtr pVgaHW = VGAHWPTR(pScrn);


    vgaHWUnlock(pVgaHW);

    /* Initialise the ModeReg values */
    if (!vgaHWInit(pScrn, disp_mode))
        return FALSE;

    pScrn->vtSema = TRUE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "monitor sense (3c5.25): 0x%02x\n",
               IN3C5B(0x25));
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "monitor sense (3cf.37): 0x%02x\n",
               IN3CFB(0x37));

    /* DAC power off
     */
    OUT3C5B(0x24, IN3C5B(0x24) & ~0x01);
    OUT3CFB(0x23, IN3CFB(0x23) | 0x03);

    OUT3X5B(0x39, 0x85);

    SetModeCRTC1(pXGI, disp_mode);
    SetColorDAC(pXGI, pScrn->bitsPerPixel);

    /* 3CF.33.5- 1: enable CRT display
     */
    OUT3CFB(0x33, IN3CFB(0x33) | 0x20);

    /* 3C5.1.5 - ScreenOff, 0: selects normal screen operation
     */
    OUT3C5B(0x01, IN3C5B(0x01) & ~0x20);

    /* DAC power on
     */
    OUT3C5B(0x24, IN3C5B(0x24) | 0x01);
    OUT3CFB(0x23, IN3CFB(0x23) & ~0x03);

    /* setup 1st timing
     */
    OUT3CFB(0x2c, IN3CFB(0x2c) & ~0x40);

    XG47SetCRTCViewStride(pScrn);
    return TRUE;
}


void XG47_NativeModeSave(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg)
{
    XGIPtr pXGI = XGIPTR(pScrn);


    pXGIReg->regs3CF[0x0f] = IN3CFB(0x0f);
    pXGIReg->regs3CF[0x5d] = IN3CFB(0x5d);

    pXGIReg->regs3C5[0x18] = IN3C5B(0x18);
    pXGIReg->regs3C5[0x19] = IN3C5B(0x19);
    pXGIReg->regs3C5[0x28] = IN3C5B(0x28);

    INB(0x3c6);
    INB(0x3c6);
    INB(0x3c6);
    INB(0x3c6);
    pXGIReg->regs3C6 = INB(0x3c6);

    pXGIReg->regs3D8 = INB(0x3d8);
    pXGIReg->regs3D9 = INB(0x3d9);
    pXGIReg->regs3DB = INB(0x3db);

    pXGIReg->regs3X5[0x13] = IN3X5B(0x13);
    pXGIReg->regs3X5[0x19] = IN3X5B(0x19);
    pXGIReg->regs3X5[0x1e] = IN3X5B(0x1e);
    pXGIReg->regs3X5[0x21] = IN3X5B(0x21);
    pXGIReg->regs3X5[0x25] = IN3X5B(0x25);
    pXGIReg->regs3X5[0x27] = IN3X5B(0x27);
    pXGIReg->regs3X5[0x2a] = IN3X5B(0x2a);
    pXGIReg->regs3X5[0x2b] = IN3X5B(0x2b);
    pXGIReg->regs3X5[0x2c] = IN3X5B(0x2c);
    pXGIReg->regs3X5[0x2f] = IN3X5B(0x2f);
    pXGIReg->regs3X5[0x33] = IN3X5B(0x33);
    pXGIReg->regs3X5[0x38] = IN3X5B(0x38);
    pXGIReg->regs3X5[0x50] = IN3X5B(0x50);
    pXGIReg->regs3X5[0x55] = IN3X5B(0x55);
    pXGIReg->regs3X5[0x5a] = IN3X5B(0x5a);
    pXGIReg->regs3X5[0x5d] = IN3X5B(0x5d);
    pXGIReg->regs3X5[0x8b] = IN3X5B(0x8b);
}


void XG47_NativeModeRestore(ScrnInfoPtr pScrn, XGIRegPtr pXGIReg)
{
    vgaHWPtr    pVgaHW = VGAHWPTR(pScrn);
    XGIPtr      pXGI = XGIPTR(pScrn);
    vgaRegPtr   pVgaReg = &pVgaHW->SavedReg;


    vgaHWUnlock(pVgaHW);
    vgaHWRestore(pScrn, pVgaReg, VGA_SR_MODE | VGA_SR_CMAP |
                                 (IsPrimaryCard ? VGA_SR_FONTS : 0));
//    vgaHWLock(pVgaHW);

    OUT3CFB(0x0F, pXGIReg->regs3CF[0x0f]);

    OUT3C5B(0x29, pXGIReg->regs3C5[0x29]);
    OUT3C5B(0x28, pXGIReg->regs3C5[0x28]);
    OUT3C5B(0x18, pXGIReg->regs3C5[0x18]);
    OUT3C5B(0x19, pXGIReg->regs3C5[0x19]);

    OUTB(0x3DB, pXGIReg->regs3DB);

    OUT3X5B(0x21, pXGIReg->regs3X5[0x21]);

    OUT3X5B(0x5d, pXGIReg->regs3X5[0x5d]);

    OUT3X5B(0x2f, pXGIReg->regs3X5[0x2f]);
    OUT3X5B(0x5a, pXGIReg->regs3X5[0x5a]);

    OUT3X5B(0x38, pXGIReg->regs3X5[0x38]);

    OUT3X5B(0x2a, pXGIReg->regs3X5[0x2a]);


    /* Turn off the hardware cursor while display modes are being changed.
     */
    OUT3X5B(0x50, IN3X5B(0x50) & ~0x80);

    /* Set count first, then enable retry.  In some cases the system
     * may hang-up if set both same time.
     */
    OUT3X5B(0x55, pXGIReg->regs3X5[0x55] & 0x1f);
    OUT3X5B(0x55, pXGIReg->regs3X5[0x55]);

    OUTB(0x3d8, pXGIReg->regs3D8);
    OUTB(0x3d9, pXGIReg->regs3D9);

    OUT3CFB(0x5D, pXGIReg->regs3CF[0x5D]);

    OUT3X5B(0x27, pXGIReg->regs3X5[0x27]);

    OUT3X5B(0x2B, pXGIReg->regs3X5[0x2B]);

    OUT3X5B(0x19, pXGIReg->regs3X5[0x19]);
    OUT3X5B(0x1E, pXGIReg->regs3X5[0x1E]);

    OUT3X5B(0x25, pXGIReg->regs3X5[0x25]);

    OUT3X5B(0x33, pXGIReg->regs3X5[0x33]);
    OUT3X5B(0x50, pXGIReg->regs3X5[0x50]);

    INB(0x3c6);
    INB(0x3c6);
    INB(0x3c6);
    INB(0x3c6);
    OUTB(0x3c6, pXGIReg->regs3C6);

    /* 3CF.33.5- 1: enable CRT display
     */
    OUT3CFB(0x33, IN3CFB(0x33) | 0x20);

    /* 3C5.1.5 - ScreenOff, 0: selects normal screen operation
     */
    OUT3C5B(0x01, IN3C5B(0x01) & ~0x20);

    /* DAC power on
     */
    OUT3C5B(0x24, IN3C5B(0x24) | 0x01);
    OUT3CFB(0x23, IN3CFB(0x23) & ~0x03);

    /* setup 1st timing
     */
    OUT3CFB(0x2c, pXGIReg->regs3CF[0x2c]);

    OUT3X5B(0x13, pXGIReg->regs3X5[0x13]);
    OUT3X5B(0x8B, pXGIReg->regs3X5[0x8B]);
}

#endif /* NATIVE_MODE_SETTING */
