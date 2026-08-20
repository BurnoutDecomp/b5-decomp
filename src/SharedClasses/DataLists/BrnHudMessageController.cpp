#include "SharedClasses/DataLists/BrnHudMessageController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"  // [gateui r3] AcquireResourceResponse (AddMessages)
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"    // [gateui r3] ResourceHandle (the bind)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // [gateui r3] BrnGui::GuiHudMessage (GetMessage's input)
#include "GameShared/GameClasses/Core/CgsID.h"                           // [gateui r3] CgsIDCompress / CgsIDUnCompress / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                  // [gateui r3] CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // [gateui r3] gpDebugPrint / gxMessageFilterFlags

// BrnResource::HudMessageController -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file SharedClasses/DataLists/BrnHudMessageController.cpp).
//
// Bodied here:
//   HudMessageController::Construct                      @ 0x82676600   (ICF-folded, see below)
//   HudMessageController::AddMessages                    @ 0x8267D580   (cpp:230/234)
//   HudMessageController::GetIndexFromMessageHash        @ 0x8267D4C8   (cpp:144)
//   HudMessageController::GetMessageTimeToWaitFromIndex  @ 0x824FC678
//   HudMessageController::GetMessageAvailabilityBitset   @ 0x824FC770
//   HudMessageController::GetMessageHashFromIndex        @ 0x824FC108
//   HudMessageController::GetMessageParamCount           @ 0x824FC200
//   HudMessageController::GetMessageParamType            @ 0x824FC328
//   HudMessageController::GetNextMessageIdInGroup        @ 0x824FC4C0
//   HudMessageController::GetPreviousMessageIdInGroup    @ 0x824FC5A0
//
// The loaded record fields are named per the full DWARF GuiHudMessageData/Resource layout
// (mppHudMessageData / miHudMessageCount / mMessageIdHash / muAvailabilityBitSet / mfTimeToWait
// / meMessageGroup / maiParamCount / maaeParams). The X360 re-derefs the resource pointer
// (ResourcePtr<GuiHudMessageResource>::operator->) on every count check + record fetch.

namespace BrnResource
{

// [gateui r3] @ 0x82676600 -- the whole body is `*(u8*)this = 0`, i.e. mbMessagesUsed = false.
// There is no distinct `HudMessageController::Construct` symbol in the ARTIST image: the
// compiler ICF-FOLDED it onto CgsResource::ResourceIO::FileSystemStatusInterface::Construct
// (same three instructions, `li r11,0 / stb r11,0(r3) / blr`). The identification is not a
// guess -- BrnResource::GameDataModule::Construct @0x82671B90 calls that folded symbol at
// 0x82671D20 on `this + 0x65950`, and 0x65950 is EXACTLY the object
// GameDataModule::PrepareHudMessages @0x8266CB1C hands to AddMessages
// (`addis r3, r30, 6 / addi r3, r3, 0x5950`). The ResourcePtr half is zeroed one level up by
// the module ctor @0x827E32E4 (`stw r31, 0(this+0x65954)` / `stw r31, 4(...)` == the X360
// 2-word BaseResourcePtr identity); on this host the ResourcePtr's own default ctor owns
// that, so the body below is the console's store list, no more and no less.
void HudMessageController::Construct()
{
    mbMessagesUsed = false;
}

// @ 0x8267D580 -- adopt an acquired GuiHudMessageResource. The console reads the response's
// resource HANDLE pair (`CreateFromHandle(this + 4, response + 24)` -- +0x18/+0x1C on the
// 32-bit target == {mpResourceMemory, mpSourceEntry}), so the bind is spelled by NAME here
// and rides the public ResourcePtr assign-from-handle (which is the same CreateFromHandle).
// Both asserts are the console's own, non-gating, in the console's order: the "already
// loaded" tripwire fires BEFORE the bind (the X360 falls straight through it), and the
// 300-message cap is checked AFTER, by re-dereferencing the freshly bound pointer.
void HudMessageController::AddMessages(const CgsResource::Events::AcquireResourceResponse* lpResource)
{
    CGS_ASSERT(!mbMessagesUsed,
               "Trying to add a message resource when one is already loaded");   // cpp:230

    CgsResource::ResourceHandle lHandle;
    lHandle.mpResourceMemory = lpResource->mpResourceMemory;
    lHandle.mpSourceEntry    = lpResource->mpSourceEntry;
    mMessagesPtr = lHandle;

    CGS_ASSERT(static_cast<u32>(mMessagesPtr->miHudMessageCount) <= KU_MAX_NUMBER_OF_MESSAGES,
               "Too many hud messages loaded : ");                               // cpp:234

    mbMessagesUsed = true;
}

// [gateui r3] @ 0x8267E528 -- FORMAT one queued GuiHudMessage against the loaded table into
// the HudMessageEvent the HUD component actually renders. (Not in the JSON export set --
// recovered from a private copy of the ARTIST .i64 with headless idat.) This was the last
// unresolved external in the HUD-message publication path: InGameMessagesComponent::AddMessage
// @0x8243DC68 calls it, so without it BrnInGameMessagesComponent.cpp cannot be mounted and
// BrnFBurnMainHudState::UpdateRunning case 154 cannot link.
//
// Field map (X360 store-for-store; the source record is CgsGui::GuiHudMessageData, the
// destination BrnResource::HudMessageEvent):
//   out.mHudMessageId          <- data +0x110 mMessageIdHash          (a3+0    <- v15+272)
//   out.macMessageStyle[32]    <- data +0x0C0 macMessageStyle  (SPrintf "%s")  (a3+1044)
//   out.macIcon[32]            <- data +0x0E0 macDefaultIcon   (SPrintf "%s")  (a3+1076)
//   out.miPriority             <- data +0x124 miPriority              (a3+8    <- v15+292)
//   out.miForceRemoveThreshold <- data +0x128 miForceRemoveThreshold  (a3+12   <- v15+296)
//   out.mfDuration             <- data +0x11C mfDuration              (a3+20   <- v15+284)
//   per string i in 0..2:
//     out.maiStringParamCount[i] <- lpInMessage->GetParamCount(i)     (a3+1032+4i)
//     out.maacString[i][64]      <- data maacStringId[i] (SPrintf "%s")(a3+24+64i <- v15+64i)
//     per param p < that count:  lpInMessage->GetParam(&out.maacStringParam[i][p], i, p)
//
// FAITHFUL ODDITY: out.miGroup (+0x10) is NEVER written -- the console leaves whatever the
// caller's stack held. Reproduced (InGameMessagesComponent never reads it).
//
// The two per-string asserts are the console's own diagnostics and are non-gating: a
// parameter-count mismatch between the queued message and the table is logged, not fixed.
bool HudMessageController::GetMessage(const BrnGui::GuiHudMessage* lpInMessage,
                                      HudMessageEvent* lpOutMessage) const
{
    if (!mbMessagesUsed)
    {
        CGS_ASSERT(false, "Message bundle not loaded");   // cpp:60
        return false;
    }

    CGS_ASSERT(lpInMessage != 0,  "Invalid in message");    // cpp:64
    CGS_ASSERT(lpOutMessage != 0, "Invalid out message");   // cpp:65

    s32 liIndex = GetIndexFromMessageHash(lpInMessage->mMessageIdHash);
    if (liIndex == -1)
    {
        // Unknown id: log it by name and fall back to the table's "TestDefault" entry.
        char lacMessageName[24];
        CgsIDUnCompress(lpInMessage->mMessageIdHash, lacMessageName);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Hud Message not found with hash \""
                                       << lacMessageName << "\"\n";
        }

        liIndex = GetIndexFromMessageHash(CgsIDCompress("TestDefault"));
        if (liIndex == -1)
        {
            CGS_ASSERT(false, "Can't find default hud message to display");   // cpp:80
            return false;
        }
    }

    const CgsGui::GuiHudMessageData* lpData = mMessagesPtr->mppHudMessageData[liIndex];

    lpOutMessage->mHudMessageId = lpData->mMessageIdHash;

    // The console stringises the INCOMING id (not the resolved record's) purely so the two
    // per-string asserts below can name the message.
    char lacMessageId[24];
    CgsIDConvertToString(lpInMessage->mMessageIdHash, lacMessageId);

    CgsCore::SPrintf(lpOutMessage->macMessageStyle,
                     HudMessageEvent::KI_FLASH_FRAME_LABEL_LENGTH, "%s", lpData->macMessageStyle);
    CgsCore::SPrintf(lpOutMessage->macIcon,
                     HudMessageEvent::KI_FLASH_FRAME_LABEL_LENGTH, "%s", lpData->macDefaultIcon);

    lpOutMessage->miPriority             = lpData->miPriority;
    lpOutMessage->miForceRemoveThreshold = lpData->miForceRemoveThreshold;
    lpOutMessage->mfDuration             = lpData->mfDuration;

    for (s32 liString = 0; liString < HudMessageEvent::KI_MAX_NUM_STRINGS; ++liString)
    {
        lpOutMessage->maiStringParamCount[liString] = lpInMessage->GetParamCount(liString);

        CgsCore::SPrintf(lpOutMessage->maacString[liString],
                         HudMessageEvent::KI_STRING_ID_LENGTH, "%s",
                         lpData->maacStringId[liString]);

        // Non-gating: `> 8` is the console's own (deliberately loose) sanity bound, twice the
        // real KI_MAX_PARAMS_PER_STRING.
        CGS_ASSERT(lpOutMessage->maiStringParamCount[liString] <= 8,
                   "Invalid parameter count. Was the Gui Message constructed?");   // cpp:113
        CGS_ASSERT(lpInMessage->GetParamCount(liString) == lpData->maiParamCount[liString],
                   "Wrong number of parameters passed in for message");            // cpp:114

        for (s32 liParam = 0; liParam < lpOutMessage->maiStringParamCount[liString]; ++liParam)
        {
            lpInMessage->GetParam(&lpOutMessage->maacStringParam[liString][liParam],
                                  liString, liParam);

            if (lpOutMessage->maacStringParam[liString][liParam].meParamType !=
                    lpData->maaeParams[liString][liParam] &&
                (CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
                CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Incorrect parameter passed to a hud message (" << lacMessageId
                    << "). Got ("
                    << static_cast<s32>(lpOutMessage->maacStringParam[liString][liParam].meParamType)
                    << ") Expected ("
                    << static_cast<s32>(lpData->maaeParams[liString][liParam]) << ")\n";
            }
        }
    }
    return true;
}

// @ 0x8267D4C8 -- linear-scan the loaded message table for the record whose 64-bit
// message-id hash matches; -1 when the bundle is not loaded (non-gating assert,
// cpp:148) or the hash is absent.
s32 HudMessageController::GetIndexFromMessageHash(CgsID lId) const
{
    if (!mbMessagesUsed)
    {
        CGS_ASSERT(false, "Message bundle not loaded");   // cpp:148
        return -1;
    }

    s32 liIndex = 0;   // cpp:152
    if (mMessagesPtr->miHudMessageCount <= 0)
        return -1;

    while (mMessagesPtr->mppHudMessageData[liIndex]->mMessageIdHash != lId)
    {
        ++liIndex;
        if (liIndex >= mMessagesPtr->miHudMessageCount)
            return -1;
    }
    return liIndex;
}

// @ 0x824FC678 -- the inter-message wait time (seconds) for the record at liIndex, loaded with
// `lfs f1,0x120` (mfTimeToWait). Loaded-guard + index-range are non-gating asserts.
f32 HudMessageController::GetMessageTimeToWaitFromIndex(s32 liIndex) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:324
    CGS_ASSERT(liIndex >= 0 && liIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:325

    return mMessagesPtr->mppHudMessageData[liIndex]->mfTimeToWait;
}

// @ 0x824FC770 -- the per-message availability bitset (which game modes/contexts the message is
// allowed in) for the record at liIndex (mAvailabilityBitSet @+0x118). Non-gating guards.
u32 HudMessageController::GetMessageAvailabilityBitset(s32 liIndex) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:341
    CGS_ASSERT(liIndex >= 0 && liIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:342

    return mMessagesPtr->mppHudMessageData[liIndex]->muAvailabilityBitSet;
}

// [gateui r3] DWARF h:148. No standalone X360 symbol: the ARTIST build inlines this accessor
// at every site as the bare `lwz r11, 8(resource)` that GetIndexFromMessageHash @0x8267D4C8
// (0x8267D50C) and AddMessages @0x8267D580 (`result[2] > 300`) both open-code -- i.e. it is
// exactly miHudMessageCount off the bound resource. Bodied here because the header declares
// it (so it is an LNK2019 the day any consumer names it) and because PrepareHudMessages'
// `[UI-gate] hud controller bound msgs=` rung needs the count by name rather than by reaching
// through the private ResourcePtr. Loaded-guard is non-gating, like every sibling above.
s32 HudMessageController::GetMessageLoadedCount() const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");

    return mMessagesPtr->miHudMessageCount;
}

// @ 0x824FC108 -- the 64-bit message-id hash of the record at liIndex (ld r3,0x110 ==
// mMessageIdHash). Loaded-guard + index-range are non-gating.
CgsID HudMessageController::GetMessageHashFromIndex(s32 liIndex) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:203
    CGS_ASSERT(liIndex >= 0 && liIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:204

    return mMessagesPtr->mppHudMessageData[liIndex]->mMessageIdHash;
}

// @ 0x824FC200 -- how many parameters the liString'th string of the record at liMessageIndex takes
// (maiParamCount[liString] @+0x130). Every assert here is non-gating.
s32 HudMessageController::GetMessageParamCount(s32 liMessageIndex, s32 liString) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:220
    CGS_ASSERT(liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS,
               "liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS");   // h:221
    CGS_ASSERT(liMessageIndex >= 0 && liMessageIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:222

    return mMessagesPtr->mppHudMessageData[liMessageIndex]->maiParamCount[liString];
}

// @ 0x824FC328 -- the type of parameter liParam of string liString for the record at
// liMessageIndex (maaeParams[liString][liParam] @+0x13C, flattened row-major). The liParam bound
// reuses the same "Invalid message index" rodata string (reproduced verbatim). Non-gating.
CgsGui::HudMessageParamTypes HudMessageController::GetMessageParamType(
    s32 liMessageIndex, s32 liString, s32 liParam) const
{
    CGS_ASSERT(mbMessagesUsed, "mbMessagesUsed");   // h:241
    CGS_ASSERT(liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS,
               "liString < CgsGui::GuiHudMessageData::KI_MAX_NUM_STRINGS");   // h:242
    CGS_ASSERT(liMessageIndex >= 0 && liMessageIndex < mMessagesPtr->miHudMessageCount,
               "Invalid message index");           // h:243
    CGS_ASSERT(liParam >= 0 && liParam < CgsGui::GuiHudMessageData::KI_MAX_PARAMS_PER_STRING,
               "Invalid message index");           // h:244

    return mMessagesPtr->mppHudMessageData[liMessageIndex]->maaeParams[liString][liParam];
}

// @ 0x824FC4C0 -- find the next message id (wrapping) whose group matches leGroup, starting just
// past liCurrentId. leGroup == E_HUDMESSAGEGROUP_ALL (0) matches immediately (the `&& leGroup`
// short-circuit). If nothing in [liCurrentId+1, count) matches, rescan [0, liCurrentId); on total
// failure return liCurrentId. (record group @+0x12C = meMessageGroup, count @+8 = miHudMessageCount.)
s32 HudMessageController::GetNextMessageIdInGroup(s32 liCurrentId,
                                                 CgsGui::HudMessageGroups leGroup) const
{
    s32 liIndex = liCurrentId + 1;
    if (liIndex < mMessagesPtr->miHudMessageCount)
    {
        while (mMessagesPtr->mppHudMessageData[liIndex]->meMessageGroup != leGroup && leGroup != 0)
        {
            ++liIndex;
            if (liIndex >= mMessagesPtr->miHudMessageCount)
                goto searchFromStart;
        }
        return liIndex;
    }

searchFromStart:
    if (liCurrentId > 0)
    {
        s32 liCount = 0;
        s32 liScan = 0;
        while (mMessagesPtr->mppHudMessageData[liScan]->meMessageGroup != leGroup && leGroup != 0)
        {
            ++liCount;
            ++liScan;
            if (liCount >= liCurrentId)
                return liCurrentId;
        }
        return liCount;
    }
    return liCurrentId;
}

// @ 0x824FC5A0 -- from a starting message id, walk BACKWARD (descending index) to the previous
// loaded record whose group matches leGroup; when leGroup is E_HUDMESSAGEGROUP_ALL (0) any record
// matches. If the descending scan runs off the front it wraps to the end of the table and keeps
// descending toward liCurrentId; returns liCurrentId when no other member of the group exists.
s32 HudMessageController::GetPreviousMessageIdInGroup(s32 liCurrentId, CgsGui::HudMessageGroups leGroup) const
{
    s32 liIndex = liCurrentId - 1;
    if (liIndex >= 0)
    {
        while (mMessagesPtr->mppHudMessageData[liIndex]->meMessageGroup != leGroup
               && leGroup != CgsGui::E_HUDMESSAGEGROUP_ALL)
        {
            --liIndex;
            if (liIndex < 0)
                goto scanFromEnd;
        }
        return liIndex;
    }

scanFromEnd:
    {
        s32 liEndIndex = mMessagesPtr->miHudMessageCount - 1;
        if (liEndIndex <= liCurrentId)
            return liCurrentId;

        while (mMessagesPtr->mppHudMessageData[liEndIndex]->meMessageGroup != leGroup
               && leGroup != CgsGui::E_HUDMESSAGEGROUP_ALL)
        {
            --liEndIndex;
            if (liEndIndex <= liCurrentId)
                return liCurrentId;
        }
        return liEndIndex;
    }
}

}
