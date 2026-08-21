#include <cstdio>

#include "GameShared/GameClasses/System/CgsHardwareInit.h"

#include <Windows.h>
#include <d3d9.h>

#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [gateui r7] the exit-path diagnostics
#include "pc/gcm/renderengine/device.h"

static const char *kDefaultAutoTestScript = "autotest.txt";
static const char *autoTestCmdPrefix = "-autotest:";

static const char *mutexName = "BurnoutParadiseexe";
static const char *windowClassName = "BurnoutParadiseWindowClass";
static const char *windowName = "Burnout Paradise";

// TODO: This should be defined in a header file
static bool isPartyEdition = false;

static HCURSOR hCursor = nullptr;

static bool deviceChanged = false;
static bool deviceChangedSinceLaunch = false;

static s32 mouseX = 0;
static s32 mouseY = 0;
static bool leftMouseDown = false;

// TODO: This should be defined in a header file
static bool gEnableMultiThreading = false;

char CgsSystem::HardwareInit::macRootPath[knHardwarePathMaxLength];
char CgsSystem::HardwareInit::macFOPENPath[knHardwarePathMaxLength];

char CgsSystem::HardwareInit::macAutoTestScriptToRun[64];

char CgsSystem::HardwareInit::macTitleIdFromCmdLine[10];

//JobScheduler CgsSystem::HardwareInit::mJobManager; // TODO: Implement HardwareInit

char CgsSystem::HardwareInit::macJobManagerBuffer[400 * 1024]; // 400 KB buffer for job manager

//CgsMemory::HeapMallocCoreAllocator CgsSystem::HardwareInit::mJobManagerAllocator; // TODO: Implement HardwareInit

volatile bool CgsSystem::HardwareInit::mbHardwareRequestsShutdown;

bool CgsSystem::HardwareInit::mbIsGuideOnScreen;

bool CgsSystem::HardwareInit::mbHasDetectedAutomaticTestingFile;

static void SetSystemParameters(bool reset)
{
    static bool alreadyRetrievedParams = false;
    if (!alreadyRetrievedParams)
    {
        // TODO: Implement SetSystemParameters

        alreadyRetrievedParams = true;
    }

    if (reset)
    {
        // TODO: Implement SetSystemParameters
    }
    else
    {
        // TODO: Implement SetSystemParameters
    }
}

static void CheckSSE2Support()
{
    if (!IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE))
    {
        MessageBox(NULL,
                  "This machine does not support the SSE2 Command Set which is required to run this game.\n"
                         "\n"
                         "The game will now terminate",
                         "CPU Error",
                         MB_OK | MB_ICONHAND | MB_SYSTEMMODAL | MB_TOPMOST | MB_SETFOREGROUND);

        exit(-881);
    }
}

static bool RegisterDeviceNotif(HDEVNOTIFY* notify)
{
    // TODO: Implement RegisterDeviceNotif
    return true;
}

// ---------------------------------------------------------------------------
// FLAG PC-platform leaf: bounded teardown. HOST POLICY -- there is no console counterpart
// (the X360 title is torn down by the kernel the moment main() returns).
//
// Once the user has asked for the window to close, this process MUST die. The engine's own
// release path is only partly landed (EngineRelease's gm->Release() loop and GameRelease's
// gm->Destruct() are still gated -- see BrnMain.cpp), and the pieces that DO run can block:
// an XAudio2 engine Release, a file device parked mid-transfer, a decoder waiting on a
// buffer, or the CRT's static teardown. So the whole shutdown gets a fixed budget measured
// from the moment the close was requested; if the normal path has not already exited the
// process by then, the watchdog terminates it outright.
//
// Armed once and never disarmed: when the normal path wins the race the process is already
// gone and this thread dies with it.
// ---------------------------------------------------------------------------
static const DWORD KU_SHUTDOWN_BUDGET_MS = 5000;

static DWORD WINAPI ShutdownWatchdogProc(LPVOID)
{
    Sleep(KU_SHUTDOWN_BUDGET_MS);
    // ⭐ [gateui r7 / defect B] the watchdog is a TerminateProcess, i.e. one of the ways this
    // process can vanish without a fault. Say so before it fires, so a log that ends here is
    // never mistaken for a crash.
    *CgsDev::Log::gpDebugPrint
        << "[exit-diag] shutdown watchdog expired after " << static_cast<s32>(KU_SHUTDOWN_BUDGET_MS)
        << " ms -- TerminateProcess\n";
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}

void CgsSystem::HardwareInit::RequestShutdown()
{
    if (mbHardwareRequestsShutdown)
        return;                                   // already requested; the watchdog is armed

    // ⭐ [gateui r7 / defect B] the ONE place the run is told to end. Anything that reaches here
    // has a window-message cause; a run that dies with no [exit-diag] line at all did not.
    *CgsDev::Log::gpDebugPrint << "[exit-diag] RequestShutdown -- the run was asked to end\n";

    mbHardwareRequestsShutdown = true;

    HANDLE hWatchdog = CreateThread(nullptr, 0, ShutdownWatchdogProc, nullptr, 0, nullptr);
    if (hWatchdog)
        CloseHandle(hWatchdog);
}

static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        // User closed the window: raise the shutdown request FIRST, then tear the window down
        // (-> WM_DESTROY). The flag is what actually ends the run -- WM_QUIT alone is not
        // enough, because whichever message pump happens to be running when the close arrives
        // consumes it, and the assert screen's modal pump used to consume it and discard it.
        // [gateui defect-B closure] name the cause: an external WM_CLOSE (a human clicking the
        // window's X, Alt+F4, taskbar close, or another process's CloseMainWindow). The
        // 2026-08-20 "silent mid-drive exits" all resolved to this route once the exit net
        // named it -- deliberate closes during concurrent play-testing, not a game defect.
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint
                << "[exit-diag] WM_CLOSE received (external close request; foreground="
                << (GetForegroundWindow() == hwnd ? 1 : 0) << ")\n";
        CgsSystem::HardwareInit::RequestShutdown();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        // Also covers a destroy that did not come through WM_CLOSE. Post WM_QUIT as well so a
        // plain `while (message != WM_QUIT)` pump still ends the moment it sees it.
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "[exit-diag] WM_DESTROY received\n";
        CgsSystem::HardwareInit::RequestShutdown();
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        // FLAG PC-platform leaf (no console counterpart -- the X360/PS3 own the display
        // outright and have no window manager). The client area belongs to the D3D9 swap
        // chain: every pixel of it is written by BrnRendererModule::Render (FrameBegin's
        // clear -> the 2D/GUI tail -> ShowPixelBuffer's Present). Letting DefWindowProc
        // erase it runs BeginPaint's fill with the class background brush straight over the
        // last PRESENTED frame -- and because this is a WINDOWED D3DSWAPEFFECT_COPY chain
        // (device.cpp), Present blits into the same DWM redirection surface GDI paints
        // into, so the fill wipes the whole client: scene, Apt GUI, loading screen AND the
        // debug HUD together, until the next present. That is the "whole screen flashes
        // black" flicker (the HUD vanishing with everything else is what identifies it --
        // no render-path failure can blank the HUD, which draws last and unconditionally).
        // Claim the erase and draw nothing.
        return 1;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
// FLAG PC-platform leaf (no console counterpart -- the X360/PS3 present straight to
// the display and have no compositor).
//
// Opt this window OUT of the Windows 11 DWM "system backdrop" (Mica / Acrylic).
// When a backdrop is applied -- either by the OS or by a third-party backdrop
// injector, which is how it reaches a window like ours that never opts in -- DWM
// composites a translucent, blurred desktop sample UNDER the window and blends the
// presented frame over it. The rendered image is untouched (the D3D9 back buffer is
// pixel-correct and X8R8G8B8, i.e. carries no alpha of its own), but everything the
// user SEES is desaturated and its dark values are lifted, which reads as "washed out
// colours" and makes fine art -- e.g. the Paradise City logo -- look muddy.
// Measured on the autosave prompt's red banner: the back buffer holds a strong red
// while a window-space grab of the same frame reads (56, 32, 33) with a ~32 green/blue
// floor that the source pixels do not contain.
//
// DWMWA_SYSTEMBACKDROP_TYPE(38) = DWMSBT_NONE(1) is the documented opt-out (Win11
// 22H2+). DWMWA_MICA_EFFECT(1029) = 0 is the undocumented predecessor honoured by
// Win11 21H2. Both are set, newest first; unsupported attributes simply return a
// failure HRESULT on older systems, so this is safe down to Win7.
// dwmapi is resolved dynamically so the link does not gain a hard dwmapi.lib dependency.
// ---------------------------------------------------------------------------
static void DisableSystemBackdrop(HWND window)
{
    typedef HRESULT(WINAPI * PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);

    HMODULE dwm = LoadLibraryA("dwmapi.dll");
    if (dwm == nullptr)
        return;

    PFN_DwmSetWindowAttribute setAttribute = reinterpret_cast<PFN_DwmSetWindowAttribute>(
        reinterpret_cast<void*>(GetProcAddress(dwm, "DwmSetWindowAttribute")));
    if (setAttribute != nullptr)
    {
        const DWORD KU_DWMWA_SYSTEMBACKDROP_TYPE = 38;
        const DWORD KU_DWMWA_MICA_EFFECT         = 1029;

        INT liBackdropNone = 1;    // DWMSBT_NONE
        setAttribute(window, KU_DWMWA_SYSTEMBACKDROP_TYPE, &liBackdropNone, sizeof(liBackdropNone));

        BOOL lbMicaOff = FALSE;
        setAttribute(window, KU_DWMWA_MICA_EFFECT, &lbMicaOff, sizeof(lbMicaOff));
    }

    FreeLibrary(dwm);
}

static HWND CreateGameWindow(const s32 width, const s32 height, bool fullscreen)
{
    HINSTANCE module = GetModuleHandle(nullptr);

    char filename[MAX_PATH + 7];
    GetModuleFileName(nullptr, filename, 260);

    WNDCLASS wndClass;
    ZeroMemory(&wndClass, sizeof(WNDCLASS));
    wndClass.style = 0;
    wndClass.lpfnWndProc = windowProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = module;
    wndClass.hIcon = ExtractIcon(module, filename, 0);
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    // No background brush: the client area is the D3D9 swap chain's (see the WM_ERASEBKGND
    // case in windowProc). A BLACK_BRUSH here is what DefWindowProc would paint over the
    // presented frame on any erase that slips past that handler.
    wndClass.hbrBackground = nullptr;
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = windowClassName;
    if (!RegisterClass(&wndClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
#ifdef _DEBUG
        MessageBox(nullptr, "Failed to register window class", "Error", MB_OK | MB_ICONERROR);
#endif
        return nullptr;
    }

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    tagRECT rc;
    SetRect(&rc, 0, 0, width, height);
    if (fullscreen)
        style = WS_POPUP | WS_CLIPCHILDREN;
    AdjustWindowRect(&rc, style, FALSE);

    HWND window = CreateWindowEx(
        0,
        windowClassName, windowName,
        style,
        0, 0,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr,
        module, nullptr);

    if (!window)
    {
        GetLastError(); // TODO: This is probably handled in the internal build
        return nullptr;
    }

    DisableSystemBackdrop(window);

    HDEVNOTIFY notify = nullptr;
    return RegisterDeviceNotif(&notify) ? window : nullptr;
}

void CgsSystem::HardwareInit::InitializeHardware(const char *lpCmdLine)
{
    bool fullscreen = renderengine::gFullscreen;
    s32 width = renderengine::gDisplayWidth;
    s32 height = renderengine::gDisplayHeight;
    timeBeginPeriod(1u);

#ifdef NDEBUG
    // TODO: This should only be for distribution builds
    //       and should use a custom define that is separate from Release/Debug configurations
    if (D3DPERF_GetStatus())
    {
        MessageBox(NULL, "Debugger detected. This application will now quit.", "ERROR", MB_OK);
        exit(0);
    }

    if (IsDebuggerPresent())
    {
        MessageBox(NULL, "Debugger detected. This application will now quit.", "ERROR", MB_OK);
        exit(0);
    }
#endif

    CheckSSE2Support();

    // TODO: A bunch of CgsCore::StrCpy and CgsCore::StrCat got inlined here, we should make it use those functions
    char pathBuffer[knHardwarePathMaxLength];
    GetCurrentDirectory(MAX_PATH, pathBuffer);
    s32 index = 0;
    char c;
    do
    {
        c = pathBuffer[index];
        macFOPENPath[index++] = c;
    } while (c);

    auto len = strlen(macFOPENPath);
    char c2 = macFOPENPath[len - 1];
    // ensure the path ends with a backslash
    if (c2 != '\\' && c2 != '/')
    {
        macFOPENPath[len] = '\\';
        macFOPENPath[len + 1] = '\0';
    }

    for (s32 i = 0; pathBuffer[i] != 0; i++)
    {
        char* c3 = &pathBuffer[i];
        // Convert backslashes to forward slashes
        if (pathBuffer[i] == '\\')
            *c3 = '/';

        // Remove colons from the path
        if (*c3 == ':')
        {
            memcpy(&pathBuffer[i], &pathBuffer[i + 1], MAX_PATH - i);
            --i; // Adjust index to account for the removed character
        }
    }

    CgsCore::SnPrintf(macRootPath, knHardwarePathMaxLength, "p_hdd:/%s/", pathBuffer);

    // TODO: Implement CgsSystem::HardwareInit::InitializeHardware

    isPartyEdition = true;
    isPartyEdition = DetermineIsPartyEditionVersion();

    mbHardwareRequestsShutdown = false;

    deviceChanged = false;
    deviceChangedSinceLaunch = false;

    mouseX = 0;
    mouseY = 0;
    leftMouseDown = false;

    // TODO: Implement CgsSystem::HardwareInit::InitializeHardware

    s32 j = 0;
    char c4 = 0;
    char buffer[1024];
    do
    {
        c4 = macFOPENPath[j];
        buffer[++j] = c4;
    } while (c4);

    char* p = buffer;
    while (*++p);

    strcpy(p, kDefaultAutoTestScript);
    FILE* file = fopen(&buffer[1], "r");
    if (file)
    {
        fgets(macAutoTestScriptToRun, sizeof(macAutoTestScriptToRun), file);
        mbHasDetectedAutomaticTestingFile = true;
        fclose(file);
    }
    else
    {
        mbHasDetectedAutomaticTestingFile = false;
        const char* autoTestCmd = strstr(lpCmdLine, autoTestCmdPrefix);
        if (autoTestCmd)
        {
            const char* ch2;
            const char* ch = &autoTestCmd[strlen(autoTestCmdPrefix)];
            if (*ch == '"')
                ch2 = strstr(ch + 1, "\"");
            else
                ch2 = strstr(ch + 1, " ");

            if (!ch2)
                ch2 = &lpCmdLine[strlen(lpCmdLine)];

            s32 length = ch2 - ch;
            strncpy(macAutoTestScriptToRun, ch, length);
            macAutoTestScriptToRun[length] = '\0';
            mbHasDetectedAutomaticTestingFile = true;
        }
    }

    gEnableMultiThreading = strstr(lpCmdLine, "-multithread") != nullptr;

    OSVERSIONINFO versionInfo;
    ZeroMemory(&versionInfo, sizeof(versionInfo));
    versionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&versionInfo);
    if (*(DWORD*)versionInfo.szCSDVersion >= 6)
        gEnableMultiThreading = true; // Enable multithreading on Vista and later

    renderengine::hWnd = CreateGameWindow(width, height, fullscreen);
    SetSystemParameters(false);
    hCursor = SetCursor(nullptr);
    ShowCursor(FALSE);
    CoInitialize(nullptr);

    // TODO: Implement CgsSystem::HardwareInit::InitializeHardware
}

void CgsSystem::HardwareInit::ReleaseHardware()
{
    SetCursor(hCursor);
    ShowCursor(TRUE);

    // TODO: Implement CgsSystem::HardwareInit::ReleaseHardware

    SetSystemParameters(true);

    // TODO: Implement CgsSystem::HardwareInit::ReleaseHardware

    timeEndPeriod(1u);
    CoUninitialize();
}

bool CgsSystem::HardwareInit::IsAlreadyRunning()
{
    HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, mutexName);
    GetLastError(); // TODO: This is probably handled in the internal build
    if (hMutex)
    {
        HWND existingWindow = FindWindow(windowClassName, windowName);
        if (existingWindow)
            ShowWindow(existingWindow, SW_SHOWNORMAL);

        return TRUE; // Another instance is already running
    }

    // No existing mutex, create a new one
    CreateMutex(nullptr, FALSE, mutexName);
    return FALSE;
}

bool CgsSystem::HardwareInit::IsHardDiskAvailable()
{
    return true;
}

bool CgsSystem::HardwareInit::DetermineIsPartyEditionVersion()
{
    char FileName[1024];

    CgsCore::SnPrintf(FileName, sizeof(FileName), "%s%s", GetFOPENDirectory(), "PARTY.DAT");

    FILE* file = fopen(FileName, "r");
    if (file)
    {
        fclose(file);
        return true;
    }

    return false;
}


