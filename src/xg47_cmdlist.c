/* xg47_cmdlist.c */

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>

#include "xf86.h"
#include "xg47_regs.h"
#include "xgi_driver.h"
#include "xg47_cmdlist.h"
#include "xgi_debug.h"

static CmdList s_cmdList;
static CmdList* s_pCmdList = &s_cmdList;

static inline int beginCmdList(CmdList* pCmdList, CARD32 size);
static inline void endCmdList(CmdList* pCmdList);
static inline void sendGECommand(CmdList* pCmdList, CARD32 addr, CARD32 cmd);
static inline void startFillData(CmdList* pCmdList, CARD32 size);
static inline void fillData(CmdList * pCmdList,
                            unsigned char * ptr,
                            CARD32 width,          /* bytes */
                            long int delta,        /* bytes */
                            CARD32 height);
static inline void submitData(CmdList* pCmdList);
static inline void waitfor2D(CmdList* pCmdList);

/* Jong 07/03/2006 */
int g_bFirst=1;
extern ScreenPtr g_pScreen;

/*Interface Part*/

int BeginCmdList(CARD32 size)
{
    int result;
    result = beginCmdList(s_pCmdList, size);
    return result;
}

void EndCmdList()
{
    endCmdList(s_pCmdList);
}

void SendGECommand(CARD32 addr, CARD32 cmd)
{
    /* addr is offset of 2D Engine address. xx must be 4byte aligned of  21xx */
    sendGECommand(s_pCmdList, addr & 0xfc, cmd);
}

void StartFillData(CARD32 size)
{
    startFillData(s_pCmdList, size);
}

void SubmitData()
{
    /* packed data to 128 bits align */
    submitData(s_pCmdList);
}

void FillData(unsigned char* ptr,
              unsigned long width,
              long int delta,
              unsigned long height)
{
    fillData(s_pCmdList, ptr, width, delta, height);
}

void Initialize(CARD32* cmdBufLinearStartAddr,
                CARD32* cmdBufHWStartAddr,
                CARD32  cmdBufSize,
                CARD32* scratchPadLinearAddr,
                CARD32* scracthPadHWAddr,
                CARD32* mmioBase,
                int		fd)
{
    s_pCmdList->_cmdBufLinearStartAddr  = cmdBufLinearStartAddr;
    s_pCmdList->_cmdBufHWStartAddr      = cmdBufHWStartAddr;
    s_pCmdList->_cmdBufSize             = cmdBufSize;
    s_pCmdList->_scratchPadLinearAddr   = scratchPadLinearAddr;
    s_pCmdList->_scratchPadHWAddr       = scracthPadHWAddr;
    s_pCmdList->_mmioBase               = mmioBase;
    *(s_pCmdList->_scratchPadLinearAddr) = 0;
    s_pCmdList->_lastBatchBegin         = 0;
    s_pCmdList->_lastBatchEnd           = 0;
    s_pCmdList->_sendDataLength         = 0;
    s_pCmdList->_writePtr               = 0;
    s_pCmdList->_fd						= fd;

    XGIDebug(DBG_CMDLIST, "\t pXGI->cmdList.cmdBufLinearStartAddr = %x\n"
       "pXGI->cmdList.cmdBufHWStartAddr = %x\n"
       "pXGI->cmdList.cmdBufSize = %x \n"
       "pXGI->cmdList.scratchPadLinearAddr = %x\n"
       "pXGI->cmdList.scracthPadHWAddr = %x\n"
       "pXGI->cmdList.mmioBase = %x\n",
       s_pCmdList->_cmdBufLinearStartAddr,
       s_pCmdList->_cmdBufHWStartAddr,
       s_pCmdList->_cmdBufSize,
       s_pCmdList->_scratchPadLinearAddr,
       s_pCmdList->_scratchPadHWAddr,
       s_pCmdList->_mmioBase);
}

void Cleanup()
{
}

void Reset()
{
    *(s_pCmdList->_scratchPadLinearAddr) = 0;
    s_pCmdList->_lastBatchBegin         = 0;
    s_pCmdList->_lastBatchEnd           = 0;
    s_pCmdList->_sendDataLength         = 0;
    s_pCmdList->_writePtr               = 0;
}

/* Implementation Part*/
static inline int submit2DBatch(CmdList* pCmdList);
static inline void sendRemainder2DCommand(CmdList* pCmdList);
static inline void reserveData(CmdList* pCmdList, CARD32 size);
static inline void addScratchBatch(CmdList* pCmdList);
static inline void linkToLastBatch(CmdList* pCmdList);

static inline void waitCmdListAddrAvailable(CmdList* pCmdList, CARD32* addrStart, CARD32* addrEnd);
static inline void preventOverwriteCmdbuf(CmdList* pCmdList);
static inline void reset(CmdList* pCmdList);
static CARD32 getCurBatchBeginPort(CmdList* pCmdList);
static inline void triggerHWCommandList(CmdList* pCmdList, CARD32 triggerCounter);
static inline void waitForPCIIdleOnly();
static inline CARD32* getGEWorkedCmdHWAddr(CmdList* pCmdList);
static void dumpCommandBuffer(CmdList* pCmdList);

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
static int beginCmdList(CmdList* pCmdList, CARD32 size)
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
static void reserveData(CmdList* pCmdList, CARD32 size)
{
    /* ASSERTDD(DATAFILL_NOFILL == m_DataFillMode, "Last Data was not submited!");  */
    /* ASSERTDD(NULL != m_pulCurBatchBegin, "What's wrong?");                       */
    CARD32  NewBatchSize;
    CARD32* pulNewBatchEnd;
    /*Size should be 4 dwords alignment (0xff000000 | data length), data 0, data 1 .... */
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
    else
    {
        /* HW guys do NOT recommemd to use out-batch mode. */
        if (1)
        {
            // If roll over, we should use new batch from the begin of command list.
            CARD32* pOldBatchBegin;
            CARD32  existCmdSize;
            pOldBatchBegin           = pCmdList->_curBatchBegin;
            pCmdList->_curBatchBegin = pCmdList->_cmdBufLinearStartAddr;
            pulNewBatchEnd           = pCmdList->_curBatchBegin + NewBatchSize;
            existCmdSize             = pCmdList->_writePtr - pOldBatchBegin;
            //ASSERTDD(NULL != m_pulCurBatchBegin, "NO command list buffer?");
            //ASSERTDD((pulOldBatchBegin <= m_ulBufferAddr) &&
            //         (m_ulBufferAddr - pulOldBatchBegin <= m_ulRequestSize), "Out of boundary!");

            // Move the midway commands to new home.
            waitCmdListAddrAvailable(pCmdList,
                                     pCmdList->_curBatchBegin,
                                     pCmdList->_curBatchBegin + existCmdSize);
            memcpy(pCmdList->_curBatchBegin, pOldBatchBegin, existCmdSize * 4);
            pCmdList->_writePtr = pCmdList->_curBatchBegin + existCmdSize;
            pCmdList->_curBatchRequestSize += size;
            pCmdList->_curBatchDataBegin = pCmdList->_writePtr;
        }
        else
        {
            // If roll over, we should use new btach to

        }
    }

    // Wait for batch available.
    waitCmdListAddrAvailable(pCmdList,
                             pCmdList->_curBatchDataBegin,
                             pCmdList->_curBatchDataBegin + NewBatchSize);

}



static void endCmdList(CmdList* pCmdList)
{
    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-1\n");
    sendRemainder2DCommand(pCmdList);

    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-2\n");
    addScratchBatch(pCmdList);

    XGIDebug(DBG_FUNCTION,"[DBG-Jong] endCmdList-3\n");
//    linkToLastBatch(pCmdList);
    submit2DBatch(pCmdList);
}

static void sendGECommand(CmdList* pCmdList, CARD32 addr, CARD32 cmd)
{
    // Encrypt the command for AGP.
    CARD32 shift        = (pCmdList->_curBatchDataCount++) & 0x00000003;
    pCmdList->_bunch[0] |= (addr | 1) << (shift << 3);
    pCmdList->_bunch[shift + 1]  = cmd;

    // Bunch finished, Send to HW.
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

static void startFillData(CmdList* pCmdList, CARD32 size)
{
    reserveData(pCmdList, size);
    sendRemainder2DCommand(pCmdList);

    pCmdList->_curBatchDataBegin = pCmdList->_writePtr;
    pCmdList->_writePtr++;
    pCmdList->_sendDataLength = 0;
}

static void fillData(CmdList * pCmdList,
                     unsigned char * ptr,
                     CARD32 width,  // bytes
                     long int delta,      // bytes
                     CARD32 height)
{
    CARD32 sizeAlign;
    sizeAlign = width & ~0x03;

    // fill data to the cmdbatch
    // write 1 dword per time
    // pack the data to 4 dwords align
    // ...
    while(height--)
    {
        // ASSERTDD((m_ulDataLength - m_ulSendDataLen) >= ulWidth, "Reserved size is not enough!");
        // m_pulDataStartAddr = (ULONG*)((m_pulDataStartAddr) & m_ulDataPortMask);
        // m_pulDataStartAddr = (ULONG*)((m_pulDataStartAddr) | m_ulDataPortOr);

        // Next line start address
        CARD32 sizeRem = 0;
        unsigned char* ptrNext = ptr + delta;

        // ????SSEable?????
        memcpy(pCmdList->_writePtr, ptr, sizeAlign);
        // DumpArray32(ptr, ulSizeAlign / 4);

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

            // Dump to file for debug
            // DumpData32(0x10000, ulRemainder);
        }
        ptr = ptrNext;
    }
    // then call endCmdList() outside
}


static void submitData(CmdList* pCmdList)
{
    // ASSERTDD(m_ulSendDataLen <= m_ulDataLength, "Reserved size is not enough!");
    // ASSERTDD(DATAFILL_INBATCH_BEGIN != m_DataFillMode, "Did not fill data after reserve.");

    // Pad the data to 128 bit alignment.
    while(0 != ((CARD32)pCmdList->_writePtr & 0x0f))
    {
        *pCmdList->_writePtr++ = 0;
    }

    // Correct the data length
    *pCmdList->_curBatchDataBegin = 0xff000000 | (pCmdList->_sendDataLength/4);
    pCmdList->_curBatchDataCount += (CARD32)(pCmdList->_writePtr - pCmdList->_curBatchDataBegin);
}


static void waitCmdListAddrAvailable(CmdList* pCmdList, CARD32* addrStart, CARD32* addrEnd)
{
	// The loop of waiting for enough command list buffer
    while(1)
    {
        // Get the current runing batch address.

        CARD32* curGEWorkedCmdAddr = pCmdList->_cmdBufLinearStartAddr
            + (getGEWorkedCmdHWAddr(pCmdList) - pCmdList->_cmdBufHWStartAddr);

        if(NULL != curGEWorkedCmdAddr)
        {
            if ((curGEWorkedCmdAddr < addrStart) || // cmdlist is fresh
                (curGEWorkedCmdAddr > addrEnd))     // cmdlist already rolled over. Current batch does not overlay the buffer.
            {
                // There is enough memory at the begin of command list.
                break;
            }
        }
        else
        {
            // No running batch
            if ((NULL != pCmdList->_lastBatchBegin) &&
                (addrStart >= pCmdList->_lastBatchBegin &&  addrEnd <= pCmdList->_lastBatchBegin))
            {
                // If current command list overlaps the last begin
                // Force to reset
                reset(pCmdList);
            }
            break;
        }
    }
}

static CARD32* getGEWorkedCmdHWAddr(CmdList* pCmdList)
{
	return (CARD32*)(*(pCmdList->_scratchPadLinearAddr));
}


static void sendRemainder2DCommand(CmdList* pCmdList)
{
    // ASSERTDD(BTYPE_2D == m_btBatchType, "Only 2D batch can use SendCommand!");
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

static void addScratchBatch(CmdList* pCmdList)
{
    //because we add 2D scratch directly at the end of this batch
    //ASSERT(BTYPE_2D == pCmdList->_curBatchType);

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
    pCmdList->_writePtr[7]  = (CARD32) pCmdList->_lastBatchEnd
                             -(CARD32) pCmdList->_cmdBufLinearStartAddr
                             +(CARD32) pCmdList->_cmdBufHWStartAddr; //garbage

    pCmdList->_writePtr[8]  = 0xff000001;
    pCmdList->_writePtr[9]  = pCmdList->_writePtr[7];
    pCmdList->_writePtr[10] = 0x00000000;
    pCmdList->_writePtr[11] = 0x00000000;

    pCmdList->_writePtr += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;
	pCmdList->_curBatchDataCount += AGPCMDLIST_2D_SCRATCH_CMD_SIZE;

	/* Jong 06/29/2006; demark */
	dumpCommandBuffer(pCmdList);
}

static void linkToLastBatch(CmdList* pCmdList)
{

    CARD32 beginHWAddr;
    CARD32 beginPort;
    //batch begin addr should be 4 Dwords alignment.
    //ASSERT(0 == (((CARD32)pCmdList->_curBatchBegin) & 0x0f);

    if (0 == pCmdList->_curBatchDataCount)
    {
        //is it reasonable?
        return;
    }

    //batch end addr should be 4 Dwords alignment
    //ASSERT(0 == (((CARD32)pCmdList->_writePtr) & 0x0f));

    beginHWAddr = (CARD32) pCmdList->_cmdBufHWStartAddr
                 +(CARD32) pCmdList->_curBatchBegin
                 -(CARD32) pCmdList->_cmdBufLinearStartAddr;

    // Which begin we should send.
    beginPort = getCurBatchBeginPort(pCmdList);

    if (NULL == pCmdList->_lastBatchBegin)
    {
        CARD32* pCmdPort;
        // Issue PCI begin

        // Wait for GE Idle.
        waitForPCIIdleOnly();

        pCmdPort = (CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + beginPort);

        // Enable PCI Trigger Mode
        *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS)) =
                (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
                M2REG_CLEAR_COUNTERS_MASK |
                0x08 |
                M2REG_PCI_TRIGGER_MODE_MASK;

        *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + M2REG_AUTO_LINK_SETTING_ADDRESS)) =
                (M2REG_AUTO_LINK_SETTING_ADDRESS << 22) |
                0x08 |
                M2REG_PCI_TRIGGER_MODE_MASK;

        // Send PCI begin command
        pCmdPort[0] = (beginPort<<22) + (BEGIN_VALID_MASK) + pCmdList->_debugBeginID;
        pCmdPort[1] = BEGIN_LINK_ENABLE_MASK + pCmdList->_curBatchDataCount;
        pCmdPort[2] = (beginHWAddr >> 4);
        pCmdPort[3] = 0;
    }
    else
    {
        // Encode BEGIN
        pCmdList->_lastBatchBegin[1] = BEGIN_LINK_ENABLE_MASK + pCmdList->_curBatchDataCount;
        pCmdList->_lastBatchBegin[2] = (beginHWAddr >> 4);
        pCmdList->_lastBatchBegin[3] = 0;

        //Fixme, Flush cache
        //_mm_mfence();

        pCmdList->_lastBatchBegin[0] = (beginPort<<22) + (BEGIN_VALID_MASK) + pCmdList->_debugBeginID;

        triggerHWCommandList(pCmdList, 1);
    }

    pCmdList->_lastBatchType  = pCmdList->_curBatchType;
    pCmdList->_lastBatchBegin = pCmdList->_curBatchBegin;
    pCmdList->_lastBatchEnd   = pCmdList->_writePtr;
    pCmdList->_debugBeginID   = (pCmdList->_debugBeginID + 1) & 0xFFFF;
}

CARD32 getCurBatchBeginPort(CmdList* pCmdList)
{
    // Convert the batch type to begin port ID
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
        //ASSERT(0);
		return 0xff;
    }
}

static void triggerHWCommandList(CmdList* pCmdList, CARD32 triggerCounter)
{
    static CARD32 s_triggerID = 1;

    XGIDebug(DBG_CMD_BUFFER, "[DBG]Before trigger [2800]=%0x\n", *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG)) );

    //Fixme, currently we just trigger one time
    while (triggerCounter--)
    {
        *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG + M2REG_PCI_TRIGGER_REGISTER_ADDRESS))
        = 0x05000000 + (0xffff & s_triggerID++);
    }

    XGIDebug(DBG_CMD_BUFFER, "[DBG]After trigger [2800]=%08x\n", *((CARD32*)((unsigned char*)pCmdList->_mmioBase + BASE_3D_ENG)) );
}

static void waitForPCIIdleOnly()
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

static void reset(CmdList* pCmdList)
{
}

void dumpCommandBuffer(CmdList* pCmdList)
{
    int i;

    XGIDebug(DBG_FUNCTION,"Entering dumpCommandBuffer\n");

    for (i=0; i<(pCmdList->_writePtr - pCmdList->_cmdBufLinearStartAddr); i++)
    {
        if (i % 4 == 0)
        {
            XGIDebug(DBG_CMD_BUFFER, "%08x ", (CARD32)pCmdList->_cmdBufLinearStartAddr + i*4);
        }

            XGIDebug(DBG_CMD_BUFFER, "%08x ", *(pCmdList->_cmdBufLinearStartAddr + i));

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
static void preventOverwriteCmdbuf(CmdList* pCmdList)
{
    CARD32* curGEWorkedCmdAddr = 0;
    while (1)
    {
        curGEWorkedCmdAddr = pCmdList->_cmdBufLinearStartAddr
                            +( getGEWorkedCmdHWAddr(pCmdList)
                              -pCmdList->_cmdBufHWStartAddr);

        if (curGEWorkedCmdAddr <= pCmdList->_lastBatchEnd)
        {
            break;
        }
    }
}

static int submit2DBatch(CmdList* pCmdList)
{
    CARD32 beginHWAddr;
    CARD32 beginPort;
	XGICmdInfoRec submitInfo;
	int retval = -1;

    XGIDebug(DBG_FUNCTION, "[DBG-Jong] submit2DBatch-1\n");

    if (0 == pCmdList->_curBatchDataCount)
    {
        /*is it reasonable? */
        return retval;
    }

    XGIDebug(DBG_FUNCTION, "[DBG-Jong] submit2DBatch-2\n");
    beginHWAddr = (CARD32) pCmdList->_cmdBufHWStartAddr
                 +(CARD32) pCmdList->_curBatchBegin
                 -(CARD32) pCmdList->_cmdBufLinearStartAddr;

    /* Which begin we should send. */
    beginPort = getCurBatchBeginPort(pCmdList);
    XGIDebug(DBG_FUNCTION, "[DBG-Jong] submit2DBatch-3\n");

    submitInfo._firstBeginType = pCmdList->_curBatchType;
    submitInfo._firstBeginAddr = beginHWAddr;
    submitInfo._lastBeginAddr = beginHWAddr;
    submitInfo._firstSize = pCmdList->_curBatchDataCount;
    submitInfo._curDebugID = pCmdList->_debugBeginID;
    submitInfo._beginCount = 1;

    if (NULL == pCmdList->_lastBatchBegin)
    {
    XGIDebug(DBG_FUNCTION, "[DBG-Jong] submit2DBatch-4\n");
        waitForPCIIdleOnly();
    XGIDebug(DBG_FUNCTION, "[DBG-Jong] submit2DBatch-5\n");
    }

	XGIDebug(DBG_FUNCTION, "[DBG-Jong] submit2DBatch-5-1-pCmdList->_fd=%d\n",pCmdList->_fd);
    /* return; */

	/* Jong 05/24/2006; cause system hang on Gateway platform but works on others */
    retval = ioctl(pCmdList->_fd, XGI_IOCTL_SUBMIT_CMDLIST, &submitInfo);
    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] submit2DBatch-6\n");

	/* Jong 05/25/2006 */
    /* return; */
    waitfor2D(pCmdList); 
    XGIDebug(DBG_FUNCTION, "[DBG-Jong] submit2DBatch-7\n");

    if (retval != -1)
    {
        pCmdList->_lastBatchType  = pCmdList->_curBatchType;
        pCmdList->_lastBatchBegin = pCmdList->_curBatchBegin;
        pCmdList->_lastBatchEnd   = pCmdList->_writePtr;
        pCmdList->_debugBeginID   = (pCmdList->_debugBeginID + 1) & 0xFFFF;
    }
    else
    {
        ErrorF("[2D] ioctl -- cmdList error!\n");
    }

    XGIDebug(DBG_FUNCTION, "[DBG-Jong] Leave submit2DBatch()\n");
    return retval;
}

static inline void waitfor2D(CmdList* pCmdList)
{
    CARD32 lastBatchEndHW = (CARD32) pCmdList->_lastBatchEnd
                            -(CARD32) pCmdList->_cmdBufLinearStartAddr
                            +(CARD32) pCmdList->_cmdBufHWStartAddr; //garbage

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
            //loop
			// Jong 07/03/2006
			// Why just need one time to output message and get working ?
			//if(g_bFirst==1)
			//{
				//ErrorF("[DBG-Jong-07032006] waitfor2D()-in loop\n");
				//XGIDumpRegisterValue(g_pScreen);
			//}

			if(g_bFirst==1)
				usleep(1); 

			/* Jong 05/25/2006 */
			/* if(WaitCount >= 1000) break;
			else
				WaitCount++; */
        }

		/* Jong 05/25/2006 */
		//g_bFirst++;
		//if(g_bFirst <= 10)
		if(g_bFirst == 1)
		{
			g_bFirst=0;
			//ErrorF("Jong-07032006-waitfor2D()-End loop\n");
			//XGIDumpRegisterValue(g_pScreen);
		} 

	    XGIDebug(DBG_FUNCTION, "[DBG-Jong-ioctl] waitfor2D()-End loop\n");
    }
}
