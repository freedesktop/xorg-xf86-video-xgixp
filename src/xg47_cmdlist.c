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

struct xg47_CmdList
{
    enum xgi_batch_type _curBatchType;
    CARD32      _curBatchDataCount;         /* DWORDs */
    CARD32      _curBatchRequestSize;       /* DWORDs */
    uint32_t *  _curBatchBegin;             /* The begin of current batch. */
    uint32_t *  _curBatchDataBegin;         /* The begin of data */

    uint32_t *  _writePtr;                  /* current writing ptr */
    CARD32      _sendDataLength;            /* record the filled data size */

    uint32_t *    _cmdBufLinearStartAddr;
    uint32_t      _cmdBufHWStartAddr;
    unsigned long _cmdBufBusStartAddr;
    unsigned int  _cmdBufSize;            /* DWORDs */

    uint32_t *  _lastBatchBegin;        /* The begin of last batch. */
    uint32_t *  _lastBatchEnd;          /* The end of last batch. */
    enum xgi_batch_type _lastBatchType;         /* The type of the last workload batch. */
    CARD32      _debugBeginID;          /* write it at begin header as debug ID */

    /* 2d cmd holder */
    CARD32      _bunch[4];

    /* scratch pad addr, allocate at outside, pass these two parameter in */
    uint32_t *    _scratchPadLinearAddr;
    uint32_t      _scratchPadHWAddr;
    unsigned long _scratchPadBusAddr;

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
    list->_cmdBufSize = cmdBufSize;
    list->_fd = fd;

    if (!XGIPcieMemAllocate(pScrn,
                            list->_cmdBufSize * sizeof(CARD32),
			    & list->_cmdBufBusStartAddr,
			    & list->_cmdBufHWStartAddr,
			    (void **) & list->_cmdBufLinearStartAddr)) {
        XGIDebug(DBG_ERROR, "[DBG Error]Allocate CmdList buffer error!\n");
	goto err;
    }

    XGIDebug(DBG_CMDLIST, "cmdBuf VAddr=0x%p  HAddr=0x%p buffsize=0x%x\n",
	     list->_cmdBufLinearStartAddr, list->_cmdBufHWStartAddr, list->_cmdBufSize);

    if (!XGIPcieMemAllocate(pScrn,
                            4 * 1024,
                            & list->_scratchPadBusAddr,
                            & list->_scratchPadHWAddr,
                            (void **) & list->_scratchPadLinearAddr)) {
        XGIDebug(DBG_ERROR, "[DBG ERROR]Allocate Scratch Pad error!\n");

	goto err;
    }


    XGIDebug(DBG_CMDLIST, "[Malloc]Scratch VAddr=0x%p HAddr=0x%x\n",
           list->_scratchPadLinearAddr, list->_scratchPadHWAddr);

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
	if (s_pCmdList->_scratchPadBusAddr) {
	    XGIDebug(DBG_CMDLIST, "[DBG Free]Scratch VAddr=0x%x HAddr=0x%x\n",
		     s_pCmdList->_scratchPadLinearAddr, 
		     s_pCmdList->_scratchPadHWAddr);
	
	    XGIPcieMemFree(pScrn, 4 * 1024,
			   s_pCmdList->_scratchPadBusAddr,
			   s_pCmdList->_scratchPadLinearAddr);
	}
	
	if (s_pCmdList->_cmdBufBusStartAddr) {
	    XGIDebug(DBG_CMDLIST, "[DBG Free]cmdBuf VAddr=0x%x  HAddr=0x%x\n",
		     s_pCmdList->_cmdBufLinearStartAddr,
		     s_pCmdList->_cmdBufHWStartAddr);

	    XGIPcieMemFree(pScrn, s_pCmdList->_cmdBufSize * sizeof(CARD32),
			   s_pCmdList->_cmdBufBusStartAddr,
			   s_pCmdList->_cmdBufLinearStartAddr);
	}

	xfree(s_pCmdList);
    }
}

void xg47_Reset(struct xg47_CmdList *s_pCmdList)
{
    *(s_pCmdList->_scratchPadLinearAddr) = 0;
    s_pCmdList->_lastBatchBegin         = 0;
    s_pCmdList->_lastBatchEnd           = 0;
    s_pCmdList->_sendDataLength         = 0;
    s_pCmdList->_writePtr               = 0;
}

/* Implementation Part*/
static inline int submit2DBatch(struct xg47_CmdList * pCmdList);
static inline void sendRemainder2DCommand(struct xg47_CmdList * pCmdList);
static inline void reserveData(struct xg47_CmdList * pCmdList, size_t size);
static inline void addScratchBatch(struct xg47_CmdList * pCmdList);
static inline void linkToLastBatch(struct xg47_CmdList * pCmdList);

static inline void waitCmdListAddrAvailable(struct xg47_CmdList * pCmdList, CARD32* addrStart, CARD32* addrEnd);
static inline void preventOverwriteCmdbuf(struct xg47_CmdList * pCmdList);
static CARD32 getCurBatchBeginPort(struct xg47_CmdList * pCmdList);
static inline void triggerHWCommandList(struct xg47_CmdList * pCmdList, CARD32 triggerCounter);
static inline void waitForPCIIdleOnly(struct xg47_CmdList *);
static inline uint32_t getGEWorkedCmdHWAddr(const struct xg47_CmdList *);
static void dumpCommandBuffer(struct xg47_CmdList * pCmdList);

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

    if (size >= pCmdList->_cmdBufSize)
    {
        return 0;
    }

    if (NULL != pCmdList->_lastBatchEnd)
    {
         /* We have spare buffer after last command list. */
        if ((pCmdList->_lastBatchEnd + size) <=
            (pCmdList->_cmdBufLinearStartAddr + pCmdList->_cmdBufSize))
        {
            /* ASSERT_MSG(0 == (((CARD32*)pCmdList->_lastBatchEnd) & 0x0f),  */
            /*                 "Command List should be 4 Dwords alignment"); */
            pCmdList->_curBatchBegin = pCmdList->_lastBatchEnd;
        }
        else /* no spare space, must roll over */
        {
            preventOverwriteCmdbuf(pCmdList);
            pCmdList->_curBatchBegin = pCmdList->_cmdBufLinearStartAddr;
        }
    }
    else /* fresh */
    {
        pCmdList->_curBatchBegin = pCmdList->_cmdBufLinearStartAddr;
    }

    /* Prepare the begin address of next batch. */
    waitCmdListAddrAvailable(pCmdList, pCmdList->_curBatchBegin,
                             pCmdList->_curBatchBegin + size);

    pCmdList->_writePtr = pCmdList->_curBatchBegin;
    pCmdList->_curBatchRequestSize = size;

    /* Prepare next begin */
    memcpy(pCmdList->_writePtr, s_emptyBegin,
           AGPCMDLIST_BEGIN_SIZE * sizeof(CARD32));
    pCmdList->_writePtr += AGPCMDLIST_BEGIN_SIZE;
    pCmdList->_curBatchDataCount = AGPCMDLIST_BEGIN_SIZE;
    pCmdList->_curBatchType = BTYPE_2D;
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
    NewBatchSize =  pCmdList->_curBatchRequestSize       /* require command size */
                  + size                                 /* require data size.   */
                  + AGPCMDLIST_BEGIN_SIZE                /* We need a begin.     */
                  + AGPCMDLIST_2D_SCRATCH_CMD_SIZE;      /* 2D scratch size      */

    pulNewBatchEnd = pCmdList->_curBatchBegin + NewBatchSize;

    if (pulNewBatchEnd <= (pCmdList->_cmdBufLinearStartAddr + pCmdList->_cmdBufSize))
    {
        /* There is enought space after this batch    */
        /* Enlarge the batch size.                    */
        pCmdList->_curBatchRequestSize += size;
        pCmdList->_curBatchDataBegin = pCmdList->_writePtr;
    }
    else {
	/* If roll over, we should use new batch from the begin of
	 * command list. 
	 */
	uint32_t *const pOldBatchBegin = pCmdList->_curBatchBegin;
	const ptrdiff_t existCmdSize = pCmdList->_writePtr - pOldBatchBegin;

	pCmdList->_curBatchBegin = pCmdList->_cmdBufLinearStartAddr;
	pulNewBatchEnd           = pCmdList->_curBatchBegin + NewBatchSize;

	/* Move the midway commands to new home. */
	waitCmdListAddrAvailable(pCmdList,
				 pCmdList->_curBatchBegin,
				 pCmdList->_curBatchBegin + existCmdSize);
	memcpy(pCmdList->_curBatchBegin, pOldBatchBegin, existCmdSize * 4);
	pCmdList->_writePtr = pCmdList->_curBatchBegin + existCmdSize;
	pCmdList->_curBatchRequestSize += size;
	pCmdList->_curBatchDataBegin = pCmdList->_writePtr;
    }

    /* Wait for batch available. */
    waitCmdListAddrAvailable(pCmdList,
                             pCmdList->_curBatchDataBegin,
                             pCmdList->_curBatchDataBegin + NewBatchSize);

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

void xg47_SendGECommand(struct xg47_CmdList *pCmdList, CARD32 addr, CARD32 cmd)
{
    /* Encrypt the command for AGP. */
    CARD32 shift        = (pCmdList->_curBatchDataCount++) & 0x00000003;
    pCmdList->_bunch[0] |= (addr | 1) << (shift << 3);
    pCmdList->_bunch[shift + 1]  = cmd;

    /* Bunch finished, Send to HW. */
    if(2 == shift)
    {
    	pCmdList->_writePtr[0] = pCmdList->_bunch[0];
    	pCmdList->_writePtr[1] = pCmdList->_bunch[1];
    	pCmdList->_writePtr[2] = pCmdList->_bunch[2];
    	pCmdList->_writePtr[3] = pCmdList->_bunch[3];
    	pCmdList->_bunch[0]    = 0x7f000000;
    	pCmdList->_writePtr    += 4;
    	pCmdList->_curBatchDataCount++;
    }
}

void xg47_StartFillData(struct xg47_CmdList *pCmdList, CARD32 size)
{
    reserveData(pCmdList, size);
    sendRemainder2DCommand(pCmdList);

    pCmdList->_curBatchDataBegin = pCmdList->_writePtr;
    pCmdList->_writePtr++;
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
        memcpy(pCmdList->_writePtr, ptr, sizeAlign);
        /* DumpArray32(ptr, ulSizeAlign / 4);*/

        pCmdList->_writePtr         += sizeAlign / 4;
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

            *pCmdList->_writePtr        = remainder;
            pCmdList->_writePtr++;
            pCmdList->_sendDataLength   += 4;

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
    while (0 != ((intptr_t)pCmdList->_writePtr & 0x0f)) {
        *pCmdList->_writePtr++ = 0;
    }

    /* Correct the data length */
    *pCmdList->_curBatchDataBegin = 0xff000000 | (pCmdList->_sendDataLength/4);
    pCmdList->_curBatchDataCount += (CARD32)(pCmdList->_writePtr - pCmdList->_curBatchDataBegin);
}


static void waitCmdListAddrAvailable(struct xg47_CmdList * pCmdList,
				     CARD32* addrStart, CARD32* addrEnd)
{
    /* The loop of waiting for enough command list buffer */
    while (1) {
        /* Get the current runing batch address. */

	const uint32_t cmd_offset = getGEWorkedCmdHWAddr(pCmdList)
	    - pCmdList->_cmdBufHWStartAddr;
        const CARD32 *curGEWorkedCmdAddr =
	    (CARD32 *)(((uint8_t *) pCmdList->_cmdBufLinearStartAddr)
		       + cmd_offset);

        if(NULL != curGEWorkedCmdAddr)
        {
            if ((curGEWorkedCmdAddr < addrStart) || /* cmdlist is fresh */
                (curGEWorkedCmdAddr > addrEnd))     /* cmdlist already rolled over. Current batch does not overlay the buffer. */
            {
                /* There is enough memory at the begin of command list. */
                break;
            }
        }
        else
        {
            /* No running batch */
            if ((NULL != pCmdList->_lastBatchBegin) 
		&& (addrStart >= pCmdList->_lastBatchBegin) 
		&& (addrEnd <= pCmdList->_lastBatchBegin)) {
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
    return *pCmdList->_scratchPadLinearAddr;
}


static void sendRemainder2DCommand(struct xg47_CmdList * pCmdList)
{
    /* ASSERTDD(BTYPE_2D == m_btBatchType, "Only 2D batch can use SendCommand!");*/
    if (0x7f000000 != pCmdList->_bunch[0])
    {
        pCmdList->_writePtr[0]       = pCmdList->_bunch[0];
        pCmdList->_writePtr[1]       = pCmdList->_bunch[1];
        pCmdList->_writePtr[2]       = pCmdList->_bunch[2];
        pCmdList->_writePtr[3]       = pCmdList->_bunch[3];
        pCmdList->_bunch[0]          = 0x7f000000;
        pCmdList->_writePtr          += 4;
        pCmdList->_curBatchDataCount = (pCmdList->_curBatchDataCount + 3) & ~3;
    }
}

static void addScratchBatch(struct xg47_CmdList * pCmdList)
{
    /*because we add 2D scratch directly at the end of this batch*/
    /*ASSERT(BTYPE_2D == pCmdList->_curBatchType);*/

    pCmdList->_writePtr[0]  = 0x7f413951;

	/* Jong 11/08/2006; seems have a bug for base=64MB=0x4000000 */
	/* base >> 4 = 0x400000; 0x400000 & 0x3fffff = 0x0 */
	/* Thus, base address must be less than 64MB=0x4000000 */
    pCmdList->_writePtr[1]  = (0x1 << 0x18) + (((CARD32)pCmdList->_scratchPadHWAddr >> 4) & 0x3fffff);

    pCmdList->_writePtr[2]  = (((CARD32)pCmdList->_scratchPadHWAddr & 0x1c000000) >> 13)
                             +((CARD32)pCmdList->_scratchPadHWAddr & 0xe0000000);
    pCmdList->_writePtr[3]  = 0x00010001;

    pCmdList->_writePtr[4]  = 0x7f792529;

	/* Drawing Flag */
    pCmdList->_writePtr[5]  = 0x10000000; /* 28~2B */

	/* 24:Command; 25~26:Op Mode; 27:ROP3 */
    pCmdList->_writePtr[6]  = 0xcc008201; 

	/* Jong 06/15/2006; this value is checked at waitfor2D() */ /* 78~7B */
    pCmdList->_writePtr[7]  = pCmdList->_cmdBufHWStartAddr
	+ ((intptr_t) pCmdList->_lastBatchEnd
	   - (intptr_t) pCmdList->_cmdBufLinearStartAddr);

    pCmdList->_writePtr[8]  = 0xff000001;
    pCmdList->_writePtr[9]  = pCmdList->_writePtr[7];
    pCmdList->_writePtr[10] = 0x00000000;
    pCmdList->_writePtr[11] = 0x00000000;

    pCmdList->_writePtr += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;
	pCmdList->_curBatchDataCount += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;

	/* Jong 06/29/2006; demark */
	dumpCommandBuffer(pCmdList);
}

static void linkToLastBatch(struct xg47_CmdList * pCmdList)
{

    CARD32 beginHWAddr;
    CARD32 beginPort;
    /*batch begin addr should be 4 Dwords alignment.*/
    /*ASSERT(0 == (((CARD32)pCmdList->_curBatchBegin) & 0x0f);*/

    if (0 == pCmdList->_curBatchDataCount)
    {
        /*is it reasonable?*/
        return;
    }

    /*batch end addr should be 4 Dwords alignment*/
    /*ASSERT(0 == (((CARD32)pCmdList->_writePtr) & 0x0f));*/

    beginHWAddr = pCmdList->_cmdBufHWStartAddr
	+ ((intptr_t) pCmdList->_curBatchBegin
	   - (intptr_t) pCmdList->_cmdBufLinearStartAddr);

    /* Which begin we should send.*/
    beginPort = getCurBatchBeginPort(pCmdList);

    if (NULL == pCmdList->_lastBatchBegin)
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
        pCmdPort[1] = BEGIN_LINK_ENABLE_MASK + pCmdList->_curBatchDataCount;
        pCmdPort[2] = (beginHWAddr >> 4);
        pCmdPort[3] = 0;
    }
    else
    {
        /* Encode BEGIN */
        pCmdList->_lastBatchBegin[1] = BEGIN_LINK_ENABLE_MASK + pCmdList->_curBatchDataCount;
        pCmdList->_lastBatchBegin[2] = (beginHWAddr >> 4);
        pCmdList->_lastBatchBegin[3] = 0;

        /*Fixme, Flush cache*/
        /*_mm_mfence();*/

        pCmdList->_lastBatchBegin[0] = (beginPort<<22) + (BEGIN_VALID_MASK) + pCmdList->_debugBeginID;

        triggerHWCommandList(pCmdList, 1);
    }

    pCmdList->_lastBatchType  = pCmdList->_curBatchType;
    pCmdList->_lastBatchBegin = pCmdList->_curBatchBegin;
    pCmdList->_lastBatchEnd   = pCmdList->_writePtr;
    pCmdList->_debugBeginID   = (pCmdList->_debugBeginID + 1) & 0xFFFF;
}

CARD32 getCurBatchBeginPort(struct xg47_CmdList * pCmdList)
{
    /* Convert the batch type to begin port ID */
    switch(pCmdList->_curBatchType)
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

void dumpCommandBuffer(struct xg47_CmdList * pCmdList)
{
    unsigned i;

    XGIDebug(DBG_FUNCTION,"Entering dumpCommandBuffer\n");

    for (i = 0; 
	 & pCmdList->_cmdBufLinearStartAddr[i] < pCmdList->_writePtr;
	 i++) {
        if (i % 4 == 0) {
            XGIDebug(DBG_CMD_BUFFER, "\n%08p ",
		     pCmdList->_cmdBufLinearStartAddr + i);
        }

	XGIDebug(DBG_CMD_BUFFER, "%08x ", 
		 *(pCmdList->_cmdBufLinearStartAddr + i));

        if ((i+1) % 4 == 0)
        {
            XGIDebug(DBG_CMD_BUFFER, "\n");
        }

    }

    XGIDebug(DBG_FUNCTION,"Leaving dumpCommandBuffer\n");
}


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
    const intptr_t L = (intptr_t) pCmdList->_cmdBufLinearStartAddr 
	- (intptr_t) pCmdList->_lastBatchEnd;
    intptr_t G;

    do {
	/* Calculate the offset of the command that is currently being
	 * processed by the GE.  This is "G" in the diagram
	 */
	G = getGEWorkedCmdHWAddr(pCmdList) - pCmdList->_cmdBufHWStartAddr;
    } while (G > L);
}

static int submit2DBatch(struct xg47_CmdList * pCmdList)
{
    CARD32 beginHWAddr;
    CARD32 beginPort;
    struct xgi_cmd_info submitInfo;
    int retval = -1;

    XGIDebug(DBG_FUNCTION, "%s: enter\n", __func__);

    if (0 == pCmdList->_curBatchDataCount) {
        /*is it reasonable? */
        return retval;
    }

    beginHWAddr = pCmdList->_cmdBufHWStartAddr
	+ ((intptr_t) pCmdList->_curBatchBegin
	   - (intptr_t) pCmdList->_cmdBufLinearStartAddr);

    /* Which begin we should send. */
    beginPort = getCurBatchBeginPort(pCmdList);

    submitInfo._firstBeginType = pCmdList->_curBatchType;
    submitInfo._firstBeginAddr = beginHWAddr;
    submitInfo._lastBeginAddr = beginHWAddr;
    submitInfo._firstSize = pCmdList->_curBatchDataCount;
    submitInfo._curDebugID = pCmdList->_debugBeginID;
    submitInfo._beginCount = 1;

    if (NULL == pCmdList->_lastBatchBegin) {
	XGIDebug(DBG_FUNCTION, "%s: calling waitForPCIIdleOnly\n", __func__);
        waitForPCIIdleOnly(pCmdList);
    }

    XGIDebug(DBG_FUNCTION, "%s: calling ioctl XGI_IOCTL_SUBMIT_CMDLIST\n", 
	     __func__);

    /* Jong 05/24/2006; cause system hang on Gateway platform but works on others */
    retval = drmCommandWrite(pCmdList->_fd, DRM_XGI_SUBMIT_CMDLIST,
			     &submitInfo, sizeof(submitInfo));


    XGIDebug(DBG_FUNCTION, "%s: calling waitFor2D\n", __func__);
    waitfor2D(pCmdList); 

    if (retval != -1) {
        pCmdList->_lastBatchType  = pCmdList->_curBatchType;
        pCmdList->_lastBatchBegin = pCmdList->_curBatchBegin;
        pCmdList->_lastBatchEnd   = pCmdList->_writePtr;
        pCmdList->_debugBeginID   = (pCmdList->_debugBeginID + 1) & 0xFFFF;
    }
    else {
        ErrorF("[2D] ioctl -- cmdList error!\n");
    }

    XGIDebug(DBG_FUNCTION, "%s: exit\n", __func__);
    return retval;
}

static inline void waitfor2D(struct xg47_CmdList * pCmdList)
{
    uint32_t lastBatchEndHW = pCmdList->_cmdBufHWStartAddr
	+ ((intptr_t) pCmdList->_lastBatchEnd
	   - (intptr_t) pCmdList->_cmdBufLinearStartAddr);

	/* Jong 05/25/2006 */
	int WaitCount=0;
	/* if(g_bFirst == 1)
	{
		ErrorF("Jong-07032006-waitfor2D()-Begin loop\n");
		XGIDumpRegisterValue(g_pScreen);
	} */

    if (lastBatchEndHW >=0)
    {
	    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] waitfor2D()-Begin loop\n");
        while(lastBatchEndHW != (CARD32) getGEWorkedCmdHWAddr(pCmdList))
        {
            /*loop*/
			/* Jong 07/03/2006
			   Why just need one time to output message and get working ?*/
			/*if(g_bFirst==1)
			{
				ErrorF("[DBG-Jong-07032006] waitfor2D()-in loop\n");
				XGIDumpRegisterValue(g_pScreen);
			}*/

			if(g_bFirst==1)
				usleep(1); 

			/* Jong 05/25/2006 */
			/* if(WaitCount >= 1000) break;
			else
				WaitCount++; */
        }

		/* Jong 05/25/2006 */
		/*g_bFirst++;
		if(g_bFirst <= 10)*/
		if(g_bFirst == 1)
		{
			g_bFirst=0;
			/*ErrorF("Jong-07032006-waitfor2D()-End loop\n");
			XGIDumpRegisterValue(g_pScreen);*/
		} 

	    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] waitfor2D()-End loop\n");
    }
}
