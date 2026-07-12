#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"       // CgsResource::Pool / Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h" // CgsResource::BundleLoader (the PC sync loader)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // the request/response event records

#include <cstdio>    // std::snprintf (log lines)

// CgsGui::GuiResourceModule -- the PC platform request servicer.
//
// On the console the module's OutputBuffer request queue feeds the resource system through
// the module scheduler: the BundleLoaderModule streams the bundle files and the PoolModule
// resolves acquires, each posting its response into the requesting module's receiver queue
// (the mpUser field every request carries). None of that scheduling exists on the PC boot
// path yet, so the module services its own queue SYNCHRONOUSLY at the Update tail -- the
// protocol (requests out / receiver-queue responses in / WAIT-stage counting) is untouched;
// only the transport between the two queue ends is PC-synchronous.
//
// Bank materialisation policy (this wave): only the FSM bundle bank (miFSMBundleBank --
// the bank the flow-controller's type-18 requests route to) is backed by a real PC pool.
// Requests routed to any other bank complete as echo-only responses (logged) so the state
// machine's WAIT counting stays exact -- the START stage's fire-and-forget PERSISTENTAPT /
// GUITEXTURES.BIN loads fall in this bucket while the apt movie slots still load host-side
// (BrnGuiAptRuntime); materialising the apt banks here is the follow-on migration step.
namespace CgsGui
{
    namespace
    {
        // The FSM bank pool: a single accumulate-bank (the console bank keeps every loaded
        // FSM bundle resident; unloads release by id). Sized above the bring-up host's
        // 256K-per-flow re-init pools because ONE bank now holds every flow's live LuaCode
        // (biggest PC FSM bundle is ~70K; 64 entries cap matches the module's queue cap).
        const u32 KU_FSM_BANK_BYTES     = 512u * 1024u;
        const u32 KU_FSM_BANK_MAX_NODES = 64u;

        u8 s_aFsmBankBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_FSM_BANK_BYTES];
        CgsResource::Pool s_FsmBankPool;
        bool s_bFsmBankLive = false;

        // Stand the FSM bank pool up on first use (the console's bank is created by the
        // resource system from the bank id GuiResourceModule::Prepare received; here the
        // pool init mirrors the host's proven GuiFsm pool options).
        CgsResource::Pool* MaterialiseFsmBankPool(s32 liPoolId)
        {
            if (!s_bFsmBankLive)
            {
                CgsResource::Pool::InitOptions lOptions;
                lOptions.miId    = liPoolId;
                lOptions.mpcName = "GuiResourceFsmBank";
                for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
                {
                    lOptions.maHeapInfo[lt].muMaxNodes       = KU_FSM_BANK_MAX_NODES;
                    lOptions.maHeapInfo[lt].muHeapMemorySize = KU_FSM_BANK_BYTES - KU_FSM_BANK_MAX_NODES * 1024u;
                    lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
                    lOptions.mResource.m_baseResources[lt]   = s_aFsmBankBacking[lt];
                    lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_FSM_BANK_BYTES;
                    lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
                }
                lOptions.muMaxResources         = KU_FSM_BANK_MAX_NODES;
                lOptions.muMaxImports           = KU_FSM_BANK_MAX_NODES;
                lOptions.miRefCountThreshold    = 0;
                lOptions.miNumDependencies      = 0;
                lOptions.miBankId               = 0;
                lOptions.mbAllowDefragmentation = false;
                s_FsmBankPool.InitPool(&lOptions);
                s_bFsmBankLive = true;

                char lac[128];
                std::snprintf(lac, sizeof(lac),
                              "[GuiResourceModule] FSM bank pool materialised (bank id %d).\n", liPoolId);
                CgsDev::Log::WriteToLog(lac);
            }
            return &s_FsmBankPool;
        }
    }

    // FLAG PC-platform leaf: the synchronous stand-in for the console's async resource /
    // pool module chain -- drains this frame's OutputBuffer request queue with CRT file IO
    // (CgsResource::BundleLoader) and posts each response into the request's receiver queue.
    void GuiResourceModule::ServicePlatformRequests(OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != 0, "Invalid output buffer in GuiResourceModule::ServicePlatformRequests");

        OutputBuffer::GuiResourceRequestQueue* lpRequests = lpOutput->GetResourceRequestQueue();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = lpRequests->GetFirstEvent(&lpEvent, &liSize);
        bool lbAnyServiced = false;
        while (liType >= 0 && lpEvent != 0)
        {
            lbAnyServiced = true;
            switch (liType)
            {
                case 2:   // LoadBundleRequest -> load the file into the bank, echo tag 2
                {
                    const CgsResource::Events::LoadBundleRequest* lpRequest =
                        reinterpret_cast<const CgsResource::Events::LoadBundleRequest*>(lpEvent);

                    CgsResource::Events::LoadBundleResponse lResponse;
                    static_cast<CgsResource::Events::BundleLoaderEvent&>(lResponse) =
                        static_cast<const CgsResource::Events::BundleLoaderEvent&>(*lpRequest);
                    lResponse.meResult = CgsResource::Events::LoadBundleResponse::E_RESULT_SUCCESS;

                    if (lpRequest->miPoolId == miFSMBundleBank)
                    {
                        CgsResource::Pool* lpPool = MaterialiseFsmBankPool(lpRequest->miPoolId);
                        CgsResource::BundleLoader lLoader;
                        const s32 liLoaded =
                            lLoader.LoadBundle(lpRequest->macFileName, lpPool, CgsResource::ResolveResourceType);

                        char lac[192];
                        std::snprintf(lac, sizeof(lac),
                                      "[GuiResourceModule] bundle '%s' -> %s (%d resources, bank %d).\n",
                                      lpRequest->macFileName, liLoaded > 0 ? "loaded" : "MISSING",
                                      liLoaded, lpRequest->miPoolId);
                        CgsDev::Log::WriteToLog(lac);
                    }
                    else
                    {
                        // The bank is not materialised on PC yet (apt/texture/font/language
                        // waves pending) -- complete the request without IO so the WAIT
                        // counting stays exact; the acquire path reports the miss.
                        char lac[192];
                        std::snprintf(lac, sizeof(lac),
                                      "[GuiResourceModule] bank %d not materialised on PC -- '%s' completed without IO.\n",
                                      lpRequest->miPoolId, lpRequest->macFileName);
                        CgsDev::Log::WriteToLog(lac);
                    }

                    if (lpRequest->mpUser != 0)
                        lpRequest->mpUser->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lResponse), 2,
                            static_cast<s32>(sizeof(lResponse)));
                    break;
                }

                case 3:   // UnloadBundleRequest -> release the bundle's entries, echo tag 3
                {
                    const CgsResource::Events::UnloadBundleRequest* lpRequest =
                        reinterpret_cast<const CgsResource::Events::UnloadBundleRequest*>(lpEvent);

                    if (lpRequest->miPoolId == miFSMBundleBank && s_bFsmBankLive)
                    {
                        CgsResource::BundleLoader lLoader;
                        const s32 liUnloaded = lLoader.UnloadBundle(lpRequest->macFileName, &s_FsmBankPool);

                        char lac[192];
                        std::snprintf(lac, sizeof(lac),
                                      "[GuiResourceModule] bundle '%s' unload -> %d resources released.\n",
                                      lpRequest->macFileName, liUnloaded);
                        CgsDev::Log::WriteToLog(lac);
                    }

                    // The echo is what ParseUnloaded matches (it re-hashes macFileName).
                    if (lpRequest->mpUser != 0)
                        lpRequest->mpUser->AddEvent(lpEvent, 3, static_cast<s32>(sizeof(*lpRequest)));
                    break;
                }

                case 4:   // AcquireResourceRequest -> resolve from the bank, echo + handle, tag 4
                {
                    const CgsResource::Events::AcquireResourceRequest* lpRequest =
                        reinterpret_cast<const CgsResource::Events::AcquireResourceRequest*>(lpEvent);

                    GuiResourceAcquireResponse lResponse;
                    static_cast<CgsResource::Events::AcquireResourceRequest&>(lResponse) = *lpRequest;
                    lResponse.mResourceHandle.mpResourceMemory = 0;
                    lResponse.mResourceHandle.mpSourceEntry    = 0;

                    if (lpRequest->miPoolId == miFSMBundleBank && s_bFsmBankLive)
                    {
                        // Mirror the console responder (PoolModule::DoAcquireResourceRequest
                        // @0x828FCD48): FindResource with status mask 2 (loaded), handle =
                        // { &entry main-memory slot, entry }.
                        s32 liIndex = -1;
                        CgsResource::Entry* lpEntry = s_FsmBankPool.FindResource(
                            lpRequest->mResourceId, lpRequest->mbCheckRefCount, 2, &liIndex);
                        if (lpEntry != 0)
                        {
                            lResponse.mResourceHandle.mpResourceMemory =
                                &lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
                            lResponse.mResourceHandle.mpSourceEntry = lpEntry;
                        }
                    }

                    if (lpRequest->mpUser != 0)
                        lpRequest->mpUser->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lResponse), 4,
                            static_cast<s32>(sizeof(lResponse)));
                    break;
                }

                default:
                {
                    char lac[96];
                    std::snprintf(lac, sizeof(lac),
                                  "[GuiResourceModule] unhandled resource request type %d dropped.\n", liType);
                    CgsDev::Log::WriteToLog(lac);
                    break;
                }
            }

            const CgsModule::Event* lpNextEvent = 0;
            liType = lpRequests->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent = lpNextEvent;
        }

        // This frame's records are fully serviced -- drop them (the console equivalent is
        // the transient IO buffer being consumed downstream; the WAIT stage's
        // HasPendingResourceRequests clears the queue anyway before counting).
        if (lbAnyServiced)
            lpRequests->Clear();
    }
}
