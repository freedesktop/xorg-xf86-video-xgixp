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
#include "xg47_accel.h"
#include "xgi_debug.h"

Bool XGIAccelInit(ScreenPtr pScreen)
{
    ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];
    XGIPtr          pXGI = XGIPTR(pScrn);
    Bool            ret = FALSE;

    switch(pXGI->chipset)
    {
    case XG47:
        ret = XG47AccelInit(pScreen);
	    XGIDebug(DBG_FUNCTION, "[DBG] Jong 06142006-After XG47AccelInit()\n");
        break;
    default:
        ret = FALSE;
        break;
    }

    return ret;
}

/*
 * Enable 2D/3D Engine for Acceleration.
 * In XP4/XP5 code, its name is InitializeAccelerator.
 * It's called in modeinit at the first time and EnterVT() every time
*/
void XGIEngineInit(ScrnInfoPtr pScrn)
{
    XGIPtr      pXGI = XGIPTR(pScrn);

    switch(pXGI->chipset)
    {
    case XG47:
    {
        XG47EngineInit(pScrn);
        break;
    }
    default:
        break;
    }
}

