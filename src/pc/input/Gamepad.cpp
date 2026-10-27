#include "pc/input/Gamepad.h"
#include "pc/input/KeyboardMapping.h"
#include "pc/input/KeyboardHost.h"
#include "pc/input/DualSenseEffects.h"
#include "pc/input/SteamHaptics.h"
#include "pc/input/FeedbackSettings.h"
#include "pc/input/EngineFeedback.h"
#include "pc/input/BoostFeedback.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <cstdio>

// FLAG PC-platform leaf: standardized SDL3 gamepads at the native PC boundary.
// No SDL window/video loop: the game owns its Win32 window and keyboard. SDL's
// manual gamepad update also performs driver hot-plug detection and motor expiry.
namespace CgsInput::PCGamepad
{
    namespace
    {
        std::mutex gMutex;
        std::atomic<bool> gbProbeRequested{true};
        SDL_Gamepad* gpPad = nullptr;
        SDL_JoystickID guSelectedId = 0;
        Uint64 guLastProbe = 0;
        State gLastState{};
        std::uint32_t guPacket = 0;
        Uint16 guLeft = 0, guRight = 0;
        Uint64 guLastRumble = 0;
        bool gbRumblePlaying = false;
        bool gbEnhancedRumble = true;
        bool gbInitialized = false;
        bool gbInputAllowed = true;
        KeyboardMapping gKeyboardMapping;
        void* gpKeyboardWindow = nullptr;

        std::mutex gFeedbackMutex;
        AdaptiveTriggers::Sample gFeedbackSample;
        AdaptiveTriggers::Sample gObservedFeedbackSample; // input-thread copy for diagnostics
        std::atomic<bool> gbObserveFeedback{false};
        std::atomic<bool> gbFeedbackPhotoMode{false};
        std::uint64_t guFeedbackVersion = 0, guPublishedAt = 0;
        std::uint64_t guReadFeedbackVersion = 0, guLastFeedbackSend = 0, guLastFeedbackLog = 0;
        std::uint64_t guObservedAt = 0, guLastWitnessLog = 0;
        std::array<std::uint64_t, static_cast<unsigned>(AdaptiveTriggers::Cue::Count)> gaFeedbackWitness{};
        AdaptiveTriggers::Cue geObservedLeft = AdaptiveTriggers::Cue::Off, geObservedRight = AdaptiveTriggers::Cue::Off;
        bool gbFeedbackAllowed = true;
        AdaptiveTriggers::Settings gFeedbackSettings;
        AdaptiveTriggers::Controller gFeedbackController;
        AdaptiveTriggers::Packet gLastFeedbackPacket{};
        FeedbackStatus gFeedbackStatus;
        void (*gpFeedbackLogger)(const char*) = nullptr;
        // Steam Controller: last presented effect and last sent tone per side (0 = L2).
        std::array<AdaptiveTriggers::Effect, 2> gaSteamEffect{};
        std::array<AdaptiveTriggers::SteamHaptics::Report, 2> gaSteamTone{};
        std::array<Uint64, 2> gauSteamToneSent{};
        // config.ini watched for [AdaptiveTriggers] edits while the game runs.
        char gacFeedbackIni[260]{};
        std::uint64_t guFeedbackIniStamp = 0;
        Uint64 guLastIniCheck = 0;

        void LogFeedback(const char* lpMessage)
        {
            if (gFeedbackSettings.mbDiagnostics && gpFeedbackLogger) gpFeedbackLogger(lpMessage);
        }

        void ResetFeedbackState()
        {
            gFeedbackController.Reset();
            const std::lock_guard<std::mutex> lLock(gFeedbackMutex);
            guReadFeedbackVersion = guFeedbackVersion;
            gFeedbackSample.muEvents = 0;
        }

        AdaptiveTriggers::RumbleState FeedbackRumble()
        {
            AdaptiveTriggers::RumbleState lRumble;
            lRumble.mbEnhanced = gbEnhancedRumble;
            if (gbInputAllowed && gbRumblePlaying)
            {
                lRumble.muLeft = guLeft;
                lRumble.muRight = guRight;
            }
            return lRumble;
        }

        bool SendSteamStop()
        {
            namespace Steam = AdaptiveTriggers::SteamHaptics;
            gaSteamEffect = {};
            gaSteamTone = {};
            // Stop exactly the configured actuators (never the restart-all id).
            const std::uint8_t lauActuator[2] = {gFeedbackSettings.muSteamLeftActuator, gFeedbackSettings.muSteamRightActuator};
            bool lbSent = true;
            for (unsigned i = 0; i < 2; ++i)
            {
                if (i && lauActuator[1] == lauActuator[0]) break;
                const auto lStop = Steam::Command(lauActuator[i], Steam::E_COMMAND_OFF);
                lbSent = SDL_SendGamepadEffect(gpPad, lStop.mau.data(), lStop.muSize) && lbSent;
            }
            return lbSent;
        }

        void RejectFeedback()
        {
            ++gFeedbackStatus.muFailures;
            char lacMessage[384];
            std::snprintf(lacMessage, sizeof(lacMessage), "[triggers] device=%u output rejected: %.240s\n",
                guSelectedId, SDL_GetError());
            LogFeedback(lacMessage);
            // A failed write may have partially changed the device. Attempt
            // one neutral reset, then stop retrying until device replacement.
            if (gFeedbackStatus.meOutput == FeedbackStatus::SteamController)
                SendSteamStop();
            else
            {
                const auto lClear = AdaptiveTriggers::Encode(AdaptiveTriggers::Output{}, FeedbackRumble());
                SDL_SendGamepadEffect(gpPad, lClear.data(), static_cast<int>(lClear.size()));
            }
            gFeedbackStatus.meSupport = FeedbackStatus::Rejected;
            gbObserveFeedback.store(gFeedbackSettings.mbDiagnostics, std::memory_order_relaxed);
            AdaptiveTriggers::EngineFeedback::SetEnabled(gFeedbackSettings.mbDiagnostics);
            gFeedbackStatus.meLeft = gFeedbackStatus.meRight = AdaptiveTriggers::Cue::Off;
            ResetFeedbackState();
        }

        // Steam Controller: one HID report per changed side. Tones expire on the
        // device, so an unchanged tone is renewed instead of deduplicated forever.
        bool SendSteamFeedback(const AdaptiveTriggers::Output& lrOutput, Uint64 luNow, bool lbImmediate)
        {
            namespace Steam = AdaptiveTriggers::SteamHaptics;
            const auto lTriggers = AdaptiveTriggers::Encode(lrOutput);
            bool lbRenew = false;
            for (unsigned i = 0; i < 2; ++i)
                lbRenew |= gaSteamTone[i].IsTone() && luNow - gauSteamToneSent[i] >= Steam::KU_RENEW_MS;
            if (!lbImmediate && ((lTriggers == gLastFeedbackPacket && !lbRenew) || luNow - guLastFeedbackSend < 20))
                return false;
            const AdaptiveTriggers::Effect* const lapEffect[2] = {&lrOutput.mLeft, &lrOutput.mRight};
            for (unsigned i = 0; i < 2; ++i)
            {
                const auto laReports = Steam::Translate(*lapEffect[i], gaSteamEffect[i],
                    i ? gFeedbackSettings.muSteamRightActuator : gFeedbackSettings.muSteamLeftActuator, gFeedbackSettings);
                for (const auto& lrReport : laReports)
                {
                    if (!lrReport.muSize) continue;
                    if (lrReport.IsTone() && lrReport == gaSteamTone[i] && luNow - gauSteamToneSent[i] < Steam::KU_RENEW_MS)
                        continue;
                    if (!SDL_SendGamepadEffect(gpPad, lrReport.mau.data(), lrReport.muSize))
                    {
                        RejectFeedback();
                        return false;
                    }
                    ++gFeedbackStatus.muPackets;
                    if (lrReport.IsTone())
                    {
                        gaSteamTone[i] = lrReport;
                        gauSteamToneSent[i] = luNow;
                    }
                    else if (lrReport.mau[2] == Steam::E_COMMAND_OFF)
                        gaSteamTone[i] = {};
                }
                gaSteamEffect[i] = *lapEffect[i];
            }
            guLastFeedbackSend = luNow;
            gLastFeedbackPacket = lTriggers; // logical last output, shared with the neutral checks
            return true;
        }

        bool SendFeedback(const AdaptiveTriggers::Output& lrOutput, Uint64 luNow, bool lbImmediate = false)
        {
            if (!gpPad || gFeedbackStatus.meSupport != FeedbackStatus::Supported) return false;
            if (gFeedbackStatus.meOutput == FeedbackStatus::SteamController)
            {
                if (!SendSteamFeedback(lrOutput, luNow, lbImmediate))
                    return gFeedbackStatus.meSupport == FeedbackStatus::Supported;
            }
            else
            {
                const auto lTriggers = AdaptiveTriggers::Encode(lrOutput);
                if (!lbImmediate && (lTriggers == gLastFeedbackPacket || luNow - guLastFeedbackSend < 20)) return true;
                const auto lPacket = AdaptiveTriggers::Encode(lrOutput, FeedbackRumble());
                if (!SDL_SendGamepadEffect(gpPad, lPacket.data(), static_cast<int>(lPacket.size())))
                {
                    RejectFeedback();
                    return false;
                }
                ++gFeedbackStatus.muPackets;
                guLastFeedbackSend = luNow;
                gLastFeedbackPacket = lTriggers; // Motor-only changes stay with SDL_RumbleGamepad.
            }
            if ((lrOutput.mLeft.meCue != gFeedbackStatus.meLeft || lrOutput.mRight.meCue != gFeedbackStatus.meRight) &&
                (lbImmediate || luNow - guLastFeedbackLog >= 250))
            {
                char lacMessage[256];
                std::snprintf(lacMessage, sizeof(lacMessage),
                    "[triggers] device=%u transport=%s left=%s/%u right=%s/%u packets=%llu\n",
                    guSelectedId, gFeedbackStatus.mbWireless ? "wireless" : "wired-or-unknown",
                    AdaptiveTriggers::GetCueName(lrOutput.mLeft.meCue), lrOutput.mLeft.muPriority,
                    AdaptiveTriggers::GetCueName(lrOutput.mRight.meCue), lrOutput.mRight.muPriority,
                    static_cast<unsigned long long>(gFeedbackStatus.muPackets));
                LogFeedback(lacMessage);
                guLastFeedbackLog = luNow;
            }
            gFeedbackStatus.meLeft = lrOutput.mLeft.meCue;
            gFeedbackStatus.meRight = lrOutput.mRight.meCue;
            return true;
        }

        void OpenFeedback()
        {
            gFeedbackStatus = {};
            gFeedbackStatus.muDevice = guSelectedId;
            gFeedbackStatus.mbWireless = SDL_GetGamepadConnectionState(gpPad) == SDL_JOYSTICK_CONNECTION_WIRELESS;
            gFeedbackStatus.meSupport = FeedbackStatus::Unsupported;
            gLastFeedbackPacket = {};
            gaSteamEffect = {};
            gaSteamTone = {};
            ResetFeedbackState();
            const Uint16 luVendor = SDL_GetGamepadVendor(gpPad), luProduct = SDL_GetGamepadProduct(gpPad);
            // Real type ignores user mapping overrides. A PS5-shaped virtual Xbox
            // mapping cannot opt itself into Sony-specific output packets.
            if (SDL_GetRealGamepadType(gpPad) == SDL_GAMEPAD_TYPE_PS5 && luVendor == 0x054c)
            {
                const auto luFirmware = SDL_GetGamepadFirmwareVersion(gpPad);
                gbEnhancedRumble = luProduct == 0x0df2 || luFirmware == 0 || luFirmware >= 0x0224;
                gFeedbackStatus.meOutput = FeedbackStatus::DualSense;
                gFeedbackStatus.meSupport = FeedbackStatus::Supported;
                SendFeedback({}, SDL_GetTicks(), true); // harmless neutral capability probe
            }
            // Second-generation Steam Controller, wired/Bluetooth or through its
            // puck (SDL's k_eControllerType_SteamControllerTriton product ids).
            else if (luVendor == 0x28de && luProduct >= 0x1302 && luProduct <= 0x1305)
            {
                gFeedbackStatus.meOutput = FeedbackStatus::SteamController;
                // A neutral stop doubles as the capability probe: only the
                // vendor/SDL fork's Triton driver accepts haptic output reports.
                gFeedbackStatus.meSupport = SendSteamStop() ? FeedbackStatus::Supported : FeedbackStatus::Unsupported;
                // Which SDL3.dll actually loaded decides this path; say so.
                const int liVersion = SDL_GetVersion();
                char lacMessage[384];
                std::snprintf(lacMessage, sizeof(lacMessage),
                    "[triggers] Steam Controller %04x:%04x actuators L2=%u R2=%u SDL %d.%d.%d (%s) haptic probe %s%.200s\n",
                    luVendor, luProduct, gFeedbackSettings.muSteamLeftActuator, gFeedbackSettings.muSteamRightActuator, SDL_VERSIONNUM_MAJOR(liVersion), SDL_VERSIONNUM_MINOR(liVersion),
                    SDL_VERSIONNUM_MICRO(liVersion), SDL_GetRevision(),
                    gFeedbackStatus.meSupport == FeedbackStatus::Supported ? "accepted" : "rejected: ",
                    gFeedbackStatus.meSupport == FeedbackStatus::Supported ? "" : SDL_GetError());
                LogFeedback(lacMessage);
            }
            else return;
            gbObserveFeedback.store(gFeedbackSettings.mbDiagnostics ||
                (gFeedbackStatus.meSupport == FeedbackStatus::Supported && gFeedbackSettings.GetScale() > 0), std::memory_order_relaxed);
            AdaptiveTriggers::EngineFeedback::SetEnabled(gbObserveFeedback.load(std::memory_order_relaxed));
            LogFeedback(gFeedbackStatus.meSupport != FeedbackStatus::Supported ?
                "[triggers] adaptive output unavailable; input and standard rumble remain active\n" :
                gFeedbackStatus.meOutput == FeedbackStatus::SteamController ?
                "[triggers] Steam Controller haptic output accepted; physical output requires hardware validation\n" :
                "[triggers] native DualSense effect output accepted; physical output requires hardware validation\n");
        }

        void UpdateFeedback(Uint64 luNow)
        {
            if (!gpPad && !gFeedbackSettings.mbDiagnostics) return;
            AdaptiveTriggers::Sample lSample;
            bool lbNew = false;
            std::uint64_t luPublished = 0;
            {
                const std::lock_guard<std::mutex> lLock(gFeedbackMutex);
                if (guReadFeedbackVersion != guFeedbackVersion)
                {
                    guReadFeedbackVersion = guFeedbackVersion;
                    lSample = gFeedbackSample;
                    luPublished = guPublishedAt;
                    gFeedbackSample.muEvents = 0;
                    lbNew = true;
                }
            }
            if (!gbInputAllowed || !gbFeedbackAllowed || gbFeedbackPhotoMode.load(std::memory_order_relaxed))
            {
                gFeedbackController.Reset();
                SendFeedback({}, luNow, gLastFeedbackPacket != AdaptiveTriggers::Encode(AdaptiveTriggers::Output{}));
                return;
            }
            if (lbNew)
            {
                gObservedFeedbackSample = lSample;
                guObservedAt = luPublished;
                gFeedbackController.Submit(lSample, luPublished);
            }
            const auto lOutput = gFeedbackController.Resolve(luNow);
            if (gFeedbackSettings.mbDiagnostics)
            {
                if (lOutput.mLeft.meCue != geObservedLeft) ++gaFeedbackWitness[static_cast<unsigned>(lOutput.mLeft.meCue)];
                if (lOutput.mRight.meCue != geObservedRight) ++gaFeedbackWitness[static_cast<unsigned>(lOutput.mRight.meCue)];
                geObservedLeft = lOutput.mLeft.meCue;
                geObservedRight = lOutput.mRight.meCue;
            }
            if (gFeedbackSettings.mbDiagnostics && luNow - guLastWitnessLog >= 1000)
            {
                char lacMessage[1024];
                std::snprintf(lacMessage, sizeof(lacMessage),
                    "[triggers] observe device=%u support=%d output=%d car=%llu age=%llu active=%d driving=%d rpm=%.0f mph=%.1f gas=%.2f brake=%.2f gear=%d ground=%u slip=%.2f boost=%d limiter=%d showtime=%d left=%s right=%s\n",
                    guSelectedId, static_cast<int>(gFeedbackStatus.meSupport), static_cast<int>(gFeedbackStatus.meOutput),
                    static_cast<unsigned long long>(gObservedFeedbackSample.muVehicle),
                    static_cast<unsigned long long>(luNow >= guObservedAt ? luNow - guObservedAt : 0),
                    gObservedFeedbackSample.mbActive, gObservedFeedbackSample.mbDriving,
                    gObservedFeedbackSample.mfRPM, gObservedFeedbackSample.mfSpeedMPH,
                    gObservedFeedbackSample.mfGas, gObservedFeedbackSample.mfBrake, gObservedFeedbackSample.miGear,
                    gObservedFeedbackSample.muGroundedWheels,
                    gObservedFeedbackSample.mfDrivenSlip, gObservedFeedbackSample.mbBoosting,
                    gObservedFeedbackSample.mbLimiter, gObservedFeedbackSample.mbShowtime,
                    AdaptiveTriggers::GetCueName(lOutput.mLeft.meCue), AdaptiveTriggers::GetCueName(lOutput.mRight.meCue));
                LogFeedback(lacMessage);
                std::size_t luUsed = static_cast<std::size_t>(std::snprintf(lacMessage, sizeof(lacMessage), "[triggers] witness physical=unverified"));
                for (unsigned i = 1; i < gaFeedbackWitness.size() && luUsed + 50 < sizeof(lacMessage); ++i)
                    luUsed += static_cast<std::size_t>(std::snprintf(lacMessage + luUsed, sizeof(lacMessage) - luUsed,
                        " %s=%llu", AdaptiveTriggers::GetCueName(static_cast<AdaptiveTriggers::Cue>(i)),
                        static_cast<unsigned long long>(gaFeedbackWitness[i])));
                std::snprintf(lacMessage + luUsed, sizeof(lacMessage) - luUsed, "\n");
                LogFeedback(lacMessage);
                guLastWitnessLog = luNow;
            }
            const bool lbClear = lOutput.mLeft.IsOff() && lOutput.mRight.IsOff() &&
                (gLastFeedbackPacket[10] != 5 || gLastFeedbackPacket[21] != 5);
            SendFeedback(lOutput, luNow, lbClear);
        }

        std::uint64_t IniStamp(const char* lpIniPath)
        {
            std::error_code lError;
            const auto lTime = std::filesystem::last_write_time(lpIniPath, lError);
            return lError ? 0 : static_cast<std::uint64_t>(lTime.time_since_epoch().count());
        }

        unsigned ReadFeedbackSettings(const char* lpIniPath, AdaptiveTriggers::Settings& lrSettings)
        {
            char lacSection[8192]{};
            const unsigned long luSize = lpIniPath ? GetPrivateProfileSectionA(
                "AdaptiveTriggers", lacSection, sizeof(lacSection), lpIniPath) : 0;
            unsigned luInvalid = luSize == sizeof(lacSection) - 2 ? 1 : AdaptiveTriggers::ParseSettings(lacSection, lrSettings);
            // Same 0..3 raw preference accepted by the existing PC system profile.
            // Zero silences adaptive output; invalid profile text keeps its default.
            char lacVibration[64] = "3";
            const unsigned long luVibrationSize = lpIniPath ? GetPrivateProfileStringA("SystemProfile", "ControllerVibration", "3",
                lacVibration, sizeof(lacVibration), lpIniPath) : 1;
            char* lpTail = nullptr;
            errno = 0;
            const long liVibration = std::strtol(lacVibration, &lpTail, 10);
            while (lpTail && std::isspace(static_cast<unsigned char>(*lpTail))) ++lpTail;
            if (luVibrationSize == sizeof(lacVibration) - 1 || !lpTail || *lpTail || errno == ERANGE || liVibration < 0 || liVibration > 3) ++luInvalid;
            else if (*lacVibration) lrSettings.mbSystemFeedbackEnabled = liVibration != 0;
            return luInvalid;
        }

        // gMutex held.
        void ApplyFeedbackSettings(const AdaptiveTriggers::Settings& lrSettings)
        {
            SendFeedback({}, SDL_GetTicks(), true);
            // The actuators may change: silence the old ones before switching.
            if (gpPad && gFeedbackStatus.meSupport == FeedbackStatus::Supported &&
                gFeedbackStatus.meOutput == FeedbackStatus::SteamController)
                SendSteamStop();
            gFeedbackSettings = lrSettings;
            gbObserveFeedback.store(lrSettings.mbDiagnostics ||
                (gFeedbackStatus.meSupport == FeedbackStatus::Supported && lrSettings.GetScale() > 0), std::memory_order_relaxed);
            AdaptiveTriggers::EngineFeedback::SetEnabled(gbObserveFeedback.load(std::memory_order_relaxed));
            gFeedbackController.Configure(lrSettings);
            ResetFeedbackState();
        }

        // Tuning without a restart: a changed config.ini write time re-reads the
        // whole section, exactly as at startup. gMutex held; at most once a second.
        void ReloadFeedbackSettings(Uint64 luNow)
        {
            if (!*gacFeedbackIni || luNow - guLastIniCheck < 1000) return;
            guLastIniCheck = luNow;
            const std::uint64_t luStamp = IniStamp(gacFeedbackIni);
            if (!luStamp || luStamp == guFeedbackIniStamp) return;
            guFeedbackIniStamp = luStamp;
            AdaptiveTriggers::Settings lSettings;
            const unsigned luInvalid = ReadFeedbackSettings(gacFeedbackIni, lSettings);
            ApplyFeedbackSettings(lSettings);
            if (gpFeedbackLogger)
            {
                char lacMessage[128];
                std::snprintf(lacMessage, sizeof(lacMessage),
                    "[input] AdaptiveTriggers: config.ini reloaded, %u invalid setting(s)\n", luInvalid);
                gpFeedbackLogger(lacMessage);
            }
        }

        bool IsModifier(unsigned luKey) { return luKey >= 0x10 && luKey <= 0x12; }

        void PostKey(unsigned luKey, bool lbDown, bool lbAlt)
        {
            if (!gpKeyboardWindow) return;
            std::uintptr_t luFlags = 1u | (static_cast<std::uintptr_t>(MapVirtualKeyW(luKey, 0) & 0xff) << 16);
            if ((luKey >= 0x21 && luKey <= 0x28) || luKey == 0x2d || luKey == 0x2e || luKey == 0x6f)
                luFlags |= 1u << 24;
            if (lbAlt) luFlags |= 1u << 29;
            if (!lbDown) luFlags |= (1u << 30) | (1u << 31);
            // Post only to our own window. Polling consumers see the same keys
            // below; no desktop/global keyboard state is changed.
            PostMessageW(gpKeyboardWindow, lbAlt ? (lbDown ? 0x104 : 0x105) : (lbDown ? 0x100 : 0x101),
                         luKey, static_cast<std::intptr_t>(luFlags));
        }

        void PublishKeyboard(std::uint64_t luButtons, bool lbAllowed)
        {
            const auto lBefore = gKeyboardMapping.GetKeys();
            gKeyboardMapping.Update(luButtons, lbAllowed);
            const auto& lrAfter = gKeyboardMapping.GetKeys();
            // Release ordinary keys before modifiers; press modifiers first.
            for (unsigned luPass = 0; luPass < 2; ++luPass)
                for (unsigned luKey = 1; luKey < 256; ++luKey)
                    if (IsModifier(luKey) == (luPass != 0) && lBefore[luKey] && !lrAfter[luKey])
                        PostKey(luKey, false, lBefore[0x12]);
            for (unsigned luPass = 0; luPass < 2; ++luPass)
                for (unsigned luKey = 1; luKey < 256; ++luKey)
                    if (IsModifier(luKey) == (luPass == 0) && !lBefore[luKey] && lrAfter[luKey])
                        PostKey(luKey, true, lrAfter[0x12]);
        }

        void UpdateKeyboard()
        {
            if (!gKeyboardMapping.HasBindings()) return;
            std::uint64_t luButtons = 0;
            if (gpPad)
                for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
                    if (SDL_GetGamepadButton(gpPad, static_cast<SDL_GamepadButton>(i)))
                        luButtons |= std::uint64_t{1} << i;
            const bool lbFocused = gpKeyboardWindow && GetForegroundWindow() == gpKeyboardWindow;
            PublishKeyboard(luButtons, lbFocused && gpPad);
        }

        bool SDLCALL DeviceChanged(void*, SDL_Event* lpEvent)
        {
            if (lpEvent->type == SDL_EVENT_GAMEPAD_ADDED ||
                lpEvent->type == SDL_EVENT_GAMEPAD_REMOVED ||
                lpEvent->type == SDL_EVENT_GAMEPAD_REMAPPED)
                gbProbeRequested.store(true, std::memory_order_relaxed);
            return true;
        }

        void ClosePad()
        {
            PublishKeyboard(0, false);
            if (gpPad)
            {
                if (SDL_GamepadConnected(gpPad)) SendFeedback({}, SDL_GetTicks(), true);
                SDL_RumbleGamepad(gpPad, 0, 0, 0);
                SDL_CloseGamepad(gpPad);
            }
            gpPad = nullptr;
            guSelectedId = 0;
            guLeft = guRight = 0;
            gbRumblePlaying = false;
            gLastState = {};
            gFeedbackStatus = {};
            gbObserveFeedback.store(gFeedbackSettings.mbDiagnostics, std::memory_order_relaxed);
            AdaptiveTriggers::EngineFeedback::SetEnabled(gFeedbackSettings.mbDiagnostics);
            AdaptiveTriggers::BoostFeedback::Reset();
            ResetFeedbackState();
        }

        bool ApplyRumble()
        {
            // SDL caps effects at 65.535 seconds. Keep a successful logical
            // request alive even when a consumer does not resend its amplitudes:
            // renew this short effect while UpdateDevice owns an active request.
            const bool lbActive = gbInputAllowed && (guLeft || guRight);
            const bool lbResult = SDL_RumbleGamepad(gpPad, gbInputAllowed ? guLeft : 0,
                gbInputAllowed ? guRight : 0, lbActive ? 1000 : 0);
            guLastRumble = SDL_GetTicks();
            gbRumblePlaying = lbResult && lbActive;
            return lbResult;
        }

        void UpdateDevice()
        {
            SDL_UpdateGamepads();
            if (gpPad && !SDL_GamepadConnected(gpPad))
            {
                ClosePad();
                gbProbeRequested.store(true, std::memory_order_relaxed);
            }
            const Uint64 luNow = SDL_GetTicks();
            if (gpPad && gbRumblePlaying && luNow - guLastRumble >= 500)
                ApplyRumble();
            // Never enumerate per frame, even with no pad. Notifications make
            // discovery immediate; the one-second fallback covers missed events.
            if (gbProbeRequested.exchange(false, std::memory_order_relaxed) ||
                luNow - guLastProbe >= 1000)
            {
                guLastProbe = luNow;
                if (!gpPad)
                {
                    int liCount = 0;
                    SDL_JoystickID* lpIds = SDL_GetGamepads(&liCount);
                    // Lowest instance ID = earliest attached. Keep that device
                    // until it disconnects; adding another never steals player 0.
                    if (lpIds)
                    {
                        std::sort(lpIds, lpIds + liCount);
                        for (int i = 0; i < liCount && !gpPad; ++i)
                        {
                            gpPad = SDL_OpenGamepad(lpIds[i]);
                            if (gpPad)
                            {
                                guSelectedId = lpIds[i];
                                OpenFeedback();
                            }
                        }
                        SDL_free(lpIds);
                    }
                }
            }
            UpdateKeyboard();
            ReloadFeedbackSettings(luNow);
            UpdateFeedback(luNow);
            // Only our input event ranges; leave the application's other SDL
            // events alone. The watch has already recorded lifecycle changes.
            SDL_FlushEvents(SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_JOYSTICK_UPDATE_COMPLETE);
            SDL_FlushEvents(SDL_EVENT_GAMEPAD_AXIS_MOTION, SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED);
        }

        Sint16 FlipY(Sint16 liValue)
        {
            // SDL up is negative; XDK up is positive. Preserve both endpoints
            // without overflowing -(-32768), and keep the centre exactly zero.
            return static_cast<Sint16>(liValue < 0 ? -static_cast<int>(liValue) * 32767 / 32768 :
                                                    -static_cast<int>(liValue) * 32768 / 32767);
        }

        Uint8 Trigger(SDL_GamepadAxis leAxis)
        {
            const int liValue = SDL_GetGamepadAxis(gpPad, leAxis);
            return static_cast<Uint8>(liValue > 0 ? (liValue * 255 + 16383) / 32767 : 0);
        }
    }

    namespace
    {
        // The window procedure can run inside an SDL call made under gMutex on
        // the same thread: a sent message such as WM_ACTIVATEAPP is delivered by
        // any message check SDL performs there. Re-locking would throw
        // resource_deadlock_would_occur, so such nested calls are recorded and
        // applied when the outer call releases the device.
        thread_local bool tbHoldingDevice = false;
        int giPendingInputAllowed = -1;      // -1 none, else 0/1
        bool gbPendingKeyboardWindow = false;
        void* gpPendingKeyboardWindow = nullptr;

        void ApplyInputAllowed(bool lbAllowed)
        {
            if (gbInputAllowed == lbAllowed)
                return;
            gbInputAllowed = lbAllowed;
            if (gpPad)
            {
                ApplyRumble();
                ResetFeedbackState();
                if (!lbAllowed) SendFeedback({}, SDL_GetTicks(), true);
            }
        }

        void ApplyKeyboardWindow(void* lpWindow)
        {
            PublishKeyboard(0, false);
            if (!lpWindow)
            {
                gbInputAllowed = false;
                ResetFeedbackState();
                SendFeedback({}, SDL_GetTicks(), true);
                if (gpPad) ApplyRumble();
            }
            gpKeyboardWindow = lpWindow;
        }

        struct DeviceLock
        {
            std::unique_lock<std::mutex> mLock{gMutex};
            DeviceLock() { tbHoldingDevice = true; }
            ~DeviceLock()
            {
                // Still held: anything these apply that re-enters defers again.
                while (giPendingInputAllowed >= 0 || gbPendingKeyboardWindow)
                {
                    if (gbPendingKeyboardWindow)
                    {
                        gbPendingKeyboardWindow = false;
                        ApplyKeyboardWindow(gpPendingKeyboardWindow);
                    }
                    if (giPendingInputAllowed >= 0)
                    {
                        const bool lbAllowed = giPendingInputAllowed != 0;
                        giPendingInputAllowed = -1;
                        ApplyInputAllowed(lbAllowed);
                    }
                }
                tbHoldingDevice = false;
            }
            DeviceLock(const DeviceLock&) = delete;
            DeviceLock& operator=(const DeviceLock&) = delete;
        };
    }

    bool Initialize()
    {
        const DeviceLock lLock;
        if (gbInitialized)
            return true;
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
        // SDL disables the Bluetooth Steam Controller driver by default. Enable
        // its native buttons, including paddles, while respecting explicit hints.
        SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "1", SDL_HINT_DEFAULT);
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
            return false;
        if (!SDL_AddEventWatch(DeviceChanged, nullptr))
        {
            SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
            return false;
        }
        gbInitialized = true;
        gbInputAllowed = true;
        gbFeedbackAllowed = true;
        gbFeedbackPhotoMode.store(false, std::memory_order_relaxed);
        gbObserveFeedback.store(gFeedbackSettings.mbDiagnostics, std::memory_order_relaxed);
        AdaptiveTriggers::EngineFeedback::SetEnabled(gFeedbackSettings.mbDiagnostics);
        gbProbeRequested.store(true, std::memory_order_relaxed);
        return true;
    }

    void Shutdown()
    {
        const DeviceLock lLock;
        if (!gbInitialized)
            return;
        ClosePad();
        SDL_RemoveEventWatch(DeviceChanged, nullptr);
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        gbInitialized = false;
        gbObserveFeedback.store(false, std::memory_order_relaxed);
        AdaptiveTriggers::EngineFeedback::SetEnabled(false);
        gpKeyboardWindow = nullptr;
    }

    void RequestProbe()
    {
        gbProbeRequested.store(true, std::memory_order_relaxed);
    }

    bool Read(State& lrState)
    {
        const DeviceLock lLock;
        lrState = {};
        if (!gbInitialized)
            return false;
        UpdateDevice();
        if (!gpPad)
            return false;
        static const SDL_GamepadButton KAE_BUTTONS[] = {
            SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
            SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_BACK,
            SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
            SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH
        };
        static const Uint16 KAU_MASKS[] = {
            0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
            0x100, 0x200, 0x1000, 0x2000, 0x4000, 0x8000
        };
        for (unsigned i = 0; i < sizeof(KAE_BUTTONS) / sizeof(KAE_BUTTONS[0]); ++i)
            if (SDL_GetGamepadButton(gpPad, KAE_BUTTONS[i]) &&
                !(gKeyboardMapping.GetConsumedButtons() & (std::uint64_t{1} << KAE_BUTTONS[i])))
                lrState.Gamepad.wButtons |= KAU_MASKS[i];
        Controls& lrControls = lrState.Gamepad;
        lrControls.bLeftTrigger = Trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        lrControls.bRightTrigger = Trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        lrControls.sThumbLX = SDL_GetGamepadAxis(gpPad, SDL_GAMEPAD_AXIS_LEFTX);
        lrControls.sThumbLY = FlipY(SDL_GetGamepadAxis(gpPad, SDL_GAMEPAD_AXIS_LEFTY));
        lrControls.sThumbRX = SDL_GetGamepadAxis(gpPad, SDL_GAMEPAD_AXIS_RIGHTX);
        lrControls.sThumbRY = FlipY(SDL_GetGamepadAxis(gpPad, SDL_GAMEPAD_AXIS_RIGHTY));
        if (std::memcmp(&lrControls, &gLastState.Gamepad, sizeof(Controls)) != 0)
            ++guPacket;
        lrState.dwPacketNumber = guPacket;
        gLastState = lrState;
        return true;
    }

    std::uint32_t SelectedId()
    {
        const DeviceLock lLock;
        return guSelectedId;
    }

    bool HasRumble()
    {
        const DeviceLock lLock;
        return gpPad && SDL_GetBooleanProperty(SDL_GetGamepadProperties(gpPad),
                                               SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
    }

    void SetInputAllowed(bool lbAllowed)
    {
        if (tbHoldingDevice)
        {
            giPendingInputAllowed = lbAllowed ? 1 : 0;
            return;
        }
        const DeviceLock lLock;
        ApplyInputAllowed(lbAllowed);
    }

    std::uint32_t SetRumble(std::uint16_t luLeft, std::uint16_t luRight)
    {
        const DeviceLock lLock;
        if (!gpPad || !SDL_GamepadConnected(gpPad))
            return 1167; // ERROR_DEVICE_NOT_CONNECTED
        guLeft = luLeft;
        guRight = luRight;
        return ApplyRumble() ? 0 : 50; // ERROR_NOT_SUPPORTED; input stays usable
    }

    void PublishFeedback(const AdaptiveTriggers::Sample& lrSample)
    {
        const std::lock_guard<std::mutex> lLock(gFeedbackMutex);
        const bool lbSameCar = lrSample.muVehicle == gFeedbackSample.muVehicle &&
            lrSample.mbShowtime == gFeedbackSample.mbShowtime && lrSample.mbActive && !lrSample.mbReset;
        const auto luPending = lbSameCar ? gFeedbackSample.muEvents : 0u;
        gFeedbackSample = lrSample;
        gFeedbackSample.muEvents |= luPending;
        guPublishedAt = SDL_GetTicks();
        ++guFeedbackVersion;
    }

    bool WantsFeedback()
    {
        return gbObserveFeedback.load(std::memory_order_relaxed);
    }

    void SetFeedbackAllowed(bool lbAllowed)
    {
        const DeviceLock lLock;
        if (gbFeedbackAllowed == lbAllowed) return;
        gbFeedbackAllowed = lbAllowed;
        ResetFeedbackState();
        if (!lbAllowed) SendFeedback({}, SDL_GetTicks(), true);
    }

    void SetFeedbackPhotoMode(bool lbPhotoMode)
    {
        gbFeedbackPhotoMode.store(lbPhotoMode, std::memory_order_relaxed);
    }

    void SuspendFeedback()
    {
        const DeviceLock lLock;
        ResetFeedbackState();
        if (gLastFeedbackPacket != AdaptiveTriggers::Encode(AdaptiveTriggers::Output{}))
            SendFeedback({}, SDL_GetTicks(), true);
    }

    unsigned LoadFeedbackSettings(const char* lpIniPath)
    {
        AdaptiveTriggers::Settings lSettings;
        const unsigned luInvalid = ReadFeedbackSettings(lpIniPath, lSettings);
        const DeviceLock lLock;
        gacFeedbackIni[0] = 0;
        if (lpIniPath && std::strlen(lpIniPath) < sizeof(gacFeedbackIni))
            std::strcpy(gacFeedbackIni, lpIniPath);
        guFeedbackIniStamp = lpIniPath ? IniStamp(lpIniPath) : 0;
        ApplyFeedbackSettings(lSettings);
        return luInvalid;
    }

    void SetFeedbackLogger(void (*lpLogger)(const char*))
    {
        const DeviceLock lLock;
        gpFeedbackLogger = lpLogger;
    }

    FeedbackStatus GetFeedbackStatus()
    {
        const DeviceLock lLock;
        return gFeedbackStatus;
    }

    unsigned LoadKeyboardMappings(const char* lpIniPath)
    {
        char lacSection[32768]{};
        const unsigned long luSize = lpIniPath ? GetPrivateProfileSectionA(
            "ControllerKeyboard", lacSection, sizeof(lacSection), lpIniPath) : 0;
        KeyboardMapping lMapping;
        const unsigned luInvalid = luSize == sizeof(lacSection) - 2 ? 1 : lMapping.LoadSection(lacSection);
        const DeviceLock lLock;
        PublishKeyboard(0, false);
        gKeyboardMapping = std::move(lMapping);
        return luInvalid;
    }

    void SetKeyboardWindow(void* lpWindow)
    {
        if (tbHoldingDevice)
        {
            gbPendingKeyboardWindow = true;
            gpPendingKeyboardWindow = lpWindow;
            return;
        }
        const DeviceLock lLock;
        ApplyKeyboardWindow(lpWindow);
    }

    void UpdateKeyboardMappings()
    {
        const DeviceLock lLock;
        if (gbInitialized) UpdateDevice(); // also services feedback expiry with no key mappings
    }

    bool MappedKeyDown(int liVirtualKey)
    {
        const DeviceLock lLock;
        return liVirtualKey > 0 && liVirtualKey < 256 && gKeyboardMapping.GetKeys()[liVirtualKey];
    }
}

// FLAG PC-platform leaf: XDK-compatible bridge for recovered console callers.
// Only player 0 is exposed, and every entry point uses the same SDL selection.
extern "C" std::uint32_t XInputGetState(std::uint32_t luPort, void* lpState)
{
    if (!lpState)
        return 87;
    CgsInput::PCGamepad::State lState{};
    const bool lbConnected = luPort == 0 && CgsInput::PCGamepad::Read(lState);
    std::memcpy(lpState, &lState, sizeof(lState));
    return lbConnected ? 0 : 1167;
}

extern "C" std::uint32_t XInputGetCapabilities(std::uint32_t luPort, std::uint32_t, void* lpCaps)
{
    if (!lpCaps)
        return 87;
    std::memset(lpCaps, 0, 20);
    CgsInput::PCGamepad::State lState{};
    if (luPort != 0 || !CgsInput::PCGamepad::Read(lState))
        return 1167;
    auto* lpBytes = static_cast<std::uint8_t*>(lpCaps);
    lpBytes[0] = lpBytes[1] = 1; // gamepad, never a console force-feedback wheel
    if (CgsInput::PCGamepad::HasRumble())
    {
        lpBytes[2] = 1;
        std::memset(lpBytes + 16, 0xff, 4);
    }
    return 0;
}
