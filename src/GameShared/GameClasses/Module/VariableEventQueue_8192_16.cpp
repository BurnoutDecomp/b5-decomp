#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"

// --- CgsSound::Io::Message<T> element-type homes (each InnerT #included so the
//     Message<InnerT> instantiations below are over a complete type). ---
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"              // CgsSound::Io::Message<T>
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"        // CgsSound::Playback::Name
#include "GameShared/GameClasses/Language/CgsSku.h"                 // CgsLanguage::ELanguage
#include "SharedClasses/Progression/BrnTrainingTypes.h"            // BrnProgression::ETrainingType
#include "GameSource/GameState/BrnGameActions.h"                    // BrnProgression::TrophyUnlockData::UnlockType
#include "GameSource/AttribSys/Enums/OnlineVoiceOver.h"            // AttribSys::Enums::OnlineVoiceOver::OnlineVoiceOver
#include "GameSource/Sound/Global/BrnFxEffect.h"                    // BrnSound::Logic::FxMessage_*
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"     // BrnSound::ESoundMessages/FxVolumes/GameModeLostResults/RaceCarIsNowActive
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"          // real home of GuiAudioTriggerEvent / GuiEventRunFsm               // BrnGui GUI audio-event payloads (read-only reuse)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // CgsGui::GuiEventPlayMusicOnMenuStream (read-only reuse)
#include "GameSource/Sound/Module/LogicModule/Brn3DEffectControl.h"      // BrnSound::Logic::Brn3DEffectControl::DrawSphere

// Explicit per-method instantiation of CgsModule::VariableEventQueue<8192, 16>.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (out-of-line per-instantiation emission).
// Shared generic bodies live in CgsVariableEventQueue.h. Per-method so safe siblings
// not attributed to this instance stay un-instantiated.
// Ledger: 12 methods (same as <4096,16>: Construct/Prepare set + accessors, NO Release).

template void   CgsModule::VariableEventQueue<8192, 16>::Construct();
template void   CgsModule::VariableEventQueue<8192, 16>::Clear();
template bool   CgsModule::VariableEventQueue<8192, 16>::Prepare();
template void   CgsModule::VariableEventQueue<8192, 16>::Destruct();
template s32    CgsModule::VariableEventQueue<8192, 16>::GetLength() const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetSizeInBytes() const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetEventPaddingSize(s32) const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetFirstEvent(const CgsModule::Event**, s32*) const;
template s32    CgsModule::VariableEventQueue<8192, 16>::GetNextEvent(const CgsModule::Event*, const CgsModule::Event**, s32*) const;
template bool   CgsModule::VariableEventQueue<8192, 16>::AddEvent(const CgsModule::Event*, s32, s32);
template void   CgsModule::VariableEventQueue<8192, 16>::OutputQueueContents() const;
template char*  CgsModule::VariableEventQueue<8192, 16>::GetFirstWritePointer();
template const char* CgsModule::VariableEventQueue<8192, 16>::GetFirstWritePointer() const;

// ==== VEQ typed-AddEvent<EventT> family (Pass-A re-home finish) ====
// Explicit instantiation of the templated AddEvent<EventT>/Append<SRCBUF,16>/
// AppendSafe<SRCBUF,16> members the X360 emits out-of-line for THIS <8192,16>
// instance. Bodies are the shared generic templates in CgsVariableEventQueue.h
// (typed AddEvent = assert-then-forward with sizeof(EventT); Append = bulk
// memcpy; AppendSafe = per-event GetFirst/Next + AddEventSafe). Element-type
// homes are #included above so each EventT is a complete type.
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<BrnResource::GameDataIO::GetVehicleListRequest>(const BrnResource::GameDataIO::GetVehicleListRequest*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<BrnResource::GameDataIO::GetWheelListRequest>(const BrnResource::GameDataIO::GetWheelListRequest*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::Append<2048, 16>(const CgsModule::VariableEventQueue<2048, 16>&);
template bool CgsModule::VariableEventQueue<8192, 16>::Append<4096, 16>(const CgsModule::VariableEventQueue<4096, 16>&);

// ==== VEQ typed-AddEvent<CgsSound::Io::Message<InnerT>> family ====
// The sound command path posts wrapped messages: AddEvent<Message<InnerT>>(&msg, type)
// forwards to the three-arg AddEvent with liSize == sizeof(Message<InnerT>) (asserts
// constructed, inline @CgsVariableEventQueue.h:688). The X360 emits one out-of-line
// instantiation per InnerT for THIS <8192,16> queue. sizeof(Message<InnerT>) == 16-byte
// MessageHeader + payload, matching each instance's baked record-size arg (read from the
// asm): Message<bool>/<float>/<u32>/<enum>/<FxMessage_*> == 0x14(20),
// Message<unsigned __int64>/<FxVolumes>/<GameModeLostResults>/<GuiEventAudioSettings>/
// <GuiEventAudioTraxPreview> == 0x18(24), Message<RaceCarIsNowActive> == 0x20(32),
// Message<GuiAudioEvent>/<GuiEventAudioTraxUpdate> == 0x28/0x30, Message<GuiAudioTriggerEvent>
// == 0x74(116). Each InnerT home is #included above so Message<InnerT> is a complete type.
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<bool> >(const CgsSound::Io::Message<bool>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<unsigned int> >(const CgsSound::Io::Message<unsigned int>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<float> >(const CgsSound::Io::Message<float>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<unsigned long long> >(const CgsSound::Io::Message<unsigned long long>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnProgression::ETrainingType> >(const CgsSound::Io::Message<BrnProgression::ETrainingType>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<CgsLanguage::ELanguage> >(const CgsSound::Io::Message<CgsLanguage::ELanguage>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<CgsSound::Playback::Name> >(const CgsSound::Io::Message<CgsSound::Playback::Name>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<AttribSys::Enums::OnlineVoiceOver::OnlineVoiceOver> >(const CgsSound::Io::Message<AttribSys::Enums::OnlineVoiceOver::OnlineVoiceOver>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::ESoundMessages> >(const CgsSound::Io::Message<BrnSound::ESoundMessages>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::FxVolumes> >(const CgsSound::Io::Message<BrnSound::FxVolumes>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::GameModeLostResults> >(const CgsSound::Io::Message<BrnSound::GameModeLostResults>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::RaceCarIsNowActive> >(const CgsSound::Io::Message<BrnSound::RaceCarIsNowActive>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::FxMessage_CameraCut> >(const CgsSound::Io::Message<BrnSound::Logic::FxMessage_CameraCut>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::FxMessage_ResetOnTrack> >(const CgsSound::Io::Message<BrnSound::Logic::FxMessage_ResetOnTrack>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::FxMessage_QuitEvent> >(const CgsSound::Io::Message<BrnSound::Logic::FxMessage_QuitEvent>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::FxMessage_StruntJump> >(const CgsSound::Io::Message<BrnSound::Logic::FxMessage_StruntJump>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::FxMessage_StuntSmash> >(const CgsSound::Io::Message<BrnSound::Logic::FxMessage_StuntSmash>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::FxMessage_StuntStunt> >(const CgsSound::Io::Message<BrnSound::Logic::FxMessage_StuntStunt>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnGui::GuiAudioEvent> >(const CgsSound::Io::Message<BrnGui::GuiAudioEvent>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnGui::GuiAudioTriggerEvent> >(const CgsSound::Io::Message<BrnGui::GuiAudioTriggerEvent>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnGui::GuiEventAudioSettings> >(const CgsSound::Io::Message<BrnGui::GuiEventAudioSettings>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnGui::GuiEventAudioTraxPreview> >(const CgsSound::Io::Message<BrnGui::GuiEventAudioTraxPreview>*, s32);
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnGui::GuiEventAudioTraxUpdate> >(const CgsSound::Io::Message<BrnGui::GuiEventAudioTraxUpdate>*, s32);
// Message<GuiEventAudioTraxLastPlayedIndexes> == 0x28(40): 16-byte MessageHeader + 24-byte
// GuiEvent<459> payload (BrnGuiDemangledEventTypes.h:293, id 459 size 24). Matches the baked
// record-size arg in the X360 thunk @0x826DF2B0 (li r6,0x28), posted by
// BrnSound::Module::SoundLogicModule::ProcessGuiEvents.
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnGui::GuiEventAudioTraxLastPlayedIndexes> >(const CgsSound::Io::Message<BrnGui::GuiEventAudioTraxLastPlayedIndexes>*, s32);
// Message<BrnProgression::TrophyUnlockData::UnlockType> == 0x14(20): 16-byte MessageHeader + a
// 4-byte enum payload (E_UNLOCKTYPE_*, home BrnGameActions.h). Matches the baked record-size arg
// in the X360 thunk @0x826DED38 (li r6,0x14), posted by
// BrnSound::Module::SoundLogicModule::ProcessGameActionQueue (the trophy-unlock sound cue).
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnProgression::TrophyUnlockData::UnlockType> >(const CgsSound::Io::Message<BrnProgression::TrophyUnlockData::UnlockType>*, s32);
// Message<BrnSound::Logic::FxMessage_CameraPhoto> == 0x14(20): 16-byte MessageHeader + a 4-byte
// FxMessage payload (empty FxMessage subclass fixing meType == E_CAMERA_PHOTO, home BrnFxEffect.h).
// Matches the baked record-size arg in the X360 thunk @0x826DFC10 (li r6,20), posted by
// BrnSound::Logic::CameraControl::UpdateParams (the photo-mode camera cue).
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::FxMessage_CameraPhoto> >(const CgsSound::Io::Message<BrnSound::Logic::FxMessage_CameraPhoto>*, s32);
// Message<CgsGui::GuiEventPlayMusicOnMenuStream> == 0x18(24): 16-byte MessageHeader + the GuiEvent<23>
// "play menu music stream" payload (home CgsGuiStateInterface.h). Matches the baked record-size arg
// in the X360 thunk @0x826DF058 (li r6,24), posted by BrnSound::Module::SoundLogicModule::ProcessGuiEvents.
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<CgsGui::GuiEventPlayMusicOnMenuStream> >(const CgsSound::Io::Message<CgsGui::GuiEventPlayMusicOnMenuStream>*, s32);
// Message<BrnSound::Logic::Brn3DEffectControl::DrawSphere> == 0x20(32): 16-byte MessageHeader + the
// 16-byte DrawSphere payload (home Brn3DEffectControl.h). Matches the baked record-size arg in the
// X360 typed-AddEvent thunk @0x826DF9B8 (li r6,0x20), posted by the 3-D effect controls
// (Traffic::TrafficEngine::ProcessUpdate, Collision::CollisionControl::UpdateParams,
// Passby::PassbyEffect::ProcessUpdate, Vehicles::Engines::DualGinsuEffect::ProcessUpdate).
static_assert(sizeof(CgsSound::Io::Message<BrnSound::Logic::Brn3DEffectControl::DrawSphere>) == 0x20,
              "Message<Brn3DEffectControl::DrawSphere> must be 32 bytes (X360 AddEvent li r6,0x20)");
template bool CgsModule::VariableEventQueue<8192, 16>::AddEvent<CgsSound::Io::Message<BrnSound::Logic::Brn3DEffectControl::DrawSphere> >(const CgsSound::Io::Message<BrnSound::Logic::Brn3DEffectControl::DrawSphere>*, s32);

// BLOCKED (2, precise -- NOT fabricated): the two remaining Message<InnerT> instances whose
// InnerT is a genuinely un-homed BrnGui payload type. Homing them requires either editing the
// committed GUI header (out of scope for this TU) or forking a BrnGui type into a non-GUI home
// (ODR risk), and the DecFIGS DWARF for both drifts from the X360-attested size:
//   * BrnGui::GuiEventAudioEventIntros  (0x826DF698, X360 payload 24) -- fully defined in the
//     DWARF (GuiEvent<459>) but absent from the committed BrnGuiDemangledEventTypes.h; homing it
//     needs an out-of-scope GUI-header edit (this TU must not fork BrnGui -> ODR risk).
//   * BrnGui::GuiEventAudioTraxPlayOrder (0x826DF440, X360 payload 4) -- the DWARF models it as
//     GuiEvent<457> + ETraxPlayOrderMode == 16 bytes, contradicting the X360 4-byte payload
//     (Message == 0x14). Reconciling needs the GUI header, which this TU must not edit.
// Both are left todo for a GUI-side slice; they do not block the 22 homed instances above.
