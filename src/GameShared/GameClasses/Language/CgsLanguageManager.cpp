#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"          // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"            // CgsUnicode::IsValidUtf8String

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

    // Setter for mbIsUsingMetricUnits (+0x60F4); no packet listing available for this function
    // (not among the packet's 35), but the member/shape is attested by the ctor's X360-gated
    // layout comment above and by IsUsingMetricUnits's paired accessor.
    void LanguageManager::SetUseMetricUnits(bool lbUseMetric)
    {
        mbIsUsingMetricUnits = lbUseMetric;
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
    // The X360 normal branch returns the FindStringByHash result UNCONDITIONALLY (including NULL
    // when the table has no entry for the key) -- the key-passthrough only happens on the
    // diagnostic show-keys branch flagged above, not as a NULL fallback. Do not substitute the key.
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

        return FindStringByHash(luHash);
    }

    // X360 0x82864028 CgsLanguage::LanguageManager::FindStringByHash.
    //
    // Faithful decompile: look the hash up in mStrings (the manager member at +0x8, modelled by
    // this TU's header); on a hit assert the stored pointer is a valid UTF-8 string
    // (CgsUnicode::IsValidUtf8String, line 282 -- non-fatal, matches CGS_ASSERT semantics) and
    // return it, otherwise return NULL (the table has no entry for that hash).
    const u8* LanguageManager::FindStringByHash(unsigned int luHash) const
    {
        const CgsUnicode::CgsUtf8* const* lppString = mStrings.Get(luHash);
        if (!lppString)
            return 0;

        CGS_ASSERT(CgsUnicode::IsValidUtf8String(*lppString), "CgsUnicode::IsValidUtf8String( *luccp )");
        return *lppString;
    }

    // X360 0x828647F8 CgsLanguage::LanguageManager::AddString.
    //
    // Faithful decompile: validate the key/string, hash the key, evict any prior entry for that
    // hash (RemoveStringByHash), heap-allocate an owned copy of lpcString (Malloc(ByteLength+1) +
    // CopyN -- AddString OWNS a private copy, unlike AddStringPointer below), heap-allocate a
    // HashTableElement<u32, const CgsUtf8*> node, stamp its key/value, insert it into mStrings
    // (the hash-table lookup index), and finally chain the same node onto mDynamicStringElements'
    // live list (X360 sub_828622F8(this+0xA8, &node) -- the dynamic-element bookkeeping list's
    // AddTail; the pool/free-list/live-list shape is a LinkedListHelper<Element*,1024>, so the
    // pool-bounded append the X360 performs is exactly LinkedListHelper::AddTail). Always
    // returns true (the X360 body has no failure return).
    bool LanguageManager::AddString(const char* lpcStringId, const u8* lpcString)
    {
        CGS_ASSERT(lpcStringId != 0, "NULL string ID passed to LanguageManager::AddString");
        CGS_ASSERT(lpcString != 0, "NULL localised string passed to LanguageManager::AddString");
        CGS_ASSERT(CgsUnicode::IsValidUtf8String(lpcString), "CgsUnicode::IsValidUtf8String( lpcString )");

        const char* lpcScan = lpcStringId;
        while (*lpcScan)
            ++lpcScan;
        int liLength = static_cast<int>(lpcScan - lpcStringId);
        unsigned int luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcStringId), liLength);

        if (FindStringByHash(luHash))
            RemoveStringByHash(luHash);

        u32 luStringSize = CgsUnicode::ByteLength(lpcString) + 1;
        u8* lpcStringCopy = static_cast<u8*>(mpLanguageAllocator->Malloc(static_cast<s32>(luStringSize), 4));
        CgsUnicode::CopyN(lpcStringCopy, lpcString, static_cast<s32>(luStringSize));

        HashIDStringArray::Element* lpElement =
            static_cast<HashIDStringArray::Element*>(mpLanguageAllocator->Malloc(sizeof(HashIDStringArray::Element), 4));
        lpElement->Set(luHash, lpcStringCopy);
        mStrings.Insert(lpElement);
        mDynamicStringElements.AddTail(lpElement);

        return true;
    }

    // X360 0x82864A08 CgsLanguage::LanguageManager::AddStringPointer.
    //
    // Faithful decompile: same validate/hash/evict shape as AddString, but stores the CALLER's
    // string pointer directly (no Malloc+CopyN -- the caller owns lpcString's lifetime) and
    // probes for a prior entry via FindString (not FindStringByHash) before evicting through
    // RemoveStringPointerByHash. Chains the new node onto mDynamicStringPointerElements instead
    // of mDynamicStringElements (X360 sub_828622F8(this+0x30C4, &node)).
    bool LanguageManager::AddStringPointer(const char* lpcStringId, const u8* lpcString)
    {
        CGS_ASSERT(lpcStringId != 0, "NULL string ID passed to LanguageManager::AddString");
        CGS_ASSERT(lpcString != 0, "NULL localised string passed to LanguageManager::AddString");
        CGS_ASSERT(CgsUnicode::IsValidUtf8String(lpcString), "CgsUnicode::IsValidUtf8String( lpcString )");

        const char* lpcScan = lpcStringId;
        while (*lpcScan)
            ++lpcScan;
        int liLength = static_cast<int>(lpcScan - lpcStringId);
        unsigned int luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcStringId), liLength);

        if (FindString(lpcStringId))
            RemoveStringPointerByHash(luHash);

        HashIDStringArray::Element* lpElement =
            static_cast<HashIDStringArray::Element*>(mpLanguageAllocator->Malloc(sizeof(HashIDStringArray::Element), 4));
        lpElement->Set(luHash, lpcString);
        mStrings.Insert(lpElement);
        mDynamicStringPointerElements.AddTail(lpElement);

        return true;
    }

    // FLAG (PC bring-up shim; see the header note): installs one serialised {hash, string}
    // table entry into mStrings -- the per-entry observable effect of the unreconstructed
    // LanguageManager::Construct (the STATIC string-table install). Unlike AddStringPointer,
    // the element is NOT chained onto mDynamicStringPointerElements: the static table has
    // thousands of entries (the dynamic bookkeeping list holds 1024 nodes and exists so
    // individually-added strings can be REMOVED); the console's static entries live in the
    // bulk mpStringElements array outside the dynamic lists, exactly like these.
    bool LanguageManager::AddStringPointerByHash(unsigned int luHash, const u8* lpcString)
    {
        CGS_ASSERT(lpcString != 0, "NULL localised string passed to LanguageManager::AddStringPointerByHash");

        // FLAG (part of the Construct stand-in): the X360 ctor deliberately leaves mStrings'
        // bins at the "uninitialised" sentinel -- LanguageManager::Construct (unreconstructed)
        // Init()s the table before installing the loaded entries. Mirror that here on the
        // first installed entry, else every Insert/Get fires the container asserts.
        static bool sbTableInit = false;
        if (!sbTableInit)
        {
            sbTableInit = true;
            mStrings.Init();
        }

        if (FindStringByHash(luHash))
            return true;   // already installed (a static-table re-load; keep the first entry)

        HashIDStringArray::Element* lpElement =
            static_cast<HashIDStringArray::Element*>(mpLanguageAllocator->Malloc(sizeof(HashIDStringArray::Element), 4));
        if (lpElement == 0)
            return false;
        lpElement->Set(luHash, lpcString);
        mStrings.Insert(lpElement);

        return true;
    }

    // X360 0x82864950 CgsLanguage::LanguageManager::RemoveString.
    //
    // Faithful decompile: validate, hash the key, and remove the hash's entry via
    // RemoveStringByHash only if FindStringByHash confirms it is currently present.
    bool LanguageManager::RemoveString(const char* lpcStringId)
    {
        CGS_ASSERT(lpcStringId != 0, "NULL string ID passed to LanguageManager::RemoveString");

        const char* lpcScan = lpcStringId;
        while (*lpcScan)
            ++lpcScan;
        int liLength = static_cast<int>(lpcScan - lpcStringId);
        unsigned int luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcStringId), liLength);

        if (FindStringByHash(luHash))
            return RemoveStringByHash(luHash);
        return false;
    }

    // X360 0x828640B0 CgsLanguage::LanguageManager::RemoveStringByHash.
    //
    // Faithful decompile: remove the hash's entry from mStrings, then walk
    // mDynamicStringElements' live list for the node whose element key matches the hash. On a
    // match, recycle the node (X360's InternalRemoveNode(live) + InternalAddHead(free) pair is
    // exactly LinkedListHelper::RecycleNode), free the owned string copy AddString allocated, and
    // free the element itself (mpLanguageAllocator owns both allocations). Returns false when no
    // dynamic-element node matches (table entry absent or entry not owned by this list).
    bool LanguageManager::RemoveStringByHash(unsigned int luHash)
    {
        mStrings.Remove(luHash);

        DynamicHashElementsList::Node* lpNode = mDynamicStringElements.GetHead();
        while (lpNode)
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            if (lpElement && lpElement->GetKey() == luHash)
                break;
            lpNode = static_cast<DynamicHashElementsList::Node*>(lpNode->GetNextNode());
        }

        if (!lpNode)
            return false;

        HashIDStringArray::Element* lpElement = lpNode->mData;
        mDynamicStringElements.RecycleNode(lpNode);
        mpLanguageAllocator->Free(const_cast<CgsUnicode::CgsUtf8*>(lpElement->GetValue()));
        mpLanguageAllocator->Free(lpElement);
        return true;
    }

    // X360 0x82864B30 CgsLanguage::LanguageManager::RemoveStringPointer.
    //
    // Faithful decompile: same shape as RemoveString, evicting through
    // RemoveStringPointerByHash instead.
    bool LanguageManager::RemoveStringPointer(const char* lpcStringId)
    {
        CGS_ASSERT(lpcStringId != 0, "NULL string ID passed to LanguageManager::RemoveString");

        const char* lpcScan = lpcStringId;
        while (*lpcScan)
            ++lpcScan;
        int liLength = static_cast<int>(lpcScan - lpcStringId);
        unsigned int luHash = CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpcStringId), liLength);

        if (FindStringByHash(luHash))
            return RemoveStringPointerByHash(luHash);
        return false;
    }

    // X360 0x82864158 CgsLanguage::LanguageManager::RemoveStringPointerByHash.
    //
    // Faithful decompile: same shape as RemoveStringByHash but walks
    // mDynamicStringPointerElements and does NOT free the string value (AddStringPointer never
    // owned it -- the caller does); only the element node's own allocation is freed.
    bool LanguageManager::RemoveStringPointerByHash(unsigned int luHash)
    {
        mStrings.Remove(luHash);

        DynamicHashElementsList::Node* lpNode = mDynamicStringPointerElements.GetHead();
        while (lpNode)
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            if (lpElement && lpElement->GetKey() == luHash)
                break;
            lpNode = static_cast<DynamicHashElementsList::Node*>(lpNode->GetNextNode());
        }

        if (!lpNode)
            return false;

        HashIDStringArray::Element* lpElement = lpNode->mData;
        mDynamicStringPointerElements.RecycleNode(lpNode);
        mpLanguageAllocator->Free(lpElement);
        return true;
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

    // X360 0x828608D0 CgsLanguage::LanguageManager::Prepare.
    //
    // Faithful decompile: assert the allocator is non-null, stash it (mpLanguageAllocator,
    // +0x60E4), and register the embedded debug component with the debug menu. Always returns
    // true (the X360 body has no failure return).
    bool LanguageManager::Prepare(CgsMemory::HeapMalloc* lpLanguageAllocator)
    {
        CGS_ASSERT(lpLanguageAllocator != 0, "Null pointer for language allocator given");
        mpLanguageAllocator = lpLanguageAllocator;
        mDebugComponent.Register();
        return true;
    }

    // X360 0x82860940 CgsLanguage::LanguageManager::PrepareDefaultFormattingStrings.
    //
    // Faithful decompile: stamp every per-locale format separator/template member with its
    // English-default literal (used before a locale's string table has loaded its own
    // PrepareFormattingStrings values, and restored by UnloadStringTable), and set the metric
    // flag to true (the X360 build's fallback default).
    bool LanguageManager::PrepareDefaultFormattingStrings()
    {
        mrLargeDistanceConversion = 0.001f;
        mrSmallDistanceConversion = 1.0f;

        mpGeneralDecimalSeparator   = reinterpret_cast<const CgsUnicode::CgsUtf8*>(".");
        mpGeneralThousandsSeparator = reinterpret_cast<const CgsUnicode::CgsUtf8*>(",");
        mpGeneralPercentage         = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1%");
        mpGeneralXOverY             = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1/%2");
        mpGeneralCurrencySeparator  = reinterpret_cast<const CgsUnicode::CgsUtf8*>(".");
        mpGeneralCurrency           = reinterpret_cast<const CgsUnicode::CgsUtf8*>("$%1");

        mpTimeFormatDate            = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1/%2/%3");
        mpTimeFormatAll             = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2:%3");
        mpTimeFormatHrsMinsSecs     = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2:%3");
        mpTimeFormatMinsSecsHnds    = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2.%3");
        mpTimeFormatMinsSecs        = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2");
        mpTimeFormatSecsHnds        = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1.%2");
        mpTimeFormatSecs            = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1s");
        mpTimeFormatSecsLong        = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Seconds");
        mpTimeFormatMinSecsMidText  = reinterpret_cast<const CgsUnicode::CgsUtf8*>("1 Min %1 Secs");
        mpTimeFormatMinsSecsMidText = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Mins %2 Secs");

        mpDistanceFormatShort       = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1m");
        mpDistanceFormatShortL      = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Meters");
        mpDistanceFormatLong        = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1km");
        mpDistanceFormatLongL       = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Kilometres");
        mpDistanceFormatIsMetric    = reinterpret_cast<const CgsUnicode::CgsUtf8*>("1");

        mbIsUsingMetricUnits = true;
        return true;
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
