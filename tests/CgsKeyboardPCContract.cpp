// Contract fixture for the held PC keyboard leaf. This test links the real
// CgsXboxLivePC.cpp and CgsXOverlappedX360.cpp production translation units.
// It drives actual Win32 EDIT/BUTTON controls with target-window messages only;
// it never injects global keyboard or mouse input.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <iterator>

#include "types.hpp"

// The fixture inspects the real embedded guest record when calling the real
// Construct body. This changes access only in this test translation unit.
#define private public
#include "GameShared/GameClasses/System/CgsXOverlapped.h"
#undef private
#include "GameShared/GameClasses/System/PC/CgsKeyboardPC.h"

using CgsSystem::CgsXOverlapped;

extern "C" u32 XShowKeyboardUI(u32 luUser, u32 luFlags, const u16* lpDefault,
                               const u16* lpTitle, const u16* lpPrompt,
                               u16* lpResult, u32 luCapacity, PXOVERLAPPED lpOverlapped);
extern "C" u32 XCancelOverlapped(void* lpOverlapped);

namespace
{
    constexpr u32 KU_FLAGS = 0x30000064u;
    constexpr u32 KU_CAPACITY = 512u;
    const u16 KU_SENTINEL = 0x5a5au;
    const int KI_EDIT = 101;
    const wchar_t KAC_OWNER_CLASS[] = L"CgsKeyboardPCContractOwner";

    unsigned guChecks = 0;
    unsigned guFailures = 0;
    HWND ghOwner = nullptr;

    void Check(bool lbPass, const char* lpcName)
    {
        ++guChecks;
        if (!lbPass)
        {
            ++guFailures;
            std::printf("FAIL %s\n", lpcName);
        }
    }

    int Finish()
    {
        std::printf("CgsKeyboardPCContract: %u checks, %u failures\n", guChecks, guFailures);
        return guFailures == 0 ? 0 : 1;
    }

    bool HostKeysReleased()
    {
        for (int liKey = 1; liKey < 256; ++liKey)
            if ((GetAsyncKeyState(liKey) & 0x8000) != 0) return false;
        return true;
    }

    void FillSentinel(std::array<u16, KU_CAPACITY>& lrBuffer)
    {
        lrBuffer.fill(KU_SENTINEL);
    }

    bool IsSentinel(const std::array<u16, KU_CAPACITY>& lrBuffer)
    {
        return std::all_of(lrBuffer.begin(), lrBuffer.end(),
                           [](u16 luUnit) { return luUnit == KU_SENTINEL; });
    }

    LRESULT CALLBACK OwnerProc(HWND lpWindow, UINT luMessage, WPARAM luWParam, LPARAM liLParam)
    {
        return DefWindowProcW(lpWindow, luMessage, luWParam, liLParam);
    }

    HWND CreateOwner()
    {
        const HINSTANCE lpInstance = GetModuleHandleW(nullptr);
        WNDCLASSW lClass = {};
        lClass.lpfnWndProc = OwnerProc;
        lClass.hInstance = lpInstance;
        lClass.lpszClassName = KAC_OWNER_CLASS;
        if (!RegisterClassW(&lClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return nullptr;
        // Keep the owner hidden; Pump creates the production owned popup itself.
        return CreateWindowExW(0, KAC_OWNER_CLASS, L"keyboard contract owner",
                               WS_OVERLAPPEDWINDOW, 80, 80, 640, 480,
                               nullptr, nullptr, lpInstance, nullptr);
    }

    struct FindDialogContext
    {
        HWND hOwner;
        HWND hFound;
    };

    BOOL CALLBACK FindDialogOnThread(HWND lpWindow, LPARAM liContext)
    {
        FindDialogContext* lpSearch = reinterpret_cast<FindDialogContext*>(liContext);
        if (GetWindow(lpWindow, GW_OWNER) != lpSearch->hOwner) return TRUE;
        wchar_t lacClass[64] = {};
        if (GetClassNameW(lpWindow, lacClass, static_cast<int>(std::size(lacClass))) > 0 &&
            lstrcmpW(lacClass, L"BurnoutPCKeyboard") == 0)
        {
            lpSearch->hFound = lpWindow;
            return FALSE;
        }
        return TRUE;
    }

    HWND FindDialog(HWND lpOwner)
    {
        FindDialogContext lSearch = {lpOwner, nullptr};
        EnumThreadWindows(GetCurrentThreadId(), FindDialogOnThread,
                          reinterpret_cast<LPARAM>(&lSearch));
        return lSearch.hFound;
    }

    struct FindPromptContext
    {
        HWND hFound;
    };

    BOOL CALLBACK FindReadOnlyEdit(HWND lpWindow, LPARAM liContext)
    {
        FindPromptContext* lpSearch = reinterpret_cast<FindPromptContext*>(liContext);
        wchar_t lacClass[32] = {};
        if (GetClassNameW(lpWindow, lacClass, static_cast<int>(std::size(lacClass))) > 0 &&
            lstrcmpiW(lacClass, L"EDIT") == 0 && GetDlgCtrlID(lpWindow) != KI_EDIT &&
            (GetWindowLongPtrW(lpWindow, GWL_STYLE) & ES_READONLY) != 0)
        {
            lpSearch->hFound = lpWindow;
            return FALSE;
        }
        return TRUE;
    }

    HWND FindPromptEdit(HWND lpDialog)
    {
        FindPromptContext lSearch = {nullptr};
        EnumChildWindows(lpDialog, FindReadOnlyEdit, reinterpret_cast<LPARAM>(&lSearch));
        return lSearch.hFound;
    }

    bool InputOwnerSamples(bool lbDetailed)
    {
        if (!lbDetailed)
        {
            CgsSystem::KeyboardPC::BlockDebugInput(true);
            CgsSystem::KeyboardPC::BlockGameInput(true);
            return true;
        }

        Check(CgsSystem::KeyboardPC::BlockDebugInput(false),
              "captured debug owner consumes non-neutral sample");
        Check(CgsSystem::KeyboardPC::BlockGameInput(true),
              "captured game owner consumes sample while debug owner is non-neutral");
        Check(CgsSystem::KeyboardPC::IsCapturingInput(),
              "release waits for both input owners to report neutral");
        Check(CgsSystem::KeyboardPC::BlockDebugInput(true),
              "captured debug owner consumes neutral sample");
        Check(CgsSystem::KeyboardPC::BlockGameInput(false),
              "captured game owner consumes non-neutral sample");
        Check(CgsSystem::KeyboardPC::IsCapturingInput(),
              "game non-neutral sample keeps capture active");
        Check(CgsSystem::KeyboardPC::BlockGameInput(true),
              "captured game owner consumes neutral sample");
        Check(CgsSystem::KeyboardPC::IsCapturingInput(),
              "pending operation keeps capture active after opening-input release");
        return true;
    }

    HWND OpenDialog(bool lbDetailedCapture)
    {
        InputOwnerSamples(lbDetailedCapture);
        CgsSystem::KeyboardPC::Pump(reinterpret_cast<HWND__*>(ghOwner));
        HWND lpDialog = FindDialog(ghOwner);
        Check(lpDialog != nullptr, "Pump creates the real owned BurnoutPCKeyboard window");
        if (lpDialog == nullptr)
        {
            std::printf("UI_UNAVAILABLE: Pump did not create a Win32 keyboard window; no UI contract is counted as passed.\n");
            return nullptr;
        }
        Check(GetWindow(lpDialog, GW_OWNER) == ghOwner,
              "keyboard dialog is owned by the supplied host window");
        DWORD luOwnerPid = 0;
        Check(GetWindowThreadProcessId(lpDialog, &luOwnerPid) == GetCurrentThreadId() &&
              luOwnerPid == GetCurrentProcessId(),
              "keyboard HWND is created on the owner process/thread");
        Check(IsWindowVisible(lpDialog) != FALSE,
              "production Pump shows the native keyboard dialog");
        return lpDialog;
    }

    void ReleaseCapture(const char* lpcLabel)
    {
        const bool lbBefore = CgsSystem::KeyboardPC::IsCapturingInput();
        Check(lbBefore, lpcLabel);
        Check(CgsSystem::KeyboardPC::BlockDebugInput(false),
              "terminal dialog consumes non-neutral debug sample");
        Check(CgsSystem::KeyboardPC::BlockGameInput(true),
              "terminal dialog consumes game sample while debug owner is non-neutral");
        Check(CgsSystem::KeyboardPC::IsCapturingInput(),
              "capture remains until debug and game owners both report neutral");
        Check(CgsSystem::KeyboardPC::BlockDebugInput(true),
              "terminal dialog consumes neutral debug sample");
        Check(CgsSystem::KeyboardPC::BlockGameInput(false),
              "terminal dialog consumes non-neutral game sample");
        Check(CgsSystem::KeyboardPC::IsCapturingInput(),
              "game non-neutral sample keeps terminal capture active");
        Check(CgsSystem::KeyboardPC::BlockGameInput(true),
              "terminal dialog consumes the release frame");
        Check(!CgsSystem::KeyboardPC::IsCapturingInput(),
              "capture releases after neutral owners and all host keys are up");
        Check(!CgsSystem::KeyboardPC::BlockGameInput(true),
              "subsequent game sample is no longer captured");
    }

    u32 Begin(CgsXOverlapped& lrOverlapped, std::array<u16, KU_CAPACITY>& lrResult,
              const u16* lpDefault, const u16* lpTitle, const u16* lpPrompt)
    {
        lrOverlapped.Construct();
        FillSentinel(lrResult);
        return XShowKeyboardUI(0, KU_FLAGS, lpDefault, lpTitle, lpPrompt,
                               lrResult.data(), KU_CAPACITY, &lrOverlapped.mOverlapped);
    }

    void CheckPending(const CgsXOverlapped& lrOverlapped, const char* lpcLabel)
    {
        Check(lrOverlapped.mOverlapped.InternalLow == ERROR_IO_PENDING &&
              lrOverlapped.mOverlapped.InternalHigh == 0 &&
              lrOverlapped.mOverlapped.dwExtendedError == 0, lpcLabel);
    }

    void TestEditedUnicodeAndIdentity()
    {
        CgsXOverlapped lOverlapped;
        std::array<u16, KU_CAPACITY> laResult;
        std::array<u16, KU_CAPACITY> laRejectedResult;
        u16 laDefault[] = {'d','e','f','a','u','l','t',0};
        u16 laTitle[] = {'C','o','n','t','r','a','c','t',0};
        u16 laPrompt[] = {'T','y','p','e',' ','t','e','x','t',0};
        const u32 luBegin = Begin(lOverlapped, laResult, laDefault, laTitle, laPrompt);
        Check(luBegin == ERROR_IO_PENDING, "XShowKeyboardUI starts asynchronously");
        if (luBegin != ERROR_IO_PENDING) return;
        CheckPending(lOverlapped, "begin publishes the pending overlapped identity");
        Check(CgsSystem::KeyboardPC::IsCapturingInput(), "begin captures game/debug input");

        // The dialog is created on the next host Pump, after the caller's strings
        // may have gone out of scope. The operation must own copies of all three.
        laDefault[0] = static_cast<u16>('X');
        laTitle[0] = static_cast<u16>('X');
        laPrompt[0] = static_cast<u16>('X');
        HWND lpDialog = OpenDialog(true);
        if (lpDialog == nullptr) return;

        wchar_t lacTitle[64] = {};
        GetWindowTextW(lpDialog, lacTitle, static_cast<int>(std::size(lacTitle)));
        Check(lstrcmpW(lacTitle, L"Contract") == 0,
              "window title survives caller buffer mutation");
        HWND lpEdit = GetDlgItem(lpDialog, KI_EDIT);
        wchar_t lacEditClass[32] = {};
        if (lpEdit != nullptr)
            GetClassNameW(lpEdit, lacEditClass, static_cast<int>(std::size(lacEditClass)));
        Check(lpEdit != nullptr && lstrcmpiW(lacEditClass, L"EDIT") == 0,
              "id 101 is an actual Win32 EDIT child");
        HWND lpPrompt = FindPromptEdit(lpDialog);
        wchar_t lacPrompt[64] = {};
        if (lpPrompt != nullptr)
            GetWindowTextW(lpPrompt, lacPrompt, static_cast<int>(std::size(lacPrompt)));
        Check(lpPrompt != nullptr && (GetWindowLongPtrW(lpPrompt, GWL_STYLE) & ES_READONLY) != 0 &&
              lstrcmpW(lacPrompt, L"Type text") == 0,
              "prompt EDIT is read-only and survives caller buffer mutation");
        if (lpEdit == nullptr) return;
        wchar_t lacInitial[64] = {};
        GetWindowTextW(lpEdit, lacInitial, static_cast<int>(std::size(lacInitial)));
        Check(lstrcmpW(lacInitial, L"default") == 0,
              "default EDIT text survives caller buffer mutation");
        DWORD luSelectionStart = 0, luSelectionEnd = 0;
        SendMessageW(lpEdit, EM_GETSEL, reinterpret_cast<WPARAM>(&luSelectionStart),
                     reinterpret_cast<LPARAM>(&luSelectionEnd));
        Check(luSelectionStart == 0 && luSelectionEnd == 7,
              "default text is selected for editing");

        FillSentinel(laRejectedResult);
        const u32 luSamePointerBusy = XShowKeyboardUI(0, KU_FLAGS, nullptr, nullptr, nullptr,
            laRejectedResult.data(), KU_CAPACITY, &lOverlapped.mOverlapped);
        Check(luSamePointerBusy == ERROR_BUSY,
              "reusing the live overlapped address is rejected as busy");
        CheckPending(lOverlapped, "busy same-pointer request preserves original pending record");
        Check(IsSentinel(laResult) && IsSentinel(laRejectedResult),
              "busy request leaves both borrowed result buffers untouched");

        XOVERLAPPED lOtherOverlapped = {};
        const u32 luOtherBusy = XShowKeyboardUI(0, KU_FLAGS, nullptr, nullptr, nullptr,
            laRejectedResult.data(), KU_CAPACITY, &lOtherOverlapped);
        Check(luOtherBusy == ERROR_BUSY && lOtherOverlapped.InternalLow == ERROR_BUSY &&
              lOtherOverlapped.dwExtendedError == ERROR_BUSY,
              "different overlapped address receives its own busy result");
        Check(XGetOverlappedResult(&lOtherOverlapped, nullptr, 0) == ERROR_BUSY,
              "busy result identity does not alias the live request");
        XOVERLAPPED lCopiedOverlapped = lOverlapped.mOverlapped;
        Check(XCancelOverlapped(&lCopiedOverlapped) == ERROR_SUCCESS &&
              XGetOverlappedResult(&lOverlapped.mOverlapped, nullptr, 0) == ERROR_IO_INCOMPLETE,
              "cancel through a copied record does not cancel the live pointer identity");

        static const wchar_t KAC_EDITED[] = {
            0x00e9, 0x0020, 0x2014, 0x0020, 0x65e5, 0x672c, 0x8a9e, 0x0020,
            0xd83c, 0xdf0c, 0
        };
        static const u16 KAC_EXPECTED[] = {
            0x00e9, 0x0020, 0x2014, 0x0020, 0x65e5, 0x672c, 0x8a9e, 0x0020,
            0xd83c, 0xdf0c, 0
        };
        SendMessageW(lpEdit, EM_SETSEL, 0, static_cast<LPARAM>(-1));
        SendMessageW(lpEdit, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(KAC_EDITED));
        wchar_t lacEdited[64] = {};
        GetWindowTextW(lpEdit, lacEdited, static_cast<int>(std::size(lacEdited)));
        Check(lstrcmpW(lacEdited, KAC_EDITED) == 0,
              "targeted EDIT messages replace selected default with Unicode text");
        HWND lpOk = GetDlgItem(lpDialog, IDOK);
        Check(lpOk != nullptr, "native OK button exists");
        if (lpOk == nullptr) return;
        SendMessageW(lpOk, BM_CLICK, 0, 0);
        Check(FindDialog(ghOwner) == nullptr, "OK button closes its owned keyboard window");
        Check(IsSentinel(laResult), "accepted text is not published before overlapped polling");
        CheckPending(lOverlapped, "completion remains pending in caller storage until polled");

        u32 luResultBytes = 0xffffffffu;
        Check(XGetOverlappedResult(&lOverlapped.mOverlapped, &luResultBytes, 0) == ERROR_SUCCESS,
              "poll publishes successful edit completion");
        Check(lOverlapped.mOverlapped.InternalHigh == 0 && luResultBytes == 0,
              "PC keyboard completion reports the documented zero byte-count field");
        Check(lOverlapped.mOverlapped.dwExtendedError == ERROR_SUCCESS &&
              XGetOverlappedExtendedError(&lOverlapped.mOverlapped) == ERROR_SUCCESS,
              "result and extended-error words publish success");
        bool lbAcceptedMatches = true;
        for (size_t lu = 0; lu < std::size(KAC_EXPECTED); ++lu)
            lbAcceptedMatches = lbAcceptedMatches && laResult[lu] == KAC_EXPECTED[lu];
        Check(lbAcceptedMatches, "published result preserves BMP and surrogate-pair code units");
        bool lbTailUntouched = true;
        for (size_t lu = std::size(KAC_EXPECTED); lu < laResult.size(); ++lu)
            lbTailUntouched = lbTailUntouched && laResult[lu] == KU_SENTINEL;
        Check(lbTailUntouched, "publication stops after the terminating UTF-16 NUL");
        Check(IsSentinel(laRejectedResult), "rejected request buffer remains borrowed but untouched");
        ReleaseCapture("successful result remains captured until release sequence");
    }

    void TestEmptyAccept()
    {
        CgsXOverlapped lOverlapped;
        std::array<u16, KU_CAPACITY> laResult;
        static const u16 KAC_DEFAULT[] = {'x','y','z',0};
        static const u16 KAC_TITLE[] = {'E','m','p','t','y',0};
        static const u16 KAC_PROMPT[] = {'P','r','o','m','p','t',0};
        const u32 luBegin = Begin(lOverlapped, laResult, KAC_DEFAULT, KAC_TITLE, KAC_PROMPT);
        Check(luBegin == ERROR_IO_PENDING, "empty-accept request begins pending");
        if (luBegin != ERROR_IO_PENDING) return;
        HWND lpDialog = OpenDialog(false);
        if (lpDialog == nullptr) return;
        HWND lpEdit = GetDlgItem(lpDialog, KI_EDIT);
        HWND lpOk = GetDlgItem(lpDialog, IDOK);
        Check(lpEdit != nullptr && lpOk != nullptr, "empty-accept EDIT and OK controls exist");
        if (lpEdit == nullptr || lpOk == nullptr) return;
        SendMessageW(lpEdit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
        wchar_t lacText[8] = {1,1,1,1,1,1,1,1};
        GetWindowTextW(lpEdit, lacText, static_cast<int>(std::size(lacText)));
        Check(lacText[0] == 0, "EDIT accepts empty text without synthesizing a character");
        SendMessageW(lpOk, BM_CLICK, 0, 0);
        Check(IsSentinel(laResult), "empty result is not published before polling");
        Check(XGetOverlappedResult(&lOverlapped.mOverlapped, nullptr, 0) == ERROR_SUCCESS,
              "empty OK is successful rather than cancellation");
        Check(laResult[0] == 0, "empty OK publishes one terminating NUL");
        bool lbTailUntouched = true;
        for (size_t lu = 1; lu < laResult.size(); ++lu)
            lbTailUntouched = lbTailUntouched && laResult[lu] == KU_SENTINEL;
        Check(lbTailUntouched, "empty OK leaves the rest of borrowed destination unchanged");
        ReleaseCapture("empty successful result remains captured until release sequence");
    }

    void TestCancelButton()
    {
        CgsXOverlapped lOverlapped;
        std::array<u16, KU_CAPACITY> laResult;
        static const u16 KAC_DEFAULT[] = {'d','i','s','c','a','r','d',0};
        static const u16 KAC_TITLE[] = {'C','a','n','c','e','l',0};
        static const u16 KAC_PROMPT[] = {'P','r','o','m','p','t',0};
        const u32 luBegin = Begin(lOverlapped, laResult, KAC_DEFAULT, KAC_TITLE, KAC_PROMPT);
        Check(luBegin == ERROR_IO_PENDING, "button-cancel request begins pending");
        if (luBegin != ERROR_IO_PENDING) return;
        HWND lpDialog = OpenDialog(false);
        if (lpDialog == nullptr) return;
        HWND lpCancel = GetDlgItem(lpDialog, IDCANCEL);
        Check(lpCancel != nullptr, "native Cancel button exists");
        if (lpCancel == nullptr) return;
        SendMessageW(lpCancel, BM_CLICK, 0, 0);
        Check(FindDialog(ghOwner) == nullptr, "Cancel button closes the owned keyboard window");
        Check(IsSentinel(laResult), "cancel leaves borrowed destination unchanged before polling");
        CheckPending(lOverlapped, "cancel status is published only when the caller polls");
        Check(XGetOverlappedResult(&lOverlapped.mOverlapped, nullptr, 0) == ERROR_CANCELLED,
              "Cancel button reports cancellation");
        Check(lOverlapped.mOverlapped.InternalHigh == 0 &&
              lOverlapped.mOverlapped.InternalLow == ERROR_CANCELLED &&
              lOverlapped.mOverlapped.dwExtendedError == ERROR_CANCELLED,
              "cancel result updates status words without writing text");
        Check(IsSentinel(laResult), "polled cancellation preserves every result code unit");
        ReleaseCapture("cancelled result remains captured until release sequence");
    }

    void TestApiCancel()
    {
        CgsXOverlapped lOverlapped;
        std::array<u16, KU_CAPACITY> laResult;
        static const u16 KAC_DEFAULT[] = {'s','t','a','y',0};
        static const u16 KAC_TITLE[] = {'A','P','I',' ','c','a','n','c','e','l',0};
        static const u16 KAC_PROMPT[] = {'P','r','o','m','p','t',0};
        const u32 luBegin = Begin(lOverlapped, laResult, KAC_DEFAULT, KAC_TITLE, KAC_PROMPT);
        Check(luBegin == ERROR_IO_PENDING, "API-cancel request begins pending");
        if (luBegin != ERROR_IO_PENDING) return;
        HWND lpDialog = OpenDialog(false);
        if (lpDialog == nullptr) return;
        Check(XCancelOverlapped(&lOverlapped.mOverlapped) == ERROR_SUCCESS,
              "XCancelOverlapped accepts cancellation of the live request");
        Check(lOverlapped.mOverlapped.InternalLow == ERROR_CANCELLED &&
              lOverlapped.mOverlapped.dwExtendedError == ERROR_CANCELLED,
              "API cancel publishes terminal status immediately");
        Check(IsSentinel(laResult), "API cancel preserves borrowed destination");
        CgsSystem::KeyboardPC::Pump(reinterpret_cast<HWND__*>(ghOwner));
        Check(FindDialog(ghOwner) == nullptr, "next owner Pump closes UI after API cancellation");
        Check(XGetOverlappedResult(&lOverlapped.mOverlapped, nullptr, 0) == ERROR_CANCELLED,
              "poll after API cancel returns cancelled status");
        Check(IsSentinel(laResult), "poll after API cancel still leaves destination unchanged");
        ReleaseCapture("API-cancelled result remains captured until release sequence");
    }

    void TestOverlappedResetAbandonsPendingUi()
    {
        CgsXOverlapped lOverlapped;
        std::array<u16, KU_CAPACITY> laResult;
        static const u16 KAC_DEFAULT[] = {'l','a','t','e',0};
        static const u16 KAC_TITLE[] = {'R','e','s','e','t',0};
        static const u16 KAC_PROMPT[] = {'P','r','o','m','p','t',0};
        const u32 luBegin = Begin(lOverlapped, laResult, KAC_DEFAULT, KAC_TITLE, KAC_PROMPT);
        Check(luBegin == ERROR_IO_PENDING, "reset request begins pending");
        if (luBegin != ERROR_IO_PENDING) return;
        HWND lpDialog = OpenDialog(false);
        if (lpDialog == nullptr) return;
        HWND lpOk = GetDlgItem(lpDialog, IDOK);
        Check(lpOk != nullptr, "reset test has a real OK control");
        if (lpOk == nullptr) return;

        // This is the actual production Construct -> KeyboardPC::Forget path.
        lOverlapped.Construct();
        const bool lbZero = lOverlapped.mOverlapped.InternalLow == 0 &&
            lOverlapped.mOverlapped.InternalHigh == 0 &&
            lOverlapped.mOverlapped.InternalContext == 0 &&
            lOverlapped.mOverlapped.hEvent == 0 &&
            lOverlapped.mOverlapped.pCompletionRoutine == 0 &&
            lOverlapped.mOverlapped.dwCompletionContext == 0 &&
            lOverlapped.mOverlapped.dwExtendedError == 0;
        Check(lbZero, "CgsXOverlapped::Construct resets all seven production words");

        // A late button message reaches the still-owned native window after reset.
        // The abandoned operation must not publish through either borrowed pointer.
        SendMessageW(lpOk, BM_CLICK, 0, 0);
        Check(IsSentinel(laResult), "late UI completion after reset cannot write result storage");
        Check(lOverlapped.mOverlapped.InternalLow == 0 &&
              lOverlapped.mOverlapped.dwExtendedError == 0,
              "late UI completion after reset cannot rewrite the reset overlapped record");
        Check(FindDialog(ghOwner) == nullptr, "late UI completion destroys abandoned window");
        Check(XGetOverlappedResult(&lOverlapped.mOverlapped, nullptr, 0) == ERROR_SUCCESS,
              "reset overlapped record is no longer associated with the abandoned request");
        Check(IsSentinel(laResult), "result remains unchanged after reset and late completion");
        ReleaseCapture("forgotten request remains captured until release sequence");
    }

    void TestOwnerShutdownAbandonsPendingUi()
    {
        CgsXOverlapped lOverlapped;
        std::array<u16, KU_CAPACITY> laResult;
        static const u16 KAC_DEFAULT[] = {'o','w','n','e','r',0};
        static const u16 KAC_TITLE[] = {'S','h','u','t','d','o','w','n',0};
        static const u16 KAC_PROMPT[] = {'P','r','o','m','p','t',0};
        const u32 luBegin = Begin(lOverlapped, laResult, KAC_DEFAULT, KAC_TITLE, KAC_PROMPT);
        Check(luBegin == ERROR_IO_PENDING, "owner-shutdown request begins pending");
        if (luBegin != ERROR_IO_PENDING) return;
        HWND lpDialog = OpenDialog(false);
        if (lpDialog == nullptr) return;

        CgsSystem::KeyboardPC::OwnerClosing(reinterpret_cast<HWND__*>(ghOwner));
        Check(FindDialog(ghOwner) == nullptr, "owner shutdown destroys the native keyboard window");
        Check(IsSentinel(laResult), "owner shutdown abandons borrowed result storage");
        CheckPending(lOverlapped, "owner shutdown does not publish through borrowed overlapped storage");
        Check(XGetOverlappedResult(&lOverlapped.mOverlapped, nullptr, 0) == ERROR_IO_INCOMPLETE,
              "forgotten owner operation is no longer completed through the old identity");
        Check(IsSentinel(laResult), "poll after owner shutdown still leaves destination untouched");

        CgsXOverlapped lAfterClose;
        std::array<u16, KU_CAPACITY> laAfterClose;
        FillSentinel(laAfterClose);
        lAfterClose.Construct();
        static const u16 KAC_DEFAULT_AFTER[] = {'n','e','w',0};
        const u32 luAfterClose = XShowKeyboardUI(0, KU_FLAGS, KAC_DEFAULT_AFTER,
            KAC_TITLE, KAC_PROMPT, laAfterClose.data(), KU_CAPACITY, &lAfterClose.mOverlapped);
        Check(luAfterClose == ERROR_OPERATION_ABORTED &&
              lAfterClose.mOverlapped.InternalLow == ERROR_OPERATION_ABORTED,
              "closed owner refuses subsequent keyboard requests");
        Check(IsSentinel(laAfterClose), "rejected post-close request does not write result storage");
        ReleaseCapture("abandoned owner operation remains captured until release sequence");
    }
}

int main()
{
    if (!HostKeysReleased())
    {
        Check(false, "host reports all keys released; fixture sends no global input");
        std::printf("HOST_INPUT_BUSY: no keyboard or mouse input was sent.\n");
        return Finish();
    }
    Check(true, "host reports all keys released; fixture sends no global input");
    ghOwner = CreateOwner();
    Check(ghOwner != nullptr, "hidden owner HWND is available on the hosted Windows desktop");
    if (ghOwner == nullptr)
    {
        std::printf("UI_UNAVAILABLE: host could not create an owner HWND.\n");
        return Finish();
    }

    TestEditedUnicodeAndIdentity();
    if (FindDialog(ghOwner) != nullptr) CgsSystem::KeyboardPC::OwnerClosing(reinterpret_cast<HWND__*>(ghOwner));
    if (!CgsSystem::KeyboardPC::IsCapturingInput())
    {
        TestEmptyAccept();
        if (FindDialog(ghOwner) != nullptr) CgsSystem::KeyboardPC::OwnerClosing(reinterpret_cast<HWND__*>(ghOwner));
    }
    if (!CgsSystem::KeyboardPC::IsCapturingInput())
    {
        TestCancelButton();
        if (FindDialog(ghOwner) != nullptr) CgsSystem::KeyboardPC::OwnerClosing(reinterpret_cast<HWND__*>(ghOwner));
    }
    if (!CgsSystem::KeyboardPC::IsCapturingInput())
    {
        TestApiCancel();
        if (FindDialog(ghOwner) != nullptr) CgsSystem::KeyboardPC::OwnerClosing(reinterpret_cast<HWND__*>(ghOwner));
    }
    if (!CgsSystem::KeyboardPC::IsCapturingInput())
    {
        TestOverlappedResetAbandonsPendingUi();
        if (FindDialog(ghOwner) != nullptr) CgsSystem::KeyboardPC::OwnerClosing(reinterpret_cast<HWND__*>(ghOwner));
    }
    if (!CgsSystem::KeyboardPC::IsCapturingInput())
        TestOwnerShutdownAbandonsPendingUi();

    CgsSystem::KeyboardPC::OwnerClosing(reinterpret_cast<HWND__*>(ghOwner));
    if (IsWindow(ghOwner)) DestroyWindow(ghOwner);
    UnregisterClassW(KAC_OWNER_CLASS, GetModuleHandleW(nullptr));
    return Finish();
}
