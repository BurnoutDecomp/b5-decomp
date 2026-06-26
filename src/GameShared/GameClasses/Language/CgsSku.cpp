#include "GameShared/GameClasses/Language/CgsSku.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsLanguage::Sku member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This TU homes the five field-init / setter / hardware-delegate functions of the GUI
// language/SKU selector:
//
//   Construct     @ 0x828607F0  field init (+ null-module assert)
//   FindLanguage  @ 0x828608C0  tail-call CgsSystem::HardwareSku::FindLanguage
//   FindSku       @ 0x828608C8  tail-call CgsSystem::HardwareSku::GetSku
//   SetSku        @ 0x828608B0  meSku = sku; mbSendSku = true
//   SetLanguage   @ 0x82860F20  meLanguage = lang; mbLoadRequest = mbSendLanguage = true
//
// The sixth ledger function, Update @ 0x828662B8, is NOT homed here. It is blocked on two
// dependencies that are not faithfully reconstructable in this batch:
//   * the single-event CgsGui::CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<GuiEventSetSku> /
//     AddGuiOutEvent<GuiEventSetLanguageNotification> template instances and their
//     GuiEventSetSku / GuiEventSetLanguageNotification event types, which are an uncommitted
//     part of the GUI module-IO output subsystem (only the bulk AddGuiOutEvents is homed); and
//   * the two per-language rodata tables the Update load path indexes -- the 32-byte-stride
//     language filename table (byte_820E5DD0 == mapcSupportedLanguageFilenames) and the
//     32-byte-stride resource-name table (unk_820E5AD0 == mapcSupportedLanguageResources) --
//     whose string VALUES live in unrecovered image .rdata and drive the HashString /
//     AddResourceRequests calls.
// Bodying Update would require fabricating those .rdata string values and an uncommitted
// event-publishing template, so it is left unhomed and refined as still-blocked.

namespace CgsSystem
{
    // CgsSystem::HardwareSku is homed per-platform in CgsHardwareSku{PC,PS3}.cpp with no shared
    // header; declared minimally here so the FindLanguage / FindSku delegates can call it.
    // (The X360 build tail-calls FindLanguage / GetSku from the Sku thunks @0x828608C0 /
    // 0x828608C8.) Modelled as the class-static form attested by the PS3 definition home and the
    // X360-reconstructed CgsServerInterfaceServerInfo caller.
    class HardwareSku
    {
    public:
        static s32 GetSku();
        static s32 FindLanguage();
    };
}

namespace CgsLanguage
{
    // X360 0x828607F0. The X360 null-module check uses the StrStream assert front-end
    // (BeginAssert / StrStream "Invalid gui module pointer" / FireAssert / EndAssert); the
    // house CGS_ASSERT substitution carries the same null-pointer guard and message. The X360
    // baked file/line ("..\\..\\..\\GameShared\\GameClasses\\Language/CgsSku.cpp", 158) are
    // discarded per project convention.
    //
    // Field init (store-for-store): meLanguage = meLoadedLanguage = E_LANGUAGE_ENGLISH_US(7);
    // miWaitForUnload = 0; mpGuiModule = lpGuiModule; mbLoaded = mbLoadRequest = mbSendLanguage
    // = 0. (meSku is intentionally NOT initialised by the X360 ctor.) The two ELanguage fields
    // are seeded to the literal 7 the asm stores (li r11,7; stw r11,0; stw r11,4) -- the
    // E_LANGUAGE_ENGLISH_US slot.
    void Sku::Construct(CgsGui::GuiModule* lpGuiModule)
    {
        CGS_ASSERT(lpGuiModule != 0, "Invalid gui module pointer");

        mpGuiModule     = lpGuiModule;        // +0x10  stw r26, 0x10(r27)
        mbLoaded        = false;              // +0x14  stb r29(0), 0x14(r27)
        mbLoadRequest   = false;              // +0x15  stb r29(0), 0x15(r27)
        mbSendLanguage  = false;              // +0x16  stb r29(0), 0x16(r27)
        miWaitForUnload = 0;                  // +0x08  stw r29(0), 8(r27)
        meLanguage      = E_LANGUAGE_ENGLISH_US; // +0x00  stw r11(7), 0(r27)
        meLoadedLanguage = E_LANGUAGE_ENGLISH_US; // +0x04 stw r11(7), 4(r27)
    }

    // X360 0x828608C0: `b CgsSystem::HardwareSku::FindLanguage` -- a pure tail-call thunk.
    ELanguage Sku::FindLanguage()
    {
        return static_cast<ELanguage>(CgsSystem::HardwareSku::FindLanguage());
    }

    // X360 0x828608C8: `b CgsSystem::HardwareSku::GetSku` -- a pure tail-call thunk.
    ESku Sku::FindSku()
    {
        return static_cast<ESku>(CgsSystem::HardwareSku::GetSku());
    }

    // X360 0x82860F20. meLanguage = lang; mbLoadRequest = 1; mbSendLanguage = 1.
    //   stw r4, 0(r3)    -> meLanguage    (+0x00)
    //   stb r11(1), 0x15 -> mbLoadRequest (+0x15)
    //   stb r11(1), 0x16 -> mbSendLanguage(+0x16)
    void Sku::SetLanguage(CgsLanguage::ELanguage leLanguage)
    {
        meLanguage     = leLanguage;
        mbLoadRequest  = true;
        mbSendLanguage = true;
    }

    // X360 0x828608B0. meSku = sku; mbSendSku = 1.
    //   stw r4, 0xC(r3)  -> meSku     (+0x0C)
    //   stb r11(1), 0x17 -> mbSendSku (+0x17)
    void Sku::SetSku(CgsLanguage::ESku leSku)
    {
        meSku     = leSku;
        mbSendSku = true;
    }
}
