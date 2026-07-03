#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h
// ============================================================================
// Real home (X360 assert strings, e.g.
// "..\\..\\..\\GameSource\\World/AI/BrnAIModuleIO.h" and
// "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\ai\\BrnAIModuleIO.h")
// of BrnAI::AIModuleIO::OutputBuffer -- the per-frame buffer the AI module
// publishes and the world bridges consume. It is a large CgsModule::IOBuffer
// payload (>110KB) holding, at attested byte offsets, several sub-interfaces and
// a trailing game-event queue.
//
// MINIMAL SLICE. Only the eleven lock-checked byte-offset accessors the X360
// ARTIST build emitted out-of-line are reconstructed. The interior member sizes
// and the gaps between them are NOT attested, so no named members are declared
// (declaring them would require inventing padding sizes). Each accessor returns a
// raw u8* to `this + <attested byte offset>`, exactly as the X360 signature
// (`unsigned __int8*`) does. Replace this slice with a fully named layout (without
// moving the pinned offsets) when the owning-buffer DWARF lands.
//
// Lock-bit guard per the recurring IOBuffer prologue:
//   read-lock  (status>>4 & 1) => IsBufferLockedForReading()  ("Not locked for reading")
//   write-lock (status>>3 & 1) => IsBufferLockedForWriting()  ("Not locked for writing")
// The X360 tests WHICHEVER bit the asm names -- some read-suffixed getters check
// the read bit, some the write bit; reproduced verbatim (not "fixed"). The
// X360-baked file path + line-number assert args are dropped per project policy.
//
// Attested return offsets (base = this):
//   GetAIOutputBufferHeader  @0x8276DB18 W  -> this + 0x1014   (4116)    :430
//   GetAIRes                 @0x8279CAA8 R  -> this + 0x4      (4)       :423
//   GetVehicleDri            @0x8276D9C8 W  -> this + 0x15120  (86304)   :402
//   GetVehi                  @0x8279CA00 R  -> this + 0x15120  (86304)   :409
//   GetAIRaceCarInterface    @0x8276DBC0 W  -> this + 0x165D0  (91600)   :444
//   GetAICarOutputInterfac   @0x8276DC68 W  -> this + 0x16A60  (92768)   :458
//   GetAICarOutputIn         @0x8279CCA0 R  -> this + 0x16A60  (92768)   :465
//   GetAIModuleResultIn      @0x8276DD10 W  -> this + 0x17F50  (98128)   :472
//   GetAIModuleRe            @0x8279CD48 R  -> this + 0x17F50  (98128)   :479
//   GetGameE                 @0x8279C658 R  -> this + 0x1AF30  (110448)  :232
//   GetGameEventQu           @0x8276D680 W  -> this + 0x1AF30  (110448)  :233

#include "types.hpp"                                     // u8
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer

namespace BrnAI
{
namespace AIModuleIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // Attested member start offsets into the buffer payload (bytes from this).
        enum EMemberOffset
        {
            KU_AI_OUTPUT_BUFFER_HEADER_OFFSET = 0x1014,   // X360 0x8276DB18 (W, :430)
            KU_AI_RESULT_OFFSET               = 0x4,
            KU_VEHICLE_DRIVER_OFFSET          = 0x15120,
            KU_AI_RACE_CAR_INTERFACE_OFFSET   = 0x165D0,
            KU_AI_CAR_OUTPUT_INTERFACE_OFFSET = 0x16A60,
            KU_AI_MODULE_RESULT_OFFSET        = 0x17F50,
            KU_GAME_EVENT_QUEUE_OFFSET        = 0x1AF30,
        };

        // X360 0x8276DB18 (W, :430) -- write-lock handle at this+0x1014. Name is a
        // placeholder pending OutputBuffer DWARF; offset/return/lock-bit are attested.
        u8* GetAIOutputBufferHeader();

        // X360 0x8279CAA8 (R, :423) -- read-lock handle at this+0x4.
        u8* GetAIResultInterface();

        // X360 0x8276D9C8 (W, :402) -- write-lock handle at this+0x15120.
        u8* GetVehicleDriverInterface();
        // X360 0x8279CA00 (R, :409) -- read-lock handle at this+0x15120.
        u8* GetVehicleInterface();

        // X360 0x8276DBC0 (W, :444) -- write-lock handle at this+0x165D0.
        u8* GetAIRaceCarInterface();

        // X360 0x8276DC68 (W, :458) -- write-lock handle at this+0x16A60.
        u8* GetAICarOutputInterface();
        // X360 0x8279CCA0 (R, :465) -- read-lock handle at this+0x16A60.
        u8* GetAICarOutputInterfaceForRead();

        // X360 0x8276DD10 (W, :472) -- write-lock handle at this+0x17F50.
        u8* GetAIModuleResultInterfaceForWrite();
        // X360 0x8279CD48 (R, :479) -- read-lock handle at this+0x17F50.
        u8* GetAIModuleResultInterface();

        // X360 0x8279C658 (R, :232) -- read-lock handle at this+0x1AF30.
        u8* GetGameEventQueueForRead();
        // X360 0x8276D680 (W, :233) -- write-lock handle at this+0x1AF30.
        u8* GetGameEventQueue();
    };
}
}
