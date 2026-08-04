#pragma once

#include "types.hpp"

// CgsLanguage::Sku - the GUI subsystem's per-frame language/SKU selector. It tracks the
// requested language + SKU, drives the load/unload of the active language resource bundle,
// and publishes SetSku / SetLanguage notification events to the GUI output buffer.
//
// Layout + member set recovered from the DecFIGS DWARF (CgsSku.h) and pinned against the
// X360 ARTIST binary store offsets:
//   Construct      @ 0x828607F0   field init + null-module assert
//   FindLanguage   @ 0x828608C0   tail-call CgsSystem::HardwareSku::FindLanguage
//   FindSku        @ 0x828608C8   tail-call CgsSystem::HardwareSku::GetSku
//   SetSku         @ 0x828608B0   meSku = sku; mbSendSku = true
//   SetLanguage    @ 0x82860F20   meLanguage = lang; mbLoadRequest = true; mbSendLanguage = true
//   Update         @ 0x828662B8   per-frame load/unload + notification pump (homed in CgsSku.cpp;
//                                  the language rodata tables were dumped from the ARTIST i64 and
//                                  re-verified 2026-08-04 -- see mapcSupportedLanguage* below)
//
// Member byte offsets proven by the Construct / SetLanguage / SetSku stores:
//   meLanguage       +0x00   stw r11(7) 0(r27) ; SetLanguage stw r4 0(r3)
//   meLoadedLanguage +0x04   stw r11(7) 4(r27)
//   miWaitForUnload  +0x08   stw r29(0) 8(r27)
//   meSku            +0x0C   SetSku stw r4 0xC(r3)
//   mpGuiModule      +0x10   stw r26 0x10(r27)
//   mbLoaded         +0x14   stb r29(0) 0x14(r27)
//   mbLoadRequest    +0x15   stb r29(0) 0x15(r27) ; SetLanguage stb 1 0x15(r3)
//   mbSendLanguage   +0x16   stb r29(0) 0x16(r27) ; SetLanguage stb 1 0x16(r3)
//   mbSendSku        +0x17   SetSku stb 1 0x17(r3)

namespace CgsGui
{
    // The owning GUI subsystem module (CgsGuiModule.cpp). Used only by pointer here.
    struct GuiModule;

    namespace ModelIO
    {
        // The GUI model module's input IO buffer (CgsModelModuleIO.h). Update reads pending
        // resource requests through it.
        struct InputBuffer;
    }

    namespace CgsGuiModuleIO
    {
        // The GUI module's output IO buffer (CgsGuiModuleIO.h). Update publishes the
        // SetSku / SetLanguage notification events through it.
        struct OutputBuffer;
    }
}

namespace CgsLanguage
{
    // CgsSku.h:46 (DWARF). The localisation languages the title knows about.
    enum ELanguage
    {
        E_LANGUAGE_INVALID             = -1,
        E_LANGUAGE_ARABIC              = 0,
        E_LANGUAGE_CHINESE             = 1,
        E_LANGUAGE_CHINESE_SIMPLIFIED  = 2,
        E_LANGUAGE_CHINESE_TRADITIONAL = 3,
        E_LANGUAGE_CZECH               = 4,
        E_LANGUAGE_DANISH              = 5,
        E_LANGUAGE_DUTCH               = 6,
        E_LANGUAGE_ENGLISH_US          = 7,
        E_LANGUAGE_ENGLISH_UK          = 8,
        E_LANGUAGE_FINNISH             = 9,
        E_LANGUAGE_FRENCH              = 10,
        E_LANGUAGE_GERMAN              = 11,
        E_LANGUAGE_GREEK               = 12,
        E_LANGUAGE_HEBREW              = 13,
        E_LANGUAGE_HUNGARIAN           = 14,
        E_LANGUAGE_ITALIAN             = 15,
        E_LANGUAGE_JAPANESE            = 16,
        E_LANGUAGE_KOREAN              = 17,
        E_LANGUAGE_NORWEGIAN           = 18,
        E_LANGUAGE_POLISH              = 19,
        E_LANGUAGE_PORTUGUESE_BRAZIL   = 20,
        E_LANGUAGE_PORTUGUESE_PORTUGAL = 21,
        E_LANGUAGE_SPANISH             = 22,
        E_LANGUAGE_SWEDISH             = 23,
        E_LANGUAGE_TOTAL               = 24,
    };

    // CgsSku.h:79 (DWARF). The retail hardware SKUs.
    enum ESku
    {
        E_SKU_US      = 0,
        E_SKU_EURO_1  = 1,
        E_SKU_EURO_2  = 2,
        E_SKU_JAPAN   = 3,
        E_SKU_KOREA   = 4,
        E_SKU_ROA     = 5,
        E_SKU_DEFAULT = 6,
        E_SKU_TOTAL   = 7,
    };

    // CgsSku.h:94 (DWARF). The number of language slots a SKU exposes.
    const s32 KI_SKU_LANGUAGES = 7;

    // CgsSku.h:112 (DWARF).
    struct Sku
    {
        // CgsSku.h:118 (DWARF). X360 0x828607F0.
        void Construct(CgsGui::GuiModule* lpGuiModule);

        // CgsSku.h:123 (DWARF). X360 0x82860F20.
        void SetLanguage(CgsLanguage::ELanguage leLanguage);

        // CgsSku.h:127 (DWARF). Reads meLanguage by name. (Not in this batch's ledger set.)
        ELanguage GetLanguage() const;

        // CgsSku.h:131 (DWARF). X360 0x828608C0 -- tail-call into the committed hardware SKU.
        ELanguage FindLanguage();

        // CgsSku.h:136 (DWARF). X360 0x828608B0.
        void SetSku(CgsLanguage::ESku leSku);

        // CgsSku.h:140 (DWARF). Reads meSku by name. (Not in this batch's ledger set.)
        ESku GetSku() const;

        // CgsSku.h:144 (DWARF). X360 0x828608C8 -- tail-call into the committed hardware SKU.
        ESku FindSku();

        // CgsSku.h:149 (DWARF). X360 0x828662B8. Per-frame pump: issues language load/unload
        // resource requests against the input buffer and publishes SetSku / SetLanguage
        // notifications to the output buffer. Bodied in CgsSku.cpp.
        void Update(CgsGui::ModelIO::InputBuffer* lpModelInputBuffer,
                    CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput);

    private:
        // CgsSku.h:169 (DWARF). Issues the load request for a language bundle. (Not in this
        // batch's ledger set: no X360 out-of-line body exists -- its shape survives only as
        // the two request-building blocks the compiler inlined into Update @0x828662B8.)
        void RequestLoadLanguage(CgsLanguage::ELanguage leLanguage);

        // CgsSku.h:176 / :177 (DWARF names). The per-language 32-byte-stride string tables
        // Update indexes by ELanguage, RECOVERED from the ARTIST i64 rodata (dumped via
        // headless idat, re-verified byte-for-byte 2026-08-04; sole xrefs are Update
        // @0x828662E8/0x828662F0, so this TU owns them):
        //   mapcSupportedLanguageFilenames @ X360 0x820E5AD0 -- the request's mpacFileToLoad
        //     ("0001".."0007" container names); empty slot == unsupported language.
        //   mapcSupportedLanguageResources @ X360 0x820E5DD0 -- the "<language>.lang" names
        //     hashed into the request's muResourceId (CgsResource::ID::HashString).
        // Definitions (with the full dumped contents) live in CgsSku.cpp.
        static const char mapcSupportedLanguageFilenames[E_LANGUAGE_TOTAL][32];
        static const char mapcSupportedLanguageResources[E_LANGUAGE_TOTAL][32];

        ELanguage meLanguage;       // +0x00  CgsSku.h:186
        ELanguage meLoadedLanguage; // +0x04  CgsSku.h:189
        s32       miWaitForUnload;  // +0x08  CgsSku.h:190
        ESku      meSku;            // +0x0C  CgsSku.h:193
        CgsGui::GuiModule* mpGuiModule; // +0x10  CgsSku.h:195
        bool      mbLoaded;         // +0x14  CgsSku.h:197
        bool      mbLoadRequest;    // +0x15  CgsSku.h:198
        bool      mbSendLanguage;   // +0x16  CgsSku.h:199
        bool      mbSendSku;        // +0x17  CgsSku.h:201
    };
}
