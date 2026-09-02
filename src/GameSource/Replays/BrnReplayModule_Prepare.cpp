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
        // ONCE. The console's guard is `v4 = *(this + 0x228); if (v4 < 2) { ...allocate... }` and
        // +0x228 belongs to the MODULE BASE: CgsModule::ModuleSingleBuffered::Prepare owns that
        // stage and advances it past 1 once it has finished, which is what stops the allocations
        // running twice. This port's base does not expose that slot, so the same "already done"
        // fact is carried by name. Without it, every frame of loading stage 8 re-entered and
        // re-bumped the linear region by ~660 KB.
        // (⚠ This guard is NOT what fixed the null region -- see StoreSerialisers below. It was
        // written while chasing that, and it is kept because it is right, not because it was the
        // cause. Correcting the record rather than letting the banner take credit.)
        // DELETE-WHEN the base's prepare stage is reachable: then read it, as the console does.
        if (mbPrepared)
        {
            return true;
        }

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
                "[replays] ReplayModule::Prepare: this=%p linear=%p region up (0x80000=%p "
                "0x4000=%p 0x20000=%p); their stream consumers are not reconstructed\n",
                static_cast<void*>(this), static_cast<void*>(mpLinearMalloc),
                lpStreamBuffer, lpSmallBuffer, lpThirdBuffer);
            CgsDev::Log::WriteToLog(lacMsg);
        }

        mbPrepared = true;
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
        // ⭐⭐ THE PRECONDITION, AND IT IS THE WHOLE BUG THIS FUNCTION SHIPPED WITH.
        // On the console StoreSerialisers cannot run before Prepare: it is called from
        // ReplayModule::Update_PostSim, and the module is prepared during loading. On this build
        // the effects leg runs from GameMain and reached this function BEFORE the loading
        // screen's stage 8 had prepared the module. Measured (run 12, `this` printed on both
        // sides so the two-instance theory could be ruled out -- it was the same object):
        //     [replays] StoreSerialisers first call: this=00007FF742C5BCC0 linear=0000000000000000
        //                                            slots=00000000100
        //     [replays] StoreSerialisers with no linear region -- ... (Prepare has not run)
        //     [replays] ReplayModule::Prepare:       this=00007FF742C5BCC0 linear=00007FF742A30258
        // -- the adoption ran FIRST, with no region.
        //
        // The first version handled that by storing `mapSerialisers[id] = serialiser` and THEN
        // skipping the allocation with a `continue`. That is a silent drop of exactly the shape
        // this project keeps finding: the bookkeeping succeeded, the work did not, and because
        // the slot now matched, every later call -- including every call after Prepare had a
        // perfectly good region -- took the `mapSerialisers[id] == serialiser` early-out and
        // never allocated anything. mpStaticBuffer stayed null for the whole run and
        // EffectsModule::Update kept returning at its GetStaticLayout() guard.
        //
        // So: no region, no adoption. Return, leave the slots untouched, and let the next call
        // (after Prepare) do the whole job. The console's own invariant, enforced.
        if (mpLinearMalloc == 0)
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog(
                    "[replays] StoreSerialisers before ReplayModule::Prepare -- no linear region "
                    "yet, so NOTHING is adopted this call (adopting now would record the slot and "
                    "leave its buffers unallocated for ever)\n");
            }
            return;
        }

        // [replays] ONE-SHOT WITNESS. Run 10 adopted nothing with the call in the right place
        // and the read lock held, so the question is what the eleven slots actually hold when
        // this runs. Print them once rather than reason about it again. DELETE with the bring-up.
        {
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                char lacMsg[320];
                s32 liOffset = std::snprintf(lacMsg, sizeof(lacMsg),
                    "[replays] StoreSerialisers first call: this=%p linear=%p slots=",
                    static_cast<void*>(this), static_cast<void*>(mpLinearMalloc));
                for (s32 liSlot = 0; liSlot < KI_NUM_SERIALISERS && liOffset > 0
                     && liOffset < static_cast<s32>(sizeof(lacMsg)) - 8; ++liSlot)
                {
                    liOffset += std::snprintf(lacMsg + liOffset, sizeof(lacMsg) - liOffset,
                        "%c", lrRequestInterface.mapSerialisers[liSlot] != 0 ? '1' : '0');
                }
                std::snprintf(lacMsg + liOffset, sizeof(lacMsg) - liOffset, "\n");
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }

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
