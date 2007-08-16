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
#include "xgi.h"
#include "xg47_regs.h"
#include "xgi_driver.h"
#include "xg47_cmdlist.h"
#include "xgi_misc.h"
#include "xgi_debug.h"

struct xg47_batch {
    enum xgi_batch_type type;
    unsigned int data_count;        /* DWORDs */
    unsigned int request_size;      /* DWORDs */
    uint32_t *  begin;              /* The begin of current batch. */
    uint32_t *  data_begin;         /* The begin of data */
    uint32_t *  end;                /* current writing ptr */
};

struct xg47_buffer {
    uint32_t *    ptr;
    uint32_t      hw_addr;
    unsigned long bus_addr;
    unsigned int  size;            /* DWORDs */
};

struct xg47_CmdList
{
    struct xg47_batch current;
    struct xg47_batch previous;

    unsigned int _sendDataLength;            /* record the filled data size */

    struct xg47_buffer command;

    /* 2d cmd holder */
    uint32_t _bunch[4];

    /* fd number */
    int		_fd;
    
    struct _drmFence  top_fence;
    int top_fence_set;
    struct _drmFence  bottom_fence;
    int bottom_fence_set;
};


struct xg47_CmdList *
xg47_Initialize(ScrnInfoPtr pScrn, unsigned int cmdBufSize, int fd)
{
    struct xg47_CmdList *list = xnfcalloc(sizeof(struct xg47_CmdList), 1);
    int ret;

    list->command.size = cmdBufSize;
    list->_fd = fd;

    if (!XGIPcieMemAllocate(pScrn,
                            list->command.size * sizeof(uint32_t),
                            & list->command.bus_addr,
                            & list->command.hw_addr,
                            (void **) & list->command.ptr)) {
        XGIDebug(DBG_ERROR, "[DBG Error]Allocate CmdList buffer error!\n");
        goto err;
    }

    XGIDebug(DBG_CMDLIST, "cmdBuf VAddr=0x%p  HAddr=0x%p buffsize=0x%x\n",
             list->command.ptr, list->command.hw_addr, list->command.size);

    ret = drmFenceCreate(list->_fd, 0, 0, DRM_FENCE_TYPE_EXE,
                         & list->top_fence);
    if (ret) {
        xf86DrvMsg(0, X_ERROR, "Unable to create top-half fence (%s, %d)!\n",
                   strerror(-ret), -ret);
        goto err;
    }

    drmFenceCreate(list->_fd, 0, 0, DRM_FENCE_TYPE_EXE,
                   & list->bottom_fence);
    if (ret) {
        xf86DrvMsg(0, X_ERROR, "Unable to create bottom-half fence "
                   "(%s, %d)!\n", strerror(-ret), -ret);
        goto err;
    }

    xg47_Reset(list);

    return list;

err:
    xg47_Cleanup(pScrn, list);
    return NULL;
}

void xg47_Cleanup(ScrnInfoPtr pScrn, struct xg47_CmdList *s_pCmdList)
{
    if (s_pCmdList) {
        if (s_pCmdList->top_fence_set) {
            drmFenceWait(s_pCmdList->_fd, 0, & s_pCmdList->top_fence, 0);
        }

        if (s_pCmdList->bottom_fence_set) {
            drmFenceWait(s_pCmdList->_fd, 0, & s_pCmdList->bottom_fence, 0);
        }

        drmFenceDestroy(s_pCmdList->_fd, & s_pCmdList->top_fence);
        drmFenceDestroy(s_pCmdList->_fd, & s_pCmdList->bottom_fence);

        if (s_pCmdList->command.bus_addr) {
            XGIDebug(DBG_CMDLIST, "[DBG Free]cmdBuf VAddr=0x%x  HAddr=0x%x\n",
                     s_pCmdList->command.ptr,
                     s_pCmdList->command.hw_addr);

            XGIPcieMemFree(pScrn, s_pCmdList->command.size * sizeof(uint32_t),
                           s_pCmdList->command.bus_addr,
                           s_pCmdList->command.ptr);
        }

        xfree(s_pCmdList);
    }
}

void xg47_Reset(struct xg47_CmdList *s_pCmdList)
{
    s_pCmdList->previous.begin = s_pCmdList->command.ptr;
    s_pCmdList->previous.end = s_pCmdList->command.ptr;
    s_pCmdList->_sendDataLength = 0;
    s_pCmdList->current.end = 0;
}

/* Implementation Part*/
static int submit2DBatch(struct xg47_CmdList * pCmdList);
static void sendRemainder2DCommand(struct xg47_CmdList * pCmdList);

#ifdef DUMP_COMMAND_BUFFER
static void dumpCommandBuffer(struct xg47_CmdList * pCmdList);
#endif

uint32_t s_emptyBegin[AGPCMDLIST_BEGIN_SIZE] =
{
    0x10000000,     /* 3D Type Begin, Invalid */
    0x80000004,     /* Length = 4;  */
    0x00000000,
    0x00000000
};


/**
 * Reserve space in the command buffer
 * 
 * \param pCmdList  pointer to the command list structure
 * \param size      Size, in DWORDS, of the command
 *
 * \returns
 * 1 -- success 0 -- false
 */
int xg47_BeginCmdList(struct xg47_CmdList *pCmdList, unsigned int req_size)
{
    /* Pad the commmand list to 128-bit alignment and add the begin header.
     */
    const unsigned size = ((req_size + 0x3) & ~0x3) + AGPCMDLIST_BEGIN_SIZE;
    const uint32_t *const mid_point =
        pCmdList->command.ptr + (pCmdList->command.size / 2);
    const uint32_t *const end_point = 
        pCmdList->command.ptr + pCmdList->command.size;
    uint32_t * begin_cmd = pCmdList->previous.end;
    uint32_t *const end_cmd = pCmdList->previous.end + size;


    XGIDebug(DBG_CMDLIST, "[DEBUG] Enter beginCmdList.\n");

    if (size >= pCmdList->command.size) {
        return 0;
    }


    /* If the command spills into the bottom half of the command buffer,
     * wait on the bottom half's fence.
     */
    if ((begin_cmd < mid_point) && (end_cmd > mid_point)) {
        if (pCmdList->bottom_fence_set) {
            drmFenceWait(pCmdList->_fd, 0, & pCmdList->bottom_fence, 0);
            pCmdList->bottom_fence_set = 0;
        }
    } else {
        /* If the command won't fit at the end of the list and we need to wrap
         * back to the top half of the command buffer, wait on the top half's
         * fence.
         *
         * After waiting on the top half's fence, emit the bottom half's
         * fence.
         */
        if (end_cmd > end_point) {
            begin_cmd = pCmdList->command.ptr;

            if (pCmdList->top_fence_set) {
                drmFenceWait(pCmdList->_fd, 0, & pCmdList->top_fence, 0);
                pCmdList->top_fence_set = 0;
            }

            drmFenceEmit(pCmdList->_fd, 0, & pCmdList->bottom_fence, 0);
            pCmdList->bottom_fence_set = 1;
        }
    }


    /* Prepare the begin address of next batch. */
    pCmdList->current.begin = begin_cmd;
    pCmdList->current.end = pCmdList->current.begin;
    pCmdList->current.request_size = size;


    /* Prepare next begin */
    memcpy(pCmdList->current.end, s_emptyBegin, sizeof(s_emptyBegin));
    pCmdList->current.end += AGPCMDLIST_BEGIN_SIZE;
    pCmdList->current.data_count = AGPCMDLIST_BEGIN_SIZE;
    pCmdList->current.type = BTYPE_2D;
    pCmdList->_bunch[0] = 0x7f000000;
    pCmdList->_bunch[1] = 0x00000000;
    pCmdList->_bunch[2] = 0x00000000;
    pCmdList->_bunch[3] = 0x00000000;

    XGIDebug(DBG_CMDLIST, "[DEBUG] Leave beginCmdList.\n");
    return 1;
}


void xg47_EndCmdList(struct xg47_CmdList *pCmdList)
{
    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-1\n");
    sendRemainder2DCommand(pCmdList);

    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-2\n");
    submit2DBatch(pCmdList);
}


void emit_bunch(struct xg47_CmdList *pCmdList)
{
    /* Copy the commands from _bunch to the command buffer and advance the
     * command buffer write pointer.
     */
    pCmdList->current.end[0] = pCmdList->_bunch[0];
    pCmdList->current.end[1] = pCmdList->_bunch[1];
    pCmdList->current.end[2] = pCmdList->_bunch[2];
    pCmdList->current.end[3] = pCmdList->_bunch[3];
    pCmdList->current.end += 4;

    /* Reset _bunch.
     */
    pCmdList->_bunch[0] = 0x7f000000;
    pCmdList->_bunch[1] = 0x00000000;
    pCmdList->_bunch[2] = 0x00000000;
    pCmdList->_bunch[3] = 0x00000000;

    /* Advance data_count to the next 128-bit boundary.
     */
    pCmdList->current.data_count = (pCmdList->current.data_count + 3) & ~3;
}


void xg47_SendGECommand(struct xg47_CmdList *pCmdList, uint32_t addr,
                        uint32_t cmd)
{
    /* Encrypt the command for AGP. */
    const unsigned int shift = (pCmdList->current.data_count++) & 0x00000003;
    const uint32_t reg = (addr & 0x00ff);

    pCmdList->_bunch[0] |= (reg | 1) << (shift << 3);
    pCmdList->_bunch[shift + 1]  = cmd;

    /* Bunch finished, Send to HW. */
    if (2 == shift) {
        emit_bunch(pCmdList);
    }
}


static void sendRemainder2DCommand(struct xg47_CmdList * pCmdList)
{
    /* If there are any pending commands in _bunch, emit the whole batch.
     */
    if (0x7f000000 != pCmdList->_bunch[0]) {
        emit_bunch(pCmdList);
    }
}


#ifdef DUMP_COMMAND_BUFFER
void dumpCommandBuffer(struct xg47_CmdList * pCmdList)
{
    const unsigned int count = pCmdList->current.end
        - pCmdList->current.begin;
    unsigned int i;

    XGIDebug(DBG_FUNCTION,"Entering dumpCommandBuffer\n");

    for (i = 0; i < count; i += 4) {
        XGIDebug(DBG_CMD_BUFFER, "%08p: %08x %08x %08x %08x\n",
                 (pCmdList->current.begin + i),
                 pCmdList->current.begin[i + 0],
                 pCmdList->current.begin[i + 1],
                 pCmdList->current.begin[i + 2],
                 pCmdList->current.begin[i + 3]);
    }

    XGIDebug(DBG_FUNCTION,"Leaving dumpCommandBuffer\n");
}
#endif /* DUMP_COMMAND_BUFFER */


static int submit2DBatch(struct xg47_CmdList * pCmdList)
{
    uint32_t beginHWAddr;
    struct xgi_cmd_info submitInfo;
    int err;

    XGIDebug(DBG_FUNCTION, "%s: enter\n", __func__);

    if (0 == pCmdList->current.data_count) {
        return 0;
    }

    beginHWAddr = pCmdList->command.hw_addr
	+ ((intptr_t) pCmdList->current.begin
	   - (intptr_t) pCmdList->command.ptr);

    submitInfo.type = pCmdList->current.type;
    submitInfo.hw_addr = beginHWAddr;
    submitInfo.size = pCmdList->current.data_count;

    XGIDebug(DBG_FUNCTION, "%s: calling ioctl XGI_IOCTL_SUBMIT_CMDLIST\n", 
             __func__);

#ifdef DUMP_COMMAND_BUFFER
    dumpCommandBuffer(pCmdList);
#endif

    err = drmCommandWrite(pCmdList->_fd, DRM_XGI_SUBMIT_CMDLIST,
                          &submitInfo, sizeof(submitInfo));
    if (!err) {
        uint32_t *const begin_cmd = pCmdList->current.begin;
        uint32_t *const end_cmd = pCmdList->current.end;
        const uint32_t *const mid_point =
            pCmdList->command.ptr + (pCmdList->command.size / 2);

        pCmdList->previous = pCmdList->current;

        /* If the command is the last command in the top half, emit the top
         * half's fence.
         */
        if ((begin_cmd < mid_point) && (end_cmd >= mid_point)) {
            drmFenceEmit(pCmdList->_fd, 0, & pCmdList->top_fence, 0);
            pCmdList->top_fence_set = 1;
        }
    }
    else {
        ErrorF("[2D] ioctl -- cmdList error (%d, %s)!\n",
               -err, strerror(-err));
    }

    XGIDebug(DBG_FUNCTION, "%s: exit\n", __func__);
    return err;
}
