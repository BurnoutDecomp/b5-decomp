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

        // AddGuiEvent<GuiEventTimeInfo> @0x823D1468 -> AddEvent(&event, 26, 8). The type is a
        // plain payload (not GuiEvent<N>-derived), so the queued event id is carried here.
        s32 GetEventType() const { return 26; }

        // The frame delta companion GuiCache latches at its +0x00 (BrnGui::GuiCache::GetTimeStep,
        // and Intro::HandleStateTransitions @0x824DAA48's `lfs f13, 0(mpGuiCache)`).
        f32 GetTimeStep() const { return mfTimeDelta; }

        // The raw "now" stamp, without GetTime()'s -FLT_MAX assert (the cache latch copies
        // the pair verbatim; the assert belongs to the consumer that READS the stamp).
        f32 GetTimeNow() const { return mfTimeNow; }

        // The producer builds the 8-byte record on the stack word by word (X360
        // BridgeGameStateToGui @0x823EF300: `payload[0] = base*multiplier; payload[1] =
        // seconds + fraction`), then hands it to AddGuiEvent<T>. This is that fill.
        void Set(f32 lfTimeDelta, f32 lfTimeNow) { mfTimeDelta = lfTimeDelta; mfTimeNow = lfTimeNow; }

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

    // GuiEventNetworkLaunching -- the "network session is launching" GUI event. The only
    // CgsGui AddGuiEvent instance absent from any prior home: CgsGui::GuiModule::AddGuiEvent<
    // GuiEventNetworkLaunching> @0x823CFCB0 -> AddEvent(&event, 57, 8). The 8-byte record is
    // smaller than the 12-byte GuiEvent<N> header, so it is a raw payload carrying its own
    // GetEventType()==57 (honest placeholder -- the total size 8 + id 57 are the X360 facts;
    // the inner field breakdown is not recovered). A sibling of the other CgsGui network GUI
    // events (Connected 43 / Disconnected 44 / InGame 50 / Launched 58) which are OutputGuiEvent
    // instances homed by their own task.
    struct GuiEventNetworkLaunching
    {
        u8  maData[8];
        s32 GetEventType() const { return 57; }
    };

    // ============================================================================
    // OUTPUT-FAMILY CgsGui network/UI GUI-event PAYLOAD homes (ADDITIVE GROW).
    // The CgsGui network GUI events referenced by the note above (Connected 43 / Disconnected 44 /
    // InGame 50 / Launched 58) plus InGameFailed 51 / ShowLoginQuestion 47 / SetCircleButtonConfig 32,
    // homed here by the output GUI event wave. These are OutputGuiEvent / AddOutputGuiEvent /
    // AddGuiOutEvent payloads (plain-add: only GetEventType()==id and sizeof matter). Same honest
    // placeholder recipe: (id,size) are the two X360 AddEvent literals; size>=12&4-aligned derives
    // from GuiEvent<id>, else a raw byte struct carrying its own GetEventType()==id.
    // ============================================================================
    struct GuiEventNetworkConnected { u8 maData[2]; s32 GetEventType() const { return 43; } };  // id 43 size 2
    struct GuiEventNetworkDisconnected : public CgsGui::GuiEvent<44> { u8 maPayload[120]; };  // id 44 size 132
    struct GuiEventNetworkInGame { u8 maData[1]; s32 GetEventType() const { return 50; } };  // id 50 size 1
    struct GuiEventNetworkInGameFailed { u8 maData[4]; s32 GetEventType() const { return 51; } };  // id 51 size 4
    struct GuiEventNetworkLaunched { u8 maData[4]; s32 GetEventType() const { return 58; } };  // id 58 size 4
    struct GuiEventSetCircleButtonConfig { u8 maData[1]; s32 GetEventType() const { return 32; } };  // id 32 size 1
    struct GuiEventShowLoginQuestion { u8 maData[4]; s32 GetEventType() const { return 47; } };  // id 47 size 4

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
