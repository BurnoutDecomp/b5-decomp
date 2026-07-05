#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"

// CgsSound::Playback::ContentLoader<ResType> load/unload state-machine method family,
// reconstructed from BURNOUT_X360_ARTIST.XEX for the CgsResource::BinaryFileResource
// instantiation. The generic template lives in CgsGenericRwacContent.h; these six methods are
// bodied as template members and explicitly instantiated at the bottom of the file so the
// BinaryFileResource symbols the X360 build attests get emitted.
//
//   Load                          @ 0x826DCBF8
//   Unload                        @ 0x826A77B0
//   StartResourceModuleLoading    @ 0x826C6998
//   RestartResourceModuleLoading  @ 0x82690810
//   CancelResourceModuleLoading   @ 0x82690888
//   PurgeLoadData                 @ 0x826AADE8

namespace CgsSound
{
namespace Playback
{
    // X360: the invalid/default resource-handle sentinel used to reset mpResource to
    // "no resource" (X360 &dword_830086E0). Modelled as a file-local zero ResourceHandle
    // (same idiom as WheelList/ChallengeList).
    namespace
    {
        const CgsResource::ResourceHandle skNullResourceHandle = {};
    }

    // Load @ 0x826DCBF8. Only the resource-module load method (mu8LoadMethod==1) is supported;
    // a load service must be present. If a load is already in flight (mpLoadData set) restart it,
    // else begin a fresh resource-module load. Baked asserts CgsContentLoader.cpp:68 / :52.
    template <class ResType>
    bool ContentLoader<ResType>::Load(Content& arContent, const ContentSpec& arSpec)
    {
        if (arSpec.mu8LoadMethod != 1)
        {
            CGS_ASSERT(arSpec.mu8LoadMethod == 1, "Invalid Content load method");
            return false;
        }

        CGS_ASSERT(arContent.mpLoadService, "lContent.GetLoadService()");

        if (mpLoadData)
        {
            RestartResourceModuleLoading(arContent, arSpec);
            return true;
        }
        return StartResourceModuleLoading(arContent, arSpec);
    }

    // Unload @ 0x826A77B0. Begin an unload: cancel any in-flight module load, advance the loader
    // to the pre-unload state, flag the Content as unloading, and rebind mpResource to the null
    // resource handle. Only the resource-module load method (==1) is supported. Baked assert
    // CgsContentLoader.cpp:106.
    template <class ResType>
    bool ContentLoader<ResType>::Unload(Content& arContent, const ContentSpec& arSpec)
    {
        if (arSpec.mu8LoadMethod != 1)
        {
            CGS_ASSERT(arSpec.mu8LoadMethod == 1, "Invalid Content load method");
            return false;
        }

        if (mpLoadData)
            CancelResourceModuleLoading(arContent, arSpec);

        meUnloadState = E_US_PRE_UNLOAD;   // +0x24 (stw 1)

        if ((arContent.mu8ContentState & 0x7F) != CgsSound::Playback::E_CONTENT_STATE_UNLOADING)
        {
            arContent.mu8ContentState = static_cast<u8>(
                CgsSound::Playback::E_CONTENT_STATE_UNLOADING | CgsSound::Playback::E_CONTENT_STATE_CHANGED); // 0x84
        }

        // X360: CreateFromHandle(&mpResource, &NULLResourcePtr-sentinel) -- rebind mpResource to
        // the null resource. Modelled via the public ResourcePtr assignment.
        mpResource = skNullResourceHandle;
        return true;
    }

    // StartResourceModuleLoading @ 0x826C6998. Allocate an 8-byte ResourceModuleLoadData block
    // through the factory's RenderWare IResourceAllocator, initialise it (state=request, current
    // request=1, not cancelled), mark the Content loading. Returns false only on allocation
    // failure. Allocator via Content.mFactory(+0x08) -> load service(+0x0C) -> allocator(+0x30).
    // Baked assert CgsContentLoader.cpp:243.
    template <class ResType>
    bool ContentLoader<ResType>::StartResourceModuleLoading(Content& arContent,
                                                            const ContentSpec& /*arSpec*/)
    {
        CGS_ASSERT(mpLoadData == 0, "0 == mpLoadData");

        u8* lpFactory      = reinterpret_cast<u8*>(&arContent.mFactory);
        u8* lpLoadService  = *reinterpret_cast<u8**>(lpFactory + 0x0C);
        rw::IResourceAllocator* lpAllocator =
            *reinterpret_cast<rw::IResourceAllocator**>(lpLoadService + 0x30);

        // X360 stack build: a FIVE-entry serialised descriptor (40B). slot0 =
        // { sizeof(ResourceModuleLoadData)==8, 4 }, slots1..4 = { 0, 1 }.
        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 8;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (u32 luIndex = 1; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }

        // Bridge the X360-faithful <5> build to the PC <4> DoAllocate ABI (inert 5th entry).
        const rw::ResourceDescriptor& lAbiDescriptor =
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor);
        rw::Resource lResource = lpAllocator->DoAllocate(lAbiDescriptor, "ResourceModuleLoadData");

        mpLoadData = static_cast<ResourceModuleLoadData*>(lResource.m_baseResources[0]);
        if (!mpLoadData)
            return false;

        mpLoadData->meState            = E_RMLS_REQUEST; // +0x00 (0)
        mpLoadData->mi16CurrentRequest = 1;              // +0x04 (sth)
        mpLoadData->mu8IsCancelled     = 0;              // +0x06

        if ((arContent.mu8ContentState & 0x7F) != CgsSound::Playback::E_CONTENT_STATE_LOADING)
        {
            arContent.mu8ContentState = static_cast<u8>(
                CgsSound::Playback::E_CONTENT_STATE_LOADING | CgsSound::Playback::E_CONTENT_STATE_CHANGED); // 0x82
        }
        return true;
    }

    // RestartResourceModuleLoading @ 0x82690810. Re-arm an already-allocated load request: bump
    // the request id, clear the cancel flag, reset the state machine to E_RMLS_REQUEST.
    // Baked assert CgsContentLoader.cpp:269.
    template <class ResType>
    void ContentLoader<ResType>::RestartResourceModuleLoading(Content& /*arContent*/,
                                                              const ContentSpec& /*arSpec*/)
    {
        CGS_ASSERT(mpLoadData, "mpLoadData");

        ++mpLoadData->mi16CurrentRequest;     // +0x04 (sth after lhz+1)
        mpLoadData->mu8IsCancelled = 0;        // +0x06
        mpLoadData->meState = E_RMLS_REQUEST;  // +0x00 (== 0)
    }

    // CancelResourceModuleLoading @ 0x82690888. Mark the in-flight resource-module load as
    // cancelled. mpLoadData @ loader+0x20, mu8IsCancelled @ +6. Baked assert CgsContentLoader.cpp:360.
    template <class ResType>
    void ContentLoader<ResType>::CancelResourceModuleLoading(Content& /*arContent*/,
                                                            const ContentSpec& /*arSpec*/)
    {
        CGS_ASSERT(mpLoadData, "mpLoadData");
        mpLoadData->mu8IsCancelled = 1;
    }

    // PurgeLoadData @ 0x826AADE8. Free the per-load ResourceModuleLoadData block through the
    // factory's RenderWare IResourceAllocator, then clear mpLoadData. Allocator reached via
    // Content.mFactory(+0x08) -> load service(+0x0C) -> resource allocator(+0x30). Baked assert
    // CgsContentLoader.cpp:226.
    //
    // X360 builds a five-entry free Resource whose slot0 = mpLoadData and slots1..4 = 0, then
    // virtual-dispatches the allocator's DoFree (vtbl slot +0x14). The committed PC
    // rw::IResourceAllocator models the release surface as Free(void*, size) (the block to free is
    // the slot-0 base pointer == mpLoadData); route the free through it.
    template <class ResType>
    void ContentLoader<ResType>::PurgeLoadData(Content& arContent, const ContentSpec& arSpec)
    {
        if (!mpLoadData)
            return;

        CGS_ASSERT(arSpec.mu8LoadMethod == 1, "Invalid Content load method");

        u8* lpFactory      = reinterpret_cast<u8*>(&arContent.mFactory);
        u8* lpLoadService  = *reinterpret_cast<u8**>(lpFactory + 0x0C);
        rw::IResourceAllocator* lpAllocator =
            *reinterpret_cast<rw::IResourceAllocator**>(lpLoadService + 0x30);

        // X360 free descriptor: slot0 = block to free (mpLoadData), rest zero.
        lpAllocator->Free(mpLoadData);
        mpLoadData = 0;
    }

    // Explicit instantiation: emit the BinaryFileResource loader symbols the X360 build attests.
    template struct ContentLoader<CgsResource::BinaryFileResource>;
}
}
