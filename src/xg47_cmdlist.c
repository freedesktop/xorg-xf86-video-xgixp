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

    CARD32      _sendDataLength;            /* record the filled data size */

    struct xg47_buffer command;
    struct xg47_buffer scratch;

    CARD32      _debugBeginID;          /* write it at begin header as debug ID */

    /* 2d cmd holder */
    CARD32      _bunch[4];

    /* fd number */
    int		_fd;
};


struct xg47_CmdList *
xg47_Initialize(ScrnInfoPtr pScrn, CARD32 cmdBufSize, int fd)
{
    struct xg47_CmdList *list = xnfcalloc(sizeof(struct xg47_CmdList), 1);

    list->command.size = cmdBufSize;
    list->_fd = fd;

    if (!XGIPcieMemAllocate(pScrn,
                            list->command.size * sizeof(CARD32),
			    & list->command.bus_addr,
			    & list->command.hw_addr,
			    (void **) & list->command.ptr)) {
        XGIDebug(DBG_ERROR, "[DBG Error]Allocate CmdList buffer error!\n");
	goto err;
    }

    XGIDebug(DBG_CMDLIST, "cmdBuf VAddr=0x%p  HAddr=0x%p buffsize=0x%x\n",
	     list->command.ptr, list->command.hw_addr, list->command.size);

    list->scratch.size = 1024;

    if (!XGIPcieMemAllocate(pScrn,
                            list->scratch.size * sizeof(uint32_t),
                            & list->scratch.bus_addr,
                            & list->scratch.hw_addr,
                            (void **) & list->scratch.ptr)) {
        XGIDebug(DBG_ERROR, "[DBG ERROR]Allocate Scratch Pad error!\n");

	goto err;
    }


    XGIDebug(DBG_CMDLIST, "[Malloc]Scratch VAddr=0x%p HAddr=0x%x\n",
           list->scratch.ptr, list->scratch.hw_addr);

    xg47_Reset(list);

    return list;

err:
    xg47_Cleanup(pScrn, list);
    return NULL;
}

void xg47_Cleanup(ScrnInfoPtr pScrn, struct xg47_CmdList *s_pCmdList)
{
    if (s_pCmdList) {
	if (s_pCmdList->scratch.bus_addr) {
	    XGIDebug(DBG_CMDLIST, "[DBG Free]Scratch VAddr=0x%x HAddr=0x%x\n",
		     s_pCmdList->scratch.ptr, 
		     s_pCmdList->scratch.hw_addr);
	
	    XGIPcieMemFree(pScrn, s_pCmdList->scratch.size * sizeof(uint32_t),
			   s_pCmdList->scratch.bus_addr,
			   s_pCmdList->scratch.ptr);
	}
	
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
    *(s_pCmdList->scratch.ptr) = 0;
    s_pCmdList->previous.begin = 0;
    s_pCmdList->previous.end = 0;
    s_pCmdList->_sendDataLength = 0;
    s_pCmdList->current.end = 0;
}

/* Implementation Part*/
static int submit2DBatch(struct xg47_CmdList * pCmdList);
static void sendRemainder2DCommand(struct xg47_CmdList * pCmdList);
static void addScratchBatch(struct xg47_CmdList * pCmdList);

static void waitCmdListAddrAvailable(struct xg47_CmdList * pCmdList,
    const void * addrStart, const void * addrEnd);

static inline void preventOverwriteCmdbuf(struct xg47_CmdList * pCmdList);
static inline uint32_t getGEWorkedCmdHWAddr(const struct xg47_CmdList *);
#ifdef DUMP_COMMAND_BUFFER
static void dumpCommandBuffer(struct xg47_CmdList * pCmdList);
#endif

CARD32 s_emptyBegin[AGPCMDLIST_BEGIN_SIZE] =
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
int xg47_BeginCmdList(struct xg47_CmdList *pCmdList, CARD32 size)
{
    XGIDebug(DBG_CMDLIST, "[DEBUG] Enter beginCmdList.\n");

    /* pad the commmand list */
    size  = (size + 0x3) & ~ 0x3;

    /* Add  begin head + scratch batch. */
    size += AGPCMDLIST_BEGIN_SIZE + AGPCMDLIST_2D_SCRATCH_CMD_SIZE;

    if (size >= pCmdList->command.size)
    {
        return 0;
    }

    if (NULL != pCmdList->previous.end)
    {
         /* We have spare buffer after last command list. */
        if ((pCmdList->previous.end + size) <=
            (pCmdList->command.ptr + pCmdList->command.size))
        {
            pCmdList->current.begin = pCmdList->previous.end;
        }
        else /* no spare space, must roll over */
        {
            preventOverwriteCmdbuf(pCmdList);
            pCmdList->current.begin = pCmdList->command.ptr;
        }
    }
    else /* fresh */
    {
        pCmdList->current.begin = pCmdList->command.ptr;
    }

    /* Prepare the begin address of next batch. */
    waitCmdListAddrAvailable(pCmdList, pCmdList->current.begin,
                             pCmdList->current.begin + size);

    pCmdList->current.end = pCmdList->current.begin;
    pCmdList->current.request_size = size;

    /* Prepare next begin */
    memcpy(pCmdList->current.end, s_emptyBegin,
           AGPCMDLIST_BEGIN_SIZE * sizeof(CARD32));
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
    addScratchBatch(pCmdList);

    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-3\n");
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


void xg47_SendGECommand(struct xg47_CmdList *pCmdList, CARD32 addr, CARD32 cmd)
{
    /* Encrypt the command for AGP. */
    CARD32 shift        = (pCmdList->current.data_count++) & 0x00000003;
    pCmdList->_bunch[0] |= (addr | 1) << (shift << 3);
    pCmdList->_bunch[shift + 1]  = cmd;

    /* Bunch finished, Send to HW. */
    if (2 == shift) {
        emit_bunch(pCmdList);
    }
}


void waitCmdListAddrAvailable(struct xg47_CmdList * pCmdList,
                              const void * addrStart, const void * addrEnd)
{
    /* Offsets, in bytes, from the start of the command buffer to the start
     * and end of the proposed range.
     */
    const intptr_t offset_start = (intptr_t) addrStart 
        - (intptr_t) pCmdList->command.ptr;
    const intptr_t offset_end = (intptr_t) addrEnd 
        - (intptr_t)pCmdList->command.ptr;


    /* The loop of waiting for enough command list buffer */
    while (1) {
        /* Get the current runing batch address. */

        const uint32_t hw_addr = getGEWorkedCmdHWAddr(pCmdList);
        const uint32_t cmd_offset = hw_addr - pCmdList->command.hw_addr;

        if (hw_addr != 0) {
            /* If cmdlist is fresh or cmdlist already rolled over, current
             * batch does not overlay the buffer. 
             */
            if ((cmd_offset < offset_start) || (cmd_offset > offset_end)) {
                /* There is enough memory at the begin of command list. */
                break;
            }
        }
        else {
            const intptr_t previous_begin = 
                (intptr_t) pCmdList->previous.begin
                - (intptr_t) pCmdList->command.ptr;
            const intptr_t previous_end = 
                (intptr_t) pCmdList->previous.end
                - (intptr_t) pCmdList->command.ptr;

            /* No running batch */
            if ((NULL != pCmdList->previous.begin)
                && (((offset_start >= previous_begin) 
                     && (offset_start <= previous_end))
                    || ((offset_end >= previous_begin) 
                        && (offset_end <= previous_end)))) {
                /* If current command list overlaps the last begin
                 * Force to reset
                 */
                xg47_Reset(pCmdList);
            }
            break;
        }
    }
}

static uint32_t getGEWorkedCmdHWAddr(const struct xg47_CmdList * pCmdList)
{
    return *pCmdList->scratch.ptr;
}


static void sendRemainder2DCommand(struct xg47_CmdList * pCmdList)
{
    /* If there are any pending commands in _bunch, emit the whole batch.
     */
    if (0x7f000000 != pCmdList->_bunch[0]) {
        emit_bunch(pCmdList);
    }
}


/**
 * Emit series of commands to write the batch end address to the scratch buffer.
 *
 * \note
 * Wouldn't it be easier to just write the end address to the software
 * scratch register (0x1f0, page 27 in 3D SPG)?
 */
static void addScratchBatch(struct xg47_CmdList * pCmdList)
{
    /* In-line command to write to ENG_DST_BASE, ENG_DESTXY, and ENG_DIMENSION
     */
    pCmdList->current.end[0]  = 0x7f413951;

    pCmdList->current.end[1]  = (0x1 << 24) 
        | (((CARD32)pCmdList->scratch.hw_addr >> 4) & 0x3fffff);

    pCmdList->current.end[2]  = (((CARD32)pCmdList->scratch.hw_addr & 0x1c000000) >> 13)
                             +((CARD32)pCmdList->scratch.hw_addr & 0xe0000000);
    pCmdList->current.end[3]  = 0x00010001;


    /* In-line command to write to ENG_DRAWINGFLAG, ENG_COMMAND, and
     * 0x78 (?).
     */
    pCmdList->current.end[4]  = 0x7f792529;

    /* Drawing Flag */
    pCmdList->current.end[5]  = 0x10000000; /* 28~2B */

    /* 24:Command; 25~26:Op Mode; 27:ROP3 */
    pCmdList->current.end[6]  = 0xcc008201; 

    pCmdList->current.end[7]  = pCmdList->command.hw_addr
        + ((intptr_t) pCmdList->previous.end
           - (intptr_t) pCmdList->command.ptr);

    /* In-line end command to write to FLUSH_ENGINE_ADDRESS.
     */
    pCmdList->current.end[8]  = 0xff000001;
    pCmdList->current.end[9]  = pCmdList->current.end[7];
    pCmdList->current.end[10] = 0x00000000;
    pCmdList->current.end[11] = 0x00000000;

    pCmdList->current.end += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;
    pCmdList->current.data_count += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;
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


/*
    |_________|______|_______|  ok
    B         G      L       E

    |_________|______|_______|  if roll back, will overwrite the cmdList
    B         L      G       E  So MUST wait till G execute to before L
*/
static void preventOverwriteCmdbuf(struct xg47_CmdList * pCmdList)
{
    /* Calculate the offset of the end of the last batch in the command
     * buffer.  This is "L" in the diagram above.
     */
    const intptr_t L = (intptr_t) pCmdList->command.ptr 
	- (intptr_t) pCmdList->previous.end;
    intptr_t G;

    do {
	/* Calculate the offset of the command that is currently being
	 * processed by the GE.  This is "G" in the diagram
	 */
	G = getGEWorkedCmdHWAddr(pCmdList) - pCmdList->command.hw_addr;
    } while (G > L);
}

static int submit2DBatch(struct xg47_CmdList * pCmdList)
{
    CARD32 beginHWAddr;
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
    submitInfo.id = pCmdList->_debugBeginID;

    XGIDebug(DBG_FUNCTION, "%s: calling ioctl XGI_IOCTL_SUBMIT_CMDLIST\n", 
             __func__);

#ifdef DUMP_COMMAND_BUFFER
    dumpCommandBuffer(pCmdList);
#endif

    err = drmCommandWrite(pCmdList->_fd, DRM_XGI_SUBMIT_CMDLIST,
                          &submitInfo, sizeof(submitInfo));
    if (!err) {
        pCmdList->previous = pCmdList->current;
        pCmdList->_debugBeginID++;
    }
    else {
        ErrorF("[2D] ioctl -- cmdList error (%d, %s)!\n",
               -err, strerror(-err));
    }

    XGIDebug(DBG_FUNCTION, "%s: exit\n", __func__);
    return err;
}
