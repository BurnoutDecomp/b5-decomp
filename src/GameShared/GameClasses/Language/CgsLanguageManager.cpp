#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"          // CgsContainers::CgsHash::CalculateHash

// [stub] CgsLanguage::LanguageManager::IsUsingMetricUnits -- pulled in transitively by
// CgsGui::StateInterface::IsUsingMetricUnits (not used on the boot-video path). The real impl reads the
// SKU/locale; returning false (imperial) is a safe placeholder until the language manager is reconstructed.

namespace CgsLanguage
{
    // X360 0x827DF9C8 CgsLanguage::LanguageManager::LanguageManager.
    //
    // Faithful decompile of the member-wise init the ctor actually performs: it does NOT
    // zero-initialise the whole object (meLanguage / mpcDefaultFontName / mpResource / the
    // allocator+format-string pointers / the metric flag are left untouched -- populated later by
    // Construct(), not yet reconstructed), it only:
    //   1. Seeds every mStrings hash bin's BaseLinkedList::miCount to the uninitialised sentinel
    //      (0x7FFFFFFF) -- the mpFirst/mpLast halves of each bin are left at whatever the compiler's
    //      implicit member-wise construction already put there, matching the asm (only miCount is
    //      explicitly stored in this loop).
    //   2. Seeds mDynamicStringElements: miNumNodes = 1024, then InternalInit's its free sublist over
    //      the embedded node pool (free list owns every node) and its live sublist empty.
    //   3. Repeats step 2 for mDynamicStringPointerElements.
    //   4. Stamps mDebugComponent's vtable pointer (the CgsDev::DebugComponent base ctor folded/
    //      inlined into this one on the X360 build).
    LanguageManager::LanguageManager()
    {
        // mStrings' 13 bins are each a BaseLinkedList; the X360 explicitly pokes every bin's miCount
        // to the uninitialised sentinel (0x7FFFFFFF) because on that build the manager is placed into
        // raw/uninitialised memory. Here mStrings is a plain by-value member, so the implicit
        // member-wise construction already default-constructs each Bin via
        // BaseLinkedList::BaseLinkedList() -- which sets mpFirst=mpLast=0, miCount=sentinel -- the
        // exact state the X360 loop stamps. No explicit action is needed for mStrings.

        // mDynamicStringElements / mDynamicStringPointerElements: seed miNumNodes=1024, then
        // InternalInit the free sublist over the embedded node pool (free list owns every node) and
        // the live sublist empty. Matches the X360's two back-to-back InternalInit call pairs.
        mDynamicStringElements.Construct();
        mDynamicStringPointerElements.Construct();

        // mDebugComponent's vtable pointer is stamped by its own (CgsDev::DebugComponent-derived)
        // default constructor as part of this object's member-wise construction -- reproducing the
        // X360's inlined base-class vtable store with no explicit action needed here.
    }

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

    // X360 0x824434C0 CgsLanguage::LanguageManager::GetDefaultFont.
    //
    // Faithful decompile: assert the loaded default font name is non-null (mpcDefaultFontName,
    // set up by PrepareDefaultFont() during Construct(), not yet reconstructed), then return it --
    // the X360 returns the pointer either way, even after a failed assert (asserts are
    // non-fatal/continue in this codebase).
    const char* LanguageManager::GetDefaultFont() const
    {
        CGS_ASSERT(mpcDefaultFontName != 0, "Invalid Default Font Name in Language Manager");
        return mpcDefaultFontName;
    }

    // ------------------------------------------------------------------------
    // FLAG trap-stub bodies (link scaffold, 2026-07-01): the ten Format*String
    // members below are declared (DWARF) and referenced by the debug component's
    // RenderHUD (CgsLanguageManagerDebugComponent.cpp, pulled in by the by-value
    // mDebugComponent member landed in wave f0de9b78), but their reconstructions
    // have not landed yet. Trap bodies per the stub scaffold -- reachable only
    // through the debug language HUD, never on the boot path. Replace each with
    // its faithful decompile.
    // ------------------------------------------------------------------------
    void LanguageManager::FormatIntegerString(char*, s32, s32) const                      { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatXoverYString(char*, s32, s32, s32) const                  { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatPercentageString(char*, s32, s32) const                   { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatCurrencyString(char*, s32, s32) const                     { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatDateString(char*, s32, s32, s32, s32) const               { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatHoursMinutesAndSecondsString(char*, f32, s32) const       { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatMinutesAndSecondsString(char*, f32, s32) const            { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatMinutesAndSecondsAndHundredsString(char*, f32, s32) const { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatSecondsAndHundredsString(char*, f32, s32) const           { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatSecondsString(char*, f32, s32) const                      { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatSmallDistanceString(char*, f32, s32) const                { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatLargeDistanceString(char*, f32, s32) const                { __debugbreak(); }   // FLAG trap-stub
    f32  LanguageManager::GetDistanceDisplayScale() const                                 { __debugbreak(); return 1.0f; }   // FLAG trap-stub
}
