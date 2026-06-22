#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSystem::DateAndTime - a wall-clock date/time value.
//
// Despite the "PS3" source path, the recovered binary is the X360 ARTIST build, and
// every body in this TU calls the Win32-subset time API (GetLocalTime / GetSystemTime /
// SystemTimeToFileTime / FileTimeToSystemTime over SYSTEMTIME / FILETIME). Those APIs
// are available natively on the Windows/MSVC host, so this is a platform-shim
// reconstruction rather than a placeholder floor.
//
// LAYOUT (pinned from the X360 asm, NOT from the DecFIGS DWARF, which describes the PS3
// libc `tm`/`time_t` flavour of this class):
//   +0  bool   mbIsLocal       - read by Update (0x828D6A68: lbz r11,0(r31)) to choose
//                                 GetLocalTime vs GetSystemTime.
//   +4  u32    muLowDateTime   - FILETIME.dwLowDateTime  (every getter/setter does
//   +8  u32    muHighDateTime  - FILETIME.dwHighDateTime   FileTimeToSystemTime(this+4,...))
//
// The stored value is a 64-bit FILETIME (100-ns ticks since 1601-01-01 UTC). It is held
// as a portable named member here; the .cpp reinterprets &mFileTime to a Win32 FILETIME*
// only at the OS-call boundary, so <windows.h> stays out of this header (no macro
// pollution).
//
// The getters/setters and Update/GetRawTimeValue/GetDifferenceInSeconds are the 14
// binary-recovered functions. SetLocal/Clear/IsZero/IsLocal/GetTimeStructure and the
// comparison operators are declared from the DecFIGS DWARF (they were inlined at every
// call site in the X360 build, so they have no standalone address) and bodied here so the
// home links standalone.
namespace CgsSystem
{
    class DateAndTime
    {
    public:

        // A portable stand-in for the Win32 FILETIME the X360 build stores at this+4.
        // Same layout (two little-endian u32 halves of a 64-bit 100-ns tick count) so it
        // reinterpret_casts to FILETIME* losslessly in the .cpp.
        struct FileTime
        {
            u32 muLowDateTime;
            u32 muHighDateTime;
        };

        DateAndTime();

        // --- recovered (have standalone X360 addresses) ---
        void Update();                                            // 0x828D6A68
        s32  GetYear() const;                                     // 0x828D6AE8
        s32  GetMonth() const;
        s32  GetDay() const;                                      // 0x828D6B48
        s32  GetHour() const;                                     // 0x828D6B78
        s32  GetMinute() const;                                   // 0x828D6BA8
        s32  GetSecond() const;                                   // 0x828D6BD8
        void SetYear(s32 liYear);                                 // 0x828D6C08
        void SetMonth(s32 liMonth);                               // 0x828D6C98
        void SetDay(s32 liDay);                                   // 0x828D6D28
        void SetHour(s32 liHour);                                 // 0x828D6DB8
        void SetMinute(s32 liMinute);                             // 0x828D6E48
        void SetSecond(s32 liSecond);                             // 0x828D6ED8
        u64  GetRawTimeValue() const;                             // 0x828D6F68
        s32  GetDifferenceInSeconds(const DateAndTime& lDateAndTime) const; // 0x828E12E0

        // --- inlined-only in the X360 build; declared from DWARF, bodied for linkage ---
        void SetLocal(bool lbIsLocal);
        void Clear();
        bool IsLocal() const;
        bool IsZero() const;

        bool operator==(const DateAndTime& lDateAndTime) const;
        bool operator<(const DateAndTime& lDateAndTime) const;
        bool operator<=(const DateAndTime& lDateAndTime) const;
        bool operator>(const DateAndTime& lDateAndTime) const;
        bool operator>=(const DateAndTime& lDateAndTime) const;

    private:

        bool     mbIsLocal;   // +0
        FileTime mFileTime;   // +4 (muLowDateTime @+4, muHighDateTime @+8)
    };
}
