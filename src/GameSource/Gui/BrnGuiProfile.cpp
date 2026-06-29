#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"  // CgsContainers::CgsHash::CalculateHash

// ===========================================================================
// BrnGui::ProfileManager  (GameSource/Gui/BrnGuiProfile.{h,cpp})
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. ProfileManager owns the boot/save/
// load lifecycle of the player profile (Progression / LiveRevenge / OptionsData
// segments are held as fixed-size opaque buffers inside mStoredData, totalling
// ~262KB). It is a CgsGui::SystemUserProfile::Listener and drives a save/load
// system plus an optional on-screen ProfileMessageDisplay.
//
// This is an ISOLATED translation unit. The full BrnGuiProfile.h class header is
// not yet homed in the tree (it pulls in ~10 not-yet-reconstructed GUI/resource
// dependency types: GuiCache, SystemUserProfile, SaveLoadSystem, the GUI event
// queues, the resource-IO events, ...). Following the precedent set by
// BrnGuiProfileHost.cpp in this same directory, this TU keeps its own minimal
// declarations for the callees it touches so they do not clash with the real
// headers pulled into other TUs. Every member offset used below is pinned to the
// X360 store offsets (verified against the disassembly).
// ===========================================================================

// ---------------------------------------------------------------------------
// Minimal callee declarations (isolated-TU shims; bodies live in their own TUs).
// Declared in their canonical namespaces so the reconstructed call sites read
// naturally. Kept opaque -- this TU never defines their layout.
// ---------------------------------------------------------------------------

// Platform memcpy intrinsic used by the X360 build.
extern "C" void* XMemCpy(void* lpDest, const void* lpSrc, unsigned int luCount);

namespace BrnGui
{
    // Source image descriptor passed to the autosave/export image paths.
    //   +0  data pointer
    //   +4  size in bytes
    //   +8  source buffer pointer
    //   +12 user/slot field
    // (Matched to the a2[0..3] reads in CopyImageToBuffer.)
    struct ImageFileInfo
    {
        void* mpData;
        s32   miSize;
        void* mpSource;
        s32   miField3;
    };

    // On-screen profile message display (3-virtual interface). Held by pointer.
    struct ProfileMessageDisplay;

    // Result-handler callback object, held by pointer.
    struct ProfileTaskResultHandler;
}

namespace BrnProgression { struct ProgressionData; class Profile; }
namespace BrnNetwork      { class LiveRevengeProfile; }

namespace CgsGui
{
    // The option-callback receiver invoked by HandleMessageChoice. The X360 call
    // is `(*(*mpOptionHandler))(mpOptionHandler, choice)` -- vtable slot 0 takes
    // (this, uint32_t). Modelled as a 1-virtual interface.
    struct MessageDisplayOptionHandler
    {
        virtual void HandleOption(u32 luChoice) = 0;
    };

    // Save/load subsystem embedded in ProfileManager at +4200; only the image-copy
    // entry point is referenced here.
    struct SaveLoadSystem
    {
        // CgsGui::SaveLoadSystem::CopyImageToBuffer(this, const MugshotImageRequest*)
        s32 CopyImageToBuffer(const void* lpRequest);
    };
}

// ---------------------------------------------------------------------------
// LoadProfileUpgradeData dependencies (resource / GUI event-queue machinery).
// This routine drives a small "profile upgrade" load state machine; its context
// object (ctx below) is independent of ProfileManager's layout.
// ---------------------------------------------------------------------------
namespace CgsResource
{
    namespace Events
    {
        // CgsResource::Events::BundleLoaderEvent::SetFileName @ 0x822770F8.
        struct BundleLoaderEvent
        {
            BundleLoaderEvent& SetFileName(const char* lpcFileName);
        };
    }
    class ID
    {
    public:
        static s32 HashString(const u8* lpString);
    };
    struct BaseResourcePtr
    {
        // CgsResource::BaseResourcePtr::CreateFromHandle(this, const u32* lpHandle)
        BaseResourcePtr* CreateFromHandle(const void* lpHandle);
    };
}

namespace CgsModule
{
    // CgsModule::BaseEventReceiverQueue -- iterated/cleared while draining a load.
    struct BaseEventReceiverQueue
    {
        // +0  base ptr   +8  count   +12 head offset (matches a3[0],a3[2],a3[3])
        void* mpBase;
        s32   miReserved1;
        s32   miCount;
        s32   miHeadOffset;

        void GetNextEvent(s32 liCurrent, s32* lpNext, s32* lpType);
        void Clear();
    };

    // CgsModule::VariableEventQueue<2048,16>::AddEvent(this, payload, type, size).
    struct VariableEventQueue2048
    {
        s32 AddEvent(const void* lpPayload, s32 liType, s32 liSize);
    };
}

namespace CgsGui
{
    // CgsGui::CgsGuiModuleIO::OutputBuffer::GetGu -- returns the request queue the
    // upgrade events are pushed onto.
    struct OutputBuffer
    {
        CgsModule::VariableEventQueue2048* GetGu();
    };
}

namespace BrnGui
{
// ===========================================================================
// ProfileManager layout
//
// Only the members the seven reconstructed functions touch are named; the rest
// of the ~262KB object is reserved with explicit padding so every named member
// lands at its X360-confirmed byte offset. The first 16 bytes are the four
// vtable pointers (SystemUserProfile::Listener + the message-display interfaces),
// written by the constructor.
// ===========================================================================
struct ProfileManager
{
    enum EProfileOperation
    {
        E_PROFILEOPERATION_NONE        = 0,
        E_PROFILEOPERATION_LOAD        = 1,
        E_PROFILEOPERATION_SAVE        = 2,
        E_PROFILEOPERATION_AUTOSAVE    = 3,
        E_PROFILEOPERATION_LOAD_IMAGES = 4,
        E_PROFILEOPERATION_EXPORT_IMAGE = 5,
    };

    // The X360 mugshot save buffer: magic + hash + 9600-byte picture payload.
    // CopyImageToBuffer fills *(buf+0)=magic, *(buf+1)=hash, picture at buf+8.
    struct MugshotSaveBuffer
    {
        u32  muMagicNumber;        // KU_CORRECT_MAGIC_NUMBER once written
        u32  muHash;
        char macBuffer[9600];      // KI_MUGSHOT_PICTURE_SIZE_BYTES
    };

    // --- offset 0 ---
    void* mapVTable[4];                 // +0   four vtable pointers (ctor-initialised)

    // --- offset 16 (0x10) ---
    u8    mPad0010[4192 - 16];          // +16  ..  GuiCache*, SystemUserProfile*, the
                                        //      embedded GuiEventQueueSmall, malloc ptrs,
                                        //      etc. (not touched here)

    // --- offset 4192 ..; muOptionCount @ +4792 (0x12B8), mpOptionHandler @ +4796 ---
    u8    mPad1060[4792 - 4192];        // +4192 .. +4791
    u32   muOptionCount;                // +4792 (0x12B8)
    CgsGui::MessageDisplayOptionHandler* mpOptionHandler; // +4796 (0x12BC)

    // mSaveLoadSystem is at +4200 (0x1068). It precedes muOptionCount, so it lives
    // inside mPad1060 above; expose it through a typed accessor (see GetSaveLoadSystem).

    // --- offset 4800 .. mpMessageDisplay @ +267304 (0x41428) ---
    u8    mPad12C0[267304 - 4800];      // +4800 .. +267303

    // --- the message-display / progression pointers cluster ---
    ProfileMessageDisplay* mpMessageDisplay;        // +267304 (0x41428)
    void*                  mpReserved267308;        // +267308
    BrnProgression::Profile* mpProgressionProfile;  // +267312 (0x41430)
    const BrnProgression::ProgressionData* mpProgressionData; // +267316 (0x41434)

    // --- offset 267320 .. mugshot circular buffer pointer @ +267376 (0x41470) ---
    u8    mPad41438[267376 - 267320];   // +267320 .. +267375
    MugshotSaveBuffer*     mpaMugshotCircularBuffer; // +267376 (0x41470)

    // Tail of the object (remaining members) is reserved so sizeof() stays sane;
    // not referenced by the reconstructed functions.
    u8    mPad41474[8];

    // CgsGui::SaveLoadSystem lives at byte +4200 inside mPad1060. Access it by
    // name via this accessor rather than a raw cast at the call site.
    CgsGui::SaveLoadSystem* GetSaveLoadSystem()
    {
        return reinterpret_cast<CgsGui::SaveLoadSystem*>(
            reinterpret_cast<u8*>(this) + 4200);
    }

    // --- public reconstructed methods ---
    void AttachMessageDisplay(ProfileMessageDisplay* lpMessageDisplay);
    void DetachMessageDisplay(ProfileMessageDisplay& lrMessageDisplay);
    void HandleMessageChoice(u32 luChoice);
    void SetProgressionProfile(BrnProgression::Profile* lpProgressionProfile,
                               const BrnProgression::ProgressionData* lpProgressionData);
    s32  CopyImageToBuffer(const ImageFileInfo* lpSourceImageFileInfo);

    ProfileManager();
};

// MugshotSaveBuffer magic number: ASM stores 0x4D554753 ('MUGS') at buf+0.
static const u32 KU_CORRECT_MAGIC_NUMBER       = 0x4D554753u;
// Picture payload size in bytes (asm immediate 0x2580 = 9600).
static const s32 KI_MUGSHOT_PICTURE_SIZE_BYTES = 9600;
// On-queue request size for the mugshot copy request (asm immediate 0x2588 = 9608).
static const s32 KI_MUGSHOT_REQUEST_SIZE_BYTES = 9608;

// ---------------------------------------------------------------------------
// ProfileManager::ProfileManager  @ 0x827E0CD0
//
// BLOCKED ctor (see `work block` reason). The X360 ctor does NOT write the members the
// earlier draft set here (muOptionCount/+4792, mpOptionHandler/+4796, mpMessageDisplay/+267304,
// mpReserved267308, mpProgressionProfile/+267312, mpProgressionData/+267316,
// mpaMugshotCircularBuffer/+267376) -- those were fabricated and have been REMOVED. The asm ctor
// instead: installs four vtable ptrs @+0/+4/+8/+12 (off_820D0090/0080/007C/006C); self-refs the
// embedded event-queue list (+32/+36/+40 = this+20; zero +20/+24/+28/+44/+80); installs the
// save-load-system / metadata vtables @+4200/+4204/+4596; and stamps -1 ("no signed-in user") into
// eight deep per-segment user-index fields (base r6=this+0x12C0: +0xE620; off r9 +0x460/0x8C8/0xD30/
// 0x1198/0x1600; off r10 +0x7B0/0xAC8). Reproducing those faithfully needs the real BrnGuiProfile.h
// layout (named members for the queue + the 8 sentinel fields) and the homed vtables -- not yet
// available in this isolated-TU model -- so the ctor is left de-fabricated (no wrong stores) and the
// TU is BLOCKED rather than shipping guessed deep-offset writes.
// ---------------------------------------------------------------------------
ProfileManager::ProfileManager()
{
}

// ---------------------------------------------------------------------------
// ProfileManager::AttachMessageDisplay  @ 0x824731A0
//   Asserts the pointer is non-null and that no display is already attached,
//   then stores it into mpMessageDisplay (+267304 / member index 66826).
// ---------------------------------------------------------------------------
void ProfileManager::AttachMessageDisplay(ProfileMessageDisplay* lpMessageDisplay)
{
    CGS_ASSERT(lpMessageDisplay != 0, "lpMessageDisplay != NULL");
    CGS_ASSERT(mpMessageDisplay == 0,
               "AttachMessageDisplay: MessageDisplay already attached");

    mpMessageDisplay = lpMessageDisplay;
}

// ---------------------------------------------------------------------------
// ProfileManager::DetachMessageDisplay  @ 0x82473278
//   Asserts the display being detached is the one currently attached, then
//   clears mpMessageDisplay. (The X360 takes the display by reference and
//   compares its address against the stored pointer.)
// ---------------------------------------------------------------------------
void ProfileManager::DetachMessageDisplay(ProfileMessageDisplay& lrMessageDisplay)
{
    CGS_ASSERT(mpMessageDisplay == &lrMessageDisplay,
               "DetachMessageDisplay: MessageDisplay was not attached");

    mpMessageDisplay = 0;
}

// ---------------------------------------------------------------------------
// ProfileManager::HandleMessageChoice  @ 0x82473330
//   If the chosen option index is within range (< muOptionCount), forward it to
//   the registered option handler's first virtual. Out-of-range choices are
//   ignored.
// ---------------------------------------------------------------------------
void ProfileManager::HandleMessageChoice(u32 luChoice)
{
    if (luChoice < muOptionCount)
    {
        CGS_ASSERT(mpOptionHandler != 0, "mpOptionHandler != 0");
        mpOptionHandler->HandleOption(luChoice);
    }
}

// ---------------------------------------------------------------------------
// ProfileManager::SetProgressionProfile  @ 0x824ECC98
//   Stores the progression profile + its data pointer (members at +267312 /
//   +267316, indices 66828 / 66829).
// ---------------------------------------------------------------------------
void ProfileManager::SetProgressionProfile(
    BrnProgression::Profile* lpProgressionProfile,
    const BrnProgression::ProgressionData* lpProgressionData)
{
    CGS_ASSERT(lpProgressionProfile != 0, "lpProgressionProfile != NULL");

    mpProgressionProfile = lpProgressionProfile;
    mpProgressionData    = lpProgressionData;
}

// ---------------------------------------------------------------------------
// ProfileManager::CopyImageToBuffer  @ 0x824EFD40
//   Copies a 9600-byte mugshot picture from the source descriptor into the next
//   slot of the mugshot circular buffer (computing its hash and stamping the
//   'MUGS' magic), builds a copy-image request describing it, and hands it to the
//   save/load system. Returns the save/load system's result.
// ---------------------------------------------------------------------------
s32 ProfileManager::CopyImageToBuffer(const ImageFileInfo* lpSourceImageFileInfo)
{
    CGS_ASSERT(lpSourceImageFileInfo != 0, "lpSourceImageFileInfo != NULL");

    // Build the on-queue request from the source descriptor. The request keeps the
    // source data pointer + field, but overrides the size with the framed request
    // size (9608) and points its buffer field at the circular-buffer slot.
    MugshotSaveBuffer* lpSlot = mpaMugshotCircularBuffer;

    struct CopyImageRequest
    {
        void* mpData;        // v8[0] = source data pointer
        s32   miSize;        // v8[1] = KI_MUGSHOT_REQUEST_SIZE_BYTES (9608)
        void* mpBuffer;      // v8[2] = circular-buffer slot
        s32   miField3;      // v8[3] = source field
    } lRequest;

    lRequest.mpData   = lpSourceImageFileInfo->mpData;
    lRequest.miSize   = KI_MUGSHOT_REQUEST_SIZE_BYTES;
    lRequest.mpBuffer = lpSlot;
    lRequest.miField3 = lpSourceImageFileInfo->miField3;

    CGS_ASSERT(lpSourceImageFileInfo->miSize == KI_MUGSHOT_PICTURE_SIZE_BYTES,
               "lpSourceImageFileInfo->miSize == KI_MUGSHOT_PICTURE_SIZE_BYTES");

    // Copy the picture into the slot, hash it, and stamp the magic number.
    XMemCpy(lpSlot->macBuffer, lpSourceImageFileInfo->mpSource,
            KI_MUGSHOT_PICTURE_SIZE_BYTES);
    lpSlot->muHash = CgsContainers::CgsHash::CalculateHash(
        lpSlot->macBuffer, KI_MUGSHOT_PICTURE_SIZE_BYTES);
    lpSlot->muMagicNumber = KU_CORRECT_MAGIC_NUMBER;

    return GetSaveLoadSystem()->CopyImageToBuffer(&lRequest);
}

} // namespace BrnGui

// ===========================================================================
// LoadProfileUpgradeData  @ 0x825135C8
//
// Drives the "PROFILEUPG" load state machine. The X360 symbol is attributed to
// ProfileManager but the routine operates on a small independent context object
// (state word at +16, an embedded resource-ptr at +20) -- not the ProfileManager
// layout -- and its own diagnostic string names ProgressionManager. Reconstructed
// faithfully against that small context.
//
// Gated on a global "feature available" bitmask check. Stages:
//   0  issue a PROFILEUPG.BIN bundle-load request           -> stage 1
//   1  drain the load-response events                        -> stage 2 (result 0)
//   2  issue the PROFILEUPG resource open/acquire request    -> stage 3
//   3  acquire the resource handle from each response event  -> stage 4 (result 1)
//   4  finished                                              -> result 1
// Returns 1 when the upgrade is complete (or the feature is unavailable), 0 while
// it is still in progress.
// ===========================================================================
namespace BrnGui
{
    // Global feature-availability gate (byte_82FFA7F0 region):
    //   if ( (gxFeatureFlags & gaxFeatureMask[gxFeatureIndex]) == gaxFeatureMask[gxFeatureIndex] )
    extern u32       gxProfileUpgradeFeatureFlags;     // dword_82FFA7F4
    extern const u32 gaxProfileUpgradeFeatureMask[];   // dword_82FFA7F8
    extern u32       gxProfileUpgradeFeatureIndex;     // dword_82FFA86C

    // Independent load context this routine drives.
    struct ProfileUpgradeData
    {
        u8                        mPad0000[16]; // +0 .. +15
        s32                       miLoadStage;  // +16 (0x10) meProfileUpgradeLoadStage
        CgsResource::BaseResourcePtr mResourcePtr; // +20 (0x14) acquired handle
    };

    // The on-queue payload built for the bundle-load request (case 0).
    struct BundleLoadRequest
    {
        void* mpReceiver;   // v23[0] = lpResponse queue
        s32   miCount;      // v23[1] = 1
        // CgsResource::Events::BundleLoaderEvent fields follow (SetFileName target).
        u8    mFileNameAndFlags[256];
    };

    // The on-queue payload built for the resource-acquire request (case 2).
    struct ResourceAcquireRequest
    {
        void*       mpReceiver;  // v21[0] = lpAcquire queue
        s32         miCount;     // v21[1] = 1
        s32         miType;      // v21[2] = 5
        u64         muResourceId; // v22 = HashString("PROFILEUPG") | 0x500000000
    };

    s32 LoadProfileUpgradeData(ProfileUpgradeData* lpData,
                               CgsGui::OutputBuffer* lpOutputBuffer,
                               CgsModule::BaseEventReceiverQueue* lpResponseQueue);
}

s32 BrnGui::LoadProfileUpgradeData(ProfileUpgradeData* lpData,
                                   CgsGui::OutputBuffer* lpOutputBuffer,
                                   CgsModule::BaseEventReceiverQueue* lpResponseQueue)
{
    // Feature gate: if the upgrade feature is not enabled, report "complete".
    const u32 luMask = gaxProfileUpgradeFeatureMask[gxProfileUpgradeFeatureIndex];
    if ((gxProfileUpgradeFeatureFlags & luMask) != luMask)
    {
        return 1;
    }

    switch (lpData->miLoadStage)
    {
        case 0:
        {
            // Issue the PROFILEUPG.BIN bundle-load request.
            BundleLoadRequest lRequest;
            lRequest.mpReceiver = lpResponseQueue;
            lRequest.miCount    = 1;
            reinterpret_cast<CgsResource::Events::BundleLoaderEvent*>(&lRequest)
                ->SetFileName("PROFILEUPG.BIN");

            CgsModule::VariableEventQueue2048* lpQueue = lpOutputBuffer->GetGu();
            lpQueue->AddEvent(&lRequest, 2, 148);

            lpData->miLoadStage = 1;
            return 0;
        }

        case 1:
        {
            // Drain the bundle-load response events, then clear the queue.
            if (lpResponseQueue->miCount == 0)
            {
                return 0;
            }
            if (lpResponseQueue->miCount > 0)
            {
                s32 liCurrent = reinterpret_cast<s32>(lpResponseQueue->mpBase)
                              + lpResponseQueue->miHeadOffset + 8;
                CGS_ASSERT(liCurrent != 0, "lpResponse");
                s32 liNext = liCurrent;
                s32 liType = *reinterpret_cast<s32*>(liCurrent - 8 + 4);
                while (liNext != 0)
                {
                    lpResponseQueue->GetNextEvent(liCurrent, &liNext, &liType);
                    liCurrent = liNext;
                }
            }
            lpResponseQueue->Clear();
            lpData->miLoadStage = 2;
            return 0;
        }

        case 2:
        {
            // Issue the PROFILEUPG resource acquire request.
            ResourceAcquireRequest lRequest;
            lRequest.mpReceiver   = lpResponseQueue;
            lRequest.miCount      = 1;
            lRequest.miType       = 5;
            lRequest.muResourceId =
                static_cast<u64>(static_cast<u32>(
                    CgsResource::ID::HashString(
                        reinterpret_cast<const u8*>("PROFILEUPG"))))
                | 0x500000000ull;

            CgsModule::VariableEventQueue2048* lpQueue = lpOutputBuffer->GetGu();
            lpQueue->AddEvent(&lRequest, 4, 24);

            lpData->miLoadStage = 3;
            return 0;
        }

        case 3:
        {
            // Acquire the resource handle from each response event, then clear.
            if (lpResponseQueue->miCount == 0)
            {
                return 0;
            }
            if (lpResponseQueue->miCount > 0)
            {
                s32 liCurrent = reinterpret_cast<s32>(lpResponseQueue->mpBase)
                              + lpResponseQueue->miHeadOffset + 8;
                CGS_ASSERT(liCurrent != 0, "lpAcquire");
                s32 liNext = liCurrent;
                s32 liType = *reinterpret_cast<s32*>(liCurrent - 8 + 4);
                while (liNext != 0)
                {
                    // The handle sits 24 bytes into the event payload.
                    lpData->mResourcePtr.CreateFromHandle(
                        reinterpret_cast<const void*>(liCurrent + 24));
                    lpResponseQueue->GetNextEvent(liCurrent, &liNext, &liType);
                    liCurrent = liNext;
                }
            }
            lpResponseQueue->Clear();
            lpData->miLoadStage = 4;
            return 1;
        }

        case 4:
            return 1;

        default:
            CGS_ASSERT(false,
                       "ProgressionManager::meProfileUpgradeLoadStage in a weird state");
            return 0;
    }
}
