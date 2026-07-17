// FLAG PC-platform leaf: the PC body of the console's async .apt STREAM.
//
// AptLoaderStartAsyncLoad (gAptFuncs slot dword_8324E838) is the platform "kick
// off the .apt stream" hook the faithful AptLoader::Update calls on the state
// 1->2 (requested -> loading) transition. On the console it starts an ASYNC
// stream whose completion posts AptCompleteAnimationAsyncLoad; PC has no such
// stream, so the hook loads the movie's data SYNCHRONOUSLY and drives that same
// completion inline (the AptFile ends resolved). This is the ONE genuinely
// platform-specific piece of the movie/import content-load: the loader STATE
// MACHINE (AptLoader::Update) + the completion (CompleteLoad -> Resolve ->
// Fixup) + the link (AptCharacterAnimation_Link -> FindExport) are all the
// faithful console flow. Re-homed out of the BrnGuiAptRuntime bring-up host
// (retirement slice: loader-leaf re-home); behaviour unchanged.
//
// REGISTERED-DATA FIRST (the console shape): a framework bundle load registers
// EVERY AptData it carries through the load-notification chain (PERSISTENTAPT
// alone carries the 61-movie import library), so a movie normally resolves
// straight from the data handler -- FindAptData(name) hits and the faithful
// AptCallbackFile::LoadAnimation completes the handle with NO per-import IO.
// FALLBACK ([PC IO]): a movie NOT carried by any loaded bundle synchronously
// loads its own GuiApt\<NAME>.bundle into a resident pool and registers its
// header directly, then completes through the same faithful LoadAnimation.

#include <cstdio>    // std::snprintf (log lines)
#include <cstring>   // std::strncpy / _stricmp (the import registry)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"         // AptAuxPointer / AptDataHandler / AptCallbackFile
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"  // CgsGui::AptDataHeader
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"         // CgsResource::Pool / Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h" // CgsResource::BundleLoader
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsSmallResourcePS3.h"     // E_MEMTYPE_* / E_MEMTYPE_NUMTYPES
#include "SDKs/EATech/include/Apt/AptLoader.h"                              // AptFilePtr / AptLoaderStartAsyncLoad contract

namespace
{
    // The Apt-data resource type id (X360 0x1E == 30; CgsResource::AptDataHeaderType::GetTypeID).
    const u32 KU_APTDATA_RESOURCE_TYPE_ID = 30u;

    // The loaded bundles must stay RESIDENT (the engine references their chars every
    // frame), so each gets a persistent pool + AptData span, kept in this small
    // name-keyed registry (dedup: a movie referenced by >1 loader is loaded once).
    // Static BSS storage (no runtime heap growth).
    const u32 KU_MAX_IMPORT_BUNDLES  = 16u;                // PERSISTENTAPT's master movie has 13 imports
    const u32 KU_IMPORT_POOL_BYTES   = 2u * 1024u * 1024u; // per-import pool (largest import ~282KB)

    struct ImportBundleSlot
    {
        char              macName[64];  // the movie name (e.g. "B5HelperComponents"), lower-cmp key
        CgsResource::Pool Pool;         // the resident pool holding the import bundle
        uintptr_t         luBase;       // the AptData resource base (the native-8 load base)
        u32               luSize;       // the AptData resource size (relocation bound)
        void*             lpHeader;     // the AptDataHeader (== the resource base)
        bool              lbUsed;
    };

    ImportBundleSlot s_aImportBundles[KU_MAX_IMPORT_BUNDLES];
    // Per-import pool backing (one E_MEMTYPE_NUMTYPES x KU_IMPORT_POOL_BYTES block per slot).
    u8 s_aImportPoolBacking[KU_MAX_IMPORT_BUNDLES][CgsResource::E_MEMTYPE_NUMTYPES][KU_IMPORT_POOL_BYTES];
    u32  s_uImportBundleCount  = 0;
    bool s_bImportReentryGuard = false;  // guard AptLoader::Update re-entrancy while a load is in flight

    // Content-load ONE movie by name and drive its AptFile through the faithful
    // completion. Idempotent by name (the registry dedups). Returns true on success.
    bool LoadImportBundle(const char* lpacMovieName, AptFilePtr* lpFile)
    {
        char lac[256];
        if (lpacMovieName == nullptr || lpacMovieName[0] == '\0')
            return false;

        CgsGui::AptAux* lpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;
        if (lpAptAux == nullptr)
            return false;

        // The registered-data path: the movie's AptData is already with the handler
        // (registered when its carrying bundle's notifications dispatched).
        if (lpAptAux->mAptDataHandler.FindAptData(lpacMovieName) != nullptr)
        {
            std::snprintf(lac, sizeof(lac),
                "[AptRT] import-load: '%s' resolves from the data handler (no bundle IO).\n",
                lpacMovieName);
            CgsDev::Log::WriteToLog(lac);
            CgsGui::AptCallbackFile::LoadAnimation(lpacMovieName, lpFile);
            return true;
        }

        // ---- FALLBACK ([PC IO]): per-movie bundle load ------------------------------
        // Dedup: already resident?
        ImportBundleSlot* lpSlot = nullptr;
        for (u32 lu = 0; lu < s_uImportBundleCount; ++lu)
        {
            if (s_aImportBundles[lu].lbUsed &&
                _stricmp(s_aImportBundles[lu].macName, lpacMovieName) == 0)
            {
                lpSlot = &s_aImportBundles[lu];
                break;
            }
        }

        if (lpSlot == nullptr)
        {
            // Fresh movie: allocate a slot + load the bundle.
            if (s_uImportBundleCount >= KU_MAX_IMPORT_BUNDLES)
            {
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] import-load: registry full (%u) -- cannot load '%s' (FLAG).\n",
                    KU_MAX_IMPORT_BUNDLES, lpacMovieName);
                CgsDev::Log::WriteToLog(lac);
                return false;
            }
            lpSlot = &s_aImportBundles[s_uImportBundleCount];
            std::strncpy(lpSlot->macName, lpacMovieName, sizeof(lpSlot->macName) - 1);
            lpSlot->macName[sizeof(lpSlot->macName) - 1] = '\0';
            lpSlot->lbUsed = true;

            // Build "GuiApt\<NAME>.bundle" (Windows resolves case; the files are UPPERCASE).
            char lacPath[160];
            std::snprintf(lacPath, sizeof(lacPath), "GuiApt\\%s.bundle", lpacMovieName);

            // Init the resident pool (same options as the parent movie pool).
            CgsResource::Pool::InitOptions lOptions;
            lOptions.miId   = 4;
            lOptions.mpcName = "AptImport";
            u8 (*lpBacking)[KU_IMPORT_POOL_BYTES] = s_aImportPoolBacking[s_uImportBundleCount];
            for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
            {
                lOptions.maHeapInfo[lt].muMaxNodes       = 256u;
                lOptions.maHeapInfo[lt].muHeapMemorySize = KU_IMPORT_POOL_BYTES - 64u * 1024u;
                lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
                lOptions.mResource.m_baseResources[lt]   = lpBacking[lt];
                lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_IMPORT_POOL_BYTES;
                lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
            }
            lOptions.muMaxResources         = 64u;
            lOptions.muMaxImports           = 64u;
            lOptions.miRefCountThreshold    = 0;
            lOptions.miNumDependencies      = 0;
            lOptions.miBankId               = 0;
            lOptions.mbAllowDefragmentation = false;
            lpSlot->Pool.InitPool(&lOptions);

            CgsResource::BundleLoader lLoader;
            const s32 liLoaded = lLoader.LoadBundle(lacPath, &lpSlot->Pool,
                                                    CgsResource::ResolveResourceType);
            std::snprintf(lac, sizeof(lac),
                "[AptRT] import-load: '%s' -> %d resources.\n", lacPath, liLoaded);
            CgsDev::Log::WriteToLog(lac);
            if (liLoaded <= 0)
            {
                CgsDev::Log::WriteToLog("[AptRT] import-load: bundle missing/unreadable -- import left "
                                        "'requested' (FLAG: honest data boundary).\n");
                lpSlot->lbUsed = false;
                return false;
            }

            s32 liIndex = -1;
            CgsResource::Entry* lpEntry =
                lpSlot->Pool.FindFirstResourceOfType(KU_APTDATA_RESOURCE_TYPE_ID, &liIndex);
            void* lpRes = (lpEntry != nullptr)
                ? lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY] : nullptr;
            if (lpRes == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] import-load: no AptData resource -- left 'requested' (FLAG).\n");
                lpSlot->lbUsed = false;
                return false;
            }
            lpSlot->lpHeader = lpRes;
            lpSlot->luBase   = reinterpret_cast<uintptr_t>(lpRes);
            lpSlot->luSize   =
                lpEntry->mResourceDescriptor.m_baseResourceDescriptors[CgsResource::E_MEMTYPE_MAINMEMORY].m_size;

            // Register the header with the data handler so the faithful FindAptData(name)
            // resolves it (the resolved-name form -- FLAG x64; see the fallback note above).
            lpAptAux->mAptDataHandler.AddAptData(
                reinterpret_cast<CgsGui::AptDataHeader*>(lpRes), lpSlot->macName);

            ++s_uImportBundleCount;
            std::snprintf(lac, sizeof(lac),
                "[AptRT] import-load: '%s' resident base=0x%016llX size=%u (slot %u).\n",
                lpSlot->macName, (unsigned long long)lpSlot->luBase, lpSlot->luSize,
                s_uImportBundleCount - 1);
            CgsDev::Log::WriteToLog(lac);
        }

        // Complete through the same faithful callback as the registered path (it resolves the
        // header's chunks + publishes the resolve64 bounds itself).
        std::snprintf(lac, sizeof(lac),
            "[AptRT] import-load: complete '%s' from the freshly-registered fallback bundle.\n",
            lpSlot->macName);
        CgsDev::Log::WriteToLog(lac);
        CgsGui::AptCallbackFile::LoadAnimation(lpacMovieName, lpFile);
        return true;
    }
}

// The strong hook definition the engine's AptLoader::Update binds to.
// Re-entrancy note: LoadImportBundle drives AptCompleteAnimationAsyncLoad which does
// NOT re-enter Update, but a nested import's own Fixup pass-3 can register further
// imports; those are picked up by the driving Update's outer re-pass, so a single
// synchronous load here is safe. The guard just prevents an accidental recursive
// StartAsyncLoad on the same in-flight file.
void AptLoaderStartAsyncLoad(const char* pFileName, AptFilePtr* pFile)
{
    if (s_bImportReentryGuard)
        return;
    s_bImportReentryGuard = true;
    LoadImportBundle(pFileName, pFile);
    s_bImportReentryGuard = false;
}
