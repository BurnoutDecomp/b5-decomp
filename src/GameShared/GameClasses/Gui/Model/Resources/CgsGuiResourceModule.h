#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"        // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"      // CgsModule::EventReceiverQueue<N,16>
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // GuiResourceModuleIO buffers + request/notification records
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // CgsResource::ResourceHandle
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // CgsResource::Events::AcquireResourceRequest (receiver echo base)

// CgsGui::GuiResourceModule -- the GUI model's resource-loading module: the per-frame
// dispatch that drains the ModelIO load-request queue, drives the acquire/load/notify
// state machine against the resource system, and posts GuiEventLoadNotification /
// GuiEventUnloadNotification records into the GUI resource output buffer.
//
// Shape: DecFIGS DWARF (CgsGuiResourceModule.h:48-309 -- every member/enum/method name
// below is the DWARF's). Behaviour: BURNOUT_X360_ARTIST.XEX (addresses on each member in
// CgsGuiResourceModule.cpp). X360 layout cross-check (all offsets reproduced by the asm):
//   +0x228 mePrepareStage      +0x22C meReleaseStage
//   +0x230 meAcquireStage      +0x234 meAfterWaitStage
//   +0x238..+0x250 the seven bank/pool ids (Prepare args, in member order)
//   +0x254 mNumWaitingResourceAllocations   +0x258 miNumBundlesToLoad
//   +0x25C miNumPendingResources
//   +0x260 maBundlesToLoad[64]  (56-byte records)   +0x1060 maBundlesPending[5]
//   +0x1178 mbHighDef           +0x117C mReceiverQueue (EventReceiverQueue<8192,16>)
// The x64 gate keeps semantic parity by NAMED members (pointer members widen), per the
// project rule; the X360 byte offsets above are documentation, not layout pins.
namespace CgsGui
{
    struct GuiResourceModule : public CgsModule::ModuleSingleBuffered
    {
        typedef GuiResourceModuleIO::InputBuffer   InputBuffer;
        typedef GuiResourceModuleIO::OutputBuffer  OutputBuffer;
        typedef CgsGui::GuiEventQueueSmall         GuiEventQueueSmall;

        // DWARF CgsGuiResourceModule.h:129-131.
        static const s32 KI_RESOURCE_RECEIVER_QUEUE       = 8192;
        // [FLAG PC-platform widening] the console value is 64 (X360 AddBundleToQueue
        // @0x82848090: `cmpwi r11, 0x40`). This build's HUD-flow stand-in (the inline
        // BrnHudFlow hack that replaces the Lua FSM -- see the Lua-system note) issues the
        // whole component-resource roster in ONE frame where the console's scripted flow
        // staggers it, so with the boost bar's twelve texture requests live the single-frame
        // peak measured 65 (the [gui-queue] diag, 2026-08-25: 50+ type-7 component records +
        // the boost textures). Widened -- capacity only, the queue mechanics and the assert
        // are unchanged -- until the faithful Lua flow retires the stand-in.
        static const s32 KI_MAX_BUNDLES_TO_LOAD           = 128;
        static const s32 KI_MAX_RESOURCES_WITHOUT_WAITING = 5;

        // DWARF CgsGuiResourceModule.h:51.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START         = 0,
            E_PREPARESTAGE_MANAGER       = 1,
            E_PREPARESTAGE_RECEIVERQUEUE = 2,
            E_PREPARESTAGE_BANKID        = 3,
            E_PREPARESTAGE_DONE          = 4,
        };

        // DWARF CgsGuiResourceModule.h:62.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };

        // DWARF CgsGuiResourceModule.h:71 (the "AQUIRE" spellings are the original's).
        enum EAcquireStage
        {
            E_ACQUIRESTAGE_START                             = 0,
            E_ACQUIRESTAGE_WAIT                              = 1,
            E_AQUIRESTAGE_INITIAL_REQUEST_RESOURCES          = 2,
            E_AQUIRESTAGE_LOAD_BUNDLES                       = 3,
            E_AQUIRESTAGE_REQUEST_RESOURCE_AFTER_BUNDLE_LOAD = 4,
            E_AQUIRESTAGE_REQUEST_REMAINING_RESOURCES        = 5,
            E_AQUIRESTAGE_SEND_NOTIFICATIONS                 = 6,
            E_ACQUIRESTAGE_DONE                              = 7,
        };

        // DWARF CgsGuiResourceModule.h:151-163. One queued/pending bundle request. The
        // X360 record is 56 bytes; the x64 build widens the pointer members.
        struct GuiBundleToLoad
        {
            const char*                 mpcBundleNameToLoad;   // X360 +0x00
            u64                         muResourceId;          // X360 +0x08 (type-12 requests carry the id directly)
            u32                         muBundleNameToLoadHash;// X360 +0x10 (CgsHash of the bare name)
            const char*                 mpcPathToBundle;       // X360 +0x14 (KAAC_RESOURCE_TEMPLATES[] format string)
            ResourceRequestTypes        meResourceType;        // X360 +0x18 (ARTIST numbering -- see the .cpp table)
            ResourceRequestLoadUnload   meLoadUnload;          // X360 +0x1C
            u32                         muRequestId;           // X360 +0x20 (echoed into the notification)
            s32                         miBundleBank;          // X360 +0x24 (routed bank/pool id)
            bool                        mbParsed;              // X360 +0x28
            bool                        mbLoaded;              // X360 +0x29
            u32                         muUnloadFileNameHash;  // X360 +0x2C (full-path hash, written by UnloadBundle)
            CgsResource::ResourceHandle mResourceHandle;       // X360 +0x30 (filled by ParseForAcquiredResources)
        };

        // X360 @0x8284FC08 (DWARF CgsGuiResourceModule.cpp:96): base Construct, bind the
        // receiver queue to its embedded buffer, seed the counters/stages, mark new-module.
        virtual void Construct(bool lHighDef);

        // X360 @0x8284FC88 (DWARF :132): staged prepare -- base manager prepare, clear the
        // receiver queue, store the seven bank/pool ids, then arm the acquire machine.
        // Argument order == member order (proven by the X360 stores: a2->+0x238 .. a8->+0x250).
        virtual bool Prepare(s32 liAptPersistentBundleBank,
                             s32 liAptStreamedBundleBank,
                             s32 liFontBundleBank,
                             s32 liFSMBundleBank,
                             s32 liLanguageBundleBank,
                             s32 liTexturesBundlePoolId,
                             s32 liGlobalTexturePoolId);

        // X360 @0x8284FD90 (DWARF :207).
        virtual bool Release();

        // X360 @0x8284FE50 (DWARF :256).
        virtual void Destruct();

        // X360 @0x8285F7A8 (DWARF :422): the per-frame module dispatch -- drain the input
        // buffer's load requests, run the acquire state machine, post the outgoing resource
        // requests + load/unload notifications into the output buffer.
        virtual void Update(InputBuffer* lpInput, OutputBuffer* lpOutput);

        // DWARF-declared pass-through helpers (CgsGuiResourceModule.h:253/294/309; not
        // X360-emitted as standalone bodies -- the buffer accessors inline through them).
        const OutputBuffer::GuiResourceRequestQueue* GetResourceRequestQueue(const OutputBuffer* lpOutput) const
        {
            return lpOutput->GetResourceRequestQueue();
        }
        void AddResourceRequests(const GuiEventQueueSmall* lpRequests, InputBuffer* lpInput)
        {
            lpInput->AddResourceRequests(lpRequests);
        }
        const InputBuffer::GuiEventQueue* GetLoadedNotifications(const OutputBuffer* lpOutput) const
        {
            return lpOutput->GetLoadNotifications();
        }

    private:
        // X360 @0x82848068 (DWARF :730): qsort comparator -- ascending meResourceType.
        static s32 SortBundleQueue(const void* lpLeft, const void* lpRight);

        // X360 @0x828581A8 (DWARF :690): drain every queued GuiEventLoadRequest into the
        // bundle load queue; kick a DONE machine back to the initial-request stage.
        void ProcessIncomingLoadRequests(InputBuffer* lpInput);

        // X360 @0x82848090 (DWARF :758): build the GuiBundleToLoad record (name hash, path
        // template, routed bank id) and insert it sorted.
        void AddToBundleLoadQueue(const GuiEventLoadRequest* lpRequest);

        // X360 @0x82848450 (DWARF :923).
        bool IsBundleQueuedToLoad();

        // X360 @0x82848468 (DWARF :945): pop the tail (highest-type) record by value.
        GuiBundleToLoad PopFromBundleLoadQueue();

        // X360 @0x8285E7C0 (DWARF :965): post a LoadBundleRequest (queue type 2) for the
        // record's templated path into its bank; count the outstanding allocation.
        void LoadBundle(GuiBundleToLoad& lrBundle, OutputBuffer* lpOutput);

        // X360 @0x8285E8C8 (DWARF :1002): post an UnloadBundleRequest (queue type 3),
        // record the full-path hash, and post the unload-request notification.
        void UnloadBundle(GuiBundleToLoad& lrBundle, OutputBuffer* lpOutput);

        // X360 @0x8285E9A8 (DWARF :1046): post an AcquireResourceRequest (queue type 4)
        // for the record's resource id (templated-name hash, or the carried id for the
        // localised-text type / nameless records).
        void RequestResource(GuiBundleToLoad& lrBundle, OutputBuffer* lpOutput);

        // X360 @0x8284EF48 (DWARF CgsGuiResourceModule.h:271): clear the outgoing request
        // queue; report whether responses are still outstanding.
        bool HasPendingResourceRequests(OutputBuffer* lpOutput);

        // X360 @0x8285F708 (DWARF :274): LoadBundle every pending load whose resource did
        // not acquire directly.
        void LoadBundleForMissingResources(OutputBuffer* lpOutput);

        // X360 @0x8284FE88 (DWARF :299): match type-4 receiver responses onto the pending
        // records; store the resolved handle, mark parsed+loaded.
        void ParseForAcquiredResources();

        // X360 @0x8284FFE8 (DWARF :362): match type-3 receiver responses (unload
        // completions, by full-path hash) onto the pending records; mark parsed, unloaded.
        void ParseUnloaded();

        // [PC] platform request servicer -- the synchronous stand-in for the console's
        // resource/pool module chain that consumes the output buffer's request queue and
        // posts the responses back into mReceiverQueue. Runs at the Update tail (the spot
        // the console scheduler would run the downstream modules). Defined in
        // CgsGuiResourceModulePC.cpp (FLAG'd PC-platform leaf).
        void ServicePlatformRequests(OutputBuffer* lpOutput);

        // ---- members (DWARF :132-173; X360 offsets in the class comment) --------------
        EPrepareStage   mePrepareStage;
        EReleaseStage   meReleaseStage;
        EAcquireStage   meAcquireStage;
        EAcquireStage   meAfterWaitStage;

        s32             miAptPersistentBundleBank;   // types 7 (apt persistent) + 10 (flapt persistent)
        s32             miAptStreamedBundleBank;     // types 4/5/6 (streamed apt movies)
        s32             miFontBundleBank;            // type 16 (font data)
        s32             miFSMBundleBank;             // types 18 (fsm) + 20 (pfx)
        s32             miLanguageBundleBank;        // type 12 (localised text)
        s32             miTexturesBundlePoolId;      // type 11 (textures / GUITEXTURES.BIN)
        s32             miGlobalTexturePoolId;       // type 22 (pfx colour cube)

        s32             mNumWaitingResourceAllocations;
        s32             miNumBundlesToLoad;
        s32             miNumPendingResources;

        GuiBundleToLoad maBundlesToLoad[KI_MAX_BUNDLES_TO_LOAD];
        GuiBundleToLoad maBundlesPending[KI_MAX_RESOURCES_WITHOUT_WAITING];

        bool            mbHighDef;

        CgsModule::EventReceiverQueue<KI_RESOURCE_RECEIVER_QUEUE, 16> mReceiverQueue;
    };

    // DWARF CgsGuiResourceModule.h:236 / X360 @0x82847190: post-increment with the
    // stage-overrun dev assert (the baked message names the original's parameter).
    GuiResourceModule::EReleaseStage operator++(GuiResourceModule::EReleaseStage& lreStage, int);

    // The type-4 record the resource system posts back into the module's receiver queue:
    // the AcquireResourceRequest echoed with the resolved handle appended. X360 32 bytes
    // (the 24-byte request + ResourceHandle at +0x18) -- ParseForAcquiredResources
    // (@0x8284FE88) reads the request id at +0x04 and the handle words at +0x18/+0x1C,
    // exactly this shape at X360 widths.
    struct GuiResourceAcquireResponse : public CgsResource::Events::AcquireResourceRequest
    {
        CgsResource::ResourceHandle mResourceHandle;
    };
}
