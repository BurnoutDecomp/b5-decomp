// ===================================================================================
// BrnGui::CrashNavOptionsData  -- the CrashNav "options" screen data model
//   class:BrnGui::CrashNavOptionsData
//
//   SetFromProfile @ 0x82482A28
//   SetToProfile   @ 0x82482B10
//   OutputEvents   @ 0x82494278
// Reconstructed store-for-store from the X360 asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavOptions.h"
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"

namespace BrnGui
{
    // @ 0x82482A28 -- pull the whole option model out of the persisted profile block.
    // Asserts the profile non-null (non-gating), then copies field by field via the
    // profile getters, in the X360 call order (each getter self-asserts !mbIsLocked).
    void CrashNavOptionsData::SetFromProfile(OptionsDataProfile* lpProfile)
    {
        CGS_ASSERT(lpProfile != nullptr, "lpOptionsData");

        meCameraUserOption = static_cast<s32>(lpProfile->GetCameraFeed());
        meVoipVolume       = static_cast<EOptionsVoipVolumes>(lpProfile->GetVoipVolume());
        meMusicVolume      = static_cast<EOptionsSoundVolumes>(lpProfile->GetMusicVolume());
        meSFXVolume        = static_cast<EOptionsSoundVolumes>(lpProfile->GetSFXVolume());
        mbSixAxisShowtime  = lpProfile->GetSixAxisShowtime();
        mbSixAxisSteering  = lpProfile->GetSixAxisSteering();
        mbForceFeedback    = lpProfile->GetForceFeedback();
        mbDefaultGameCamera = lpProfile->GetDefaultGameCamera();
        mbTips             = lpProfile->GetTips();
    }

    // @ 0x82482B10 -- write the whole option model back into the persisted profile block.
    // Asserts the profile non-null (non-gating), then pushes field by field via the
    // profile setters, in the X360 call order (DefaultGameCamera loaded before Tips).
    void CrashNavOptionsData::SetToProfile(OptionsDataProfile* lpProfile)
    {
        CGS_ASSERT(lpProfile != nullptr, "lpOptionsData");

        lpProfile->SetCameraFeed(
            static_cast<BrnNetwork::BrnNetworkModuleIO::ECameraUserOptions>(meCameraUserOption));
        lpProfile->SetVoipVolume(static_cast<s32>(meVoipVolume));
        lpProfile->SetMusicVolume(static_cast<s32>(meMusicVolume));
        lpProfile->SetSFXVolume(static_cast<s32>(meSFXVolume));
        lpProfile->SetSixAxisShowtime(mbSixAxisShowtime);
        lpProfile->SetSixAxisSteering(mbSixAxisSteering);
        lpProfile->SetForceFeedback(mbForceFeedback);
        lpProfile->SetDefaultGameCamera(mbDefaultGameCamera);
        lpProfile->SetTips(mbTips);
    }

    // @ 0x82494278 -- publish the option model as a burst of GuiEvent records onto the
    // state's large output queue (channel id 40 = GuiEventOut). Store-for-store faithful
    // to the X360: seven records in asm order. meVoipVolume (this+0x04) is deliberately
    // NOT emitted; the audio-volumes record (type 463) is pushed twice with a byte-
    // identical {music,sfx} payload exactly as the X360 does. The default-game-camera
    // value is the RAW PROFILE word at +0x734C (asm: a bare `lwz r11, 0x734C(r28)` --
    // NO getter call, NO lock assert, NO ==1 normalization).
    void CrashNavOptionsData::OutputEvents(OptionsDataProfile* lpProfile,
                                           CgsGui::StateInterface* lpStateInterface)
    {
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpQueue =
            lpStateInterface->GetOutputEventQueue();

        // (1) camera-user-option (type 278) -- payload = meCameraUserOption (this+0x00).
        {
            GuiEventCrashNavCameraUserOption lEvent;
            lEvent.miCameraUserOption = meCameraUserOption;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (2) audio volumes (type 463) -- payload = { meMusicVolume(+0x08), meSFXVolume(+0x0C) }.
        {
            GuiEventCrashNavAudioVolumes lEvent;
            lEvent.miMusicVolume = static_cast<s32>(meMusicVolume);
            lEvent.miSFXVolume   = static_cast<s32>(meSFXVolume);
            lpQueue->AddEvent(&lEvent, 40, 20);

            // (3) the X360 pushes the same {music,sfx} record a second time, byte-identical.
            lpQueue->AddEvent(&lEvent, 40, 20);
        }

        // (4) controller toggles (type 472) -- payload = { showtime(+0x10), steering(+0x11), force(+0x12) }.
        {
            GuiEventCrashNavControllerToggles lEvent;
            lEvent.mbSixAxisShowtime = mbSixAxisShowtime;
            lEvent.mbSixAxisSteering = mbSixAxisSteering;
            lEvent.mbForceFeedback   = mbForceFeedback;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (5) default-game-camera (type 475) -- payload = the RAW PROFILE word at +0x734C.
        // The X360 emits a plain `lwz r11, 0x734C(r28)` here: the raw 32-bit flag word,
        // NOT OptionsDataProfile::GetDefaultGameCamera() (which would assert !mbIsLocked and
        // normalize to `word == 1`). Read the modelled member's word directly to match.
        {
            GuiEventCrashNavDefaultGameCamera lEvent;
            lEvent.miDefaultGameCamera = *reinterpret_cast<const s32*>(
                reinterpret_cast<const u8*>(lpProfile) + 0x734C);   // profile mbDefaultGameCamera word
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (6) tips (type 473) -- payload byte = mbTips (this+0x13).
        {
            GuiEventCrashNavTips lEvent;
            lEvent.mbTips = mbTips;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }

        // (7) commit marker (type 356) -- payload byte 0.
        {
            GuiEventCrashNavCommit lEvent;
            lEvent.mbFlag = 0;
            lpQueue->AddEvent(&lEvent, 40, 16);
        }
    }
}
