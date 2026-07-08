// =====================================================================================
// CReferenceLibrary -- fixed-capacity pool of YUV reference-frame pictures for the RTCMV/WMV
// video-object encoder (CWMVideoObjectEncoder). SetupReferenceLibrary builds a CQueue of N
// PictureCYUV420 frames; AddFrame / RemoveFrame splice frames through that queue;
// CloseReferenceLibrary cleans and frees every frame and then the queue.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No reference
// source and no DecFIGS DWARF hints exist for this TU.
//
//   CReferenceLibrary::AddFrame              @0x82A03F28
//   CReferenceLibrary::RemoveFrame           @0x82A03ED8
//   CReferenceLibrary::CloseReferenceLibrary @0x82A04068
//   CReferenceLibrary::SetupReferenceLibrary @0x82A04198
//   CReferenceLibrary::~CReferenceLibrary    @0x82A04190
//
// Base pointer in the asm is byte-addressed (this[N] == byte offset N*4). Only +0x00 and the
// queue pointer at +0x0C are load-bearing for the recovered functions.
//   +0x00 miCurrentIndex   frame cursor: ++ on AddFrame, conditional -- on RemoveFrame
//   +0x04 miReserved04     not referenced by the recovered functions (padding)
//   +0x08 miReserved08     zeroed by SetupReferenceLibrary; not otherwise referenced
//   +0x0C mpQueue          CQueue* of PictureCYUV420* frames (allocated in setup)
// The object is owned/embedded by CWMVideoObjectEncoder; only offsets +0x00..+0x0F are
// attested here, so no total-size assert is pinned.
// =====================================================================================
#pragma once

#include "types.hpp"
#include "SDKs/EATech/rwmovie/CQueue.h"
#include "SDKs/EATech/rwmovie/PictureCYUV420.h"

class CReferenceLibrary
{
public:
    // Non-virtual: destruction is a plain tail-call to CloseReferenceLibrary (no vptr).
    ~CReferenceLibrary();

    // Allocates the frame CQueue (liNumFrames node capacity) and populates it with
    // liNumFrames PictureCYUV420 frames, each built through PictureCYUV420_init from the
    // forwarded parameters. All error reporting flows through *lpiResult (0 = success, -3 on
    // an allocation failure, or whatever the queue ctor / frame init flag). On any failure the
    // partially built library is torn down through CloseReferenceLibrary. Returns non-zero on
    // success, 0 on failure (the meaningful channel is *lpiResult).
    s32 SetupReferenceLibrary(s32* lpiResult, s32 liParam2, s32 liParam3, s32 liNumFrames,
                              s32 liParam5, s32 liParam6, s32 liParam7);

    // Cleans + frees every queued frame, then destroys and frees the CQueue and nulls it.
    s32 CloseReferenceLibrary();

    // Pushes lpFrame onto the front of the queue. Returns 0 on success, -100 if the frame is
    // null or the queue is in a bad state (mirrors CQueue::AddElement's success condition).
    s32 AddFrame(PictureCYUV420* lpFrame);

    // Removes the frame at liIndex (-1 = tail) into *lppFrame, recycling the queue node.
    // Returns CQueue::RemoveElement's result on success; on an out-of-range request leaves
    // *lppFrame == 0.
    s32 RemoveFrame(PictureCYUV420** lppFrame, s32 liIndex);

private:
    s32     miCurrentIndex;   // +0x00
    s32     miReserved04;     // +0x04
    s32     miReserved08;     // +0x08
    CQueue* mpQueue;          // +0x0C
};
