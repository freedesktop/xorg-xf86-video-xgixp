/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/xgi/xgi_driver.h,v */

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

#ifndef _XGI_DRIVER_H_
#define _XGI_DRIVER_H_

/*
 * IOCTL defines
 */
typedef enum {
    NON_LOCAL = 0,
    LOCAL     = 1
} XGIMemLocationRec;

typedef struct {
    XGIMemLocationRec   location;
    unsigned long       size;
    unsigned long       isFront;
    unsigned long       pad;
} XGIMemReqRec;

typedef struct {
    XGIMemLocationRec   location;
    unsigned long       size;
    unsigned long       busAddr;
    unsigned long       hwAddr;
} XGIMemAllocRec;

typedef struct {
    CARD32      scrnStart;
    CARD32      scrnWidth;
    CARD32      scrnHeight;
    CARD32      scrnBpp;
    CARD32      scrnPitch;
} XGIScreenInfoRec;

typedef struct {
    unsigned long   busAddr;
    unsigned long   size;
} XGIShareAreaRec;

typedef enum
{
    BTYPE_2D = 0,
    BTYPE_3D = 1,
    BTYPE_FLIP = 2,
    BTYPE_CTRL = 3,
    BTYPE_NONE = 0x7fffffff
}BATCH_TYPE;

typedef struct xgi_cmd_info_s
{
    BATCH_TYPE  _firstBeginType;
    CARD32      _firstBeginAddr;
    CARD32      _firstSize;
    CARD32      _curDebugID;
    CARD32      _lastBeginAddr;
    CARD32      _beginCount;
} XGICmdInfoRec;

typedef struct xgi_state_info_s
{
    CARD32      _fromState;
    CARD32      _toState;
} XGIStateInfoRec;

#ifdef XFree86LOADER
#define _IOC_NRBITS     8
#define _IOC_TYPEBITS   8
#define _IOC_SIZEBITS   14
#define _IOC_DIRBITS    2

#define _IOC_NRSHIFT	0
#define _IOC_TYPESHIFT	(_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT	(_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT	(_IOC_SIZESHIFT+_IOC_SIZEBITS)

/* Direction bits. */
#define _IOC_NONE	0U
#define _IOC_WRITE	1U
#define _IOC_READ	2U

#define _IOC(dir,type,nr,size) \
	(((dir)  << _IOC_DIRSHIFT) | \
	 ((type) << _IOC_TYPESHIFT) | \
	 ((nr)   << _IOC_NRSHIFT) | \
	 ((size) << _IOC_SIZESHIFT))

/* used to create numbers */
#define _IO(type,nr)        _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)  _IOC(_IOC_READ,(type),(nr), sizeof(size))
#define _IOW(type,nr,size)  _IOC(_IOC_WRITE,(type),(nr), sizeof(size))
#define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE, (type), (nr), sizeof(size))
#endif

/*
 * Ioctl definitions
 */

#define XGI_IOCTL_MAGIC             'x'     /* use 'x' as magic number */

#define XGI_IOCTL_BASE              0
#define XGI_ESC_DEVICE_INFO         (XGI_IOCTL_BASE + 0)
#define XGI_ESC_POST_VBIOS          (XGI_IOCTL_BASE + 1)
#define XGI_ESC_FB_INIT             (XGI_IOCTL_BASE + 2)
#define XGI_ESC_FB_ALLOC            (XGI_IOCTL_BASE + 3)
#define XGI_ESC_FB_FREE             (XGI_IOCTL_BASE + 4)
#define XGI_ESC_PCIE_INIT           (XGI_IOCTL_BASE + 5)
#define XGI_ESC_PCIE_ALLOC          (XGI_IOCTL_BASE + 6)
#define XGI_ESC_PCIE_FREE           (XGI_IOCTL_BASE + 7)
#define XGI_ESC_SUBMIT_CMDLIST      (XGI_IOCTL_BASE + 8)
#define XGI_ESC_PUT_SCREEN_INFO     (XGI_IOCTL_BASE + 9)
#define XGI_ESC_GET_SCREEN_INFO     (XGI_IOCTL_BASE + 10)
#define XGI_ESC_GE_RESET            (XGI_IOCTL_BASE + 11)
#define XGI_ESC_SAREA_INFO          (XGI_IOCTL_BASE + 12)

#define XGI_ESC_DEBUG_INFO          (XGI_IOCTL_BASE + 14)

#define XGI_ESC_TEST_RWINKERNEL     (XGI_IOCTL_BASE + 16)
#define XGI_ESC_STATE_CHANGE        (XGI_IOCTL_BASE + 17)
#define XGI_ESC_MMIO_INFO           (XGI_IOCTL_BASE + 18)




//#define XGI_IOCTL_DEVICE_INFO       _IOR(XGI_IOCTL_MAGIC, XGI_ESC_DEVICE_INFO, xgi_chip_info_t)
#define XGI_IOCTL_POST_VBIOS        _IO(XGI_IOCTL_MAGIC, XGI_ESC_POST_VBIOS)

#define XGI_IOCTL_FB_INIT           _IO(XGI_IOCTL_MAGIC, XGI_ESC_FB_INIT)
#define XGI_IOCTL_FB_ALLOC          _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_FB_ALLOC, XGIMemReqRec)
#define XGI_IOCTL_FB_FREE           _IOW(XGI_IOCTL_MAGIC, XGI_ESC_FB_FREE, unsigned long)

#define XGI_IOCTL_PCIE_INIT         _IO(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_INIT)
#define XGI_IOCTL_PCIE_ALLOC        _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_ALLOC, XGIMemReqRec)
#define XGI_IOCTL_PCIE_FREE         _IOW(XGI_IOCTL_MAGIC, XGI_ESC_PCIE_FREE, unsigned long)

//#define XGI_IOCTL_MMIO_INFO         _IOR(XGI_IOCTL_MAGIC, XGI_ESC_MMIO_INFO, xgi_mmio_info_t)
#define XGI_IOCTL_PUT_SCREEN_INFO   _IOW(XGI_IOCTL_MAGIC, XGI_ESC_PUT_SCREEN_INFO, XGIScreenInfoRec)
#define XGI_IOCTL_GET_SCREEN_INFO   _IOR(XGI_IOCTL_MAGIC, XGI_ESC_GET_SCREEN_INFO, XGIScreenInfoRec)

#define XGI_IOCTL_GE_RESET          _IO(XGI_IOCTL_MAGIC, XGI_ESC_GE_RESET)
#define XGI_IOCTL_SAREA_INFO        _IOW(XGI_IOCTL_MAGIC, XGI_ESC_SAREA_INFO, XGIShareAreaRec)

#define XGI_IOCTL_DEBUG_INFO        _IO(XGI_IOCTL_MAGIC, XGI_ESC_DEBUG_INFO)

#define XGI_IOCTL_SUBMIT_CMDLIST    _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_SUBMIT_CMDLIST, XGICmdInfoRec)

#define XGI_IOCTL_TEST_RWINKERNEL   _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_TEST_RWINKERNEL, unsigned long)

#define XGI_IOCTL_STATE_CHANGE      _IOWR(XGI_IOCTL_MAGIC, XGI_ESC_STATE_CHANGE, XGIStateInfoRec)

#define XGI_IOCTL_MAXNR             20


#define IsPrimaryCard   ((xf86IsPrimaryPci(pXGI->pPciInfo)) || (xf86IsPrimaryIsa()))

extern Bool XGISwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
extern void XGIAdjustFrame(int scrnIndex, int x, int y, int flags);

#endif
