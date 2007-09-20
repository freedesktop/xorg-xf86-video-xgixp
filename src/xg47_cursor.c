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
#include "compiler.h"
#include "xf86fbman.h"

#include "xgi.h"
#include "xg47_cursor.h"
#include "xgi_regs.h"

#define CURSOR_WIDTH    64
#define CURSOR_HEIGHT   64

/* #undef ARGB_CURSOR */

static void XG47LoadCursorImage(ScrnInfoPtr pScrn, CARD8 *src);
static void XG47SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg);
static void XG47SetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static void XG47HideCursor(ScrnInfoPtr pScrn);
static void XG47ShowCursor(ScrnInfoPtr pScrn);
static Bool XG47UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs);

#ifdef ARGB_CURSOR
static Bool XG47UseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs);
static void XG47LoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs);
#endif


Bool XG47HWCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XGIPtr pXGI = XGIPTR(pScrn);
    xf86CursorInfoPtr pCursorInfo;
    const unsigned size_bytes = CURSOR_WIDTH * 4 * CURSOR_HEIGHT;

#ifdef CURSOR_DEBUG
    ErrorF("XG47HWCursorInit()-pScreen=0x%x, pScreen->myNum=%d\n", pScreen, pScreen->myNum);
#endif

    pCursorInfo = xf86CreateCursorInfoRec();
    if (!pCursorInfo) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "can't create cursor\n");
        return FALSE;
    }

    pCursorInfo->MaxWidth          = CURSOR_WIDTH;
    pCursorInfo->MaxHeight         = CURSOR_HEIGHT;
    pCursorInfo->Flags             = HARDWARE_CURSOR_BIT_ORDER_MSBFIRST        |
                                     HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK      |
                                     HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
                                     HARDWARE_CURSOR_TRUECOLOR_AT_8BPP;
    pCursorInfo->SetCursorColors   = XG47SetCursorColors;
    pCursorInfo->SetCursorPosition = XG47SetCursorPosition;
    pCursorInfo->LoadCursorImage   = XG47LoadCursorImage;
    pCursorInfo->HideCursor        = XG47HideCursor;
    pCursorInfo->ShowCursor        = XG47ShowCursor;
    pCursorInfo->UseHWCursor       = XG47UseHWCursor;

#ifdef ARGB_CURSOR
    pCursorInfo->UseHWCursorARGB   = XG47UseHWCursorARGB;
    pCursorInfo->LoadCursorARGB    = XG47LoadCursorARGB;
#endif

    /* 128 bit alignment */
    pXGI->cursorStart = ((12*1024 - 256) *1024 + 127) & 0xFFFFF80;

    pXGI->cursorEnd = pXGI->cursorStart + size_bytes;
    pXGI->pCursorInfo = pCursorInfo;
    pXGI->ScreenIndex = pScreen->myNum;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "Hardware cursor LOCATES in (0x%08x-0x%08x)\n",
               pXGI->cursorStart, pXGI->cursorEnd);

    return(xf86InitCursor(pScreen, pCursorInfo));
}

void XG47HWCursorCleanup(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn   = xf86Screens[pScreen->myNum];
    XGIPtr      pXGI    = XGIPTR(pScrn);
    uint32_t *d = (uint32_t *)(pXGI->fbBase + pXGI->cursorStart);
    int test = 0; /* 1; */ /* Jong 09/27/2006; test */

#ifdef CURSOR_DEBUG
    ErrorF("XG47HWCursorCleanup()-pScreen=0x%x\n", pScreen);
#endif

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "++ Enter %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif

#ifdef ARGB_CURSOR
    enableAlphaCursor(pXGI, FALSE);
#endif

#ifdef CURSOR_DEBUG
    ErrorF("XG47HWCursorCleanup()-pXGI->ScreenIndex=%d\n", pXGI->ScreenIndex);
#endif

    enableMonoCursor(pXGI, FALSE); 

    if (test == 1) {
        memset(d, 0, CURSOR_WIDTH * CURSOR_HEIGHT * 4);
    }

#if DBG_FLOW
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "-- Leave %s() %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
#endif
}

static void XG47LoadCursorImage(ScrnInfoPtr pScrn, CARD8 *src)
{ 
    XGIPtr pXGI = XGIPTR(pScrn);
    xf86CursorInfoPtr pCursor = pXGI->pCursorInfo;
    uint32_t *d = (uint32_t *)(pXGI->fbBase + pXGI->cursorStart);


#ifdef CURSOR_DEBUG
    ErrorF("XG47LoadCursorImage()-pScrn=0x%x\n", pScrn);
#endif

#ifdef ARGB_CURSOR
    pXGI->cursor_argb = FALSE;      
#endif

    vAcquireRegIOProtect(pXGI);

#ifdef CURSOR_DEBUG
    ErrorF("XG47LoadCursorImage()-pXGI->ScreenIndex=%d\n", pXGI->ScreenIndex);
#endif

    enableMonoCursorOfSecondView(pXGI, FALSE);
    enableMonoCursor(pXGI, FALSE);

    memcpy((CARD8*)d, src, pCursor->MaxWidth * pCursor->MaxHeight / 4);

#ifdef CURSOR_DEBUG
    ErrorF("XG47LoadCursorImage()-pXGI->ScreenIndex=%d\n", pXGI->ScreenIndex);
#endif

    setMonoCursorPatternOfSecondView(pXGI, pXGI->cursorStart);
    setMonoCursorSizeOfSecondView(pXGI, 64);
    setMonoCursorPitchOfSecondView(pXGI, 64); 
    setMonoCursorPattern(pXGI, pXGI->cursorStart);
    setMonoCursorSize(pXGI, 64); 
}

static void XG47SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    XGIPtr pXGI = XGIPTR(pScrn);

#ifdef CURSOR_DEBUG
    ErrorF("XG47SetCursorColors()-pScrn=0x%x, bg=%d, fg=%d\n", pScrn, bg, fg);
#endif

#ifdef ARGB_CURSOR
    if (pXGI->cursor_argb)
        return;     /* not need to set color */
#endif

    vAcquireRegIOProtect(pXGI);
    setMonoCursorColor(pXGI, bg, fg);

    setMonoCursorColorOfSecondView(pXGI, bg, fg);
}

static void XG47SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    XGIPtr pXGI = XGIPTR(pScrn);
 
#ifdef CURSOR_DEBUG
    ErrorF("XG47SetCursorPosition()-pScrn=0x%x, x=%d, y=%d\n", pScrn, x, y);
#endif

    vAcquireRegIOProtect(pXGI);

#ifdef CURSOR_DEBUG
    ErrorF("XG47SetCursorPosition()-pXGI->ScreenIndex=%d\n", pXGI->ScreenIndex);
#endif

    if (pXGI->ScreenIndex == 1) {
        setMonoCursorPositionOfSecondView(pXGI, x, y);    
    } else {
#ifdef ARGB_CURSOR
        if (pXGI->cursor_argb) {
            setAlphaCursorPosition(pXGI, x, y);
            return;
        }
#endif

        setMonoCursorPosition(pXGI, x, y);    
    }
}

static void XG47HideCursor(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);

#ifdef CURSOR_DEBUG
    ErrorF("XG47HideCursor()-pScrn=0x%x\n", pScrn);
#endif

    vAcquireRegIOProtect(pXGI);

#ifdef ARGB_CURSOR
    if (pXGI->cursor_argb) {
        enableAlphaCursor(pXGI, FALSE);
        return;
    }        
#endif

#ifdef CURSOR_DEBUG
    ErrorF("XG47HideCursor()-pXGI->ScreenIndex=%d\n", pXGI->ScreenIndex);
#endif

    if (pXGI->ScreenIndex == 1) {
        enableMonoCursorOfSecondView(pXGI, FALSE);
    } else {
        enableMonoCursor(pXGI, FALSE);
    }
}

static void XG47ShowCursor(ScrnInfoPtr pScrn)
{
    XGIPtr pXGI = XGIPTR(pScrn);

#ifdef CURSOR_DEBUG
    ErrorF("XG47ShowCursor()-pScrn=0x%x\n", pScrn);
#endif

    vAcquireRegIOProtect(pXGI);

#ifdef ARGB_CURSOR
    if (pXGI->cursor_argb) {
        enableAlphaCursor(pXGI, TRUE);
        return;
    }        
#endif

#ifdef CURSOR_DEBUG
    ErrorF("XG47ShowCursor()-pXGI->ScreenIndex=%d\n", pXGI->ScreenIndex);
#endif

    if (pXGI->ScreenIndex == 1) {
        enableMonoCursorOfSecondView(pXGI, TRUE);
        enableMonoCursor(pXGI, TRUE); 
    } else {
        enableMonoCursor(pXGI, TRUE);
    }
}

static Bool XG47UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XGIPtr pXGI = XGIPTR(pScrn);


    return ((pXGI->isHWCursor) && (pXGI->cursorStart));
}


#ifdef ARGB_CURSOR
#include "cursorstr.h"

static Bool XG47UseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XGIPtr pXGI = XGIPTR(pScrn);
    Bool ret;

    /* Jong 09/27/2006; use software cursor for 2nd view instead */
    ret = (pScreen->myNum != 1) && pXGI->isHWCursor && pXGI->cursorStart
        && (pCurs->bits->height <= CURSOR_HEIGHT)
        && (pCurs->bits->width <= CURSOR_WIDTH);

#ifdef CURSOR_DEBUG
    ErrorF("XG47UseHWCursorARGB() pScreen->myNum = %d, return %s\n",
           pScreen->myNum, (ret) ? "TRUE" : "FALSE");
#endif

    return ret;
}

static void XG47LoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    XGIPtr  pXGI = XGIPTR(pScrn);
    CARD32  *d = (CARD32*) (pXGI->fbBase + pXGI->cursorStart);
    int     x, y, w, h;
    CARD32  *image = pCurs->bits->argb;
    CARD32  *i;

#ifdef CURSOR_DEBUG
    ErrorF("XG47LoadCursorARGB()-pScrn=0x%x, pCurs=0x%x\n", pScrn, pCurs);
#endif

    if (!image)
        return; /* XXX can't happen */

    pXGI->cursor_argb = TRUE;

    w = pCurs->bits->width;
    if (w > CURSOR_WIDTH)
        w = CURSOR_WIDTH;
    h = pCurs->bits->height;
    if (h > CURSOR_HEIGHT)
        h = CURSOR_HEIGHT;

    for (y = 0; y < h; y++) {
        i = image;
        image += pCurs->bits->width;
        for (x = 0; x < w; x++) {
            *d++ = *i++;
        }

        /* pad to the right with transparent */
        for (/* empty */; x < CURSOR_WIDTH; x++) {
            *d++ = 0;
        }
    }

    /* pad below with transparent */
    for (/* empty */; y < CURSOR_HEIGHT; y++) {
        for (x = 0; x < CURSOR_WIDTH; x++) {
            *d++ = 0;
        }
    }

#ifdef CURSOR_DEBUG
    ErrorF("XG47LoadCursorARGB()-pXGI->ScreenIndex=%d\n", pXGI->ScreenIndex);
#endif

    enableMonoCursorOfSecondView(pXGI, FALSE);
    setMonoCursorPatternOfSecondView(pXGI, pXGI->cursorStart);
    setMonoCursorSizeOfSecondView(pXGI, 64);
    setMonoCursorPitchOfSecondView(pXGI, 64); 
    setAlphaCursorPattern(pXGI, pXGI->cursorStart);
    setAlphaCursorSize(pXGI);
}
#endif /* #ifdef ARGB_CURSOR */


/* 
    [Wolke] the following code is derived from XG47 winXP code 
    1. Use HC2 to realize mono and alpha cursor since HC1 may have issue.
    2. simplified code according to the limitation under linux
*/
/* [Jong 09/25/2006] use video alpha cursor for second view */

/* Jong 09/25/2006; support dual view */
void setMonoCursorPitchOfSecondView(XGIPtr pXGI, int cursorSize)
{
    const unsigned pitch = (cursorSize == 128) ? 0x200 : 0x100;

#ifdef CURSOR_DEBUG
    ErrorF("setMonoCursorPitchOfSecondView()-cursorSize=%d\n", cursorSize);
#endif

    vAcquireRegIOProtect(pXGI);

    /*Video Alpha Cursor Pitch (128 bits alignment)*/
    OUTW(0x24D2, (CARD16)(pitch >> 4));
}

/* Jong 09/25/2006; support dual view */
void setMonoCursorPatternOfSecondView(XGIPtr pXGI, CARD32 patternAddr)
{
#ifdef CURSOR_DEBUG
    ErrorF("setMonoCursorPatternOfSecondView()-patternAddr=0x%x\n", patternAddr);
#endif

    vAcquireRegIOProtect(pXGI);

    /*Video Alpha Cursor Start Address (128 bits alignment)*/
    OUTDW(0x24D4,patternAddr >> 4); 
}

void setMonoCursorPattern(XGIPtr pXGI, CARD32 patternAddr)
{
    CARD16 data;
    CARD8 data8;
    patternAddr >>= 10;

    /* 3D5.79 and 3D5.78 define starting address bit15 - bit0.  The 2nd
     * Hardware starting address 3D4/3D5.3D bit18 - bit16
     * OUT3X5W(0x78, (CARD16)patternAddr);
     */
    data = (CARD16)patternAddr;
    OUT3X5W(0x78, data);

    patternAddr >>= 16;
    patternAddr  &= 0x7;

    data8 = (IN3X5B(0x3D) & 0xF8);
    OUT3X5B(0x3D, data8 | (CARD8)patternAddr);
}


void enableMonoCursorOfSecondView(XGIPtr pXGI, Bool visible)
{
#ifdef CURSOR_DEBUG
    ErrorF("enableMonoCursorOfSecondView()-visible=%d\n", visible);
#endif

    /* Jong 09/28/2006; use SW cursor instead */
    return;

    vAcquireRegIOProtect(pXGI);

    /* Jong 09/25/2006; enable cursor and select 8-8-8-8 Mode */
    OUTB(0x24D1, ((CARD8)INB(0x24D1) & 0xF8) | 0x03); /* OK */

    OUT3CFB(0x77, (IN3CFB(0x77) & 0x3F) | 0xC0);  /* OK */

    /* if(visible) */
    OUT3X5B(0x50, IN3X5B(0x50) | 0x08); /* Turn on Video Hardware Cursor */ /* OK */
    /*OUT3X5B(0x50, IN3X5B(0x50) | 0x48);*/ /* Turn on Video Hardware Cursor and select X11 */
    /* else */
    /* OUT3X5B(0x50, IN3X5B(0x50) & 0xF7);*/ /* Turn off Video Hardware Cursor */

    /* Jong 09/26/2006; Use X11 Compatible; will make second view black */
    /* OUT3X5B(0x50, IN3X5B(0x50) | 0x40); */
}

void enableMonoCursor(XGIPtr pXGI, Bool visible)
{
    const CARD8 data = IN3X5B(0x65);

    if (visible) {
        OUT3X5B(0x65, (data & 0xC7) | 0xc0);
    } else {
        OUT3X5B(0x65, (data & 0xC7) & 0x7f);
    }
}


void setMonoCursorColorOfSecondView(XGIPtr pXGI, int bg, int fg)
{
    OUT3X5B(0x48, (fg & 0x000000ff));
    OUT3X5B(0x49, (fg & 0x0000ff00) >> 8);
    OUT3X5B(0x4A, (fg & 0x00ff0000) >> 16);
    OUT3X5B(0x4C, (bg & 0x000000ff));
    OUT3X5B(0x4D, (bg & 0x0000ff00) >> 8);
    OUT3X5B(0x4E, (bg & 0x00ff0000) >> 16);
}


void setMonoCursorColor(XGIPtr pXGI, int bg, int fg)
{
    /* set HC2 foreground and background color for mono */
    OUT3X5B(0x6a, (fg & 0x000000ff));
    OUT3X5B(0x6b, (fg & 0x0000ff00) >> 8);
    OUT3X5B(0x6c, (fg & 0x00ff0000) >> 16);
    OUT3X5B(0x6d, (bg & 0x000000ff));
    OUT3X5B(0x6e, (bg & 0x0000ff00) >> 8);
    OUT3X5B(0x6f, (bg & 0x00ff0000) >> 16);
}


void setMonoCursorPositionOfSecondView(XGIPtr pXGI, int x, int y)
{
#ifdef CURSOR_DEBUG
    ErrorF("setMonoCursorPositionOfSecondView()-x=%d-y=%d\n", x,y);
#endif

    vAcquireRegIOProtect(pXGI);


    CARD8 X = (CARD8)(x & 0xFF);
    OUT3CFB(0x64, X);

    CARD8 Y = (CARD8)(y & 0xFF);
    OUT3CFB(0x66, Y);

    CARD8 XY = (CARD8)((((y >> 8) << 4) & 0xf0) + ((x >> 8) & 0x0f));
    OUT3CFB(0x65, XY);

    /* Offset */
    OUT3X5B(0x46, (CARD8)(X >> 16));
    OUT3X5B(0x47, (CARD8)(Y >> 16));

    /* Let the position setting take effect ??? Yes, spec says that ... */
    /* OUT3X5B(0x43, IN3X5B(0x43)); */
    OUT3X5B(0x43, 0x00);
}

void setMonoCursorPosition(XGIPtr pXGI, int x, int y)
{
    CARD16 data;
    CARD8 data8;
    CARD32 xCursor = x < 0 ? ((-x) << 16) : x;
    CARD32 yCursor = y < 0 ? ((-y) << 16) : y;

    data = (CARD16)xCursor;
    OUT3X5W(0x66, data);

    data8 = (CARD8)(xCursor >> 16);
    OUT3X5B(0x73, data8);

    data8 = (CARD8)(yCursor >> 16);
    OUT3X5B(0x77, data8);

    /* 3x5.69 should be set lastly. */
    data =  (CARD16)yCursor;
    OUT3X5W(0x68, data);
}


void setMonoCursorSizeOfSecondView(XGIPtr pXGI, int cursorSize)
{
#ifdef CURSOR_DEBUG
    ErrorF("setMonoCursorSizeOfSecondView()-cursorSize=%d\n", cursorSize);
#endif

    vAcquireRegIOProtect(pXGI);

    if (cursorSize == 64) {
        OUT3X5B(0x50, (IN3X5B(0x50) & 0xFC) | 0x01);
    } else {
        OUT3X5B(0x50, (IN3X5B(0x50) & 0xFC) | 0x02);
    }
}

void setMonoCursorSize(XGIPtr pXGI, CARD32 cursorSize)
{
    /* Jong 07/12/2006 */
    /* bits[0:1] = 1 -> 64x64 */
    /* bits[0:1] = 2 -> 128x128 */
    CARD8 sizeReg = 0x65;
    CARD8 data = IN3X5B(sizeReg);


    if (cursorSize == 128) {
        OUT3X5B(sizeReg, (data & 0xFC) | 0x02);
    } else if (cursorSize == 64 || cursorSize == 32) {
        OUT3X5B(sizeReg, (data & 0xFC) | 0x01);
    }
}


void setAlphaCursorPosition(XGIPtr pXGI, int x, int y)
{
    CARD32 xCursor = x < 0 ? ((-x) << 16) : x;
    CARD32 yCursor = y < 0 ? ((-y) << 16) : y;

    OUT3X5W(0x66, (CARD16)xCursor);
    OUT3X5W(0x73, (CARD8)(xCursor >> 16));
    OUT3X5W(0x77, (CARD8)(yCursor >> 16));
    /* 3x5.69 should be set lastly. */
    OUT3X5W(0x68, (CARD16)yCursor);
}


void enableAlphaCursor(XGIPtr pXGI, Bool visible)
{
    if (visible) {
        /* Set window key, touch bit4-5 only */
        OUT3CFB(0x77, (IN3CFB(0x77) & 0xCF) | 0x20);
        OUT3X5B(0x65, (IN3X5B(0x65) & 0xBF) | 0x98);
    } else {
        OUT3X5B(0x65, (IN3X5B(0x65) & 0xBF) & 0x7f);
    }
}

void setAlphaCursorPattern(XGIPtr pXGI, CARD32 patternAddr)
{
    patternAddr >>= 10;
    /* 3D5.79 and 3D5.78 define starting address bit15 - bit0. */
    /* The 2nd Hardware starting address 3D4/3D5.3D bit18 - bit16 */
    OUT3X5W(0x78, (CARD16)patternAddr);
    patternAddr >>= 16;
    patternAddr  &= 0x7;
    OUT3X5B(0x3D, (IN3X5B(0x3D) & 0xF8) | (CARD8)patternAddr);
}

/* under linux, only support a8r8b8g8 @ 64x64 */
void setAlphaCursorSize(XGIPtr pXGI)
{
    OUT3X5B(0x65, (IN3X5B(0x65) & 0xCC) | 0x31);
}
