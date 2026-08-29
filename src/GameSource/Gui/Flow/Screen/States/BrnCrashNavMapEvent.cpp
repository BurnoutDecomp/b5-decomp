// ===================================================================================
// BrnGui::CrashNavMapEvent -- the CN_MAP_EVENT screen state (the event-creation map),
// plus its keyboard listener's FillString.
//
//   Construct   @0x824B7510  (BrnCrashNavMapEvent.cpp:59)   LANDED
//   OnEnter     @0x824CC6C0  (cpp:76)                       LANDED
//   OnLeave     @0x824CC790  (cpp:232)                      LANDED
//   ClearTracker@0x824BCD38  (cpp:607)                      LANDED
//   CrashNavMapEventKeyboardListener::FillString
//               @0x824B7568  (cpp:703)                      LANDED
//
//   Update      @0x824DDB90  (cpp:118)   ⛔ BLOCKED -- see below
//   HandleSelect@0x824D8F68  (cpp:443)   ⛔ BLOCKED
//   SetTracker  @0x824BFA88  (cpp:526)   ⛔ BLOCKED
//   SetEventData@0x824CC830  (cpp:627)   ⛔ BLOCKED
//   UpdatePanelData          (cpp:664)   ⛔ BLOCKED (no X360 export)
//   HandleControllerInput    (cpp:257)   ⛔ BLOCKED (no X360 export)
//   UpdateEventData          (cpp:590)   ⛔ BLOCKED (no X360 export)
//
// Bodies are read off the raw X360 ARTIST assembly; OnEnter's nine member stores were
// store-walked directly (0x824CC758..0x824CC77C, with r29 == 1 and r30 == 0) rather
// than taken from Hex-Rays.
//
// ⛔⛔ THIS TU IS DELIBERATELY *NOT* MOUNTABLE, AND THAT IS THE HONEST OUTCOME OF THIS
// PASS. Six methods cannot be reconstructed without FABRICATING types the tree does not
// have a home for, so the class's vtable cannot be completed. Naming the blockers
// precisely so the follow-up wave can cost them:
//
//   (a) `GuiCache::PresetRace` is an OPAQUE forward declaration (BrnGuiCache.h:83:
//       "opaque boundary record ... stride 120 -- pointer-only, un-homed element type").
//       SetEventData @0x824CC830 and SetTracker @0x824BFA88 both read its interior:
//       +0x20 (an 8-byte id), +0x30 (u16 landmark indices), +0x50 (a second per-landmark
//       array) and +0x70 (`muNumLandmarks`, asserted >= 2 and <= 16). Homing that
//       element is a BrnGuiCache.h job, not this TU's.
//   (b) The 3088-byte GuiTracker route record. Its shape IS recovered (see
//       GuiTrackerRouteRecord below) but its 64 ENTRIES are not: SetTracker's fill loop
//       @0x824BFB98..0x824BFBE8 writes three fields per 48-byte entry -- a whole-quadword
//       position lane at entry+0x00, a byte 4 at entry+0x0E and a u16 at entry+0x18,
//       sourced from GuiCache::GetLandmarkInfoFromIndex's SatNavIconInfo out-record. No
//       consumer for those three has been read, so the entry stays unnamed. ClearTracker
//       only needs the count word and is therefore landed below.
//   (c) `BrnGuiKeyboard::Show` is not declared anywhere: BrnGuiKeyboard.h carries only
//       Prepare @0x824EACC0 plus a 0x420-byte reserved block. Both HandleSelect's
//       E_CREATE_EVENT_EDIT_MODIFIER arm and Update's keyboard arm call it with four
//       UTF-16 rodata pointers (unk_8206AB64 / unk_8206AB78 / unk_8206AB90) whose bytes
//       this pass could not read.
//   (d) ✅ RETIRED 2026-08-29 (FIX1). `maiEventToObserve` (X360 dword_8206632C) is now
//       DEFINED below from an actual big-endian read of the raw image; the old claim
//       "this repo ships only the ARTIST .i64, not a raw image" was FALSE. The old
//       dispatch-set inference it declined to commit is also confirmed wrong -- see the
//       definition's comment. This blocker no longer holds anything back.
//   (e) Update's own remaining needs: `GuiCache::GetPresetRace` (blocked by (a)), the
//       id-174 record it posts, and the `(*(*this + 52))(this)` UpdatePanelData virtual,
//       for which there is no X360 export to reconstruct from at all.
//
// CONSEQUENCE, AND THE INSTRUCTION THAT GOES WITH IT: do NOT add this file to
// tools/build/build_game_exe.bat, and do NOT delete the CrashNavMapEvent placeholder
// from BrnScreenStatesLinkStubs.{h,cpp} yet. While this TU is unmounted its class
// declaration is seen only by this file, so the "two BrnGui::CrashNavMapEvent
// definitions" ODR hazard stays DORMANT. Mount and stub-delete together, or neither.
//
// ⚠️ NOTHING SHIPS INTO A MOUNTED TU FROM THIS HEADER (corrected 2026-08-29, FIX1).
// An earlier edit this wave had the mounted GameShared/GameClasses/Gui/CgsSaveLoad.cpp
// include BrnCrashNavMapEvent.h to de-fork the keyboard listener. That ACTIVATED the
// dormant hazard above instead of removing one: it put the real CrashNavMapEvent in the
// same image as the stub. CgsSaveLoad.cpp now keeps a minimal, layout-identical
// file-local `CrashNavMapEventKeyboardListener` with a DELETE-WHEN note; its
// KeyboardClosed @0x824C1820 body stays there. Do the listener de-fork at MOUNT time.
//
// LINK-TIME EXTERNALS (reported, not fabricated): CrashNavMap::{Construct, OnEnter,
// OnLeave}, BrnProgression::Race::Construct, GuiCache::GetGuiTracker (header-inline).
// (maiEventToObserve left this list on 2026-08-29 -- it is defined here now; so did
// GuiTracker::RecvEvent @0x82501D28, bodied this pass in BrnGuiTracker.cpp.)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMapEvent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface (out-queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / VariableEventQueue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                          // BrnGui::GuiTracker

#include <cstring>   // std::memset (the tracker record)

namespace BrnGui
{
    namespace
    {
        // The out-queue selector word (40 == OutputGuiEvent), same legend as
        // BrnCrashNavMap_wJ_08.cpp.
        const s32 KI_CHANNEL_GUI_EVENT = 40;

        // The wire id OnEnter posts alongside the crash-nav deactivate: { 1, 141, 12 }
        // ch 40, 16 bytes (`li r11, 0x8D` @0x824CC748). FLAG consumer-named: 141 has no
        // recovered type in the tree, and the record carries a 1-byte payload the console
        // never writes.
        const s32 KI_EVENT_MAP_EVENT_ENTER = 141;   // 0x8D

        // The acknowledgement OnLeave posts: { 1, 533, 12 } ch 40, 16 bytes
        // (`li r11 == 0x215`). Same record CrashNavMapMain's exit arm sends.
        const s32 KI_EVENT_CRASHNAV_DONE = 533;     // 0x215

        // GuiTracker::RecEvent's wire id and record size, both compile-time immediates at
        // the two call sites (`li r5, 0xE8` / `li r6, 0xC10`).
        const s32 KI_EVENT_TRACKER_ROUTE      = 232;    // 0xE8
        const s32 KI_TRACKER_ROUTE_RECORD_SIZE = 3088;  // 0xC10

        // The X360 assert-site file string, verbatim.
        const char KAC_ASSERT_FILE[] =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMapEvent.cpp";

        // ---- the tracker route record -----------------------------------------------
        // The 3088-byte stack record both ClearTracker and SetTracker hand to
        // GuiTracker::RecEvent. MEASURED from SetTracker's frame @0x824BFA88, which is
        // the site that fills it: the record base is sp+0xA0, the fill loop's cursor
        // starts at sp+0xC8 and strides 48, and the three trailing words live at sp+0xCA0
        // / +0xCA4 / +0xCA8 -- i.e. record+0xC00 / +0xC04 / +0xC08. (0xC00 - 0x10) / 0x30
        // == 64 entries exactly, and 0xC10 == 3088 == the size immediate. ClearTracker
        // @0x824BCD38 lays the same record on its own frame (base sp+0x60, count word at
        // sp+0xC60 == base+0xC00) and initialises ONLY the count.
        // FLAG consumer-named, and only partly recovered: the 16-byte head and the entry
        // interior have no reader this pass could point at, so they stay reserved rather
        // than being named. See the TU banner's blocker (b).
        struct GuiTrackerRouteRecord
        {
            u8  maHeadReserved[0x10];        // +0x0000..+0x000F  never written by either site
            u8  maEntries[64 * 0x30];        // +0x0010..+0x0BFF  64 x 48-byte route points
            s32 miNumPoints;                 // +0x0C00  the live point count
            s32 miReserved_C04;              // +0x0C04  SetTracker stores 0
            u8  mbReserved_C08;              // +0x0C08  SetTracker stores 1
            u8  maTailPad[7];                // +0x0C09..+0x0C0F  record-size pad
        };
    }

    const s32 CrashNavMapEvent::miNumEventsObserved = 10;

    // ---------------------------------------------------------------------------------
    // ⭐ FIX1 (2026-08-29). Blocker (d) is RETIRED. The table WAS recoverable: the wave's
    // raw image (scratch/postfx_step9_final/envfix/work/image.bin, file offset ==
    // VA - 0x82000000, BIG-ENDIAN) carries dword_8206632C, and it was read twice --
    // ten big-endian dwords at 0x8206632C:
    //   0000000E 00000010 00000006 00000008 000000CA
    //   000000C7 00000040 000000D5 000000BD 0000008E
    // with the word immediately past the array (0x82066354, == 0x8206632C + 10*4) reading
    // 0x0000000A -- the same 10 both OnEnter @0x824CC6EC and OnLeave @0x824CC7A0 load as
    // `li r5, 0xA`, which corroborates the array's extent from the other side.
    //
    // NOTE the banner's old inference was WRONG, exactly as it warned it might be: the
    // guessed set {6,7,8,64,199,332,334,344,142,189} shares only six ids with the measured
    // table. 332/334/344/7 are NOT members; 14/16/213 are. Values below are the dump, not
    // the guess.
    // ---------------------------------------------------------------------------------
    const s32 CrashNavMapEvent::maiEventToObserve[10] =
    {
         14,   // 0x0E
         16,   // 0x10
          6,   // 0x06
          8,   // 0x08
        202,   // 0xCA
        199,   // 0xC7
         64,   // 0x40
        213,   // 0xD5
        189,   // 0xBD
        142,   // 0x8E
    };

    // =================================================================================
    //  Construct  @0x824B7510  (cpp:59)
    //
    //  Assert the FSM and chain the base. Unlike CrashNavMapMain::Construct there are no
    //  overrides of the base's cold-start values -- the event map keeps the base's road
    //  signs / drive-through choices.
    // =================================================================================
    void CrashNavMapEvent::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "lpFsm");                                   // cpp:61

        CrashNavMap::Construct(liId, lpFsm);
    }

    // =================================================================================
    //  OnEnter  @0x824CC6C0  (cpp:76)
    //
    //  Bring the event map up: chain the base, subscribe to the ten events, deactivate
    //  crash-nav (the pause, with the second payload word set -- the event map's
    //  variant), announce the screen on wire id 141, then reset the whole event-creation
    //  state machine and construct the race the player is about to author.
    // =================================================================================
    void CrashNavMapEvent::OnEnter()
    {
        CrashNavMap::OnEnter();

        // 0x824CC6EC -- `li r5, 0xA`.
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // 0x824CC6F4..0x824CC72C -- { 8, 191, 12, 0, 1 } ch 40, 20 bytes. NOTE the second
        // payload word: CrashNavMapMain posts { 0, 0 } here, this screen posts { 0, 1 }.
        // BrnGuiEventTypeDefs.h records muParam's role as unrecovered and its ctor zeroes
        // it, so the 1 is set explicitly -- it is a real, measured difference between the
        // two map screens, not a default.
        GuiEventActivateCrashNav lDeactivate(false);
        lDeactivate.muParam = 1;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDeactivate), KI_CHANNEL_GUI_EVENT,
            static_cast<s32>(sizeof(lDeactivate)));

        // 0x824CC730..0x824CC754 -- { 1, 141, 12 } ch 40, 16 bytes.
        CgsGui::GuiEvent<KI_EVENT_MAP_EVENT_ENTER> lEnterRecord(1, 12);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lEnterRecord), KI_CHANNEL_GUI_EVENT, 16);

        // 0x824CC758..0x824CC77C -- nine stores with r29 == 1 and r30 == 0, in the
        // console's own order.
        //
        // ⭐ mbShouldUpdateRoute IS NOW ATTESTED. BrnCrashNavMap.h carries it as
        // "DWARF h:236 (X360 slot un-attested; kept in order)". `stb r29, 0x5E50(r31)`
        // here is 24144 == the byte immediately before miSatNavIconsToLoad (+24148), and
        // Update @0x824DDD9C reads that same byte, calls SetTracker and clears it. Slot
        // and role both pinned; the header's note can be upgraded.
        mbShouldUpdateRoute  = true;                       // stb 1, 0x5E50 (+24144)
        meMapState           = E_MAPSTATE_PANEL;           // stw 0, 0x38   (+56)
        mbIsInEvent          = true;                       // stb 1, 0x60D9 (+24793)
        meCreateEventStage   = E_CREATE_EVENT_NONE;        // stw 0, 0x6160 (+24928)
        mbUpdateNewEventInfo = false;                      // stb 0, 0x61E0 (+25056)
        mi8NextEventIndex    = 0;                          // stb 0, 0x61E1 (+25057)
        mpGuiKeyboard        = 0;                          // stw 0, 0x61E4 (+25060)
        mKeyboardListener.mbKeyboardClosed = false;        // stb 0, 0x620C (+25100)
        mKeyboardListener.mbNewData        = false;        // stb 0, 0x620D (+25101)

        // 0x824CC758 loaded r3 = this + 0x6168 (24936) for the tail call.
        mCreatedRace.Construct();
    }

    // =================================================================================
    //  OnLeave  @0x824CC790  (cpp:232)
    //
    //  Release the subscription, ACTIVATE crash-nav again (the unpause -- id 191 with
    //  payload word 0 set, which GameBridgeGUIToX_GameState turns into game event 93),
    //  send the { 1, 533, 12 } acknowledgement, then chain the base teardown. Note this
    //  screen does NOT post a network-suspension record on either side, and does NOT
    //  call CrashNavPanel::StoreSettings -- both are CrashNavMapMain-only.
    // =================================================================================
    void CrashNavMapEvent::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // { 8, 191, 12, 1, 1 } ch 40, 20 bytes -- again with the second payload word set.
        GuiEventActivateCrashNav lActivate(true);
        lActivate.muParam = 1;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lActivate), KI_CHANNEL_GUI_EVENT,
            static_cast<s32>(sizeof(lActivate)));

        // { 1, 533, 12 } ch 40, 16 bytes.
        CgsGui::GuiEvent<KI_EVENT_CRASHNAV_DONE> lDone(1, 12);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDone), KI_CHANNEL_GUI_EVENT, 16);

        CrashNavMap::OnLeave();
    }

    // =================================================================================
    //  ClearTracker  @0x824BCD38  (cpp:607)
    //
    //  Drop whatever route the tracker is drawing: hand it an empty route record. The
    //  assert is the STREAMED flavour (CgsDev::StrStream), which is why Hex-Rays shows
    //  the BasePriorityQueue::Clear / gpcMessageBuffer preamble.
    // =================================================================================
    void CrashNavMapEvent::ClearTracker()
    {
        // The console builds the record on the stack and initialises ONLY the count word
        // (`stw r30(=0), 0xC00(record)`); the remaining 3084 bytes are whatever the frame
        // held. Zeroed here instead: RecEvent reads miNumPoints first and a zero route has
        // no entries to read, so the observable record is identical and the reconstruction
        // does not ship uninitialised stack.
        GuiTrackerRouteRecord lRecord;
        std::memset(&lRecord, 0, sizeof(lRecord));
        lRecord.miNumPoints = 0;

        if (mpGuiCache->GetGuiTracker() == 0)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer,
                                         CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid tracker pointer";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), KAC_ASSERT_FILE, 616);
            CgsDev::Assert::EndAssert();
        }

        // The console re-loads mpGuiCache and derefs the tracker AFTER the assert -- an
        // assert is not a guard here.
        mpGuiCache->GetGuiTracker()->RecEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord),
            KI_EVENT_TRACKER_ROUTE, KI_TRACKER_ROUTE_RECORD_SIZE);
    }

    // =================================================================================
    //  CrashNavMapEventKeyboardListener::FillString  @0x824B7568  (cpp:703)
    //
    //  Consume the dialog result exactly once: assert the dialog really closed, clear the
    //  closed flag, and hand back the buffer only when the close reported new data.
    //  NOTE the console reads mbNewData BEFORE clearing mbKeyboardClosed and compares it
    //  against the literal 1 (`cmplwi r10, 1`), not against zero.
    // =================================================================================
    char* CrashNavMapEventKeyboardListener::FillString()
    {
        CGS_ASSERT(mbKeyboardClosed, "mbKeyboardClosed");                  // cpp:709

        const bool lbNewData = mbNewData;
        mbKeyboardClosed = false;

        if (!lbNewData)
        {
            return 0;
        }
        return macKeyboardString;
    }
}
