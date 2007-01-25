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

#ifndef _XGI_REGS_H_
#define _XGI_REGS_H_

#ifdef XFree86Module
#include "xf86_ansic.h"
#endif
/* inb/outb ..., and this must be included after "xf86_ansic.h*/
#include "compiler.h"

static __inline__ CARD8 xinb(XGIPtr pXGI, CARD32 index)
{
    CARD8 reg;
    if(pXGI->noMMIO)
    {
        reg = inb(pXGI->PIOBase + index);
    }
    else
    {
        reg = MMIO_IN8(pXGI->IOBase, index);
    }
    return reg;
}

static __inline__ CARD16 xinw(XGIPtr pXGI, CARD32 index)
{
    CARD16 reg;
    if(pXGI->noMMIO)
    {
        reg = inw(pXGI->PIOBase + index);
    }
    else
    {
        reg = MMIO_IN16(pXGI->IOBase, index);
    }
    return reg;
}

static __inline__ CARD32 xinl(XGIPtr pXGI, CARD32 index)
{
    CARD32 reg;
    if(pXGI->noMMIO)
    {
        reg = inl(pXGI->PIOBase + index);
    }
    else
    {
        reg = MMIO_IN32(pXGI->IOBase, index);
    }
    return reg;
}

static __inline__ void xoutb(XGIPtr pXGI, CARD32 index, CARD8 data)
{
    if(pXGI->noMMIO)
    {
        outb(pXGI->PIOBase + index, data);
    }
    else
    {
        MMIO_OUT8(pXGI->IOBase, index, data);
    }
}

static __inline__ void xoutw(XGIPtr pXGI, CARD32 index, CARD16 data)
{
    if(pXGI->noMMIO)
    {
        outw(pXGI->PIOBase + index, data);
    }
    else
    {
        MMIO_OUT16(pXGI->IOBase, index, data);
    }
}

static __inline__ void xoutl(XGIPtr pXGI, CARD32 index, CARD32 data)
{
    if(pXGI->noMMIO)
    {
        outl(pXGI->PIOBase + index, data);
    }
    else
    {
        MMIO_OUT32(pXGI->IOBase, index, data);
    }
}

static __inline__ CARD8 xinb3x5(XGIPtr pXGI, CARD8 index)
{
    volatile CARD8 data=0;

    xoutb(pXGI, 0x3d4, index);
    data = xinb(pXGI, 0x3d5);

    return data;
}

static __inline__ void xoutb3x5(XGIPtr pXGI, CARD8 index, CARD8 data)
{
    xoutb(pXGI, 0x3d4, index);
    xoutb(pXGI, 0x3d5, data);
}

static __inline__ CARD8 xinb3c5(XGIPtr pXGI, CARD8 index)
{
    volatile CARD8 data=0;

    xoutb(pXGI, 0x3c4, index);
    data = xinb(pXGI, 0x3c5);

    return data;
}

static __inline__ void xoutb3c5(XGIPtr pXGI, CARD8 index, CARD8 data)
{
    xoutb(pXGI, 0x3c4, index);
    xoutb(pXGI, 0x3c5, data);
}

static __inline__ CARD8 xinb3cf(XGIPtr pXGI, CARD8 index)
{
    volatile CARD8 data=0;

    xoutb(pXGI, 0x3ce, index);
    data = xinb(pXGI, 0x3cf);

    return data;
}

static __inline__ void xoutb3cf(XGIPtr pXGI, CARD8 index, CARD8 data)
{
    xoutb(pXGI, 0x3ce, index);
    xoutb(pXGI, 0x3cf, data);
}

static __inline__ CARD16 xinw3x5(XGIPtr pXGI, CARD8 index)
{
    volatile CARD16 data=0;

    xoutb(pXGI, 0x3d4, index);
    data = xinb(pXGI, 0x3d5);

    xoutb(pXGI, 0x3d4, index+1);
    data += xinb(pXGI, 0x3d5) << 8;

    return data;
}

static __inline__ void xoutw3x5(XGIPtr pXGI, CARD8 index, CARD16 data)
{
    xoutb(pXGI, 0x3d4, index);
    xoutb(pXGI, 0x3d5, (CARD8)data);
    xoutb(pXGI, 0x3d4, index+1);
    xoutb(pXGI, 0x3d5, (CARD8)(data>>8));
}

static __inline__ CARD16 xinw3c5(XGIPtr pXGI, CARD8 index)
{
    volatile CARD16 data=0;

    xoutb(pXGI, 0x3c4, index);
    data = xinb(pXGI, 0x3c5);

    xoutb(pXGI, 0x3c4, index+1);
    data += xinb(pXGI, 0x3c5) << 8;

    return data;
}

static __inline__ void xoutw3c5(XGIPtr pXGI, CARD8 index, CARD16 data)
{
    xoutb(pXGI, 0x3c4, index);
    xoutb(pXGI, 0x3c5, data);
    xoutb(pXGI, 0x3c4, index+1);
    xoutb(pXGI, 0x3c5, data>>8);
}

static __inline__ CARD16 xinw3cf(XGIPtr pXGI, CARD8 index)
{
    volatile CARD16 data=0;

    xoutb(pXGI, 0x3ce, index);
    data = xinb(pXGI, 0x3cf);

    xoutb(pXGI, 0x3ce, index+1);
    data += xinb(pXGI, 0x3cf) << 8;

    return data;
}

static __inline__ void xoutw3cf(XGIPtr pXGI, CARD8 index, CARD16 data)
{
    xoutb(pXGI, 0x3ce, index);
    xoutb(pXGI, 0x3cf, data);
    xoutb(pXGI, 0x3ce, index+1);
    xoutb(pXGI, 0x3cf, data>>8);
}

static __inline__ void vAcquireRegIOProctect(XGIPtr pXGI)
{
    /* unprotect all register except which protected by 3c5.0e.7 */
    xoutb3c5(pXGI, 0x11, 0x92);
}

#define INB(port)               xinb(pXGI, port)
#define INW(port)               xinw(pXGI, port)
#define INDW(port)              xinl(pXGI, port)
#define OUTB(port, data)        xoutb(pXGI, port, data)
#define OUTW(port, data)        xoutw(pXGI, port, data)
#define OUTDW(port, data)       xoutl(pXGI, port, data)

#define OUT3X5B(index, data)    xoutb3x5(pXGI, index, data)
#define OUT3X5W(index, data)    xoutw3x5(pXGI, index, data)
#define OUT3C5B(index, data)    xoutb3c5(pXGI, index, data)
#define OUT3C5W(index, data)    xoutw3c5(pXGI, index, data)
#define OUT3CFB(index, data)    xoutb3cf(pXGI, index, data)
#define OUT3CFW(index, data)    xoutw3cf(pXGI, index, data)

#define IN3C5B(index)           xinb3c5(pXGI, index)
#define IN3C5W(index)           xinw3c5(pXGI, index)
#define IN3X5B(index)           xinb3x5(pXGI, index)
#define IN3X5W(index)           xinw3x5(pXGI, index)
#define IN3CFB(index)           xinb3cf(pXGI, index)
#define IN3CFW(index)           xinw3cf(pXGI, index)


static __inline__ void EnableProtect(XGIPtr pXGI)
{
    OUTB(0x3C4, 0x11);
    OUTB(0x3C5, 0x92);
}

static __inline__ void DisableProtect(XGIPtr pXGI)
{
    OUTB(0x3C4, 0x11);
    OUTB(0x3C5, 0x92);
}

#define Out(port, data)         xoutb(pXGI, port, data)
#define bOut(port, data)        xoutb(pXGI, port, data)
#define wOut(port, data)        xoutw(pXGI, port, data)
#define dwOut(port, data)       xoutl(pXGI, port, data)

#define Out3x5(index, data)     xoutb3x5(pXGI, index, data)
#define bOut3x5(index, data)    xoutb3x5(pXGI, index, data)
#define wOut3x5(index, data)    xoutw3x5(pXGI, index, data)

#define Out3c5(index, data)     xoutb3c5(pXGI, index, data)
#define bOut3c5(index, data)    xoutb3c5(pXGI, index, data)
#define wOut3c5(index, data)    xoutw3c5(pXGI, index, data)

#define Out3cf(index, data)     xoutb3cf(pXGI, index, data)
#define bOut3cf(index, data)    xoutb3cf(pXGI, index, data)
#define wOut3cf(index, data)    xoutw3cf(pXGI, index, data)

#define In(port)                xinb(pXGI, port)
#define bIn(port)               xinb(pXGI, port)
#define wIn(port)               xinw(pXGI, port)
#define dwIn(port)              xinl(pXGI, port)

#define In3x5(index)            xinb3x5(pXGI, index)
#define bIn3x5(index)           xinb3x5(pXGI, index)
#define wIn3x5(index)           xinw3x5(pXGI, index)

#define In3c5(index)            xinb3c5(pXGI, index)
#define bIn3c5(index)           xinb3c5(pXGI, index)
#define wIn3c5(index)           xinw3c5(pXGI, index)

#define In3cf(index)            xinb3cf(pXGI, index)
#define bIn3cf(index)           xinb3cf(pXGI, index)
#define wIn3cf(index)           xinw3cf(pXGI, index)

/*
#define OUTW_3C4(reg) \
            OUTW(0x3C4, (pXGIReg->regs3C4[reg])<<8 | (reg))
#define OUTW_3CE(reg) \
            OUTW(0x3CE, (pXGIReg->regs3CE[reg])<<8 | (reg))
#define OUTW_3X4(reg) \
            OUTW(0x3D4, (pXGIReg->regs3X4[reg])<<8 | (reg))
#define INB_3X4(reg) \
            OUTB(0x3D4, reg); \
            pXGIReg->regs3X4[reg] = INB(0x3D5)
#define INB_3C4(reg) \
            OUTB(0x3C4, reg); \
            pXGIReg->regs3C4[reg] = INB(0x3C5);
#define INB_3CE(reg) \
            OUTB(0x3CE, reg); \
            pXGIReg->regs3CE[reg] = INB(0x3CF);
*/
#endif
