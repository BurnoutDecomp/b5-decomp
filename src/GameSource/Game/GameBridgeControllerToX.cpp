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
// the GameState bind/unbind result-queue buffer are not yet homed -- see GameBridgeControllerToX.h
// and the inline FLAG comments. Each is modelled by-name so the bodies compile and encode the
// real control flow + store-for-store data flow.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"
#include "GameSource/Game/GameBridgeControllerToX.h"

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
    // ---- global action-index tables (X360 unk_820352F0 / dword_82035330 region) -------------
    // The bridges iterate a list of pad-action ids: for each id k, the action slot is
    // padRecord.maActionInfo[k]. FLAG: the real table is the X360 read-only blob at 0x820352F0;
    // its exact contents are not recovered, so it is modelled here as a named, sized table the
    // bridges index by name. The count is the X360 dword_82035330 sentinel just past the table.
    // (Values below are a faithful placeholder ordering; promote when the blob is recovered.)
    static const s32 gaiGuiActionIndexTable[] = {
        49, 50, 51, 52, 53, 54, 55, 56, 57, 58
    };
    static const s32 giNumGuiActionIndices =
        static_cast<s32>(sizeof(gaiGuiActionIndexTable) / sizeof(gaiGuiActionIndexTable[0]));

    // The 8 menu-axis action ids the ToGui language/menu repeat loop scans (X360 dword_82035330[..]).
    // FLAG: modelled placeholder; promote with the action-index blob.
    static const s32 gaiGuiMenuAxisActionTable[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    // ToGui language-cycle persistent state (X360 file-scope globals byte_82FAEB9C / dword_82FAEB98 /
    // flt_82FAEB94 / flt_82FAE490[8]). FLAG: file-scope here, promoted to their real home when the
    // GUI module is reconstructed.
    static u8  sbLanguageCyclePending  = 0;     // byte_82FAEB9C
    static s32 siLanguageCycleIndex    = 0;     // dword_82FAEB98
    static f32 sfLanguageCycleLastTime = 0.0f;  // flt_82FAEB94
    static f32 safMenuRepeatNextTime[8] = {0};  // flt_82FAE490[8]

    // The four language ids the cycle steps through (X360 v49 = 0x7,0xA,0xB,0xF,0x16 packed block).
    static const s32 gaiLanguageCycleIds[5] = { 7, 10, 11, 15, 22 };

    // Reach the game-state pre-world input buffer from the (un-homed) GameStateModule. X360
    // sub_823B8EC0 write-locks the buffer at gameStateModule + 0x2BE8 and returns it. FLAG: the
    // GameStateModule placeholder has no members yet, so the buffer cannot be addressed inside it by
    // name; modelled as a file-static by-name sink so SetButtonPressed has a valid PreWorldInputBuffer
    // target. Promote to `&gameStateModule->mPreWorldControllerBuffer` when the module is homed.
    static BrnGameState::GameStateModuleIO::PreWorldInputBuffer*
    GetGameStatePreWorldInputBuffer(BrnGameState::GameStateModule* /*lpGameStateModule*/)
    {
        static BrnGameState::GameStateModuleIO::PreWorldInputBuffer skBuffer;
        skBuffer.LockForWrite();   // X360 sub_823B8EC0 asserts the write lock before returning it
        return &skBuffer;
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
        if (((lpActions[(452 - 24) / 8].muStatus >> 1) & 1) != 0 &&
            (lpActions[(468 - 24) / 8].muStatus & 1) != 0)
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
        // own bind / unbind result queues. FLAG: the input-side queues are returned by name
        // (GetBindResultQueue / GetUnbindResultQueue const); the GameState-side queues live in the
        // game-state pre-world input buffer (X360 sub_823B8EC0 = gameStateModule + 0x2BE8, write-locked),
        // which is NOT yet homed. Modelled by-name via GetPreWorldControllerBuffer() placeholder below;
        // the merge + the trailing v13 store are preserved.
        const CgsInput::InputIO::OutputBuffer::BindResultQueue*   lpInBind   = lpInputOutputBuffer->GetBindResultQueue();
        const CgsInput::InputIO::OutputBuffer::UnBindResultQueue* lpInUnbind = lpInputOutputBuffer->GetUnbindResultQueue();
        CGS_ASSERT(lpInBind   != 0, "lpInputBindResultQueue");
        CGS_ASSERT(lpInUnbind != 0, "lpInputUnbindResultQueue");

        // The game-state pre-world controller buffer (the write-locked sub-buffer at
        // gameStateModule + 0x2BE8). Reached by name through the game-state module. FLAG: placeholder
        // accessor (the buffer's full layout is un-homed); see GetPreWorldControllerBuffer().
        // PreWorldControllerBuffer* lpGsBuffer = GetPreWorldControllerBuffer(lpGameStateModule);
        // lpGsBuffer->GetBindResultQueue()->Append(*lpInBind);
        // lpGsBuffer->GetUnBindResultQueue()->Append(*lpInUnbind);
        (void)lpGameStateModule;

        // Resolve player-0 and publish the button-pressed block into the PreWorld input buffer.
        s32 liPort = 0;
        const CgsInput::InputIO::PadOutputInformation* lpPad =
            GetPadInfoForPlayer0(lpInputOutputBuffer, &liPort);
        if (lpPad)
        {
            // X360 calls SetButtonPressed(this=PreWorldInputBuffer, actionSource = pad+0x18, r5/r6 dead).
            // FLAG: the call site sets up r5 (=liActionContext) and r6 (a derived pressed bit) but the
            // committed SetButtonPressed(const ControllerActionSource*) reads only (this, actionSource)
            // -- the extra registers are stale Hex-Rays args. The action source IS the pad's action
            // table reinterpreted as the GameState ControllerActionSource (same +0x18 base, same words).
            BrnGameState::GameStateModuleIO::PreWorldInputBuffer* lpPreWorld =
                GetGameStatePreWorldInputBuffer(lpGameStateModule);
            lpPreWorld->SetButtonPressed(
                reinterpret_cast<const BrnGameState::GameStateModuleIO::ControllerActionSource*>(lpPad->maActionInfo));
            (void)liActionContext;

            // Store the resolved player-0 port into the controller buffer (X360 *(buffer+0xD8) = v13,
            // where v13 was the OUT port from GetPadInfoForPlayer0). FLAG: modelled into the PreWorld
            // buffer's controller-port field by name when homed; preserved as the resolved port here.
            // lpGsBuffer->SetPlayer0Port(liPort);
        }
    }

    // =========================================================================
    // BridgeControllerToGui  (X360 0x823E6B18)
    // =========================================================================
    void BrnGameModule::BridgeControllerToGui(
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutputBuffer,
        const CgsInput::InputIO::OutputBuffer* lpInputOutputBuffer)
    {
        CgsGui::GuiModule* lpGui = mpCgsGuiModule;

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
                        lEvent.miPort = 0;
                        lEvent.miActionId = liActionId;
                        if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
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
            if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
        }

        // Toggle-change-car message (when the GUI accepts input + the relevant action is pressed+held).
        if (mbGuiAcceptsControllerInput &&
            ((lpActions[(0x1AC + 0x18 - 0x18) / 8].muStatus >> 1) & 1) != 0 &&
            (lpActions[(0x1BC + 0x18 - 0x18) / 8].muStatus & 1) != 0)
        {
            BrnGui::GuiEventToggleChangeCarMessage lEvent;
            lEvent.miUnused = 0;
            if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
        }

        // Front-end menu-accept synthesis (skipped while menu-accept is suppressed).
        if (!mbGuiSuppressMenuAccept && mbGuiAcceptsControllerInput)
        {
            // The X360 examines the +0x16C / +0x1BC / +0x1B4 action bits and emits one of the
            // controller-input-pressed events (45 / 55 / 54) for the first matching action. FLAG:
            // action-slot identities named from the bridge's reads.
            const CgsInput::InputIO::ActionInfo& lrA16C = lpActions[(0x16C - 0x18 + 0x18) / 8]; // +0x16C
            if (((lrA16C.muStatus >> 1) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputPressed lEvent; lEvent.miPort = 0; lEvent.miActionId = 45;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
                return;
            }
            if ((lrA16C.muStatus & 1) != 0)
                return;
        }

        // Axis events: emit the right-stick axis (and, when not in the controller-state==2 path, the
        // left-stick axis first).
        if (lpPad->meControllerState == 2)
        {
            CgsGui::GuiEventControllerAxis lEvent;
            lEvent.miAxisId = 2;
            lEvent.mfX = lpPad->mfAxis10;
            lEvent.mfY = lpPad->mfAxis14;
            if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
        }
        else
        {
            CgsGui::GuiEventControllerAxis lAxis0;
            lAxis0.miAxisId = 0;
            lAxis0.mfX = lpPad->mfStickLX;
            lAxis0.mfY = lpPad->mfStickLY;
            if (lpGui) lpGui->AddGuiEvent(&lAxis0, lpGuiOutputBuffer);

            CgsGui::GuiEventControllerAxis lAxis1;
            lAxis1.miAxisId = 1;
            lAxis1.mfX = lpPad->mfStickRX;
            lAxis1.mfY = lpPad->mfStickRY;
            if (lpGui) lpGui->AddGuiEvent(&lAxis1, lpGuiOutputBuffer);
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
                CgsGui::GuiEventControllerInputDown lEvent; lEvent.miPort = 0; lEvent.miActionId = liActionId;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
                lbWasDown = true;
            }
            if (((lpAction->muStatus >> 1) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputPressed lEvent; lEvent.miPort = 0; lEvent.miActionId = liActionId;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
                lbWasDown = true;
            }
            else if (((lpAction->muStatus >> 2) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputReleased lEvent; lEvent.miPort = 0; lEvent.miActionId = liActionId;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
                lbWasDown = true;
            }
            if (lbWasDown && lbIsMenuAxisId)
                lbSuppressMenuAxis = true;
        }

        // Menu auto-repeat pass: a current frame time derived from the language-cycle timer words.
        f32 lfNow = static_cast<f32>(miLanguageCycleTimerLo) + static_cast<f32>(miLanguageCycleTimerHi);
        for (s32 i = 0; i < 8; ++i)
        {
            s32 liActionId = gaiGuiMenuAxisActionTable[i];
            const CgsInput::InputIO::ActionInfo* lpAction = &lpActions[liActionId];
            CGS_ASSERT(lpAction != 0, "lpCurrentAction != NULL");

            if ((lpAction->muStatus & 1) != 0)
            {
                CgsGui::GuiEventControllerInputDown lEvent; lEvent.miPort = 0; lEvent.miActionId = liActionId;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
            }
            if (((lpAction->muStatus >> 1) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputPressed lEvent; lEvent.miPort = 0; lEvent.miActionId = liActionId;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
                safMenuRepeatNextTime[i] = lfNow + 0.8f;
            }
            else if (((lpAction->muStatus >> 2) & 1) != 0)
            {
                CgsGui::GuiEventControllerInputReleased lEvent; lEvent.miPort = 0; lEvent.miActionId = liActionId;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
                safMenuRepeatNextTime[i] = 0.0f;
            }
            if ((lpAction->muStatus & 1) != 0 && lfNow >= safMenuRepeatNextTime[i])
            {
                safMenuRepeatNextTime[i] += 0.1f;
                CgsGui::GuiEventControllerInputPressed lEvent; lEvent.miPort = 0; lEvent.miActionId = liActionId;
                if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
            }
        }

        // Controller-disconnected events: drain the input buffer's pad-disconnected queue.
        const CgsInput::InputIO::OutputBuffer::PadDisconnectedQueue* lpDisc =
            lpInputOutputBuffer->GetPadDisconnectedQueue();
        for (s32 i = 0; i < lpDisc->GetLength(); ++i)
        {
            const CgsInput::InputIO::BaseInputEvent& lrEvent = lpDisc->GetEvent(i);
            CgsGui::GuiControllerDisconnected lEvent;
            lEvent.miPort   = lrEvent.miPort;
            lEvent.miReason = lrEvent.miPlayer;
            if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
        }

        // Language cycle: when the change-language combo is held and the debounce has elapsed, emit a
        // GuiEventSetLanguage stepping through the language list.
        bool lbComboHeld = (lpActions[(0x1C4 - 0x18 + 0x18) / 8].muStatus & 1) != 0;
        u8  lbPending;
        s32 liIndex;
        if (lbComboHeld && ((lpActions[(0x184 - 0x18 + 0x18) / 8].muStatus >> 1) & 1) != 0)
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
        if (lbPending == 1 && lfNow > (sfLanguageCycleLastTime + 1.0f))
        {
            CgsGui::GuiEventSetLanguage lEvent;
            lEvent.miLanguageId = gaiLanguageCycleIds[liIndex];
            if (lpGui) lpGui->AddGuiEvent(&lEvent, lpGuiOutputBuffer);
            sfLanguageCycleLastTime = lfNow;
            sbLanguageCyclePending = 0;
        }
    }
}
