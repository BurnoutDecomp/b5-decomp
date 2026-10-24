// Source-extracted candidate/control harness; generated production bodies live in audio_extracted.inc.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdlib.h>
#include <string>

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"

extern bool gbLogGuiAudioTriggers;
#include "audio_extracted.inc"

bool gbLogGuiAudioTriggers = false;
static int gAssertionCount = 0;

namespace CgsDev
{
namespace Assert
{
    int BeginAssert() { ++gAssertionCount; return 0; }
    int FireAssert(const char*, const char*, int) { ++gAssertionCount; return 0; }
    void* EndAssert() { return 0; }
}
namespace Log { StrStreamBase* gpDebugPrint = 0; }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace
{
    int gChecks = 0;
    int gFailures = 0;

    void Check(bool lbCondition, const char* lpacLabel)
    {
        ++gChecks;
        if (!lbCondition)
        {
            ++gFailures;
            std::printf("FAIL %s\n", lpacLabel);
        }
    }

    using GuiOutputQueue = CgsModule::VariableEventQueue<18432, 16>;
    using GuiInputQueue = CgsGui::GuiEventQueueBase<32768, 16>;
    using SoundQueue = CgsModule::VariableEventQueue<8192, 16>;
    using AudioMessage = CgsSound::Io::Message<BrnGui::GuiAudioTriggerEvent>;

    struct EventView
    {
        s32 miType;
        s32 miSize;
        const CgsModule::Event* mpEvent;
    };

    template <class TQueue>
    EventView First(TQueue& arQueue)
    {
        EventView lView = {};
        lView.miType = arQueue.GetFirstEvent(&lView.mpEvent, &lView.miSize);
        return lView;
    }

    template <class TQueue>
    bool Empty(TQueue& arQueue)
    {
        const EventView lView = First(arQueue);
        return lView.miType < 0 && lView.mpEvent == 0 && lView.miSize == 0;
    }

    void CheckField(const char* lpacField, const std::string& racSource,
                    const char* lpacName)
    {
        const size_t luExpected = (std::min)(static_cast<size_t>(31), racSource.size());
        Check(std::strlen(lpacField) == luExpected, lpacName);
        Check(std::memcmp(lpacField, racSource.data(), luExpected) == 0, lpacName);
        Check(lpacField[31] == 0, lpacName);
    }

    bool ReadAudioMessage(SoundQueue& arQueue, s32 liExpectedPayloadSize,
                          s32 liExpectedAction, const char* lpacComponent,
                          const char* lpacLabel, const char* lpacMovie,
                          const char* lpacScenario)
    {
        const EventView lView = First(arQueue);
        Check(lView.miType == 6, lpacScenario);
        Check(lView.mpEvent != 0, lpacScenario);
        Check(lView.miSize == static_cast<s32>(sizeof(AudioMessage)), lpacScenario);
        Check(static_cast<s32>(sizeof(BrnGui::GuiAudioTriggerEvent)) == liExpectedPayloadSize,
              lpacScenario);
        if (!lView.mpEvent || lView.miSize < static_cast<s32>(sizeof(AudioMessage)))
            return false;

        const AudioMessage* lpMessage =
            reinterpret_cast<const AudioMessage*>(lView.mpEvent);
        Check(lpMessage->GetEventId() == 6, lpacScenario);
        Check(lpMessage->GetStateManagerId() == 0, lpacScenario);
        Check(lpMessage->GetInstanceId() == 0, lpacScenario);
        Check(lpMessage->GetEffectId() == 0, lpacScenario);
        Check(lpMessage->GetEffectType() == CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT,
              lpacScenario);
        Check(lpMessage->mData.meAction == liExpectedAction, lpacScenario);
        Check(std::strcmp(lpMessage->mData.macComponent, lpacComponent) == 0, lpacScenario);
        Check(std::strcmp(lpMessage->mData.macLabel, lpacLabel) == 0, lpacScenario);
        Check(std::strcmp(lpMessage->mData.macMovie, lpacMovie) == 0, lpacScenario);

        const CgsModule::Event* lpNext = 0;
        s32 liNextSize = 0;
        const s32 liNextType = arQueue.GetNextEvent(lView.mpEvent, &lpNext, &liNextSize);
        Check(liNextType < 0 && lpNext == 0 && liNextSize == 0, lpacScenario);
        return true;
    }

    EventView Trigger(BrnGui::GuiModule& arModule, GuiOutputQueue& arOutput,
                      const char* lpacComponent, const char* lpacMovie,
                      const char* lpacAction, const char* lpacLabel)
    {
        arOutput.Clear();
#if defined(AUDIO_MODE_ID_ONLY)
        BrnGui::GuiModule::FlaptSoundTriggerCallbackIdOnly(
            &arModule, lpacComponent, lpacMovie, lpacAction, lpacLabel);
#else
        BrnGui::GuiModule::FlaptSoundTriggerCallback(
            &arModule, lpacComponent, lpacMovie, lpacAction, lpacLabel);
#endif
        return First(arOutput);
    }

#if defined(AUDIO_MODE_CANDIDATE)
    void RunCandidate()
    {
        static_assert(sizeof(BrnGui::GuiAudioTriggerEvent) == 100, "canonical payload size");
        static_assert(alignof(BrnGui::GuiAudioTriggerEvent) == 4, "canonical payload alignment");
        static_assert(offsetof(BrnGui::GuiAudioTriggerEvent, macComponent) == 0, "component offset");
        static_assert(offsetof(BrnGui::GuiAudioTriggerEvent, meAction) == 32, "action offset");
        static_assert(offsetof(BrnGui::GuiAudioTriggerEvent, macLabel) == 36, "label offset");
        static_assert(offsetof(BrnGui::GuiAudioTriggerEvent, macMovie) == 68, "movie offset");
        static_assert(sizeof(BrnGui::GuiAudioTriggerEventRecord40) == 112 &&
                      offsetof(BrnGui::GuiAudioTriggerEventRecord40, mOutEvent) == 12,
                      "state output wrapper is the original {100,457,12}+100 record");
        static_assert(sizeof(AudioMessage) == 116, "sound message 6 is header16 + payload100");

        GuiOutputQueue lModuleOutput;
        lModuleOutput.Construct();
        BrnGui::GuiModule lModule = { &lModuleOutput };
        SoundQueue lSoundQueue;
        lSoundQueue.Construct();
        BrnSound::Module::SoundDecoderProjection lSound;
        lSound.mMessageQueue.Construct();
        AudioFixture::GuiCacheCase201Projection lCache = {};
        lCache.miNumDriveThroughs = 1;
        const u64 luDriveThroughId = 0xAABBCCDDEEFF0011ull;
        std::memcpy(&lCache.maDriveThroughInfo[0].mDriveThruId,
                    &luDriveThroughId, sizeof(luDriveThroughId));

        const char* const laActions[14] = {
            "ON_ENTER", "ON_LEAVE", "ON_FOCUS", "ON_LOSE_FOCUS", "ON_ACCEPT", "ON_CANCEL",
            "ON_TICK", "ON_CHANGE", "ON_UP", "ON_DOWN", "ON_LEFT", "ON_LEFT_SWEEP",
            "ON_RIGHT", "ON_RIGHT_SWEEP"
        };
        for (s32 liAction = 0; liAction < 14; ++liAction)
        {
            const EventView lEvent = Trigger(lModule, lModuleOutput, "UI_COMPONENT",
                "MainMenu", laActions[liAction], "MenuAccept");
            Check(lEvent.miType == 457, "callback raw event id457");
            Check(lEvent.miSize == 100, "callback raw event size100");
            Check(lEvent.mpEvent != 0, "callback event pointer");
            if (lEvent.mpEvent)
            {
                lCache.RecEvent(lEvent.mpEvent, lEvent.miType);
                const BrnGui::GuiAudioTriggerEvent* lpAudio =
                    reinterpret_cast<const BrnGui::GuiAudioTriggerEvent*>(lEvent.mpEvent);
                Check(lpAudio->meAction == liAction, "authored action mapping");
                CheckField(lpAudio->macComponent, "UI_COMPONENT", "component field");
                CheckField(lpAudio->macLabel, "MenuAccept", "label field");
                CheckField(lpAudio->macMovie, "MainMenu", "movie field");
                lSound.mMessageQueue.Clear();
                lSound.ProcessGuiEvent(lEvent.miType, lEvent.mpEvent, lEvent.miSize);
                ReadAudioMessage(lSound.mMessageQueue, 100, liAction, "UI_COMPONENT",
                                 "MenuAccept", "MainMenu", "sound message6 fields");
            }
        }

        const EventView lUnknown = Trigger(lModule, lModuleOutput, "", "", "FUTURE_ACTION", "");
        Check(lUnknown.miType == 457 && lUnknown.miSize == 100, "unknown action retains raw457/100");
        if (lUnknown.mpEvent)
        {
            const BrnGui::GuiAudioTriggerEvent* lpAudio =
                reinterpret_cast<const BrnGui::GuiAudioTriggerEvent*>(lUnknown.mpEvent);
            Check(lpAudio->meAction == 14, "unknown action maps to sentinel14");
            Check(lpAudio->macComponent[0] == 0 && lpAudio->macLabel[0] == 0 &&
                  lpAudio->macMovie[0] == 0, "empty strings remain terminated");
        }

        Check(lCache.miCase201DispatchCount == 0 &&
              !lCache.maDriveThroughInfo[0].mbHidden,
              "audio event457 never enters GuiCache case201");

        const std::string lLongComponent(40, 'C');
        const std::string lThirtyOneLabel(31, 'L');
        const std::string lThirtyTwoMovie(32, 'M');
        const EventView lTruncated = Trigger(lModule, lModuleOutput, lLongComponent.c_str(),
            lThirtyTwoMovie.c_str(), "ON_ACCEPT", lThirtyOneLabel.c_str());
        Check(lTruncated.miType == 457 && lTruncated.miSize == 100, "long-string event shape");
        if (lTruncated.mpEvent)
        {
            const BrnGui::GuiAudioTriggerEvent* lpAudio =
                reinterpret_cast<const BrnGui::GuiAudioTriggerEvent*>(lTruncated.mpEvent);
            CheckField(lpAudio->macComponent, lLongComponent, "31-byte component truncation");
            CheckField(lpAudio->macLabel, lThirtyOneLabel, "31-byte label termination");
            CheckField(lpAudio->macMovie, lThirtyTwoMovie, "32-byte movie truncation");
        }

        CgsGui::StateInterface lState;
        lState.Construct();
        BrnGui::GuiAudioTriggerEvent lStateAudio;
        lStateAudio.Construct(7, "STATE_COMPONENT", "StateCue", "StateMovie");
        lState.OutputGuiEvent<BrnGui::GuiAudioTriggerEvent>(lStateAudio);
        CgsGui::StateOutputDrainProjection lStateDrain;
        lStateDrain.mGuiOutputQueue.Construct();
        lStateDrain.Drain(lState.GetOutputEventQueue());
        const EventView lStateEvent = First(lStateDrain.mGuiOutputQueue);
        Check(lStateEvent.miType == 457 && lStateEvent.miSize == 100,
              "actual ProcessOutEvents staging unwraps state channel40 to raw457/100");
        if (lStateEvent.mpEvent && lStateEvent.miSize == 100)
        {
            const BrnGui::GuiAudioTriggerEvent* lpAudio =
                reinterpret_cast<const BrnGui::GuiAudioTriggerEvent*>(lStateEvent.mpEvent);
            Check(std::memcmp(lpAudio, &lStateAudio, 100) == 0,
                  "state wrapper/channel40 drain preserves all payload fields");
            lCache.RecEvent(lStateEvent.mpEvent, lStateEvent.miType);
            Check(lCache.miCase201DispatchCount == 0, "state inner id457 bypasses cache201 arm");
            lSound.mMessageQueue.Clear();
            lSound.ProcessGuiEvent(lStateEvent.miType, lStateEvent.mpEvent, lStateEvent.miSize);
            ReadAudioMessage(lSound.mMessageQueue, 100, 7, "STATE_COMPONENT", "StateCue",
                             "StateMovie", "state wrapper sound route");
        }

        // The actual inbox append/clear statements preserve caller input before stored output
        // and retire the stored batch once. The output event itself came from the production callback.
        const EventView lInboxSource = Trigger(lModule, lModuleOutput, "INBOX_COMPONENT",
            "InboxMovie", "ON_CHANGE", "InboxCue");
        AudioFixture::QueueHandoff lHandoff(lModuleOutput);
        lHandoff.mStoredGuiEvents.Construct();
        GuiInputQueue lInput;
        lInput.Construct();
        const u8 luCallerMarker = 0xA5;
        lInput.AddEvent(reinterpret_cast<const CgsModule::Event*>(&luCallerMarker), 77, 1);
        lHandoff.StoreOutput();
        lModuleOutput.Clear();
        lHandoff.Drain(&lInput);
        EventView lFirst = First(lInput);
        Check(lFirst.miType == 77 && lFirst.miSize == 1, "caller-owned input remains first");
        const CgsModule::Event* lpSecond = 0;
        s32 liSecondSize = 0;
        s32 liSecondType = -1;
        if (lFirst.mpEvent)
            liSecondType = lInput.GetNextEvent(lFirst.mpEvent, &lpSecond, &liSecondSize);
        Check(liSecondType == 457 && liSecondSize == 100 && lpSecond != 0,
              "stored audio appends after caller input");
        if (lpSecond)
        {
            lSound.mMessageQueue.Clear();
            lSound.ProcessGuiEvent(liSecondType, lpSecond, liSecondSize);
            ReadAudioMessage(lSound.mMessageQueue, 100, 7, "INBOX_COMPONENT", "InboxCue",
                             "InboxMovie", "stored inbox sound route");
        }
        lInput.Clear();
        lHandoff.Drain(&lInput);
        Check(Empty(lInput), "stored event is retired once and does not replay");
        Check(lInboxSource.miType == 457 && lInboxSource.miSize == 100,
              "inbox source remains raw457/100");

        // Exact old PC compatibility remains bounded to {0,201,12}+100.
        alignas(4) u8 lau8Legacy201[112] = {};
        const u32 lauHeader[3] = { 0, 201, 12 };
        std::memcpy(lau8Legacy201, lauHeader, sizeof(lauHeader));
        BrnGui::GuiAudioTriggerEvent lLegacyPayload;
        lLegacyPayload.Construct(7, "LEGACY_COMPONENT", "LegacyCue", "LegacyMovie");
        std::memcpy(lau8Legacy201 + 12, &lLegacyPayload, 100);
        lSound.mMessageQueue.Clear();
        lSound.ProcessGuiEvent(201, reinterpret_cast<const CgsModule::Event*>(lau8Legacy201), 112);
        ReadAudioMessage(lSound.mMessageQueue, 100, 7, "LEGACY_COMPONENT", "LegacyCue",
                         "LegacyMovie", "bounded legacy201 audio compatibility");

        // A real 201/8 HideDriveThru payload is not audio and remains byte-for-byte untouched.
        u64 luHideDriveThruId = 0xAABBCCDDEEFF0011ull;
        GuiOutputQueue lHideInput;
        lHideInput.Construct();
        lHideInput.AddEvent(reinterpret_cast<const CgsModule::Event*>(&luHideDriveThruId),
                            201, static_cast<s32>(sizeof(luHideDriveThruId)));
        EventView lHide = First(lHideInput);
        lCache.RecEvent(lHide.mpEvent, lHide.miType);
        Check(lCache.maDriveThroughInfo[0].mbHidden && lCache.miCase201DispatchCount == 1,
              "GuiCache's actual case201 arm still hides the matching drive-thru");
        lSound.mMessageQueue.Clear();
        lSound.ProcessGuiEvent(lHide.miType, lHide.mpEvent, lHide.miSize);
        Check(Empty(lSound.mMessageQueue), "HideDriveThru201/8 is not converted to sound");
        Check(lHide.mpEvent != 0, "HideDriveThru event remains in input queue");
        if (lHide.mpEvent)
            Check(lHide.miType == 201 && lHide.miSize == 8 &&
                  std::memcmp(lHide.mpEvent, &luHideDriveThruId, 8) == 0,
                  "HideDriveThru201/8 bytes are unchanged");
    }
#endif

#if defined(AUDIO_MODE_BASELINE)
    void RunBaselineNegative()
    {
        GuiOutputQueue lOutput; lOutput.Construct();
        BrnGui::GuiModule lModule = { &lOutput };
        const EventView lEvent = Trigger(lModule, lOutput, "BASE_COMPONENT", "BaseMovie",
                                         "ON_ENTER", "BaseCue");
        Check(lEvent.miType == 457 && lEvent.miSize == 100,
              "baseline must produce canonical raw457/100");
        AudioFixture::GuiCacheCase201Projection lCache = {};
        lCache.miNumDriveThroughs = 0;
        if (lEvent.mpEvent) lCache.RecEvent(lEvent.mpEvent, lEvent.miType);
        const bool lbCase201WouldRun = lCache.miCase201DispatchCount != 0;
        Check(!lbCase201WouldRun, "baseline audio must not reach cache case201");
        BrnSound::Module::SoundDecoderProjection lSound; lSound.mMessageQueue.Construct();
        if (lEvent.mpEvent) lSound.ProcessGuiEvent(lEvent.miType, lEvent.mpEvent, lEvent.miSize);
        const EventView lMessage = First(lSound.mMessageQueue);
        Check(lMessage.miSize == 116, "baseline sound message must use canonical size116");
        std::printf("EXPECTED_BASELINE_NEGATIVE id=%d size=%d cache201=%u message=%d assertions=%d\n",
            static_cast<int>(lEvent.miType), static_cast<int>(lEvent.miSize),
            static_cast<unsigned>(lbCase201WouldRun), static_cast<int>(lMessage.miSize),
            gAssertionCount);

    }
#endif

#if defined(AUDIO_MODE_ID_ONLY)
    void RunIdOnlyNegative()
    {
        Check(sizeof(BrnGui::GuiAudioTriggerEvent) == 100,
              "ID-only control keeps the candidate canonical payload type at100 bytes");
        Check(sizeof(AudioMessage) == 116,
              "ID-only control keeps the candidate sound message at116 bytes");
        GuiOutputQueue lOutput; lOutput.Construct();
        BrnGui::GuiModule lModule = { &lOutput };
        const EventView lEvent = Trigger(lModule, lOutput, "IDONLY_COMPONENT", "IDOnlyMovie",
                                         "ON_ENTER", "IDOnlyCue");
        Check(lEvent.miType == 457 && lEvent.miSize == 100,
              "ID-only change must also repair the 112-byte representation");
        AudioFixture::GuiCacheCase201Projection lCache = {};
        lCache.miNumDriveThroughs = 0;
        if (lEvent.mpEvent) lCache.RecEvent(lEvent.mpEvent, lEvent.miType);
        Check(lCache.miCase201DispatchCount == 0, "ID-only id457 does not select cache201");
        BrnSound::Module::SoundDecoderProjection lSound; lSound.mMessageQueue.Construct();
        s32 liMessageSize = 0;
        bool lbMessagePayloadMatches = false;
        if (lEvent.mpEvent)
        {
            lSound.ProcessGuiEvent(lEvent.miType, lEvent.mpEvent, lEvent.miSize);
            const EventView lMessage = First(lSound.mMessageQueue);
            liMessageSize = lMessage.miSize;
            Check(lMessage.miType == 6, "ID-only control uses the candidate SoundLogic message6 arm");
            Check(lMessage.miSize == 116, "ID-only control must use canonical message size116");
            if (lMessage.mpEvent && lMessage.miSize == static_cast<s32>(sizeof(AudioMessage)))
            {
                const AudioMessage* lpMessage =
                    reinterpret_cast<const AudioMessage*>(lMessage.mpEvent);
                lbMessagePayloadMatches = lpMessage->mData.meAction == 0 &&
                    std::strcmp(lpMessage->mData.macComponent, "IDONLY_COMPONENT") == 0 &&
                    std::strcmp(lpMessage->mData.macLabel, "IDOnlyCue") == 0 &&
                    std::strcmp(lpMessage->mData.macMovie, "IDOnlyMovie") == 0;
            }
            Check(!lbMessagePayloadMatches,
                  "ID-only control retains the wrong decoded 112-byte payload layout");
        }
        CgsGui::StateInterface lState; lState.Construct();
        BrnGui::GuiAudioTriggerEvent lAudio;
        lAudio.Construct(7, "IDONLY_COMPONENT", "IDOnlyCue", "IDOnlyMovie");
        lState.OutputGuiEvent<BrnGui::GuiAudioTriggerEvent>(lAudio);
        CgsGui::StateOutputDrainProjection lStateDrain;
        lStateDrain.mGuiOutputQueue.Construct();
        lStateDrain.Drain(lState.GetOutputEventQueue());
        const EventView lStateEvent = First(lStateDrain.mGuiOutputQueue);
        Check(lStateEvent.miType == 457 && lStateEvent.miSize == 100,
              "ID-only control leaves candidate channel-40 specialization intact");

        std::printf("EXPECTED_ID_ONLY_NEGATIVE id=%d size=%d message=%d payload_match=%d assertions=%d\n",
            static_cast<int>(lEvent.miType), static_cast<int>(lEvent.miSize),
            static_cast<int>(liMessageSize), static_cast<int>(lbMessagePayloadMatches),
            gAssertionCount);
    }
#endif
}

int main()
{
    // Check writes through the counters in the unnamed namespace.
#if defined(AUDIO_MODE_CANDIDATE)
    RunCandidate();
    // The runner recognizes the numeric summary and process exit independently.
#elif defined(AUDIO_MODE_BASELINE)
    RunBaselineNegative();
#elif defined(AUDIO_MODE_ID_ONLY)
    RunIdOnlyNegative();
#endif
    Check(gAssertionCount == 0, "no source assertion fired in this fixture mode");
    std::printf("AUDIO_%s: %d checks, %d failures, %d assertions\n",
#if defined(AUDIO_MODE_CANDIDATE)
        "CANDIDATE",
#elif defined(AUDIO_MODE_BASELINE)
        "BASELINE",
#else
        "ID_ONLY",
#endif
        gChecks, gFailures, gAssertionCount);
    return gFailures ? 1 : 0;
}
