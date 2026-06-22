#include "SDKs/Realmc/RealmcLocale.h"

#include <cstdarg>

// ===========================================================================
// RealmcCore::Locale -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Locale::SetLocaleGetStrCallback  @ 0x82C448D8
//   Locale::GetString                @ 0x82C448E8
//
// No leak source / no DWARF: SHAPE and BODIES come from the X360 asm. See
// RealmcLocale.h for the layout and the flagged platform/vendor data.
// ===========================================================================

namespace RealmcCore
{
namespace Locale
{

// The installed template-getter callback (X360 dword_832BE200). Null until
// SetLocaleGetStrCallback installs it. Owning definition.
static LocaleGetStrCallback g_pfnLocaleGetStr = nullptr;

// Five rotating output buffers (X360 unk_832BB9F0) and the next-buffer index
// (X360 dword_832BE1FC). Owning definitions.
static u16 gauLocaleStringBuffers[KI_LOCALE_BUFFER_COUNT][KI_LOCALE_BUFFER_CAPACITY_U16];
static int guLocaleBufferIndex = 0;

// ---------------------------------------------------------------------------
// SetLocaleGetStrCallback @ 0x82C448D8
//
//   lis r11, dword_832BE200@ha ; stw r3, dword_832BE200@l(r11) ; blr
//
// Stores the argument into the callback global and returns it (the asm returns
// r3 unchanged).
// ---------------------------------------------------------------------------
LocaleGetStrCallback SetLocaleGetStrCallback(LocaleGetStrCallback pfnCallback)
{
    g_pfnLocaleGetStr = pfnCallback;
    return pfnCallback;
}

// ---------------------------------------------------------------------------
// GetString @ 0x82C448E8
//
// Step 1 -- pack varargs by the per-arg conversion-character spec pszArgSpec:
//   liArgCount = strlen(pszArgSpec)
//   for each slot i in [0, liArgCount):
//     switch (pszArgSpec[i]):
//       'c'        : consume one va_arg, store its (sign-extended byte) value
//       'd','p','s': consume one va_arg, store its 4-byte value (int or ptr)
//       default    : store nothing (slot left untouched)
// The asm walks the varargs as 8-byte-aligned slots; for 'c' it reads the low
// byte (lbz at -1), for d/p/s the low word (lwz at -4).
//
// Step 2 -- fetch the localized template via the callback. If it is null
// (callback unset / no template), return null.
//
// Step 3 -- expand the template into the next rotating buffer:
//   state = "literal".
//   for each u16 ch in template (until NUL):
//     if state == "after-percent":
//       liArg = ch - '1'                     (the ASCII digit selects the slot)
//       switch (pszArgSpec[liArg]):
//         'd': write signed-decimal of packed[liArg]
//         's': write the wide string at packed[liArg]
//         else: nothing
//       state = "literal"
//     else:
//       if ch == '%': state = "after-percent"
//       else if ch != '"': copy ch verbatim
//   NUL-terminate; advance guLocaleBufferIndex = (index + 1) % 5.
// Returns the filled buffer.
//
// FLAGGED: the packed-arg storage (a 16-entry int/pointer array on the X360
// stack) is modelled as a fixed-size local union array; 'p' is treated like
// 'd'/'s' for packing (4-byte value) exactly as the asm does, and is never
// expanded (only 'd' and 's' have expansion arms in the asm).
// ---------------------------------------------------------------------------
u16* GetString(const char* pszArgSpec, ...)
{
    // The packed arg slots. Each holds either a signed int ('c'/'d') or a
    // pointer ('p'/'s'); on the 32-bit X360 both are 4 bytes (asm stores a
    // single word per slot). Model as a union so both reads are well-typed.
    union ArgSlot
    {
        int   miValue;     // 'c' / 'd'
        u16*  mpString;    // 's'
        void* mpPointer;   // 'p'
    };
    ArgSlot laArgs[KI_LOCALE_MAX_ARGS];

    // strlen(pszArgSpec): number of conversion slots.
    int liArgCount = 0;
    if (pszArgSpec)
    {
        const char* lpScan = pszArgSpec;
        while (*lpScan++)
            ;
        liArgCount = static_cast<int>(lpScan - pszArgSpec - 1);
    }

    // Step 1 -- pack the varargs per the spec.
    va_list lArgs;
    va_start(lArgs, pszArgSpec);
    for (int li = 0; li < liArgCount; ++li)
    {
        const char lcSpec = pszArgSpec[li];
        switch (lcSpec)
        {
        case 'c':
            // sign-extended byte (asm: lbz then extsb of the consumed slot).
            laArgs[li].miValue = static_cast<signed char>(va_arg(lArgs, int));
            break;
        case 'd':
            laArgs[li].miValue = va_arg(lArgs, int);
            break;
        case 'p':
            laArgs[li].mpPointer = va_arg(lArgs, void*);
            break;
        case 's':
            laArgs[li].mpString = va_arg(lArgs, u16*);
            break;
        default:
            // Anything else: the asm stores nothing for this slot.
            break;
        }
    }
    va_end(lArgs);

    // Step 2 -- fetch the localized template via the callback.
    if (!g_pfnLocaleGetStr)
        return nullptr;
    const u16* lpTemplate = g_pfnLocaleGetStr();
    if (!lpTemplate)
        return nullptr;

    // Step 3 -- expand into the current rotating buffer.
    const int liBufferIndex = guLocaleBufferIndex;
    u16* lpOut = gauLocaleStringBuffers[liBufferIndex];

    // Scratch for decimal conversion (asm uses a 32-byte stack buffer v40).
    char laDecimal[32];

    bool lbAfterPercent = false;
    do
    {
        const u16 luCh = *lpTemplate;
        if (lbAfterPercent)
        {
            const int liArg = static_cast<s16>(luCh) - '1';  // digit -> slot
            const char lcSpec = pszArgSpec[liArg];
            if (lcSpec == 'd')
            {
                // Signed-decimal expansion of laArgs[liArg].miValue.
                int liVal = laArgs[liArg].miValue;
                if (liVal < 0)
                {
                    liVal = -liVal;
                    *lpOut++ = static_cast<u16>('-');
                }

                // Count digits (do/while: at least one).
                int liDigits = 0;
                int liTmp = liVal;
                do
                {
                    ++liDigits;
                    liTmp /= 10;
                } while (liTmp);

                // Emit digits low-to-high into laDecimal then read forward.
                laDecimal[liDigits] = 0;
                char* lpDigit = &laDecimal[liDigits];
                int liRemaining = liDigits;
                do
                {
                    --lpDigit;
                    --liRemaining;
                    *lpDigit = static_cast<char>(liVal % 10 + '0');
                    liVal /= 10;
                } while (liRemaining);

                for (const char* lpStr = laDecimal; *lpStr; ++lpStr)
                    *lpOut++ = static_cast<u16>(static_cast<signed char>(*lpStr));
            }
            else if (lcSpec == 's')
            {
                // Wide-string expansion.
                for (const u16* lpStr = laArgs[liArg].mpString; *lpStr; ++lpStr)
                    *lpOut++ = *lpStr;
            }
            lbAfterPercent = false;
        }
        else
        {
            if (luCh == static_cast<u16>('%'))
            {
                lbAfterPercent = true;
            }
            else if (luCh != static_cast<u16>('"'))
            {
                *lpOut++ = luCh;
            }
        }
    } while (*lpTemplate++);

    *lpOut = 0;

    // Advance the rotating index (idx + 1) % 5.
    guLocaleBufferIndex = (liBufferIndex + 1) % KI_LOCALE_BUFFER_COUNT;

    return gauLocaleStringBuffers[liBufferIndex];
}

} // namespace Locale
} // namespace RealmcCore
