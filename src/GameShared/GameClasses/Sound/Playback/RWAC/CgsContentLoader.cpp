#define CGS_RESOURCE_PTR_MATCH_RELATIVE_ASSERTS // Both typed accessors reference 0x82007258 in ARTIST.
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"   // complete Factory + Environment (the by-name allocator walk)
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"
#include "GameShared/GameClasses/System/Resource/CgsAlignedBinaryFileResource.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// ARTIST's __FILE__ for these template bodies (CgsSound::Playback, not the RWAC directory);
// every MessageAt line below is ARTIST's li r5 in the BinaryFileResource instance.
#define KAC_CONTENT_LOADER_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsContentLoader.cpp"
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
    // Load @ 0x826DCBF8. Only the resource-module load method (mu8LoadMethod==1) is supported;
    // a load service must be present. If a load is already in flight (mpLoadData set) restart it,
    // else begin a fresh resource-module load. Baked asserts CgsContentLoader.cpp:68 / :52.
    template <class ResType>
    bool ContentLoader<ResType>::Load(Content& arContent, const ContentSpec& arSpec)
    {
        if (arSpec.mu8LoadMethod != 1)
        {
            CGS_ASSERT_AT(arSpec.mu8LoadMethod == 1, "Invalid Content load method", KAC_CONTENT_LOADER_FILE, 68);
            return false;
        }

        CGS_ASSERT_AT(arContent.GetLoadService(), "lContent.GetLoadService()", KAC_CONTENT_LOADER_FILE, 52);

        // ARTIST 0x826DCC88: the fresh-load call is the fall-through (bne to the restart).
        if (!mpLoadData)
            return StartResourceModuleLoading(arContent, arSpec);

        RestartResourceModuleLoading(arContent, arSpec);
        return true;
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
            CGS_ASSERT_AT(arSpec.mu8LoadMethod == 1, "Invalid Content load method", KAC_CONTENT_LOADER_FILE, 106);
            return false;
        }

        if (mpLoadData)
            CancelResourceModuleLoading(arContent, arSpec);

        meUnloadState = E_US_PRE_UNLOAD;   // +0x24 (stw 1)

        // ARTIST 0x826A7824..0x826A7830: Content::SetContentState(E_CONTENT_STATE_UNLOADING)
        // inlined (GetContentState() compared signed, then the state stored with CHANGED).
        arContent.SetContentState(CgsSound::Playback::E_CONTENT_STATE_UNLOADING);

        // X360: CreateFromHandle(&mpResource, &NULLResourcePtr-sentinel) -- rebind mpResource to
        // the null resource. Modelled via the public ResourcePtr assignment.
        mpResource = CgsResource::NULLResourcePtr;
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
        CGS_ASSERT_AT(mpLoadData == 0, "0 == mpLoadData", KAC_CONTENT_LOADER_FILE, 243);

        mpLoadData = static_cast<ResourceModuleLoadData*>(
            arContent.GetFactory().GetEnvironment().Allocate(
                sizeof(ResourceModuleLoadData), __alignof(ResourceModuleLoadData),
                "ResourceModuleLoadData"));
        if (mpLoadData)
        {
            mpLoadData->meState = E_RMLS_REQUEST;
            mpLoadData->mi16CurrentRequest = 1;
            mpLoadData->mu8IsCancelled = 0;
            arContent.SetContentState(E_CONTENT_STATE_LOADING);
            return true;
        }

        return false;
    }

    // RestartResourceModuleLoading @ 0x82690810. Re-arm an already-allocated load request: bump
    // the request id, clear the cancel flag, reset the state machine to E_RMLS_REQUEST.
    // Baked assert CgsContentLoader.cpp:269.
    template <class ResType>
    void ContentLoader<ResType>::RestartResourceModuleLoading(Content& /*arContent*/,
                                                              const ContentSpec& /*arSpec*/)
    {
        CGS_ASSERT_AT(mpLoadData, "mpLoadData", KAC_CONTENT_LOADER_FILE, 269);

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
        CGS_ASSERT_AT(mpLoadData, "mpLoadData", KAC_CONTENT_LOADER_FILE, 360);
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

        CGS_ASSERT_AT(arSpec.mu8LoadMethod == 1, "Invalid Content load method", KAC_CONTENT_LOADER_FILE, 226);

        // Factory::mEnvironment -> Environment::mpAllocator, by name (see
        // StartResourceModuleLoading above).
        arContent.GetFactory().GetEnvironment().Free(mpLoadData);
        mpLoadData = 0;
    }

    // Update @ 0x826DD670. Per-frame pump dispatched off the low 7 bits of the Content's
    // content-state byte (+0x1E): E_CONTENT_STATE_LOADING (2) advances the resource-module
    // load (resource-module method only), E_CONTENT_STATE_UNLOADING (4) advances the unload;
    // any other state is a no-op. Baked assert CgsContentLoader.cpp:135.
    template <class ResType>
    void ContentLoader<ResType>::Update(Content& arContent, const ContentSpec& arSpec)
    {
        // ARTIST 0x826DCCB8..0x826DCCC8: a switch on GetContentState() (masked with ~CHANGED,
        // compared signed); the compare chain falls through into the UNLOADING arm.
        switch (arContent.GetContentState())
        {
        case CgsSound::Playback::E_CONTENT_STATE_LOADING:   // 2
            if (arSpec.mu8LoadMethod != 1)
            {
                CGS_ASSERT_AT(arSpec.mu8LoadMethod == 1, "Invalid Content load method", KAC_CONTENT_LOADER_FILE, 135);
                return;
            }
            UpdateResourceModuleLoading(arContent, arSpec);
            break;
        case CgsSound::Playback::E_CONTENT_STATE_UNLOADING: // 4
            UpdateUnload(arContent, arSpec);
            break;
        default:
            break;
        }
    }

    // UpdateResourceModuleLoading @ 0x826C6E40. Drive the four-state resource-module load
    // machine (meState @ mpLoadData+0x00):
    //   E_RMLS_REQUEST(0)  -> resolve path zone 0, issue the load service's load request
    //                         (vtbl slot 0), advance to LOADING on success; else "LOAD FAILED".
    //   E_RMLS_LOADING(1)  -> once mpResource is populated (no longer the null ptr) advance to
    //                         POST_LOAD and publish the resource payload size.
    //   E_RMLS_POST_LOAD(2)-> run the Content's post-load hook (vtbl slot +0x10); advance to
    //                         FINISHED when it reports done (falls through to FINISHED).
    //   E_RMLS_FINISHED(3) -> publish UNLOADED for a cancelled request or LOADED on success,
    //                         then purge the load data.
    // Baked asserts CgsContentLoader.cpp:287 ("mpLoadData") / :311 ("LOAD FAILED").
    template <class ResType>
    void ContentLoader<ResType>::UpdateResourceModuleLoading(Content& arContent,
                                                             const ContentSpec& arSpec)
    {
        CGS_ASSERT_AT(mpLoadData, "mpLoadData", KAC_CONTENT_LOADER_FILE, 287);

        ResourceModuleLoadData* lpLoadData = mpLoadData;
        switch (lpLoadData->meState)
        {
        case E_RMLS_REQUEST: // 0
        {
            char lacPath[128];
            if (arSpec.GetPathZone(0, lacPath, sizeof(lacPath)) &&
                arContent.GetLoadService()->DoServiceContentLoadRequest(
                    static_cast<u32>(mpLoadData->mi16CurrentRequest),
                    static_cast<EContentLoadMethod>(arSpec.mu8LoadMethod),
                    lacPath, this))
            {
                lpLoadData->meState = E_RMLS_LOADING;
            }
            else
            {
                CGS_FAIL_AT("LOAD FAILED", KAC_CONTENT_LOADER_FILE, 311);
            }
            return;
        }
        case E_RMLS_LOADING: // 1
            // Wait until mpResource is bound (X360 IsEqual(&NULLResourcePtr, mpResource)==false).
            if (mpResource != CgsResource::NULLResourcePtr)
            {
                lpLoadData->meState = E_RMLS_POST_LOAD; // 2
                arContent.mu32DataSize = mpResource->GetDataSize();
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
            if (mpLoadData->mu8IsCancelled)
            {
                arContent.SetContentState(E_CONTENT_STATE_UNLOADED);
                PurgeLoadData(arContent, arSpec);
            }
            else
            {
                arContent.SetContentState(E_CONTENT_STATE_LOADED);
                PurgeLoadData(arContent, arSpec);
            }
            break;
        }
        default:
            break;
        }
    }

    // UpdateUnload @ 0x826C6C40. Two-step unload pump keyed off meUnloadState (loader+0x24):
    //   E_US_PRE_UNLOAD(1) -> advance when load data is present or Content's pre-unload hook is ready.
    //   E_US_UNLOADING(2)   -> cancel + purge the load data, rebind mpResource to the null handle
    //                          (resource-module method only), flag the Content changed, and reset
    //                          meUnloadState to E_US_IDLE. Baked assert CgsContentLoader.cpp:193.
    template <class ResType>
    void ContentLoader<ResType>::UpdateUnload(Content& arContent, const ContentSpec& arSpec)
    {
        switch (meUnloadState)
        {
        case E_US_PRE_UNLOAD:
            if (mpLoadData || arContent.DoOnPreUnload())
                meUnloadState = E_US_UNLOADING;
            break;

        case E_US_UNLOADING:
            if (arSpec.mu8LoadMethod != E_CONTENT_LOAD_RESOURCE_MODULE)
            {
                CGS_FAIL_AT("Invalid Content load method", KAC_CONTENT_LOADER_FILE, 193);
            }
            else
            {
                if (mpLoadData)
                    CancelResourceModuleLoading(arContent, arSpec);
                PurgeLoadData(arContent, arSpec);
                mpResource = CgsResource::NULLResourcePtr;
            }
            arContent.SetContentState(E_CONTENT_STATE_UNLOADED);
            meUnloadState = E_US_IDLE;
            break;

        default:
            break;
        }
    }

    // Explicit instantiations: emit the ContentLoader symbols for both resource-element types the
    // X360 build attests -- BinaryFileResource (GenericRwacWaveContent) and
    // AlignedBinaryFileResource (GenericRwacReverbIRContent).
    #define CGS_INSTANTIATE_CONTENT_LOADER(RES_TYPE) \
        template bool ContentLoader<RES_TYPE>::Load(Content&, const ContentSpec&); \
        template bool ContentLoader<RES_TYPE>::Unload(Content&, const ContentSpec&); \
        template void ContentLoader<RES_TYPE>::Update(Content&, const ContentSpec&); \
        template void ContentLoader<RES_TYPE>::UpdateUnload(Content&, const ContentSpec&); \
        template void ContentLoader<RES_TYPE>::PurgeLoadData(Content&, const ContentSpec&); \
        template bool ContentLoader<RES_TYPE>::StartResourceModuleLoading(Content&, const ContentSpec&); \
        template void ContentLoader<RES_TYPE>::RestartResourceModuleLoading(Content&, const ContentSpec&); \
        template void ContentLoader<RES_TYPE>::UpdateResourceModuleLoading(Content&, const ContentSpec&); \
        template void ContentLoader<RES_TYPE>::CancelResourceModuleLoading(Content&, const ContentSpec&)

    CGS_INSTANTIATE_CONTENT_LOADER(CgsResource::BinaryFileResource);
    CGS_INSTANTIATE_CONTENT_LOADER(CgsResource::AlignedBinaryFileResource);
    #undef CGS_INSTANTIATE_CONTENT_LOADER
}
}
