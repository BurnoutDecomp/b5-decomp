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
#pragma comment(lib, "user32.lib")

static unsigned guEnumerations = 0;
static Uint64 guNowMs = 0;
static SDL_JoystickID* CountGamepads(int* lpCount)
{
    ++guEnumerations;
    return SDL_GetGamepads(lpCount);
}
static Uint64 TestTicks() { return guNowMs; }
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
#include "../src/pc/input/Gamepad.cpp"
#undef SDL_GetGamepads
#undef SDL_GetTicks
#undef SDL_RumbleGamepad
#include "../src/GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.cpp"
#include "../src/GameShared/GameClasses/System/Input/Devices/X360/CgsInputManagerX360.cpp"

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
    std::printf("PCGamepad: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
