// ============================================================================
// b5-decomp/src/GameShared/GameClasses/System/Resource/CgsLiveUpdateModuleIO.h
//
// Canonical (DWARF) home for CgsResource::LiveUpdateIO::OutputBuffer (CgsLiveUpdateModuleIO.h).
// The live-update IO output payload buffer the resource live-update module fills each frame.
// It derives from CgsModule::IOBuffer and carries ONE embedded request queue
// (VariableEventQueue<512,16>, 528 bytes) followed by a 1-byte in-flight status.
//
// X360-emitted accessors (bit-tested lock guard on the base status byte, verbatim messages):
//   GetOutputQueue() const  @ 0x826642D8  read-lock  (bit 4) -> +4     request queue (const)
//   Get()                   @ 0x828E64A0  write-lock (bit 3) -> +4     request queue (non-const)
//   GetSt()  const          @ 0x82664230  read-lock  (bit 4) -> +0x214 status byte    (DWARF :108)
//   SetStatus(const u8*)    @ 0x828E63F0  write-lock (bit 3) -> +0x214 publish status  (DWARF :109)
//   Construct()             @ 0x828EEF98  status=constructed + queue Construct + status byte = 0
//
// LAYOUT (X360 authoritative -- Construct stores the constructed bit at this+0, calls
// VariableEventQueue<512,16>::Construct(this+4), then zeroes the status byte at this+0x214=532):
//   base   CgsModule::IOBuffer                      (1-byte FlagSet status; +1..+3 pad)
//   +4     VariableEventQueue<512,16> mOutputQueue  (528 bytes; sizeof == 0x210, ends at +0x214)
//   +0x214 u8                         mStatus       (live-update in-progress flag)
//
// The +4 member is the SAME buffer viewed two ways: LiveUpdateModule::Update takes the write lock,
// Get()s it as the request queue and AddEvents a LoadBundleRequest; consumers read-lock it via
// GetOutputQueue(). SetStatus()/GetSt() publish/read the +0x214 in-flight byte.
// OutputQueueStorage is retained as an alias of the real queue type so the already-committed
// GetOutputQueue() const body (return &mOutputQueue) keeps compiling unchanged.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<512,16>

namespace CgsResource
{
namespace LiveUpdateIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // The live-update request queue at +4 (VariableEventQueue<512,16>, 528 bytes). Construct()
        // constructs it and LiveUpdateModule::Update AddEvents its LoadBundleRequest onto it.
        typedef CgsModule::VariableEventQueue<512, 16> OutputQueue;
        typedef OutputQueue                            OutputQueueStorage;  // back-compat alias

        // ---- lifecycle -----------------------------------------------------------------
        void Construct();   // status=constructed + queue Construct + status byte = 0

        // ---- accessors owned/bodied by this TU -----------------------------------------
        const OutputQueue* GetOutputQueue() const;        // +4,     read-lock  (bit 4)
        OutputQueue*       Get();                          // +4,     write-lock (bit 3)  request queue
        const u8*          GetSt() const;                  // +0x214, read-lock  (bit 4)  status byte
        void               SetStatus(const u8* lpStatus);  // +0x214, write-lock (bit 3)  publish status

        static void _AssertLayout();

    private:
        u8          maStatusPad[3];  // +1..+3 (force +4 placement)
        OutputQueue mOutputQueue;    // +4     (VariableEventQueue<512,16>, 528 bytes)
        u8          mStatus;         // +0x214 (live-update in-progress flag)
    };
}
}
