#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"          // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"            // CgsUnicode::IsValidUtf8String
#include "GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h" // CgsResource::LanguageResource (LoadStringTable)

#include <cstring>   // std::strlen / std::strncpy (the FormatText resolver)
#include <cstdlib>   // std::atof / std::atoi (the FormatText value branches)
#include <cmath>     // std::floor (the PPC fsel round-then-correct idiom in the time leaves)

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

    // X360 0x82862490 CgsLanguage::LanguageManager::Construct.
    //
    // The RUNTIME (re-)initialiser -- NOT the string-table loader (a prior comment claimed Construct
    // installs the loaded table; the asm disproves it: it NULLS mpResource and Init's mStrings EMPTY).
    // Store-for-store faithful to the asm: reset the lifecycle to a clean pre-load state, re-seed the
    // two dynamic-string pools (same as the ctor), Init the empty mStrings hash, set metric units on,
    // and construct the embedded debug component. The actual localised-string LOAD (reading the
    // LanguageResource into mStrings) is a SEPARATE function, so AptLoadLanguageStrings's per-entry
    // install path is retired by THAT, not this. Called from CgsGui::ViewModule::Construct (0x828605A0).
    void LanguageManager::Construct()
    {
        mePrepareStage      = E_PREPARESTAGE_START;   // 0 -- stw r29 @0x60EC
        meReleaseStage      = E_RELEASESTAGE_DONE;    // 2 -- stw r11=2 @0x60F0
        mpResource          = nullptr;                //      stw r29 @0x60E8
        mpStringElements    = nullptr;                //      stw r29 @0x60E0
        mpLanguageAllocator = nullptr;                //      stw r29 @0x60E4

        // Re-seed both dynamic-string pools (free sublist owns all 1024 nodes, live empty) -- the
        // X360's two back-to-back InternalInit pairs, identical to the ctor's seeding.
        mDynamicStringElements.Construct();
        mDynamicStringPointerElements.Construct();

        mStrings.Init();                              // __13_::Init(this+8) -- (re-)init the empty hash

        mpcDefaultFontName   = nullptr;               // stw r29 @0x04
        mbIsUsingMetricUnits = true;                  // stb r11=1 @0x60F4

        mDebugComponent.Construct(this);              // LanguageManagerDebugComponent::Construct(&mDebugComponent, this)
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

    // X360 0x828664B8 CgsLanguage::LanguageManager::LoadStringTable.
    //
    // Faithful decompile: assert the language id ("Language ID <id> is invalid" for ids >= 24;
    // streamed on the console -- the StrStream text collapses to the plain string per project
    // convention), unload any prior table, CAlloc the bulk static element block (one hash
    // element per entry, mpStringElements), then insert every {hash, string} pair into
    // mStrings, UTF-8-validating each string. Records the resource (mpResource), re-derives
    // the formatting strings, picks the language's default font ("dfheic" only for language
    // 16, the wide-glyph locale), and stamps meLanguage last (the X360 store order).
    // The entry/offset widths are the x64-widened data layout (see CgsLanguageResourceType.h);
    // parity is by named member.
    void LanguageManager::LoadStringTable(CgsResource::LanguageResource* lpResource)
    {
        CGS_ASSERT(lpResource->meLanguageID < 0x18u, "Language ID is invalid");

        UnloadStringTable();

        // The X360 ctor leaves mStrings' bins at the uninitialised sentinel; the table must be
        // live before the inserts (UnloadStringTable only re-Inits when a table was loaded).
        if (mpResource == 0 && mpStringElements == 0)
            mStrings.Init();

        mpStringElements = static_cast<HashIDStringArray::Element*>(
            mpLanguageAllocator->CAlloc(lpResource->muSize,
                                        static_cast<s32>(sizeof(HashIDStringArray::Element))));
        if (lpResource->muSize)
        {
            const CgsResource::LanguageResourceHashEntry* lpEntries = lpResource->GetEntries();
            for (s32 li = 0; li < lpResource->muSize; ++li)
            {
                const u8* lpcString = reinterpret_cast<const u8*>(
                    static_cast<uintptr_t>(lpEntries[li].mpString));
                CGS_ASSERT(CgsUnicode::IsValidUtf8String(lpcString),
                           "CgsUnicode::IsValidUtf8String( resource.mpEntries[luIndex].GetString() )");

                HashIDStringArray::Element* lpElement = &mpStringElements[li];
                lpElement->Set(static_cast<u32>(lpEntries[li].muHash), lpcString);
                mStrings.Insert(lpElement);
            }
        }

        mpResource = lpResource;
        const u32 luLanguageId = lpResource->meLanguageID;
        // The console re-derives the PER-LOCALE formatting strings here
        // (PrepareFormattingStrings @0x82865B70, reading the loaded table). NOT YET
        // RECONSTRUCTED -- the boot language (English) formats identically through the
        // defaults; land the per-locale variant with the localisation slice.
        PrepareDefaultFormattingStrings();
        mpcDefaultFontName = (luLanguageId == 16u) ? "dfheic" : "NODEFAULTFONTSPECIFIED";
        meLanguage = static_cast<CgsLanguage::ELanguage>(luLanguageId);
    }

    // X360 0x82862540 CgsLanguage::LanguageManager::UnloadStringTable.
    //
    // Faithful decompile: only acts when a table is loaded (mpResource non-null). Re-Init the
    // mStrings bins, null the resource, drain both dynamic-string live lists head-first back
    // to their free sublists -- freeing each dynamic entry's heap string copy (+ its element)
    // for the owned list, and just the element for the pointer list -- then free the bulk
    // static element block and re-derive the default formatting strings.
    void LanguageManager::UnloadStringTable()
    {
        if (mpResource == 0)
            return;

        mStrings.Init();
        mpResource = 0;

        while (DynamicHashElementsList::Node* lpNode = mDynamicStringElements.GetHead())
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            mDynamicStringElements.RecycleNode(lpNode);
            // The owned list heap-copied its string (AddString): free the copy, then the element.
            mpLanguageAllocator->Free(const_cast<CgsUnicode::CgsUtf8*>(lpElement->GetValue()));
            mpLanguageAllocator->Free(lpElement);
        }

        while (DynamicHashElementsList::Node* lpNode = mDynamicStringPointerElements.GetHead())
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            mDynamicStringPointerElements.RecycleNode(lpNode);
            // The pointer list stores the caller's string (caller-owned): free the element only.
            mpLanguageAllocator->Free(lpElement);
        }

        mpLanguageAllocator->Free(mpStringElements);
        mpStringElements = 0;

        PrepareDefaultFormattingStrings();
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

    // X360 0x82864000 -- UNVERIFIED body: that address is not in the export set, so
    // this reconstruction is inferred from the ViewModule::Release call contract (the
    // caller loops until true) and the <=0x28-byte size budget: reset the lifecycle
    // stages, drop the allocator, report done. Export 0x82864000 to verify the exact
    // stores before trusting this beyond the boot path.
    bool LanguageManager::Release()
    {
        meReleaseStage = E_RELEASESTAGE_DONE;
        mePrepareStage = E_PREPARESTAGE_START;
        mpLanguageAllocator = 0;
        return true;
    }

    // The X360 Destruct is the ICF-folded EMPTY function (resolved to
    // BaseCollisionGenerator::Destruct @0x8284CB38, body `;`): the guest tears nothing
    // down here, so neither does this body.
    void LanguageManager::Destruct()
    {
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

    // @ 0x82862650 -- render liValue into the caller's buffer under one of the
    // four integer formats. The out-of-range assert (cpp:940) and the unknown-
    // format assert (cpp:968) both stream on the console; folded static. Every
    // path NUL-terminates the buffer's last byte and returns true.
    bool LanguageManager::FormatText(char* lpacBuffer, u32 luBufferSize, s32 liValue,
                                     ParameterFormatType leType)
    {
        CGS_ASSERT(leType < E_FORMAT_COUNT,
                   "Invalid Localisation Format supplied to TextField::FormatText");   // cpp:940

        switch (leType)
        {
        case E_FORMAT_INTEGER:
            FormatIntegerString(lpacBuffer, liValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_INTEGER_NOSEPERATOR:
            FormatIntegerNoSeperatorString(lpacBuffer, liValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_PERCENTAGE:
            FormatPercentageString(lpacBuffer, liValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_MONEY:
            FormatCurrencyString(lpacBuffer, liValue, static_cast<s32>(luBufferSize));
            break;
        default:
            CGS_ASSERT(false, "Invalid Parameter sent to SetLocalisedText with int");   // cpp:968
            break;
        }

        lpacBuffer[luBufferSize - 1] = 0;
        return true;
    }

    // ------------------------------------------------------------------------
    // The FormatAndAddText family + the positional-parameter formatters
    // (reconstructed from BURNOUT_X360_ARTIST.XEX). Their shared FormatText
    // resolver (@0x82864C48) is still the FLAG trap-stub at the end of this
    // file; these bodies are faithful and take over the real behaviour the
    // moment that resolver lands.
    // ------------------------------------------------------------------------

    // @ 0x828651A0 -- resolve+format lpcSourceText into a 1KB local, then
    // AddString the result under lpcStringId. Returns FormatText's own result
    // (the AddString result is discarded, per the X360 body).
    bool LanguageManager::FormatAndAddText(const char* lpcStringId, const char* lpcSourceText,
                                           ParameterFormatType leType)
    {
        char lacBuffer[1024];
        const bool lbResult = FormatText(lacBuffer, 1024, lpcSourceText, leType);
        AddString(lpcStringId, reinterpret_cast<const u8*>(lacBuffer));
        return lbResult;
    }

    // @ 0x82866450 -- the positional-parameter form: format the source and the
    // liNumParams (const char* text, ParameterFormatType) vararg pairs through
    // FormatTextV, then AddString the result. Returns FormatTextV's result.
    bool LanguageManager::FormatAndAddText(const char* lpcStringId, const char* lpcSourceText,
                                           ParameterFormatType leType, s32 liNumParams, ...)
    {
        char lacBuffer[1024];

        va_list lArguments;
        va_start(lArguments, liNumParams);
        const bool lbResult =
            FormatTextV(lacBuffer, 1024, lpcSourceText, leType, liNumParams, lArguments);
        va_end(lArguments);

        AddString(lpcStringId, reinterpret_cast<const u8*>(lacBuffer));
        return lbResult;
    }

    // @ 0x828651E8 -- resolve+format lpcSourceText into a 1KB local, format each
    // of the liNumParams (1..3) vararg (const char* text, ParameterFormatType)
    // pairs into its own 512-byte slot, then print the slots into the source's
    // %1..%N positional markers, capped at luBufferSize. The X360 walks its
    // register-save-area va cursor with a per-pair "lpArgument" null tripwire
    // (cpp:1086); the va_list itself carries that role here.
    bool LanguageManager::FormatTextV(char* lpacBuffer, u32 luBufferSize,
                                      const char* lpcSourceText, ParameterFormatType leType,
                                      s32 liNumParams, va_list lArguments)
    {
        CGS_ASSERT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText");   // cpp:1062
        CGS_ASSERT(lpcSourceText != 0, "Text field is invalid in LanguageManager::FormatText");  // cpp:1063
        CGS_ASSERT(liNumParams > 0 && liNumParams < 4, "Wrong number of Parameters int SetLocalisedText"); // cpp:1064

        char lacSourceBuffer[1024];
        FormatText(lacSourceBuffer, 1024, lpcSourceText, leType);

        // One 512-byte slot + one argument pointer per parameter (the X360 reserves
        // four pointer slots on its stack; the count is asserted to 1..3 above).
        const CgsUnicode::CgsUtf8* lapUtf8Params[4];
        char lacParamBuffers[4][512];

        for (s32 liParam = 0; liParam < liNumParams; ++liParam)
        {
            CGS_ASSERT(lArguments != 0, "lpArgument");   // cpp:1086 (the console's va cursor check)

            const char* lpcParamText   = va_arg(lArguments, const char*);
            const u32   luParamFormat  = va_arg(lArguments, u32);

            CGS_ASSERT(lpcParamText != 0, "Invalid Text Pointer in Parameterised SetLocalisedText"); // cpp:1093
            CGS_ASSERT(luParamFormat <= 0x14, "Invalid Parameter");                                   // cpp:1094

            FormatText(lacParamBuffers[liParam], 512, lpcParamText,
                       static_cast<ParameterFormatType>(luParamFormat));
            lapUtf8Params[liParam] = reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacParamBuffers[liParam]);
        }

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                           reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSourceBuffer),
                           static_cast<s32>(luBufferSize), lapUtf8Params,
                           static_cast<u8>(liNumParams));
        return true;
    }

    // @ 0x82865480 -- as FormatTextV but the liNumParams (1..3) parameter texts /
    // format types arrive as parallel arrays (the format array read is null-guarded,
    // defaulting to E_FORMAT_TEXT). Returns false when any parameter failed to
    // format (the source-format result is discarded, per the X360 body).
    bool LanguageManager::Obsolete_FormatTextByArray(char* lpacBuffer, u32 luBufferSize,
                                                     const char* lpcSourceText, ParameterFormatType leType,
                                                     s32 liNumParams, const char* const* lppcParams,
                                                     const ParameterFormatType* lpeParamFormatTypes)
    {
        CGS_ASSERT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText");   // cpp:1127
        CGS_ASSERT(lpcSourceText != 0, "Text field is invalid in TextField::SetLocalisedText");  // cpp:1128
        CGS_ASSERT(liNumParams > 0 && liNumParams < 4, "Wrong number of Parameters int SetLocalisedText"); // cpp:1129

        bool lbAnyParamFailed = false;

        char lacSourceBuffer[1024];
        FormatText(lacSourceBuffer, 1024, lpcSourceText, leType);

        const CgsUnicode::CgsUtf8* lapUtf8Params[4];
        char lacParamBuffers[4][512];

        for (s32 liParam = 0; liParam < liNumParams; ++liParam)
        {
            const char* lpcParamText  = lppcParams[liParam];
            const u32   luParamFormat =
                (lpeParamFormatTypes != 0) ? static_cast<u32>(lpeParamFormatTypes[liParam]) : 0;

            CGS_ASSERT(lpcParamText != 0, "Invalid Text Pointer in Parameterised SetLocalisedText"); // cpp:1156
            CGS_ASSERT(luParamFormat <= 0x14, "Invalid Parameter");                                   // cpp:1157

            const bool lbFormatted = FormatText(lacParamBuffers[liParam], 512, lpcParamText,
                                                static_cast<ParameterFormatType>(luParamFormat));
            lbAnyParamFailed |= !lbFormatted;
            lapUtf8Params[liParam] = reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacParamBuffers[liParam]);
        }

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                           reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSourceBuffer),
                           static_cast<s32>(luBufferSize), lapUtf8Params,
                           static_cast<u8>(liNumParams));
        return !lbAnyParamFailed;
    }

    // @ 0x82865710 (DWARF CgsLanguageManager.h:260, cpp:1185/1186) -- FormatText's
    // one-int-parameter sibling and the last unresolved external the HUD-message analyzer
    // mount needs (HandleStuntPerformed's four "STUNT_RUN_MULTIPLIER_*_MULTIPLE" renders in
    // BrnGuiHudMessageAnalyzer_wC_03.cpp).
    //
    // Register map from the asm (r5 == luBufferSize is NEVER READ -- see below):
    //   r3 this · r4 lpacBuffer · r5 luBufferSize · r6 lpcSourceText · r7 leType
    //   r8 liValue · r9 leValueType
    // Body, store-for-store:
    //   0x8286581C-34  var_4D0 := &var_4C0            (the one-entry parameter array)
    //   0x82865834     sub_82864C48(this, var_480, 1024, lpcSourceText, leType)
    //                  == FormatText(char*, u32, const char*, ParameterFormatType)
    //   0x8286584C     FormatText(this, var_4C0, 64, liValue, leValueType)
    //                  == FormatText(char*, u32, s32, ParameterFormatType) @0x82862650
    //   0x82865864     CgsUnicode::_Print(lpacBuffer, var_480, 1024, &var_4D0, 1)
    //   0x82865868     li r3, 1        -- unconditionally true
    //
    // ⚠ CONSOLE DEFECT, reproduced verbatim: `li r5, 0x400` at 0x82865858 passes the
    // SOURCE scratch buffer's size (1024) as _Print's lnTargetStringSize, i.e. as the
    // TARGET cap -- luBufferSize is dropped on the floor. Both console call sites pass 63
    // against a 64-byte stack buffer (HandleStuntPerformed @0x8251B7F0/B93C/BA88/BCDC), so
    // the cap is wrong by 16x on the platform this shipped on too. It does not smash in
    // practice because _Print copies the SOURCE up to its terminator and the cap is only a
    // maximum -- the resolved "%1 spins"-class strings are far shorter than 64. Left as the
    // binary has it (the sibling FormatTextV/Obsolete_FormatTextByArray above DO forward
    // luBufferSize, which is what makes this one visibly an oversight rather than a
    // convention). Do NOT "fix" it to luBufferSize without re-reading 0x82865858.
    bool LanguageManager::FormatTextFromInt(char* lpacBuffer, u32 luBufferSize,
                                            const char* lpcSourceText, ParameterFormatType leType,
                                            s32 liValue, ParameterFormatType leValueType)
    {
        CGS_ASSERT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText");  // cpp:1185
        CGS_ASSERT(lpcSourceText != 0, "Text field is invalid in TextField::SetLocalisedText"); // cpp:1186

        (void)luBufferSize;   // see the CONSOLE DEFECT note above

        char lacSourceBuffer[1024];
        FormatText(lacSourceBuffer, 1024, lpcSourceText, leType);

        char lacValueBuffer[64];
        FormatText(lacValueBuffer, 64, liValue, leValueType);

        const CgsUnicode::CgsUtf8* lapUtf8Params[1];
        lapUtf8Params[0] = reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacValueBuffer);

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                           reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSourceBuffer),
                           1024, lapUtf8Params, 1);
        return true;
    }

    // [H1 wave 2026-08-25] @ 0x82865878 (asserts cpp:1221/1222) -- FormatTextFromInt's float
    // sibling, store-for-store: resolve the source id into a 1024 scratch, render the float
    // value into a 64-byte slot through the FLOAT dispatcher, print into %1.
    // ⚠ The SAME console defect as FormatTextFromInt above, reproduced verbatim: _Print's
    // target cap is the literal 1024 source-scratch size; luBufferSize is dropped.
    bool LanguageManager::FormatTextFromFloat(char* lpacBuffer, u32 luBufferSize,
                                              const char* lpcSourceText, ParameterFormatType leType,
                                              f32 lfValue, ParameterFormatType leValueType)
    {
        CGS_ASSERT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText");  // cpp:1221
        CGS_ASSERT(lpcSourceText != 0, "Text field is invalid in TextField::SetLocalisedText"); // cpp:1222

        (void)luBufferSize;   // see the CONSOLE DEFECT note on FormatTextFromInt

        char lacSourceBuffer[1024];
        FormatText(lacSourceBuffer, 1024, lpcSourceText, leType);

        char lacValueBuffer[64];
        FormatText(lacValueBuffer, 64, lfValue, leValueType);

        const CgsUnicode::CgsUtf8* lapUtf8Params[1];
        lapUtf8Params[0] = reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacValueBuffer);

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                           reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSourceBuffer),
                           1024, lapUtf8Params, 1);
        return true;
    }

    // [H1 wave 2026-08-25] @ 0x82861D88 (asserts cpp:2166..2170) -- the LARGE-distance leaf
    // (format 19; the odometer's STAT_LABEL_DIST_OFFLINE readout rides it). Scale by
    // mrLargeDistanceConversion (the metric/imperial factor), render with ONE decimal place
    // through FloatToString with the locale separators (the two <=1-length tripwires are the
    // inlined UnicodeBuffer::SetThousandsSeparator/SetDecimalPointCharacter, exactly as
    // FormatIntegerString above), stage through a UnicodeBuffer and print into
    // mpDistanceFormatLong's %1. Unlike the FromInt/FromFloat wrappers, the REAL target size
    // is forwarded to _Print here (per the asm -- no 1024 defect in this leaf).
    void LanguageManager::FormatLargeDistanceString(char* lpcTarget, f32 lfValue,
                                                    s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                          // cpp:2166
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");                        // cpp:2167
        CGS_ASSERT(mpDistanceFormatLong != 0, "mpDistanceFormatLong");                 // cpp:2168
        CGS_ASSERT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator");   // cpp:2169
        CGS_ASSERT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator");       // cpp:2170

        const f32 lfScaledValue = mrLargeDistanceConversion * lfValue;

        CgsUnicode::CgsUtf8 lacValue[256];
        CgsUnicode::CgsUtf8 lacThousandsSeparator[4];
        CgsUnicode::CgsUtf8 lacDecimalPointCharacter[4];
        lacValue[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
        {
            lacThousandsSeparator[liByte]    = 0;
            lacDecimalPointCharacter[liByte] = 0;
        }

        // UnicodeBuffer::SetThousandsSeparator, inlined (CgsUnicode.h:598).
        CGS_ASSERT(CgsUnicode::StringLength(mpGeneralThousandsSeparator) <= 1,
                   "StringLength(lUtf8ThousandsSeparator) <= 1");
        CgsUnicode::Copy(lacThousandsSeparator, mpGeneralThousandsSeparator);

        // UnicodeBuffer::SetDecimalPointCharacter, inlined (CgsUnicode.h:617).
        CGS_ASSERT(CgsUnicode::StringLength(mpGeneralDecimalSeparator) <= 1,
                   "StringLength(lUtf8DecimalPointCharacter) <= 1");
        CgsUnicode::Copy(lacDecimalPointCharacter, mpGeneralDecimalSeparator);

        CgsUnicode::FloatToString(lacValue, lfScaledValue, 0, 1,
                                  lacThousandsSeparator, lacDecimalPointCharacter);

        CgsUnicode::UnicodeBuffer lValueBuffer;
        lValueBuffer.Convert(lacValue);

        const CgsUnicode::CgsUtf8* lapUtf8Params[1];
        lapUtf8Params[0] = lValueBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpDistanceFormatLong,
                           liTargetSize, lapUtf8Params, 1);
    }

    // ------------------------------------------------------------------------
    // FLAG trap-stub bodies (link scaffold, 2026-07-01): the Format*String members
    // below are declared (DWARF) and referenced by the debug component's
    // RenderHUD (CgsLanguageManagerDebugComponent.cpp, pulled in by the by-value
    // mDebugComponent member landed in wave f0de9b78), but their reconstructions
    // have not landed yet. Replace each with its faithful decompile.
    //
    // ⚠️ [gateui r5] THE ORIGINAL BANNER'S "reachable only through the debug language HUD,
    // never on the boot path" CLAIM IS REFUTED and has been deleted. The four INTEGER leaves
    // it covered are on the live HUD-message path (RefreshString -> SetLocalisedText ->
    // Obsolete_FormatTextByArray -> FormatText -> here) and their traps killed the process on
    // the first smashed gate; they now have faithful bodies above the float dispatcher.
    // [E1 wave 2026-08-26] THE SIX TIME LEAVES BELOW ARE GONE TOO. RefreshString maps
    // CgsGui::E_HUDMESSAGEPARAMTYPES_TIME -> format 6 -> FormatSecondsString (@0x82863328)
    // through the float dispatcher, so every HUD message carrying a TIME parameter was an
    // int3 kill; formats 3/4/5/6/7/8 now have faithful bodies below the format-2 timer.
    // What is LEFT here is the DISTANCE family (15..18, 20) + FormatXoverYString +
    // FormatHoursMinutesAndSecondsString / FormatHoursAndMinutesAndSecondsString +
    // GetDistanceDisplayScale. Treat the remaining trap bodies as live hazards, not debug-only.
    //
    // SHARED SHAPE OF THE TIME LEAVES (all six + the landed format 2), decoded from the X360:
    //   ROUNDING -- there is no rounding-mode trick and no fctiwz-rounds-for-me shortcut. Each
    //   body computes `fadds/fmadds f0` (the scaled time + flt_82001DA0 = 0.5f), then runs the
    //   compiler's inline FLOOR: `fsel f13, f0, dbl_82001CB8, dbl_82001CB0` picks -2^52 /
    //   +2^52 (image: 0x82001CB8 = c330000000000000 = -4503599627370496.0, 0x82001CB0 =
    //   4330000000000000 = +4503599627370496.0) by the sign of f0; `fsub`+`fadd` of that magic
    //   forces the value into the [2^52,2^53) binade where the ULP is 1.0, i.e. round-to-
    //   nearest-even; then `fsub f11, f0, f13` takes the residue and
    //   `fsel f0, f11, dbl_82001CA8, dbl_82001CA0` subtracts 1.0 (0x82001CA0 = 3ff0000000000000)
    //   when the round went UP and 0.0 (0x82001CA8 = 0000000000000000) when it went down.
    //   Net: floor(). The trailing frsp/fctiwz then truncate an already-integral value.
    //   So every leaf is floor(scaledTime + 0.5f) == round-half-up over the asserted
    //   lfTimeInSeconds >= 0.0f domain (flt_82001CC0 = 0.0f is the compare constant).
    //   SPLITTING -- the divisors are read off the asm, not assumed: 0x3C/60 (mulhw magic
    //   0x88888889, add, srawi 5) for the minute leaves, 0x64/100 (mulhw magic 0x51EB851F,
    //   srawi 5 -- NO add) for the hundredths leaves, and NO divisor at all in formats 6/7.
    //   BUFFERS -- each console stack block is a CgsUnicode::UnicodeBuffer with Reset() and the
    //   Set* separator setters inlined: maBuffer[256], maUtf8ThousandsSeparator[4],
    //   maUtf8DecimalPointCharacter[4], muDecimalPlaces, muMinimumDigits. The bodies zero
    //   [256..264] one stb at a time and then store muMinimumDigits ([265]) explicitly, so the
    //   separator IntToString sees is always EMPTY -- no grouping in a clock readout. Modelled
    //   with explicit locals for the same reason the integer leaves are (Reset / the Set*
    //   setters have no reconstructed bodies in the tree).
    // ------------------------------------------------------------------------
    void LanguageManager::FormatXoverYString(char*, s32, s32, s32) const                  { __debugbreak(); }   // FLAG trap-stub
    // (FormatDateString + the four INTEGER leaves are no longer trap-stubs -- their faithful
    //  bodies are above this block.)
    void LanguageManager::FormatHoursMinutesAndSecondsString(char*, f32, s32) const       { __debugbreak(); }   // FLAG trap-stub

    // @ 0x828629A8 (asserts cpp:1701/1702/1704/1706) -- [H2 wave 2026-08-25] the
    // M:SS.hh timer leaf (format 2; the road-rule panel's running/best time readouts
    // ride it through the float dispatcher, which made this trap-stub a BOOT KILL the
    // moment the RoadRule TU mounted -- 0x80000003 @RVA 0x207ea0, caught by the H2
    // verification run). Round to hundredths, split M / SS / hh, render each through
    // IntToString with NO separator (min digits 1/2/2), and print into
    // mpTimeFormatMinsSecsHnds's three positional parameters. The REAL target size is
    // forwarded to _Print (per the asm -- no 1024 defect in this leaf).
    void LanguageManager::FormatMinutesAndSecondsAndHundredsString(char* lpcTarget, f32 lfTimeInSeconds,
                                                                   s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                // cpp:1701
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");              // cpp:1702
        CGS_ASSERT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f");      // cpp:1704
        CGS_ASSERT(mpTimeFormatMinsSecsHnds != 0, "mpTimeFormatMinsSecsHnds"); // cpp:1706

        // [E1 2026-08-26 comment correction] The H2 banner called the fsel chain
        // "round-to-nearest of seconds*100". It is not: the chain is the compiler's inline
        // FLOOR (see the decode in the shared-shape note below), applied to seconds*100 + 0.5f
        // -- i.e. round-HALF-UP. The CODE below is unchanged and stays correct, because
        // truncation and floor agree over the lfTimeInSeconds >= 0.0f domain this leaf asserts.
        const s32 liTotalHundredths = static_cast<s32>(lfTimeInSeconds * 100.0f + 0.5f);
        const s32 liMinutes    = liTotalHundredths / 6000;
        const s32 liSeconds    = (liTotalHundredths % 6000) / 100;
        const s32 liHundredths = (liTotalHundredths % 6000) % 100;

        CgsUnicode::CgsUtf8 lacMinutes[256];
        CgsUnicode::CgsUtf8 lacSeconds[256];
        CgsUnicode::CgsUtf8 lacHundredths[256];
        CgsUnicode::CgsUtf8 lacNoSeparator[4];
        lacMinutes[0] = lacSeconds[0] = lacHundredths[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacNoSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacMinutes,    liMinutes,    1, lacNoSeparator);
        CgsUnicode::IntToString(lacSeconds,    liSeconds,    2, lacNoSeparator);
        CgsUnicode::IntToString(lacHundredths, liHundredths, 2, lacNoSeparator);

        CgsUnicode::UnicodeBuffer lMinutesBuffer;
        CgsUnicode::UnicodeBuffer lSecondsBuffer;
        CgsUnicode::UnicodeBuffer lHundredthsBuffer;
        lMinutesBuffer.Convert(lacMinutes);
        lSecondsBuffer.Convert(lacSeconds);
        lHundredthsBuffer.Convert(lacHundredths);

        const CgsUnicode::CgsUtf8* lapUtf8Params[3];
        lapUtf8Params[0] = lMinutesBuffer.GetBuffer();
        lapUtf8Params[1] = lSecondsBuffer.GetBuffer();
        lapUtf8Params[2] = lHundredthsBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatMinsSecsHnds, liTargetSize, lapUtf8Params, 3);
    }

    // @ 0x82862C60 (asserts cpp:1764/:1765/:1767/:1769) -- the M:SS leaf (format 3). Round to
    // whole seconds, split M / SS, render both through IntToString with the EMPTY separator,
    // and print into mpTimeFormatMinsSecs ("%1:%2"). The real target size is forwarded to
    // _Print (no 1024 defect in this leaf).
    //
    // ONE LOCALE QUIRK, PINNED FROM THE ASM, NOT ASSUMED: the MINUTES field's minimum-digit
    // count is not a constant.
    //   0x82862D4C  lwz    r11, 0(r29)       ; meLanguage (+0x00)
    //   0x82862D54  addi   r11, r11, -0xF    ; - 15
    //   0x82862D60  cntlzw r11, r11
    //   0x82862D6C  extrwi r11, r11, 1,26    ; bit 26 == (cntlzw >> 5) & 1 == (operand == 0)
    //   0x82862D78  addi   r5,  r11, 1       ; -> 2 when meLanguage == 15, else 1
    // and 15 == CgsLanguage::E_LANGUAGE_ITALIAN (CgsSku.h). So Italian renders "01:07" and
    // every other language renders "1:07". The SECONDS field is a literal 2 (li r5, 2 @
    // 0x82862E34, and the matching muMinimumDigits store `li r11, 2` @ 0x82862E28).
    void LanguageManager::FormatMinutesAndSecondsString(char* lpcTarget, f32 lfTimeInSeconds,
                                                        s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                // cpp:1764
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");              // cpp:1765
        CGS_ASSERT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f");      // cpp:1767
        CGS_ASSERT(mpTimeFormatMinsSecs != 0, "mpTimeFormatMinsSecs");       // cpp:1769

        // fadds flt_82001DA0 (0.5f) + the inline floor -- see the shared-shape note above.
        const s32 liTotalSeconds = static_cast<s32>(std::floor(lfTimeInSeconds + 0.5f));
        const s32 liSeconds = liTotalSeconds % 60;                  // mulli 0x3C / subf
        const s32 liMinutes = (liTotalSeconds - liSeconds) / 60;    // subf / divw r8 == 0x3C

        const u8 lu8MinuteDigits =
            (meLanguage == E_LANGUAGE_ITALIAN) ? static_cast<u8>(2) : static_cast<u8>(1);

        CgsUnicode::CgsUtf8 lacMinutes[256];
        CgsUnicode::CgsUtf8 lacSeconds[256];
        CgsUnicode::CgsUtf8 lacNoSeparator[4];
        lacMinutes[0] = 0;
        lacSeconds[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacNoSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacMinutes, liMinutes, lu8MinuteDigits, lacNoSeparator);
        CgsUnicode::IntToString(lacSeconds, liSeconds, 2, lacNoSeparator);

        CgsUnicode::UnicodeBuffer lMinutesBuffer;
        CgsUnicode::UnicodeBuffer lSecondsBuffer;
        lMinutesBuffer.Convert(lacMinutes);
        lSecondsBuffer.Convert(lacSeconds);

        const CgsUnicode::CgsUtf8* lapUtf8Params[2];
        lapUtf8Params[0] = lMinutesBuffer.GetBuffer();
        lapUtf8Params[1] = lSecondsBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatMinsSecs, liTargetSize, lapUtf8Params, 2);
    }

    // @ 0x82862E98 (asserts cpp:1826/:1827/:1829/:1831) -- the LONG seconds-and-hundredths leaf
    // (format 5). TWO _Prints, not one: it first renders "S.hh" into a SIXTY-FOUR byte stack
    // scratch through mpTimeFormatSecsHnds ("%1.%2") -- `li r5, 0x40` @ 0x8286309C, and the
    // next local starts exactly 0x40 bytes further up the frame -- then feeds that whole string
    // as the single %1 of mpTimeFormatSecsLong ("%1 Seconds") into the caller's buffer.
    //
    // CONSOLE ASSERT SET REPRODUCED VERBATIM: only mpTimeFormatSecsHnds is null-checked
    // (cpp:1831). mpTimeFormatSecsLong is dereferenced at 0x828630C0 with NO tripwire -- do not
    // "helpfully" add one.
    //
    // The multiplier is flt_820049E0, read from the image at file offset 0x49E0 =
    // 42c80000 = 100.0f (fmadds f0, f31, f0, f13 @ 0x82862FA8 -- one fused rounding), and the
    // /100 is the mulhw 0x51EB851F + srawi 5 magic, NOT an assumed "hundredths" divisor.
    void LanguageManager::FormatSecondsAndHundredsStringLong(char* lpcTarget, f32 lfTimeInSeconds,
                                                             s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                // cpp:1826
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");              // cpp:1827
        CGS_ASSERT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f");      // cpp:1829
        CGS_ASSERT(mpTimeFormatSecsHnds != 0, "mpTimeFormatSecsHnds");       // cpp:1831

        const s32 liTotalHundredths =
            static_cast<s32>(std::floor(lfTimeInSeconds * 100.0f + 0.5f));
        const s32 liHundredths = liTotalHundredths % 100;                       // mulli 0x64
        const s32 liSeconds    = (liTotalHundredths - liHundredths) / 100;      // divw r8 == 0x64

        CgsUnicode::CgsUtf8 lacSeconds[256];
        CgsUnicode::CgsUtf8 lacHundredths[256];
        CgsUnicode::CgsUtf8 lacNoSeparator[4];
        lacSeconds[0]    = 0;
        lacHundredths[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacNoSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacSeconds,    liSeconds,    1, lacNoSeparator);
        CgsUnicode::IntToString(lacHundredths, liHundredths, 2, lacNoSeparator);

        CgsUnicode::UnicodeBuffer lSecondsBuffer;
        CgsUnicode::UnicodeBuffer lHundredthsBuffer;
        lSecondsBuffer.Convert(lacSeconds);
        lHundredthsBuffer.Convert(lacHundredths);

        const CgsUnicode::CgsUtf8* lapUtf8Params[2];
        lapUtf8Params[0] = lSecondsBuffer.GetBuffer();
        lapUtf8Params[1] = lHundredthsBuffer.GetBuffer();

        // The 64-byte intermediate is the console's own frame slot, not a chosen size.
        CgsUnicode::CgsUtf8 lacSecondsAndHundredths[64];
        CgsUnicode::_Print(lacSecondsAndHundredths, mpTimeFormatSecsHnds, 64,
                           lapUtf8Params, 2);

        CgsUnicode::UnicodeBuffer lCombinedBuffer;
        lCombinedBuffer.Convert(lacSecondsAndHundredths);
        lapUtf8Params[0] = lCombinedBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatSecsLong, liTargetSize, lapUtf8Params, 1);
    }

    // @ 0x828630F8 (asserts cpp:1882/:1883/:1885/:1887) -- the S.hh leaf (format 4): the same
    // *100 round and /100 split as the Long variant above, printed straight into the caller's
    // buffer through mpTimeFormatSecsHnds ("%1.%2") with no intermediate. Seconds carry
    // minimum-digits 1 (li r5, 1 @ 0x828631D8 with the matching muMinimumDigits store
    // `li r11, 1` @ 0x82863204), hundredths carry 2 (@ 0x828632B8 / 0x828632C4).
    void LanguageManager::FormatSecondsAndHundredsString(char* lpcTarget, f32 lfTimeInSeconds,
                                                         s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                // cpp:1882
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");              // cpp:1883
        CGS_ASSERT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f");      // cpp:1885
        CGS_ASSERT(mpTimeFormatSecsHnds != 0, "mpTimeFormatSecsHnds");       // cpp:1887

        const s32 liTotalHundredths =
            static_cast<s32>(std::floor(lfTimeInSeconds * 100.0f + 0.5f));
        const s32 liHundredths = liTotalHundredths % 100;
        const s32 liSeconds    = (liTotalHundredths - liHundredths) / 100;

        CgsUnicode::CgsUtf8 lacSeconds[256];
        CgsUnicode::CgsUtf8 lacHundredths[256];
        CgsUnicode::CgsUtf8 lacNoSeparator[4];
        lacSeconds[0]    = 0;
        lacHundredths[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacNoSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacSeconds,    liSeconds,    1, lacNoSeparator);
        CgsUnicode::IntToString(lacHundredths, liHundredths, 2, lacNoSeparator);

        CgsUnicode::UnicodeBuffer lSecondsBuffer;
        CgsUnicode::UnicodeBuffer lHundredthsBuffer;
        lSecondsBuffer.Convert(lacSeconds);
        lHundredthsBuffer.Convert(lacHundredths);

        const CgsUnicode::CgsUtf8* lapUtf8Params[2];
        lapUtf8Params[0] = lSecondsBuffer.GetBuffer();
        lapUtf8Params[1] = lHundredthsBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatSecsHnds, liTargetSize, lapUtf8Params, 2);
    }

    // @ 0x82863328 (asserts cpp:1935/:1936/:1938/:1940) -- the bare-seconds leaf (format 6).
    // THE ONE THE HUD ACTUALLY HITS: InGameMessagesComponent::RefreshString maps
    // CgsGui::E_HUDMESSAGEPARAMTYPES_TIME onto format 6, so every parameterised HUD message
    // carrying a time landed on this function's __debugbreak().
    //
    // FAMOUS-VALUE TRAP AVOIDED: there is NO /60 and NO %60 anywhere in this body -- no
    // mulhw/mulli/divw at all between the fctiwz and the IntToString. The rounded value goes
    // to the formatter WHOLE, so 3661 seconds renders "3661s", not "1:01". minimum-digits is a
    // literal 1 (li r5, 1 @ 0x828633F8; muMinimumDigits store `li r11, 1` @ 0x82863434).
    void LanguageManager::FormatSecondsString(char* lpcTarget, f32 lfTimeInSeconds,
                                              s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                // cpp:1935
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");              // cpp:1936
        CGS_ASSERT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f");      // cpp:1938
        CGS_ASSERT(mpTimeFormatSecs != 0, "mpTimeFormatSecs");               // cpp:1940

        const s32 liTotalSeconds = static_cast<s32>(std::floor(lfTimeInSeconds + 0.5f));

        CgsUnicode::CgsUtf8 lacSeconds[256];
        CgsUnicode::CgsUtf8 lacNoSeparator[4];
        lacSeconds[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacNoSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacSeconds, liTotalSeconds, 1, lacNoSeparator);

        CgsUnicode::UnicodeBuffer lSecondsBuffer;
        lSecondsBuffer.Convert(lacSeconds);

        const CgsUnicode::CgsUtf8* lapUtf8Params[1];
        lapUtf8Params[0] = lSecondsBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatSecs, liTargetSize, lapUtf8Params, 1);
    }

    // @ 0x828634C8 (asserts cpp:1974/:1975/:1977/:1979) -- format 7. Instruction-for-instruction
    // the same body as format 6 above (same 0.5f, same floor chain, same minimum-digits 1, same
    // absence of any 60/100 divisor); the ONLY differences are the assert line numbers, the
    // tripwire string, and the template pointer: mpTimeFormatSecsLong (+0x6134, "%1 Seconds")
    // instead of mpTimeFormatSecs (+0x6130, "%1s").
    void LanguageManager::FormatSecondsStringLong(char* lpcTarget, f32 lfTimeInSeconds,
                                                  s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                // cpp:1974
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");              // cpp:1975
        CGS_ASSERT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f");      // cpp:1977
        CGS_ASSERT(mpTimeFormatSecsLong != 0, "mpTimeFormatSecsLong");       // cpp:1979

        const s32 liTotalSeconds = static_cast<s32>(std::floor(lfTimeInSeconds + 0.5f));

        CgsUnicode::CgsUtf8 lacSeconds[256];
        CgsUnicode::CgsUtf8 lacNoSeparator[4];
        lacSeconds[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacNoSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacSeconds, liTotalSeconds, 1, lacNoSeparator);

        CgsUnicode::UnicodeBuffer lSecondsBuffer;
        lSecondsBuffer.Convert(lacSeconds);

        const CgsUnicode::CgsUtf8* lapUtf8Params[1];
        lapUtf8Params[0] = lSecondsBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatSecsLong, liTargetSize, lapUtf8Params, 1);
    }

    // @ 0x82863668 (asserts cpp:2013/:2014/:2016/:2018/:2019) -- format 8, the prose leaf. FIVE
    // tripwires, not four: BOTH mid-text templates are null-checked, in this order --
    // mpTimeFormatMinsSecsMidText (+0x613C, cpp:2018) then mpTimeFormatMinSecsMidText
    // (+0x6138, cpp:2019) -- even though only one of them is used per call.
    //
    // THE SHAPE IS A SINGULAR/PLURAL FORK, not a formatting choice: `cmpwi cr6, r4, 1` /
    // `bne cr6, loc_82863850` @ 0x828637E0 tests the MINUTES count against 1.
    //   minutes == 1 -> ONE parameter (the seconds, minimum-digits 2) through
    //                   mpTimeFormatMinSecsMidText, whose default text is "1 Min %1 Secs" --
    //                   the "1" is baked into the template, which is exactly why the minutes
    //                   value is not passed on this arm.
    //   otherwise    -> TWO parameters (minutes minimum-digits 1, seconds minimum-digits 2)
    //                   through mpTimeFormatMinsSecsMidText ("%1 Mins %2 Secs").
    // Same 0.5f floor and the same 0x3C/60 split as format 3.
    void LanguageManager::FormatMinutesSecondsStringMediumText(char* lpcTarget, f32 lfTimeInSeconds,
                                                               s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                            // cpp:2013
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");                          // cpp:2014
        CGS_ASSERT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f");                  // cpp:2016
        CGS_ASSERT(mpTimeFormatMinsSecsMidText != 0, "mpTimeFormatMinsSecsMidText");     // cpp:2018
        CGS_ASSERT(mpTimeFormatMinSecsMidText != 0, "mpTimeFormatMinSecsMidText");       // cpp:2019

        const s32 liTotalSeconds = static_cast<s32>(std::floor(lfTimeInSeconds + 0.5f));
        const s32 liSeconds = liTotalSeconds % 60;
        const s32 liMinutes = (liTotalSeconds - liSeconds) / 60;

        CgsUnicode::CgsUtf8 lacMinutes[256];
        CgsUnicode::CgsUtf8 lacSeconds[256];
        CgsUnicode::CgsUtf8 lacNoSeparator[4];
        lacMinutes[0] = 0;
        lacSeconds[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacNoSeparator[liByte] = 0;

        const CgsUnicode::CgsUtf8* lapUtf8Params[2];

        if (liMinutes == 1)
        {
            CgsUnicode::IntToString(lacSeconds, liSeconds, 2, lacNoSeparator);

            CgsUnicode::UnicodeBuffer lSecondsBuffer;
            lSecondsBuffer.Convert(lacSeconds);
            lapUtf8Params[0] = lSecondsBuffer.GetBuffer();

            CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                               mpTimeFormatMinSecsMidText, liTargetSize, lapUtf8Params, 1);
            return;
        }

        CgsUnicode::IntToString(lacMinutes, liMinutes, 1, lacNoSeparator);
        CgsUnicode::IntToString(lacSeconds, liSeconds, 2, lacNoSeparator);

        CgsUnicode::UnicodeBuffer lMinutesBuffer;
        CgsUnicode::UnicodeBuffer lSecondsBuffer;
        lMinutesBuffer.Convert(lacMinutes);
        lSecondsBuffer.Convert(lacSeconds);

        lapUtf8Params[0] = lMinutesBuffer.GetBuffer();
        lapUtf8Params[1] = lSecondsBuffer.GetBuffer();

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatMinsSecsMidText, liTargetSize, lapUtf8Params, 2);
    }

    void LanguageManager::FormatSmallDistanceString(char*, f32, s32) const                { __debugbreak(); }   // FLAG trap-stub
    // (FormatLargeDistanceString is no longer a trap-stub -- its faithful body is above
    //  this block, landed with the H1 odometer wave.)
    f32  LanguageManager::GetDistanceDisplayScale() const                                 { __debugbreak(); return 1.0f; }   // FLAG trap-stub
    // FLAG trap-stubs for the float-dispatch leaves 0x828641F0 references beyond the
    // block above (declared additions; decompiles land with the value-format slice).
    void LanguageManager::FormatAutoDistanceString(char*, f32, s32) const                 { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatAutoDistanceStringLong(char*, f32, s32) const             { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatSmallDistanceStringLong(char*, f32, s32) const            { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatLargeDistanceStringLong(char*, f32, s32) const            { __debugbreak(); }   // FLAG trap-stub
    void LanguageManager::FormatHoursAndMinutesAndSecondsString(u8*, f32, s32) const      { __debugbreak(); }   // FLAG trap-stub

    // ------------------------------------------------------------------------
    // @ 0x828615F8 (CgsLanguageManager.cpp:1566/:1567 own its two asserts) -- print a
    // day/month/year triple through the locale's date template. Lifted out of the trap-stub
    // block above (2026-07-30): it is on the driver-licence path --
    // BrnGui::LicenseComponent::SetProfilePointer @0x824B3248 formats the profile's licence
    // issue date through it -- so a __debugbreak() there took the intro down.
    //
    // The X360 body renders each field with CgsUnicode::IntToString (day/month zero-padded to
    // 2 digits, year to 4) with an EMPTY thousands separator (the stack descriptor it passes
    // leads with a NUL byte), stages the three results through UnicodeBuffer::Convert, and
    // hands them to _Print against mpTimeFormatDate ("%1/%2/%3" from
    // PrepareDefaultFormattingStrings).
    // ------------------------------------------------------------------------
    void LanguageManager::FormatDateString(char* lpcTarget, s32 liDays, s32 liMonths,
                                           s32 liYears, s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");   // cpp:1566
        CGS_ASSERT(mpTimeFormatDate != 0, "mpTimeFormatDate");  // cpp:1567

        // The X360 stack descriptors are memset to zero before use, so the separator string
        // each IntToString sees is empty -- no thousands separators in a date.
        static const CgsUnicode::CgsUtf8 kacNoSeparator[1] = { 0 };

        CgsUnicode::CgsUtf8 lacDay[256];
        CgsUnicode::CgsUtf8 lacMonth[256];
        CgsUnicode::CgsUtf8 lacYear[256];

        CgsUnicode::IntToString(lacDay,   liDays,   2, kacNoSeparator);
        CgsUnicode::IntToString(lacMonth, liMonths, 2, kacNoSeparator);
        CgsUnicode::IntToString(lacYear,  liYears,  4, kacNoSeparator);

        CgsUnicode::UnicodeBuffer lDayBuffer;
        CgsUnicode::UnicodeBuffer lMonthBuffer;
        CgsUnicode::UnicodeBuffer lYearBuffer;
        lDayBuffer.Convert(lacDay);
        lMonthBuffer.Convert(lacMonth);
        lYearBuffer.Convert(lacYear);

        const CgsUnicode::CgsUtf8* lapArgs[3] =
        {
            lDayBuffer.GetBuffer(),
            lMonthBuffer.GetBuffer(),
            lYearBuffer.GetBuffer(),
        };

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpTimeFormatDate, liTargetSize, lapArgs, 3);
    }

    // ------------------------------------------------------------------------
    // [gateui r5] THE FOUR INTEGER LEAVES, lifted out of the trap-stub block below.
    //
    // The trap block's banner claimed these were "reachable only through the debug language
    // HUD, never on the boot path". THAT PREMISE IS REFUTED -- they are on the HUD-message
    // path, and the __debugbreak() bodies killed the game on the first smashed gate:
    //   host rva 0x1B951A = FormatText(char*,u32,s32,ParameterFormatType) @0x82862650
    //      + 0x7A, EXCEPTION 0x80000003 (int3, NOT an access violation) -- the four leaf
    //      calls inlined to nothing but their __debugbreak, and MSVC then folded ALL FOUR
    //      case arms (E_FORMAT_INTEGER / _NOSEPERATOR / _PERCENTAGE / _MONEY) onto that ONE
    //      int3 at +0x7A, jumped to by all four `jz` arms while the default/assert path
    //      hops over it (`eb 01 cc`). So any of the four killed the process.
    //   <- FormatText(char*,u32,const char*,ParameterFormatType) @0x82864C48 case 11..14
    //   <- Obsolete_FormatTextByArray @0x82865480
    //   <- BrnFlapt::TextFieldRef::SetLocalisedText <- InGameMessagesComponent::RefreshString.
    // RefreshString maps CgsGui::E_HUDMESSAGEPARAMTYPES_INT -> format 11 and _MONEY -> 14
    // (its dword_8204B83C table), so EVERY parameterised HUD message lands here.
    //
    // Shared shape (all four): the console's stack block IS a CgsUnicode::UnicodeBuffer with
    // Reset() + the separator setters + Convert(s32) inlined -- maBuffer[256], then
    // maUtf8ThousandsSeparator[4], maUtf8DecimalPointCharacter[4], muDecimalPlaces,
    // muMinimumDigits (the ten tail bytes each body zeroes one stb at a time). Modelled with
    // explicit locals rather than a UnicodeBuffer object because Reset / the Set*
    // separator setters / Convert(s32) have no reconstructed bodies in the tree yet (only
    // Convert(const CgsUtf8*) does) -- same call sequence, no unresolved external.
    // muMinimumDigits is read straight back out of that zeroed block, i.e. always 0.
    // ------------------------------------------------------------------------

    // @ 0x828610B0 (asserts cpp:1418/:1419/:1420/:1421). Render liValue with the locale's
    // thousands separator. The decimal-point character is copied into the buffer too (the
    // inlined SetDecimalPointCharacter) even though an integer render never consumes it --
    // reproduced, because its <=1-character tripwire is a real side effect.
    void LanguageManager::FormatIntegerString(char* lpcTarget, s32 liValue, s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                          // cpp:1418
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");                        // cpp:1419
        CGS_ASSERT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator");   // cpp:1420
        CGS_ASSERT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator");       // cpp:1421

        CgsUnicode::CgsUtf8 lacValue[256];
        CgsUnicode::CgsUtf8 lacThousandsSeparator[4];
        CgsUnicode::CgsUtf8 lacDecimalPointCharacter[4];
        const u8 lu8MinimumDigits = 0;   // the buffer's muMinimumDigits, zeroed with the block

        lacValue[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
        {
            lacThousandsSeparator[liByte]    = 0;
            lacDecimalPointCharacter[liByte] = 0;
        }

        // UnicodeBuffer::SetDecimalPointCharacter, inlined (CgsUnicode.h:617).
        CGS_ASSERT(CgsUnicode::StringLength(mpGeneralDecimalSeparator) <= 1,
                   "StringLength(lUtf8DecimalPointCharacter) <= 1");
        CgsUnicode::Copy(lacDecimalPointCharacter, mpGeneralDecimalSeparator);

        // UnicodeBuffer::SetThousandsSeparator, inlined (CgsUnicode.h:598).
        CGS_ASSERT(CgsUnicode::StringLength(mpGeneralThousandsSeparator) <= 1,
                   "StringLength(lUtf8ThousandsSeparator) <= 1");
        CgsUnicode::Copy(lacThousandsSeparator, mpGeneralThousandsSeparator);

        CgsUnicode::IntToString(lacValue, liValue, lu8MinimumDigits, lacThousandsSeparator);
        CgsUnicode::CopyN(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget), lacValue,
                          liTargetSize);
    }

    // @ 0x82861248 (asserts cpp:1453..:1456). Same as above with NO separator set: the
    // console passes the Reset()-zeroed (empty) thousands buffer and a literal 0 minimum-digit
    // count straight to IntToString, so no grouping is spliced in.
    void LanguageManager::FormatIntegerNoSeperatorString(char* lpcTarget, s32 liValue,
                                                         s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                          // cpp:1453
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");                        // cpp:1454
        CGS_ASSERT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator");   // cpp:1455
        CGS_ASSERT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator");       // cpp:1456

        CgsUnicode::CgsUtf8 lacValue[256];
        CgsUnicode::CgsUtf8 lacThousandsSeparator[4];

        lacValue[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacThousandsSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacValue, liValue, 0, lacThousandsSeparator);
        CgsUnicode::CopyN(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget), lacValue,
                          liTargetSize);
    }

    // @ 0x828614D8 (asserts cpp:1529/:1530/:1532). Render liValue with no grouping, copy it
    // into the caller's buffer, then OVERWRITE that buffer by printing the rendered digits
    // through the locale's percentage template ("%1%"). The intermediate CopyN is the
    // console's own -- @0x828615BC, before the _Print at @0x828615EC -- and is kept.
    void LanguageManager::FormatPercentageString(char* lpcTarget, s32 liValue,
                                                 s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");        // cpp:1529
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");      // cpp:1530
        CGS_ASSERT(mpGeneralPercentage != 0, "mpGeneralPercentage"); // cpp:1532

        CgsUnicode::CgsUtf8 lacValue[256];
        CgsUnicode::CgsUtf8 lacThousandsSeparator[4];

        lacValue[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
            lacThousandsSeparator[liByte] = 0;

        CgsUnicode::IntToString(lacValue, liValue, 0, lacThousandsSeparator);
        CgsUnicode::CopyN(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget), lacValue,
                          liTargetSize);

        CgsUnicode::UnicodeBuffer lValueBuffer;
        lValueBuffer.Convert(lacValue);

        const CgsUnicode::CgsUtf8* lapArgs[1] = { lValueBuffer.GetBuffer() };
        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpGeneralPercentage, liTargetSize, lapArgs, 1);
    }

    // @ 0x82860F38 (asserts cpp:1384/:1385/:1387/:1388). Render liValue grouped by the
    // locale's CURRENCY separator (mpGeneralCurrencySeparator takes the thousands-separator
    // slot -- the inlined setter's tripwire still reads "lUtf8ThousandsSeparator"), then print
    // it through the currency template ("$%1"). No intermediate CopyN here, unlike the
    // percentage leaf.
    void LanguageManager::FormatCurrencyString(char* lpcTarget, s32 liCurrencyValue,
                                               s32 liTargetSize) const
    {
        CGS_ASSERT(lpcTarget != 0, "lpTargetString != NULL");                        // cpp:1384
        CGS_ASSERT(liTargetSize > 0, "lnTargetStringSize > 0");                      // cpp:1385
        CGS_ASSERT(mpGeneralCurrencySeparator != 0, "mpGeneralCurrencySeparator");   // cpp:1387
        CGS_ASSERT(mpGeneralCurrency != 0, "mpGeneralCurrency");                     // cpp:1388

        CgsUnicode::CgsUtf8 lacValue[256];
        CgsUnicode::CgsUtf8 lacThousandsSeparator[4];
        CgsUnicode::CgsUtf8 lacDecimalPointCharacter[4];
        const u8 lu8MinimumDigits = 0;

        lacValue[0] = 0;
        for (s32 liByte = 0; liByte < 4; ++liByte)
        {
            lacThousandsSeparator[liByte]    = 0;
            lacDecimalPointCharacter[liByte] = 0;
        }

        // UnicodeBuffer::SetThousandsSeparator, inlined (CgsUnicode.h:598).
        CGS_ASSERT(CgsUnicode::StringLength(mpGeneralCurrencySeparator) <= 1,
                   "StringLength(lUtf8ThousandsSeparator) <= 1");
        CgsUnicode::Copy(lacThousandsSeparator, mpGeneralCurrencySeparator);

        CgsUnicode::IntToString(lacValue, liCurrencyValue, lu8MinimumDigits,
                                lacThousandsSeparator);

        CgsUnicode::UnicodeBuffer lValueBuffer;
        lValueBuffer.Convert(lacValue);

        const CgsUnicode::CgsUtf8* lapArgs[1] = { lValueBuffer.GetBuffer() };
        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                           mpGeneralCurrency, liTargetSize, lapArgs, 1);
    }

    // @ 0x828641F0 -- the FLOAT value dispatcher (DWARF CgsLanguageManager.h: the
    // (char*, u32, f32, ParameterFormatType) overload): switch the format onto its
    // time/percentage/currency/distance leaf, streamed-assert on an unknown format
    // ("Invalid Parameter sent to SetLocalisedText with float"), always NUL the
    // buffer's last byte, return true. The leaves are the FLAG trap-stubs above until
    // their decompiles land -- no boot/menu text rides a float format.
    bool LanguageManager::FormatText(char* lpacBuffer, u32 luBufferSize, f32 lfValue,
                                     ParameterFormatType leType)
    {
        CGS_ASSERT(leType < 21, "Invalid Localisation Format supplied to TextField::FormatText");

        switch (leType)
        {
        case 1:  FormatHoursAndMinutesAndSecondsString(
                     reinterpret_cast<u8*>(lpacBuffer), lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 2:  FormatMinutesAndSecondsAndHundredsString(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 3:  FormatMinutesAndSecondsString(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 4:  FormatSecondsAndHundredsString(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 5:  FormatSecondsAndHundredsStringLong(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 6:  FormatSecondsString(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 7:  FormatSecondsStringLong(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 8:  FormatMinutesSecondsStringMediumText(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 11: FormatIntegerString(lpacBuffer, static_cast<s32>(lfValue),
                     static_cast<s32>(luBufferSize));                                    break;
        case 13: FormatPercentageString(lpacBuffer, static_cast<s32>(lfValue),
                     static_cast<s32>(luBufferSize));                                    break;
        case 14: FormatCurrencyString(lpacBuffer, static_cast<s32>(lfValue),
                     static_cast<s32>(luBufferSize));                                    break;
        case 15: FormatAutoDistanceString(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 16: FormatAutoDistanceStringLong(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 17: FormatSmallDistanceString(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 18: FormatSmallDistanceStringLong(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 19: FormatLargeDistanceString(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        case 20: FormatLargeDistanceStringLong(lpacBuffer, lfValue,
                     static_cast<s32>(luBufferSize));                                    break;
        default:
            CGS_ASSERT(false, "Invalid Parameter sent to SetLocalisedText with float : ");
            break;
        }

        lpacBuffer[luBufferSize - 1] = 0;
        return true;
    }

    // @ 0x82864C48 -- the FormatText RESOLVER (TextField::SetLocalisedText's entry;
    // the profile/boot prompts ride the type-0 copy and type-9/10 database-lookup
    // branches). Faithful branch map:
    //   0      : plain copy (own too-long streamed assert kept as the plain form).
    //   1-8,
    //   15-20  : atof -> the float dispatcher above; returns FALSE (value formats
    //            report "not a literal copy", per the X360 result register).
    //   9 / 10 : FindString(source): hit -> copy (10 = ToUpperN uppercase copy) of
    //            the DATABASE string; miss -> copy the source text verbatim.
    //            Returns TRUE.
    //   11-14  : atoi -> the s32 formatter; returns FALSE.
    //   other  : "Invalid Parameter sent to FormatText" assert; returns FALSE.
    // Every returning path NULs the buffer's last byte, matching the X360 stores.
    bool LanguageManager::FormatText(char* lpacBuffer, u32 luBufferSize,
                                     const char* lpcSourceText, ParameterFormatType leType)
    {
        CGS_ASSERT(lpacBuffer != 0, "Target field is invalid in TextField::FormatText");
        CGS_ASSERT(lpcSourceText != 0, "Text field is invalid in TextField::FormatText");
        CGS_ASSERT(std::strlen(lpcSourceText) < luBufferSize,
                   "Text string too long in TextField::FormatText");
        CGS_ASSERT(leType < 21, "Invalid Localisation Format supplied to TextField::FormatText");

        switch (leType)
        {
        case 0:
        {
            // CgsStringUtils.h:65's streamed "String <s> is too long" -- plain form.
            CGS_ASSERT(std::strlen(lpcSourceText) < luBufferSize, "String is too long");
            std::strncpy(lpacBuffer, lpcSourceText, luBufferSize);
            lpacBuffer[luBufferSize - 1] = 0;
            return true;
        }

        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        case 15: case 16: case 17: case 18: case 19: case 20:
        {
            FormatText(lpacBuffer, luBufferSize,
                       static_cast<f32>(std::atof(lpcSourceText)), leType);
            return false;
        }

        case 9: case 10:
        {
            const u8* lpDatabaseString = FindString(lpcSourceText);
            if (lpDatabaseString != 0)
            {
                CGS_ASSERT(std::strlen(reinterpret_cast<const char*>(lpDatabaseString)) <
                               luBufferSize,
                           "Database Text string too long in TextField::FormatText");
                if (leType != 9)
                {
                    CgsUnicode::ToUpperN(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                                         reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpDatabaseString),
                                         static_cast<s32>(luBufferSize));
                    lpacBuffer[luBufferSize - 1] = 0;
                    return true;
                }
                CgsUnicode::CopyN(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                                  reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpDatabaseString),
                                  static_cast<s32>(luBufferSize));
            }
            else
            {
                CgsUnicode::CopyN(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                                  reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpcSourceText),
                                  static_cast<s32>(luBufferSize));
            }
            lpacBuffer[luBufferSize - 1] = 0;
            return true;
        }

        case 11: case 12: case 13: case 14:
        {
            FormatText(lpacBuffer, luBufferSize, static_cast<s32>(std::atoi(lpcSourceText)),
                       leType);
            return false;
        }

        default:
            CGS_ASSERT(false, "Invalid Parameter sent to FormatText");
            return false;
        }
    }
}
