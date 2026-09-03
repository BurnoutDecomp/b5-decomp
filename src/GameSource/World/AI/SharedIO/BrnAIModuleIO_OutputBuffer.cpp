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
    // COMPLETED 2026-09-03 (aiwave lane A4) against the console's own body @0x8278ACB8, which
    // the earlier note guessed was inlined into CreateIOBuffer -- it is a real out-of-line
    // function. Its order (offsets this-relative, documentation only):
    //   0x8278ACD0  stb 1                                            IOBuffer::Construct
    //   0x8278ACDC..0x8278AD04  PerfMonCpu::AddMonitor("AI Output buffer construct A") once + StartMonitor
    //   0x8278AD10  VariableEventQueue<4096,16>::Construct(+0x4) ; Clear     mAIResourceRequestInterface
    //   0x8278AD20  RouteResponse,16>::Construct(+0x1014)                    mRouteResponseQueue
    //   0x8278AD2C  Vehicle::VehicleDriverInputInterface::Construct(+0x15120) mVehicleDriverInterface
    //   0x8278AD48  std 0 @+0x165D0+0x460 ; std 0 @+0x165D0+0x468            mAIRaceCarInterface.mSetRaceCars /
    //                                                                        .mCanPassThroughTraffic (BitArray<35>::Prepare inlined)
    //   0x8278AD64..0x8278AD7C  35x { stfs FLT_MAX @+0x16A60+0x140C ; sth 0x7FFF @+0x16A60+0x1498 }
    //                                                                        AICarOutputInterface::Construct inlined
    //   0x8278AD8C  ResetOnTrackResult,128>::Construct(+0x17F50)             } mAIModuleResultInterface
    //   0x8278AD94  PlaceOnTrackRequest,128>::Construct(+0x17F50+0x1810)     }
    //   0x8278ADA0  VariableEventQueue<1536,16>::Construct(+0x1AF30)         mGameEventQueue
    //   0x8278ADA8  PerfMonCpu::StopMonitor
    // [FLAG PC bring-up] the "AI Output buffer construct A" CPU monitor (a file-static id at
    // 0x82F3016C) is not modelled -- instrumentation only. DELETE-WHEN a PerfMonCpu id home for
    // the IO-buffer constructs exists.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mAIResourceRequestInterface.Construct();
        mRouteResponseQueue.Construct();
        mVehicleDriverInterface.Construct();
        mAIRaceCarInterface.mSetRaceCars.Prepare();
        mAIRaceCarInterface.mCanPassThroughTraffic.Prepare();
        mAICarOutputInterface.Construct();
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

    // The two members retyped 2026-09-03 (aiwave lane A4): console extents == host sizes.
    static_assert(sizeof(OutputBuffer::VehicleDriverInputInterface) == 0x14B0,
                  "mVehicleDriverInterface is the console's 0x14B0-byte VehicleDriverInputInterface");
    static_assert(sizeof(AIRaceCarInterface) == 0x490,
                  "mAIRaceCarInterface is the console's 0x490-byte AIRaceCarInterface");

    // X360 0x8276D9C8 (W, :402) -- write-lock handle at this+0x15120.
    OutputBuffer::VehicleDriverInputInterface* OutputBuffer::GetVehicleDriverInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mVehicleDriverInterface;
    }

    // X360 0x8279CA00 (R, :409) -- read-lock handle at this+0x15120, TYPED const (DWARF :200).
    // Consumer: WorldModule::BridgeAIModuleToPhysicsModule @0x827AAAA8 (0x827AAAF0).
    const OutputBuffer::VehicleDriverInputInterface* OutputBuffer::GetVehicleDriverInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mVehicleDriverInterface;
    }

    // X360 0x8279CA00 (R, :409) -- the same read seat under its pre-wave u8* spelling.
    u8* OutputBuffer::GetVehicleInterface()
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return reinterpret_cast<u8*>(&mVehicleDriverInterface);
    }

    // X360 0x8276DBC0 (W, :444) -- write-lock handle at this+0x165D0.
    AIRaceCarInterface* OutputBuffer::GetAIRaceCarInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mAIRaceCarInterface;
    }

    // X360 0x8279CBF8 (R, :451) -- read-lock handle at this+0x165D0. An ARTIST export HOLE
    // (no JSON); disassembled from the image: `rlwinm r11,r11,0x1c,31,31` (bit 4),
    // "Not locked for reading\n" @0x820062AC, FireAssert(..., BrnAIModuleIO.h @0x820C9258,
    // 0x1C3 == 451), `addis r3,r28,1 ; addi r3,r3,0x65D0`. Consumer: WorldModule::
    // BridgeAIToEntityModules_PostPhysics @0x827A4F58 (0x827A4F80).
    const AIRaceCarInterface* OutputBuffer::GetAIRaceCarInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mAIRaceCarInterface;
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

    // X360 0x8279CD48 (R, :479) -- the TYPED const twin of the accessor above; same seat.
    const AIModuleResultInterface* OutputBuffer::GetAIModuleResultInterfaceConst() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mAIModuleResultInterface;
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
