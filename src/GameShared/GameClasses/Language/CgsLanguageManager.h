#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Language/CgsSku.h"                            // CgsLanguage::ELanguage
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"                           // CgsUnicode::CgsUtf8 (== u8)
#include "GameShared/GameClasses/Containers/CgsHashTable.h"                    // CgsContainers::HashTable/HashTableElement
#include "GameShared/GameClasses/Containers/CgsLinkedList.h"                   // CgsContainers::LinkedListHelper/LinkedListNode
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                       // CgsMemory::HeapMalloc
#include "GameShared/GameClasses/Language/CgsLanguageManagerDebugComponent.h"  // CgsLanguage::LanguageManagerDebugComponent (by-value member)

namespace CgsResource { struct LanguageResource; }

// CgsLanguage::LanguageManager - localisation/units manager: owns the loaded string table (a
// fixed-13-bin hash of hashed-key -> UTF-8 string), the dynamic (formatted/allocated) string
// bookkeeping, the per-locale format separators/templates, and an embedded debug HUD.
//
// LAYOUT (DecFIGS DWARF, CgsLanguageManager.h:65 -- member order/names/types), gated against the
// X360 ARTIST ctor @ 0x827DF9C8, which attests the offsets below (X360 32-bit-pointer build; per
// the x64-gate rule in AGENTS.md this reconstruction uses NAMED members, not raw offsets, so the
// x64 compile naturally has different spacing/size -- byte-exactness is not the goal):
//   +0x00   meLanguage                      (CgsLanguage::ELanguage; NOT touched by this ctor)
//   +0x04   mpcDefaultFontName               (const char*; NOT touched by this ctor -- read by GetDefaultFont)
//   +0x08   mStrings                         (HashIDStringArray = HashTable<u32,const CgsUtf8*,13>; ctor seeds
//                                             each of the 13 bins' miCount to the uninitialised sentinel)
//   +0xA8   mDynamicStringElements           (DynamicHashElementsList = LinkedListHelper<HashTableElement*,1024>;
//                                             ctor seeds miNumNodes=1024 then InternalInit's the free/live sublists)
//   +0x30C4 mDynamicStringPointerElements    (same type as above; ctor repeats the same seed/Init pair)
//   +0x60E0 mpStringElements, mpLanguageAllocator, mpResource, mePrepareStage, meReleaseStage,
//           mbIsUsingMetricUnits, mrLargeDistanceConversion (+0x60F8), mrSmallDistanceConversion, and
//           the 21 per-locale format-string pointers (mpGeneral*/mpTimeFormat*/mpDistanceFormat*) --
//           NOT touched by this ctor (populated by Construct(), a separate not-yet-reconstructed
//           function); mrLargeDistanceConversion's offset independently matches the
//           GetDistanceDisplayScale accessor comment below (InGameMessageRenderer usage)
//   +0x6154 mDebugComponent                  (LanguageManagerDebugComponent, by value; the ctor manually
//                                             stamps its vtable pointer -- the CgsDev::DebugComponent base
//                                             ctor folded/inlined into this one)
namespace CgsLanguage
{
    class LanguageManager
    {
    public:
        // CgsLanguageManager.h:98 (DWARF).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START   = 0,
            E_PREPARESTAGE_MANAGER = 1,
            E_PREPARESTAGE_DONE    = 2,
        };

        // CgsLanguageManager.h:105 (DWARF).
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };

        // ADDITIVE GROW (GuiFlow-consumers group): the localised-value format selector
        // (DWARF: CgsLanguageManager.h:84 `enum ParameterFormatType`). The GUI text
        // fields pass it to LanguageManager::FormatParameter / TextField::SetLocalisedText
        // to pick how a value renders (raw text, a clock format, an id-table lookup, a
        // distance, ...). E_FORMAT_ID_LOOKUP (9) is the "look this string up in the
        // localisation database by id" mode the drive-thru panel uses for its
        // "DT_NAME_%llu" / "DT_LOC_%llu" keys. Values are X360-attested via the
        // SetLocalisedText overloads that take this type.
        enum ParameterFormatType
        {
            E_FORMAT_TEXT                       = 0,
            E_FORMAT_HOURS_MINUTES_SECONDS      = 1,
            E_FORMAT_MINUTES_SECONDS_HUNDREDTHS = 2,
            E_FORMAT_MINUTES_SECONDS            = 3,
            E_FORMAT_SECONDS_HUNDREDTHS         = 4,
            E_FORMAT_SECONDS_HUNDREDTHS_LONG    = 5,
            E_FORMAT_SECONDS                    = 6,
            E_FORMAT_SECONDS_LONG               = 7,
            E_FORMAT_MINUTES_SECONDS_MID_TEXT   = 8,
            E_FORMAT_ID_LOOKUP                  = 9,
            E_FORMAT_ID_LOOKUP_TOUPPER          = 10,
            E_FORMAT_INTEGER                    = 11,
            E_FORMAT_INTEGER_NOSEPERATOR        = 12,
            E_FORMAT_PERCENTAGE                 = 13,
            E_FORMAT_MONEY                      = 14,
            E_FORMAT_AUTO_DISTANCE              = 15,
            E_FORMAT_AUTO_DISTANCE_LONG         = 16,
            E_FORMAT_SMALL_DISTANCE            = 17,
            E_FORMAT_SMALL_DISTANCE_LONG        = 18,
            E_FORMAT_LARGE_DISTANCE             = 19,
            E_FORMAT_LARGE_DISTANCE_LONG        = 20,
            E_FORMAT_COUNT                      = 21,
        };

        // X360 0x827DF9C8. Seeds the two 1024-node dynamic-string LinkedListHelper pools (their free
        // sublists own every node, their live sublists start empty) and every mStrings hash bin's
        // miCount sentinel, then stamps mDebugComponent's vtable pointer. meLanguage,
        // mpcDefaultFontName, mpResource, the allocator/format-string pointers and the metric flag
        // are left untouched here (Construct() nulls mpResource + the allocator/element ptrs and sets
        // the metric flag; Prepare() stashes the allocator; PrepareDefaultFormattingStrings() fills the
        // format strings). See ViewModule.h for the by-value-member call site (CgsGui::ViewModule).
        LanguageManager();

        // X360 0x82862490. Runtime (re-)init to a clean PRE-LOAD state (NOT the string-table loader --
        // the asm NULLS mpResource + Init's mStrings EMPTY, disproving a prior comment): resets the
        // lifecycle stages (prepare=START, release=DONE), nulls the resource/allocator/string-element
        // pointers, re-seeds both dynamic-string pools, Init's the empty mStrings hash, nulls the
        // default font, sets metric units on, and constructs the embedded debug component. Called by
        // CgsGui::ViewModule::Construct (0x828605A0).
        void Construct();

        void SetUseMetricUnits(bool lbUseMetric);
        bool IsUsingMetricUnits() const { return mbIsUsingMetricUnits; }

        // ADDITIVE GROW (GUI text consumers): look up a localised string by its database key
        // (e.g. "CREDITS_TITLE_0"). Returns the UTF-8 string, or NULL when the key is not in
        // the localisation database. The X360 emits it out-of-line; callers (e.g. the credits
        // renderer's RecalculateParagraphs) pass a SPrintf'd key and store the result as a
        // CgsResource::CgsUtf8* to feed the text path. Body links from the CgsLanguageManager
        // TU. (DWARF buffer type is CgsUnicode::CgsUtf8* == u8; returned as that here.)
        const u8* FindString(const char* lpcKey) const;

        // X360 0x82864028. FindString's back-end: looks a precomputed key hash up in the
        // manager's loaded localised-string table and returns the stored UTF-8 string pointer
        // (or NULL when the table holds no entry for that hash). Split out so the credits /
        // HUD paths that already hold a hash skip the re-hash. Body links from this TU.
        const u8* FindStringByHash(unsigned int luHash) const;

        // X360 0x828647F8 / 0x82864A08. Add a localised string under lpcStringId, evicting any
        // prior entry for the same key first. AddString heap-copies lpcString (the manager owns
        // the copy, tracked in mDynamicStringElements); AddStringPointer instead stores the
        // caller's pointer directly (the caller owns its lifetime, tracked in
        // mDynamicStringPointerElements). Both always return true. Bodies link from this TU.
        bool AddString(const char* lpcStringId, const u8* lpcString);
        bool AddStringPointer(const char* lpcStringId, const u8* lpcString);

        // FLAG (PC bring-up shim; NOT an X360 method): install one loaded string-table entry by
        // its PRECOMPUTED key hash. The serialised LanguageResource carries only {hash, string}
        // pairs (no key strings) -- this is LoadStringTable's per-entry observable effect (an
        // AddStringPointer minus the re-hash; the caller owns the string's lifetime, exactly
        // like AddStringPointer). Retained for the dynamic-string consumers; the STATIC table
        // install is LoadStringTable below.
        bool AddStringPointerByHash(unsigned int luHash, const u8* lpcString);

        // X360 0x828664B8. Install a loaded (FixUp-relocated) string table: assert the language
        // id, unload any prior table, CAlloc one hash element per entry from the language
        // allocator (mpStringElements -- the bulk static block), insert every {hash, string}
        // pair into mStrings (each string UTF-8-validated), record the resource
        // (mpResource), re-derive the formatting strings, pick the language's default font
        // ("dfheic" for language 16, the wide-glyph locale, else "NODEFAULTFONTSPECIFIED"),
        // and stamp meLanguage. ViewModule::ProcessIncomingLoadNotification (the
        // localised-text load notification) is its only X360 caller. Body in this TU.
        void LoadStringTable(CgsResource::LanguageResource* lpResource);

        // X360 0x82862540. Tear the loaded table down (only when one is loaded): re-Init the
        // mStrings bins, null mpResource, drain both dynamic-string lists back to their free
        // sublists (freeing each dynamic entry's heap copy + node), free the bulk static
        // element block (mpStringElements), and re-derive the default formatting strings.
        void UnloadStringTable();

        // X360 0x828651A0 / 0x82866450 (DWARF CgsLanguageManager.h:200/:231). Resolve
        // lpcSourceText through the given format (E_FORMAT_ID_LOOKUP resolves it as a
        // loc-string id) into a local 1KB buffer, then AddString the result under
        // lpcStringId. The vararg overload formats liNumParams positional parameters
        // into the resolved string first; each vararg pair is (const char* text,
        // ParameterFormatType) -- the overlay popups pass their GuiPopupParameter's
        // text + raw type word (BaseOk/OkCancelOverlayState::SetupOverlay
        // @0x824B1BC0/@0x824B1C78, the "TEMP_POPUP_STRING1/2" button labels).
        // ADDITIVE GROW: declaration-only (their bodies are this TU's own ledger
        // functions, not yet reconstructed).
        // X360 0x82864C48 (DWARF CgsLanguageManager.h:255) -- resolve lpcSourceText
        // through the given format into the caller's buffer (E_FORMAT_ID_LOOKUP resolves
        // it as a loc-string id; the value formats render clocks/distances/money).
        // TextField::SetLocalisedText resolves through it with a 1024 cap. ADDITIVE GROW:
        // declaration-only (its body is this TU's own ledger function).
        bool FormatText(char* lpacBuffer, u32 luBufferSize, const char* lpcSourceText,
                        ParameterFormatType leType);

        bool FormatAndAddText(const char* lpcStringId, const char* lpcSourceText,
                              ParameterFormatType leType);
        bool FormatAndAddText(const char* lpcStringId, const char* lpcSourceText,
                              ParameterFormatType leType, s32 liNumParams, ...);

        // X360 0x82864950 / 0x828640B0. Remove the string added by AddString, by id or by a
        // precomputed hash. Returns false when no matching dynamic-string entry is found.
        // Bodies link from this TU.
        bool RemoveString(const char* lpcStringId);
        bool RemoveStringByHash(unsigned int luHash);

        // X360 0x82864B30 / 0x82864158. Remove the string added by AddStringPointer, by id or by
        // a precomputed hash. Returns false when no matching dynamic-string-pointer entry is
        // found. Bodies link from this TU.
        bool RemoveStringPointer(const char* lpcStringId);
        bool RemoveStringPointerByHash(unsigned int luHash);

        // The active language id. The X360 reads it as the manager's leading field
        // (the InGameMessageRenderer compares it against 16 -- a wide-glyph language --
        // to nudge the on-screen message Y-position). Exposed as a named accessor so
        // callers read it by name rather than poking offset 0. Body links from the
        // CgsLanguageManager TU. (CgsLanguage::ELanguage modelled as s32; 0 = English.)
        s32 GetCurrentLanguage() const { return static_cast<s32>(meLanguage); }

        // Localised value -> string formatters. The overlapping signatures (target buffer, value(s),
        // buffer size) are grounded in the DWARF; the X360 ARTIST build adds the XoverY / Date /
        // *AndHundreds variants.
        // The DWARF types the buffer as CgsUnicode::CgsUtf8* (== u8); modelled here as char*
        // since callers (e.g. the debug HUD) pass a plain byte buffer straight to the text renderer.
        // Bodies link from the CgsLanguageManager TU.
        void FormatIntegerString(char* lpcTarget, s32 liValue, s32 liTargetSize) const;
        void FormatXoverYString(char* lpcTarget, s32 liX, s32 liY, s32 liTargetSize) const;
        void FormatPercentageString(char* lpcTarget, s32 liValue, s32 liTargetSize) const;
        void FormatCurrencyString(char* lpcTarget, s32 liCurrencyValue, s32 liTargetSize) const;
        void FormatDateString(char* lpcTarget, s32 liDays, s32 liMonths, s32 liYears, s32 liTargetSize) const;

        void FormatHoursMinutesAndSecondsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatMinutesAndSecondsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatMinutesAndSecondsAndHundredsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatSecondsAndHundredsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;
        void FormatSecondsString(char* lpcTarget, f32 lfTimeInSeconds, s32 liTargetSize) const;

        void FormatSmallDistanceString(char* lpcTarget, f32 lfMetres, s32 liTargetSize) const;
        void FormatLargeDistanceString(char* lpcTarget, f32 lfMetres, s32 liTargetSize) const;

        // The metres -> display-unit scale the HUD multiplies a distance by to decide whether the
        // small or large distance string reads better (X360 reads it as a float member at +0x60F8;
        // exposed as a named accessor so callers don't poke the raw offset).
        f32 GetDistanceDisplayScale() const;

        // X360 0x824434C0. Returns the loaded default font name (mpcDefaultFontName), asserting first
        // that Construct()/PrepareDefaultFont() has already set it (a null default font is a
        // programming error, not a recoverable runtime state -- the assert fires and the function
        // still returns the null pointer, matching the X360 body exactly).
        const char* GetDefaultFont() const;

        // X360 0x828608D0. Stashes the language allocator (mpLanguageAllocator, used by every
        // Add/RemoveString* body's Malloc/Free) and registers the embedded debug component with
        // the debug menu. Body links from this TU.
        bool Prepare(CgsMemory::HeapMalloc* lpLanguageAllocator);
        bool Release();
        void Destruct();

        // X360 0x82860940. Stamps every per-locale format separator/template member with its
        // English-default literal and sets the metric flag to true. Body links from this TU.
        bool PrepareDefaultFormattingStrings();

    private:
        // CgsLanguageManager.h:52 (DWARF).
        typedef CgsContainers::HashTable<u32, const CgsUnicode::CgsUtf8*, 13> HashIDStringArray;
        // CgsLanguageManager.h:54 (DWARF).
        typedef CgsContainers::LinkedListHelper<CgsContainers::HashTableElement<u32, const CgsUnicode::CgsUtf8*>*, 1024> DynamicHashElementsList;

        CgsLanguage::ELanguage meLanguage;                    // +0x00 -- NOT touched by the ctor
        const char*            mpcDefaultFontName;             // +0x04 -- NOT touched by the ctor; read by GetDefaultFont
        HashIDStringArray      mStrings;                       // +0x08
        DynamicHashElementsList mDynamicStringElements;        // +0xA8
        DynamicHashElementsList mDynamicStringPointerElements; // +0x30C4
        CgsContainers::HashTableElement<u32, const CgsUnicode::CgsUtf8*>* mpStringElements; // +0x60E0 -- NOT touched by the ctor
        CgsMemory::HeapMalloc*        mpLanguageAllocator;     // +0x60E4 -- NOT touched by the ctor
        CgsResource::LanguageResource* mpResource;              // +0x60E8 -- NOT touched by the ctor
        EPrepareStage           mePrepareStage;                 // +0x60EC -- NOT touched by the ctor
        EReleaseStage           meReleaseStage;                 // +0x60F0 -- NOT touched by the ctor
        bool                    mbIsUsingMetricUnits;           // +0x60F4 -- NOT touched by the ctor
        f32                     mrLargeDistanceConversion;      // +0x60F8 -- NOT touched by the ctor
        f32                     mrSmallDistanceConversion;      // +0x60FC -- NOT touched by the ctor

        const CgsUnicode::CgsUtf8* mpGeneralDecimalSeparator;      // +0x6100
        const CgsUnicode::CgsUtf8* mpGeneralThousandsSeparator;    // +0x6104
        const CgsUnicode::CgsUtf8* mpGeneralPercentage;            // +0x6108
        const CgsUnicode::CgsUtf8* mpGeneralXOverY;                // +0x610C
        const CgsUnicode::CgsUtf8* mpGeneralCurrencySeparator;     // +0x6110
        const CgsUnicode::CgsUtf8* mpGeneralCurrency;              // +0x6114
        const CgsUnicode::CgsUtf8* mpTimeFormatDate;                // +0x6118
        const CgsUnicode::CgsUtf8* mpTimeFormatAll;                 // +0x611C
        const CgsUnicode::CgsUtf8* mpTimeFormatHrsMinsSecs;         // +0x6120
        const CgsUnicode::CgsUtf8* mpTimeFormatMinsSecsHnds;        // +0x6124
        const CgsUnicode::CgsUtf8* mpTimeFormatMinsSecs;            // +0x6128
        const CgsUnicode::CgsUtf8* mpTimeFormatSecsHnds;            // +0x612C
        const CgsUnicode::CgsUtf8* mpTimeFormatSecs;                // +0x6130
        const CgsUnicode::CgsUtf8* mpTimeFormatSecsLong;            // +0x6134
        const CgsUnicode::CgsUtf8* mpTimeFormatMinSecsMidText;      // +0x6138
        const CgsUnicode::CgsUtf8* mpTimeFormatMinsSecsMidText;     // +0x613C
        const CgsUnicode::CgsUtf8* mpDistanceFormatShort;           // +0x6140
        const CgsUnicode::CgsUtf8* mpDistanceFormatShortL;          // +0x6144
        const CgsUnicode::CgsUtf8* mpDistanceFormatLong;            // +0x6148
        const CgsUnicode::CgsUtf8* mpDistanceFormatLongL;           // +0x614C
        const CgsUnicode::CgsUtf8* mpDistanceFormatIsMetric;        // +0x6150

        LanguageManagerDebugComponent mDebugComponent;              // +0x6154 -- vtable stamped by the ctor
    };
}
