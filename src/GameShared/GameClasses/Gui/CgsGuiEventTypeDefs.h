#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"      // CgsGui::GuiEvent<N> (12-byte event header)
#include "GameShared/GameClasses/Language/CgsSku.h"       // CgsLanguage::ELanguage / ESku enums

// CgsGui::GuiEventTimeInfo - the per-frame time payload carried on GUI update events.
// GetTime() returns the "now" stamp (a 32-bit float widened to double) after asserting
// it has been initialised (i.e. is not the -FLT_MAX "unset" marker).
//
// Layout from BURNOUT_X360_ARTIST.XEX @ 0x8240E328 (GetTime): mfTimeNow is read with
// `lfs ...,4(this)` so it sits at +0x04, and is compared against -FLT_MAX before being
// returned. Path/line from the baked assert string
// (GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h:250).
namespace CgsGui
{
    class GuiEventTimeInfo
    {
    public:
        // @ 0x8240E328 - assert mfTimeNow has been set (!= -FLT_MAX) and return it
        // widened to double, matching the X360's lfs->return of the f32.
        f64 GetTime() const;

    private:
        f32 mfTimeDelta;    // +0x00 - leading frame-delta companion to mfTimeNow
        f32 mfTimeNow;      // +0x04 - current time stamp (-FLT_MAX while unset)
    };

    static_assert(sizeof(GuiEventTimeInfo) == 8, "GuiEventTimeInfo: mfTimeNow at +0x04");

    // ===============================================================================
    // GUI-event PAYLOAD TYPE family (controller-input + SKU/language sub-cluster).
    //
    // ADDITIVE GROW. The leading GuiEventTimeInfo home above is untouched. The structs
    // below are the canonical CgsGui GUI-event payloads that the two named producers
    // synthesise and publish:
    //   * CgsLanguage::Sku::Update @0x828662B8 publishes GuiEventSetSku (event 27) and
    //     GuiEventSetLanguageNotification (event 29) through
    //     OutputBuffer::AddGuiOutEvent<T>; the payload word it stores is the enum value
    //     (Update: v12[0] = *(this+0xC) == meSku for SetSku; v12[0] = *this == meLanguage
    //     for SetLanguageNotification).
    //   * BrnGame::BrnGameModule::BridgeControllerToGui @0x823E6B18 synthesises the
    //     controller-input events (GuiEventActiveUserIndex, ...InputPressed/Down/Released,
    //     ...Axis, GuiControllerDisconnected) and the language event from the player pad.
    //
    // LAYOUT SOURCE: references/DecFIGS/dwarfdump/GameShared/GameClasses/Gui/
    // CgsGuiEventTypeDefs.h names every member and the GuiEvent<N> event-type id (the
    // template id is the on-queue record type tag). Each event carries the 12-byte
    // GuiEvent header (muHeader0/muEventType/muHeader2 from CgsGuiEvent.h) followed by the
    // named payload members below; the enum widths (s32) match the X360 stw/lwz widths.
    // These are plain payload PODs (no out-of-line bodies in this header's X360 image);
    // they are homed for layout so producers/consumers can reference them BY NAME.
    // ===============================================================================

    // --- controller-input events (DWARF :77/:85/:92/:99/:106/:123) ------------------
    // Identifier event for the active controller user (-1 == none). DWARF :77.
    struct GuiEventActiveUserIndex : public GuiEvent<4>
    {
        static const s32 KI_INVALID_ACTIVE_USER_INDEX = -1; // DWARF :80 (stored as 0xFFFFFFFF)
        s32 miActiveUserIndex;                              // DWARF :81
    };

    // A controller action transitioned to / is in the "down" state. DWARF :85.
    struct GuiEventControllerInputDown : public GuiEvent<5>
    {
        s32 miPadId;     // DWARF :87
        s32 miButtonId;  // DWARF :88
    };

    // A controller action was just pressed (rising edge). DWARF :99.
    struct GuiEventControllerInputPressed : public GuiEvent<6>
    {
        s32 miPadId;     // DWARF :101
        s32 miButtonId;  // DWARF :102
    };

    // A controller action was just released (falling edge). DWARF :92.
    struct GuiEventControllerInputReleased : public GuiEvent<7>
    {
        s32 miPadId;     // DWARF :94
        s32 miButtonId;  // DWARF :95
    };

    // A controller analogue axis sample. DWARF :106.
    struct GuiEventControllerAxis : public GuiEvent<8>
    {
        s32 miAxis;      // DWARF :117
        f32 mfXAxis;     // DWARF :118
        f32 mfYAxis;     // DWARF :119
    };

    // A controller was disconnected. DWARF :123.
    struct GuiControllerDisconnected : public GuiEvent<9>
    {
        s32 miPlayer;    // DWARF :125
        s32 miPort;      // DWARF :126
    };

    // --- SKU / language events (DWARF :268/:274/:280/:287/:294) ---------------------
    // SKU set request. CgsSku::Update copies meSku (this+0xC) into the payload word and
    // publishes this as event 27. DWARF :268.
    struct GuiEventSetSku : public GuiEvent<27>
    {
        CgsLanguage::ESku meSku;          // DWARF :270 (Update v12[0] = *(this+0xC))
    };

    // Language set request from the controller/front-end. DWARF :274.
    struct GuiEventSetLanguage : public GuiEvent<28>
    {
        CgsLanguage::ELanguage meLanguage; // DWARF :276
    };

    // Language-changed notification. CgsSku::Update copies meLanguage (this+0x00) into the
    // payload word and publishes this as event 29. DWARF :280.
    struct GuiEventSetLanguageNotification : public GuiEvent<29>
    {
        CgsLanguage::ELanguage meLanguage; // DWARF :282 (Update v12[0] = *this)
    };

    // A request for the GUI to report the current language back. DWARF :287.
    struct GuiEventGetLanguage : public GuiEvent<31>
    {
        CgsLanguage::ELanguage mLanguage;  // DWARF :289 (filled in by the responder)
    };

    // A bare request-for-language event (no payload beyond the header). DWARF :294/:652.
    struct GuiEventRequestLanguage : public GuiEvent<30>
    {
    };

    // X360-pinned payload offsets. CgsModule::Event is an empty base, so the GuiEvent<N>
    // header (muHeader0/muEventType/muHeader2 == 3 x u32) occupies +0x00..+0x0B and the
    // first payload member lands at +0x0C. This matches CgsSku::Update, which reads/writes
    // the SetSku payload word at this+0xC (meSku) and the SetLanguageNotification payload
    // word at the language source this+0x00. (The event-type id is the GuiEvent<N> template
    // argument, self-documenting in each type name above.)
    static_assert(sizeof(GuiEvent<27>) == 0x0C, "GuiEvent header is 12 bytes (3 x u32)");
    static_assert(__builtin_offsetof(GuiEventSetSku, meSku)                       == 0x0C, "GuiEventSetSku::meSku @+0x0C (Update this+0xC)");
    static_assert(__builtin_offsetof(GuiEventSetLanguageNotification, meLanguage) == 0x0C, "GuiEventSetLanguageNotification::meLanguage @+0x0C");
    static_assert(__builtin_offsetof(GuiEventSetLanguage, meLanguage)             == 0x0C, "GuiEventSetLanguage::meLanguage @+0x0C");
    static_assert(__builtin_offsetof(GuiEventActiveUserIndex, miActiveUserIndex)  == 0x0C, "GuiEventActiveUserIndex::miActiveUserIndex @+0x0C");
    static_assert(__builtin_offsetof(GuiEventControllerInputPressed, miPadId)     == 0x0C, "controller pad id @+0x0C");
    static_assert(__builtin_offsetof(GuiEventControllerInputPressed, miButtonId)  == 0x10, "controller button id @+0x10");
    static_assert(__builtin_offsetof(GuiEventControllerAxis, miAxis)              == 0x0C, "axis id @+0x0C");
    static_assert(__builtin_offsetof(GuiEventControllerAxis, mfXAxis)             == 0x10, "axis x @+0x10");
    static_assert(__builtin_offsetof(GuiEventControllerAxis, mfYAxis)             == 0x14, "axis y @+0x14");
    static_assert(__builtin_offsetof(GuiControllerDisconnected, miPlayer)         == 0x0C, "disconnect player @+0x0C");
    static_assert(__builtin_offsetof(GuiControllerDisconnected, miPort)           == 0x10, "disconnect port @+0x10");
}
