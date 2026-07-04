#include "SDKs/XCam/XCamConfig.h"

// ===========================================================================
// XCAM config free functions, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. See XCamConfig.h for the shared shapes/collaborators.
// ===========================================================================

namespace XCAM
{

// @ 0x82987F78 -- WMV-decoder "get data" callback trampoline. `a1` is a
// COM-style interface pointer (int(***)()); the handler loads its vtable, calls
// slot 0, and tail-calls it as vtable[0](a1, a3, a5, a6). The a2 and a4 arguments
// arrive but are not forwarded (the r4/r5/r6 shuffle keeps a3->r4, a5->r5, a6->r6
// with r3=a1 untouched).
int WMVDecCBGetData_HANDLER(void* a1, int /*a2*/, int a3, int /*a4*/, int a5, int a6)
{
    int (**const* ppVtable)(void*, int, int, int) =
        static_cast<int (**const*)(void*, int, int, int)>(a1); // lwz r11,0(r3)
    return (**ppVtable)(a1, a3, a5, a6);                        // vtable[0](this, a3, a5, a6)
}

// @ 0x82981EE8 -- classify an XCam control code into (controlId, terminalId,
// value-size-class). Writes controlId->*lpControlId, terminalId->*lpTerminalId,
// and the payload size class (1, 2 or 4 bytes) ->*lpSize. Codes outside the
// recognised ranges leave the corresponding out-params untouched. The input
// code is returned verbatim in r3. Two independent classification passes mirror
// the asm exactly (the code ranges do not fully overlap between them).
int XCamGetControlIDTerminalID(int iCode, int* lpControlId, int* lpTerminalId, int* lpSize)
{
    int iSizeClass = 2; // r11 default

    // --- pass 1: controlId + terminalId ---
    if (iCode >= 2)
    {
        if (iCode <= 4)
        {
            *lpControlId  = iCode;
            *lpTerminalId = 1;
        }
        else if (iCode > 100)
        {
            if (iCode <= 111)
            {
                *lpControlId  = iCode - 100;
                *lpTerminalId = 2;
            }
            else if (iCode == 202 || iCode == 204)
            {
                *lpControlId  = iCode - 200;
                *lpTerminalId = 3;
            }
        }
    }

    // --- pass 2: value size class ---
    if (iCode <= 110)
    {
        if (iCode >= 106)
        {
            *lpSize = iSizeClass; // r11 (== 2 here)
            return iCode;
        }
        if (iCode < 2)
            return iCode;
        if (iCode > 3)
        {
            if (iCode == 4)
            {
                iSizeClass = 4;
                *lpSize = iSizeClass;
                return iCode;
            }
            if (iCode <= 100)
                return iCode;
            if (iCode > 104)
            {
                *lpSize = 1;
                return iCode;
            }
            *lpSize = iSizeClass; // r11 (== 2)
            return iCode;
        }
        *lpSize = 1;
        return iCode;
    }

    if (iCode == 111 || iCode == 202 || iCode == 204)
        *lpSize = 1;

    return iCode;
}

// @ 0x829827D0 -- apply a composite (multi-control) XCam configuration profile.
// a1 selects the descriptor table (300 -> the 9-entry profile table
// gXCamCompConfigTable300, else the 3-entry gXCamCompConfigTable301); a2 is the
// profile row index; a3 is the optional OVERLAPPED* for async completion. Each
// table row is a run of KXCamControlEntry {code,value} terminated by 0x7FFFFFFF;
// every entry is pushed to the camera via XUsbcamSetConfig. On full success the
// applied-profile index is latched into the table's companion global.
int XCamHandleSetCompConfig(int a1, int a2, void* a3)
{
    int  iResult = 0;
    XCamOverlappedInfo* lpOverlap = nullptr; // r28
    u8   uEntryIndex = 0;                     // r22 (byte counter)
    KXCamConfigPayload payload;              // v15 @ var_70
    payload.uWord = 0;

    if (a3)
    {
        lpOverlap = GetOverlappedInfo();
        if (!lpOverlap)
            return 170;
        // pack (table<<16)|index into the overlap state word @ +0x14
        lpOverlap->uStateWord = (static_cast<u32>(a1) << 16) | static_cast<u8>(a2);
        iResult = SetupOverlap(lpOverlap, a3);
        if (iResult)
            return iResult;
    }

    const KXCamControlEntry* lpTable;
    int  iCount;
    int* lpAppliedFlag;
    if (a1 == 300)
    {
        lpTable       = &gXCamCompConfigTable300[a2 * 9]; // row stride 72 bytes
        iCount        = 9;
        lpAppliedFlag = &gXCamCompConfigApplied300;
    }
    else
    {
        lpTable       = &gXCamCompConfigTable301[a2 * 3]; // row stride 24 bytes
        iCount        = 3;
        lpAppliedFlag = &gXCamCompConfigApplied301;
    }

    const KXCamControlEntry* lpEntry = lpTable;
    for (; lpEntry->iCode != 0x7FFFFFFF; lpEntry += 1)
    {
        int iControlId;
        int iTerminalId;
        int iSizeClass = 0;
        XCamGetControlIDTerminalID(lpEntry->iCode, &iControlId, &iTerminalId, &iSizeClass);

        switch (iSizeClass)
        {
        case 1: payload.uByte = static_cast<u8>(lpEntry->iValue);  break; // stb
        case 2: payload.uHalf = static_cast<u16>(lpEntry->iValue); break; // sth
        case 4: payload.uWord = static_cast<u32>(lpEntry->iValue); break; // stw
        }

        void* lpCompletion = a3 ? reinterpret_cast<void*>(&HandleSetCompCompletion)
                                : nullptr;
        iResult = XUsbcamSetConfig(gXCamDeviceHandle, iTerminalId, iControlId,
                                   &payload, lpCompletion, lpOverlap);
        if (iResult)
            return iResult;

        uEntryIndex = static_cast<u8>(uEntryIndex + 1);
        if (uEntryIndex == iCount)
            break;
        if (a3)
        {
            // advance the packed overlap state word to the next entry index
            lpOverlap->uStateWord =
                (((static_cast<u32>(a1) << 8) & 0xFFFF00u) | uEntryIndex) << 8
                | static_cast<u8>(a2);
        }
    }

    *lpAppliedFlag = a2;
    return iResult;
}

// @ 0x82980820 -- the internal XCam SetConfig entry point. Composite profiles
// (300/301) are forwarded to XCamHandleSetCompConfig; every other control code
// is resolved to its (controlId, terminalId, size class) and pushed to the
// camera via XUsbcamSetConfig. When a3 is a non-null OVERLAPPED* the call is set
// up for async completion (via HandleCompletion); otherwise it is synchronous.
// Special-cases control 2 (power-line frequency): value 1 -> 8, else -> 1.
int XCamSetConfigInternal(int a1, int a2, int* a3)
{
    int  iValue = a2;                       // r28
    XCamOverlappedInfo* lpOverlap = nullptr; // r26
    KXCamConfigPayload  payload;            // v13 @ var_50
    payload.uWord = 0;

    if (a1 == 300 || a1 == 301)
        return XCamHandleSetCompConfig(a1, a2, a3);

    int iControlId;
    int iTerminalId;
    int iSizeClass;
    XCamGetControlIDTerminalID(a1, &iControlId, &iTerminalId, &iSizeClass);

    int iStatus;
    if (a3)
    {
        lpOverlap = GetOverlappedInfo();
        if (!lpOverlap)
        {
            iStatus = 170;
            goto epilogue;
        }
        iStatus = SetupOverlap(lpOverlap, a3);
        if (iStatus)
            goto epilogue;
        lpOverlap->uMode = 2; // stw 2, 0xC(overlap)
    }

    if (a1 == 2)
        iValue = (a2 == 1) ? 8 : 1;

    switch (iSizeClass)
    {
    case 1: payload.uByte = static_cast<u8>(iValue);  break; // stb
    case 2: payload.uHalf = static_cast<u16>(iValue); break; // sth
    case 4: payload.uWord = static_cast<u32>(iValue); break; // stw
    }

    {
        void* lpCompletion = a3 ? reinterpret_cast<void*>(&HandleCompletion)
                                : nullptr;
        iStatus = XUsbcamSetConfig(gXCamDeviceHandle, iTerminalId, iControlId,
                                   &payload, lpCompletion, lpOverlap);
    }

epilogue:
    if (a3)
        *a3 = iStatus;
    if (iStatus != 997 && lpOverlap)
        ReturnOverlappedInfo(lpOverlap);
    return iStatus;
}

// @ 0x82981FC0 -- convert a frame rate (frames per second) to a UVC frame
// interval in 100-nanosecond units: interval = 10000000 / fps. The __twllei on
// the divisor is the compiler-inserted divide-by-zero trap for the divwu below,
// not a separate guard. Always returns 0 (success).
int XcamLookupFrameInterval(u32 uFramesPerSecond, u32* lpInterval)
{
    *lpInterval = 10000000u / uFramesPerSecond; // divwu; twllei = div-by-zero trap
    return 0;
}

} // namespace XCAM
