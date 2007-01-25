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

#ifndef _XG47_CMDLIST_H_
#define _XG47_CMDLIST_H_

/*
    Usage:
    Initialize()

    BegineCmdList()

    SendGECommand()
    ...
    StartFillData()
    FillData()
    SubmitData()
    ...

    EndCmdlist()

    Cleanup()
*/

extern int  BeginCmdList(CARD32 size);	/* cmdList size in dword */
extern void EndCmdList();
extern void SendGECommand(CARD32 addr, CARD32 cmd);

extern void StartFillData(CARD32 size);
extern void SubmitData();

extern void FillData(unsigned char* ptr,
                     unsigned long width,
                     long int delta,        /* Src Pitch */
                     unsigned long height);

extern void Initialize(CARD32*  cmdBufLinearStartAddr,
                       CARD32*  cmdBufHWStartAddr,
                       CARD32   cmdBufSize,
                       CARD32*  scratchPadLinearAddr,
                       CARD32*  scracthPadHWAddr,
                       CARD32*  mmioBase,
                       int		fd);
extern void Cleanup();
extern void Reset();

// extern int XGIDebug(int level, const char *format, ...);

/*data struct define, simulate the OO*/

typedef enum
{
    FLUSH_2D                        = M2REG_FLUSH_2D_ENGINE_MASK,
    FLUSH_3D                        = M2REG_FLUSH_3D_ENGINE_MASK,
    FLUSH_FLIP                      = M2REG_FLUSH_FLIP_ENGINE_MASK
}FLUSH_CODE;

typedef enum
{
    AGPCMDLIST_SCRATCH_SIZE         = 0x100,
    AGPCMDLIST_BEGIN_SIZE           = 0x004,
    AGPCMDLIST_3D_SCRATCH_CMD_SIZE  = 0x004,
    AGPCMDLIST_2D_SCRATCH_CMD_SIZE  = 0x00c,
    AGPCMDLIST_FLUSH_CMD_LEN        = 0x004,
    AGPCMDLIST_DUMY_END_BATCH_LEN   = AGPCMDLIST_BEGIN_SIZE
}CMD_SIZE;

typedef struct _CmdList
{
	BATCH_TYPE  _curBatchType;
    CARD32      _curBatchDataCount;         // DWORDs
    CARD32      _curBatchRequestSize;       // DWORDs
    CARD32*     _curBatchBegin;             // The begin of current batch.
    CARD32*     _curBatchDataBegin;         // The begin of data

    CARD32*     _writePtr;                  // current writing ptr
    CARD32      _sendDataLength;            // record the filled data size

    CARD32*     _cmdBufLinearStartAddr;
    CARD32*     _cmdBufHWStartAddr;
    CARD32      _cmdBufSize;            // DWORDs

	CARD32*     _lastBatchBegin;        // The begin of last batch.
    CARD32*     _lastBatchEnd;          // The end of last batch.
    BATCH_TYPE  _lastBatchType;         // The type of the last workload batch.
    CARD32      _debugBeginID;          // write it at begin header as debug ID

	//2d cmd holder
	CARD32      _bunch[4];

	//scratch pad addr, allocate at outside, pass these two parameter in
    CARD32*     _scratchPadLinearAddr;
	CARD32*     _scratchPadHWAddr;

	//MMIO base
	CARD32*     _mmioBase;

	//fd number
	int			_fd;
}CmdList;

#endif
