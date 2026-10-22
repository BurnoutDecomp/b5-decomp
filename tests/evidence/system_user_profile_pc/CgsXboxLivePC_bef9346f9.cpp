// ============================================================================
// CgsXboxLivePC.cpp -- PC bodies for the Xbox system-software (X* / XAM) calls the game reaches.
//
// [PC platform leaf] Every function here is a console platform API with no game body to
// reconstruct. The callers declare them extern "C" in local blocks (one C name, so ONE
// definition serves every declaration). Each returns the answer the console gives for:
//   OFFLINE (default)  a profile signed in locally, no network service, no camera, no invites;
//   LAN (default)      the local profile connected to the supported PC LAN service,
//                      with the shared PC identity (BP_LAN=0 opts out).
//                      (CgsPcNetIdentity.h) as its gamertag and XUID.
// Win32 error codes used: 0 ERROR_SUCCESS, 18 ERROR_NO_MORE_FILES, 87 ERROR_INVALID_PARAMETER.
// The keyboard below is explicitly native PC policy, including cancellation;
// it does not claim the unavailable XAM keyboard flag/layout/status contract.
// ============================================================================

// The native controls need USER/GDI declarations before any lean engine headers.
#ifdef NOUSER
#undef NOUSER
#endif
#ifdef NOGDI
#undef NOGDI
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "types.hpp"
#include "GameShared/GameClasses/System/CgsXOverlapped.h"   // XOVERLAPPED
#include "GameShared/GameClasses/System/PC/CgsKeyboardPC.h"
#include "GameShared/GameClasses/System/PC/CgsPcNetIdentity.h"

#include <cstring>
#include <atomic>
#include <memory>
#include <mutex>
#include <new>
#include <string>

// FLAG PC-platform leaf: one native Unicode EDIT window, driven by the existing
// host message pump. This is explicit PC UI/status policy, not recovered XAM.
namespace CgsSystem { namespace KeyboardPC
{
    namespace
    {
        struct Operation
        {
            XOVERLAPPED* mpOverlapped = nullptr;
            u16* mpResult = nullptr;
            u32 muCapacity = 0;
            std::wstring mDefault, mTitle, mPrompt, mAccepted;
            u32 muStatus = ERROR_IO_PENDING;
            bool mbPublished = false;
            bool mbAbandoned = false;
        };
        std::mutex sMutex;
        std::shared_ptr<Operation> spOperation;
        std::atomic<bool> sbCapture(false), sbVisible(false);
        bool sbNeedRelease = false;       // protected by sMutex
        bool sbDebugControlsNeutral = false; // protected by sMutex; fresh after each release latch
        bool sbOwnerClosing = false;      // protected by sMutex
        HWND spOwner = nullptr;           // only the main window thread uses HWNDs
        HWND spWindow = nullptr;
        std::shared_ptr<Operation> spUiOperation; // main window thread only
        const wchar_t KAC_WINDOW_CLASS[] = L"BurnoutPCKeyboard";
        const int KI_EDIT = 101;

        bool KeysReleased()
        {
            for (int liKey = 1; liKey < 256; ++liKey)
                if ((GetAsyncKeyState(liKey) & 0x8000) != 0) return false;
            return true;
        }

        std::wstring CopyText(const u16* lpText, u32 luMaximum = 0xffffffffu)
        {
            std::wstring lText;
            if (lpText != nullptr)
                for (u32 lu = 0; lu < luMaximum && lpText[lu] != 0; ++lu)
                    lText.push_back(static_cast<wchar_t>(lpText[lu]));
            return lText;
        }

        bool ValidUtf16(const std::wstring& lrText)
        {
            for (size_t lu = 0; lu < lrText.size(); ++lu)
            {
                const u16 luUnit = static_cast<u16>(lrText[lu]);
                if (luUnit >= 0xd800 && luUnit <= 0xdbff)
                {
                    if (++lu == lrText.size() || lrText[lu] < 0xdc00 || lrText[lu] > 0xdfff) return false;
                }
                else if (luUnit >= 0xdc00 && luUnit <= 0xdfff) return false;
            }
            return true;
        }

        u32 NativeFailure()
        {
            const DWORD luError = GetLastError();
            return luError != ERROR_SUCCESS ? static_cast<u32>(luError) : ERROR_GEN_FAILURE;
        }

        // Caller holds sMutex; only API callers publish to their live title storage.
        // Native window callbacks only change owned Operation data. API publication
        // may also run on the main thread in the existing serial frame mode.
        // Terminal data precedes status.
        void Publish(Operation& lrOperation)
        {
            if (lrOperation.mbPublished || lrOperation.mbAbandoned || lrOperation.muStatus == ERROR_IO_PENDING) return;
            if (lrOperation.muStatus == ERROR_SUCCESS)
            {
                for (size_t lu = 0; lu < lrOperation.mAccepted.size(); ++lu)
                    lrOperation.mpResult[lu] = static_cast<u16>(lrOperation.mAccepted[lu]);
                lrOperation.mpResult[lrOperation.mAccepted.size()] = 0;
            }
            lrOperation.mpOverlapped->InternalHigh = 0; // PC policy: this keyboard supplies no byte-count result.
            lrOperation.mpOverlapped->dwExtendedError = lrOperation.muStatus;
            lrOperation.mpOverlapped->InternalLow = lrOperation.muStatus;
            lrOperation.mbPublished = true;
        }

        void Complete(const std::shared_ptr<Operation>& lpOperation, u32 luStatus, std::wstring* lpAccepted = nullptr)
        {
            std::lock_guard<std::mutex> lLock(sMutex);
            if (spOperation != lpOperation || lpOperation->mbAbandoned || lpOperation->muStatus != ERROR_IO_PENDING) return;
            if (lpAccepted != nullptr) lpOperation->mAccepted.swap(*lpAccepted);
            lpOperation->muStatus = luStatus;
            sbNeedRelease = true;
            sbDebugControlsNeutral = false;
        }

        LRESULT CALLBACK KeyboardProc(HWND lpWindow, UINT luMessage, WPARAM luWParam, LPARAM liLParam)
        {
            const std::shared_ptr<Operation> lpOperation = spUiOperation;
            try
            {
                switch (luMessage)
                {
                case WM_CREATE:
                {
                    const HINSTANCE lpModule = GetModuleHandleW(nullptr);
                    HWND laControls[4] = {};
                    laControls[0] = CreateWindowExW(0, L"EDIT", lpOperation->mPrompt.c_str(),
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                            16, 12, 472, 48, lpWindow, nullptr, lpModule, nullptr);
                    if (!laControls[0]) { Complete(lpOperation, NativeFailure()); return -1; }
                    laControls[1] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                            16, 66, 472, 26, lpWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(KI_EDIT)), lpModule, nullptr);
                    if (!laControls[1]) { Complete(lpOperation, NativeFailure()); return -1; }
                    laControls[2] = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                            300, 112, 88, 28, lpWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), lpModule, nullptr);
                    if (!laControls[2]) { Complete(lpOperation, NativeFailure()); return -1; }
                    laControls[3] = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                            400, 112, 88, 28, lpWindow, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), lpModule, nullptr);
                    if (!laControls[3]) { Complete(lpOperation, NativeFailure()); return -1; }
                    for (HWND lpControl : laControls)
                    {
                        SendMessageW(lpControl, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
                    }
                    SendMessageW(laControls[1], EM_LIMITTEXT, lpOperation->muCapacity - 1, 0);
                    if (!SetWindowTextW(laControls[1], lpOperation->mDefault.c_str()))
                    { Complete(lpOperation, NativeFailure()); return -1; }
                    SendMessageW(laControls[1], EM_SETSEL, 0, -1); // PC policy: select the supplied default for editing.
                    return 0;
                }
                case DM_GETDEFID:
                    return MAKELRESULT(IDOK, DC_HASDEFID);
                case WM_COMMAND:
                    if (LOWORD(luWParam) == IDOK && HIWORD(luWParam) == BN_CLICKED)
                    {
                        wchar_t laText[512] = {};
                        SetLastError(ERROR_SUCCESS);
                        const int liCount = GetWindowTextW(GetDlgItem(lpWindow, KI_EDIT), laText, lpOperation->muCapacity);
                        if (liCount == 0 && GetLastError() != ERROR_SUCCESS) Complete(lpOperation, NativeFailure());
                        else
                        {
                            std::wstring lAccepted(laText, static_cast<size_t>(liCount));
                            if (ValidUtf16(lAccepted)) Complete(lpOperation, ERROR_SUCCESS, &lAccepted);
                            else Complete(lpOperation, ERROR_NO_UNICODE_TRANSLATION);
                        }
                        DestroyWindow(lpWindow);
                        return 0;
                    }
                    if (LOWORD(luWParam) != IDCANCEL || HIWORD(luWParam) != BN_CLICKED) break;
                    // Fall through to the native Cancel/X close path.
                case WM_CLOSE:
                    Complete(lpOperation, ERROR_CANCELLED);
                    DestroyWindow(lpWindow);
                    return 0;
                case WM_DESTROY:
                    Complete(lpOperation, ERROR_OPERATION_ABORTED);
                    return 0;
                case WM_NCDESTROY:
                    spWindow = nullptr;
                    spUiOperation.reset();
                    sbVisible.store(false, std::memory_order_release);
                    return DefWindowProcW(lpWindow, luMessage, luWParam, liLParam);
                }
            }
            catch (const std::bad_alloc&)
            {
                Complete(lpOperation, ERROR_NOT_ENOUGH_MEMORY);
                if (luMessage == WM_CREATE) return -1;
                DestroyWindow(lpWindow);
                return 0;
            }
            return DefWindowProcW(lpWindow, luMessage, luWParam, liLParam);
        }
    }

    bool IsCapturingInput() { return sbCapture.load(std::memory_order_acquire); }

    bool BlockGameInput(bool lbControlsNeutral)
    {
        if (!IsCapturingInput()) return false;
        std::lock_guard<std::mutex> lLock(sMutex);
        if (sbNeedRelease && lbControlsNeutral && sbDebugControlsNeutral && KeysReleased()) sbNeedRelease = false;
        if (!sbNeedRelease && !sbVisible.load(std::memory_order_acquire) &&
            (!spOperation || spOperation->muStatus != ERROR_IO_PENDING))
            sbCapture.store(false, std::memory_order_release);
        return true; // The release frame is consumed too; held closing input never reaches the game.
    }

    bool BlockDebugInput(bool lbControlsNeutral)
    {
        if (!IsCapturingInput()) return false;
        std::lock_guard<std::mutex> lLock(sMutex);
        sbDebugControlsNeutral = lbControlsNeutral;
        return true; // Only BlockGameInput releases capture, after both input owners are neutral.
    }

    void Forget(XOVERLAPPED* lpOverlapped)
    {
        std::lock_guard<std::mutex> lLock(sMutex);
        if (spOperation && spOperation->mpOverlapped == lpOverlapped)
        {
            spOperation->mbAbandoned = true;
            spOperation->mpOverlapped = nullptr;
            spOperation->mpResult = nullptr;
            spOperation.reset();
            sbNeedRelease = true;
            sbDebugControlsNeutral = false;
        }
    }

    void Pump(HWND__* lpOwner)
    {
        std::shared_ptr<Operation> lpOperation;
        bool lbCreate = false, lbClose = false;
        {
            std::lock_guard<std::mutex> lLock(sMutex);
            spOwner = lpOwner;
            lpOperation = spOperation;
            lbClose = spWindow != nullptr && (sbOwnerClosing || spUiOperation != lpOperation ||
                      !lpOperation || lpOperation->muStatus != ERROR_IO_PENDING);
            lbCreate = !sbOwnerClosing && lpOperation && lpOperation->muStatus == ERROR_IO_PENDING && !sbNeedRelease;
        }
        if (lbClose) DestroyWindow(spWindow); // Pump is on the creating thread; no mutex crosses a Win32 call.
        if (!lbCreate || spWindow != nullptr) return;
        DWORD luProcess = 0;
        if (!IsWindow(lpOwner) || GetWindowThreadProcessId(lpOwner, &luProcess) != GetCurrentThreadId() ||
            luProcess != GetCurrentProcessId())
        { Complete(lpOperation, ERROR_INVALID_WINDOW_HANDLE); return; }
        WNDCLASSW lClass = {};
        lClass.lpfnWndProc = KeyboardProc;
        lClass.hInstance = GetModuleHandleW(nullptr);
        lClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        lClass.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1));
        lClass.lpszClassName = KAC_WINDOW_CLASS;
        if (!RegisterClassW(&lClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        { Complete(lpOperation, NativeFailure()); return; }
        RECT lOwnerRect = {};
        GetWindowRect(lpOwner, &lOwnerRect);
        RECT lRect = {0, 0, 504, 156};
        const DWORD luStyle = WS_POPUP | WS_CAPTION | WS_SYSMENU;
        AdjustWindowRectEx(&lRect, luStyle, FALSE, WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT);
        spUiOperation = lpOperation;
        spWindow = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, KAC_WINDOW_CLASS,
            lpOperation->mTitle.c_str(), luStyle, lOwnerRect.left + 24, lOwnerRect.top + 24,
            lRect.right - lRect.left, lRect.bottom - lRect.top, lpOwner, nullptr, lClass.hInstance, nullptr);
        if (spWindow == nullptr)
        { Complete(lpOperation, NativeFailure()); spUiOperation.reset(); return; }
        sbVisible.store(true, std::memory_order_release);
        ShowWindow(spWindow, SW_SHOW);
        SetFocus(GetDlgItem(spWindow, KI_EDIT));
    }

    bool ProcessMessage(tagMSG* lpMessage)
    {
        return spWindow != nullptr && IsDialogMessageW(spWindow, lpMessage) != FALSE;
    }

    void OwnerClosing(HWND__* lpOwner)
    {
        {
            std::lock_guard<std::mutex> lLock(sMutex);
            if (spOwner != nullptr && spOwner != lpOwner) return;
            sbOwnerClosing = true;
            if (spOperation)
            {
                spOperation->mbAbandoned = true;
                spOperation->mpOverlapped = nullptr;
                spOperation->mpResult = nullptr;
                spOperation.reset();
            }
            sbNeedRelease = true;
            sbDebugControlsNeutral = false;
        }
        if (spWindow != nullptr) DestroyWindow(spWindow);
    }

    bool TryResult(XOVERLAPPED* lpOverlapped, u32* lpResult, u32& lrStatus)
    {
        std::lock_guard<std::mutex> lLock(sMutex);
        if (!spOperation || spOperation->mpOverlapped != lpOverlapped) return false;
        // The supported original polling record has hEvent==0: ARTIST returns996
        // while pending even if bWait is true. No guest32 word is cast to HANDLE.
        if (spOperation->muStatus == ERROR_IO_PENDING) lrStatus = ERROR_IO_INCOMPLETE;
        else
        {
            Publish(*spOperation);
            if (lpResult != nullptr) *lpResult = lpOverlapped->InternalHigh;
            lrStatus = spOperation->muStatus;
        }
        return true;
    }

    bool TryCancel(XOVERLAPPED* lpOverlapped, u32& lrStatus)
    {
        std::lock_guard<std::mutex> lLock(sMutex);
        if (!spOperation || spOperation->mpOverlapped != lpOverlapped) return false;
        if (spOperation->muStatus == ERROR_IO_PENDING)
        { spOperation->muStatus = ERROR_CANCELLED; sbNeedRelease = true; sbDebugControlsNeutral = false; }
        Publish(*spOperation); // Logical I/O cancellation is complete before return; Pump closes owned UI.
        lrStatus = ERROR_SUCCESS;
        return true;
    }

    u32 Begin(u32 luUser, u32 luFlags, const u16* lpDefault, const u16* lpTitle,
              const u16* lpPrompt, u16* lpResult, u32 luCapacity, XOVERLAPPED* lpOverlapped)
    {
        static_assert(sizeof(wchar_t) == 2, "Win32 Unicode controls use UTF16 code units");
        u32 luError = ERROR_SUCCESS;
        std::shared_ptr<Operation> lpOperation;
        try
        {
            if (lpOverlapped == nullptr) return ERROR_INVALID_PARAMETER;
            if (lpResult == nullptr || luCapacity != 512 || luUser >= 4) luError = ERROR_INVALID_PARAMETER;
            else if (luFlags != 0x30000064u || lpOverlapped->InternalContext != 0 ||
                     lpOverlapped->hEvent != 0 || lpOverlapped->pCompletionRoutine != 0 ||
                     lpOverlapped->dwCompletionContext != 0) luError = ERROR_NOT_SUPPORTED;
            else
            {
                lpOperation = std::make_shared<Operation>();
                lpOperation->mpOverlapped = lpOverlapped;
                lpOperation->mpResult = lpResult;
                lpOperation->muCapacity = luCapacity;
                lpOperation->mDefault = CopyText(lpDefault, luCapacity - 1);
                if (lpOperation->mDefault.size() == luCapacity - 1 && lpOperation->mDefault.back() >= 0xd800 &&
                    lpOperation->mDefault.back() <= 0xdbff) lpOperation->mDefault.pop_back();
                lpOperation->mTitle = CopyText(lpTitle);
                lpOperation->mPrompt = CopyText(lpPrompt);
                if (!ValidUtf16(lpOperation->mDefault) || !ValidUtf16(lpOperation->mTitle) || !ValidUtf16(lpOperation->mPrompt))
                    luError = ERROR_NO_UNICODE_TRANSLATION;
            }
        }
        catch (const std::bad_alloc&) { luError = ERROR_NOT_ENOUGH_MEMORY; }
        std::lock_guard<std::mutex> lLock(sMutex);
        if (spOperation && (spOperation->muStatus == ERROR_IO_PENDING || !spOperation->mbPublished))
        {
            if (spOperation->mpOverlapped == lpOverlapped) return ERROR_BUSY; // Never overwrite another in-flight call.
            luError = ERROR_BUSY;
        }
        if (sbOwnerClosing) luError = ERROR_OPERATION_ABORTED;
        if (luError != ERROR_SUCCESS)
        {
            // A failed new request must not retain a completed result at the same address.
            // The live/unpublished case above is still BUSY and keeps its original identity.
            if (spOperation && spOperation->mpOverlapped == lpOverlapped)
            {
                spOperation->mbAbandoned = true;
                spOperation->mpOverlapped = nullptr;
                spOperation->mpResult = nullptr;
                spOperation.reset();
            }
            lpOverlapped->InternalHigh = 0;
            lpOverlapped->dwExtendedError = luError;
            lpOverlapped->InternalLow = luError;
            return luError;
        }
        spOperation = lpOperation;
        lpOverlapped->InternalHigh = 0;
        lpOverlapped->dwExtendedError = 0;
        lpOverlapped->InternalLow = ERROR_IO_PENDING;
        sbNeedRelease = true; // Do not reuse the game's opening key/button inside the native UI.
        sbDebugControlsNeutral = false;
        sbCapture.store(true, std::memory_order_release);
        return ERROR_IO_PENDING;
    }
} }

extern "C" u32 XShowKeyboardUI(u32 luUser, u32 luFlags, const u16* lpDefault, const u16* lpTitle,
                               const u16* lpPrompt, u16* lpResult, u32 luCapacity, PXOVERLAPPED lpOverlapped)
{
    return CgsSystem::KeyboardPC::Begin(luUser, luFlags, lpDefault, lpTitle, lpPrompt, lpResult, luCapacity, lpOverlapped);
}

namespace
{
    const u32 KU_ERROR_SUCCESS           = 0u;
    const u32 KU_ERROR_NO_MORE_FILES     = 18u;
    const u32 KU_ERROR_INVALID_PARAMETER = 87u;
    const u32 KU_ERROR_IO_INCOMPLETE     = 996u;
    const u32 KU_ERROR_IO_PENDING        = 997u;
    const u32 KU_ERROR_FUNCTION_FAILED   = 1627u;
    const u32 KU_ERROR_NOT_FOUND         = 1168u;

    // Sign-in states: 0 not signed in, 1 signed in locally, 2 signed in to the service.
    const u32 KU_SIGNIN_STATE_LOCAL   = 1u;
    const u32 KU_SIGNIN_STATE_SERVICE = 2u;

    // The 40-byte sign-in info block XUserGetSigninInfo fills (callers pass a 40- or 48-byte
    // buffer and read the flags word at +0x08).
    struct SigninInfo
    {
        u64  mu64Xuid;             // +0x00
        u32  muInfoFlags;          // +0x08 -- bit 0 service-enabled, bit 1 guest
        u32  muSigninState;        // +0x0C
        u32  muGuestNumber;        // +0x10
        u32  muSponsorUserIndex;   // +0x14
        char macUserName[16];      // +0x18
    };
    static_assert(sizeof(SigninInfo) == 0x28, "sign-in info block is 40 bytes");

    const u32 KU_SIGNIN_INFO_SERVICE_ENABLED = 0x01u;

    void CopyName(char* lpcOut, u32 luCapacity, const char* lpcName)
    {
        if (lpcOut == 0 || luCapacity == 0u)
            return;
        u32 lu = 0u;
        for (; lu + 1u < luCapacity && lpcName[lu] != '\0'; ++lu)
            lpcOut[lu] = lpcName[lu];
        lpcOut[lu] = '\0';
    }

    // An asynchronous call that finished at once: the completion code lands in the
    // overlapped's status word, where XGetOverlappedResult reads it.
    void CompleteOverlapped(void* lpOverlapped, u32 luResult)
    {
        if (lpOverlapped == 0)
            return;
        XOVERLAPPED* lpXOverlapped = static_cast<XOVERLAPPED*>(lpOverlapped);
        lpXOverlapped->InternalLow     = luResult;
        lpXOverlapped->InternalHigh    = 0u;
        lpXOverlapped->dwExtendedError = 0u;
    }
}

// ---------------------------------------------------------------------------
// Overlapped I/O
// ---------------------------------------------------------------------------

// [PC platform leaf] Polls an asynchronous system call: 996 while it is still pending, else the
// completion code in the overlapped's status word. The PC leaves that start an asynchronous
// call finish it at once (CompleteOverlapped); an overlapped no PC leaf ever used keeps the zero
// its owner's Construct wrote and reads as finished successfully.
extern "C" u32 XGetOverlappedResult(PXOVERLAPPED lpXOverlapped, u32* lpdwResult, u32 /*bWait*/)
{
    u32 luKeyboardStatus = 0;
    if (CgsSystem::KeyboardPC::TryResult(lpXOverlapped, lpdwResult, luKeyboardStatus)) return luKeyboardStatus;
    if (lpXOverlapped == 0)
        return KU_ERROR_SUCCESS;
    if (lpXOverlapped->InternalLow == KU_ERROR_IO_PENDING)
        return KU_ERROR_IO_INCOMPLETE;
    if (lpdwResult != 0)
        *lpdwResult = lpXOverlapped->InternalHigh;
    return lpXOverlapped->InternalLow;
}

// [PC platform leaf] Extended error of a finished asynchronous call (the overlapped's
// extended-error word).
extern "C" u32 XGetOverlappedExtendedError(PXOVERLAPPED lpXOverlapped)
{
    u32 luKeyboardStatus = 0;
    if (CgsSystem::KeyboardPC::TryResult(lpXOverlapped, nullptr, luKeyboardStatus)) return luKeyboardStatus;
    if (lpXOverlapped == 0)
        return KU_ERROR_SUCCESS;
    return lpXOverlapped->dwExtendedError;
}

// [PC platform leaf] Cancels an asynchronous call; 0, nothing is in flight.
extern "C" u32 XCancelOverlapped(void* lpOverlapped)
{
    u32 luKeyboardStatus = 0;
    if (CgsSystem::KeyboardPC::TryCancel(static_cast<XOVERLAPPED*>(lpOverlapped), luKeyboardStatus)) return luKeyboardStatus;
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Reads the next batch from a system enumerator; no enumerator ever exists
// on PC, so the answer is "no more items" with nothing returned.
extern "C" u32 XEnumerate(void* /*lhEnum*/, void* /*lpvBuffer*/, u32 /*lcbBuffer*/,
                          u32* lpcItemsReturned, void* /*lpOverlapped*/)
{
    if (lpcItemsReturned != 0)
        *lpcItemsReturned = 0u;
    return KU_ERROR_NO_MORE_FILES;
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

// [PC platform leaf] The console's block fill; memset on the host.
extern "C" void* XMemSet(void* lpDest, s32 liValue, u32 luCount)
{
    return std::memset(lpDest, liValue, luCount);
}

// ---------------------------------------------------------------------------
// Notifications
// ---------------------------------------------------------------------------

// [PC platform leaf] Creates a system-notification listener. The console always hands back a
// handle (the buddy manager asserts on it and CloseHandle()s it on Destruct), so the PC returns a
// real kernel event that is never signalled: the handle is valid and closable, and XNotifyGetNext
// below never has a notification to dequeue from it.
extern "C" void* XNotifyCreateListener(unsigned long long /*qwAreas*/)
{
    return CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

// [PC platform leaf] Dequeues one system notification; 0 == nothing pending.
extern "C" int XNotifyGetNext(void* /*hListener*/, unsigned long /*dwMsgFilter*/,
                              unsigned long* /*pdwId*/, unsigned long* /*pParam*/)
{
    return 0;
}

// ---------------------------------------------------------------------------
// User / profile
// ---------------------------------------------------------------------------

// [PC platform leaf] Sign-in state of a controller's profile. OFFLINE: 1, signed in locally.
// The PC player always has a local profile, so 0 (no user) would be wrong: it made the pause
// menu's SAVE/LOAD row inert (BrnCrashNavSettings gates it on != 0). Callers read the state
// two ways only: != 0 "is there a user" (save/load, SystemUserProfile) and == 2 "is that user
// on the service" (training, buddies, network adapter, GUI cache, login); 1 opens the first
// and keeps every service path closed. LAN: 2, signed in to the service, which opens the
// online flow (login substate 0 gates on it).
extern "C" u32 XUserGetSigninState(u32 /*luUserIndex*/)
{
    return CgsPcNetLanEnabled() ? KU_SIGNIN_STATE_SERVICE : KU_SIGNIN_STATE_LOCAL;
}

// [PC platform leaf] Privilege query (multiplayer, communications, ...). OFFLINE: 87, the
// caller keeps its running answer. LAN: success with the privilege granted.
extern "C" s32 XUserCheckPrivilege(u32 /*luUserIndex*/, u32 /*luPrivilegeType*/, u32* lpbResult)
{
    if (!CgsPcNetLanEnabled())
        return static_cast<s32>(KU_ERROR_INVALID_PARAMETER);
    if (lpbResult != 0)
        *lpbResult = 1u;
    return static_cast<s32>(KU_ERROR_SUCCESS);
}

// [PC platform leaf] Fills the sign-in info block of a controller's profile. OFFLINE: 87, the
// caller treats the query as failed. LAN: the shared identity, service-enabled, not a guest.
extern "C" s32 XUserGetSigninInfo(u32 /*luUserIndex*/, u32 /*luFlags*/, void* lpSigninInfo)
{
    if (!CgsPcNetLanEnabled() || lpSigninInfo == 0)
        return static_cast<s32>(KU_ERROR_INVALID_PARAMETER);

    SigninInfo lInfo;
    std::memset(&lInfo, 0, sizeof(lInfo));
    lInfo.mu64Xuid      = CgsPcNetIdentityXuid();
    lInfo.muInfoFlags   = KU_SIGNIN_INFO_SERVICE_ENABLED;
    lInfo.muSigninState = KU_SIGNIN_STATE_SERVICE;
    CopyName(lInfo.macUserName, sizeof(lInfo.macUserName), CgsPcNetIdentityName());
    std::memcpy(lpSigninInfo, &lInfo, sizeof(lInfo));
    return static_cast<s32>(KU_ERROR_SUCCESS);
}

// [PC platform leaf] Gamertag of a controller's profile. OFFLINE: success with an empty name.
// LAN: the shared persona.
extern "C" u32 XUserGetName(u32 /*luUserIndex*/, char* lpszUserName, u32 luCchUserName)
{
    CopyName(lpszUserName, luCchUserName, CgsPcNetLanEnabled() ? CgsPcNetIdentityName() : "");
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] XUID of a controller's profile. OFFLINE: 87 with a zero XUID (no online
// identity). LAN: the shared stand-in XUID.
extern "C" u32 XUserGetXUID(u32 /*luUserIndex*/, u64* lpXuid)
{
    if (!CgsPcNetLanEnabled())
    {
        if (lpXuid != 0)
            *lpXuid = 0u;
        return KU_ERROR_INVALID_PARAMETER;
    }
    if (lpXuid != 0)
        *lpXuid = CgsPcNetIdentityXuid();
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Reads the local profile's settings; 87, the caller treats the read as
// failed and keeps its defaults.
extern "C" u32 XUserReadProfileSettings(u32 /*luTitleId*/, u32 /*luUserIndex*/,
                                        u32 /*luNumSettingIds*/, unsigned long* /*lpaSettingIds*/,
                                        unsigned long* /*lpcbResults*/, void* /*lpResults*/,
                                        void* /*lpOverlapped*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Reads other users' profile settings (the gamer-picture keys). The call is
// asynchronous: 997 (started), and the overlapped finishes with 1627, there are no remote
// profiles on PC. The results block is left as the caller cleared it.
extern "C" u32 XUserReadProfileSettingsByXuid(u32 /*luTitleId*/, u32 /*luUserIndexRequester*/,
                                              u32 /*luNumXuids*/, const u64* /*lpaXuids*/,
                                              u32 /*luNumSettingIds*/, const u32* /*lpaSettingIds*/,
                                              u32* /*lpcbResults*/, void* /*lpResults*/,
                                              void* lpOverlapped)
{
    CompleteOverlapped(lpOverlapped, KU_ERROR_FUNCTION_FAILED);
    return KU_ERROR_IO_PENDING;
}

// [PC platform leaf] Downloads a gamer picture into a texture buffer. Asynchronous: 997
// (started), and the overlapped finishes with 1627, there are no gamer pictures on PC.
extern "C" u32 XUserReadGamerPictureByKey(const void* /*lpPictureKey*/, s32 /*lbSmall*/,
                                          u8* /*lpTextureBuffer*/, u32 /*luPitch*/,
                                          u32 /*luHeight*/, void* lpOverlapped)
{
    CompleteOverlapped(lpOverlapped, KU_ERROR_FUNCTION_FAILED);
    return KU_ERROR_IO_PENDING;
}

// [PC platform leaf] Asks whether a remote talker is on the local user's mute list; 0 with
// "not muted", there is no system mute list on PC.
extern "C" u32 XUserMuteListQuery(u32 /*luUserIndex*/, u64 /*luXuidRemoteTalker*/,
                                  s32* lpbOnMuteList)
{
    if (lpbOnMuteList != 0)
        *lpbOnMuteList = 0;
    return KU_ERROR_SUCCESS;
}

// FLAG PC-platform leaf: Xbox profile presence is unavailable on the host;
// the supported PC LAN backend maintains its local matchmaking/session context.
extern "C" void XUserSetContext(u32 /*uUserIndex*/, u32 /*uContextId*/, u32 /*uContextValue*/)
{
}

// ---------------------------------------------------------------------------
// Presence, invites, launch data
// ---------------------------------------------------------------------------

// [PC platform leaf] Creates a presence enumerator for a set of XUIDs; 87, no presence service.
extern "C" u32 XPresenceCreateEnumerator(u32 /*luUserIndex*/, u32 /*luPeers*/,
                                         const u64* /*lpaPeers*/, u32 /*luStartingIndex*/,
                                         u32 /*luPeersToReturn*/, u32* /*lpcbBuffer*/,
                                         void** /*lphEnum*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Reads the invite the user accepted from the system guide; 87, no invites
// on PC (unreached: XNotifyGetNext never dequeues the invite-accepted notification).
extern "C" u32 XInviteGetAcceptedInfo(u32 /*luUserIndex*/, void* /*lpInviteInfo*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Sends game invites to a list of XUIDs; 87, no invite service.
extern "C" u32 XInviteSend(u32 /*luUserIndex*/, u32 /*luXuidCount*/, const u64* /*lpaXuids*/,
                           const wchar_t* /*lpwcText*/, void* /*lpOverlapped*/)
{
    return KU_ERROR_INVALID_PARAMETER;
}

// [PC platform leaf] Stores data handed to the title on its next launch; no-op success.
extern "C" u32 XSetLaunchData(const void* /*lpLaunchData*/, u32 /*luLaunchDataSize*/)
{
    return KU_ERROR_SUCCESS;
}

// FLAG PC-platform leaf: reads the data a previous title instance left for this launch (an
// accepted invite reboots into the title with it). A PC launch never carries any: 1168
// ERROR_NOT_FOUND with the buffer untouched (XSetLaunchData above keeps nothing).
extern "C" u32 XGetLaunchData(void* /*lpBuffer*/, u32 /*luBufferSize*/)
{
    return KU_ERROR_NOT_FOUND;
}

// ---------------------------------------------------------------------------
// System guide UI
// ---------------------------------------------------------------------------

// [PC platform leaf] Shows the "disc unreadable" system screen; 0, nothing shown (the disk-error
// thread that calls it never runs on PC).
extern "C" unsigned long XShowDirtyDiscErrorUI(unsigned long /*dwUserIndex*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens the guide's sign-in panes; 0, nothing shown.
extern "C" u32 XShowSigninUI(u32 /*luPanes*/, u32 /*luFlags*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens a player's gamer card in the guide; 0, nothing shown.
extern "C" u32 XShowGamerCardUI(u32 /*luUserIndex*/, u64 /*luXuid*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens the "review player" guide page; 0, nothing shown.
extern "C" u32 XShowPlayerReviewUI(u32 /*luUserIndex*/, u64 /*luXuid*/)
{
    return KU_ERROR_SUCCESS;
}

// [PC platform leaf] Opens the guide's message inbox; 0, nothing shown.
extern "C" u32 XShowMessagesUI(u32 /*luUserIndex*/)
{
    return KU_ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------

// [PC platform leaf] Camera readiness; 0, no camera (callers only proceed on 2, ready).
extern "C" u32 XCamGetStatus()
{
    return 0u;
}

// [PC platform leaf] Tears the camera session down; nothing to tear down.
extern "C" void XCamShutdown()
{
}

// ---------------------------------------------------------------------------
// Network address
// ---------------------------------------------------------------------------

// [PC platform leaf] NAT type of the connection; 1, open (there is no NAT between LAN peers,
// and offline nothing is ever reached). The player-parameters Prepare asserts on anything but
// 1 open, 2 moderate, 3 strict.
extern "C" u32 XOnlineGetNatType()
{
    return 1u;
}

// [PC platform leaf] The title's 36-byte network address (address, online address, online
// port, 6-byte adapter address at +0x0A, 20-byte online id). OFFLINE: all zero, 1 (no address
// acquired). LAN: the adapter bytes are the low 48 bits of the PC XUID (the same bytes
// NetConnMAC prints), 2 (address from the ethernet adapter).
extern "C" u32 XNetGetTitleXnAddr(void* lpXnAddr)
{
    const u32 KU_XNADDR_SIZE          = 36u;
    const u32 KU_XNADDR_ENET_OFFSET   = 0x0Au;
    const u32 KU_XNET_ADDR_NONE       = 0x01u;
    const u32 KU_XNET_ADDR_ETHERNET   = 0x02u;

    u8* lpAddr = static_cast<u8*>(lpXnAddr);
    memset(lpAddr, 0, KU_XNADDR_SIZE);
    if (!CgsPcNetLanEnabled())
    {
        return KU_XNET_ADDR_NONE;
    }
    const u64 lu64Xuid = CgsPcNetIdentityXuid();
    for (u32 luByte = 0u; luByte < 6u; ++luByte)
    {
        lpAddr[KU_XNADDR_ENET_OFFSET + luByte] = static_cast<u8>(lu64Xuid >> (8u * (5u - luByte)));
    }
    return KU_XNET_ADDR_ETHERNET;
}
