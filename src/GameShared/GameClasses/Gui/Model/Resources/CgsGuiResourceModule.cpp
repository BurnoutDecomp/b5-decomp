#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Containers/CgsHash.h"    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"       // CgsResource::ID::HashString
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // LoadBundleRequest / UnloadBundleRequest / AcquireResourceRequest

#include <cstdlib>   // qsort
#include <cstring>   // strlen

// CgsGui::GuiResourceModule -- reconstructed from BURNOUT_X360_ARTIST.XEX (source
// CgsGuiResourceModule.cpp; the DecFIGS DWARF supplies every member/enum/local name and
// the PS3 build's line numbers, which the X360 baked asserts match one-for-one).
//
// THIS TU's functions (X360 addresses):
//   Construct                     0x8284FC08     Prepare                    0x8284FC88
//   Release                       0x8284FD90     Destruct                   0x8284FE50
//   Update                        0x8285F7A8     ProcessIncomingLoadRequests 0x828581A8
//   AddToBundleLoadQueue          0x82848090     SortBundleQueue            0x82848068
//   IsBundleQueuedToLoad          0x82848450     PopFromBundleLoadQueue     0x82848468
//   LoadBundle                    0x8285E7C0     UnloadBundle               0x8285E8C8
//   RequestResource               0x8285E9A8     HasPendingResourceRequests 0x8284EF48
//   LoadBundleForMissingResources 0x8285F708     ParseForAcquiredResources  0x8284FE88
//   ParseUnloaded                 0x8284FFE8     operator++(EReleaseStage&) 0x82847190
//
// Protocol (the module dispatch the whole GUI bundle IO rides):
//   requests in : GuiEventLoadRequest(39) records drained from InputBuffer::mLoadRequests
//                 -> AddToBundleLoadQueue -> maBundlesToLoad (sorted by type).
//   the machine : START loads PERSISTENTAPT + GUITEXTURES.BIN fire-and-forget, then per
//                 batch of <=5 pending records: acquire-by-id first (type-4 request); the
//                 misses get their container bundle file loaded (type-2 request) and are
//                 re-acquired; unloads ride type-3 requests matched back by path hash.
//                 Every outgoing request carries &mReceiverQueue as the reply target; the
//                 WAIT stage completes when the receiver count equals the issued count.
//   notify out  : SEND_NOTIFICATIONS posts GuiEventLoadNotification (queue type 14, with
//                 the resolved ResourceHandle) / GuiEventUnloadNotification (16) onto the
//                 OutputBuffer for the FSM controller / view chain to consume.
//
// The X360-baked assert file/line pairs are discarded per project convention (CGS_ASSERT
// supplies __FILE__/__LINE__); the streamed-StrStream assert forms are collapsed to their
// attested message literals, as the sibling CgsPoolModule.cpp TU does.
namespace CgsGui
{
    // The per-request-type path/name format-template table. DWARF CgsGuiResourceModule.cpp:29
    // names it (`const char* KAAC_RESOURCE_TEMPLATES[]`); the strings below are the X360 rdata
    // table @0x82F330C8 (24 slots; GuiAptPath/GuiAptSDPath are the binary's names for slots
    // 2/3), dumped from the ARTIST database. Dual use, exactly as the X360 reads it:
    //   - RequestResource formats [meResourceType] with the bundle name -> the resource-id
    //     string that is hashed (CgsResource::ID::HashString) for the acquire request.
    //   - AddToBundleLoadQueue / Update's START stage store the CONTAINER type's slot as the
    //     record's mpcPathToBundle -> LoadBundle/UnloadBundle format it into the file path.
    // ARTIST numbering note: the ARTIST enum drifted +1 from the PS3 DecFIGS enum from the
    // apt family onward (an extra apt slot at 6) -- the same drift the committed
    // BrnGuiViewModule.cpp / CgsGuiViewModule.cpp consumers pin (types 4..7 apt, 12 text,
    // 16 font). Values are therefore commented with their ARTIST roles, not the PS3 names.
    static const char* const KAAC_RESOURCE_TEMPLATES[24] =
    {
        "",                                  // [ 0] START
        "%s.bundle",                         // [ 1] plain bundle (container for type 11)
        "GuiApt\\%s.bundle",                 // [ 2] HD apt bundle   (X360 global GuiAptPath)
        "GuiAptSD\\%s.bundle",               // [ 3] SD apt bundle   (X360 global GuiAptSDPath)
        "%s.swf",                            // [ 4] apt movie
        "%s.swf",                            // [ 5] apt loading-screen movie
        "%s.swf",                            // [ 6] apt movie (ARTIST-only slot; the +1 drift)
        "%s.swf",                            // [ 7] apt persistent movie
        "%s.BUNDLE",                         // [ 8] HD flapt bundle
        "%sSD.BUNDLE",                       // [ 9] SD flapt bundle
        "%s",                                // [10] flapt persistent movie
        "%s",                                // [11] texture (GUITEXTURES.BIN loads verbatim)
        "%s.lang",                           // [12] localised text
        "Language\\%s.bundle",               // [13] localised-text bundle (container for 12)
        "Language\\Fonts\\%s.font",          // [14] HD font bundle
        "Language\\FontsSD\\%s.font",        // [15] SD font bundle
        "%s",                                // [16] font data
        // [17] fsm bundle (container for type 18). X360 rdata: "Fsm\\%s.dat".
        // FLAG PC-platform leaf: the PC repack ships the fsm flow bundles as
        // FSM\<NAME>.BUNDLE (build\game\FSM\*.BUNDLE -- the path the bring-up host
        // already boots with), so the PC table carries the repack's spelling.
        "FSM\\%s.BUNDLE",
        "%s",                                // [18] fsm (the LuaCode resource; id = hash of the bare name)
        "PostFx\\%s.pfx",                    // [19] pfx bundle (container for type 20)
        "PostFx\\%s",                        // [20] pfx
        "PostFx/colourcubedictionary.bin",   // [21] pfx colour-cube dictionary (container for 22)
        "%s",                                // [22] pfx colour cube
        "",                                  // [23] DONE
    };

    // X360 0x82847190: post-increment the release stage, dev-asserting it never runs past
    // DONE. The baked message text names the original's parameter (leEnumIndex); the baked
    // d:\p4 header path is discarded per convention.
    GuiResourceModule::EReleaseStage operator++(GuiResourceModule::EReleaseStage& lreStage, int)
    {
        const GuiResourceModule::EReleaseStage lePrevious = lreStage;
        lreStage = static_cast<GuiResourceModule::EReleaseStage>(lreStage + 1);
        CGS_ASSERT(lreStage <= GuiResourceModule::E_RELEASESTAGE_DONE,
                   "leEnumIndex <= GuiResourceModule::E_RELEASESTAGE_DONE");
        return lePrevious;
    }

    // X360 0x8284FC08. Base construct, bind the receiver queue (the X360 inlines
    // EventReceiverQueue<8192,16>::Construct: buffer ptr / capacity 0x2000 / alignment 16 /
    // Clear), then seed the counters and stages. meAcquireStage is NOT seeded here -- Prepare
    // arms it (the X360 leaves +0x230 untouched until Prepare's DONE stage).
    void GuiResourceModule::Construct(bool lHighDef)
    {
        CgsModule::ModuleSingleBuffered::Construct();

        mReceiverQueue.Construct();

        mbHighDef                      = lHighDef;
        mNumWaitingResourceAllocations = 0;
        miNumBundlesToLoad             = 0;
        miNumPendingResources          = 0;
        mePrepareStage                 = E_PREPARESTAGE_START;
        meReleaseStage                 = E_RELEASESTAGE_DONE;
        mbIsNewModule                  = true;
    }

    // X360 0x8284FC88. Staged prepare with fallthrough: each stage stamps itself then runs;
    // only a false base-manager Prepare suspends the walk (re-entry resumes at MANAGER).
    bool GuiResourceModule::Prepare(s32 liAptPersistentBundleBank,
                                    s32 liAptStreamedBundleBank,
                                    s32 liFontBundleBank,
                                    s32 liFSMBundleBank,
                                    s32 liLanguageBundleBank,
                                    s32 liTexturesBundlePoolId,
                                    s32 liGlobalTexturePoolId)
    {
        switch (mePrepareStage)
        {
            case E_PREPARESTAGE_START:
                mePrepareStage = E_PREPARESTAGE_START;
                // fall through
            case E_PREPARESTAGE_MANAGER:
                mePrepareStage = E_PREPARESTAGE_MANAGER;
                if (!CgsModule::ModuleSingleBuffered::Prepare())
                    return false;
                // fall through
            case E_PREPARESTAGE_RECEIVERQUEUE:
                mePrepareStage = E_PREPARESTAGE_RECEIVERQUEUE;
                mReceiverQueue.Clear();
                // fall through
            case E_PREPARESTAGE_BANKID:
                miAptStreamedBundleBank   = liAptStreamedBundleBank;
                miAptPersistentBundleBank = liAptPersistentBundleBank;
                miFontBundleBank          = liFontBundleBank;
                miFSMBundleBank           = liFSMBundleBank;
                miLanguageBundleBank      = liLanguageBundleBank;
                mePrepareStage            = E_PREPARESTAGE_BANKID;
                miTexturesBundlePoolId    = liTexturesBundlePoolId;
                miGlobalTexturePoolId     = liGlobalTexturePoolId;
                // fall through
            case E_PREPARESTAGE_DONE:
                meReleaseStage = E_RELEASESTAGE_START;
                meAcquireStage = E_ACQUIRESTAGE_START;
                mePrepareStage = E_PREPARESTAGE_DONE;
                return true;

            default:
                CGS_ASSERT(false, "Unknown prepare stage");
                return false;
        }
    }

    // X360 0x8284FD90. START clears the receiver queue then advances; MANAGER releases the
    // base; a completed walk (and a Release on an already-DONE module) resets the stage to
    // START and reports success (`li r11,0 / stw r11,0(r30)` -- the X360 tail store).
    bool GuiResourceModule::Release()
    {
        switch (meReleaseStage)
        {
            case E_RELEASESTAGE_START:
                mReceiverQueue.Clear();
                meReleaseStage++;
                // fall through
            case E_RELEASESTAGE_MANAGER:
                if (!CgsModule::ModuleSingleBuffered::Release())
                    return false;
                meReleaseStage++;
                // fall through
            case E_RELEASESTAGE_DONE:
                meReleaseStage = E_RELEASESTAGE_START;
                return true;

            default:
                CGS_ASSERT(false, "Unknown release stage");
                return false;
        }
    }

    // X360 0x8284FE50.
    void GuiResourceModule::Destruct()
    {
        mReceiverQueue.Clear();
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // X360 0x828581A8. Every record in the input buffer's load-request queue is a
    // GuiEventLoadRequest (the X360 does not test the queue id); a DONE machine is kicked
    // back to the initial-request stage unconditionally at the tail.
    void GuiResourceModule::ProcessIncomingLoadRequests(InputBuffer* lpInput)
    {
        const InputBuffer::GuiEventQueue* lpLoadRequests = lpInput->GetLoadRequests();
        CGS_ASSERT(lpLoadRequests != 0,
                   "Invalid load request queue in GuiResourceModule::ProcessIncomingLoadRequests");

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        lpLoadRequests->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            AddToBundleLoadQueue(reinterpret_cast<const GuiEventLoadRequest*>(lpEvent));

            const CgsModule::Event* lpNextEvent = 0;
            lpLoadRequests->GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent = lpNextEvent;
        }

        if (meAcquireStage == E_ACQUIRESTAGE_DONE)
            meAcquireStage = E_AQUIRESTAGE_INITIAL_REQUEST_RESOURCES;
    }

    // X360 0x82848090. Build the queue record: bare-name hash (or the carried resource id
    // for nameless records), the CONTAINER type's path template, the routed bank id; insert
    // + keep the queue sorted ascending by type (PopFromBundleLoadQueue pops the tail, so
    // higher-numbered types service first). Case values are the ARTIST enum (see the
    // template table note); the assert messages name the original's AddBundleToQueue.
    void GuiResourceModule::AddToBundleLoadQueue(const GuiEventLoadRequest* lpRequest)
    {
        CGS_ASSERT(lpRequest != 0, "Invalid load request sent to GuiResourceModule::AddBundleToQueue");

        GuiBundleToLoad& lrBundle = maBundlesToLoad[miNumBundlesToLoad];

        if (lpRequest->mpacFileToLoad != 0)
        {
            lrBundle.mpcBundleNameToLoad    = lpRequest->mpacFileToLoad;
            lrBundle.muResourceId           = 0;
            lrBundle.muBundleNameToLoadHash = CgsContainers::CgsHash::CalculateHash(
                const_cast<char*>(lpRequest->mpacFileToLoad),
                static_cast<int>(std::strlen(lpRequest->mpacFileToLoad)));
        }
        else
        {
            lrBundle.mpcBundleNameToLoad    = 0;
            lrBundle.muBundleNameToLoadHash = 0;
            lrBundle.muResourceId           = lpRequest->muResourceId;
        }

        lrBundle.meResourceType       = lpRequest->meRequestType;
        lrBundle.meLoadUnload         = lpRequest->meLoadUnload;
        lrBundle.muRequestId          = lpRequest->muLoadRequestId;
        lrBundle.mbParsed             = false;
        lrBundle.mbLoaded             = false;
        lrBundle.muUnloadFileNameHash = 0;
        lrBundle.mResourceHandle.mpResourceMemory = 0;
        lrBundle.mResourceHandle.mpSourceEntry    = 0;

        switch (static_cast<s32>(lpRequest->meRequestType))
        {
            case 4:    // streamed apt movies -> the HD/SD apt bundle
            case 5:
            case 6:
                lrBundle.mpcPathToBundle = mbHighDef ? KAAC_RESOURCE_TEMPLATES[2]
                                                     : KAAC_RESOURCE_TEMPLATES[3];
                lrBundle.miBundleBank    = miAptStreamedBundleBank;
                break;
            case 7:    // persistent apt -> the HD/SD apt bundle, persistent bank
                lrBundle.mpcPathToBundle = mbHighDef ? KAAC_RESOURCE_TEMPLATES[2]
                                                     : KAAC_RESOURCE_TEMPLATES[3];
                lrBundle.miBundleBank    = miAptPersistentBundleBank;
                break;
            case 10:   // persistent flapt -> the HD/SD flapt bundle, persistent bank
                lrBundle.mpcPathToBundle = (mbHighDef == true) ? KAAC_RESOURCE_TEMPLATES[8]
                                                               : KAAC_RESOURCE_TEMPLATES[9];
                lrBundle.miBundleBank    = miAptPersistentBundleBank;
                break;
            case 11:   // texture -> plain bundle, textures pool
                lrBundle.mpcPathToBundle = KAAC_RESOURCE_TEMPLATES[1];
                lrBundle.miBundleBank    = miTexturesBundlePoolId;
                break;
            case 12:   // localised text -> the language bundle; the record keeps the carried id
                lrBundle.mpcPathToBundle = KAAC_RESOURCE_TEMPLATES[13];
                lrBundle.miBundleBank    = miLanguageBundleBank;
                lrBundle.muResourceId    = lpRequest->muResourceId;
                break;
            case 16:   // font data -> the HD/SD font bundle
                lrBundle.mpcPathToBundle = mbHighDef ? KAAC_RESOURCE_TEMPLATES[14]
                                                     : KAAC_RESOURCE_TEMPLATES[15];
                lrBundle.miBundleBank    = miFontBundleBank;
                break;
            case 18:   // fsm -> the fsm bundle (the flow-controller load path)
                lrBundle.mpcPathToBundle = KAAC_RESOURCE_TEMPLATES[17];
                lrBundle.miBundleBank    = miFSMBundleBank;
                break;
            case 20:   // pfx -> the pfx bundle (shares the fsm bank)
                lrBundle.mpcPathToBundle = KAAC_RESOURCE_TEMPLATES[19];
                lrBundle.miBundleBank    = miFSMBundleBank;
                break;
            case 22:   // pfx colour cube -> the colour-cube dictionary
                lrBundle.mpcPathToBundle = KAAC_RESOURCE_TEMPLATES[21];
                lrBundle.miBundleBank    = miGlobalTexturePoolId;
                break;
            default:
                CGS_ASSERT(false, "Unexpected resource type in GuiResourceModule::AddBundleToQueue");
                break;
        }

        ++miNumBundlesToLoad;
        CGS_ASSERT(miNumBundlesToLoad <= KI_MAX_BUNDLES_TO_LOAD,
                   "Trying to load too many bundles at once in GuiResourceModule::AddBundleToQueue");

        std::qsort(maBundlesToLoad, static_cast<size_t>(miNumBundlesToLoad),
                   sizeof(GuiBundleToLoad), &GuiResourceModule::SortBundleQueue);
    }

    // X360 0x82848068. Ascending by request type (the X360 compares the +0x18 field).
    s32 GuiResourceModule::SortBundleQueue(const void* lpLeft, const void* lpRight)
    {
        const GuiBundleToLoad* lpBundleLeft  = static_cast<const GuiBundleToLoad*>(lpLeft);
        const GuiBundleToLoad* lpBundleRight = static_cast<const GuiBundleToLoad*>(lpRight);
        if (lpBundleLeft->meResourceType > lpBundleRight->meResourceType)
            return 1;
        if (lpBundleLeft->meResourceType < lpBundleRight->meResourceType)
            return -1;
        return 0;
    }

    // X360 0x82848450.
    bool GuiResourceModule::IsBundleQueuedToLoad()
    {
        return miNumBundlesToLoad > 0;
    }

    // X360 0x82848468. Pops the tail record by value.
    GuiResourceModule::GuiBundleToLoad GuiResourceModule::PopFromBundleLoadQueue()
    {
        CGS_ASSERT(miNumBundlesToLoad > 0, "miNumBundlesToLoad > 0");
        --miNumBundlesToLoad;
        return maBundlesToLoad[miNumBundlesToLoad];
    }

    // X360 0x8285E7C0. Format the container path from the record's template, post the
    // LoadBundleRequest (queue type 2; X360 payload 148 -- the pointer member widens on
    // x64) with this module's receiver queue as the reply target, and count it outstanding.
    void GuiResourceModule::LoadBundle(GuiBundleToLoad& lrBundle, OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != 0, "Invalid output buffer in GuiResourceModule::LoadBundle");

        char lacFileName[128];
        CgsCore::SPrintf(lacFileName, 128, lrBundle.mpcPathToBundle, lrBundle.mpcBundleNameToLoad);

        CgsResource::Events::LoadBundleRequest lRequest;
        lRequest.mpUser              = &mReceiverQueue;
        lRequest.miEventId           = static_cast<s32>(lrBundle.meResourceType);
        lRequest.SetFileName(lacFileName);
        lRequest.miPoolId            = lrBundle.miBundleBank;
        lRequest.mbLiveUpdateReplace = false;
        lRequest.mbAllowFailiure     = false;
        lRequest.mbUseHDCache        = false;

        lpOutput->GetResourceRequestQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 2,
            static_cast<s32>(sizeof(lRequest)));
        ++mNumWaitingResourceAllocations;
    }

    // X360 0x8285E8C8. Format the container path, stamp the record's full-path hash (the
    // key ParseUnloaded matches the completion by), post the UnloadBundleRequest (queue
    // type 3; X360 payload 144), then the unload-request notification, and count it.
    void GuiResourceModule::UnloadBundle(GuiBundleToLoad& lrBundle, OutputBuffer* lpOutput)
    {
        char lacFileName[128];
        CgsCore::SPrintf(lacFileName, 128, lrBundle.mpcPathToBundle, lrBundle.mpcBundleNameToLoad);

        lrBundle.muUnloadFileNameHash = CgsContainers::CgsHash::CalculateHash(
            lacFileName, static_cast<int>(std::strlen(lacFileName)));

        CgsResource::Events::UnloadBundleRequest lRequest;
        lRequest.mpUser              = &mReceiverQueue;
        lRequest.miEventId           = static_cast<s32>(lrBundle.meResourceType);
        lRequest.SetFileName(lacFileName);
        lRequest.miPoolId            = lrBundle.miBundleBank;
        lRequest.mbLiveUpdateReplace = false;

        lpOutput->GetResourceRequestQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 3,
            static_cast<s32>(sizeof(lRequest)));

        GuiEventUnloadRequestNotification lNotification;
        lNotification.meRequestType   = lrBundle.meResourceType;
        lNotification.muLoadRequestId = lrBundle.muRequestId;
        lNotification.muFileNameHash  = lrBundle.muBundleNameToLoadHash;
        lpOutput->AddUnloadRequestNotification(&lNotification);

        ++mNumWaitingResourceAllocations;
    }

    // X360 0x8285E9A8. Post the acquire-by-id request (queue type 4; X360 payload 24): the
    // localised-text type (12) and nameless records carry their resource id directly;
    // everything else hashes the type's templated name. mbCheckRefCount: the X360 record
    // leaves the +0x0C..0x17 pad slots unwritten (the 24-byte AddEvent copies stack
    // residue); the PC build zeroes the flag deterministically.
    void GuiResourceModule::RequestResource(GuiBundleToLoad& lrBundle, OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != 0, "Invalid output buffer in GuiResourceModule::RequestResource");

        CgsResource::Events::AcquireResourceRequest lRequest;
        lRequest.mpUser         = &mReceiverQueue;
        lRequest.miEventId      = static_cast<s32>(lrBundle.muRequestId);
        lRequest.miPoolId       = lrBundle.miBundleBank;
        lRequest.mbCheckRefCount = false;

        if (static_cast<s32>(lrBundle.meResourceType) == 12 || lrBundle.mpcBundleNameToLoad == 0)
        {
            lRequest.mResourceId.SetHash(lrBundle.muResourceId);
        }
        else
        {
            char lacResourceName[128];
            CgsCore::SPrintf(lacResourceName, 128,
                             KAAC_RESOURCE_TEMPLATES[static_cast<s32>(lrBundle.meResourceType)],
                             lrBundle.mpcBundleNameToLoad);
            // The bundle entry ids are the zero-extended 32-bit name hash (verified against
            // the PC repack: FSM\BRNLEGALFSM.BUNDLE's LuaCode entry id == HashString("BRNLEGALFSM")).
            lRequest.mResourceId.SetHash(static_cast<u32>(
                CgsResource::ID::HashString(reinterpret_cast<const u8*>(lacResourceName))));
        }

        lpOutput->GetResourceRequestQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 4,
            static_cast<s32>(sizeof(lRequest)));
        ++mNumWaitingResourceAllocations;
    }

    // X360 0x8284EF48. Clears the outgoing request queue (the downstream modules consumed
    // this frame's records), then reports whether responses are still outstanding: the
    // issued count vs the receiver queue's received count (X360 +0x254 vs +0x1184 --
    // +0x1184 is mReceiverQueue's miCount field).
    bool GuiResourceModule::HasPendingResourceRequests(OutputBuffer* lpOutput)
    {
        lpOutput->GetResourceRequestQueue()->Clear();
        return mNumWaitingResourceAllocations != mReceiverQueue.GetLength();
    }

    // X360 0x8285F708. Every pending LOAD record that did not acquire directly gets its
    // container bundle file loaded.
    void GuiResourceModule::LoadBundleForMissingResources(OutputBuffer* lpOutput)
    {
        for (s32 li = 0; li < miNumPendingResources; ++li)
        {
            GuiBundleToLoad& lrBundle = maBundlesPending[li];
            if (!lrBundle.mbLoaded && lrBundle.meLoadUnload == E_GUI_RESOURCEREQUEST_LOAD)
            {
                CGS_ASSERT(lrBundle.mbParsed == false, "lBundle.mbParsed == false");
                LoadBundle(lrBundle, lpOutput);
            }
        }
    }

    // X360 0x8284FE88. Walk the receiver queue's type-4 acquire responses: each matches the
    // first unparsed pending LOAD record with the same request id; a response whose handle
    // is not the default (null) handle marks the record parsed+loaded and stores the handle
    // (the X360 compares against its zero-initialised module-static default handle at
    // 0x8305F174/78 -- the null handle on the x64 build).
    void GuiResourceModule::ParseForAcquiredResources()
    {
        if (mReceiverQueue.GetLength() <= 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (liType == 4)
            {
                const GuiResourceAcquireResponse* lpResponse =
                    reinterpret_cast<const GuiResourceAcquireResponse*>(lpEvent);

                for (s32 li = 0; li < miNumPendingResources; ++li)
                {
                    GuiBundleToLoad& lrBundle = maBundlesPending[li];
                    if (!lrBundle.mbParsed &&
                        lrBundle.meLoadUnload == E_GUI_RESOURCEREQUEST_LOAD &&
                        lpResponse->miEventId == static_cast<s32>(lrBundle.muRequestId))
                    {
                        CGS_ASSERT(lrBundle.mbLoaded == false, "lBundle.mbLoaded == false");
                        if (lpResponse->mResourceHandle.mpResourceMemory != 0 ||
                            lpResponse->mResourceHandle.mpSourceEntry != 0)
                        {
                            lrBundle.mbParsed        = true;
                            lrBundle.mbLoaded        = true;
                            lrBundle.mResourceHandle = lpResponse->mResourceHandle;
                        }
                        break;
                    }
                }
            }

            const CgsModule::Event* lpNextEvent = 0;
            liType = mReceiverQueue.GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent = lpNextEvent;
        }
    }

    // X360 0x8284FFE8. Walk the receiver queue's type-3 unload completions: hash the
    // response's file path (the X360 reads the string at the BundleLoaderEvent's X360 +9
    // macFileName slot) and mark the matching pending UNLOAD record parsed + unloaded
    // (`stb 1 / stb 0` onto mbParsed/mbLoaded in the X360 body).
    void GuiResourceModule::ParseUnloaded()
    {
        if (mReceiverQueue.GetLength() <= 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != 0)
        {
            if (liType == 3)
            {
                const CgsResource::Events::UnloadBundleRequest* lpResponse =
                    reinterpret_cast<const CgsResource::Events::UnloadBundleRequest*>(lpEvent);

                const u32 luFileNameHash = CgsContainers::CgsHash::CalculateHash(
                    const_cast<char*>(lpResponse->macFileName),
                    static_cast<int>(std::strlen(lpResponse->macFileName)));

                for (s32 li = 0; li < miNumPendingResources; ++li)
                {
                    GuiBundleToLoad& lrBundle = maBundlesPending[li];
                    if (lrBundle.meLoadUnload == E_GUI_RESOURCEREQUEST_UNLOAD &&
                        lrBundle.muUnloadFileNameHash == luFileNameHash)
                    {
                        lrBundle.mbParsed = true;
                        lrBundle.mbLoaded = false;
                        break;
                    }
                }
            }

            const CgsModule::Event* lpNextEvent = 0;
            liType = mReceiverQueue.GetNextEvent(lpEvent, &lpNextEvent, &liSize);
            lpEvent = lpNextEvent;
        }
    }

    // X360 0x8285F7A8. The per-frame module dispatch. Every arm of the X360 switch funnels
    // into the same UnlockForWrite tail, so the PC body restructures to a single tail --
    // which is also where the [PC] platform servicer runs (the point in the frame where the
    // console scheduler would run the downstream resource/pool modules against the
    // just-posted request queue).
    void GuiResourceModule::Update(InputBuffer* lpInput, OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpInput != 0, "lpInput != NULL");
        CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");

        lpInput->LockForRead();
        lpOutput->LockForWrite();

        ProcessIncomingLoadRequests(lpInput);
        lpInput->UnlockForRead();

        switch (meAcquireStage)
        {
            case E_ACQUIRESTAGE_START:
            {
                // The boot loads: PERSISTENTAPT (type 7, persistent apt bank) and
                // GUITEXTURES.BIN (type 11, textures pool), fire-and-forget (never enter
                // the pending set -- they are counted and waited on only).
                miNumPendingResources          = 0;
                mNumWaitingResourceAllocations = 0;

                GuiBundleToLoad lBundleToLoad = GuiBundleToLoad();
                lBundleToLoad.mpcBundleNameToLoad = "PERSISTENTAPT";
                lBundleToLoad.mpcPathToBundle     = mbHighDef ? KAAC_RESOURCE_TEMPLATES[2]
                                                              : KAAC_RESOURCE_TEMPLATES[3];
                lBundleToLoad.meResourceType      = static_cast<ResourceRequestTypes>(7);
                lBundleToLoad.miBundleBank        = miAptPersistentBundleBank;
                LoadBundle(lBundleToLoad, lpOutput);

                lBundleToLoad.mpcBundleNameToLoad = "GUITEXTURES.BIN";
                lBundleToLoad.mpcPathToBundle     = KAAC_RESOURCE_TEMPLATES[11];
                lBundleToLoad.meResourceType      = static_cast<ResourceRequestTypes>(11);
                lBundleToLoad.miBundleBank        = miTexturesBundlePoolId;
                LoadBundle(lBundleToLoad, lpOutput);

                meAcquireStage   = E_ACQUIRESTAGE_WAIT;
                meAfterWaitStage = E_AQUIRESTAGE_INITIAL_REQUEST_RESOURCES;
                break;
            }

            case E_ACQUIRESTAGE_WAIT:
                if (!HasPendingResourceRequests(lpOutput))
                    meAcquireStage = meAfterWaitStage;
                break;

            case E_AQUIRESTAGE_INITIAL_REQUEST_RESOURCES:
            {
                miNumPendingResources          = 0;
                mNumWaitingResourceAllocations = 0;
                mReceiverQueue.Clear();

                for (; IsBundleQueuedToLoad(); ++miNumPendingResources)
                {
                    if (miNumPendingResources >= KI_MAX_RESOURCES_WITHOUT_WAITING)
                        break;

                    GuiBundleToLoad lBundle = PopFromBundleLoadQueue();
                    if (lBundle.meLoadUnload != E_GUI_RESOURCEREQUEST_LOAD)
                        UnloadBundle(lBundle, lpOutput);
                    else
                        RequestResource(lBundle, lpOutput);
                    maBundlesPending[miNumPendingResources] = lBundle;
                }

                if (miNumPendingResources != 0)
                {
                    meAfterWaitStage = E_AQUIRESTAGE_LOAD_BUNDLES;
                    meAcquireStage   = E_ACQUIRESTAGE_WAIT;
                }
                else
                {
                    meAcquireStage = E_ACQUIRESTAGE_DONE;
                }
                break;
            }

            case E_AQUIRESTAGE_LOAD_BUNDLES:
                mNumWaitingResourceAllocations = 0;
                ParseForAcquiredResources();
                LoadBundleForMissingResources(lpOutput);
                ParseUnloaded();
                mReceiverQueue.Clear();
                if (mNumWaitingResourceAllocations != 0)
                {
                    meAfterWaitStage = E_AQUIRESTAGE_REQUEST_RESOURCE_AFTER_BUNDLE_LOAD;
                    meAcquireStage   = E_ACQUIRESTAGE_WAIT;
                }
                else
                {
                    meAcquireStage = E_AQUIRESTAGE_SEND_NOTIFICATIONS;
                }
                break;

            case E_AQUIRESTAGE_REQUEST_RESOURCE_AFTER_BUNDLE_LOAD:
                mNumWaitingResourceAllocations = 0;
                mReceiverQueue.Clear();
                for (s32 li = 0; li < miNumPendingResources; ++li)
                {
                    GuiBundleToLoad& lrBundle = maBundlesPending[li];
                    if (!lrBundle.mbParsed && !lrBundle.mbLoaded)
                        RequestResource(lrBundle, lpOutput);
                }
                meAcquireStage   = E_ACQUIRESTAGE_WAIT;
                meAfterWaitStage = E_AQUIRESTAGE_REQUEST_REMAINING_RESOURCES;
                break;

            case E_AQUIRESTAGE_REQUEST_REMAINING_RESOURCES:
                ParseForAcquiredResources();
                meAcquireStage = E_AQUIRESTAGE_SEND_NOTIFICATIONS;
                // fall through (the X360 falls straight into the notification sweep)
            case E_AQUIRESTAGE_SEND_NOTIFICATIONS:
            {
                for (s32 li = 0; li < miNumPendingResources; ++li)
                {
                    GuiBundleToLoad& lrBundle = maBundlesPending[li];
                    // FLAG PC-platform guard: on the console every pending record has been
                    // parsed by the time the notification sweep runs (the acquire response
                    // always carries real container data). On PC a MISSING bundle completes
                    // "without IO" with the null handle, so ParseForAcquiredResources
                    // deliberately never marks it parsed -- relax the assert for exactly
                    // that case (null resource handle on a LOAD record); the sweep below
                    // still posts the (null-resource) notification and marks it parsed.
                    CGS_ASSERT(lrBundle.mbParsed ||
                                   (lrBundle.meLoadUnload == E_GUI_RESOURCEREQUEST_LOAD &&
                                    lrBundle.mResourceHandle.mpResourceMemory == 0),
                               "lBundle.mbParsed");

                    if (lrBundle.meLoadUnload != E_GUI_RESOURCEREQUEST_LOAD)
                    {
                        GuiEventUnloadNotification lNotification;
                        lNotification.meRequestType   = lrBundle.meResourceType;
                        lNotification.muLoadRequestId = lrBundle.muRequestId;
                        lNotification.muFileNameHash  = lrBundle.muBundleNameToLoadHash;
                        lpOutput->AddUnloadNotification(&lNotification);
                    }
                    else
                    {
                        // The inlined ParseResource sweep: validate the handle (the X360
                        // dev-asserts then dereferences regardless -- log-and-continue),
                        // then post the load notification carrying it.
                        //
                        // FLAG PC-platform guard: on the console the container data always
                        // exists, so both dev-asserts hold and the second dereferences the
                        // handle unconditionally. On PC a MISSING bundle -- un-converted data,
                        // e.g. the sat-nav map/mask GUI textures the freeburn HUD requests --
                        // legitimately leaves a null resource handle. Guard both asserts on
                        // that condition (matching the resource module's existing "MISSING ...
                        // completed without IO" PC paths) and still post the notification
                        // (null resource), which StateLoadingHelper::OnLoadNotification
                        // tolerates by design. Without the guard every un-converted GUI
                        // texture spams a dev-assert on the boot path.
                        void* const* lppResourceMemory =
                            static_cast<void* const*>(lrBundle.mResourceHandle.mpResourceMemory);
                        const bool lbResourcePresent = lppResourceMemory != 0;
                        CGS_ASSERT(!lbResourcePresent || *lppResourceMemory != 0,
                                   "Invalid memory resource in GuiResourceModule::ParseResource");

                        GuiEventLoadNotification lNotification;
                        lNotification.mResourceHandle = lrBundle.mResourceHandle;
                        lNotification.meRequestType   = lrBundle.meResourceType;
                        lNotification.muLoadRequestId = lrBundle.muRequestId;
                        lpOutput->AddLoadNotification(&lNotification);
                    }

                    lrBundle.mbParsed = true;
                }
                meAcquireStage = E_AQUIRESTAGE_INITIAL_REQUEST_RESOURCES;
                break;
            }

            case E_ACQUIRESTAGE_DONE:
                break;

            default:
                CGS_ASSERT(false, "Invalid state in GuiResourceModule::Update");
                break;
        }

        // [PC] service the request queue this frame's stage just posted (the console
        // scheduler runs the downstream resource/pool modules here); see the FLAG'd body.
        ServicePlatformRequests(lpOutput);

        lpOutput->UnlockForWrite();
    }
}
