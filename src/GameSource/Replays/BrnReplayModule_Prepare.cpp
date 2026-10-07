#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayRequestInterface.h"
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"
#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace BrnReplays
{
    // ARTIST 82652768. The derived stages are real ReplayModule members,
    // not a synthetic once-only boolean standing in for the module base.
    bool ReplayModule::Prepare(const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
    {
        if (static_cast<u32>(mePrepareStage)<2u)
        {
            mePrepareStage=E_PREPARESTAGE_MANAGER;
            if (!CgsModule::ModuleSingleBuffered::Prepare()) return false;
            mDebugComponent.Register();
            mpLinearMalloc=lpAllocatorList->GetLinearAllocator(KI_REPLAY_LINEAR_BANK);
            mpLinearMalloc->SetAlignment(16);
            mpcPrimaryStreamBuffer=static_cast<char*>(mpLinearMalloc->Malloc(0x80000));
            mpcHeaderStreamBuffer=static_cast<char*>(mpLinearMalloc->Malloc(0x4000));
            rw::ResourceDescriptor lDescriptor;
            lDescriptor.m_baseResourceDescriptors[0].m_size=0x80048;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment=16;
            mpSecondaryStreamBuffer=static_cast<MemBuffer*>(
                BrnResource::GetDebugAllocator()->DoAllocate(lDescriptor,nullptr).m_baseResources[0]);
            mpcHeaderBuffer=static_cast<char*>(mpLinearMalloc->Malloc(0x20000));
        }
        else if (static_cast<u32>(mePrepareStage)>=3u)
        {CGS_ASSERT(false,"Invalid Stage\n");return false;}
        // The actual binary writes MANAGER(1), even on the stage2 path.
        mePrepareStage=E_PREPARESTAGE_MANAGER;meReleaseStage=E_RELEASESTAGE_START;return true;
    }

    // ARTIST 8264B600. Runtime replacement asserts and clears the old buffer
    // before adopting the new serialiser; no synthetic prepare/null guard.
    void ReplayModule::StoreSerialisers(const ReplayIO::RequestInterface& lrRequestInterface)
    {
        for (s32 liId=0;liId<KI_NUM_SERIALISERS;++liId)
        {
            BaseSerialiser* lpSerialiser=lrRequestInterface.mapSerialisers[liId];
            if (!lpSerialiser || mapSerialisers[liId]==lpSerialiser) continue;
            if (mapSerialisers[liId] && mapSerialisers[liId]->GetBuffer())
            {
                CGS_ASSERT(false,"Don't currently support runtime de-allocation of serialisers - see Chris if you REALLY need this\n");
                mapSerialisers[liId]->SetBuffer(nullptr);
            }
            mapSerialisers[liId]=lpSerialiser;
            if (lpSerialiser->GetBufferSize()<=0) lpSerialiser->SetBuffer(nullptr);
            else
            {
                lpSerialiser->SetBuffer(mpLinearMalloc->Malloc(lpSerialiser->GetBufferSize()));
                if (!lpSerialiser->GetBuffer())
                {
                    CgsDev::Assert::BeginAssert();char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lMessage(lacMessage,sizeof(lacMessage));
                    lMessage<<"Could not allocate stream buffer for serialiser "<<lpSerialiser->GetName()<<"\n";
                    CgsDev::Assert::FireAssert(lacMessage,__FILE__,__LINE__);CgsDev::Assert::EndAssert();
                }
            }
            if (lpSerialiser->GetStaticBufferSize()<=0) lpSerialiser->SetStaticBuffer(nullptr);
            else
            {
                lpSerialiser->SetStaticBuffer(mpLinearMalloc->Malloc(lpSerialiser->GetStaticBufferSize()));
                if (!lpSerialiser->GetStaticBufferPtr())
                {
                    CgsDev::Assert::BeginAssert();char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lMessage(lacMessage,sizeof(lacMessage));
                    lMessage<<"Could not allocate static buffer for serialiser "<<lpSerialiser->GetName()<<"\n";
                    CgsDev::Assert::FireAssert(lacMessage,__FILE__,__LINE__);CgsDev::Assert::EndAssert();
                }
            }
        }
    }
}
