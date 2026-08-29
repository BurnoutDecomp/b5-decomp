// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeControllerToX.cpp
//
// The BrnGame::BrnGameModule controller-bridge family. Each per-frame bridge reads
// player-0's pad record (CgsInput::InputIO::PadOutputInformation, via
// CgsInput::InputIO::OutputBuffer::GetPadInfo BY NAME) and publishes the controller
// state into a downstream subsystem's input buffer (Director / World / GameState / GUI),
// reusing the committed input-buffer homes BY NAME.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   GetPadInfoForPlayer0           0x823C0EA0
//   MapActionInfoToDebugController 0x823AA580
//   BridgeControllerToDirector     0x823C0F70
//   BridgeControllerToWorld        0x823CD890
//   BridgeControllerToGameState    0x823CD738  (ledger dest BrnGameStateModuleIO.h;
//                                               co-located here -- it is a BrnGameModule method
//                                               sharing the GetPadInfoForPlayer0 helper. FLAGGED.)
//   BridgeControllerToGui          0x823E6B18
//
// HONEST PLACEHOLDERS (FLAGGED): CgsGui::GuiModule + its GuiEvent payloads, the
// debug-controller / director-controller-info images, the global action-index tables, and
// the GameState bind/unbind result QUEUE SEATS (buffer +0x2BE8 / +0x2C54 / +0x2CC0 -- addresses
// now measured, see the inline block) are not yet homed -- see GameBridgeControllerToX.h and the
// inline FLAG comments. Each is modelled by-name so the bodies compile and encode the
// real control flow + store-for-store data flow.
//
// ⭐ [A2 bridge-fidelity, 2026-08-29] BridgeControllerToGui repaired against 0x823E6B18:
//   (a) the menu-accept early-out is now the console's THREE arms (45, then 55 gated on 54-not-held,
//       then 54) instead of the single 45 arm the tree carried -- see the block comment there;
//   (b) every `(0x1AC + 0x18 - 0x18) / 8` style pseudo-derivation is gone. The console holds
//       r24 = padRecord + 0x18 == &maActionInfo[0] across the whole function, so each displacement
//       is maActionInfo[(disp - 4) / 8].muStatus; the ids are now named constants at the top of the
//       namespace. Re-derived: change-car = 53 pressed + 55 held; language cycle = 56 held + 48
//       pressed (NOT 45 -- s3_input.md's guess). Values are unchanged; the addressing is now real.
//   gaiGuiActionIndexTable / gaiGuiMenuAxisActionTable are untouched recovered console data.
//
// ⭐ [D2 gesture-sink, 2026-08-26] BridgeControllerToGameState's PreWorldInputBuffer is NO LONGER
// a placeholder: it is the module-owned buffer GameStateModule::GetPreWorldInputBuffer() hands
// back, and its ControllerInput +0x45 gesture byte is readable by consumers for the first time.
// The banner on GetGameStatePreWorldInputBuffer below carries the console attestation and
// refutes the "gameStateModule + 0x2BE8" reading this file used to state.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeControllerToX.h"

#include "GameShared/GameClasses/Gui/CgsGuiModule.h"               // CgsGui::GuiModule::AddGuiEvent<T> (the real GUI event sink)

#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h" // CgsInput::InputIO::OutputBuffer / PadOutputInformation / ActionInfo
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"    // CgsInput::KU_NUMBER_OF_PADS
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h" // BrnDirector::DirectorIO::InputBuffer
#include "GameSource/World/BrnWorldModuleIO.h"                    // BrnWorldIO::UpdateInputBuffer
#include "GameSource/GameState/BrnGameStateModuleIO.h"            // BrnGameState::GameStateModuleIO::PreWorldInputBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<13312,16>

#include <cmath>     // std::fabs
#include <cstring>   // std::memcpy

namespace BrnGame
{
    // ---- global action-index tables (X360 unk_820352F0 / dword_82035330 rodata) -------------
    // The bridges iterate a list of pad-action ids: for each id k, the action slot is
    // padRecord.maActionInfo[k]. Values extracted from the X360 rodata (2026-07-10): the scan
    // table is the 16 dwords at 0x820352F0 (the ToGui loop runs from the table base to the
    // menu-axis table's address -- pointer-bounded, so 16 entries), and the menu-axis
    // auto-repeat table is the 8 dwords at 0x82035330 (ids 37..44; BootLegal's menu-next 41 /
    // menu-prev 42 ride this repeat pass).
    static const s32 gaiGuiActionIndexTable[] = {
        49, 46, 50, 51, 52, 53, 54, 55, 56, 57, 45, 47, 48, 21, 59, 58
    };
    static const s32 giNumGuiActionIndices =
        static_cast<s32>(sizeof(gaiGuiActionIndexTable) / sizeof(gaiGuiActionIndexTable[0]));

    // The 8 menu-axis action ids the ToGui language/menu repeat loop scans (X360 dword_82035330).
    static const s32 gaiGuiMenuAxisActionTable[8] = { 37, 38, 39, 40, 41, 42, 43, 44 };

    // ---- the individual action slots BridgeControllerToGui reads directly ---------------------
    // MEASURED, not derived. The X360 keeps r24 = padRecord + 0x18 == &maActionInfo[0] for the whole
    // function (0x823E6C80 `addi r24, r31, 0x18`), so every `lwz r11, <disp>(r24)` is
    // maActionInfo[(disp - 4) / 8].muStatus. Decoded:
    //     0x16C(r24) -> maActionInfo[45].muStatus   (45 = GUI_START)
    //     0x1B4(r24) -> maActionInfo[54].muStatus   (54 = GUI_LSHOULDER)
    //     0x1BC(r24) -> maActionInfo[55].muStatus   (55 = GUI_RSHOULDER)
    //     0x1AC(r24) -> maActionInfo[53].muStatus   (53 = GUI_OPTION2)
    //     0x1C4(r24) -> maActionInfo[56].muStatus   (56 = GUI_LTRIGGER)
    //     0x184(r24) -> maActionInfo[48].muStatus   (48 = GUI_RTHUMB)
    // ⚠️ These replace the old `(0x16C - 0x18 + 0x18) / 8` style expressions in this file, which
    // were arithmetically `disp / 8` -- a no-op dressed as an offset derivation. They landed on the
    // right ids only because integer division happened to truncate to them; they are NOT the
    // console's addressing and must never be copied into a new read.
    // Names per EGameInputActions (DecFIGS GameSource/Input/GameInputActions.h).
    static const s32 KI_GUI_ACTION_START      = 45;  // GUI_START
    static const s32 KI_GUI_ACTION_RTHUMB     = 48;  // GUI_RTHUMB   (language-cycle trigger)
    static const s32 KI_GUI_ACTION_OPTION2    = 53;  // GUI_OPTION2  (change-car combo trigger)
    static const s32 KI_GUI_ACTION_LSHOULDER  = 54;  // GUI_LSHOULDER
    static const s32 KI_GUI_ACTION_RSHOULDER  = 55;  // GUI_RSHOULDER (change-car combo modifier)
    static const s32 KI_GUI_ACTION_LTRIGGER   = 56;  // GUI_LTRIGGER  (language-cycle modifier)

    // ToGui language-cycle persistent state (X360 file-scope globals byte_82FAEB9C / dword_82FAEB98 /
    // flt_82FAEB94 / flt_82FAE490[8]). FLAG: file-scope here, promoted to their real home when the
    // GUI module is reconstructed.
    static u8  sbLanguageCyclePending  = 0;     // byte_82FAEB9C
    static s32 siLanguageCycleIndex    = 0;     // dword_82FAEB98
    static f32 sfLanguageCycleLastTime = 0.0f;  // flt_82FAEB94
    static f32 safMenuRepeatNextTime[8] = {0};  // flt_82FAE490[8]

    // The four language ids the cycle steps through (X360 v49 = 0x7,0xA,0xB,0xF,0x16 packed block).
    static const s32 gaiLanguageCycleIds[5] = { 7, 10, 11, 15, 22 };

    // ========================================================================================
    // ⭐⭐ [D2 gesture-sink] THE SINK IS REAL NOW -- AND THE COMMENT THAT USED TO SIT HERE WAS
    // WRONG ABOUT THE CONSOLE, WHICH IS WHY IT PRESCRIBED AN IMPOSSIBLE FIX.
    //
    // What it said: "X360 sub_823B8EC0 write-locks the buffer at gameStateModule + 0x2BE8 and
    // returns it ... promote to &gameStateModule->mPreWorldControllerBuffer when the module is
    // homed", and until then it handed back a FILE-STATIC throwaway. The throwaway is the whole
    // defect: SetButtonPressed ran every frame and wrote the offline event-start gesture byte
    // (+0x45, ControllerInput::mbRaceModePressed) into an object no consumer can name, so
    // ShouldStartSnapRaceMode could never see the player hold accelerator + brake. (It also
    // LockForWrite()d a static and never unlocked it -- the second call would have asserted
    // "Already locked for write". Nothing ever called this bridge on PC, so that never fired.)
    //
    // ⛔ MEASURED, sub_823B8EC0 IS A METHOD OF THE BUFFER, NOT OF THE MODULE. Its body tests
    // `(*a1 >> 3) & 1` -- the IOBuffer write-lock bit at a1+0 -- fires "Not locked for writing"
    // at BrnGameStateModuleIO.h:144, and returns `a1 + 11240`. So +0x2BE8 is an offset INSIDE
    // PreWorldInputBuffer (its input BIND-result queue), and `a2` in
    // BridgeControllerToGameState @0x823CD738 is the BUFFER ITSELF -- which the SetButtonPressed
    // call site proves outright: `SetButtonPressed(a2, result + 24, ...)` passes a2 straight in
    // as `this` to a PreWorldInputBuffer method.
    //
    // Where the console's buffer comes from: BrnGameModule::DoUpdate_GameStatePreWorld
    // @0x823EE0E8 stages it per frame out of the update IOBufferStack
    // (`CreateIOBuffer<GameStateModuleIO::PreWorldInputBuffer>(stack, &buf, "GameStatePreWorld")`),
    // runs BridgeNetworkToGameState + BridgeControllerToGameState against it, hands it to
    // GameStateModule::PreWorldUpdate, and DestroyIOBuffer()s it at the tail.
    //
    // ⚠️ FLAG (PC bring-up seam) -- THE PC SHAPE, STATED RATHER THAN HIDDEN. Nothing on PC runs
    // DoUpdate_GameStatePreWorld, so no per-frame buffer exists. GameStateModule now owns one
    // (GameStateModule::GetPreWorldInputBuffer -- same TYPE, same Construct, same accessors, same
    // write-lock contract; only the allocation SITE and the lifetime move), and this bridge keeps
    // its `GameStateModule*` parameter and reaches the module's buffer BY NAME. That is the ONE
    // deviation from the console's `(PreWorldInputBuffer*)` parameter, and it is the honest one
    // while the module is the buffer's owner. DELETE-WHEN DoUpdate_GameStatePreWorld lands: the
    // parameter then becomes the staged buffer, exactly as the console's is, and this helper goes.
    //
    // ⓘ NO LOCK IS TAKEN HERE, deliberately -- the console does not take one either. The write
    // lock is the CALLER's (X360 sub_823B7620 == LockBuffersForIO takes it for the whole
    // pre-world staging pass; the PC call site in BrnGameModule::GameMain brackets this bridge
    // with LockForWrite/UnlockForWrite the same way DoUpdate_World brackets its sibling). If a
    // caller forgets, SetButtonPressed's own console assert ("Not locked for writing\n",
    // BrnGameStateModuleIO.h:393) names it.
    // ========================================================================================
    static BrnGameState::GameStateModuleIO::PreWorldInputBuffer*
    GetGameStatePreWorldInputBuffer(BrnGameState::GameStateModule* lpGameStateModule)
    {
        if (lpGameStateModule == 0)
            return 0;
        return lpGameStateModule->GetPreWorldInputBuffer();
    }

    // =========================================================================
    // GetPadInfoForPlayer0  (X360 0x823C0EA0)
    // =========================================================================
    const CgsInput::InputIO::PadOutputInformation* BrnGameModule::GetPadInfoForPlayer0(
        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer, s32* lpOutPort)
    {
        // The input module must be ready (state == 4) AND the player-0 pad must report a usable
        // connection (muConnectionWord == 0). Otherwise return no controller.
        if (miInputModuleState != 4)
        {
            *lpOutPort = 4;
            CGS_ASSERT(static_cast<u32>(miPlayer0ControllerPort) <= CgsInput::KU_NUMBER_OF_PADS,
                       "miPlayer0ControllerPort <= CgsInput::KU_NUMBER_OF_PADS");
            return 0;
        }

        const CgsInput::InputIO::PadOutputInformation* lpPadInfo =
            lpInputOutputBuffer->GetPadInfo(miPlayer0ControllerPort);

        if (lpPadInfo->muConnectionWord != 0)
        {
            *lpOutPort = 4;
            CGS_ASSERT(static_cast<u32>(miPlayer0ControllerPort) <= CgsInput::KU_NUMBER_OF_PADS,
                       "miPlayer0ControllerPort <= CgsInput::KU_NUMBER_OF_PADS");
            return 0;
        }

        *lpOutPort = miPlayer0ControllerPort;
        CGS_ASSERT(static_cast<u32>(miPlayer0ControllerPort) <= CgsInput::KU_NUMBER_OF_PADS,
                   "miPlayer0ControllerPort <= CgsInput::KU_NUMBER_OF_PADS");
        return lpPadInfo;
    }

    // =========================================================================
    // MapActionInfoToDebugController  (X360 0x823AA580)
    // Copies the player's 22 pad ActionInfo slots into the 172-byte debug-controller image:
    // per action i -> value (f32), then three status bools (bit0, bit1, bit2). Store-for-store.
    // =========================================================================
    void BrnGameModule::MapActionInfoToDebugController(
        DebugControllerImage* lpImage, const CgsInput::InputIO::ActionInfo* lpActionInfo)
    {
        // The X360 reads its SOURCE action table starting at lpActionInfo+0x70 -- i.e. ActionInfo
        // SLOT 14 (0x70/sizeof(ActionInfo) = 0x70/8 = 14) -- and copies 22 groups (slots 14..35).
        // The image-write side (value @ +0x00 stride 4, bit0 @ +0x58, bit1 @ +0x6E, bit2 @ +0x84)
        // is unchanged.
        static const s32 KI_SOURCE_SLOT_BASE = 0x70 / static_cast<s32>(sizeof(CgsInput::InputIO::ActionInfo));
        for (s32 i = 0; i < DebugControllerImage::KI_NUM_ACTIONS; ++i)
        {
            const CgsInput::InputIO::ActionInfo& lrAction = lpActionInfo[KI_SOURCE_SLOT_BASE + i];
            lpImage->mafValue[i] = lrAction.mfValue;
            lpImage->mabFlagA[i] = static_cast<u8>(lrAction.muStatus & 1);
            lpImage->mabFlagB[i] = static_cast<u8>((lrAction.muStatus & 2) != 0);
            lpImage->mabFlagC[i] = static_cast<u8>((lrAction.muStatus & 4) != 0);
        }
    }

    // =========================================================================
    // BridgeControllerToDirector  (X360 0x823C0F70)
    // =========================================================================
    void BrnGameModule::BridgeControllerToDirector(
        BrnDirector::DirectorIO::InputBuffer* lpDirectorInput,
        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer)
    {
        s32 liPort = 0;
        const CgsInput::InputIO::PadOutputInformation* lpPad =
            GetPadInfoForPlayer0(lpInputOutputBuffer, &liPort);
        if (!lpPad)
            return;

        // The secondary controller port's pad record supplies the debug-controller action source.
        const CgsInput::InputIO::PadOutputInformation* lpSecondaryPad =
            lpInputOutputBuffer->GetPadInfo(miSecondaryControllerPort);

        const CgsInput::InputIO::ActionInfo* lpActions = lpPad->maActionInfo;

        // Build the 16-byte controller-state flag block (X360 v31) from the pad action bits.
        DirectorControllerInfoImage lImage;
        std::memset(&lImage, 0, sizeof(lImage));

        // FLAG: flag-slot identities named from their source ActionInfo status bits.
        lImage.mabFlags[0]  = static_cast<u8>(((~0u) >> 31) & 0); // placeholder for the cntlzw(connection) bit
        lImage.mabFlags[0]  = static_cast<u8>((lpPad->mbDisconnected == 0) ? 1 : 0); // cntlzw(connWord)&0x20 -> connected
        lImage.mabFlags[1]  = static_cast<u8>((lpActions[7].muStatus & 2) != 0); // +0x54
        lImage.mabFlags[2]  = static_cast<u8>(lpActions[5].muStatus & 1);        // +0x44 bit0
        lImage.mabFlags[3]  = static_cast<u8>((lpActions[5].muStatus & 2) != 0); // +0x44 bit1
        lImage.mabFlags[5]  = 0;
        lImage.mabFlags[7]  = static_cast<u8>((lpActions[2].muStatus & 2) != 0); // +0x2C
        lImage.mabFlags[8]  = static_cast<u8>(lpActions[3].muStatus & 1);        // +0x34 bit0
        lImage.mabFlags[9]  = static_cast<u8>(lpActions[2].mfValue > 0.1f);      // +0x28 throttle

        // Steering-curve latch (X360: meControllerState gate; mfAxis10 timer decay vs Director timer).
        // FLAG: the steering-response curve is a polynomial approximation inlined on X360 (VMX); here
        // the steering source is taken store-for-store and the timer-decay branch is reproduced.
        f32 lfSteeringTimer = lpPad->mfAxis10; // X360 reads/writes a1+10097328 (a game-module timer field)
        if ((lpActions[6].muStatus & 1) != 0 && (lpActions[7].muStatus & 1) == 0)
        {
            // GetTimerStatusInterface()+28 holds a delta the timer is decremented by.
            const void* lpTimer = lpDirectorInput->GetTimerStatusInterface();
            f32 lfDelta = *(reinterpret_cast<const f32*>(static_cast<const u8*>(lpTimer) + 28));
            if (lfSteeringTimer > 0.0f)
                lfSteeringTimer = lfSteeringTimer - lfDelta;
        }
        lImage.mabFlags[4] = static_cast<u8>(lfSteeringTimer <= 0.0f);
        lImage.mabFlags[6] = static_cast<u8>(lpSecondaryPad->maActionInfo[100 / 8].muStatus & 1); // asm *(r26+0x64), r26=secondary maActionInfo base -> slot 12 status

        // Fill the debug-controller image from the player's action slots, then append the 4 axis
        // floats from the secondary pad. The X360 writes v35[39..42] = secondaryPad[0..3] (the
        // secondary pad's first 4 floats -- its stick quad LX/LY/RX/RY) at image byte offsets
        // +0x9C/+0xA0/+0xA4/+0xA8 (39*4 .. 42*4), past the 22 action-value floats, then memcpy 0xAC.
        // asm passes r5 = secondaryPad->maActionInfo (the SECONDARY controller port pad), not the
        // primary -- matching the sibling ToWorld bridge.
        MapActionInfoToDebugController(&lImage.mDebugController, lpSecondaryPad->maActionInfo);
        {
            f32* lpTail = reinterpret_cast<f32*>(
                reinterpret_cast<u8*>(&lImage.mDebugController) + 39 * 4);
            lpTail[0] = lpSecondaryPad->mfStickLX; // v35[39] = *v12      (secondaryPad +0x00)
            lpTail[1] = lpSecondaryPad->mfStickLY; // v35[40] = v12[1]    (secondaryPad +0x04)
            lpTail[2] = lpSecondaryPad->mfStickRX; // v35[41] = v12[2]    (secondaryPad +0x08)
            lpTail[3] = lpSecondaryPad->mfStickRY; // v35[42] = v12[3]    (secondaryPad +0x0C)
        }

        lpDirectorInput->SetControllerInfo(&lImage);
    }

    // =========================================================================
    // BridgeControllerToWorld  (X360 0x823CD890)
    // =========================================================================
    void BrnGameModule::BridgeControllerToWorld(
        BrnWorldIO::UpdateInputBuffer* lpWorldInput,
        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer)
    {
        s32 liPort = 0;
        const CgsInput::InputIO::PadOutputInformation* lpPad =
            GetPadInfoForPlayer0(lpInputOutputBuffer, &liPort);
        if (!lpPad)
            return;

        const CgsInput::InputIO::ActionInfo* lpActions = lpPad->maActionInfo;

        WorldVehicleControlsImage lControls;
        std::memset(&lControls, 0, sizeof(lControls));

        // Steering: |stickLX| run through the response curve (X360 VMX polynomial). FLAG: modelled as
        // the faithful magnitude + sign reproduction (the curve is a monotone shaping fn; store-for-store
        // we preserve sign and pass the magnitude through). When stick is at rest the curve gives 0.
        f32 lfStickX = lpPad->mfStickLX;
        f32 lfSteering;
        bool lbControllerStateNotTwo = (lpPad->meControllerState - 2) != 0;
        if (!lbControllerStateNotTwo)
        {
            lfSteering = lpPad->mfAxis10; // controller-state==2 path uses the raw axis
        }
        else
        {
            f32 lfShaped = std::fabs(lfStickX);  // FLAG: response-curve placeholder (identity magnitude)
            lfSteering = (lfStickX < 0.0f) ? (lfShaped * -1.0f) : lfShaped;
        }

        // Axis block, store-for-store against the asm (controls offsets are var_140-relative):
        //   controls+0x00 = *(pad+0x08)      controls+0x04 = *(pad+0x00) (stickLX)
        //   controls+0x08 = *(pad+0x0C)      controls+0x0C = *(pad+0x04) (stickLY)
        //   controls+0x10..0x1C = 0          (already zeroed by the memset above)
        //   controls+0x20 = *(pad+0x18)      controls+0x24 = *(pad+0x20)  controls+0x28 = *(pad+0x28)
        lControls.mfAxis08  = lpPad->mfStickRX;   // var_140 = *(pad+0x08)
        lControls.mfStickLX = lpPad->mfStickLX;   // var_13C = *(pad+0x00)
        lControls.mfAxis0C  = lpPad->mfStickRY;   // var_138 = *(pad+0x0C)
        lControls.mfStickLY = lpPad->mfStickLY;   // var_134 = *(pad+0x04)
        lControls.mfAxis18  = lpActions[0].mfValue; // var_120 = *(pad+0x18) = maActionInfo[0].mfValue
        lControls.mfAxis20  = lpActions[1].mfValue; // var_11C = *(pad+0x20) = maActionInfo[1].mfValue
        lControls.mfAxis28  = lpActions[2].mfValue; // var_118 = *(pad+0x28) = maActionInfo[2].mfValue

        lControls.mfSteeringCurved = lfSteering;  // var_114 = curve(stickX)  @ +0x2C
        lControls.mfDistance = lpActions[(464 - 24) / 8].mfValue - lpActions[(456 - 24) / 8].mfValue; // var_110 @ +0x30 = action[55].value - action[54].value

        // Eight status bytes @ controls+0x34..+0x3B (X360 var_10C..var_105), each from a pad status
        // bit (the status words live inside maActionInfo at pad+0x34/0x44/0x54/0x5C/0x6C/0x84).
        lControls.mabStatus[0] = static_cast<u8>(lpActions[13].muStatus & 1);          // var_10C = *(pad+0x84) & 1
        lControls.mabStatus[1] = static_cast<u8>((lpActions[5].muStatus >> 1) & 1);    // var_10B = (*(pad+0x44)>>1)&1
        lControls.mabStatus[2] = static_cast<u8>((lpActions[8].muStatus >> 1) & 1);    // var_10A = (*(pad+0x5C)>>1)&1
        lControls.mabStatus[3] = static_cast<u8>(lpActions[7].muStatus & 1);           // var_109 = *(pad+0x54) & 1
        lControls.mabStatus[4] = static_cast<u8>(lpActions[10].muStatus & 1);          // var_108 = *(pad+0x6C) & 1
        lControls.mabStatus[5] = static_cast<u8>(lpActions[3].muStatus & 1);           // var_107 = *(pad+0x34) & 1
        lControls.mabStatus[6] = static_cast<u8>(lpPad->meControllerState == 2);       // var_106 = (controllerState == 2)
        lControls.mabStatus[7] = static_cast<u8>((lpActions[3].muStatus >> 1) & 1);    // var_105 = (*(pad+0x34)>>1)&1

        // ShowtimeIntro override: when the game-state module is in the showtime intro, force the
        // controls and take the steering from GetShowtimeIntroSteering. FLAG: IsInShowtimeIntro /
        // GetShowtimeIntroSteering are GameStateModule methods (un-homed in the minimal layout); the
        // game module embeds mGameStateModule, addressed by name.
        // (Modelled as a guarded no-op until GameStateModule exposes these; the store-for-store
        //  override block is preserved in comments.)
        // if (mGameStateModule.IsInShowtimeIntro()) { ...force controls...; lControls.mfSteeringCurved = mGameStateModule.GetShowtimeIntroSteering(); }

        lpWorldInput->SetPlayerVehicleControls(
            reinterpret_cast<const BrnWorldIO::PlayerVehicleControls*>(&lControls));

        // Camera/replay control events: when the relevant action bits are set, push two events onto
        // the world game-action queue (X360 AddEvent type 3 size 4, then a 144-byte type-97 event).
        // ⚠️ SLOTS RE-DERIVED 2026-08-29 alongside the ToGui repair. The old `(452 - 24) / 8` /
        // `(468 - 24) / 8` expressions are the mfValue-offset formula applied to a muStatus offset:
        // they truncate to 53 and 55 only by accident. The console (r31 = padRecord base here, NOT
        // maActionInfo) reads
        //     0x823CDBDC  lwz r11, 0x1C4(r31) ; extrwi 1,30 -> maActionInfo[53].muStatus PRESSED
        //     0x823CDBEC  lwz r11, 0x1D4(r31) ; clrlwi  31  -> maActionInfo[55].muStatus HELD
        // -- the SAME 53-pressed-while-55-held gesture BridgeControllerToGui turns into
        // GuiEventToggleChangeCarMessage. Values unchanged; the addressing is now real.
        if (((lpActions[KI_GUI_ACTION_OPTION2].muStatus >> 1) & 1) != 0 &&
            (lpActions[KI_GUI_ACTION_RSHOULDER].muStatus & 1) != 0)
        {
            f32 lfZero = 0.0f;
            BrnWorldIO::GameActionQueue* lpQueue = lpWorldInput->GetGameActionQueue();
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lfZero), 3, 4);

            // Second event: a 144-byte payload whose word[32] is sourced from the world entity at
            // +0x3C8 (X360 sub_82310240(a1+6958304)+968). FLAG: that world-entity getter is un-homed;
            // modelled with a zero-filled payload + the flag word the X360 sets (HIWORD(payload[33])=1).
            u32 laPayload[44];
            std::memset(laPayload, 0, sizeof(laPayload));
            laPayload[33] = (1u << 16);  // HIWORD = 1
            // laPayload[32] = mWorldEntity.GetField0x3C8();  // FLAG: un-homed world-entity source
            lpQueue = lpWorldInput->GetGameActionQueue();
            lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(laPayload), 97, 144);
        }

        // Debug-controller image into the world buffer's DebugController member (from the SECONDARY pad).
        const CgsInput::InputIO::PadOutputInformation* lpSecondaryPad =
            lpInputOutputBuffer->GetPadInfo(miSecondaryControllerPort);
        DebugControllerImage lDebugImage;
        std::memset(&lDebugImage, 0, sizeof(lDebugImage));
        MapActionInfoToDebugController(&lDebugImage, lpSecondaryPad->maActionInfo);
        BrnWorldIO::DebugController* lpDebug = lpWorldInput->GetDebugController();
        std::memcpy(lpDebug, &lDebugImage, 172);
    }

    // =========================================================================
    // BridgeControllerToGameState  (X360 0x823CD738)
    // FLAG: ledger dest is GameSource/GameState/BrnGameStateModuleIO.h. Body co-located here with
    // its sibling bridges + the shared GetPadInfoForPlayer0 helper (it IS a BrnGameModule method).
    // =========================================================================
    void BrnGameModule::BridgeControllerToGameState(
        BrnGameState::GameStateModule* lpGameStateModule,
        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer,
        s32 liActionContext)
    {
        // Merge the input module's bind / unbind result queues into the game-state pre-world buffer's
        // own bind / unbind result queues. The input-side queues are returned by name
        // (GetBindResultQueue / GetUnbindResultQueue const).
        const CgsInput::InputIO::OutputBuffer::BindResultQueue*   lpInBind   = lpInputOutputBuffer->GetBindResultQueue();
        const CgsInput::InputIO::OutputBuffer::UnBindResultQueue* lpInUnbind = lpInputOutputBuffer->GetUnbindResultQueue();
        CGS_ASSERT(lpInBind   != 0, "lpInputBindResultQueue");
        CGS_ASSERT(lpInUnbind != 0, "lpInputUnbindResultQueue");

        BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpPreWorld =
            GetGameStatePreWorldInputBuffer(lpGameStateModule);
        if (lpPreWorld == 0)
            return;   // module not Construct()ed yet -- nothing to publish into

        // ---- THE BIND / UNBIND MERGE: STILL PARKED, BUT ITS OFFSETS ARE NO LONGER A MYSTERY ----
        // ⭐ [D2] The three console addresses, READ OFF 0x823CD738 + sub_823B8EC0 rather than
        // guessed. sub_823B8EC0 is a write-locked PreWorldInputBuffer accessor returning
        // `this + 11240`; the bridge derives the other two from it:
        //     v10 = sub_823B8EC0(buffer)             -> buffer + 0x2BE8  == the BIND result queue
        //     v11 = sub_823B8EC0(buffer) + 108       -> buffer + 0x2C54  == the UNBIND result queue
        //     *(sub_823B8EC0(buffer) + 216) = port   -> buffer + 0x2CC0  == the resolved player-0 port
        // All three land inside PreWorldInputBuffer's still-opaque +0x7B0..+0x2CC8 span (the
        // maPadToPlayerStatus seat in BrnGameStateModuleIO.h) -- and +0x2CC0 sitting exactly 8
        // bytes below the ASSERTED mPlayerStatusInterface @ +0x2CC8 is the independent check that
        // these are in the right span, not merely in the right ballpark.
        // ⚠️ NOT WIRED, deliberately: homing them means splitting that padding seat into three
        // typed members -- a BrnGameStateModuleIO.h layout change with its own _AssertLayout work,
        // out of scope for this sink, and the gesture leg reads none of them. The console's two
        // Appends and the port store are preserved verbatim:
        //     BindResultQueue::Append  (buffer + 0x2BE8, lpInBind);
        //     UnBindResultQueue::Append(buffer + 0x2C54, lpInUnbind);
        //     *(buffer + 0x2CC0) = liPort;
        (void)lpInBind;
        (void)lpInUnbind;

        // Resolve player-0 and publish the button-pressed block into the PreWorld input buffer.
        s32 liPort = 0;
        const CgsInput::InputIO::PadOutputInformation* lpPad =
            GetPadInfoForPlayer0(lpInputOutputBuffer, &liPort);
        if (lpPad)
        {
            // ⭐⭐ THE GESTURE SINK. X360 call site:
            //     SetButtonPressed(a2, result + 24, a4, *(result + 924) == 2)
            // `a2` is the PreWorldInputBuffer (see GetGameStatePreWorldInputBuffer's banner);
            // `result + 24` is pad + 0x18 == &PadOutputInformation::maActionInfo[0]; the trailing
            // two are STALE HEX-RAYS ARGS -- the CALLEE at 0x823BA240 is a two-parameter function
            // (`SetButtonPressed(_DWORD *result, int a2)`) whose body touches nothing but the
            // write-lock bit, the null check on a2, and its 21 stores. Verified by reading the
            // callee, not inferred from the call site.
            //
            // The last of those 21 stores is the one this whole leg exists for:
            //     v2[69] = *a2 > 0.25 && *(a2 + 8) > 0.25;        (0x823BA454..0x823BA480)
            // i.e. buffer +0x45 == ControllerInput::mbRaceModePressed == "accelerator AND brake
            // analogue both past quarter travel" -- the offline event-start gesture that
            // GameStateModule::ShouldStartSnapRaceMode @0x82363700 holds for 0.35 s at speed <= 30
            // before StartModeAtLights @0x82396CF8 runs. Source bytes 0 and 8 are action slots 0
            // and 1, which are ACCELERATE and BRAKE: CgsInputPadsPC.cpp's binding table binds
            // exactly those two ids to the triggers, and BridgeControllerToWorld reads
            // maActionInfo[0].mfValue -> mfAcceleration and [1].mfValue -> mfBraking.
            lpPreWorld->SetButtonPressed(
                reinterpret_cast<const BrnGameState::GameStateModuleIO::ControllerActionSource*>(lpPad->maActionInfo));
            (void)liActionContext;

            // The resolved player-0 port store (X360 `*(sub_823B8EC0(buffer) + 216) = v13`, i.e.
            // buffer + 0x2CC0). Parked with the two queue Appends above -- same un-homed seat.
            (void)liPort;
        }
    }

    // =========================================================================
    // BridgeControllerToGui  (X360 0x823E6B18)
    // The GUI sink is the real CgsGui::GuiModule::AddGuiEvent<T> (the X360 calls it on the
    // module embedded at BrnGameModule+7252512; the body never reads `this`, so it is
    // callable without the embed until that object is constructed on PC).
    // =========================================================================
    void BrnGameModule::BridgeControllerToGui(
        CgsGui::CgsGuiModuleIO::InputBuffer* lpGuiInputBuffer,
        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer)
    {
        s32 liActiveUserIndex = 0;
        const CgsInput::InputIO::PadOutputInformation* lpPad =
            GetPadInfoForPlayer0(lpInputOutputBuffer, &liActiveUserIndex);

        if (!lpPad)
        {
            // No player-0 controller: if the input module is ready, scan all pads for a menu-accept
            // press so an unassigned pad can still drive the front-end (X360 GetPadInfo loop).
            if (miInputModuleState == 4)
                return;

            bool lbAccepted = false;
            for (s32 liPort = 0; liPort < 4 && !lbAccepted; ++liPort)
            {
                const CgsInput::InputIO::PadOutputInformation* lpScanPad =
                    lpInputOutputBuffer->GetPadInfo(liPort);
                if (lpScanPad->mbDisconnected != 0)
                    continue;

                for (s32 i = 0; i < giNumGuiActionIndices; ++i)
                {
                    s32 liActionId = gaiGuiActionIndexTable[i];
                    const CgsInput::InputIO::ActionInfo* lpAction = &lpScanPad->maActionInfo[liActionId];
                    CGS_ASSERT(lpAction != 0, "lpCurrentAction != NULL");
                    if (((lpAction->muStatus >> 1) & 1) == 1)
                    {
                        CgsGui::GuiEventControllerInputPressed lEvent;
                        lEvent.miPadId = 0;
                        lEvent.miButtonId = liActionId;
                        CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                        lbAccepted = true;
                        break;
                    }
                }
            }
            return;
        }

        const CgsInput::InputIO::ActionInfo* lpActions = lpPad->maActionInfo;

        // Active-user-index event (-1 unless the pad is in the connected state).
        {
            CgsGui::GuiEventActiveUserIndex lEvent;
            lEvent.miActiveUserIndex = (lpPad->meControllerState != 0) ? liActiveUserIndex : -1;
            CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
        }

        // Toggle-change-car message: 53 GUI_OPTION2 *pressed* while 55 GUI_RSHOULDER is *held*
        // (X360 0x823E6C94 `lwz r11,0x1AC(r24)` + extrwi 1,30 -> bit1, then 0x823E6CA4
        // `lwz r11,0x1BC(r24)` + clrlwi 31 -> bit0).
        if (mbGuiAcceptsControllerInput &&
            ((lpActions[KI_GUI_ACTION_OPTION2].muStatus >> 1) & 1) != 0 &&
            (lpActions[KI_GUI_ACTION_RSHOULDER].muStatus & 1) != 0)
        {
            // The canonical record (BrnGuiDemangledEventTypes.h:277) is the X360-attested
            // id 540 / SIZE 1 shape; the local 12-byte fork this file used to carry is
            // retired (see the note in GameBridgeControllerToX.h). The one payload byte is
            // a marker whose value is not attested, so it is stamped rather than left
            // uninitialised -- the record must not carry stack garbage into the queue.
            BrnGui::GuiEventToggleChangeCarMessage lEvent;
            lEvent.maData[0] = 0;
            CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
        }

        // ---- Front-end menu-accept early-out: THREE arms, in this order --------------------------
        // (skipped while menu-accept is suppressed). Restored 2026-08-29 store-for-store from
        // 0x823E6CE4..0x823E6DC8; the tree previously carried only the 45 arm, which silently
        // dropped the "55 fires only while 54 is NOT held" guard -- the partner of the crash-mode
        // both-bumpers gesture.
        //
        //   0x823E6CE4  lwz 0x16C(r24)  -> 45 ; bit1 -> emit 45, return ; bit0 -> return
        //   0x823E6D2C  lwz 0x1BC(r24)  -> 55 ; bit1 -> if (!(54 bit0)) emit 55 ; return
        //                                     ; bit0 -> return
        //   0x823E6D84  lwz 0x1B4(r24)  -> 54 ; bit1 -> emit 54, return ; bit0 -> return
        //
        // Every one of those exits is a real `b __restgprlr_18` (LABEL_14 / loc_823E7200): once any
        // of 45/55/54 is down or pressed, NOTHING else in this bridge runs this frame -- no axis
        // events, no scan-table pass, no auto-repeat, no language cycle. Reproduced as `return`.
        if (!mbGuiSuppressMenuAccept && mbGuiAcceptsControllerInput)
        {
            const u32 luStatus45 = lpActions[KI_GUI_ACTION_START].muStatus;      // +0x16C(r24)
            if (((luStatus45 >> 1) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputPressed lEvent;
                lEvent.miPadId = 0;
                lEvent.miButtonId = KI_GUI_ACTION_START;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                return;
            }
            if ((luStatus45 & 1) != 0)
                return;

            const u32 luStatus55 = lpActions[KI_GUI_ACTION_RSHOULDER].muStatus;  // +0x1BC(r24)
            if (((luStatus55 >> 1) & 1) != 0)
            {
                // 0x823E6D3C: the 55 press is swallowed outright while 54 is held -- the console
                // does NOT fall through to the 54 arm here, it returns either way.
                if ((lpActions[KI_GUI_ACTION_LSHOULDER].muStatus & 1) == 0)
                {
                    CgsGui::GuiEventControllerInputPressed lEvent;
                    lEvent.miPadId = 0;
                    lEvent.miButtonId = KI_GUI_ACTION_RSHOULDER;
                    CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                }
                return;
            }
            if ((luStatus55 & 1) != 0)
                return;

            const u32 luStatus54 = lpActions[KI_GUI_ACTION_LSHOULDER].muStatus;  // +0x1B4(r24)
            if (((luStatus54 >> 1) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputPressed lEvent;
                lEvent.miPadId = 0;
                lEvent.miButtonId = KI_GUI_ACTION_LSHOULDER;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                return;
            }
            if ((luStatus54 & 1) != 0)
                return;
        }

        // Axis events: emit the right-stick axis (and, when not in the controller-state==2 path, the
        // left-stick axis first).
        if (lpPad->meControllerState == 2)
        {
            CgsGui::GuiEventControllerAxis lEvent;
            lEvent.miAxis = 2;
            lEvent.mfXAxis = lpPad->mfAxis10;
            lEvent.mfYAxis = lpPad->mfAxis14;
            CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
        }
        else
        {
            CgsGui::GuiEventControllerAxis lAxis0;
            lAxis0.miAxis = 0;
            lAxis0.mfXAxis = lpPad->mfStickLX;
            lAxis0.mfYAxis = lpPad->mfStickLY;
            CgsGui::GuiModule::AddGuiEvent(lAxis0, lpGuiInputBuffer);

            CgsGui::GuiEventControllerAxis lAxis1;
            lAxis1.miAxis = 1;
            lAxis1.mfXAxis = lpPad->mfStickRX;
            lAxis1.mfYAxis = lpPad->mfStickRY;
            CgsGui::GuiModule::AddGuiEvent(lAxis1, lpGuiInputBuffer);
        }

        // Per-action down / pressed / released events over the action-index table.
        bool lbSuppressMenuAxis = false;
        for (s32 i = 0; i < giNumGuiActionIndices; ++i)
        {
            s32 liActionId = gaiGuiActionIndexTable[i];
            const CgsInput::InputIO::ActionInfo* lpAction = &lpActions[liActionId];
            CGS_ASSERT(lpAction != 0, "lpCurrentAction != NULL");

            bool lbIsMenuAxisId = (liActionId == 49 || liActionId == 50 ||
                                   liActionId == 51 || liActionId == 52);
            if (lbSuppressMenuAxis && lbIsMenuAxisId)
                continue;

            bool lbWasDown = false;
            if ((lpAction->muStatus & 1) != 0)
            {
                CgsGui::GuiEventControllerInputDown lEvent; lEvent.miPadId = 0; lEvent.miButtonId = liActionId;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                lbWasDown = true;
            }
            if (((lpAction->muStatus >> 1) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputPressed lEvent; lEvent.miPadId = 0; lEvent.miButtonId = liActionId;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                lbWasDown = true;
            }
            else if (((lpAction->muStatus >> 2) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputReleased lEvent; lEvent.miPadId = 0; lEvent.miButtonId = liActionId;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                lbWasDown = true;
            }
            if (lbWasDown && lbIsMenuAxisId)
                lbSuppressMenuAxis = true;
        }

        // Menu auto-repeat pass. The X360 time base is the game module's language-cycle clock
        // pair -- an integer-seconds word widened to double plus a fractional-seconds float
        // (LODWORD/HIDWORD u32->f64 idiom @0x823E6E30) -- compared against the per-action
        // repeat deadlines (0.8s initial delay, 0.1s repeat).
        // X360 keeps this in fp31 as a DOUBLE (0x823E6E30 magic-widen idiom) and compares the
        // f32 deadlines against it; only the STORES narrow to f32. Reproduced that way.
        f64 lfNow = static_cast<f64>(static_cast<u32>(miLanguageCycleTimerLo)) +
                    static_cast<f64>(mfLanguageCycleTimerFrac);
        for (s32 i = 0; i < 8; ++i)
        {
            s32 liActionId = gaiGuiMenuAxisActionTable[i];
            const CgsInput::InputIO::ActionInfo* lpAction = &lpActions[liActionId];
            CGS_ASSERT(lpAction != 0, "lpCurrentAction != NULL");

            if ((lpAction->muStatus & 1) != 0)
            {
                CgsGui::GuiEventControllerInputDown lEvent; lEvent.miPadId = 0; lEvent.miButtonId = liActionId;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
            }
            if (((lpAction->muStatus >> 1) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputPressed lEvent; lEvent.miPadId = 0; lEvent.miButtonId = liActionId;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                safMenuRepeatNextTime[i] = static_cast<f32>(lfNow + 0.80000001);
            }
            else if (((lpAction->muStatus >> 2) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputReleased lEvent; lEvent.miPadId = 0; lEvent.miButtonId = liActionId;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
                safMenuRepeatNextTime[i] = 0.0f;
            }
            if ((lpAction->muStatus & 1) != 0 && lfNow >= safMenuRepeatNextTime[i])
            {
                safMenuRepeatNextTime[i] += 0.1f;
                CgsGui::GuiEventControllerInputPressed lEvent; lEvent.miPadId = 0; lEvent.miButtonId = liActionId;
                CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
            }
        }

        // Controller-disconnected events: drain the input buffer's pad-disconnected queue.
        // Payload order per the X360 store pair (HIDWORD=miPlayer first, LODWORD=miPort second):
        // {miPlayer, miPort} -- matching GuiControllerDisconnected's DWARF member order.
        const CgsInput::InputIO::OutputBuffer::PadDisconnectedQueue* lpDisc =
            lpInputOutputBuffer->GetPadDisconnectedQueue();
        for (s32 i = 0; i < lpDisc->GetLength(); ++i)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpDisc->GetEvent(i);
            CgsGui::GuiControllerDisconnected lEvent;
            lEvent.miPlayer = lrEvent.miPlayer;
            lEvent.miPort   = lrEvent.miPort;
            CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
        }

        // Language cycle: when the change-language combo is held and the debounce has elapsed, emit a
        // GuiEventSetLanguage stepping through the language list.
        // ⚠️ SLOTS RE-DERIVED 2026-08-29 from the asm, not from the old fake arithmetic:
        //   0x823E711C  lwz r11, 0x1C4(r24) ; clrlwi 31  -> maActionInfo[56].muStatus bit0 (HELD)
        //   0x823E7158  lwz r11, 0x184(r24) ; extrwi 1,30 -> maActionInfo[48].muStatus bit1 (PRESSED)
        // i.e. the combo is 56 GUI_LTRIGGER held + 48 GUI_RTHUMB pressed -- NOT 45, as a reading of
        // the old `(0x184 - 0x18 + 0x18) / 8` expression against the pad base would suggest. The old
        // expressions truncated to 56 and 48 anyway, so this is a fidelity/readability repair, not a
        // behaviour change. ⓘ It also means the new 56 map-zoom binding cannot double-fire the
        // language cycle on its own: 48 (pad RTHUMB) must be pressed in the same frame.
        bool lbComboHeld = (lpActions[KI_GUI_ACTION_LTRIGGER].muStatus & 1) != 0;
        u8  lbPending;
        s32 liIndex;
        if (lbComboHeld && ((lpActions[KI_GUI_ACTION_RTHUMB].muStatus >> 1) & 1) != 0)
        {
            sbLanguageCyclePending = 1;
            siLanguageCycleIndex = (siLanguageCycleIndex + 1) % 5;
            liIndex = siLanguageCycleIndex;
            lbPending = 1;
        }
        else
        {
            liIndex = siLanguageCycleIndex;
            lbPending = sbLanguageCyclePending;
        }
        if (lbPending == 1 && lfNow > (static_cast<f64>(sfLanguageCycleLastTime) + 1.0))
        {
            CgsGui::GuiEventSetLanguage lEvent;
            lEvent.meLanguage = static_cast<CgsLanguage::ELanguage>(gaiLanguageCycleIds[liIndex]);
            CgsGui::GuiModule::AddGuiEvent(lEvent, lpGuiInputBuffer);
            sfLanguageCycleLastTime = static_cast<f32>(lfNow);
            sbLanguageCyclePending = 0;
        }
    }
}
