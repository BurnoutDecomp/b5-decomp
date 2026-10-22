// ============================================================================
// GameSource/Replays/BrnReplayModule_Prepare.cpp
//
// BrnReplays::ReplayModule::Prepare and ::StoreSerialisers.
//
// WHAT THEY ARE FOR: StoreSerialisers is the ONLY place in the engine that gives any
// BrnReplays::BaseSerialiser its stream buffer and its STATIC buffer, and Prepare is what
// acquires the linear region they are carved from. Until they ran,
// BrnEffects::EffectsModule::Update returned at its `GetStaticLayout() == 0` guard
// before it ever reached HandleWheels -- so no tyre marks, sparks, debris or Lion effects.
// ============================================================================

#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"        // ReplayIO::RequestInterface
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"          // BaseSerialiser
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"  // AllocatorList
#include "GameSource/Resource/BrnResourceAllocator.h"               // GetDebugAllocator
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"       // CgsMemory::LinearMalloc
#include "GameShared/GameClasses/Development/Log/CgsLog.h"       // WriteToLog
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT
#include <cstdio>                                                // snprintf

namespace BrnReplays
{
    // ARTIST ReplayModule::Prepare @0x82652768. Preserve the derived stage
    // comparisons, base Prepare/Register order, allocator bank, and allocation order.
    bool ReplayModule::Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
    {
        const u32 luStage = static_cast<u32>(mePrepareStage);
        if (luStage < static_cast<u32>(E_PREPARE_STAGE_DONE))
        {
            mePrepareStage = E_PREPARE_STAGE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;

            mDebugComponent.Register();
            mpLinearMalloc = lpAllocatorList->GetLinearAllocator(KI_REPLAY_LINEAR_BANK);
            mpLinearMalloc->SetAlignment(16);
            mpcPrimaryStreamBuffer =
                static_cast<char*>(mpLinearMalloc->Malloc(0x80000));
            mpcHeaderStreamBuffer =
                static_cast<char*>(mpLinearMalloc->Malloc(0x4000));
            mpSecondaryStreamBuffer = static_cast<MemBuffer*>(
                BrnResource::GetDebugAllocator()->AllocateMemoryResource(0x80048, 16, nullptr));
            mpcHeaderBuffer =
                static_cast<char*>(mpLinearMalloc->Malloc(0x20000));
        }
        else if (luStage >= 3)
        {
            CGS_ASSERT(false, "Invalid Stage\n");
            return false;
        }

        mePrepareStage = E_PREPARE_STAGE_MANAGER;
        meReleaseStage = E_RELEASE_STAGE_START;
        return true;
    }

    // =========================================================================================
    // ReplayModule::StoreSerialisers
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
