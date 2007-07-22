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

    /* MMIO base */
    uint32_t *  _mmioBase;

    /* fd number */
    int		_fd;
};
static inline void waitfor2D(struct xg47_CmdList * pCmdList);

/* Jong 07/03/2006 */
int g_bFirst=1;
extern ScreenPtr g_pScreen;

struct xg47_CmdList *
xg47_Initialize(ScrnInfoPtr pScrn, CARD32 cmdBufSize, CARD32 *mmioBase, int fd)
{
    struct xg47_CmdList *list = xnfcalloc(sizeof(struct xg47_CmdList), 1);

    list->_mmioBase = mmioBase;
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

    XGIDebug(DBG_CMDLIST, "mmioBase = %x\n", list->_mmioBase);
    
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
static inline int submit2DBatch(struct xg47_CmdList * pCmdList);
static inline void sendRemainder2DCommand(struct xg47_CmdList * pCmdList);
static inline void reserveData(struct xg47_CmdList * pCmdList, size_t size);
static inline void addScratchBatch(struct xg47_CmdList * pCmdList);
static inline void linkToLastBatch(struct xg47_CmdList * pCmdList);

static void waitCmdListAddrAvailable(struct xg47_CmdList * pCmdList,
    const void * addrStart, const void * addrEnd);

static inline void preventOverwriteCmdbuf(struct xg47_CmdList * pCmdList);
static CARD32 getCurBatchBeginPort(struct xg47_CmdList * pCmdList);
static inline void triggerHWCommandList(struct xg47_CmdList * pCmdList, CARD32 triggerCounter);
static inline void waitForPCIIdleOnly(struct xg47_CmdList *);
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

/*
    return: 1 -- success 0 -- false
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
            /* ASSERT_MSG(0 == (((CARD32*)pCmdList->previous.end) & 0x0f),  */
            /*                 "Command List should be 4 Dwords alignment"); */
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
    pCmdList->_bunch[0]     = 0x7f000000;

    XGIDebug(DBG_CMDLIST, "[DEBUG] Leave beginCmdList.\n");
    return 1;
}


/*
    parameter: size -- length of space in bytes
*/
static void reserveData(struct xg47_CmdList * pCmdList, size_t size)
{
    unsigned int NewBatchSize;
    uint32_t *pulNewBatchEnd;

    /* Size should be 4 dwords alignment (0xff000000 | data length), data 0,
     * data 1 .... 
     */
    size = ((size + 0x04 + 0x0f) & ~0x0f) / 4;

    /* Check for enlarging this batch to accomodate Data */
    NewBatchSize =  pCmdList->current.request_size       /* require command size */
                  + size                                 /* require data size.   */
                  + AGPCMDLIST_BEGIN_SIZE                /* We need a begin.     */
                  + AGPCMDLIST_2D_SCRATCH_CMD_SIZE;      /* 2D scratch size      */

    pulNewBatchEnd = pCmdList->current.begin + NewBatchSize;

    if (pulNewBatchEnd <= (pCmdList->command.ptr + pCmdList->command.size))
    {
        /* There is enought space after this batch    */
        /* Enlarge the batch size.                    */
        pCmdList->current.request_size += size;
        pCmdList->current.data_begin = pCmdList->current.end;
    }
    else {
	/* If roll over, we should use new batch from the begin of
	 * command list. 
	 */
	uint32_t *const pOldBatchBegin = pCmdList->current.begin;
	const ptrdiff_t existCmdSize = pCmdList->current.end - pOldBatchBegin;

	pCmdList->current.begin = pCmdList->command.ptr;
	pulNewBatchEnd           = pCmdList->current.begin + NewBatchSize;

	/* Move the midway commands to new home. */
	waitCmdListAddrAvailable(pCmdList,
				 pCmdList->current.begin,
				 pCmdList->current.begin + existCmdSize);
	memcpy(pCmdList->current.begin, pOldBatchBegin, existCmdSize * 4);
	pCmdList->current.end = pCmdList->current.begin + existCmdSize;
	pCmdList->current.request_size += size;
	pCmdList->current.data_begin = pCmdList->current.end;
    }

    /* Wait for batch available. */
    waitCmdListAddrAvailable(pCmdList,
                             pCmdList->current.data_begin,
                             pCmdList->current.data_begin + NewBatchSize);

}



void xg47_EndCmdList(struct xg47_CmdList *pCmdList)
{
    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-1\n");
    sendRemainder2DCommand(pCmdList);

    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-2\n");
    addScratchBatch(pCmdList);

    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-3\n");
/*    linkToLastBatch(pCmdList);*/
    submit2DBatch(pCmdList);
}

void emit_bunch(struct xg47_CmdList *pCmdList)
{
    pCmdList->current.end[0] = pCmdList->_bunch[0];
    pCmdList->current.end[1] = pCmdList->_bunch[1];
    pCmdList->current.end[2] = pCmdList->_bunch[2];
    pCmdList->current.end[3] = pCmdList->_bunch[3];
    pCmdList->_bunch[0] = 0x7f000000;
    pCmdList->current.end += 4;
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
        pCmdList->current.data_count++;
    }
}

void xg47_StartFillData(struct xg47_CmdList *pCmdList, CARD32 size)
{
    reserveData(pCmdList, size);
    sendRemainder2DCommand(pCmdList);

    pCmdList->current.data_begin = pCmdList->current.end;
    pCmdList->current.end++;
    pCmdList->_sendDataLength = 0;
}

/**
 * \param width  Width in bytes.
 * \param delta  Source pitch, in bytes.
 */
void xg47_FillData(struct xg47_CmdList *pCmdList,
		   const unsigned char *ptr,
		   unsigned width, int delta, unsigned height)
{
    const unsigned sizeAlign = width & ~0x03;

    /* fill data to the cmdbatch
     * write 1 dword per time
     * pack the data to 4 dwords align
     * ...
     */
    while(height--)
    {
        /* ASSERTDD((m_ulDataLength - m_ulSendDataLen) >= ulWidth, "Reserved size is not enough!");*/
        /* m_pulDataStartAddr = (ULONG*)((m_pulDataStartAddr) & m_ulDataPortMask);*/
        /* m_pulDataStartAddr = (ULONG*)((m_pulDataStartAddr) | m_ulDataPortOr);*/

        /* Next line start address */
        CARD32 sizeRem = 0;
        const unsigned char *ptrNext = ptr + delta;

        /* ????SSEable????? */
        memcpy(pCmdList->current.end, ptr, sizeAlign);
        /* DumpArray32(ptr, ulSizeAlign / 4);*/

        pCmdList->current.end         += sizeAlign / 4;
        pCmdList->_sendDataLength   += sizeAlign;
        ptr                         += sizeAlign;

        sizeRem     = width - sizeAlign;

        if(0 != sizeRem)
        {
            CARD32 remainder   = 0;
            int i;

            for (i=0; i < sizeRem; i++)
            {
                remainder += (*ptr++) << (i*8);
            }

            *pCmdList->current.end = remainder;
            pCmdList->current.end++;
            pCmdList->_sendDataLength += 4;

            /* Dump to file for debug */
            /* DumpData32(0x10000, ulRemainder);*/
        }
        ptr = ptrNext;
    }
    /* then call endCmdList() outside */
}


void xg47_SubmitData(struct xg47_CmdList *pCmdList)
{
    /* ASSERTDD(m_ulSendDataLen <= m_ulDataLength, "Reserved size is not enough!");*/
    /* ASSERTDD(DATAFILL_INBATCH_BEGIN != m_DataFillMode, "Did not fill data after reserve.");*/

    /* Pad the data to 128 bit alignment. */
    while (0 != ((intptr_t)pCmdList->current.end & 0x0f)) {
        *pCmdList->current.end++ = 0;
    }

    /* Correct the data length */
    *pCmdList->current.data_begin = 0xff000000 | (pCmdList->_sendDataLength/4);
    pCmdList->current.data_count += (CARD32)(pCmdList->current.end - pCmdList->current.data_begin);
}


void waitCmdListAddrAvailable(struct xg47_CmdList * pCmdList,
                              const void * addrStart, const void * addrEnd)
{
    /* Offsets, in bytes, from the start of the command buffer to the start
     * and end of the proposed range.
     */
    const intptr_t offset_start = (intptr_t) pCmdList->command.ptr -
        (intptr_t) addrStart;
    const intptr_t offset_end = (intptr_t) pCmdList->command.ptr -
        (intptr_t) addrEnd;

    /* The loop of waiting for enough command list buffer */
    while (1) {
        /* Get the current runing batch address. */

        const uint32_t hw_addr = getGEWorkedCmdHWAddr(pCmdList);
        const uint32_t cmd_offset = hw_addr - pCmdList->command.hw_addr;

        if (hw_addr != 0) {
            /* If cmdlist is fresh or cmdlist already rolled over, current
             * batch does not overlay the buffer. 
             */
            if ((cmd_offset < offset_start) || (cmd_offset > offset_end))
            {
                /* There is enough memory at the begin of command list. */
                break;
            }
        }
        else {
            /* No running batch */
            if ((NULL != pCmdList->previous.begin) 
                && (((addrStart >= pCmdList->previous.begin) 
                     && (addrStart <= pCmdList->previous.end))
                    || ((addrEnd >= pCmdList->previous.begin) 
                        && (addrEnd <= pCmdList->previous.end)))) {
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
    /* ASSERTDD(BTYPE_2D == m_btBatchType, "Only 2D batch can use SendCommand!");*/
    if (0x7f000000 != pCmdList->_bunch[0]) {
        emit_bunch(pCmdList);
        pCmdList->current.data_count = (pCmdList->current.data_count + 3) & ~3;
    }
}

static void addScratchBatch(struct xg47_CmdList * pCmdList)
{
    /*because we add 2D scratch directly at the end of this batch*/
    /*ASSERT(BTYPE_2D == pCmdList->current.type);*/

    pCmdList->current.end[0]  = 0x7f413951;

	/* Jong 11/08/2006; seems have a bug for base=64MB=0x4000000 */
	/* base >> 4 = 0x400000; 0x400000 & 0x3fffff = 0x0 */
	/* Thus, base address must be less than 64MB=0x4000000 */
    pCmdList->current.end[1]  = (0x1 << 0x18) + (((CARD32)pCmdList->scratch.hw_addr >> 4) & 0x3fffff);

    pCmdList->current.end[2]  = (((CARD32)pCmdList->scratch.hw_addr & 0x1c000000) >> 13)
                             +((CARD32)pCmdList->scratch.hw_addr & 0xe0000000);
    pCmdList->current.end[3]  = 0x00010001;

    pCmdList->current.end[4]  = 0x7f792529;

	/* Drawing Flag */
    pCmdList->current.end[5]  = 0x10000000; /* 28~2B */

	/* 24:Command; 25~26:Op Mode; 27:ROP3 */
    pCmdList->current.end[6]  = 0xcc008201; 

	/* Jong 06/15/2006; this value is checked at waitfor2D() */ /* 78~7B */
    pCmdList->current.end[7]  = pCmdList->command.hw_addr
	+ ((intptr_t) pCmdList->previous.end
	   - (intptr_t) pCmdList->command.ptr);

    pCmdList->current.end[8]  = 0xff000001;
    pCmdList->current.end[9]  = pCmdList->current.end[7];
    pCmdList->current.end[10] = 0x00000000;
    pCmdList->current.end[11] = 0x00000000;

    pCmdList->current.end += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;
	pCmdList->current.data_count += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;
}

static void linkToLastBatch(struct xg47_CmdList * pCmdList)
{

    CARD32 beginHWAddr;
    CARD32 beginPort;
    /*batch begin addr should be 4 Dwords alignment.*/
    /*ASSERT(0 == (((CARD32)pCmdList->current.begin) & 0x0f);*/

    if (0 == pCmdList->current.data_count)
    {
        /*is it reasonable?*/
        return;
    }

    /*batch end addr should be 4 Dwords alignment*/
    /*ASSERT(0 == (((CARD32)pCmdList->current.end) & 0x0f));*/

    beginHWAddr = pCmdList->command.hw_addr
	+ ((intptr_t) pCmdList->current.begin
	   - (intptr_t) pCmdList->command.ptr);

    /* Which begin we should send.*/
    beginPort = getCurBatchBeginPort(pCmdList);

    if (NULL == pCmdList->previous.begin)
    {
        CARD32* pCmdPort;
        /* Issue PCI begin */

        /* Wait for GE Idle. */
        waitForPCIIdleOnly(pCmdList);

        pCmdPort = (CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + beginPort);

        /* Enable PCI Trigger Mode */
        *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS)) =
                (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
                M2REG_CLEAR_COUNTERS_MASK |
                0x08 |
                M2REG_PCI_TRIGGER_MODE_MASK;

        *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS)) =
                (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
                0x08 |
                M2REG_PCI_TRIGGER_MODE_MASK;

        /* Send PCI begin command */
        pCmdPort[0] = (beginPort<<22) + (BEGIN_VALID_MASK) + pCmdList->_debugBeginID;
        pCmdPort[1] = BEGIN_LINK_ENABLE_MASK + pCmdList->current.data_count;
        pCmdPort[2] = (beginHWAddr >> 4);
        pCmdPort[3] = 0;
    }
    else
    {
        /* Encode BEGIN */
        pCmdList->previous.begin[1] = BEGIN_LINK_ENABLE_MASK + pCmdList->current.data_count;
        pCmdList->previous.begin[2] = (beginHWAddr >> 4);
        pCmdList->previous.begin[3] = 0;

        /*Fixme, Flush cache*/
        /*_mm_mfence();*/

        pCmdList->previous.begin[0] = (beginPort<<22) + (BEGIN_VALID_MASK) + pCmdList->_debugBeginID;

        triggerHWCommandList(pCmdList, 1);
    }

    pCmdList->previous = pCmdList->current;
    pCmdList->_debugBeginID = (pCmdList->_debugBeginID + 1) & 0xFFFF;
}

CARD32 getCurBatchBeginPort(struct xg47_CmdList * pCmdList)
{
    /* Convert the batch type to begin port ID */
    switch(pCmdList->current.type)
    {
    case BTYPE_2D:
        return 0x30;
    case BTYPE_3D:
        return 0x40;
    case BTYPE_FLIP:
        return 0x50;
    case BTYPE_CTRL:
        return 0x20;
    default:
        /*ASSERT(0);*/
		return 0xff;
    }
}

static void triggerHWCommandList(struct xg47_CmdList * pCmdList, CARD32 triggerCounter)
{
    static CARD32 s_triggerID = 1;

    XGIDebug(DBG_CMD_BUFFER, "[DBG]Before trigger [2800]=%0x\n", *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG)) );

    /*Fixme, currently we just trigger one time */
    while (triggerCounter--)
    {
        *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + M2REG_PCI_TRIGGER_REGISTER_ADDRESS))
        = 0x05000000 + (0xffff & s_triggerID++);
    }

    XGIDebug(DBG_CMD_BUFFER, "[DBG]After trigger [2800]=%08x\n", *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG)) );
}

static void waitForPCIIdleOnly(struct xg47_CmdList *s_pCmdList)
{
    volatile CARD32* v3DStatus;
    int idleCount = 0;

    v3DStatus = (volatile CARD32*) ((unsigned char*)s_pCmdList->_mmioBase + WHOLD_GE_STATUS);
    while(idleCount < 5)
    {
        if ((*v3DStatus) & IDLE_MASK)
        {
            idleCount = 0;
        }
        else
        {
            idleCount ++;
        }
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
    CARD32 beginPort;
    struct xgi_cmd_info submitInfo;
    int err;

    XGIDebug(DBG_FUNCTION, "%s: enter\n", __func__);

    if (0 == pCmdList->current.data_count) {
        return 0;
    }

    beginHWAddr = pCmdList->command.hw_addr
	+ ((intptr_t) pCmdList->current.begin
	   - (intptr_t) pCmdList->command.ptr);

    /* Which begin we should send. */
    beginPort = getCurBatchBeginPort(pCmdList);

    submitInfo.type = pCmdList->current.type;
    submitInfo.hw_addr = beginHWAddr;
    submitInfo.size = pCmdList->current.data_count;
    submitInfo.id = pCmdList->_debugBeginID;

    if (NULL == pCmdList->previous.begin) {
	XGIDebug(DBG_FUNCTION, "%s: calling waitForPCIIdleOnly\n", __func__);
        waitForPCIIdleOnly(pCmdList);
    }

    XGIDebug(DBG_FUNCTION, "%s: calling ioctl XGI_IOCTL_SUBMIT_CMDLIST\n", 
             __func__);

#ifdef DUMP_COMMAND_BUFFER
    dumpCommandBuffer(pCmdList);
#endif

    err = drmCommandWrite(pCmdList->_fd, DRM_XGI_SUBMIT_CMDLIST,
                          &submitInfo, sizeof(submitInfo));


    XGIDebug(DBG_FUNCTION, "%s: calling waitFor2D\n", __func__);
    waitfor2D(pCmdList); 

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

static inline void waitfor2D(struct xg47_CmdList * pCmdList)
{
    const uint32_t lastBatchEndHW = pCmdList->command.hw_addr
	+ ((intptr_t) pCmdList->previous.end
	   - (intptr_t) pCmdList->command.ptr);

    if (lastBatchEndHW >=0) {
	XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] waitfor2D()-Begin loop\n");
        while (lastBatchEndHW != (CARD32) getGEWorkedCmdHWAddr(pCmdList)) {
	    if(g_bFirst==1)
		usleep(1);
        }

	if (g_bFirst == 1) {
	    g_bFirst = 0;
	} 

	XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] waitfor2D()-End loop\n");
    }
}
