#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h
// ============================================================================
// Real home (X360 assert strings, e.g.
// "..\\..\\..\\GameSource\\World/AI/BrnAIModuleIO.h" and
// "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\world\\ai\\BrnAIModuleIO.h")
// of BrnAI::AIModuleIO::OutputBuffer -- the per-frame buffer the AI module
// publishes and the world bridges consume.
//
// ============================================================================
// ⛔⛔ 2026-08-25 (aimodule wave) -- THIS BUFFER WAS A LATENT ~108 KB HEAP OVERWRITE
// ============================================================================
// Until this wave the struct declared NO data members at all and its eleven accessors
// returned `reinterpret_cast<u8*>(this) + <X360 byte offset>`, the highest of them
// this + 0x1AF30 (110448). That is only valid on the CONSOLE, where the buffer really is
// >110 KB of contiguous payload and CgsModule::IOBuffer is one byte. On this host
// `sizeof(CgsModule::IOBuffer) == 1` (a single FlagSet8) and OutputBuffer added nothing,
// so `sizeof(OutputBuffer) == 1` -- and IOBufferStack::CreateIOBuffer<T> allocates
// exactly sizeof(T). Every accessor therefore handed out a pointer up to ~108 KB PAST a
// ONE-BYTE allocation, into the middle of whatever the frame stack allocated next.
//
// Nothing had fired yet only because every consumer of those accessors is still a boot
// gate. That is precisely the shape the crash-exit wave paid for twice in one day:
// ⭐⭐ AN UNCONSTRUCTED / UNBACKED BUFFER IS INVISIBLE UNTIL SOMETHING PUTS DATA IN IT --
// un-gating a producer CREATES the fault, it does not reveal it. AIModule::LoadMapData
// (this wave) is exactly such a producer: it takes GetAIResourceRequestInterface() and
// LoadBundle()s through it.
//
// FIX: the buffer now carries REAL, TYPED members in the X360's attested member ORDER,
// and every accessor returns the address of the member it names. The X360 byte offsets
// are kept below as the ORDER/IDENTITY authority (they are what proves which member each
// accessor names); they are NOT reproduced as host offsets, because they cannot be --
// pointer-width and alignment differ throughout. Nothing outside this header pins them:
// every consumer goes through an accessor or takes `&member`.
//
// Attested accessor -> offset -> member map (base = this):
//   GetAIResourceRequestInterface  @0x8276DA70 W -> this + 0x4      (4)      :~409
//   GetAIResourceRequestInterface  @0x8279CAA8 R -> this + 0x4      (4)      :423
//        size check: RequestInterface<4096> == 4096 + 16 == 0x1010 == 0x1014 - 0x4  ✓ EXACT
//        (the W twin is an ARTIST export HOLE -- no JSON -- but LoadMapData's xrefs_from
//         names it `BrnAI::AIModuleIO::OutputBuffer::GetAIResourceR...` @0x8276DA70, and it
//         sits directly after GetVehicleDri @0x8276D9C8 in the WRITE-side accessor group,
//         whose members every one test the write bit. LoadMapData LockForWrite()s the buffer
//         before calling it, which is what makes it the write twin and not the read one.)
//   GetRouteResponseQueueForWrite  @0x8276DB18 W -> this + 0x1014   (4116)   :430
//   GetRouteResponseQueue          @0x8279CB50 R -> this + 0x1014   (4116)   :437
//   GetVehicleDriverInterface      @0x8276D9C8 W -> this + 0x15120  (86304)  :402
//   GetVehicleInterface            @0x8279CA00 R -> this + 0x15120  (86304)  :409
//   GetAIRaceCarInterface          @0x8276DBC0 W -> this + 0x165D0  (91600)  :444
//   GetAICarOutputInterface        @0x8276DC68 W -> this + 0x16A60  (92768)  :458
//   GetAICarOutputInterfaceForRead @0x8279CCA0 R -> this + 0x16A60  (92768)  :465
//   GetAIModuleResultInterface(W)  @0x8276DD10 W -> this + 0x17F50  (98128)  :472
//   GetAIModuleResultInterface(R)  @0x8279CD48 R -> this + 0x17F50  (98128)  :479
//   GetGameEventQueueForRead       @0x8279C658 R -> this + 0x1AF30  (110448) :232
//   GetGameEventQueue              @0x8276D680 W -> this + 0x1AF30  (110448) :233
//
// ⭐ TWO ACCESSOR NAMES WERE WRONG, AND THE OFFSETS SAY SO
//   * the old `GetAIResultInterface` returned this+0x4, which the exact 0x1010 size match
//     above proves is the AI RESOURCE REQUEST interface -- the same thing the (declared-
//     only) GetAIResourceRequestInterface names. They were the same accessor under two
//     names, one of which pointed at the wrong concept. Unified.
//   * the old `GetAIOutputBufferHeader` returned this+0x1014, and the ARTIST body at
//     0x8279CB50 (unnamed, `sub_8279CB50`) returns the SAME this+0x1014 under a read lock
//     at BrnAIModuleIO.h:437 -- the read twin of the write-side :430. The tree's
//     declared-only `GetRouteResponseQueue()` is that read twin, so +0x1014 IS the route
//     response queue, not an untyped "header". Renamed; the old spelling is gone (it had
//     no caller outside this group -- verified by grep before the rename).
//
// Lock-bit guard per the recurring IOBuffer prologue:
//   read-lock  (status>>4 & 1) => IsBufferLockedForReading()  ("Not locked for reading")
//   write-lock (status>>3 & 1) => IsBufferLockedForWriting()  ("Not locked for writing")
// The X360 tests WHICHEVER bit the asm names -- some read-suffixed getters check
// the read bit, some the write bit; reproduced verbatim (not "fixed"). The
// X360-baked file path + line-number assert args are dropped per project policy.

#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"            // RouteResponseQueue
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"     // AICarOutputInterface
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"      // AIRaceCarInterface (mAIRaceCarInterface, DWARF :240)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"  // BrnPhysics::Vehicle::VehicleDriverInputInterface (mVehicleDriverInterface, DWARF :239)
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"  // AIModuleResultInterface
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"      // RequestInterface<N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // VariableEventQueue<N,A>
#include "types.hpp"                                     // u8
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer

namespace BrnAI
{
namespace AIModuleIO
{
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // Attested X360 member start offsets (bytes from this). ORDER/IDENTITY authority
        // only -- see the banner: the host layout cannot and does not reproduce them.
        enum EMemberOffset
        {
            KU_AI_RESOURCE_REQUEST_OFFSET     = 0x4,
            KU_ROUTE_RESPONSE_QUEUE_OFFSET    = 0x1014,
            KU_VEHICLE_DRIVER_OFFSET          = 0x15120,
            KU_AI_RACE_CAR_INTERFACE_OFFSET   = 0x165D0,
            KU_AI_CAR_OUTPUT_INTERFACE_OFFSET = 0x16A60,
            KU_AI_MODULE_RESULT_OFFSET        = 0x17F50,
            KU_GAME_EVENT_QUEUE_OFFSET        = 0x1AF30,
        };

        // ---- ADDITIVE (aimodule wave 2026-08-25) -------------------------------------
        // The buffer's own Construct. CgsIOBufferStack::CreateIOBuffer<T> calls T::Construct()
        // statically bound to T; with no override here it bound to the BASE
        // CgsModule::IOBuffer::Construct and every embedded queue kept its unconstructed
        // state (mpEvents == nullptr for the event queues). The crash-exit wave measured two
        // access violations from exactly that omission on three sibling buffers the same day
        // ("memcpy+0x131 <- ...::SetCrashInterface"), so this override lands WITH the first
        // producer that writes into the buffer rather than after it.
        void Construct();

        // The AI module's resource request interface. Written by AIModule::LoadMapData
        // (LoadBundle "AI.dat" + AcquireResource "WorldMapData") through the WRITE twin
        // @0x8276DA70; read by WorldModule::BridgeAIModuleToOutput through the READ twin
        // @0x8279CAA8. Each tests the lock bit its own console body tests.
        BrnResource::GameDataIO::RequestInterface<4096>*       GetAIResourceRequestInterface();
        const BrnResource::GameDataIO::RequestInterface<4096>* GetAIResourceRequestInterface() const;

        // X360 0x8279CB50 (R, :437) / 0x8276DB18 (W, :430) -- the route response queue.
        const RouteMapModuleIO::RouteResponseQueue* GetRouteResponseQueue() const;
        u8*                                         GetRouteResponseQueueForWrite();

        // X360 0x8276DC68 (W, :458) / 0x8279CCA0 (R, :465).
        const AICarOutputInterface* GetAICarOutputInterfaceConst() const;
        u8*                         GetAICarOutputInterface();
        u8*                         GetAICarOutputInterfaceForRead();

        // X360 0x8279C658 (R, :232) / 0x8276D680 (W, :233).
        const CgsModule::VariableEventQueue<1536, 16>* GetGameEventQueueConst() const;
        u8*                                            GetGameEventQueueForRead();
        u8*                                            GetGameEventQueue();

        // X360 0x8276D9C8 (W, :402) / 0x8279CA00 (R, :409).
        // RETYPED 2026-09-03 (aiwave lane A4): the member is the real
        // BrnPhysics::Vehicle::VehicleDriverInputInterface (DWARF :239, `OutputBuffer::
        // VehicleDriverInputInterface mVehicleDriverInterface`; the console Construct
        // @0x8278AD2C calls Vehicle::VehicleDriverInputInterface::Construct on it and
        // host sizeof == the console's 5296). The DWARF pair is :197 (W) / :200 (R const);
        // the R const twin is what WorldModule::BridgeAIModuleToPhysicsModule @0x827AAAA8
        // walks (VariableEventQueue<5040,16>::GetLength/GetFirstEvent/GetNextEvent on it).
        // GetVehicleInterface() keeps its pre-wave u8* spelling of the same read seat.
        typedef BrnPhysics::Vehicle::VehicleDriverInputInterface VehicleDriverInputInterface;   // DWARF :~60
        VehicleDriverInputInterface*       GetVehicleDriverInterface();           // :197 (W)
        const VehicleDriverInputInterface* GetVehicleDriverInterface() const;     // :200 (R) X360 0x8279CA00
        u8*                                GetVehicleInterface();                 // pre-wave spelling of the R seat (kept)

        // X360 0x8276DBC0 (W, :444) / 0x8279CBF8 (R, :451 -- export HOLE, disassembled from
        // the image: bit-4 test, "Not locked for reading\n", returns this+0x165D0).
        // RETYPED 2026-09-03 (aiwave lane A4): the member is the real AIRaceCarInterface
        // (DWARF :240; host sizeof == the console's 0x490, pointer-free). DWARF pair :215 / :218.
        AIRaceCarInterface*       GetAIRaceCarInterface();          // :215 (W)
        const AIRaceCarInterface* GetAIRaceCarInterface() const;    // :218 (R) X360 0x8279CBF8

        // X360 0x8276DD10 (W, :472) / 0x8279CD48 (R, :479).
        u8* GetAIModuleResultInterfaceForWrite();
        u8* GetAIModuleResultInterface();
        // ADDITIVE 2026-08-26 (resetpump wave), same precedent as GetAICarOutputInterfaceConst
        // above: the SAME read-locked seat (X360 0x8279CD48, this+0x17F50), typed and const, so
        // WorldModule::BridgeAIToEntityModules_PrePhysics -- which takes the buffer by const
        // pointer exactly as the console does -- can reach the interface BY NAME. The untyped
        // u8* twin stays for the callers that hand the raw seat on.
        const AIModuleResultInterface* GetAIModuleResultInterfaceConst() const;

    private:
        // @X360 +0x0004. Exact-size confirmed: 4096 + 16 == 0x1010 == 0x1014 - 0x4.
        BrnResource::GameDataIO::RequestInterface<4096> mAIResourceRequestInterface;

        // @X360 +0x1014.
        RouteMapModuleIO::RouteResponseQueue mRouteResponseQueue;

        // @X360 +0x15120 / +0x165D0 -- REAL TYPES since 2026-09-03 (aiwave lane A4). They were
        // u8[0x14B0] / u8[0x490] blobs ("replace with the real types when their own TUs land");
        // both TUs are on the bat (BrnVehicleDriverInputInterface.cpp, BrnAIRaceCarInterface.cpp)
        // and both types are pointer-free, so the console extents (0x14B0 == 5296, 0x490 ==
        // 1168) are the host sizes too -- pinned by static_asserts in the .cpp.
        VehicleDriverInputInterface mVehicleDriverInterface;    // DWARF :239
        AIRaceCarInterface          mAIRaceCarInterface;        // DWARF :240

        // @X360 +0x16A60 / +0x17F50 / +0x1AF30 -- all three have real reconstructed types.
        AICarOutputInterface                   mAICarOutputInterface;
        AIModuleResultInterface                mAIModuleResultInterface;
        CgsModule::VariableEventQueue<1536, 16> mGameEventQueue;
    };
}
}
