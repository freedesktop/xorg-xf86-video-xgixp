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

Bool XGIPcieMemAllocate(ScrnInfoPtr pScrn,
                        unsigned long size,
                        unsigned long *pBufBusAddr,
                        unsigned long *pBufHWAddr,
                        unsigned long *pBufVirtAddr)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    XGIMemReqRec    req;
    XGIMemAllocRec  alloc;
    int             ret = 0;
    Bool            isAllocated = FALSE;

    req.size = size;

    ret = ioctl(pXGI->fd, XGI_IOCTL_PCIE_ALLOC, &req);
    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIPcieMemAllocate()-1\n");

    if (ret < 0)
    {
        isAllocated = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory allocate ioctl failed !\n");
    }
    else
    {
        isAllocated = TRUE;
        alloc.location = req.location;
        alloc.size     = req.size;
        alloc.busAddr  = req.isFront;
        alloc.hwAddr   = req.pad;
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "alloc.size: 0x%x alloc.busAddr: 0x%x alloc.hwAddr: 0x%x\n",
                           alloc.size, alloc.busAddr, alloc.hwAddr);
    }

    if (alloc.busAddr == 0)
    {
        isAllocated = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory allocate failed !\n");
    }
    else
    {
        if (pBufBusAddr != NULL)
        {
            *pBufBusAddr  = alloc.busAddr;
		    XGIDebug(DBG_FUNCTION, "[DBG-Jong-XGIPcieMemAllocate] pBufBusAddr != NULL\n");
        }

        *pBufHWAddr   = alloc.hwAddr;

	    XGIDebug(DBG_FUNCTION, "[DBG-Jong 06022006] XGIPcieMemAllocate()-alloc.hwAddr=0x%lx\n", alloc.hwAddr);
	    XGIDebug(DBG_FUNCTION, "[DBG-Jong 06022006] XGIPcieMemAllocate()-alloc.busAddr=0x%lx\n", alloc.busAddr);

		/* Jong 06/06/2006; why alloc.busAddr located in Frame Buffer adress space on Gateway platform */
		/* Jong 06/14/2006; this virtual address seems not correct */
        *pBufVirtAddr = (unsigned long)mmap(0, alloc.size, PROT_READ|PROT_WRITE,
                                            MAP_SHARED, pXGI->fd, alloc.busAddr);

	    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIPcieMemAllocate()-*pBufHWAddr=0x%lx\n", *pBufHWAddr);
	    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIPcieMemAllocate()-*pBufVirtAddr=0x%lx\n", *pBufVirtAddr);

        if (*pBufVirtAddr == (unsigned long)(-1))
        {
            isAllocated = FALSE;
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory mmap failed !\n");
        }
        else
        {
            isAllocated = TRUE;
            if (pBufBusAddr != NULL)
            {
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "pBufVirtAddr: 0x%x pBufBusAddr: 0x%x pBufHWAddr: 0x%x\n",
                           *pBufVirtAddr, *pBufBusAddr, *pBufHWAddr);
	 		    XGIDebug(DBG_FUNCTION, "[DBG-Jong-XGIPcieMemAllocate] pBufBusAddr != NULL\n");
           }
            else
            {
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "pBufVirtAddr: 0x%x pBufBusAddr: 0x%x pBufHWAddr: 0x%x\n",
                           *pBufVirtAddr, NULL, *pBufHWAddr);
			    XGIDebug(DBG_FUNCTION, "[DBG-Jong-XGIPcieMemAllocate] pBufBusAddr == NULL\n");
            }
        }
    }

    return isAllocated;
}

Bool XGIPcieMemFree(ScrnInfoPtr pScrn,
                    unsigned long size,
                    unsigned long bufBusAddr,
                    unsigned long bufHWAddr,
                    void          *pBufVirtAddr)
{
    XGIPtr          pXGI = XGIPTR(pScrn);
    XGIMemAllocRec  alloc;
    int             ret = 0;
    Bool            isFreed = FALSE;

    if (!bufBusAddr)
    {
        bufBusAddr = bufHWAddr;
    }

    if ((size <= 0) || ((size & 0xFFF) != 0)
         || (bufBusAddr < pXGI->fbSize)
         || (bufBusAddr != bufHWAddr)
         || ((bufBusAddr & 0xFFF) != 0))
    {
        isFreed = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Wrong PCIE memory size: 0x%x bufBusAddr: 0x%x bufHWAddr: 0x%x to free \n",
                   size, bufBusAddr, bufHWAddr);
    }
    alloc.size    = size;
    alloc.busAddr = bufBusAddr;
    ret = munmap((void *)pBufVirtAddr, alloc.size);

    if (ret < 0)
    {
        isFreed = FALSE;
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory munmap failed !\n");
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "PCIE memory free size: 0x%x pBufVirtAddr: 0x%x bufBusAddr: 0x%x bufHWAddr: 0x%x \n",
                   size, (unsigned long *)pBufVirtAddr, bufBusAddr, bufHWAddr);
    }
    else
    {
        isFreed = TRUE;
        ret = ioctl(pXGI->fd, XGI_IOCTL_PCIE_FREE, &(alloc.busAddr));
	    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIPcieMemFree()-1\n");

        if (ret < 0)
        {
            isFreed = FALSE;
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "PCIE memory IOCTL free failed !\n");
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                       "PCIE memory free size: 0x%x pBufVirtAddr: 0x%x bufBusAddr: 0x%x bufHWAddr: 0x%x \n",
                       size, (unsigned long *)pBufVirtAddr, bufBusAddr, bufHWAddr);
        }
        else
        {
            isFreed = TRUE;
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "PCIE memory munmap successfully !\n");
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "PCIE memory IOCTL free successfully !\n");
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                       "PCIE memory free size: 0x%x pBufVirtAddr: 0x%x bufBusAddr: 0x%x bufHWAddr: 0x%x \n",
                       size, (unsigned long *)pBufVirtAddr, bufBusAddr, bufHWAddr);
        }
    }

    return isFreed;
}

Bool XGIShareAreaInfo(ScrnInfoPtr pScrn,
                     unsigned long busAddr,
                     unsigned long size)
{
    int retVal;
    XGIPtr          pXGI = XGIPTR(pScrn);
    XGIShareAreaRec sarea;

    sarea.busAddr = busAddr;
    sarea.size    = size;
    retVal    = ioctl(pXGI->fd, XGI_IOCTL_SAREA_INFO, &sarea);
    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] XGIShareAreaInfo()-1\n");

    if (retVal < 0)
    {
        return FALSE;
    }

    return TRUE;
}
