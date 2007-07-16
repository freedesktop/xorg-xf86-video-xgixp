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
#include "xgi_driver.h"
#include "xgi_bios.h"

#include "xgi_debug.h"

void XGIGetLcdSize(ScrnInfoPtr pScrn, CARD16 *lcdWidth, CARD16 *lcdHeight)
{
    XGIPtr pXGI = XGIPTR(pScrn);

    switch(pXGI->chipset)
    {
    case XG47:
    default:
        break;
    }
}

float XGICalculateMemoryClock(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
    float   freq = 0.0;

    switch (pXGI->chipset)
    {
    case XG47:
        freq = XGIBiosCalculateClock(pXGI, 0x1E, 0x1F);
        break;
    default:
        break;
    }

    return (freq);
}

#if 0
void XGIDumpRegisterValue(ScrnInfoPtr pScrn)
{
    XGIPtr  pXGI = XGIPTR(pScrn);

    int i ;
    unsigned long temp ;

    ErrorF("----------------------------------------------------------------------\n") ;
    ErrorF("0x3C4 xx\n") ;
    ErrorF("----------------------------------------------------------------------\n") ;
    for( i = 0 ; i < 0xFF ; i++ )
    {
        OUTB(0x3C4, i);
        temp = INB(0x3C5);
        ErrorF("R%02X = 0x%02X   ", i, temp);
        if ( ((i+1) % 4) == 0 )
        {
            ErrorF("\n") ;
        }
        if ( ((i+1) % 16) == 0 )
        {
            ErrorF("\n") ;
        }
    }
    ErrorF( "\n" ) ;
    ErrorF("----------------------------------------------------------------------\n") ;
    ErrorF("0x3D4 xx\n") ;
    ErrorF("----------------------------------------------------------------------\n") ;
    for( i = 0 ; i < 0xFF ; i++ )
    {
        OUTB(0x3D4, i);
        temp = INB(0x3D5);

        ErrorF("R%02X = 0x%02X   ", i, temp);
        if ( ((i+1) % 4) == 0 )
        {
            ErrorF("\n") ;
        }
        if ( ((i+1) % 16) == 0 )
        {
            ErrorF("\n") ;
        }
    }
    ErrorF( "\n" ) ;
    ErrorF("----------------------------------------------------------------------\n") ;
    ErrorF("0x3CE xx\n") ;
    ErrorF("----------------------------------------------------------------------\n") ;
    for( i = 0 ; i < 0xFF; i++ )
    {
        OUTB(0x3CE, i);
        temp = INB(0x3CF);

        ErrorF("R%02X = 0x%02X   ", i, temp);
        if (((i+1) % 4) == 0)
        {
            ErrorF("\n");
        }
        if (((i+1) % 16) == 0)
        {
            ErrorF("\n");
        }
    }
    ErrorF( "\n" );
}
#endif

/* Jong 07/12/2006 */
void XGIDumpMemory(CARD8 *addr, unsigned long size)
{
    int             i, j;

/* Jong 07/192006 */
#ifndef DUMP_MEMORY
	return;
#endif

	ErrorF("\n==================memory dump at 0x%x, size = %d ===============\n", addr, size);

    for(i=0; i<0x10; i++)
    {
        if(i == 0)
        {
            ErrorF("%5x", i);
        }
        else
        {
            ErrorF("%3x", i);
        }
    }
    ErrorF("\n");

    for(i=0; i<0x10; i++)
    {
        ErrorF("%1x ", i);

        for(j=0; j<0x10; j++)
        {
            ErrorF("%3x", *((addr+i*16)+j));
        }
        ErrorF("\n");
    }
}

void XGIDumpRegisterValue(ScrnInfoPtr pScrn)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    int             i, j;
    unsigned char   temp;

    /* 0x3C5 */
    ErrorF("\n==================0x%x===============\n", 0x3C5);

    for(i=0; i<0x10; i++)
    {
        if(i == 0)
        {
            ErrorF("%5x", i);
        }
        else
        {
            ErrorF("%3x", i);
        }
    }
    ErrorF("\n");

    for(i=0; i<0x10; i++)
    {
        ErrorF("%1x ", i);

        for(j=0; j<0x10; j++)
        {
            temp = IN3C5B(i*0x10 + j);
            ErrorF("%3x", temp);
        }
        ErrorF("\n");
    }

    /* 0x3D5 */
    ErrorF("\n==================0x%x===============\n", 0x3D5);
    for(i=0; i<0x10; i++)
    {
        if(i == 0)
        {
            ErrorF("%5x", i);
        }
        else
        {
            ErrorF("%3x", i);
        }
    }
    ErrorF("\n");

    for(i=0; i<0x10; i++)
    {
        ErrorF("%1x ", i);

        for(j=0; j<0x10; j++)
        {
            temp = IN3X5B(i*0x10 + j);
            ErrorF("%3x", temp);
        }
        ErrorF("\n");
    }

    /* 0x3CF */
    ErrorF("\n==================0x%x===============\n", 0x3CF);
    for(i=0; i<0x10; i++)
    {
        if(i == 0)
        {
            ErrorF("%5x", i);
        }
        else
        {
            ErrorF("%3x", i);
        }
    }
    ErrorF("\n");

    for(i=0; i<0x10; i++)
    {
        ErrorF("%1x ", i);

        for(j=0; j<0x10; j++)
        {
            temp = IN3CFB(i*0x10 + j);
            ErrorF("%3x", temp);
        }
        ErrorF("\n");
    }

    ErrorF("\n==================0x%x===============\n", 0xB000);
    for(i=0; i<0x10; i++)
    {
        if(i == 0)
        {
            ErrorF("%5x", i);
        }
        else
        {
            ErrorF("%3x", i);
        }
    }
    ErrorF("\n");

    for(i=0; i<0x5; i++)
    {
        ErrorF("%1x ", i);

        for(j=0; j<0x10; j++)
        {
            temp = INB(0xB000 + i*0x10 + j);
            ErrorF("%3x", temp);
        }
        ErrorF("\n");
    }

    ErrorF("\n==================0x%x===============\n", 0x2300);
    for(i=0; i<0x10; i++)
    {
        if(i == 0)
        {
            ErrorF("%5x", i);
        }
        else
        {
            ErrorF("%3x", i);
        }
    }
    ErrorF("\n");

    for(i=0; i<0x7; i++)
    {
        ErrorF("%1x ", i);

        for(j=0; j<0x10; j++)
        {
            temp = INB(0x2300 + i*0x10 + j);
            ErrorF("%3x", temp);
        }
        ErrorF("\n");
    }

    ErrorF("\n==================0x%x===============\n", 0x2400);
    for(i=0; i<0x10; i++)
    {
        if(i == 0)
        {
            ErrorF("%5x", i);
        }
        else
        {
            ErrorF("%3x", i);
        }
    }
    ErrorF("\n");

    for(i=0; i<0x10; i++)
    {
        ErrorF("%1x ", i);

        for(j=0; j<0x10; j++)
        {
            temp = INB(0x2400 + i*0x10 + j);
            ErrorF("%3x", temp);
        }
        ErrorF("\n");
    }
}

Bool XGIPcieMemAllocate(ScrnInfoPtr pScrn, size_t size,
                        unsigned long *pBufBusAddr,
                        uint32_t *pBufHWAddr, void **pBufVirtAddr)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    struct xgi_mem_alloc  alloc;
    int             ret = 0;

    alloc.size = size;

    ret = ioctl(pXGI->fd, XGI_IOCTL_PCIE_ALLOC, &alloc);
    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIPcieMemAllocate()-1\n");

    if ((ret < 0) || (alloc.bus_addr == 0)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory allocate ioctl failed !\n");
        return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "alloc.size: 0x%x "
               "alloc.busAddr: 0x%lx alloc.hwAddr: 0x%x\n",
               alloc.size, alloc.bus_addr, alloc.hw_addr);

    *pBufBusAddr = alloc.bus_addr;
    *pBufHWAddr = alloc.hw_addr;

    /* Jong 06/06/2006; why alloc.busAddr located in Frame Buffer adress space on Gateway platform */
    /* Jong 06/14/2006; this virtual address seems not correct */
    *pBufVirtAddr = mmap(0, alloc.size, PROT_READ|PROT_WRITE,
                         MAP_SHARED, pXGI->fd, alloc.bus_addr);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "pBufVirtAddr: 0x%p\n",
               *pBufVirtAddr);

    if (*pBufVirtAddr == MAP_FAILED) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory mmap failed!\n");
    }

    return *pBufVirtAddr != MAP_FAILED;
}

Bool XGIPcieMemFree(ScrnInfoPtr pScrn, size_t size,
                    unsigned long bufBusAddr, void *pBufVirtAddr)
{
    XGIPtr pXGI = XGIPTR(pScrn);
    int ret;
    int mode;


    if ((size <= 0) || ((size & 0xFFF) != 0)
         || (bufBusAddr < pXGI->fbSize)
         || ((bufBusAddr & 0xFFF) != 0)) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Wrong PCIE memory size: 0x%x bufBusAddr: 0x%lx to free\n",
                   (unsigned int) size, bufBusAddr);
    }

    ret = munmap(pBufVirtAddr, size);
    if (ret < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory munmap failed!\n");
        mode = X_ERROR;
    }
    else {
        ret = ioctl(pXGI->fd, XGI_IOCTL_PCIE_FREE, & bufBusAddr);
        XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIPcieMemFree()-1\n");

        mode = (ret < 0) ? X_ERROR : X_INFO;
        xf86DrvMsg(pScrn->scrnIndex, mode, "PCIE memory IOCTL free %s\n",
                   (ret < 0) ? "failed!" : "successful.");
    }

    xf86DrvMsg(pScrn->scrnIndex, mode,
               "PCIE memory free size: 0x%x pBufVirtAddr: 0x%p "
               "bufBusAddr: 0x%lx\n",
               (unsigned int) size, pBufVirtAddr, bufBusAddr);

    return ret == 0;
}
