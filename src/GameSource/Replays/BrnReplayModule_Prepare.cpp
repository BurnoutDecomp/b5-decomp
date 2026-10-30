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
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT

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
                BrnResource::GetDebugAllocator()->AllocateMemoryResource(0x80048, 16, 0));
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
        for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
        {
            BaseSerialiser* lpSerialiser = lrRequestInterface.mapSerialisers[liId];
            if (lpSerialiser == 0)
                continue;
            if (mapSerialisers[liId] == lpSerialiser)
                continue;

            if (mapSerialisers[liId] && mapSerialisers[liId]->GetBuffer())
            {
                CGS_ASSERT(false, "Don't currently support runtime de-allocation of serialisers - see "
                           "Chris if you REALLY need this\n");
                mapSerialisers[liId]->SetBuffer(0);
            }

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

        }
    }
}
