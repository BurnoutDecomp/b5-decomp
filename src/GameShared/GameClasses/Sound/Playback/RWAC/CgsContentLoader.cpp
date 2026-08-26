#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"   // complete Factory + Environment (the by-name allocator walk)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"

// CgsSound::Playback::ContentLoader<ResType> load/unload/update state-machine method family,
// reconstructed from BURNOUT_X360_ARTIST.XEX. The generic template lives in
// CgsGenericRwacContent.h; the methods are bodied as template members here and explicitly
// instantiated at the bottom of the file for BOTH resource-element types the X360 build attests
// (BinaryFileResource and AlignedBinaryFileResource), so every ContentLoader symbol is emitted.
//
// BinaryFileResource instantiation:
//   Load                          @ 0x826DCBF8
//   Unload                        @ 0x826A77B0
//   StartResourceModuleLoading    @ 0x826C6998
//   RestartResourceModuleLoading  @ 0x82690810
//   CancelResourceModuleLoading   @ 0x82690888
//   PurgeLoadData                 @ 0x826AADE8
//
// AlignedBinaryFileResource instantiation:
//   Load                          @ 0x826DD5C0
//   Unload                        @ 0x826AA7A8
//   RestartResourceModuleLoading  @ 0x82690C68
//   CancelResourceModuleLoading   @ 0x82690CE0
//   PurgeLoadData                 @ 0x826AAEA0
//   Update                        @ 0x826DD670
//   UpdateUnload                  @ 0x826C6C40
//   UpdateResourceModuleLoading   @ 0x826C6E40
//
// The two instantiations share one generic body store-for-store (proven by comparing the
// paired addresses above); only the load-service GetPathZone/allocator element type differs.

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
    // failure. Allocator via Content::mFactory(+0x08) -> Factory::mEnvironment(+0x0C) ->
    // Environment::mpAllocator(+0x30), all by name. Baked assert CgsContentLoader.cpp:243.
    template <class ResType>
    bool ContentLoader<ResType>::StartResourceModuleLoading(Content& arContent,
                                                            const ContentSpec& /*arSpec*/)
    {
        CGS_ASSERT(mpLoadData == 0, "0 == mpLoadData");

        // The X360 walk factory+0x0C -> +0x30 is Factory::mEnvironment ->
        // Environment::mpAllocator -- by name (2026-08-25 wave 3; the "+0x0C load
        // service" reading was a misnomer for the environment).
        rw::IResourceAllocator* lpAllocator =
            arContent.GetFactory().GetEnvironment().GetAllocator();

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

        // Factory::mEnvironment -> Environment::mpAllocator, by name (see
        // StartResourceModuleLoading above).
        rw::IResourceAllocator* lpAllocator =
            arContent.GetFactory().GetEnvironment().GetAllocator();

        // X360 free descriptor: slot0 = block to free (mpLoadData), rest zero.
        lpAllocator->Free(mpLoadData);
        mpLoadData = 0;
    }

    // Update @ 0x826DD670. Per-frame pump dispatched off the low 7 bits of the Content's
    // content-state byte (+0x1E): E_CONTENT_STATE_LOADING (2) advances the resource-module
    // load (resource-module method only), E_CONTENT_STATE_UNLOADING (4) advances the unload;
    // any other state is a no-op. Baked assert CgsContentLoader.cpp:135.
    template <class ResType>
    void ContentLoader<ResType>::Update(Content& arContent, const ContentSpec& arSpec)
    {
        const u32 luContentState =
            static_cast<u32>(arContent.mu8ContentState) & 0x7Fu;

        if (luContentState == CgsSound::Playback::E_CONTENT_STATE_LOADING)   // 2
        {
            if (arSpec.mu8LoadMethod != 1)
            {
                CGS_ASSERT(arSpec.mu8LoadMethod == 1, "Invalid Content load method");
                return;
            }
            UpdateResourceModuleLoading(arContent, arSpec);
        }
        else if (luContentState == CgsSound::Playback::E_CONTENT_STATE_UNLOADING) // 4
        {
            UpdateUnload(arContent, arSpec);
        }
    }

    // UpdateResourceModuleLoading @ 0x826C6E40. Drive the four-state resource-module load
    // machine (meState @ mpLoadData+0x00):
    //   E_RMLS_REQUEST(0)  -> resolve path zone 0, issue the load service's load request
    //                         (vtbl slot 0), advance to LOADING on success; else "LOAD FAILED".
    //   E_RMLS_LOADING(1)  -> once mpResource is populated (no longer the null ptr) advance to
    //                         POST_LOAD and publish the loaded data pointer into Content+0x18.
    //   E_RMLS_POST_LOAD(2)-> run the Content's post-load hook (vtbl slot +0x10); advance to
    //                         FINISHED when it reports done (falls through to FINISHED).
    //   E_RMLS_FINISHED(3) -> set the final content state (LOADED unless the request was
    //                         re-armed) and purge the load data.
    // Baked asserts CgsContentLoader.cpp:287 ("mpLoadData") / :311 ("LOAD FAILED").
    template <class ResType>
    void ContentLoader<ResType>::UpdateResourceModuleLoading(Content& arContent,
                                                             const ContentSpec& arSpec)
    {
        CGS_ASSERT(mpLoadData, "mpLoadData");

        ResourceModuleLoadData* lpLoadData = mpLoadData;
        switch (lpLoadData->meState)
        {
        case E_RMLS_REQUEST: // 0
        {
            char lacPath[128];
            bool lbOk = arSpec.GetPathZone(0, lacPath, sizeof(lacPath));
            if (lbOk)
            {
                // X360: (*mpLoadService->vtbl[0])(mpLoadService, mi16CurrentRequest,
                //         arSpec.mu8LoadMethod, lacPath, this) -- issue the load
                // request. (Phase B4: mpLoadService is the REAL typed
                // IContentLoadService, so the vtbl[0] dispatch IS this virtual
                // call -- DoServiceContentLoadRequest, the interface's only slot.)
                lbOk = arContent.mpLoadService->DoServiceContentLoadRequest(
                    static_cast<u32>(mpLoadData->mi16CurrentRequest),
                    static_cast<EContentLoadMethod>(arSpec.mu8LoadMethod),
                    lacPath,
                    this);
            }

            if (lbOk)
            {
                lpLoadData->meState = E_RMLS_LOADING; // 1
                return;
            }
            CGS_ASSERT(lbOk, "LOAD FAILED");
            return;
        }
        case E_RMLS_LOADING: // 1
            // Wait until mpResource is bound (X360 IsEqual(&NULLResourcePtr, mpResource)==false).
            if (!CgsResource::NULLResourcePtr.IsEqual(&mpResource))
            {
                lpLoadData->meState = E_RMLS_POST_LOAD; // 2
                // Publish the loaded main-memory resource pointer into Content+0x18.
                arContent.mu32DataSize =
                    *reinterpret_cast<u32*>(mpResource.operator->());
            }
            break;

        case E_RMLS_POST_LOAD: // 2
        {
            // The X360 vtbl[+0x10] dispatch is Content vtable slot 4 ==
            // DoOnPostLoad (DWARF CgsContent.h:271) -- by name (2026-08-25 wave 3).
            if (!arContent.DoOnPostLoad())
                break;
            lpLoadData->meState = E_RMLS_FINISHED; // 3
        }
        // fall through
        case E_RMLS_FINISHED: // 3
        {
            // LOADED unless the request was cancelled/re-armed mid-flight (mu8IsCancelled set),
            // in which case restart into E_CONTENT_STATE_LOADED-then-reload (state 3 vs 1).
            int liFinalState = 1;
            if (!mpLoadData->mu8IsCancelled)
                liFinalState = 3;
            arContent.SetContentState(liFinalState);
            PurgeLoadData(arContent, arSpec);
            break;
        }
        default:
            break;
        }
    }

    // UpdateUnload @ 0x826C6C40. Two-step unload pump keyed off meUnloadState (loader+0x24):
    //   E_US_PRE_UNLOAD(1)  -> wait for any in-flight load to release (mpLoadData cleared, or the
    //                          load service's release hook (vtbl slot +0x14) reports done), then
    //                          advance to E_US_UNLOADING.
    //   E_US_UNLOADING(2)   -> cancel + purge the load data, rebind mpResource to the null handle
    //                          (resource-module method only), flag the Content changed, and reset
    //                          meUnloadState to E_US_IDLE. Baked assert CgsContentLoader.cpp:193.
    template <class ResType>
    void ContentLoader<ResType>::UpdateUnload(Content& arContent, const ContentSpec& arSpec)
    {
        if (meUnloadState == E_US_PRE_UNLOAD) // 1
        {
            bool lbReady = (mpLoadData != 0);
            if (!lbReady)
            {
                // The X360 vtbl[+0x14] dispatch is Content vtable slot 5 ==
                // DoOnPreUnload (DWARF CgsContent.h:274) -- by name (2026-08-25
                // wave 3).
                lbReady = arContent.DoOnPreUnload();
            }
            if (lbReady)
                meUnloadState = E_US_UNLOADING; // 2
            return;
        }

        if (meUnloadState != E_US_UNLOADING) // 2
            return;

        if (arSpec.mu8LoadMethod == 1)
        {
            if (mpLoadData)
                CancelResourceModuleLoading(arContent, arSpec);
            PurgeLoadData(arContent, arSpec);
            mpResource = skNullResourceHandle;
        }
        else
        {
            CGS_ASSERT(arSpec.mu8LoadMethod == 1, "Invalid Content load method");
        }

        if ((arContent.mu8ContentState & 0x7F) != CgsSound::Playback::E_CONTENT_STATE_UNLOADED)
        {
            arContent.mu8ContentState = static_cast<u8>(
                CgsSound::Playback::E_CONTENT_STATE_UNLOADED |
                CgsSound::Playback::E_CONTENT_STATE_CHANGED); // 0x81
        }

        meUnloadState = E_US_IDLE; // 0
    }

    // Explicit instantiations: emit the ContentLoader symbols for both resource-element types the
    // X360 build attests -- BinaryFileResource (GenericRwacWaveContent) and
    // AlignedBinaryFileResource (GenericRwacReverbIRContent).
    template struct ContentLoader<CgsResource::BinaryFileResource>;
    template struct ContentLoader<CgsResource::AlignedBinaryFileResource>;
}
}
