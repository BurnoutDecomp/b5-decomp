#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Out-of-line bodies of the lock-checked accessors of BrnAI::AIModuleIO::OutputBuffer that
// the X360 ARTIST build emitted out-of-line, plus the buffer's own Construct.
//
// Each accessor returns the address of the member the X360 offset names (see the offset ->
// member map in the header), guarded by whichever lock bit the asm names -- read (bit 4) or
// write (bit 3). Reproduced verbatim; not "fixed". The X360 file/line assert args are
// dropped per project policy.
//
// ⛔ 2026-08-25: these used to return `reinterpret_cast<u8*>(this) + <X360 offset>` into a
// one-byte host object. See the header banner.

namespace BrnAI
{
namespace AIModuleIO
{
    // ---- ADDITIVE (aimodule wave 2026-08-25) ------------------------------------------
    // Construct the base status flags, then every embedded queue. The X360 emits the AI
    // output buffer's Construct inline into its CreateIOBuffer<OutputBuffer> instantiation;
    // the shape is the one every sibling buffer in this tree uses -- base first, then each
    // queue member's own Construct in member order (see the attested expansion quoted in
    // CgsIOBufferStack.h: `*p = 1` IS IOBuffer::Construct, followed by the members').
    //
    // The two untyped interiors (maVehicleDriverInterface / maAIRaceCarInterface) are NOT
    // touched: their real types are not reconstructed, so there is nothing to construct and
    // a memset would be [[memset is not construction]] wearing a disguise. They are storage,
    // and the accessors that hand them out are still boot-gated on the consumer side.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mAIResourceRequestInterface.Construct();
        mRouteResponseQueue.Construct();
        mAIModuleResultInterface.GetResetOnTrackResultQueue()->Construct();
        mAIModuleResultInterface.GetPlaceOnTrackRequestQueue()->Construct();
        mGameEventQueue.Construct();
    }

    // X360 0x8276DA70 (W) -- write-lock handle at this+0x4 (the AI resource request
    // interface; the 0x1010 size match pins it, see the header). AIModule::LoadMapData
    // LockForWrite()s the buffer and posts its LoadBundle/AcquireResource through this twin.
    BrnResource::GameDataIO::RequestInterface<4096>* OutputBuffer::GetAIResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mAIResourceRequestInterface;
    }

    // X360 0x8279CAA8 (R, :423) -- the read twin, used by WorldModule::BridgeAIModuleToOutput.

    const BrnResource::GameDataIO::RequestInterface<4096>*
    OutputBuffer::GetAIResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mAIResourceRequestInterface;
    }

    // X360 0x8279CB50 (R, :437) -- read-lock handle at this+0x1014.
    const RouteMapModuleIO::RouteResponseQueue* OutputBuffer::GetRouteResponseQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mRouteResponseQueue;
    }

    // (GetRouteResponseQueueForWrite @0x8276DB18 keeps its own file --
    //  BrnAIModuleIO_OutputBuffer_Accessors.cpp -- because the X360 emitted it in a
    //  separate function group.)

    // X360 0x8276D9C8 (W, :402) -- write-lock handle at this+0x15120.
    u8* OutputBuffer::GetVehicleDriverInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return maVehicleDriverInterface;
    }

    // X360 0x8279CA00 (R, :409) -- read-lock handle at this+0x15120.
    u8* OutputBuffer::GetVehicleInterface()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return maVehicleDriverInterface;
    }

    // X360 0x8276DBC0 (W, :444) -- write-lock handle at this+0x165D0.
    u8* OutputBuffer::GetAIRaceCarInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return maAIRaceCarInterface;
    }

    // X360 0x8276DC68 (W, :458) -- write-lock handle at this+0x16A60.
    u8* OutputBuffer::GetAICarOutputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(&mAICarOutputInterface);
    }

    // X360 0x8279CCA0 (R, :465) -- read-lock handle at this+0x16A60.
    u8* OutputBuffer::GetAICarOutputInterfaceForRead()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(&mAICarOutputInterface);
    }

    // Typed read twin used by WorldModule::BridgeAIModuleToOutput.
    const AICarOutputInterface* OutputBuffer::GetAICarOutputInterfaceConst() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mAICarOutputInterface;
    }

    // X360 0x8276DD10 (W, :472) -- write-lock handle at this+0x17F50.
    u8* OutputBuffer::GetAIModuleResultInterfaceForWrite()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(&mAIModuleResultInterface);
    }

    // X360 0x8279CD48 (R, :479) -- read-lock handle at this+0x17F50.
    u8* OutputBuffer::GetAIModuleResultInterface()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(&mAIModuleResultInterface);
    }

    // X360 0x8279C658 (R, :232) -- read-lock handle at this+0x1AF30.
    u8* OutputBuffer::GetGameEventQueueForRead()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(&mGameEventQueue);
    }

    // X360 0x8276D680 (W, :233) -- write-lock handle at this+0x1AF30.
    u8* OutputBuffer::GetGameEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return reinterpret_cast<u8*>(&mGameEventQueue);
    }

    // Typed read twin used by WorldModule::BridgeAIModuleToOutput.
    const CgsModule::VariableEventQueue<1536, 16>* OutputBuffer::GetGameEventQueueConst() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mGameEventQueue;
    }
}
}
