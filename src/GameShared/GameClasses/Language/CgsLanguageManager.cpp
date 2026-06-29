#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"          // CgsContainers::CgsHash::CalculateHash

// [stub] CgsLanguage::LanguageManager::IsUsingMetricUnits -- pulled in transitively by
// CgsGui::StateInterface::IsUsingMetricUnits (not used on the boot-video path). The real impl reads the
// SKU/locale; returning false (imperial) is a safe placeholder until the language manager is reconstructed.

namespace CgsLanguage
{
    bool LanguageManager::IsUsingMetricUnits() const
    {
        return false;
    }

    // X360 0x828646A0 CgsLanguage::LanguageManager::FindString.
    //
    // Faithful decompile of the validate -> hash -> table-lookup front-end. The X360:
    //   1. Brackets the whole call in a PerfMonCpu monitor (dword_82F2769C). PerfMon is pure
    //      profiling with no observable effect on the returned string, and the X360 registers
    //      that monitor handle in a global the modelled engine has no slot for -- emitting it
    //      here would introduce a fresh unresolved global, so the instrumentation is omitted.
    //      // FLAG: PerfMonCpu Start/StopMonitor instrumentation dropped (profiling only).
    //   2. Asserts the key is non-NULL ("NULL string passed in as hash ID to ...", line 228).
    //   3. strlen's the key and asserts it is <= 128 ("Overlong string ...", line 229).
    //   4. v8 = CgsHash::CalculateHash(key, strlen) -- the committed reflected CRC-32 hash.
    //   5. StringByHash = FindStringByHash(this, v8) -- the loaded-table lookup.
    //   6. Then branches on two diagnostic flag bytes deep in the object:
    //        *(this + 0x6165) (show-keys QA mode)  -> return the raw key,
    //        *(this + 0x6166) (length-placeholder) -> return *(this + 4*(min(len,5) + 0x1859)),
    //        otherwise                              -> return the resolved string.
    //      Those two flag fields + the placeholder pointer table sit ~24 KB into the manager,
    //      far past the modelled header, and exist only for localisation QA -- out of scope for
    //      the boot/render path, which always takes the third (normal) branch.
    //      // FLAG: the two QA diagnostic branches (+0x6165 / +0x6166 placeholder table) are
    //      // deferred; the normal "return the resolved string" branch is reproduced.
    //
    // Common-boot fallback: when the localised-string table is not yet populated FindStringByHash
    // returns NULL; we fall back to returning the key itself (passthrough). This matches the X360
    // show-keys diagnostic result for an unresolved key and keeps the text path rendering the raw
    // key string rather than a NULL deref while the table subsystem is still being reconstructed.
    const u8* LanguageManager::FindString(const char* lpcKey) const
    {
        CGS_ASSERT(lpcKey != 0, "NULL string passed in as hash ID to LanguageManager::FindString");

        // strlen, then guard against an overlong key (X360 compares the length against 0x80).
        const char* lpcScan = lpcKey;
        while (*lpcScan)
            ++lpcScan;
        int liLength = static_cast<int>(lpcScan - lpcKey);
        CGS_ASSERT(liLength <= 128, "Overlong string passed as hash ID to LanguageManager::FindString");

        // CalculateHash takes a non-const char* (the committed signature); the key is only read.
        unsigned int luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcKey), liLength);

        const u8* lpResolved = FindStringByHash(luHash);

        // Normal branch: hand back the resolved string. On the common boot case (table empty)
        // fall back to the key passthrough -- see the // FLAG above.
        if (lpResolved)
            return lpResolved;
        return reinterpret_cast<const u8*>(lpcKey);
    }

    // X360 0x82864028 CgsLanguage::LanguageManager::FindStringByHash.
    //
    // Faithful-minimal body. The X360 fetches the table at (this + 8) through a
    // CgsContainers::LinearHashTable<...,13>::Get(hash), asserts the stored pointer is a valid
    // UTF-8 string (CgsUnicode::IsValidUtf8String, line 282), and returns it (or NULL when the
    // table has no entry). That LinearHashTable bucket-walk + the language-table member at +0x8
    // are a deep subsystem not yet wired into the modelled manager object, so a faithful walk
    // here would dereference an unmodelled member and could not link cleanly.
    // // FLAG: the LinearHashTable<...,13>::Get(hash) bucket-walk over the loaded language table
    // // (manager member +0x8) + the IsValidUtf8String guard are deferred. Returns NULL (no
    // // entry) so FindString takes its key-passthrough fallback for the common boot case.
    const u8* LanguageManager::FindStringByHash(unsigned int /*luHash*/) const
    {
        return 0;
    }
}
