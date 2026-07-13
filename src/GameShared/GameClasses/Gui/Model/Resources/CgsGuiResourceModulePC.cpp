#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"       // CgsResource::Pool / Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h" // CgsResource::BundleLoader (the PC sync loader)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h" // RegisterAllResourceTypes (AptDataHeaderType 0x1E)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"        // CgsResource::ID::HashString (apt acquire id)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // the request/response event records
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // GuiEventLoadNotification (the apt registration record)

#include <cstdio>    // std::snprintf (log lines)
#include <cstring>   // std::strncpy / _stricmp (the bundle-lead registry)

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

        // The streamed-apt bank pool (request types 4/5/6 -- the GuiApt movie bundles). The
        // console bank keeps every loaded streamed movie resident; here one accumulate pool
        // holds them, and its AptData resources are registered by the view module through the
        // load notifications this servicer emits on each apt bundle load (the notification
        // carries the resolved header for ViewModule::ProcessIncomingLoadNotification ->
        // AptDataHandler::AddAptData). 24 MiB/type covers the title (~1.6 MB) plus a
        // concurrent state overlay with graphics-memory headroom.
        const u32 KU_APT_STREAMED_BANK_BYTES     = 24u * 1024u * 1024u;
        const u32 KU_APT_STREAMED_BANK_MAX_NODES = 256u;
        const u32 KU_APTDATA_RESOURCE_TYPE_ID    = 30u;   // X360 0x1E (CgsResource::AptDataHeaderType)

        u8 s_aAptStreamedBankBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_APT_STREAMED_BANK_BYTES];
        CgsResource::Pool s_AptStreamedBankPool;
        bool s_bAptStreamedBankLive = false;

        // Stand the streamed-apt bank pool up on first use (mirrors the FSM bank; the console's
        // bank is created by the resource system from the Prepare bank id). RegisterAllResourceTypes
        // (idempotent) ensures the AptDataHeaderType (0x1E) FixUp handler is live so LoadBundle
        // relocates the movie header.
        CgsResource::Pool* MaterialiseAptStreamedBankPool(s32 liPoolId)
        {
            if (!s_bAptStreamedBankLive)
            {
                CgsResource::RegisterAllResourceTypes();
                CgsResource::Pool::InitOptions lOptions;
                lOptions.miId    = liPoolId;
                lOptions.mpcName = "GuiResourceAptStreamedBank";
                for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
                {
                    lOptions.maHeapInfo[lt].muMaxNodes       = KU_APT_STREAMED_BANK_MAX_NODES;
                    lOptions.maHeapInfo[lt].muHeapMemorySize = KU_APT_STREAMED_BANK_BYTES - 256u * 1024u;
                    lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
                    lOptions.mResource.m_baseResources[lt]   = s_aAptStreamedBankBacking[lt];
                    lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_APT_STREAMED_BANK_BYTES;
                    lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
                }
                lOptions.muMaxResources         = KU_APT_STREAMED_BANK_MAX_NODES;
                lOptions.muMaxImports           = KU_APT_STREAMED_BANK_MAX_NODES;
                lOptions.miRefCountThreshold    = 0;
                lOptions.miNumDependencies      = 0;
                lOptions.miBankId               = 0;
                lOptions.mbAllowDefragmentation = false;
                s_AptStreamedBankPool.InitPool(&lOptions);
                s_bAptStreamedBankLive = true;
                CgsDev::Log::WriteToLog("[GuiResourceModule] streamed-apt bank pool materialised.\n");
            }
            return &s_AptStreamedBankPool;
        }

        // Emit one type-14 load notification per AptData (0x1E) resource in the pool, each
        // carrying a handle to the entry's main-memory slot + the apt request type. GuiModule's
        // frame bridge routes these (request types 4..7) into the view input buffer, where the
        // REAL CgsGui::ViewModule::ProcessIncomingLoadNotification @0x8285BD30 registers each
        // header (AddAptData) -- the console notification flow, with only the bundle IO now
        // module-side. Re-emitting a resident header is a no-op (AddAptData dedups by name), so
        // walking the whole accumulate pool each load is safe.
        // FLAG PC-platform leaf: a GuiApt movie bundle carries one AptData per embedded movie
        // (PERSISTENTAPT's import library is 61); registering them all from the bundle load is
        // the PC stand-in for the console's per-movie async .apt registration (the async
        // AptLoader stream is out of this slice).
        u32 EmitAptDataNotifications(CgsResource::Pool* lpPool, s32 liAptType,
                                     GuiResourceModuleIO::OutputBuffer* lpOutput)
        {
            u32 luEmitted = 0;
            const u32 luMax = lpPool->GetMaxResources();
            for (u32 lu = 0; lu < luMax; ++lu)
            {
                if (lpPool->GetEntryStatusDirect(static_cast<s32>(lu)) == 0)
                    continue;
                CgsResource::Entry* lpEntry = const_cast<CgsResource::Entry*>(
                    lpPool->GetEntryDirect(static_cast<s32>(lu)));
                if (lpEntry->mpResourceType == 0 ||
                    lpEntry->mpResourceType->GetTypeID() != KU_APTDATA_RESOURCE_TYPE_ID)
                    continue;
                if (lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY] == 0)
                    continue;

                GuiEventLoadNotification lNote;
                lNote.mResourceHandle.mpResourceMemory =
                    &lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
                lNote.mResourceHandle.mpSourceEntry = lpEntry;
                lNote.meRequestType   = static_cast<ResourceRequestTypes>(liAptType);
                lNote.muLoadRequestId = 0;
                lpOutput->AddLoadNotification(reinterpret_cast<const CgsModule::Event*>(&lNote));
                ++luEmitted;
            }
            return luEmitted;
        }

        // ---- the loaded-bundle LEAD registry (bundle name -> its primary AptDataHeader) -------
        // The framework/persist slots are keyed by the HOST bundle name (MAIN / PERSISTENTAPT),
        // but each AptData registers under its AUTHORED movie name (main / CrashNavTitleBar). The
        // host's DriveFaithfulLoad resolves the slot's own movie by reading the authored name off
        // the bundle's LEAD header; on the console that lead came from the slot's own synchronous
        // pool load. With the load now module-side, this registry hands the host the lead so the
        // authored-name resolution still works. The lead is the bundle's FIRST contributed AptData
        // (file order == pool-slot order in this accumulate bank), matching the old
        // FindFirstResourceOfType(0x1E) the host used.
        const u32 KU_MAX_APT_BUNDLE_LEADS = 32u;
        struct AptBundleLead
        {
            char                macName[64];  // the movie/bundle name (host lookup key)
            u32                 muSwfHash;     // HashString("<name>.swf") -- the acquire request id
            CgsResource::Entry* mpLeadEntry;  // the bundle's lead AptData resource entry
        };
        AptBundleLead s_aAptBundleLeads[KU_MAX_APT_BUNDLE_LEADS];
        u32   s_uAptBundleLeadCount     = 0;
        void* s_apAttributedAptData[KU_APT_STREAMED_BANK_MAX_NODES]; // headers already tied to a bundle
        u32   s_uAttributedAptDataCount = 0;

        bool IsAptDataAttributed(void* lpHeader)
        {
            for (u32 lu = 0; lu < s_uAttributedAptDataCount; ++lu)
                if (s_apAttributedAptData[lu] == lpHeader)
                    return true;
            return false;
        }

        // Record bundle lpacBundleName's lead (its first not-yet-attributed AptData in slot order)
        // and attribute all its AptData so the NEXT bundle's lead scan skips them. The lead is keyed
        // by both the bundle name (host lead-header lookup) and the acquire request id
        // HashString("<name>.swf") (so the acquire resolves to THIS bundle, missing until it loads).
        void RecordAptBundleLead(CgsResource::Pool* lpPool, const char* lpacBundleName)
        {
            CgsResource::Entry* lpLeadEntry = 0;
            const u32 luMax = lpPool->GetMaxResources();
            for (u32 lu = 0; lu < luMax; ++lu)
            {
                if (lpPool->GetEntryStatusDirect(static_cast<s32>(lu)) == 0)
                    continue;
                CgsResource::Entry* lpEntry = const_cast<CgsResource::Entry*>(
                    lpPool->GetEntryDirect(static_cast<s32>(lu)));
                if (lpEntry->mpResourceType == 0 ||
                    lpEntry->mpResourceType->GetTypeID() != KU_APTDATA_RESOURCE_TYPE_ID)
                    continue;
                void* lpMem = lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
                if (lpMem == 0 || IsAptDataAttributed(lpMem))
                    continue;
                if (lpLeadEntry == 0)
                    lpLeadEntry = lpEntry;
                if (s_uAttributedAptDataCount < KU_APT_STREAMED_BANK_MAX_NODES)
                    s_apAttributedAptData[s_uAttributedAptDataCount++] = lpMem;
            }
            if (lpLeadEntry != 0 && s_uAptBundleLeadCount < KU_MAX_APT_BUNDLE_LEADS)
            {
                AptBundleLead& lrLead = s_aAptBundleLeads[s_uAptBundleLeadCount++];
                std::strncpy(lrLead.macName, lpacBundleName, sizeof(lrLead.macName) - 1);
                lrLead.macName[sizeof(lrLead.macName) - 1] = '\0';
                char lacSwf[80];
                std::snprintf(lacSwf, sizeof(lacSwf), "%s.swf", lpacBundleName);
                lrLead.muSwfHash   = static_cast<u32>(
                    CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacSwf)));
                lrLead.mpLeadEntry = lpLeadEntry;
            }
        }

        // Parse the movie name out of "GuiApt\<NAME>.bundle" (the LoadBundleRequest path) into lpacOut.
        void ParseAptBundleName(const char* lpacFileName, char* lpacOut, u32 luOutSize)
        {
            const char* lpStart = lpacFileName;
            for (const char* lp = lpacFileName; *lp != '\0'; ++lp)
                if (*lp == '\\' || *lp == '/')
                    lpStart = lp + 1;
            u32 lu = 0;
            for (; lpStart[lu] != '\0' && lpStart[lu] != '.' && lu + 1u < luOutSize; ++lu)
                lpacOut[lu] = lpStart[lu];
            lpacOut[lu] = '\0';
        }
    }

    // Hand the host the lead AptDataHeader of a module-loaded apt bundle (by bundle name), so the
    // AptRuntimeHost's DriveFaithfulLoad can resolve the slot's authored movie name. Returns null
    // until the bundle's load has been serviced. Forward-declared for the host in BrnGuiAptRuntime.cpp.
    void* GetLoadedAptBundleLeadHeader(const char* lpacBundleName)
    {
        if (lpacBundleName == 0)
            return 0;
        for (u32 lu = 0; lu < s_uAptBundleLeadCount; ++lu)
            if (_stricmp(s_aAptBundleLeads[lu].macName, lpacBundleName) == 0)
                return s_aAptBundleLeads[lu].mpLeadEntry
                       ->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
        return 0;
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
                    else if (lpRequest->miPoolId == miAptStreamedBundleBank)
                    {
                        // The streamed GuiApt movie bundle (types 4/5/6): load the file into the
                        // apt bank, then register every AptData it carries with the view via load
                        // notifications (the apt request type rode along on the LoadBundleRequest's
                        // miEventId; GuiModule's bridge routes types 4..7 into the view input
                        // buffer -> ProcessIncomingLoadNotification -> AddAptData).
                        CgsResource::Pool* lpPool = MaterialiseAptStreamedBankPool(lpRequest->miPoolId);
                        CgsResource::BundleLoader lLoader;
                        const s32 liLoaded =
                            lLoader.LoadBundle(lpRequest->macFileName, lpPool, CgsResource::ResolveResourceType);
                        if (liLoaded > 0)
                        {
                            // Attribute the bundle's AptData so the host can resolve its lead movie.
                            char lacMovieName[64];
                            ParseAptBundleName(lpRequest->macFileName, lacMovieName, sizeof(lacMovieName));
                            RecordAptBundleLead(lpPool, lacMovieName);
                        }
                        const u32 luEmitted =
                            EmitAptDataNotifications(lpPool, lpRequest->miEventId, lpOutput);

                        char lac[224];
                        std::snprintf(lac, sizeof(lac),
                                      "[GuiResourceModule] apt bundle '%s' -> %s (%d resources, %u AptData registration(s), bank %d).\n",
                                      lpRequest->macFileName, liLoaded > 0 ? "loaded" : "MISSING",
                                      liLoaded, luEmitted, lpRequest->miPoolId);
                        CgsDev::Log::WriteToLog(lac);
                    }
                    else
                    {
                        // The bank is not materialised on PC yet (texture/font/language +
                        // persistent-apt waves pending) -- complete the request without IO so the
                        // WAIT counting stays exact; the acquire path reports the miss.
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
                    else if (lpRequest->miPoolId == miAptStreamedBundleBank && s_bAptStreamedBankLive)
                    {
                        // Resolve the acquire against the REQUESTED bundle's lead, keyed by the
                        // request id (HashString("<name>.swf")) in the lead registry. This MISSES
                        // until the requested bundle has loaded -- which is what drives the module
                        // to LoadBundle it (a plain first-resident-AptData match would falsely
                        // resolve against an already-resident movie and skip the load). The real
                        // per-movie registration is the bundle-load notification (EmitAptData-
                        // Notifications); this handle only parses the module's pending record.
                        const u32 luReqHash = static_cast<u32>(lpRequest->mResourceId.GetHash());
                        for (u32 lu = 0; lu < s_uAptBundleLeadCount; ++lu)
                        {
                            if (s_aAptBundleLeads[lu].muSwfHash != luReqHash)
                                continue;
                            CgsResource::Entry* lpEntry = s_aAptBundleLeads[lu].mpLeadEntry;
                            lResponse.mResourceHandle.mpResourceMemory =
                                &lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
                            lResponse.mResourceHandle.mpSourceEntry = lpEntry;
                            break;
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
