// ============================================================================
// GameSource/Replays/BrnReplayModule_Prepare.cpp
//
// BrnReplays::ReplayModule::Prepare @0x82652768 and ::StoreSerialisers @0x8264B600.
//
// WHY A SEPARATE TU (the same reason ParticleModule_Lifecycle.cpp and
// BrnReplayPropSerialiserFrame_operator_assign.cpp are separate): these two bodies belong to
// BrnReplayModule.cpp, but that file also defines Update_Dispatch, whose
// GPUDiskWriteStream::Dispatch pulls Stream/BrnReplayGPUDiskWriteStream.cpp into the link --
// and that TU does not compile today (two u64 -> CgsFileSystem::Handle casts, :186/:220). It
// also defines the ctor, which BrnBaselineLinkStubs.cpp still stands in for. Splitting the two
// functions out lets the ALLOCATOR land without dragging the replay-stream closure in.
// DELETE THIS FILE and fold the bodies back into BrnReplayModule.cpp when that closure links.
//
// WHAT THEY ARE FOR: StoreSerialisers is the ONLY place in the engine that gives any
// BrnReplays::BaseSerialiser its stream buffer and its STATIC buffer, and Prepare is what
// acquires the linear region they are carved from. Until they ran,
// BrnEffects::EffectsModule::Update @0x8229EC28 returned at its `GetStaticLayout() == 0` guard
// before it ever reached HandleWheels -- so no tyre marks, sparks, debris or Lion effects.
// ============================================================================

#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"        // ReplayIO::RequestInterface
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"          // BaseSerialiser
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"  // AllocatorList
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"       // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // WriteToLog
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include <cstdio>                                                // snprintf

namespace BrnReplays
{
    // =========================================================================================
    // ReplayModule::Prepare  @0x82652768
    //   v4 = *(this + 0x228);                       // the module's own prepare stage
    //   if (v4 < 2) {
    //       *(this + 0x228) = 1;
    //       if (!ModuleSingleBuffered::Prepare(this)) return 0;
    //       DebugComponent::Register(this + 6297);
    //       linear = AllocatorList::GetLinearAllocator(allocatorList, 48);
    //       *(this + 2168) = linear;  LinearMalloc::SetAlignment(linear, 16);
    //       *(this + 2300) = LinearMalloc::Malloc(linear, 0x80000);
    //       *(this + 2312) = LinearMalloc::Malloc(linear, 0x4000);
    //       *(this + 2320) = rw::IResourceAllocator::AllocateMemoryResource(
    //                            BrnResource::GetDebugAllocator(), 524360, 16, 0);
    //       *(this + 2296) = LinearMalloc::Malloc(linear, 0x20000);
    //   }
    //   *(this + 0x228) = 1; *(this + 0x22C) = 0; return 1;
    //
    // ⭐ WHAT THIS UNBLOCKS, and why it is here at all: BaseSerialiser::mpStaticBuffer is
    // allocated in exactly ONE place in the whole engine -- StoreSerialisers, below, out of the
    // linear region THIS function acquires. With no Prepare there is no region, with no region
    // there is no static buffer, and BrnEffects::EffectsModule::Update @0x8229EC28 returns at
    // its `GetStaticLayout() == 0` guard before it ever reaches the wheel loop. Measured
    // (BRN_SKID_PROBE, 2026-09-02):
    //     [skid-gate] Update REACHED the per-system enables: trails=1 sparks=1 ...
    //     [skid-gate] EffectsModule::Update RETURNED EARLY at:
    //                 mEffectsSerialiser.GetStaticLayout() == 0
    // -- i.e. every tyre mark, spark, piece of debris and Lion effect in the game was behind
    // this one allocation.
    //
    // THE THREE MODULE-OWN BUFFERS ARE ALLOCATED, NOT SKIPPED: they are three LinearMalloc
    // bumps and leaving them out would silently move every later allocation's address. Their
    // CONSUMERS (the write/read stream machinery) are not reconstructed, so the pointers are
    // announced once and not stored -- the region is reserved exactly as the console reserves
    // it. The debug memory resource is the one piece skipped, and it says so.
    // =========================================================================================
    bool ReplayModule::Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
    {
        if (miPrepareStage >= 2)
        {
            miPrepareStage = 1;
            return true;
        }

        miPrepareStage = 1;

        if (lpAllocatorList == 0)
        {
            // The console cannot reach this: stage 8 always has the game-data output buffer.
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog(
                    "[replays] ReplayModule::Prepare called with a null allocator list -- "
                    "no linear region, so no serialiser gets a static buffer\n");
            }
            return false;
        }

        // `CgsDev::DebugComponent::Register(a1 + 6297)` -- the replay debug component is not
        // registered on this build (its component object is not modelled at that offset).
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog(
                    "[replays] NOT RECONSTRUCTED: ReplayModule::Prepare's "
                    "CgsDev::DebugComponent::Register(this + 6297) and its "
                    "rw::IResourceAllocator::AllocateMemoryResource(debug, 524360, 16, 0)\n");
            }
        }

        mpLinearMalloc = lpAllocatorList->GetLinearAllocator(KI_REPLAY_LINEAR_BANK);
        if (mpLinearMalloc == 0)
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog(
                    "[replays] ReplayModule::Prepare: allocator bank 48 has no linear "
                    "allocator -- serialiser buffers cannot be carved\n");
            }
            return false;
        }

        mpLinearMalloc->SetAlignment(16);

        // The three module-own bumps, in the console's own order and sizes.
        void* lpStreamBuffer = mpLinearMalloc->Malloc(0x80000);
        void* lpSmallBuffer  = mpLinearMalloc->Malloc(0x4000);
        void* lpThirdBuffer  = mpLinearMalloc->Malloc(0x20000);
        {
            char lacMsg[224];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[replays] ReplayModule::Prepare: linear region up (0x80000=%p 0x4000=%p "
                "0x20000=%p); their stream consumers are not reconstructed\n",
                lpStreamBuffer, lpSmallBuffer, lpThirdBuffer);
            CgsDev::Log::WriteToLog(lacMsg);
        }

        miPrepareStage = 1;
        return true;
    }

    // =========================================================================================
    // ReplayModule::StoreSerialisers  @0x8264B600
    //   for (id = 0; id < 11; ++id)
    //       if (RequestInterface::GetSerialiser(rq, id))
    //       {
    //           s = GetSerialiser(rq, id);
    //           if (mapSerialisers[id] != s)
    //           {
    //               if (mapSerialisers[id] && mapSerialisers[id]->mpBuffer)
    //                   assert("Don't currently support runtime de-allocation of serialisers
    //                           - see Chris if you REALLY need this", BrnReplayModule.cpp:1713);
    //               mapSerialisers[id] = s;
    //               n = s->miBufferSize;                       // +0x0C
    //               if (n <= 0) s->mpBuffer = 0;               // +0x08
    //               else      { s->mpBuffer = Malloc(linear, n);
    //                           assert(s->mpBuffer, "Could not allocate stream buffer for
    //                                   serialiser <name>", :1728); }
    //               m = s->miStaticBufferSize;                 // +0x24
    //               if (m <= 0) s->mpStaticBuffer = 0;         // +0x20
    //               else      { s->mpStaticBuffer = Malloc(linear, m);
    //                           assert(s->mpStaticBuffer, "Could not allocate static buffer for
    //                                   serialiser <name>", :1737); }
    //           }
    //       }
    // =========================================================================================
    void ReplayModule::StoreSerialisers(const ReplayIO::RequestInterface& lrRequestInterface)
    {
        for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
        {
            BaseSerialiser* lpSerialiser = lrRequestInterface.mapSerialisers[liId];
            if (lpSerialiser == 0)
                continue;
            if (mapSerialisers[liId] == lpSerialiser)
                continue;

            CGS_ASSERT(mapSerialisers[liId] == 0 || mapSerialisers[liId]->GetBuffer() == 0,
                       "Don't currently support runtime de-allocation of serialisers - see "
                       "Chris if you REALLY need this\n");   // BrnReplayModule.cpp:1713

            mapSerialisers[liId] = lpSerialiser;

            if (mpLinearMalloc == 0)
            {
                // Prepare has not run (or found no bank); the console cannot get here.
                static bool sbLogged = false;
                if (!sbLogged)
                {
                    sbLogged = true;
                    CgsDev::Log::WriteToLog(
                        "[replays] StoreSerialisers with no linear region -- every serialiser "
                        "keeps a null static buffer (ReplayModule::Prepare has not run)\n");
                }
                continue;
            }

            const s32 liBufferSize = lpSerialiser->GetBufferSize();
            if (liBufferSize <= 0)
            {
                lpSerialiser->SetBuffer(0);
            }
            else
            {
                lpSerialiser->SetBuffer(mpLinearMalloc->Malloc(static_cast<size_t>(liBufferSize)));
                CGS_ASSERT(lpSerialiser->GetBuffer() != 0,
                           "Could not allocate stream buffer for serialiser\n");   // :1728
            }

            const s32 liStaticSize = lpSerialiser->GetStaticBufferSize();
            if (liStaticSize <= 0)
            {
                lpSerialiser->SetStaticBuffer(0);
            }
            else
            {
                lpSerialiser->SetStaticBuffer(
                    mpLinearMalloc->Malloc(static_cast<size_t>(liStaticSize)));
                CGS_ASSERT(lpSerialiser->GetStaticBufferPtr() != 0,
                           "Could not allocate static buffer for serialiser\n");   // :1737
            }

            char lacMsg[224];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[replays] serialiser %d '%s' adopted: stream=%d bytes @%p static=%d bytes @%p\n",
                liId, lpSerialiser->GetName(), liBufferSize, lpSerialiser->GetBuffer(),
                liStaticSize, lpSerialiser->GetStaticBufferPtr());
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }
}
