#include "GameShared/GameClasses/Language/CgsSku.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                     // GuiEventSetSku / GuiEventSetLanguageNotification
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                          // CgsGuiModuleIO::OutputBuffer::AddGuiOutEvent<>
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"                  // ModelIO::InputBuffer::AddResourceRequests
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // GuiEventLoadRequest (the ch-39 record)
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"               // CgsResource::ID::HashString

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
// The sixth ledger function, Update @ 0x828662B8, is homed below: its two former blockers
// are resolved -- the single-event AddGuiOutEvent<> instances + both event types are
// committed (CgsGuiModuleIO.h / CgsGuiEventTypeDefs.h), and the two per-language rodata
// tables were RECOVERED from the ARTIST i64 (idat batch dump, 2026-07-17): the 32-byte-
// stride BUNDLE-name slots @0x820E5AD0 and resource-name slots @0x820E5DD0, indexed by
// ELanguage (unpopulated slots are empty == unsupported languages).

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

    // ---- the per-language rodata tables (recovered from the ARTIST i64) --------------
    // 32-byte-stride char slots indexed by ELanguage. The BUNDLE names (@0x820E5AD0)
    // feed the request's mpacFileToLoad (the module's container template [13] formats
    // "Language\%s.bundle"); the RESOURCE names (@0x820E5DD0) feed the request's
    // hashed resource id (the "%s.lang" acquire). Empty slots = unsupported languages.
    static const char KAAC_LANGUAGE_BUNDLE_NAMES[E_LANGUAGE_TOTAL][32] =
    {
        "",     "",     "",     "",     "",     "",     "",     "0001",   // 7  ENGLISH_US
        "0002", "",     "0003", "0006", "",     "",     "",     "0005",   // 8  ENGLISH_UK .. 15 ITALIAN
        "0007", "",     "",     "",     "",     "",     "0004", "",       // 16 JAPANESE .. 22 SPANISH
    };
    static const char KAAC_LANGUAGE_RESOURCE_NAMES[E_LANGUAGE_TOTAL][32] =
    {
        "",              "", "", "", "", "", "", "english.lang",
        "english_uk.lang", "", "french.lang", "german.lang", "", "", "", "italian.lang",
        "japanese.lang", "", "", "", "", "", "spanish.lang", "",
    };

    // X360 0x828662B8. The per-frame language/SKU pump GuiModule::Update drives:
    //   1. a pending language change with the previous bundle still loaded posts the
    //      previous language's UNLOAD request (type 12) and arms a 10-frame unload
    //      dwell (miWaitForUnload);
    //   2. once the dwell has elapsed, the pending language posts its LOAD request
    //      and becomes the loaded language (the trailing decrement runs once more
    //      after the load frame, exactly as the asm does);
    //   3. a pending SKU notification publishes GuiEventSetSku (event 27);
    //   4. a pending language notification publishes GuiEventSetLanguageNotification
    //      (event 29).
    // The request record carries the hashed RESOURCE name ("english.lang") as the
    // resource id and the BUNDLE name ("0001") as the file to load.
    void Sku::Update(CgsGui::ModelIO::InputBuffer* lpModelInputBuffer,
                     CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        if (mbLoaded && mbLoadRequest)
        {
            CgsGui::GuiEventLoadRequest lRequest;
            lRequest.meRequestType   = static_cast<CgsGui::ResourceRequestTypes>(12);
            lRequest.meLoadUnload    = CgsGui::E_GUI_RESOURCEREQUEST_UNLOAD;
            lRequest.mpacFileToLoad  = KAAC_LANGUAGE_BUNDLE_NAMES[meLoadedLanguage];
            lRequest.muLoadRequestId = 0;
            lRequest.muResourceId    =
                CgsResource::ID::HashString(
                    reinterpret_cast<const u8*>(KAAC_LANGUAGE_RESOURCE_NAMES[meLoadedLanguage]));
            lpModelInputBuffer->AddResourceRequests(lRequest);
            mbLoaded        = false;
            miWaitForUnload = 10;
        }

        if (mbLoadRequest)
        {
            if (miWaitForUnload == 0)
            {
                CgsGui::GuiEventLoadRequest lRequest;
                lRequest.meRequestType   = static_cast<CgsGui::ResourceRequestTypes>(12);
                lRequest.meLoadUnload    = CgsGui::E_GUI_RESOURCEREQUEST_LOAD;
                lRequest.mpacFileToLoad  = KAAC_LANGUAGE_BUNDLE_NAMES[meLanguage];
                lRequest.muLoadRequestId = 0;
                lRequest.muResourceId    =
                    CgsResource::ID::HashString(
                        reinterpret_cast<const u8*>(KAAC_LANGUAGE_RESOURCE_NAMES[meLanguage]));
                lpModelInputBuffer->AddResourceRequests(lRequest);
                mbLoaded         = true;
                mbLoadRequest    = false;
                meLoadedLanguage = meLanguage;
            }
            --miWaitForUnload;
        }

        if (mbSendSku)
        {
            CgsGui::GuiEventSetSku lEvent;
            lEvent.meSku = meSku;
            lpOutput->AddGuiOutEvent(lEvent);
            mbSendSku = false;
        }

        if (mbSendLanguage)
        {
            CgsGui::GuiEventSetLanguageNotification lEvent;
            lEvent.meLanguage = meLanguage;
            lpOutput->AddGuiOutEvent(lEvent);
            mbSendLanguage = false;
        }
    }
}
