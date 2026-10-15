// Fixture for run_hardware_init_helpers.py: the production SetSystemParameters and
// RegisterDeviceNotif bodies run against a modelled Win32 settings store and a recording
// RegisterDeviceNotificationA. No system setting is touched.
#define NOMINMAX
#include <Windows.h>
#include <Dbt.h>
#include <cstdio>
#include <cstring>

namespace Fake
{
    struct Settings
    {
        STICKYKEYS sticky;
        TOGGLEKEYS toggle;
        FILTERKEYS filter;
        BOOL screenSaver;
    };

    Settings gSystem;
    int giGets = 0;
    int giSets = 0;
    int giBadCalls = 0;   // wrong uiParam/cbSize, non-zero fWinIni or an unexpected action

    BOOL WINAPI SystemParametersInfoA(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni)
    {
        if (fWinIni != 0)
            ++giBadCalls;
        switch (uiAction)
        {
        case SPI_GETSTICKYKEYS:
        case SPI_SETSTICKYKEYS:
        {
            STICKYKEYS* p = static_cast<STICKYKEYS*>(pvParam);
            if (uiParam != sizeof(STICKYKEYS) || p->cbSize != sizeof(STICKYKEYS))
                ++giBadCalls;
            if (uiAction == SPI_GETSTICKYKEYS) { *p = gSystem.sticky; ++giGets; }
            else { gSystem.sticky = *p; ++giSets; }
            return TRUE;
        }
        case SPI_GETTOGGLEKEYS:
        case SPI_SETTOGGLEKEYS:
        {
            TOGGLEKEYS* p = static_cast<TOGGLEKEYS*>(pvParam);
            if (uiParam != sizeof(TOGGLEKEYS) || p->cbSize != sizeof(TOGGLEKEYS))
                ++giBadCalls;
            if (uiAction == SPI_GETTOGGLEKEYS) { *p = gSystem.toggle; ++giGets; }
            else { gSystem.toggle = *p; ++giSets; }
            return TRUE;
        }
        case SPI_GETFILTERKEYS:
        case SPI_SETFILTERKEYS:
        {
            FILTERKEYS* p = static_cast<FILTERKEYS*>(pvParam);
            if (uiParam != sizeof(FILTERKEYS) || p->cbSize != sizeof(FILTERKEYS))
                ++giBadCalls;
            if (uiAction == SPI_GETFILTERKEYS) { *p = gSystem.filter; ++giGets; }
            else { gSystem.filter = *p; ++giSets; }
            return TRUE;
        }
        case SPI_GETSCREENSAVEACTIVE:
            if (pvParam == nullptr)
                ++giBadCalls;
            else
                *static_cast<BOOL*>(pvParam) = gSystem.screenSaver;
            ++giGets;
            return TRUE;
        case SPI_SETSCREENSAVEACTIVE:
            if (pvParam != nullptr)
                ++giBadCalls;
            gSystem.screenSaver = uiParam ? TRUE : FALSE;
            ++giSets;
            return TRUE;
        default:
            ++giBadCalls;
            return FALSE;
        }
    }

    HDEVNOTIFY gResult = nullptr;
    int giRegisterCalls = 0;
    HANDLE gRecipient = nullptr;
    DWORD guFlags = 0xFFFFFFFF;
    DEV_BROADCAST_DEVICEINTERFACE_A gFilter;
    DWORD guFilterSize = 0;

    HDEVNOTIFY WINAPI RegisterDeviceNotificationA(HANDLE hRecipient, LPVOID notificationFilter, DWORD flags)
    {
        ++giRegisterCalls;
        gRecipient = hRecipient;
        guFlags = flags;
        guFilterSize = static_cast<DEV_BROADCAST_HDR*>(notificationFilter)->dbch_size;
        std::memcpy(&gFilter, notificationFilter, sizeof(gFilter));
        return gResult;
    }
}

#define SystemParametersInfoA Fake::SystemParametersInfoA
#define RegisterDeviceNotificationA Fake::RegisterDeviceNotificationA

// Each namespace holds a fresh copy of the production bodies, so each scenario starts
// with its own never-retrieved snapshot.
namespace RetrieveOnce {
#include "hardware_init_helpers.inc"
}
namespace RestoreFirst {
#include "hardware_init_helpers.inc"
}
namespace ScreenSaverOff {
#include "hardware_init_helpers.inc"
}

#undef SystemParametersInfoA
#undef RegisterDeviceNotificationA

static int giChecks = 0;
static int giFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass)
        ++giFailures;
    std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcName);
}

static Fake::Settings UserSettings(BOOL lbScreenSaver)
{
    Fake::Settings l;
    l.sticky = { sizeof(STICKYKEYS), 0x1FE };
    l.toggle = { sizeof(TOGGLEKEYS), 0x3E };
    l.filter = { sizeof(FILTERKEYS), 0x7E, 1000, 500, 300, 20 };
    l.screenSaver = lbScreenSaver;
    return l;
}

static bool SameFilter(const FILTERKEYS& a, const FILTERKEYS& b)
{
    return a.cbSize == b.cbSize && a.dwFlags == b.dwFlags && a.iWaitMSec == b.iWaitMSec &&
           a.iDelayMSec == b.iDelayMSec && a.iRepeatMSec == b.iRepeatMSec && a.iBounceMSec == b.iBounceMSec;
}

static bool SameSettings(const Fake::Settings& a, const Fake::Settings& b)
{
    return a.sticky.cbSize == b.sticky.cbSize && a.sticky.dwFlags == b.sticky.dwFlags &&
           a.toggle.cbSize == b.toggle.cbSize && a.toggle.dwFlags == b.toggle.dwFlags &&
           SameFilter(a.filter, b.filter) && a.screenSaver == b.screenSaver;
}

static void ResetFake(const Fake::Settings& lSystem)
{
    Fake::gSystem = lSystem;
    Fake::giGets = Fake::giSets = Fake::giBadCalls = 0;
}

static bool IsApplied(const Fake::Settings& lUser)
{
    FILTERKEYS lFilter = lUser.filter;
    lFilter.dwFlags &= 0xFFFFFFF3u;
    return Fake::gSystem.sticky.dwFlags == (lUser.sticky.dwFlags & 0xFFFFFFF3u) &&
           Fake::gSystem.toggle.dwFlags == (lUser.toggle.dwFlags & 0xFFFFFFF3u) &&
           SameFilter(Fake::gSystem.filter, lFilter) && Fake::gSystem.screenSaver == FALSE;
}

int main()
{
    // Startup snapshots once, applies, and a later restore returns the snapshot even when
    // the live settings changed in between.
    {
        const Fake::Settings lUser = UserSettings(TRUE);
        ResetFake(lUser);
        RetrieveOnce::SetSystemParameters(false);
        Check(Fake::giGets == 4, "startup: four settings retrieved");
        Check(Fake::giSets == 4, "startup: four settings applied");
        Check(IsApplied(lUser), "startup: hotkey-active/confirm bits cleared, other fields kept, screensaver off");

        Fake::gSystem.sticky.dwFlags = 0x3;
        Fake::gSystem.filter.iWaitMSec = 7;
        RetrieveOnce::SetSystemParameters(false);
        Check(Fake::giGets == 4, "second apply: snapshot is not retrieved again");
        Check(IsApplied(lUser), "second apply: applies from the first snapshot");

        RetrieveOnce::SetSystemParameters(true);
        Check(Fake::giGets == 4, "restore: snapshot is not retrieved again");
        Check(SameSettings(Fake::gSystem, lUser), "restore: every snapshot field written back, screensaver on");
        Check(Fake::giBadCalls == 0, "all calls: sizes, cbSize, fWinIni 0 and actions as expected");
    }

    // A restore before any apply (the shutdown watchdog firing early) leaves the user's
    // settings exactly as they were.
    {
        const Fake::Settings lUser = UserSettings(TRUE);
        ResetFake(lUser);
        RestoreFirst::SetSystemParameters(true);
        Check(Fake::giGets == 4 && SameSettings(Fake::gSystem, lUser), "restore first: settings unchanged");
        RestoreFirst::SetSystemParameters(false);
        Check(IsApplied(lUser), "restore first: a later apply still masks");
        Check(Fake::giBadCalls == 0, "restore first: no malformed calls");
    }

    // A user whose screensaver was off gets it back off.
    {
        const Fake::Settings lUser = UserSettings(FALSE);
        ResetFake(lUser);
        ScreenSaverOff::SetSystemParameters(false);
        Fake::gSystem.screenSaver = TRUE;
        ScreenSaverOff::SetSystemParameters(true);
        Check(Fake::gSystem.screenSaver == FALSE && SameSettings(Fake::gSystem, lUser), "screensaver off: restored off");
    }

    // Device notification success: the window is the recipient and the returned handle is
    // written and reported.
    {
        const HWND lWindow = reinterpret_cast<HWND>(static_cast<UINT_PTR>(0x5678));
        Fake::gResult = reinterpret_cast<HDEVNOTIFY>(static_cast<UINT_PTR>(0x1234));
        Fake::giRegisterCalls = 0;
        HDEVNOTIFY lNotify = reinterpret_cast<HDEVNOTIFY>(static_cast<UINT_PTR>(0xDEAD));
        const bool lbResult = RetrieveOnce::RegisterDeviceNotif(lWindow, &lNotify);
        Check(lbResult && Fake::giRegisterCalls == 1, "notify success: returns true after one registration");
        Check(lNotify == Fake::gResult, "notify success: handle written");
        Check(Fake::gRecipient == lWindow, "notify success: window handle is the recipient");
        Check(Fake::guFlags == (DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES),
              "notify success: window-handle recipient, all interface classes");
        GUID lZero = {};
        Check(Fake::guFilterSize == 32 && Fake::gFilter.dbcc_size == sizeof(DEV_BROADCAST_DEVICEINTERFACE_A) &&
              Fake::gFilter.dbcc_devicetype == DBT_DEVTYP_DEVICEINTERFACE && Fake::gFilter.dbcc_reserved == 0 &&
              std::memcmp(&Fake::gFilter.dbcc_classguid, &lZero, sizeof(GUID)) == 0 && Fake::gFilter.dbcc_name[0] == 0,
              "notify success: 32-byte device-interface filter, zeroed class and name");
    }

    // Device notification failure: null is written and reported as failure.
    {
        const HWND lWindow = reinterpret_cast<HWND>(static_cast<UINT_PTR>(0x5678));
        Fake::gResult = nullptr;
        HDEVNOTIFY lNotify = reinterpret_cast<HDEVNOTIFY>(static_cast<UINT_PTR>(0xDEAD));
        const bool lbResult = RestoreFirst::RegisterDeviceNotif(lWindow, &lNotify);
        Check(!lbResult, "notify failure: returns false");
        Check(lNotify == nullptr, "notify failure: null handle written");
    }

    std::printf("HardwareInitHelpers: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
