#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"          // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"            // CgsUnicode::IsValidUtf8String
#include "GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h" // CgsResource::LanguageResource (LoadStringTable)

#include <rw/math/fpu/scalar_operation.h>

#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include <cstring>   // std::strlen / std::strncpy (the FormatText resolver)
#include <cstdlib>   // std::atof / std::atoi (the FormatText value branches)

#define CGS_ARTIST_LANGUAGE_MANAGER_FILE "..\\..\\..\\GameShared\\GameClasses\\Language/CgsLanguageManager.cpp"

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
        mpResource          = 0;                //      stw r29 @0x60E8
        mpStringElements    = 0;                //      stw r29 @0x60E0
        mpLanguageAllocator = 0;                //      stw r29 @0x60E4

        // Re-seed both dynamic-string pools (free sublist owns all 1024 nodes, live empty) -- the
        // X360's two back-to-back InternalInit pairs, identical to the ctor's seeding.
        mDynamicStringElements.Construct();
        mDynamicStringPointerElements.Construct();

        mStrings.Init();                              // __13_::Init(this+8) -- (re-)init the empty hash

        mpcDefaultFontName   = 0;               // stw r29 @0x04
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
        CGS_ASSERT_AT(lpcKey != 0, "NULL string passed in as hash ID to LanguageManager::FindString", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 228);

        // strlen, then guard against an overlong key (X360 compares the length against 0x80).
        const char* lpcScan = lpcKey;
        while (*lpcScan)
            ++lpcScan;
        int liLength = static_cast<int>(lpcScan - lpcKey);
        CGS_ASSERT_AT(liLength <= 128, "Overlong string passed as hash ID to LanguageManager::FindString", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 229);

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
        if (lppString != 0)
        {
            CGS_ASSERT_AT(CgsUnicode::IsValidUtf8String(*lppString), "CgsUnicode::IsValidUtf8String( *luccp )",
                          CGS_ARTIST_LANGUAGE_MANAGER_FILE, 282);
            return *lppString;
        }
        return 0;
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
        CGS_ASSERT_AT(lpcStringId != 0, "NULL string ID passed to LanguageManager::AddString",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 312);
        CGS_ASSERT_AT(lpcString != 0, "NULL localised string passed to LanguageManager::AddString",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 313);
        CGS_ASSERT_AT(CgsUnicode::IsValidUtf8String(lpcString), "CgsUnicode::IsValidUtf8String( lpcString )",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 314);

        const u32 luHash = GetHashID(lpcStringId);

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
        CGS_ASSERT_AT(lpcStringId != 0, "NULL string ID passed to LanguageManager::AddString",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 438);
        CGS_ASSERT_AT(lpcString != 0, "NULL localised string passed to LanguageManager::AddString",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 439);
        CGS_ASSERT_AT(CgsUnicode::IsValidUtf8String(lpcString), "CgsUnicode::IsValidUtf8String( lpcString )",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 440);

        const u32 luHash = GetHashID(lpcStringId);

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

    void LanguageManager::LoadStringTable(const CgsResource::LanguageResource& resource)
    {
        CGS_ASSERT_STREAM_AT(resource.meLanguageID < E_LANGUAGE_TOTAL,
                             "Language ID " << resource.meLanguageID << " is invalid",
                             CGS_ARTIST_LANGUAGE_MANAGER_FILE, 547);
        UnloadStringTable();
        mpStringElements = static_cast<HashIDStringArray::Element*>(
            mpLanguageAllocator->CAlloc(resource.muSize, sizeof(HashIDStringArray::Element)));

        for (u32 luIndex = 0; luIndex < resource.muSize; ++luIndex)
        {
            HashIDStringArray::Element* lpElement = &mpStringElements[luIndex];
            CGS_ASSERT_AT(CgsUnicode::IsValidUtf8String(resource.mpEntries[luIndex].GetString()),
                          "CgsUnicode::IsValidUtf8String( resource.mpEntries[luIndex].GetString() )",
                          CGS_ARTIST_LANGUAGE_MANAGER_FILE, 559);
            lpElement->Set(resource.mpEntries[luIndex].GetHash(), resource.mpEntries[luIndex].GetString());
            mStrings.Insert(lpElement);
        }

        // DecFIGS gives the const input and mutable stored resource pointer.
        mpResource = const_cast<CgsResource::LanguageResource*>(&resource);
        const ELanguage leLanguage = static_cast<ELanguage>(resource.meLanguageID);
        PrepareFormattingStrings();
        if (leLanguage != E_LANGUAGE_JAPANESE)
            mpcDefaultFontName = "NODEFAULTFONTSPECIFIED";
        else
            mpcDefaultFontName = "dfheic";
        meLanguage = static_cast<ELanguage>(resource.meLanguageID);
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

        DynamicHashElementsList::Node* lpNode = mDynamicStringElements.GetHead();
        while (lpNode != 0)
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            mDynamicStringElements.RemoveHead();
            lpNode = mDynamicStringElements.GetHead();
            mpLanguageAllocator->Free(const_cast<CgsUnicode::CgsUtf8*>(lpElement->GetValue()));
            mpLanguageAllocator->Free(lpElement);
        }

        lpNode = mDynamicStringPointerElements.GetHead();
        while (lpNode != 0)
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            mDynamicStringPointerElements.RemoveHead();
            lpNode = mDynamicStringPointerElements.GetHead();
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
        CGS_ASSERT_AT(lpcStringId != 0, "NULL string ID passed to LanguageManager::RemoveString",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 359);

        const u32 luHash = GetHashID(lpcStringId);

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

        bool lbRemoved = false;
        DynamicHashElementsList::Node* lpNode = mDynamicStringElements.GetHead();
        while (lpNode != 0)
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            if (lpElement != 0 && lpElement->GetKey() == luHash)
            {
                mDynamicStringElements.RecycleNode(lpNode);
                mpLanguageAllocator->Free(const_cast<CgsUnicode::CgsUtf8*>(lpElement->GetValue()));
                mpLanguageAllocator->Free(lpElement);
                lbRemoved = true;
                break;
            }
            lpNode = static_cast<DynamicHashElementsList::Node*>(lpNode->GetNextNode());
        }
        return lbRemoved;
    }

    // X360 0x82864B30 CgsLanguage::LanguageManager::RemoveStringPointer.
    //
    // Faithful decompile: same shape as RemoveString, evicting through
    // RemoveStringPointerByHash instead.
    bool LanguageManager::RemoveStringPointer(const char* lpcStringId)
    {
        CGS_ASSERT_AT(lpcStringId != 0, "NULL string ID passed to LanguageManager::RemoveString",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 476);

        const u32 luHash = GetHashID(lpcStringId);

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

        bool lbRemoved = false;
        DynamicHashElementsList::Node* lpNode = mDynamicStringPointerElements.GetHead();
        while (lpNode != 0)
        {
            HashIDStringArray::Element* lpElement = lpNode->mData;
            if (lpElement != 0 && lpElement->GetKey() == luHash)
            {
                mDynamicStringPointerElements.RecycleNode(lpNode);
                mpLanguageAllocator->Free(lpElement);
                lbRemoved = true;
                break;
            }
            lpNode = static_cast<DynamicHashElementsList::Node*>(lpNode->GetNextNode());
        }
        return lbRemoved;
    }

    // X360 0x824434C0 CgsLanguage::LanguageManager::GetDefaultFont.
    //
    // Faithful decompile: assert the loaded default font name is non-null (mpcDefaultFontName,
    // set up by PrepareDefaultFont() during Construct(), not yet reconstructed), then return it --
    // the X360 returns the pointer either way, even after a failed assert (asserts are
    // non-fatal/continue in this codebase).
    const char* LanguageManager::GetDefaultFont() const
    {
        CGS_ASSERT_AT(mpcDefaultFontName != 0,
            "Invalid Default Font Name in Language Manager",
                "..\\..\\..\\GameShared\\GameClasses\\Language/CgsLanguageManager.h", 642 /* ARTIST 0x824434E8 */);

        return mpcDefaultFontName;
    }

    // X360 0x828608D0 CgsLanguage::LanguageManager::Prepare.
    //
    // Faithful decompile: assert the allocator is non-null, stash it (mpLanguageAllocator,
    // +0x60E4), and register the embedded debug component with the debug menu. Always returns
    // true (the X360 body has no failure return).
    bool LanguageManager::Prepare(CgsMemory::HeapMalloc* lpLanguageAllocator)
    {
        CGS_ASSERT_AT(lpLanguageAllocator != 0,
            "Null pointer for language allocator given",
                CGS_ARTIST_LANGUAGE_MANAGER_FILE, 180 /* ARTIST 0x828608FC */);

        mpLanguageAllocator = lpLanguageAllocator;
        mDebugComponent.Register();
        return true;
    }

    // X360 0x82864000 calls UnloadStringTable and then returns true. Keep the
    // allocator installed until UnloadStringTable has freed table and dynamic entries.
    bool LanguageManager::Release()
    {
        UnloadStringTable();
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
    void LanguageManager::PrepareDefaultFormattingStrings()
    {
        mrLargeDistanceConversion = 0.001f;
        mpGeneralDecimalSeparator = reinterpret_cast<const CgsUnicode::CgsUtf8*>(".");
        mpGeneralCurrencySeparator = reinterpret_cast<const CgsUnicode::CgsUtf8*>(".");
        mpGeneralThousandsSeparator = reinterpret_cast<const CgsUnicode::CgsUtf8*>(",");
        mrSmallDistanceConversion = 1.0f;
        mpGeneralPercentage = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1%");
        mpGeneralXOverY = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1/%2");
        mpGeneralCurrency = reinterpret_cast<const CgsUnicode::CgsUtf8*>("$%1");
        mpTimeFormatDate = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1/%2/%3");
        mpTimeFormatAll = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2:%3");
        mpTimeFormatHrsMinsSecs = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2:%3");
        mpTimeFormatMinsSecsHnds = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2.%3");
        mpTimeFormatMinsSecs = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1:%2");
        mpTimeFormatSecsHnds = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1.%2");
        mpTimeFormatSecs = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1s");
        mpTimeFormatSecsLong = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Seconds");
        mpTimeFormatMinsSecsMidText = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Mins %2 Secs");
        mpTimeFormatMinSecsMidText = reinterpret_cast<const CgsUnicode::CgsUtf8*>("1 Min %1 Secs");
        mpDistanceFormatIsMetric = reinterpret_cast<const CgsUnicode::CgsUtf8*>("1");
        mpDistanceFormatShort = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1m");
        mpDistanceFormatShortL = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Meters");
        mpDistanceFormatLong = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1km");
        mpDistanceFormatLongL = reinterpret_cast<const CgsUnicode::CgsUtf8*>("%1 Kilometres");
        mbIsUsingMetricUnits = true;
    }

    namespace
    {
        // PrepareFormattingStrings' ids, in the console's order: the .data pointers off_82F33340 ..
        // off_82F333A0 (r30 = 0x82F333A0, read at -0x60 .. 0), each naming a string at
        // 0x820E54E0 .. 0x820E5760 -- all read out of the image with the verified reader.
        const char* const KAC_GENERAL_DECIMAL_SEPARATOR           = "GENERAL_DECIMAL_SEPARATOR";          // off_82F33340
        const char* const KAC_GENERAL_THOUSANDS_SEPARATOR         = "GENERAL_THOUSANDS_SEPARATOR";        // off_82F33344
        const char* const KAC_GENERAL_PERCENTAGE_DISPLAY          = "GENERAL_PERCENTAGE_DISPLAY";         // off_82F33348
        const char* const KAC_GENERAL_X_OVER_Y_DISPLAY            = "GENERAL_X_OVER_Y_DISPLAY";           // off_82F3334C
        const char* const KAC_GENERAL_CURRENCY_SEPARATOR          = "GENERAL_CURRENCY_SEPARATOR";         // off_82F33350
        const char* const KAC_GENERAL_CURRENCY_NO_DECIMAL         = "GENERAL_CURRENCY_NO_DECIMAL";        // off_82F33354
        const char* const KAC_TIME_FORMAT_DATE                    = "TIME_FORMAT_DATE";                   // off_82F33358
        const char* const KAC_TIME_FORMAT_ALL                     = "TIME_FORMAT_ALL";                    // off_82F3335C
        const char* const KAC_TIME_FORMAT_HRS_MINS_SECS           = "TIME_FORMAT_HRS_MINS_SECS";          // off_82F33360
        const char* const KAC_TIME_FORMAT_MINS_SECS_HNDS          = "TIME_FORMAT_MINS_SECS_HNDS";         // off_82F33364
        const char* const KAC_TIME_FORMAT_MINS_SECS               = "TIME_FORMAT_MINS_SECS";              // off_82F33368
        const char* const KAC_TIME_FORMAT_SECS_HNDS               = "TIME_FORMAT_SECS_HNDS";              // off_82F3336C
        const char* const KAC_TIME_FORMAT_SECS                    = "TIME_FORMAT_SECS";                   // off_82F33370
        const char* const KAC_TIME_FORMAT_SECS_L                  = "TIME_FORMAT_SECS_L";                 // off_82F33374
        const char* const KAC_TIME_FORMAT_MIN_SECS_MID_TEXT       = "TIME_FORMAT_MIN_SECS_MID_TEXT";      // off_82F33378
        const char* const KAC_TIME_FORMAT_MINS_SECS_MID_TEXT      = "TIME_FORMAT_MINS_SECS_MID_TEXT";     // off_82F3337C
        const char* const KAC_DISTANCE_FORMAT_SHORT               = "DISTANCE_FORMAT_SHORT";              // off_82F33380
        const char* const KAC_DISTANCE_FORMAT_SHORT_L             = "DISTANCE_FORMAT_SHORT_L";            // off_82F33384
        const char* const KAC_DISTANCE_FORMAT_LONG                = "DISTANCE_FORMAT_LONG";               // off_82F33388
        const char* const KAC_DISTANCE_FORMAT_LONG_L              = "DISTANCE_FORMAT_LONG_L";             // off_82F3338C
        const char* const KAC_DISTANCE_FORMAT_ISMETRIC            = "DISTANCE_FORMAT_ISMETRIC";           // off_82F33390
        const char* const KAC_DISTANCE_FORMAT_SHORT_IMPERIAL      = "DISTANCE_FORMAT_SHORT_IMPERIAL";     // off_82F33394
        const char* const KAC_DISTANCE_FORMAT_SHORT_IMPERIAL_L    = "DISTANCE_FORMAT_SHORT_IMPERIAL_L";   // off_82F33398
        const char* const KAC_DISTANCE_FORMAT_LONG_IMPERIAL       = "DISTANCE_FORMAT_LONG_IMPERIAL";      // off_82F3339C
        const char* const KAC_DISTANCE_FORMAT_LONG_IMPERIAL_L     = "DISTANCE_FORMAT_LONG_IMPERIAL_L";    // off_82F333A0

        // The two unit systems' metres -> display-unit factors (image .rdata, verified reader).
        const f32 KF_METRIC_LARGE_DISTANCE_CONVERSION   = 0.001f;           // flt_82013F90 = 0x3A83126F (km)
        const f32 KF_METRIC_SMALL_DISTANCE_CONVERSION   = 1.0f;             // flt_82001C98 = 0x3F800000 (m)
        const f32 KF_IMPERIAL_LARGE_DISTANCE_CONVERSION = 0.00062137126f;   // flt_820E6B74 = 0x3A22E385 (mi)
        const f32 KF_IMPERIAL_SMALL_DISTANCE_CONVERSION = 1.0936133f;       // flt_820E60D4 = 0x3F8BFB85 (yd)

        // The switch's two characters (`cmplwi r11, 0x31` @0x82865F38, `cmplwi r11, 0x30` @0x82865FB0).
        const u8 KU8_IS_METRIC_YES = '1';
        const u8 KU8_IS_METRIC_NO  = '0';
    }

    // X360 0x82865B70 CgsLanguage::LanguageManager::PrepareFormattingStrings -- the only caller is
    // LoadStringTable @0x828664B8, right after the new table is installed.
    //
    // ⛔ [owner list 2026-09-28, L6] THIS WAS NEVER CALLED, AND ITS ABSENCE WAS VISIBLE. LoadStringTable
    // re-stamped the ENGLISH-DEFAULT literals instead (PrepareDefaultFormattingStrings), on the claim
    // that English renders the same through those defaults. It does not: the English
    // tables the PC loads (LANGUAGE/0002.bundle, and 0001) carry GENERAL_CURRENCY_SEPARATOR ',' (the
    // default is '.'), DISTANCE_FORMAT_ISMETRIC '0' -- IMPERIAL: miles and yards -- where the default is
    // metric, "%1 Kilometers" / "%1 km" / "%1 m" where the defaults spell "%1 Kilometres" / "%1km" /
    // "%1m", TIME_FORMAT_DATE "%2/%1/%3", TIME_FORMAT_ALL "%1:%2", TIME_FORMAT_SECS "%1" and
    // "%1 Min %2 Secs". So every distance and every money readout on the PC (the Driver Details panel's
    // "5,657.6 Kilometres", "414 Meters" and "$6.180.200" among them) differed from the console's
    // "3,515.4 Miles", "452 Yards" and "$6,180,200". Every other locale follows its own table the same way (French / German / ... carry
    // '1' and read metric), which is why nothing here forces a unit system.
    //
    // Straight line code, statement for statement: four FindString stores then their four null
    // asserts (cpp:2318..2321), the two currency strings (2327 / 2328), the ten time templates
    // (2355..2364), the metric switch string (2368), then the switch on its FIRST CHARACTER:
    //   '1' -> the four metric distance templates, mrLargeDistanceConversion 0.001,
    //          mbIsUsingMetricUnits = 1, mrSmallDistanceConversion 1.0 (the store order @0x82865F9C..);
    //   '0' -> the four IMPERIAL templates, 0.00062137126 (metres -> miles), metric flag 0,
    //          1.0936133 (metres -> yards) (@0x82866014..);
    //   else: when either localisation-QA mode is on (the debug component's ShowKeysOnly /
    //          ShowLocalisedTextAsStars, `lbz 0x6165` / `lbz 0x6166`) the metric templates are read and
    //          the function RETURNS without its distance asserts (0x828660F4..0x8286613C); otherwise it
    //          asserts "DISTANCE_FORMAT_ISMETRIC needs to be set to either 0 or 1" (cpp:2407) and falls
    //          through to the four distance-template null asserts (cpp:2410..2413) with the templates
    //          and the factors left as they were.
    // The console dereferences the switch string right after its null assert (`lbz r11, 0(r11)`); an
    // assert is not a guard, and the table always carries the id, so the read is reproduced as is.
    void LanguageManager::PrepareFormattingStrings()
    {
        mpGeneralDecimalSeparator   = FindString(KAC_GENERAL_DECIMAL_SEPARATOR);      // +0x6100
        mpGeneralThousandsSeparator = FindString(KAC_GENERAL_THOUSANDS_SEPARATOR);    // +0x6104
        mpGeneralPercentage         = FindString(KAC_GENERAL_PERCENTAGE_DISPLAY);     // +0x6108
        mpGeneralXOverY             = FindString(KAC_GENERAL_X_OVER_Y_DISPLAY);       // +0x610C
        CGS_ASSERT_AT(mpGeneralDecimalSeparator != 0,   KAC_GENERAL_DECIMAL_SEPARATOR, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2318);    // cpp:2318
        CGS_ASSERT_AT(mpGeneralThousandsSeparator != 0, KAC_GENERAL_THOUSANDS_SEPARATOR, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2319);  // cpp:2319
        CGS_ASSERT_AT(mpGeneralPercentage != 0,         KAC_GENERAL_PERCENTAGE_DISPLAY, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2320);   // cpp:2320
        CGS_ASSERT_AT(mpGeneralXOverY != 0,             KAC_GENERAL_X_OVER_Y_DISPLAY, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2321);     // cpp:2321

        mpGeneralCurrencySeparator = FindString(KAC_GENERAL_CURRENCY_SEPARATOR);      // +0x6110
        mpGeneralCurrency          = FindString(KAC_GENERAL_CURRENCY_NO_DECIMAL);     // +0x6114
        CGS_ASSERT_AT(mpGeneralCurrencySeparator != 0, KAC_GENERAL_CURRENCY_SEPARATOR, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2327);    // cpp:2327
        CGS_ASSERT_AT(mpGeneralCurrency != 0,          KAC_GENERAL_CURRENCY_NO_DECIMAL, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2328);   // cpp:2328

        mpTimeFormatDate            = FindString(KAC_TIME_FORMAT_DATE);               // +0x6118
        mpTimeFormatAll             = FindString(KAC_TIME_FORMAT_ALL);                // +0x611C
        mpTimeFormatHrsMinsSecs     = FindString(KAC_TIME_FORMAT_HRS_MINS_SECS);      // +0x6120
        mpTimeFormatMinsSecsHnds    = FindString(KAC_TIME_FORMAT_MINS_SECS_HNDS);     // +0x6124
        mpTimeFormatMinsSecs        = FindString(KAC_TIME_FORMAT_MINS_SECS);          // +0x6128
        mpTimeFormatSecsHnds        = FindString(KAC_TIME_FORMAT_SECS_HNDS);          // +0x612C
        mpTimeFormatSecs            = FindString(KAC_TIME_FORMAT_SECS);               // +0x6130
        mpTimeFormatSecsLong        = FindString(KAC_TIME_FORMAT_SECS_L);             // +0x6134
        mpTimeFormatMinSecsMidText  = FindString(KAC_TIME_FORMAT_MIN_SECS_MID_TEXT);  // +0x6138
        mpTimeFormatMinsSecsMidText = FindString(KAC_TIME_FORMAT_MINS_SECS_MID_TEXT); // +0x613C
        CGS_ASSERT_AT(mpTimeFormatDate != 0,            KAC_TIME_FORMAT_DATE, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2355);               // cpp:2355
        CGS_ASSERT_AT(mpTimeFormatAll != 0,             KAC_TIME_FORMAT_ALL, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2356);                // cpp:2356
        CGS_ASSERT_AT(mpTimeFormatHrsMinsSecs != 0,     KAC_TIME_FORMAT_HRS_MINS_SECS, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2357);      // cpp:2357
        CGS_ASSERT_AT(mpTimeFormatMinsSecsHnds != 0,    KAC_TIME_FORMAT_MINS_SECS_HNDS, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2358);     // cpp:2358
        CGS_ASSERT_AT(mpTimeFormatMinsSecs != 0,        KAC_TIME_FORMAT_MINS_SECS, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2359);          // cpp:2359
        CGS_ASSERT_AT(mpTimeFormatSecsHnds != 0,        KAC_TIME_FORMAT_SECS_HNDS, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2360);          // cpp:2360
        CGS_ASSERT_AT(mpTimeFormatSecs != 0,            KAC_TIME_FORMAT_SECS, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2361);               // cpp:2361
        CGS_ASSERT_AT(mpTimeFormatSecsLong != 0,        KAC_TIME_FORMAT_SECS_L, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2362);             // cpp:2362
        CGS_ASSERT_AT(mpTimeFormatMinSecsMidText != 0,  KAC_TIME_FORMAT_MIN_SECS_MID_TEXT, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2363);  // cpp:2363
        CGS_ASSERT_AT(mpTimeFormatMinsSecsMidText != 0, KAC_TIME_FORMAT_MINS_SECS_MID_TEXT, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2364); // cpp:2364

        mpDistanceFormatIsMetric = FindString(KAC_DISTANCE_FORMAT_ISMETRIC);          // +0x6150
        CGS_ASSERT_AT(mpDistanceFormatIsMetric != 0, KAC_DISTANCE_FORMAT_ISMETRIC, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2368);          // cpp:2368

        const u8 lu8IsMetric = mpDistanceFormatIsMetric[0];                            // lbz r11, 0(r11)
        if (lu8IsMetric == KU8_IS_METRIC_YES)
        {
            mpDistanceFormatShort  = FindString(KAC_DISTANCE_FORMAT_SHORT);            // +0x6140
            mpDistanceFormatShortL = FindString(KAC_DISTANCE_FORMAT_SHORT_L);          // +0x6144
            mpDistanceFormatLong   = FindString(KAC_DISTANCE_FORMAT_LONG);             // +0x6148
            mpDistanceFormatLongL  = FindString(KAC_DISTANCE_FORMAT_LONG_L);           // +0x614C
            mrLargeDistanceConversion = KF_METRIC_LARGE_DISTANCE_CONVERSION;           // +0x60F8
            mbIsUsingMetricUnits      = true;                                          // +0x60F4
            mrSmallDistanceConversion = KF_METRIC_SMALL_DISTANCE_CONVERSION;           // +0x60FC
        }
        else if (lu8IsMetric == KU8_IS_METRIC_NO)
        {
            mpDistanceFormatShort  = FindString(KAC_DISTANCE_FORMAT_SHORT_IMPERIAL);   // +0x6140
            mpDistanceFormatShortL = FindString(KAC_DISTANCE_FORMAT_SHORT_IMPERIAL_L); // +0x6144
            mpDistanceFormatLong   = FindString(KAC_DISTANCE_FORMAT_LONG_IMPERIAL);    // +0x6148
            mpDistanceFormatLongL  = FindString(KAC_DISTANCE_FORMAT_LONG_IMPERIAL_L);  // +0x614C
            mrLargeDistanceConversion = KF_IMPERIAL_LARGE_DISTANCE_CONVERSION;         // +0x60F8
            mbIsUsingMetricUnits      = false;                                         // +0x60F4
            mrSmallDistanceConversion = KF_IMPERIAL_SMALL_DISTANCE_CONVERSION;         // +0x60FC
        }
        else
        {
            if (mDebugComponent.ShowKeysOnly() || mDebugComponent.ShowLocalisedTextAsStars())
            {
                mpDistanceFormatShort  = FindString(KAC_DISTANCE_FORMAT_SHORT);
                mpDistanceFormatShortL = FindString(KAC_DISTANCE_FORMAT_SHORT_L);
                mpDistanceFormatLong   = FindString(KAC_DISTANCE_FORMAT_LONG);
                mpDistanceFormatLongL  = FindString(KAC_DISTANCE_FORMAT_LONG_L);
                return;
            }
            CGS_FAIL_AT("DISTANCE_FORMAT_ISMETRIC needs to be set to either 0 or 1", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2407);    // cpp:2407
        }

        CGS_ASSERT_AT(mpDistanceFormatShort != 0,  KAC_DISTANCE_FORMAT_SHORT, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2410);               // cpp:2410
        CGS_ASSERT_AT(mpDistanceFormatShortL != 0, KAC_DISTANCE_FORMAT_SHORT_L, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2411);             // cpp:2411
        CGS_ASSERT_AT(mpDistanceFormatLong != 0,   KAC_DISTANCE_FORMAT_LONG, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2412);                // cpp:2412
        CGS_ASSERT_AT(mpDistanceFormatLongL != 0,  KAC_DISTANCE_FORMAT_LONG_L, CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2413);              // cpp:2413
    }


    bool LanguageManager::FormatText(char* lpacBuffer, u32 luBufferSize, s32 liValue,
                                     ParameterFormatType leType)
    {
        CGS_ASSERT_STREAM_AT(leType < E_FORMAT_COUNT,
            "Invalid Localisation Format supplied to TextField::FormatText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 940);

        CgsUnicode::CgsUtf8* lpUtf8Target = reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer);
        switch (leType)
        {
        case E_FORMAT_INTEGER:
            FormatIntegerString(lpUtf8Target, liValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_INTEGER_NOSEPERATOR:
            FormatIntegerNoSeperatorString(lpUtf8Target, liValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_PERCENTAGE:
            FormatPercentageString(lpUtf8Target, liValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_MONEY:
            FormatCurrencyString(lpUtf8Target, liValue, static_cast<s32>(luBufferSize));
            break;
        default:
            CGS_FAIL_STREAM_AT("Invalid Parameter sent to SetLocalisedText with int",
                CGS_ARTIST_LANGUAGE_MANAGER_FILE, 968);
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
        CGS_ASSERT_STREAM_AT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1062);
        CGS_ASSERT_STREAM_AT(lpcSourceText != 0, "Text field is invalid in LanguageManager::FormatText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1063);
        CGS_ASSERT_STREAM_AT(liNumParams < 4 && liNumParams > 0,
            "Wrong number of Parameters int SetLocalisedText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1064);

        char lacSourceBuffer[1024];
        FormatText(lacSourceBuffer, sizeof(lacSourceBuffer), lpcSourceText, leType);

        const CgsUnicode::CgsUtf8* lapUtf8Params[4];
        char lacParamBuffers[4][512];
        for (s32 liParam = 0; liParam < liNumParams; ++liParam)
        {
            CGS_ASSERT_AT(lArguments != 0, "lpArgument", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1086);
            const char* lpcParamText = va_arg(lArguments, const char*);
            const s32 liParamFormat = va_arg(lArguments, s32);
            CGS_ASSERT_AT(lpcParamText != 0, "Invalid Text Pointer in Parameterised SetLocalisedText",
                CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1093);
            CGS_ASSERT_AT(liParamFormat >= 0 && liParamFormat < E_FORMAT_COUNT, "Invalid Parameter",
                CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1094);

            FormatText(lacParamBuffers[liParam], sizeof(lacParamBuffers[liParam]), lpcParamText,
                       static_cast<ParameterFormatType>(liParamFormat));
            lapUtf8Params[liParam] = reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacParamBuffers[liParam]);
        }

        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                           reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSourceBuffer),
                           static_cast<s32>(luBufferSize), lapUtf8Params, static_cast<u8>(liNumParams));
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
        CGS_ASSERT_AT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1127);   // cpp:1127
        CGS_ASSERT_AT(lpcSourceText != 0, "Text field is invalid in TextField::SetLocalisedText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1128);  // cpp:1128
        CGS_ASSERT_AT(liNumParams > 0 && liNumParams < 4, "Wrong number of Parameters int SetLocalisedText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1129); // cpp:1129

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

            CGS_ASSERT_AT(lpcParamText != 0, "Invalid Text Pointer in Parameterised SetLocalisedText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1156); // cpp:1156
            CGS_ASSERT_AT(luParamFormat <= 0x14, "Invalid Parameter", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1157);                                   // cpp:1157

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

    // ARTIST passes the source scratch buffer's 1024-byte size to _Print
    // in both single-value overloads, at 0x82865858 and 0x828659C4.
    bool LanguageManager::FormatTextFromInt(char* lpacBuffer, u32 luBufferSize,
                                            const char* lpcSourceText, ParameterFormatType leType,
                                            s32 liValue, ParameterFormatType leValueType)
    {
        CGS_ASSERT_STREAM_AT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1185);
        CGS_ASSERT_STREAM_AT(lpcSourceText != 0, "Text field is invalid in TextField::SetLocalisedText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1186);
        (void)luBufferSize;

        char lacSourceBuffer[1024];
        char lacValueBuffer[64];
        const CgsUnicode::CgsUtf8* lapUtf8Params[4];
        lapUtf8Params[0] = reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacValueBuffer);
        FormatText(lacSourceBuffer, sizeof(lacSourceBuffer), lpcSourceText, leType);
        FormatText(lacValueBuffer, sizeof(lacValueBuffer), liValue, leValueType);
        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                           reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSourceBuffer),
                           sizeof(lacSourceBuffer), lapUtf8Params, 1);
        return true;
    }

    bool LanguageManager::FormatTextFromFloat(char* lpacBuffer, u32 luBufferSize,
                                              const char* lpcSourceText, ParameterFormatType leType,
                                              f32 lfValue, ParameterFormatType leValueType)
    {
        CGS_ASSERT_STREAM_AT(lpacBuffer != 0, "Target field is invalid in LanguageManager::FormatText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1221);
        CGS_ASSERT_STREAM_AT(lpcSourceText != 0, "Text field is invalid in TextField::SetLocalisedText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1222);
        (void)luBufferSize;

        char lacSourceBuffer[1024];
        char lacValueBuffer[64];
        const CgsUnicode::CgsUtf8* lapUtf8Params[4];
        lapUtf8Params[0] = reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacValueBuffer);
        FormatText(lacSourceBuffer, sizeof(lacSourceBuffer), lpcSourceText, leType);
        FormatText(lacValueBuffer, sizeof(lacValueBuffer), lfValue, leValueType);
        CgsUnicode::_Print(reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer),
                           reinterpret_cast<const CgsUnicode::CgsUtf8*>(lacSourceBuffer),
                           sizeof(lacSourceBuffer), lapUtf8Params, 1);
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
        CGS_ASSERT_AT(lpcTarget != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2166);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2167);
        CGS_ASSERT_AT(mpDistanceFormatLong != 0, "mpDistanceFormatLong",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2168);
        CGS_ASSERT_AT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2169);
        CGS_ASSERT_AT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2170);

        const f32 lfScaledDistance = mrLargeDistanceConversion * lfValue;
        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetThousandsSeparator(mpGeneralThousandsSeparator);
        lValue.SetDecimalPointCharacter(mpGeneralDecimalSeparator);
        lValue.SetDecimalPlaces(1);
        lValue.Convert(lfScaledDistance);

        CgsUnicode::Print(mpDistanceFormatLong,
                          reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                          liTargetSize, lValue.GetBuffer());
    }

    // DecFIGS identifies RenderWare IntRound<float> in the original time formatters.
    // ARTIST initializes the Unicode format buffers before setting each field width.

    // Format the rounded minute, second and hundredth fields through their locale template.
    void LanguageManager::FormatMinutesAndSecondsAndHundredsString(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                                                   s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1701);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1702);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1704);
        CGS_ASSERT_AT(mpTimeFormatMinsSecsHnds != 0, "mpTimeFormatMinsSecsHnds",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1706);

        const s32 liTotalHundredths = rw::math::fpu::IntRound(lfTimeInSeconds * 100.0f);
        const s32 liSecondsAndHundredths = liTotalHundredths % 6000;
        const s32 liMinutes = (liTotalHundredths - liSecondsAndHundredths) / 6000;
        const s32 liHundredths = liSecondsAndHundredths % 100;
        const s32 liSeconds = (liSecondsAndHundredths - liHundredths) / 100;

        CgsUnicode::UnicodeBuffer lMinutes;
        lMinutes.Construct();
        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();
        CgsUnicode::UnicodeBuffer lHundredths;
        lHundredths.Construct();

        lMinutes.SetDecimalPlaces(0);
        lMinutes.SetMinimumDigits(1);
        lMinutes.Convert(liMinutes);
        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(2);
        lSeconds.Convert(liSeconds);
        lHundredths.SetDecimalPlaces(0);
        lHundredths.SetMinimumDigits(2);
        lHundredths.Convert(liHundredths);

        CgsUnicode::Print(mpTimeFormatMinsSecsHnds,
                          lpUtf8Target,
                          liTargetSize, lMinutes.GetBuffer(), lSeconds.GetBuffer(), lHundredths.GetBuffer());
    }

    // Use two minute digits in the Italian locale and one elsewhere.
    void LanguageManager::FormatMinutesAndSecondsString(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                                        s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1764);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1765);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1767);
        CGS_ASSERT_AT(mpTimeFormatMinsSecs != 0, "mpTimeFormatMinsSecs",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1769);

        const s32 liTotalSeconds = rw::math::fpu::IntRound(lfTimeInSeconds);
        const s32 liSeconds = liTotalSeconds % 60;
        const s32 liMinutes = (liTotalSeconds - liSeconds) / 60;

        CgsUnicode::UnicodeBuffer lMinutes;
        lMinutes.Construct();
        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();

        lMinutes.SetDecimalPlaces(0);
        if (meLanguage == E_LANGUAGE_ITALIAN)
            lMinutes.SetMinimumDigits(2);
        else
            lMinutes.SetMinimumDigits(1);
        lMinutes.Convert(liMinutes);
        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(2);
        lSeconds.Convert(liSeconds);

        CgsUnicode::Print(mpTimeFormatMinsSecs,
                          lpUtf8Target,
                          liTargetSize, lMinutes.GetBuffer(), lSeconds.GetBuffer());
    }

    // ARTIST writes the numeric text into a 64-byte buffer before applying the long locale template.
    void LanguageManager::FormatSecondsAndHundredsStringLong(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                                             s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1826);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1827);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1829);
        CGS_ASSERT_AT(mpTimeFormatSecsHnds != 0, "mpTimeFormatSecsHnds",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1831);

        const s32 liTotalHundredths = rw::math::fpu::IntRound(lfTimeInSeconds * 100.0f);
        const s32 liHundredths = liTotalHundredths % 100;
        const s32 liSeconds = (liTotalHundredths - liHundredths) / 100;

        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();
        CgsUnicode::UnicodeBuffer lHundredths;
        lHundredths.Construct();

        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(1);
        lSeconds.Convert(liSeconds);
        lHundredths.SetDecimalPlaces(0);
        lHundredths.SetMinimumDigits(2);
        lHundredths.Convert(liHundredths);

        CgsUnicode::CgsUtf8 lacShortTime[64];
        CgsUnicode::Print(mpTimeFormatSecsHnds,
                          lacShortTime,
                          sizeof(lacShortTime), lSeconds.GetBuffer(), lHundredths.GetBuffer());
        CgsUnicode::Print(mpTimeFormatSecsLong, lpUtf8Target,
                          liTargetSize, static_cast<const CgsUnicode::CgsUtf8*>(lacShortTime));
    }

    // Round the elapsed time to hundredths before formatting the fields.
    void LanguageManager::FormatSecondsAndHundredsString(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                                         s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1882);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1883);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1885);
        CGS_ASSERT_AT(mpTimeFormatSecsHnds != 0, "mpTimeFormatSecsHnds",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1887);

        const s32 liTotalHundredths = rw::math::fpu::IntRound(lfTimeInSeconds * 100.0f);
        const s32 liHundredths = liTotalHundredths % 100;
        const s32 liSeconds = (liTotalHundredths - liHundredths) / 100;

        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();
        CgsUnicode::UnicodeBuffer lHundredths;
        lHundredths.Construct();

        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(1);
        lSeconds.Convert(liSeconds);
        lHundredths.SetDecimalPlaces(0);
        lHundredths.SetMinimumDigits(2);
        lHundredths.Convert(liHundredths);

        CgsUnicode::Print(mpTimeFormatSecsHnds,
                          lpUtf8Target,
                          liTargetSize, lSeconds.GetBuffer(), lHundredths.GetBuffer());
    }

    // Round to a whole second before applying the locale template.
    void LanguageManager::FormatSecondsString(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                              s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1935);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1936);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1938);
        CGS_ASSERT_AT(mpTimeFormatSecs != 0, "mpTimeFormatSecs",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1940);

        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();
        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(1);
        lSeconds.Convert(rw::math::fpu::IntRound(lfTimeInSeconds));

        CgsUnicode::Print(mpTimeFormatSecs, lpUtf8Target,
                          liTargetSize, lSeconds.GetBuffer());
    }

    // Round to a whole second before applying the locale template.
    void LanguageManager::FormatSecondsStringLong(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                                  s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1974);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1975);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1977);
        CGS_ASSERT_AT(mpTimeFormatSecsLong != 0, "mpTimeFormatSecsLong",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1979);

        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();
        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(1);
        lSeconds.Convert(rw::math::fpu::IntRound(lfTimeInSeconds));

        CgsUnicode::Print(mpTimeFormatSecsLong, lpUtf8Target,
                          liTargetSize, lSeconds.GetBuffer());
    }

    // The singular minutes template supplies its own leading 1. Both templates receive the seconds.
    void LanguageManager::FormatMinutesSecondsStringMediumText(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                                               s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2013);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2014);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2016);
        CGS_ASSERT_AT(mpTimeFormatMinsSecsMidText != 0, "mpTimeFormatMinsSecsMidText",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2018);
        CGS_ASSERT_AT(mpTimeFormatMinSecsMidText != 0, "mpTimeFormatMinSecsMidText",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2019);

        const s32 liTotalSeconds = rw::math::fpu::IntRound(lfTimeInSeconds);
        const s32 liSeconds = liTotalSeconds % 60;
        const s32 liMinutes = (liTotalSeconds - liSeconds) / 60;

        if (liMinutes == 1)
        {
            CgsUnicode::UnicodeBuffer lSeconds;
            lSeconds.Construct();

            lSeconds.SetDecimalPlaces(0);
            lSeconds.SetMinimumDigits(2);
            lSeconds.Convert(liSeconds);

            CgsUnicode::Print(mpTimeFormatMinSecsMidText,
                              lpUtf8Target,
                              liTargetSize, lSeconds.GetBuffer());
            return;
        }

        CgsUnicode::UnicodeBuffer lMinutes;
        lMinutes.Construct();
        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();

        lMinutes.SetDecimalPlaces(0);
        lMinutes.SetMinimumDigits(1);
        lMinutes.Convert(liMinutes);
        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(2);
        lSeconds.Convert(liSeconds);

        CgsUnicode::Print(mpTimeFormatMinsSecsMidText,
                          lpUtf8Target,
                          liTargetSize, lMinutes.GetBuffer(), lSeconds.GetBuffer());
    }

    // ========================================================================================
    // [pause-stats wave 2026-08-29] THE SEVEN LEAVES THE DRIVER-DETAILS PAUSE PANEL RIDES.
    // Every one of them was a __debugbreak() trap stub, and the first one HandleStatData reaches
    // killed the process the instant the stat chain started delivering data (measured: an
    // 0x80000003 in LanguageManager::FormatText <- TextField::SetLocalisedText <-
    // CrashNavDriverDetails::HandleStatData, on the very first field). The screen could not have
    // shown a number no matter how good the producer was.
    // WARNING: the block banner above already says "treat the remaining trap bodies as live
    // hazards, not debug-only" -- this is the third time that has been proved on a real screen.
    //
    // All seven share the shape the landed integer/time leaves already use: zero a
    // UnicodeBuffer separator pair, Copy the locale separators in, render each value through
    // IntToString/FloatToString, Convert each into a UnicodeBuffer, then _Print the positional
    // parameters into the locale template. Asserts carry the console's own baked line numbers.
    // ========================================================================================

    // Format both numbers with the locale template and no grouping separator.
    void LanguageManager::FormatXoverYString(CgsUnicode::CgsUtf8* lpUtf8Target, s32 liX, s32 liY,
                                             s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1490);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1491);
        CGS_ASSERT_AT(mpGeneralXOverY != 0, "mpGeneralXOverY",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1492);

        CgsUnicode::UnicodeBuffer lX;
        lX.Construct();
        CgsUnicode::UnicodeBuffer lY;
        lY.Construct();

        lX.SetDecimalPlaces(0);
        lX.SetMinimumDigits(1);
        lX.Convert(liX);
        lY.SetDecimalPlaces(0);
        lY.SetMinimumDigits(1);
        lY.Convert(liY);

        CgsUnicode::Print(mpGeneralXOverY,
                          lpUtf8Target,
                          liTargetSize, lX.GetBuffer(), lY.GetBuffer());
    }

    // Format the split time fields through the locale template.
    void LanguageManager::FormatHoursAndMinutesAndSecondsString(CgsUnicode::CgsUtf8* lpUtf8Target, s32 liHours,
                                                                s32 liMinutes, s32 liSeconds,
                                                                s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1651);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1652);
        CGS_ASSERT_AT(mpTimeFormatHrsMinsSecs != 0, "mpTimeFormatHrsMinsSecs",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1654);

        CgsUnicode::UnicodeBuffer lHours;
        lHours.Construct();
        CgsUnicode::UnicodeBuffer lMinutes;
        lMinutes.Construct();
        CgsUnicode::UnicodeBuffer lSeconds;
        lSeconds.Construct();

        lHours.SetDecimalPlaces(0);
        lHours.SetMinimumDigits(2);
        lHours.Convert(liHours);
        lMinutes.SetDecimalPlaces(0);
        lMinutes.SetMinimumDigits(2);
        lMinutes.Convert(liMinutes);
        lSeconds.SetDecimalPlaces(0);
        lSeconds.SetMinimumDigits(2);
        lSeconds.Convert(liSeconds);

        CgsUnicode::Print(mpTimeFormatHrsMinsSecs,
                          lpUtf8Target,
                          liTargetSize, lHours.GetBuffer(), lMinutes.GetBuffer(), lSeconds.GetBuffer());
    }

    // Round elapsed time before splitting it into the three time fields.
    void LanguageManager::FormatHoursMinutesAndSecondsString(CgsUnicode::CgsUtf8* lpUtf8Target, f32 lfTimeInSeconds,
                                                             s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1614);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1615);
        CGS_ASSERT_AT(lfTimeInSeconds >= 0.0f, "lfTimeInSeconds >= 0.0f",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1617);

        const s32 liTotalSeconds = rw::math::fpu::IntRound(lfTimeInSeconds);
        const s32 liMinutesAndSeconds = liTotalSeconds % 3600;
        const s32 liHours = (liTotalSeconds - liMinutesAndSeconds) / 3600;
        const s32 liSeconds = liMinutesAndSeconds % 60;
        const s32 liMinutes = (liMinutesAndSeconds - liSeconds) / 60;
        FormatHoursAndMinutesAndSecondsString(lpUtf8Target, liHours, liMinutes, liSeconds, liTargetSize);
    }

    // @ 0x82861988 (asserts cpp:2087..2091) -- the SHORT-distance leaf (format 17). Scale by
    // mrSmallDistanceConversion (+0x60FC, the metric/imperial factor), TRUNCATE to an integer
    // (`fctiwz` -- metres are whole numbers here, unlike the large form's one decimal place),
    // render through IntToString with the locale separators, print into mpDistanceFormatShort's
    // %1. Min digits is the zeroed UnicodeBuffer byte the console reads back, i.e. 0.
    void LanguageManager::FormatSmallDistanceString(char* lpcTarget, f32 lfMetres,
                                                    s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpcTarget != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2087);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2088);
        CGS_ASSERT_AT(mpDistanceFormatShort != 0, "mpDistanceFormatShort",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2089);
        CGS_ASSERT_AT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2090);
        CGS_ASSERT_AT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2091);

        const s32 liDistance = static_cast<s32>(mrSmallDistanceConversion * lfMetres);
        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetThousandsSeparator(mpGeneralThousandsSeparator);
        lValue.SetDecimalPointCharacter(mpGeneralDecimalSeparator);
        lValue.SetDecimalPlaces(0);
        lValue.Convert(liDistance);

        CgsUnicode::Print(mpDistanceFormatShort,
                          reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                          liTargetSize, lValue.GetBuffer());
    }

    // @ 0x82861B88 -- the SHORT-distance LONG-UNIT-NAME twin (format 18). FLAG: this is the ONE
    // function in this block with NO IDA export (the exports have a hole between
    // FormatSmallDistanceString @0x82861988 and FormatLargeDistanceString @0x82861D88, and
    // FormatAutoDistanceStringLong's `bl` names the symbol that lives in it). Its body is
    // reconstructed as the exact twin of FormatSmallDistanceString differing only in the format
    // member -- which is precisely how FormatLargeDistanceStringLong (127 insns, EXPORTED)
    // differs from FormatLargeDistanceString (127 insns): same code, mpDistanceFormatLongL
    // (+0x614C) in place of mpDistanceFormatLong. INFERRED, not read; flagged as such.
    void LanguageManager::FormatSmallDistanceStringLong(char* lpcTarget, f32 lfMetres,
                                                        s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpcTarget != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2127);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2128);
        CGS_ASSERT_AT(mpDistanceFormatShortL != 0, "mpDistanceFormatShortL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2129);
        CGS_ASSERT_AT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2130);
        CGS_ASSERT_AT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2131);

        const s32 liDistance = static_cast<s32>(mrSmallDistanceConversion * lfMetres);
        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetThousandsSeparator(mpGeneralThousandsSeparator);
        lValue.SetDecimalPlaces(0);
        lValue.SetDecimalPointCharacter(mpGeneralDecimalSeparator);
        lValue.Convert(liDistance);

        CgsUnicode::Print(mpDistanceFormatShortL,
                          reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                          liTargetSize, lValue.GetBuffer());
    }

    // @ 0x82861F88 (asserts cpp:2205..2209) -- the LARGE-distance LONG-UNIT-NAME twin
    // (format 20). Instruction-for-instruction FormatLargeDistanceString with
    // mpDistanceFormatLongL (+0x614C) in place of mpDistanceFormatLong: same
    // mrLargeDistanceConversion scale, same FloatToString with ONE decimal place.
    void LanguageManager::FormatLargeDistanceStringLong(char* lpcTarget, f32 lfMetres,
                                                        s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpcTarget != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2205);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2206);
        CGS_ASSERT_AT(mpDistanceFormatLongL != 0, "mpDistanceFormatLongL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2207);
        CGS_ASSERT_AT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2208);
        CGS_ASSERT_AT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2209);

        const f32 lfScaledDistance = mrLargeDistanceConversion * lfMetres;
        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetThousandsSeparator(mpGeneralThousandsSeparator);
        lValue.SetDecimalPointCharacter(mpGeneralDecimalSeparator);
        lValue.SetDecimalPlaces(1);
        lValue.Convert(lfScaledDistance);

        CgsUnicode::Print(mpDistanceFormatLongL,
                          reinterpret_cast<CgsUnicode::CgsUtf8*>(lpcTarget),
                          liTargetSize, lValue.GetBuffer());
    }

    // @ 0x82862188 (asserts cpp:2244/2245) -- pick the unit by MAGNITUDE: if the value in LARGE
    // units (metres * mrLargeDistanceConversion, i.e. kilometres/miles) is below 1.0, print it
    // in small units instead. The console's compare constant is flt_82001C98 == 1.0f.
    void LanguageManager::FormatAutoDistanceString(char* lpcTarget, f32 lfMetres,
                                                   s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpcTarget != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2244);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2245);

        // ARTIST's less-than branch selects small units. Unordered values use large units.
        if (!(mrLargeDistanceConversion * lfMetres < 1.0f))
        {
            FormatLargeDistanceString(lpcTarget, lfMetres, liTargetSize);
            return;
        }
        FormatSmallDistanceString(lpcTarget, lfMetres, liTargetSize);
    }

    // @ 0x82862240 (asserts cpp:2275/2276) -- the long-unit-name twin of the chooser above
    // (format 16). FOUR of the pause panel's rows ride it: distance driven, best drift, best
    // oncoming and best airtime distance.
    void LanguageManager::FormatAutoDistanceStringLong(char* lpcTarget, f32 lfMetres,
                                                       s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpcTarget != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2275);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 2276);

        // ARTIST's less-than branch selects small units. Unordered values use large units.
        if (!(mrLargeDistanceConversion * lfMetres < 1.0f))
        {
            FormatLargeDistanceStringLong(lpcTarget, lfMetres, liTargetSize);
            return;
        }
        FormatSmallDistanceStringLong(lpcTarget, lfMetres, liTargetSize);
    }
    // FLAG trap-stubs for the float-dispatch leaves 0x828641F0 references beyond the
    // block above (declared additions; decompiles land with the value-format slice).

    // ------------------------------------------------------------------------
    // Format dates through the locale template with two-digit day/month and four-digit year.
    void LanguageManager::FormatDateString(CgsUnicode::CgsUtf8* lpUtf8Target, s32 liDays, s32 liMonths,
                                           s32 liYears, s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1566);
        CGS_ASSERT_AT(mpTimeFormatDate != 0, "mpTimeFormatDate",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1567);

        CgsUnicode::UnicodeBuffer lDay;
        lDay.Construct();
        CgsUnicode::UnicodeBuffer lMonth;
        lMonth.Construct();
        CgsUnicode::UnicodeBuffer lYear;
        lYear.Construct();

        lDay.SetDecimalPlaces(0);
        lDay.SetMinimumDigits(2);
        lDay.Convert(liDays);
        lMonth.SetDecimalPlaces(0);
        lMonth.SetMinimumDigits(2);
        lMonth.Convert(liMonths);
        lYear.SetDecimalPlaces(0);
        lYear.SetMinimumDigits(4);
        lYear.Convert(liYears);

        CgsUnicode::Print(mpTimeFormatDate,
                          lpUtf8Target,
                          liTargetSize, lDay.GetBuffer(), lMonth.GetBuffer(), lYear.GetBuffer());
    }

    // Use the locale separators and the Unicode integer conversion.
    void LanguageManager::FormatIntegerString(CgsUnicode::CgsUtf8* lpUtf8Target, s32 liValue, s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1418);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1419);
        CGS_ASSERT_AT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1420);
        CGS_ASSERT_AT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1421);

        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetDecimalPlaces(0);
        lValue.SetMinimumDigits(0);
        lValue.SetDecimalPointCharacter(mpGeneralDecimalSeparator);
        lValue.SetThousandsSeparator(mpGeneralThousandsSeparator);
        lValue.Convert(liValue);

        CgsUnicode::CopyN(lpUtf8Target,
                          lValue.GetBuffer(), liTargetSize);
    }

    // Format the integer with the empty thousands separator.
    void LanguageManager::FormatIntegerNoSeperatorString(CgsUnicode::CgsUtf8* lpUtf8Target, s32 liValue,
                                                         s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1453);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1454);
        CGS_ASSERT_AT(mpGeneralThousandsSeparator != 0, "mpGeneralThousandsSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1455);
        CGS_ASSERT_AT(mpGeneralDecimalSeparator != 0, "mpGeneralDecimalSeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1456);

        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetDecimalPlaces(0);
        lValue.SetMinimumDigits(0);
        lValue.Convert(liValue);

        CgsUnicode::CopyN(lpUtf8Target,
                          lValue.GetBuffer(), liTargetSize);
    }

    // ARTIST copies the integer text first, then applies the locale percentage template.
    void LanguageManager::FormatPercentageString(CgsUnicode::CgsUtf8* lpUtf8Target, s32 liValue,
                                                 s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1529);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1530);
        CGS_ASSERT_AT(mpGeneralPercentage != 0, "mpGeneralPercentage",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1532);

        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetDecimalPlaces(0);
        lValue.SetMinimumDigits(0);
        lValue.Convert(liValue);

        CgsUnicode::CopyN(lpUtf8Target,
                          lValue.GetBuffer(), liTargetSize);
        CgsUnicode::Print(mpGeneralPercentage, lpUtf8Target,
                          liTargetSize, lValue.GetBuffer());
    }

    // Format the value through the locale currency separator and template.
    void LanguageManager::FormatCurrencyString(CgsUnicode::CgsUtf8* lpUtf8Target, s32 liCurrencyValue,
                                               s32 liTargetSize) const
    {
        CGS_ASSERT_AT(lpUtf8Target != 0, "lpTargetString != NULL",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1384);
        CGS_ASSERT_AT(liTargetSize > 0, "lnTargetStringSize > 0",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1385);
        CGS_ASSERT_AT(mpGeneralCurrencySeparator != 0, "mpGeneralCurrencySeparator",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1387);
        CGS_ASSERT_AT(mpGeneralCurrency != 0, "mpGeneralCurrency",
                      CGS_ARTIST_LANGUAGE_MANAGER_FILE, 1388);

        CgsUnicode::UnicodeBuffer lValue;
        lValue.Construct();
        lValue.SetThousandsSeparator(mpGeneralCurrencySeparator);
        lValue.Convert(liCurrencyValue);

        CgsUnicode::Print(mpGeneralCurrency, lpUtf8Target,
                          liTargetSize, lValue.GetBuffer());
    }

    bool LanguageManager::FormatText(char* lpacBuffer, u32 luBufferSize, f32 lfValue,
                                     ParameterFormatType leType)
    {
        CGS_ASSERT_STREAM_AT(leType < E_FORMAT_COUNT,
            "Invalid Localisation Format supplied to TextField::FormatText",
            CGS_ARTIST_LANGUAGE_MANAGER_FILE, 823);

        CgsUnicode::CgsUtf8* lpUtf8Target = reinterpret_cast<CgsUnicode::CgsUtf8*>(lpacBuffer);
        switch (leType)
        {
        case E_FORMAT_INTEGER:
            FormatIntegerString(lpUtf8Target, static_cast<s32>(lfValue), static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_PERCENTAGE:
            FormatPercentageString(lpUtf8Target, static_cast<s32>(lfValue), static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_HOURS_MINUTES_SECONDS:
            FormatHoursMinutesAndSecondsString(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_MINUTES_SECONDS_HUNDREDTHS:
            FormatMinutesAndSecondsAndHundredsString(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_MINUTES_SECONDS:
            FormatMinutesAndSecondsString(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_SECONDS_HUNDREDTHS:
            FormatSecondsAndHundredsString(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_SECONDS:
            FormatSecondsString(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_SECONDS_LONG:
            FormatSecondsStringLong(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_MINUTES_SECONDS_MID_TEXT:
            FormatMinutesSecondsStringMediumText(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_AUTO_DISTANCE:
            FormatAutoDistanceString(lpacBuffer, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_SECONDS_HUNDREDTHS_LONG:
            FormatSecondsAndHundredsStringLong(lpUtf8Target, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_AUTO_DISTANCE_LONG:
            FormatAutoDistanceStringLong(lpacBuffer, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_SMALL_DISTANCE:
            FormatSmallDistanceString(lpacBuffer, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_SMALL_DISTANCE_LONG:
            FormatSmallDistanceStringLong(lpacBuffer, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_LARGE_DISTANCE:
            FormatLargeDistanceString(lpacBuffer, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_LARGE_DISTANCE_LONG:
            FormatLargeDistanceStringLong(lpacBuffer, lfValue, static_cast<s32>(luBufferSize));
            break;
        case E_FORMAT_MONEY:
            FormatCurrencyString(lpUtf8Target, static_cast<s32>(lfValue), static_cast<s32>(luBufferSize));
            break;
        default:
            CGS_FAIL_STREAM_AT("Invalid Parameter sent to SetLocalisedText with float : " << leType << "\n",
                CGS_ARTIST_LANGUAGE_MANAGER_FILE, 916);
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
        CGS_ASSERT_AT(lpacBuffer != 0, "Target field is invalid in TextField::FormatText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 697);
        CGS_ASSERT_AT(lpcSourceText != 0, "Text field is invalid in TextField::FormatText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 698);
        CGS_ASSERT_AT(std::strlen(lpcSourceText) < luBufferSize,
                   "Text string too long in TextField::FormatText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 699);
        CGS_ASSERT_AT(leType < 21, "Invalid Localisation Format supplied to TextField::FormatText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 700);

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
                CGS_ASSERT_AT(std::strlen(reinterpret_cast<const char*>(lpDatabaseString)) <
                               luBufferSize,
                           "Database Text string too long in TextField::FormatText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 721);
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
            CGS_FAIL_AT("Invalid Parameter sent to FormatText", CGS_ARTIST_LANGUAGE_MANAGER_FILE, 770);
            return false;
        }
    }
}

// FOLDED FROM CgsLanguageManager_wS34_00.cpp
// CgsLanguageManager_wS34_00.cpp -- the two-argument FormatAndAddText (CgsLanguageManager.cpp
// family): add a string-table entry under a new id, copying the text of an existing id.

#include "GameShared/GameClasses/Language/CgsLanguageManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsLanguage
{
    // Look the source id up and add its text under lpcStringId. Returns AddString's result.
    bool LanguageManager::FormatAndAddText(const char* lpcStringId, const char* lpcSourceText)
    {
        const u8* lpUtf8 = FindString(lpcSourceText);
        CGS_ASSERT(lpUtf8 != 0, "lpUtf8");
        return AddString(lpcStringId, lpUtf8);
    }
}

#undef CGS_ARTIST_LANGUAGE_MANAGER_FILE
