// Execute the actual PC input source with scripted XInput and a deterministic
// clock. The host keyboard and real controller are never read by this fixture.
#define WIN32_LEAN_AND_MEAN
#define NOUSER
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#pragma comment(lib, "user32.lib")

static unsigned long long guNowMs = 0;
static unsigned long guResult = 0;
static unsigned guCalls = 0;
static bool gbNotifyDuringCall = false;
static HMODULE TestLoadLibraryA(LPCSTR) { return reinterpret_cast<HMODULE>(1); }
static FARPROC TestGetProcAddress(HMODULE, LPCSTR);
static ULONGLONG TestGetTickCount64() { return guNowMs; }
#define LoadLibraryA TestLoadLibraryA
#define GetProcAddress TestGetProcAddress
#define GetTickCount64 TestGetTickCount64
#include "pc_xinput_polling.inc"
#undef LoadLibraryA
#undef GetProcAddress
#undef GetTickCount64

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
        DebugPrint* gpDebugPrint = nullptr;
        void WriteToLog(const char*) {}
    }
}
namespace renderengine { HWND__* hWnd = nullptr; }
namespace CgsInput
{
    EBindResult InputPads::BindPlayerToPort(s32, s32) { return E_BINDRESULTOK; }
    bool DeviceX360Pad::BindToPort(u32, s32) { return true; }
}

static XInputGamepad gPad = {};
static unsigned long __stdcall TestGetState(unsigned long luUser, XInputState* lpState)
{
    ++guCalls;
    if (gbNotifyDuringCall)
    {
        gbNotifyDuringCall = false;
        CgsInput::gPCDisconnectedPadPoll.RequestProbe();
    }
    if (luUser != 0) return 1167;
    if (!guResult)
    {
        lpState->dwPacketNumber = guCalls;
        lpState->Gamepad = gPad;
    }
    return guResult;
}
static FARPROC TestGetProcAddress(HMODULE, LPCSTR lpcName)
{
    return std::strcmp(lpcName,"XInputGetState") == 0
        ? reinterpret_cast<FARPROC>(&TestGetState) : nullptr;
}

static unsigned guChecks = 0, guFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++guChecks;
    if (!lbPass) { ++guFailures; std::printf("FAIL %s\n",lpcName); }
}
alignas(16) static unsigned char gaOutput[sizeof(CgsInput::InputIO::OutputBuffer)] = {};
static CgsInput::InputIO::OutputBuffer* Output()
{
    return reinterpret_cast<CgsInput::InputIO::OutputBuffer*>(gaOutput);
}
static void Frame(unsigned short luButtons = 0)
{
    gPad.wButtons = luButtons;
    CgsInput::InputPadsPC::UpdatePlayer0(Output());
}
static unsigned Status()
{
    return Output()->maPadOutputInformation[0].maActionInfo[3].muStatus;
}

int main(int argc, char** argv)
{
    const bool lbControl = argc > 1 && std::strcmp(argv[1],"--control") == 0;
    _putenv_s("BRN_XINPUT_POLL_BACKOFF", lbControl ? "0" : "1");
    char lacSlot[32];
    std::snprintf(lacSlot,sizeof(lacSlot),"xp%lu",GetCurrentProcessId());
    _putenv_s("BRN_HARNESS_SLOT",lacSlot);
    _putenv_s("BRN_INPUT_ALLOW_BACKGROUND","1");
    _putenv_s("BRN_INPUT_KEEP_KEYBOARD","");
    _putenv_s("BRN_INPUT_MAP_DUMP","");

    // No actual or dummy device has been queried before this first frame.
    guResult = 1167;
    Frame();
    Check(guCalls == 1,"first empty-slot probe is immediate");
    XInputState lState = {};
    ReadHostPad0(&lState);
    if (lbControl)
    {
        Check(guCalls == 2,"control retains both consumer calls");
        for (unsigned lu=0; lu<100; ++lu) Frame();
        Check(guCalls == 102,"control never throttles absent-slot calls");
    }
    else
    {
        Check(guCalls == 1,"second consumer shares absence result");
        guNowMs = 999;
        for (unsigned lu=0; lu<100; ++lu) Frame();
        Check(guCalls == 1,"empty slot is not enumerated every frame");
        guNowMs = 1000;
        Frame();
        Check(guCalls == 2,"fallback probes at the one-second boundary");
        guResult = 0;
        Frame(0x1000);
        Check(guCalls == 2 && Status() == 0,"no invented pad state before discovery");
        CgsInput::gPCDisconnectedPadPoll.RequestProbe();
        Frame(0x1000);
        Check(guCalls == 3 && Status() == 3,"device notification discovers held boost immediately");
        Frame(0x1000);
        Check(guCalls == 4 && Status() == 1,"connected pad polls again at the same timestamp");
        Frame();
        Check(guCalls == 5 && Status() == 4,"connected release edge remains intact");
        Frame(0x1000);
        guResult = 1167;
        Frame();
        Check(guCalls == 7 && Status() == 4,"disconnect releases the held action immediately");
        Frame();
        Check(guCalls == 7 && Status() == 0,"cached absence does not repeat a release edge");
        guNowMs = 1999;
        Frame();
        Check(guCalls == 7,"disconnect starts a fresh retry interval");
        guNowMs = 2000;
        guResult = 0;
        Frame(0x1000);
        Check(guCalls == 8 && Status() == 3,"fallback discovers a pad without a notification");
        guResult = 5;
        Frame();
        guResult = 0;
        Frame();
        Check(guCalls == 10,"unexpected errors are retried without delay");
        guResult = 1167;
        gbNotifyDuringCall = true;
        Frame();
        guResult = 0;
        Frame();
        Check(guCalls == 12,"notification during a failed query is not lost");
        guNowMs = std::numeric_limits<unsigned long long>::max()-499;
        guResult = 1167;
        Frame();
        guNowMs = 499;
        Frame();
        Check(guCalls == 13,"elapsed-time subtraction remains bounded across clock wrap");
        guNowMs = 500;
        Frame();
        Check(guCalls == 14,"retry boundary also works across clock wrap");
    }
    std::printf("PCXInputPolling: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures ? 1 : 0;
}
