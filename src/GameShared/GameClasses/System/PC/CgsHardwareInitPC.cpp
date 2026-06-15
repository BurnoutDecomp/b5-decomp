#include <cstdio>

#include "GameShared/GameClasses/System/CgsHardwareInit.h"

#include <Windows.h>
#include <d3d9.h>

#include "GameShared/GameClasses/Core/CgsStringUtils.h"
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

static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        // User closed the window: tear it down (-> WM_DESTROY).
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        // Post WM_QUIT so EngineUpdate's message loop exits and WinMain returns (otherwise the
        // loop spins forever on the destroyed window and the process lingers).
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
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
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
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


