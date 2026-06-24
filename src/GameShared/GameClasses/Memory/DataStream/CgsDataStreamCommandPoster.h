#pragma once

#include "types.hpp"
#include "SDKs/EATech/eathread/Futex.h"  // Futex::AtomicUint64 (minimal slice)

// CgsMemory::DataStreamCommandPoster
// ---------------------------------------------------------------------------
// The posting side of a lock-free data stream: callers fill fixed-stride command
// records (and optionally a variable-size data region) which the consuming
// DataStreamResultReader plays back. The poster owns a fixed-size command buffer
// (records of stride miCommandSize), a variable-size data buffer, and a packed
// 64-bit status word (mEncodedStatus) that the streaming side updates lock-free.
//
// X360 homes (reconstructed in this pass):
//   AddCommand      @ 0x82867D28
//   AllocateCommand @ 0x82867E60
//
// Layout is X360-authoritative (verified against the pseudocode this->[n] member
// accesses in AddCommand / AllocateCommand): mEncodedStatus@0 (8B),
// mpcCommandBuffer@8, miCommandBufferSize@12, miCommandSize@16, miNumCommands@20,
// miMaxCommands@24, mpcDataBuffer@28, miDataBufferSize@32, miDataBufferUsed@36,
// mbStreaming@40. (mEncodedStatus's 8-byte width at +0 pushes mpcCommandBuffer to
// +8; the remaining members follow the DWARF declaration order on a 32-bit ABI.)
namespace CgsMemory
{
    // The command reader plays back commands the poster appended: it holds a
    // pointer to this poster and CAS-advances the poster's packed nextCommand
    // field while copying out each fixed-stride record (see CgsDataStreamCommandReader).
    struct DataStreamCommandReader;

    struct DataStreamCommandPoster
    {
        // The reader reads the poster's command buffer and CAS-updates its packed
        // status word; it needs access to the private members below. (Additive:
        // layout/sizeof unchanged.)
        friend struct DataStreamCommandReader;

        // Packed-status bit layout (CgsDataStreamCommandPoster.h:44-51). Verbatim
        // from DWARF; the encode/decode helpers (EncodeStatus / DecodeStatus,
        // declared below) are the only users and are not reconstructed in this
        // pass. KU_NEXT_COMMAND_BIT has no value in the DWARF (its mask starts at
        // bit 0). (FLAG)
        static const u64 KU_NEXT_COMMAND_BIT  = 0;            // h:44 (no value in DWARF; mask starts at bit 0)
        static const u64 KU_NUM_USERS_BIT     = 24;           // h:45

        static const u64 KU_NEXT_COMMAND_MAX  = 16777215u;    // h:47  0x00FFFFFF
        static const u64 KU_NUM_USERS_MAX     = 15u;          // h:48  0x0000000F

        static const u64 KU_NEXT_COMMAND_MASK = 16777215u;    // h:50  0x00FFFFFF
        static const u64 KU_NUM_USERS_MASK    = 251658240u;   // h:51  0x0F000000

        // --- API (CgsDataStreamCommandPoster.h:64-109) -----------------------
        // Reconstructed in this pass.
        s32 AddCommand(void* lpCommand);
        s32 AllocateCommand(void** lppOutBuffer);

        // Declared-only (not reconstructed in this TU pass).
        void Construct(void* lpCommandBuffer, s32 liCommandBufferSize, s32 liCommandSize,
                       s32 liInitialCommandCount, void* lpDataBuffer, s32 liDataBufferSize,
                       s32 liInitialDataBufferUsed);
        void Destruct();
        void Begin();
        void End();
        void* AllocateData(s32 liSize, s32 liAlignment);
        s32  AddCommands(void* lpCommands, s32 liCommandCount);
        s32  AllocateCommands(s32 liCommandCount, void** lppOutBuffer);
        s32  GetNumCommands() const;

    private:
        // Declared-only status (un)pack helpers (CgsDataStreamCommandPoster.h:132/138).
        u64  EncodeStatus(u8 lucNumUsers, u32 luNextCommand);
        void DecodeStatus(const u64& lruEncoded, u8* lpucNumUsers, u32* lpuNextCommand);
        u64  GetEncodedStatus() const;
        void SetEncodedStatusConditional(u64 luNewValue, u64 luCurrValue);

        // --- members (CgsDataStreamCommandPoster.h:114-127) ------------------
        Futex::AtomicUint64 mEncodedStatus;      // +0  packed (numUsers|nextCommand)
        char*               mpcCommandBuffer;    // +8  fixed-stride command records
        s32                 miCommandBufferSize; // +12 command buffer size (bytes)
        s32                 miCommandSize;       // +16 one command's stride (bytes)
        s32                 miNumCommands;       // +20 commands posted so far (write cursor)
        s32                 miMaxCommands;       // +24 commands that fit (bufSize / commandSize)
        char*               mpcDataBuffer;       // +28 variable-size data buffer
        s32                 miDataBufferSize;    // +32 data buffer size (bytes)
        s32                 miDataBufferUsed;    // +36 data buffer bytes consumed
        bool                mbStreaming;         // +40 true between Begin() and End()
    };
}
