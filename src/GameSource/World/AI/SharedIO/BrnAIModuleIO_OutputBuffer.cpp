#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Out-of-line bodies of the ten lock-checked byte-offset accessors of
// BrnAI::AIModuleIO::OutputBuffer that the X360 ARTIST build emitted out-of-line.
// Each returns a raw u8* to `this + <attested offset>` (EMemberOffset enum in the
// header), guarded by whichever lock bit the asm names -- read (bit 4) or write
// (bit 3). Reproduced verbatim; not "fixed". The X360 file/line assert args are
// dropped per project policy.

namespace BrnAI
{
namespace AIModuleIO
{
    // X360 0x8279CAA8 (R, :423) -- read-lock handle at this+0x4 (4).
    u8* OutputBuffer::GetAIResultInterface()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(this) + KU_AI_RESULT_OFFSET;
    }

    // X360 0x8276D9C8 (W, :402) -- write-lock handle at this+0x15120 (86304).
    u8* OutputBuffer::GetVehicleDriverInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(this) + KU_VEHICLE_DRIVER_OFFSET;
    }

    // X360 0x8279CA00 (R, :409) -- read-lock handle at this+0x15120 (86304).
    u8* OutputBuffer::GetVehicleInterface()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(this) + KU_VEHICLE_DRIVER_OFFSET;
    }

    // X360 0x8276DBC0 (W, :444) -- write-lock handle at this+0x165D0 (91600).
    u8* OutputBuffer::GetAIRaceCarInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(this) + KU_AI_RACE_CAR_INTERFACE_OFFSET;
    }

    // X360 0x8276DC68 (W, :458) -- write-lock handle at this+0x16A60 (92768).
    u8* OutputBuffer::GetAICarOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(this) + KU_AI_CAR_OUTPUT_INTERFACE_OFFSET;
    }

    // X360 0x8279CCA0 (R, :465) -- read-lock handle at this+0x16A60 (92768).
    u8* OutputBuffer::GetAICarOutputInterfaceForRead()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(this) + KU_AI_CAR_OUTPUT_INTERFACE_OFFSET;
    }

    // X360 0x8276DD10 (W, :472) -- write-lock handle at this+0x17F50 (98128).
    u8* OutputBuffer::GetAIModuleResultInterfaceForWrite()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(this) + KU_AI_MODULE_RESULT_OFFSET;
    }

    // X360 0x8279CD48 (R, :479) -- read-lock handle at this+0x17F50 (98128).
    u8* OutputBuffer::GetAIModuleResultInterface()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(this) + KU_AI_MODULE_RESULT_OFFSET;
    }

    // X360 0x8279C658 (R, :232) -- read-lock handle at this+0x1AF30 (110448).
    u8* OutputBuffer::GetGameEventQueueForRead()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(this) + KU_GAME_EVENT_QUEUE_OFFSET;
    }

    // X360 0x8276D680 (W, :233) -- write-lock handle at this+0x1AF30 (110448).
    u8* OutputBuffer::GetGameEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(this) + KU_GAME_EVENT_QUEUE_OFFSET;
    }
}
}
