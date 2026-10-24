// Production SDL backend + production PC action fill, driven through SDL's real
// virtual joystick driver. No physical controller or host keyboard is queried.
#define WIN32_LEAN_AND_MEAN
#define NOUSER
#define NOMINMAX
#include <windows.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "../src/pc/input/KeyboardHost.h"
#pragma comment(lib, "user32.lib")

static unsigned guEnumerations = 0;
static Uint64 guNowMs = 0;
static SDL_JoystickID* CountGamepads(int* lpCount)
{
    ++guEnumerations;
    return SDL_GetGamepads(lpCount);
}
static Uint64 TestTicks() { return guNowMs; }
struct KeyMessage { void* mpWindow; unsigned muMessage, muKey; std::intptr_t miFlags; };
static std::vector<KeyMessage> gKeyMessages;
static void* gpForeground = nullptr;
static void* TestForeground() { return gpForeground; }
static int TestPostMessage(void* lpWindow, unsigned luMessage, std::uintptr_t luKey, std::intptr_t liFlags)
{
    gKeyMessages.push_back({lpWindow, luMessage, static_cast<unsigned>(luKey), liFlags});
    return 1;
}
static unsigned TestMapVirtualKey(unsigned luKey, unsigned) { return luKey; }
static unsigned guRumbleRequests = 0;
static Uint32 guRumbleDuration = 0;
static bool TrackRumble(SDL_Gamepad* lpPad, Uint16 luLeft, Uint16 luRight, Uint32 luDuration)
{
    ++guRumbleRequests;
    guRumbleDuration = luDuration;
    return SDL_RumbleGamepad(lpPad, luLeft, luRight, luDuration);
}
#define SDL_GetGamepads CountGamepads
#define SDL_GetTicks TestTicks
#define SDL_RumbleGamepad TrackRumble
#define GetForegroundWindow TestForeground
#define PostMessageW TestPostMessage
#define MapVirtualKeyW TestMapVirtualKey
#include "../src/pc/input/Gamepad.cpp"
#undef SDL_GetGamepads
#undef SDL_GetTicks
#undef SDL_RumbleGamepad
#undef GetForegroundWindow
#undef PostMessageW
#undef MapVirtualKeyW
#include "../src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.cpp"
#include "../src/GameShared/GameClasses/System/Input/Devices/X360/CgsInputManagerX360.cpp"

static bool gbPhysicalEnd = false;
static short TestAssertKey(int) { return gbPhysicalEnd ? static_cast<short>(0x8000) : 0; }
#define GetAsyncKeyState TestAssertKey
#define VK_END 0x23
namespace CgsDev::Assert
{
#include "assert_end_key.inc"
}
#undef GetAsyncKeyState
#undef VK_END

namespace CgsDev
{
    bool IsDebugKeyboardCapturedPC() { return false; }
    namespace Assert
    {
        int BeginAssert() { return 0; }
        int FireAssert(const char*, const char*, int) { return 0; }
        void* EndAssert() { return nullptr; }
    }
    namespace Log
    {
        StrStreamBase* gpDebugPrint = nullptr;
        void WriteToLog(const char*) {}
    }
}
namespace renderengine { HWND__* hWnd = nullptr; }
namespace CgsSystem::KeyboardPC
{
    bool IsCapturingInput() { return false; }
    bool BlockGameInput(bool) { return false; }
}
namespace CgsInput
{
    EBindResult InputPads::BindPlayerToPort(s32, s32) { return E_BINDRESULTOK; }
    bool DeviceX360Pad::BindToPort(u32 luPort, s32 leType)
    {
        mePort = luPort;
        meType = leType;
        mbConnected = 1;
        return true;
    }
}

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++guChecks;
    if (!lbPass) { ++guFailures; std::printf("FAIL %s: %s\n", lpcName, SDL_GetError()); }
}

struct Motors
{
    Uint16 muLeft = 0, muRight = 0;
    unsigned muCalls = 0;
};
static bool SDLCALL Rumble(void* lpData, Uint16 luLeft, Uint16 luRight)
{
    auto& lrMotors = *static_cast<Motors*>(lpData);
    lrMotors.muLeft = luLeft;
    lrMotors.muRight = luRight;
    ++lrMotors.muCalls;
    return true;
}

static SDL_JoystickID Attach(Motors* lpMotors, const char* lpcName)
{
    SDL_VirtualJoystickDesc lDesc{};
    SDL_INIT_INTERFACE(&lDesc);
    lDesc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    lDesc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    lDesc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    lDesc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1;
    lDesc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1;
    lDesc.name = lpcName;
    lDesc.userdata = lpMotors;
    lDesc.Rumble = lpMotors ? Rumble : nullptr;
    return SDL_AttachVirtualJoystick(&lDesc);
}

alignas(16) static unsigned char gaOutput[sizeof(CgsInput::InputIO::OutputBuffer)]{};
static auto* Output() { return reinterpret_cast<CgsInput::InputIO::OutputBuffer*>(gaOutput); }
static void Frame() { CgsInput::InputPadsPC::UpdatePlayer0(Output()); }
static unsigned BoostStatus() { return Output()->maPadOutputInformation[0].maActionInfo[3].muStatus; }

static void TestKeyboardMappings()
{
    using namespace CgsInput::PCGamepad;
    KeyboardMapping lParser;
    Check(lParser.LoadSection("R4=End\0L4=Ctrl+Shift+K\0Back+Start=F11\0A=W\0\0") == 0,
          "parse paddles, button chords and keyboard chords");
    lParser.Update(std::uint64_t{1} << SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, true);
    Check(lParser.GetKeys()[0x23], "Steam R4 alias produces End");
    Check(lParser.LoadSection("R4=vk\0Bogus=End\0A=UnknownKey\0B+B=End\0X=F25\0Y=Ctrl+\0\0") == 6,
          "invalid keys, buttons, duplicates and incomplete chords are rejected");
    lParser.Update(~std::uint64_t{0}, true);
    Check(lParser.GetKeys().none(), "invalid bindings cannot produce keys");
    Check(lParser.LoadSection(" right_paddle1 = VK_END\0Left_Paddle2=0x24\0south=F24\0\0") == 0,
          "canonical SDL names, spacing, virtual-key aliases and hex codes");
    lParser.Update(std::uint64_t{1} << SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, true);
    Check(lParser.GetKeys()[0x23], "canonical paddle mapping is usable");

    char lacIni[MAX_PATH];
    GetFullPathNameA("controller-mappings.ini", sizeof(lacIni), lacIni, nullptr);
    FILE* lpIni = std::fopen(lacIni, "wb");
    if (!lpIni) { Check(false, "create isolated mapping INI"); return; }
    std::fputs("[ControllerKeyboard]\nR4=End\nMisc1=End\nL4=Ctrl+Shift+K\nBack=Escape\nBack+Start=F11\nA=W\n", lpIni);
    std::fclose(lpIni);
    Check(LoadKeyboardMappings(lacIni) == 0, "load the ControllerKeyboard INI section");
    void* const lpGameWindow = reinterpret_cast<void*>(0x1234);
    gpForeground = lpGameWindow;
    SetKeyboardWindow(lpGameWindow);
    Check(Initialize(), "initialize mapping fixture");
    const SDL_JoystickID luPad = Attach(nullptr, "SDL virtual keyboard mapping");
    SDL_Joystick* lpPad = SDL_OpenJoystick(luPad);
    Check(luPad && lpPad, "attach virtual paddle-capable controller");
    if (!lpPad) { Shutdown(); return; }
    State lState{};
    Read(lState);
    gKeyMessages.clear();
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, true);
    // Call the production assert helper with no normal input/action update.
    Check(CgsDev::Assert::IsAssertReleaseKeyDownPC(), "R4 dismisses asserts while the gameplay input loop is halted");
    Check(MappedKeyDown(0x23) && gKeyMessages.size() == 1 && gKeyMessages[0].mpWindow == lpGameWindow &&
          gKeyMessages[0].muMessage == 0x100 && gKeyMessages[0].muKey == 0x23,
          "End key-down is posted only to the game's window");
    Check(!gKeyMessages.empty() && (gKeyMessages[0].miFlags & (1u << 24)) != 0,
          "End keyboard message has the extended-key flag");
    UpdateKeyboardMappings();
    Check(gKeyMessages.size() == 1, "held controller button does not repeat keyboard press edges");
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_MISC1, true);
    UpdateKeyboardMappings();
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, false);
    Check(CgsDev::Assert::IsAssertReleaseKeyDownPC() && gKeyMessages.size() == 1,
          "shared destination stays down until every mapped source is released");
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_MISC1, false);
    Check(!CgsDev::Assert::IsAssertReleaseKeyDownPC() && gKeyMessages.size() == 2 &&
          gKeyMessages.back().muMessage == 0x101, "assert release wait observes controller release and posts End up");
    gbPhysicalEnd = true;
    Check(CgsDev::Assert::IsAssertReleaseKeyDownPC(), "physical End still dismisses asserts");
    gbPhysicalEnd = false;

    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_BACK, true);
    Read(lState);
    Check(MappedKeyDown(0x1b) && !MappedKeyDown(0x7a) && !(lState.Gamepad.wButtons & 0x20),
          "single-button remap consumes its original controller action");
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_START, true);
    Read(lState);
    Check(!MappedKeyDown(0x1b) && MappedKeyDown(0x7a) && !(lState.Gamepad.wButtons & 0x30),
          "complete Back+Start chord wins over Back and consumes both buttons");
    Check(gKeyMessages.back().muMessage == 0x100 && gKeyMessages.back().muKey == 0x7a,
          "keyboard-message consumers receive F11 from a controller chord");
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_BACK, false);
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_START, false);
    UpdateKeyboardMappings();
    Check(!MappedKeyDown(0x1b) && !MappedKeyDown(0x7a), "releasing a controller chord releases its key");

    gKeyMessages.clear();
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, true);
    Read(lState);
    Check(MappedKeyDown(0x10) && MappedKeyDown(0x11) && MappedKeyDown('K'), "L4 can press a modifier/key combination");
    Check(gKeyMessages.size() == 3 && gKeyMessages[0].muKey == 0x10 && gKeyMessages[1].muKey == 0x11 &&
          gKeyMessages[2].muKey == 'K', "keyboard modifiers are pressed before the ordinary key");
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1, false);
    Read(lState);
    Check(gKeyMessages.size() == 6 && gKeyMessages[3].muKey == 'K' &&
          gKeyMessages[4].muKey == 0x10 && gKeyMessages[5].muKey == 0x11,
          "keyboard modifiers are released after the ordinary key");

    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_SOUTH, true);
    Read(lState);
    const int laiW[] = {'W', 0};
    Check(!(lState.Gamepad.wButtons & 0x1000) && AnyKeyDown(laiW),
          "mapped keyboard keys reach the production gameplay key reader");
    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_SOUTH, false);
    UpdateKeyboardMappings();
    Check(!AnyKeyDown(laiW), "mapped gameplay key releases");

    SDL_SetJoystickVirtualButton(lpPad, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, true);
    UpdateKeyboardMappings();
    gpForeground = reinterpret_cast<void*>(0x5678);
    UpdateKeyboardMappings();
    Check(!MappedKeyDown(0x23) && gKeyMessages.back().muMessage == 0x101,
          "focus loss releases mapped keys even with background pad input enabled");
    gpForeground = lpGameWindow;
    UpdateKeyboardMappings();
    Check(MappedKeyDown(0x23), "mapped input resumes when the game regains focus");
    SDL_CloseJoystick(lpPad);
    SDL_DetachVirtualJoystick(luPad);
    Check(!CgsDev::Assert::IsAssertReleaseKeyDownPC() && !MappedKeyDown(0x23),
          "disconnect releases assert-dismiss key without normal gameplay updates");

    const SDL_JoystickID luReplacement = Attach(nullptr, "SDL virtual replacement mapping");
    SDL_Joystick* lpReplacement = SDL_OpenJoystick(luReplacement);
    if (!lpReplacement) { Check(false, "open mapping replacement"); Shutdown(); return; }
    SDL_SetJoystickVirtualButton(lpReplacement, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, true);
    UpdateKeyboardMappings();
    Check(MappedKeyDown(0x23), "replacement controller can use mappings");
    Check(LoadKeyboardMappings(nullptr) == 0 && !MappedKeyDown(0x23), "removing mappings releases held keys");
    Check(LoadKeyboardMappings(lacIni) == 0, "reload mappings");
    UpdateKeyboardMappings();
    Check(MappedKeyDown(0x23), "reloaded mapping works");
    Shutdown();
    Check(!MappedKeyDown(0x23) && gKeyMessages.back().muMessage == 0x101, "shutdown releases mapped keyboard state");
    SDL_CloseJoystick(lpReplacement);
    Check(!MappedKeyDown(-1) && !MappedKeyDown(256), "out-of-range virtual keys are rejected");
}

int main(int argc, char** argv)
{
    using namespace CgsInput::PCGamepad;
    if (argc > 1 && std::strcmp(argv[1], "--native") == 0)
    {
        Check(Initialize(), "packaged SDL3 initializes without installed SDL");
        State lState{};
        const bool lbConnected = Read(lState);
        std::printf("Native SDL3: controller=%s, instance=%u\n", lbConnected ? "connected" : "absent", SelectedId());
        Shutdown();
        return guFailures ? 1 : 0;
    }
    // Suppress physical Windows drivers, retaining SDL's virtual driver.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS4, "0", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_PS5, "0", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_STEAM, "0", SDL_HINT_OVERRIDE);
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_DIRECTINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_GAMEINPUT, "0");
    _putenv_s("BRN_INPUT_ALLOW_BACKGROUND", "1");
    _putenv_s("BRN_INPUT_KEEP_KEYBOARD", "");
    char lacSlot[32];
    std::snprintf(lacSlot, sizeof(lacSlot), "sdl%lu", GetCurrentProcessId());
    _putenv_s("BRN_HARNESS_SLOT", lacSlot);

    State lState{};
    Check(!Read(lState), "read before initialization is disconnected");
    Check(Initialize(), "SDL3 gamepad initialization");
    Check(Initialize(), "initialization is idempotent");
    Check(!Read(lState) && guEnumerations == 1, "first empty scan is immediate");
    for (unsigned i = 0; i < 500; ++i) Read(lState);
    Check(guEnumerations == 1, "500 empty reads do not enumerate per frame");
    guNowMs = 999;
    Read(lState);
    Check(guEnumerations == 1, "no early fallback enumeration");
    guNowMs = 1000;
    Read(lState);
    Check(guEnumerations == 2, "one-second discovery fallback");
    RequestProbe();
    Read(lState);
    Check(guEnumerations == 3, "Win32 device notification requests immediate discovery");

    Motors lFirstMotors, lSecondMotors;
    const SDL_JoystickID luFirst = Attach(&lFirstMotors, "SDL virtual first");
    const SDL_JoystickID luSecond = Attach(&lSecondMotors, "SDL virtual second");
    Check(luFirst && luSecond, "attach two SDL virtual gamepads");
    SDL_Joystick* lpFirst = SDL_OpenJoystick(luFirst);
    SDL_Joystick* lpSecond = SDL_OpenJoystick(luSecond);
    Check(lpFirst && lpSecond, "open virtual producer handles");
    if (!lpFirst || !lpSecond) return 1;
    for (SDL_Joystick* lpJoystick : {lpFirst, lpSecond})
    {
        SDL_SetJoystickVirtualAxis(lpJoystick, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
        SDL_SetJoystickVirtualAxis(lpJoystick, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
    }
    Check(Read(lState) && SelectedId() == luFirst, "hot-plug chooses lowest instance ID immediately");
    const unsigned luScans = guEnumerations;
    for (unsigned i = 0; i < 100; ++i) Read(lState);
    Check(guEnumerations == luScans && SelectedId() == luFirst, "connected reads retain selection without re-enumeration");
    Check(HasRumble(), "SDL rumble capability is exposed");
    CgsInput::InputPads lPads{};
    lPads.maPads[0].mePort = -1;
    CgsInput::InputPadsPC::UpdatePadDevices(&lPads);
    Check(lPads.maPads[0].mePort == 0 && lPads.maPads[0].mbConnected,
          "production device scan binds the selected SDL gamepad");
    static const SDL_GamepadButton KAE_BUTTONS[] = {
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_BACK,
        SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH
    };
    static const unsigned KAU_MASKS[] = {1,2,4,8,16,32,64,128,256,512,4096,8192,16384,32768};
    for (unsigned i = 0; i < 14; ++i)
    {
        SDL_SetJoystickVirtualButton(lpFirst, KAE_BUTTONS[i], true);
        Read(lState);
        Check(lState.Gamepad.wButtons == KAU_MASKS[i], "SDL button -> console bit");
        SDL_SetJoystickVirtualButton(lpFirst, KAE_BUTTONS[i], false);
        Read(lState);
        Check(lState.Gamepad.wButtons == 0, "button release clears the console bit");
    }
    for (Sint16 liValue : {static_cast<Sint16>(-32768), static_cast<Sint16>(0), static_cast<Sint16>(32767)})
    {
        for (int i = 0; i < 4; ++i) SDL_SetJoystickVirtualAxis(lpFirst, i, liValue);
        Read(lState);
        const Sint16 liY = liValue == -32768 ? 32767 : liValue == 32767 ? -32768 : 0;
        Check(lState.Gamepad.sThumbLX == liValue && lState.Gamepad.sThumbRX == liValue, "both stick X ranges");
        Check(lState.Gamepad.sThumbLY == liY && lState.Gamepad.sThumbRY == liY, "both stick Y ranges and inversion");
    }
    for (int i = 0; i < 4; ++i) SDL_SetJoystickVirtualAxis(lpFirst, i, 0);
    SDL_SetJoystickVirtualAxis(lpFirst, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
    SDL_SetJoystickVirtualAxis(lpFirst, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    Read(lState);
    Check(lState.Gamepad.bLeftTrigger == 255 && lState.Gamepad.bRightTrigger == 255, "both triggers reach console maximum");
    SDL_SetJoystickVirtualAxis(lpFirst, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
    SDL_SetJoystickVirtualAxis(lpFirst, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
    Read(lState);
    Check(lState.Gamepad.bLeftTrigger == 0 && lState.Gamepad.bRightTrigger == 0, "both triggers rest at zero");
    State lBridge{};
    Check(XInputGetState(0, &lBridge) == 0 && std::memcmp(&lBridge, &lState, sizeof(lState)) == 0,
          "X360 PC bridge reads the same device and raw state");
    Check(XInputGetState(1, &lBridge) == 1167 && lBridge.Gamepad.wButtons == 0, "unexposed console slots are disconnected");
    Check(XInputGetState(0, nullptr) == 87, "state bridge rejects null output");
    CgsInput::ManagerX360 lManager{};
    lManager.UpdateConnectedDevices();
    Check(lManager.maDevices[0].mbConnected && lManager.maDevices[0].meDeviceType == 1, "production manager recognizes the SDL gamepad");
    Check(!lManager.maDevices[1].mbConnected && !lManager.maDevices[2].mbConnected && !lManager.maDevices[3].mbConnected,
          "production manager exposes only player 0");

    Frame();
    SDL_SetJoystickVirtualButton(lpFirst, SDL_GAMEPAD_BUTTON_SOUTH, true);
    Frame();
    Check(BoostStatus() == 3, "production action mapping produces boost pressed+held");
    Frame();
    Check(BoostStatus() == 1, "production held edge is stable");
    SDL_SetJoystickVirtualButton(lpFirst, SDL_GAMEPAD_BUTTON_SOUTH, false);
    Frame();
    Check(BoostStatus() == 4, "production action release edge");
    Check(SetRumble(12345, 54321) == 0 && lFirstMotors.muLeft == 12345 && lFirstMotors.muRight == 54321,
          "both rumble motors reach selected controller");
    Check(lSecondMotors.muCalls == 0, "unselected controller does not rumble");
    Check(guRumbleDuration == 1000, "rumble uses an expiring one-second SDL effect");
    const unsigned luRumbleRequests = guRumbleRequests;
    guNowMs += 499;
    Read(lState);
    Check(guRumbleRequests == luRumbleRequests, "unchanged rumble is not resent per frame");
    ++guNowMs;
    Read(lState);
    Check(guRumbleRequests == luRumbleRequests + 1, "active cached rumble renews at 500 ms");
    for (unsigned i = 0; i < 140; ++i)
    {
        guNowMs += 500;
        Read(lState);
    }
    Check(guRumbleRequests == luRumbleRequests + 141 && lFirstMotors.muLeft == 12345 &&
          lFirstMotors.muRight == 54321, "unchanged rumble renews beyond SDL's 65-second duration cap");
    unsigned short lauSpeeds[] = {4321, 1234};
    Check(XInputSetState(0, lauSpeeds) == 0 && lFirstMotors.muLeft == 4321 && lFirstMotors.muRight == 1234,
          "recovered motor bridge reaches the same selected SDL controller");
    Check(XInputSetState(1, lauSpeeds) == 1167 && XInputSetState(0, nullptr) == 87,
          "motor bridge rejects other slots and null input");
    SetInputAllowed(false);
    Check(lFirstMotors.muLeft == 0 && lFirstMotors.muRight == 0, "focus loss stops rumble");
    SetRumble(2222, 3333);
    Check(lFirstMotors.muLeft == 0 && lFirstMotors.muRight == 0, "background rumble requests remain muted");
    SetInputAllowed(true);
    Check(lFirstMotors.muLeft == 2222 && lFirstMotors.muRight == 3333, "focus return restores requested rumble");
    SetRumble(0, 0);
    Check(lFirstMotors.muLeft == 0 && lFirstMotors.muRight == 0, "explicit stop clears both motors");
    SDL_SetJoystickVirtualButton(lpFirst, SDL_GAMEPAD_BUTTON_SOUTH, true);
    Frame();
    SDL_CloseJoystick(lpFirst);
    Check(SDL_DetachVirtualJoystick(luFirst), "disconnect selected gamepad");
    Frame();
    Check(SelectedId() == luSecond && BoostStatus() == 4, "disconnect selects next pad and releases held action");
    Check(lSecondMotors.muCalls == 0, "replacement does not inherit old motor request");
    lPads.maPads[0].mCachedRumble = {12345, 54321};
    CgsInput::InputPadsPC::UpdatePadDevices(&lPads);
    Check(lPads.maPads[0].mePort == 0 && !lPads.maPads[0].mCachedRumble.muLeftMotorSpeed &&
          !lPads.maPads[0].mCachedRumble.muRightMotorSpeed, "replacement invalidates console motor cache");
    SDL_SetJoystickVirtualButton(lpSecond, SDL_GAMEPAD_BUTTON_SOUTH, true);
    Frame();
    Check(BoostStatus() == 3, "replacement produces fresh press");
    SDL_CloseJoystick(lpSecond);
    SDL_DetachVirtualJoystick(luSecond);
    Frame();
    Check(SelectedId() == 0 && BoostStatus() == 4, "last disconnect releases held action");
    CgsInput::InputPadsPC::UpdatePadDevices(&lPads);
    Check(lPads.maPads[0].mePort == -1 && !lPads.maPads[0].mbConnected, "production device scan unbinds on disconnect");
    Frame();
    Check(BoostStatus() == 0, "disconnected release does not repeat");
    Check(SetRumble(1, 1) == 1167, "disconnected rumble fails quietly");
    lManager.UpdateConnectedDevices();
    Check(!lManager.maDevices[0].mbConnected, "production manager observes disconnect");

    const SDL_JoystickID luNoRumble = Attach(nullptr, "SDL virtual without rumble");
    Check(Read(lState) && SelectedId() == luNoRumble, "reconnect without restart");
    Check(!HasRumble() && SetRumble(1, 1) == 50 && Read(lState), "unsupported rumble does not prevent input");
    SDL_DetachVirtualJoystick(luNoRumble);
    Read(lState);
    const SDL_JoystickID luFinal = Attach(&lFirstMotors, "SDL virtual final");
    Check(Read(lState) && SelectedId() == luFinal, "second reconnect");
    SetRumble(4321, 1234);
    Shutdown();
    Check(lFirstMotors.muLeft == 0 && lFirstMotors.muRight == 0, "shutdown stops both motors");
    Check(SelectedId() == 0 && !Read(lState), "shutdown clears selection and state");
    Shutdown();
    TestKeyboardMappings();
    std::printf("PCGamepad: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
