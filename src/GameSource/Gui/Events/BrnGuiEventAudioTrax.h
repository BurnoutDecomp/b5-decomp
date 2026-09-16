#pragma once

// BrnGuiEventAudioTrax.h
// Home of the two EA-Trax GUI payload types: BrnGui::GuiEventAudioTraxUpdate (the pair of
// per-track bit fields, GUI event 458) and BrnGui::GuiEventAudioTraxPlayOrder (the
// play-order selector, GUI event 462).
//
// ⭐⭐ WHY THIS FILE EXISTS (2026-09-16). GuiEventAudioTraxUpdate had TWO definitions:
//   * BrnGuiOptionsDataProfile.h -- a typedef-only holder, there because the profile stores
//     three of these bit fields and needed the nested EATraxArrayType name; and
//   * BrnGuiDemangledEventTypes.h -- an opaque `GuiEvent<458> { u8 maPayload[20]; }` mirror,
//     there because the two template-instantiation TUs that need the type as a COMPLETE
//     type could not include the profile header.
// The pair is a hard C2011 in any TU that sees both, which is what the EA Trax wave hit the
// moment CN_TRAX's header needed the profile types. Splitting the type into its own leaf
// header is the fix the tree already uses for exactly this (GuiAutosaveRequestEvent was
// moved out of the demangled header for the same reason, and BrnGuiEventStatsResponse.h is
// the same shape of home): ONE definition, includable from everywhere.
//
// ⭐ AND THE MIRROR'S SHAPE WAS WRONG. It read "id 458 size 32" as a 12-byte GuiEvent header
// plus a 20-byte payload. The X360 settles it:
// CgsGui::StateInterface::OutputGuiEvent<BrnGui::GuiEventAudioTraxUpdate> @0x824C30A0 copies
// FOUR 8-byte words straight from the event object's offset 0 (`ld 0/8/0x10/0x18` ->
// `std 0/8/0x10/0x18`), writes the wrapper header { 32, 0x1CA, 16 } and calls AddEvent with
// 0x30 == 48 bytes. So the object is 32 bytes of PAYLOAD with no GuiEvent header at all, the
// wrapper's size word is those 32 bytes, and the 8-byte alignment of the bit fields is what
// puts the payload at wrapper offset 16 rather than 12.
// CrashNavTrax::HandleTraxEnabledStateChange @0x824CF898 is that same body inlined, and it
// sources the two words from the EA Trax menu component's FreeBurn (+0xA0) and Events
// (+0xB0) bit fields -- in that order.

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"   // FastBitArray<128>

namespace BrnGui
{
    // GUI event 458. 8-byte aligned, 32 bytes, no GuiEvent header (see the banner).
    struct GuiEventAudioTraxUpdate
    {
        // A 128-bit fixed bit set, one bit per track: the X360 stores/loads it as two
        // adjacent u64 fields.
        typedef CgsContainers::FastBitArray<128> EATraxArrayType;

        EATraxArrayType mEATraxEnabledInFreeBurn;   // +0x00
        EATraxArrayType mEATraxEnabledInEvents;     // +0x10

        s32 GetEventType() const { return 458; }
    };

    static_assert(sizeof(GuiEventAudioTraxUpdate) == 32,
                  "OutputGuiEvent<GuiEventAudioTraxUpdate> @0x824C30A0 copies 32 bytes from "
                  "offset 0 and posts a 48-byte record");

    // GUI event 462 -- the trax play-order selector (the profile stores it as a 4-byte word
    // at +0x7340).
    //
    // The two enumerators are attested twice over:
    //   * EATraxMenuComponent::UpdatePlayOrderMode @0x824197F0 accepts exactly 0 and 1
    //     (`cmpwi 1` / `cmpwi 0`, everything else asserts) and its assert text spells both
    //     names -- "liPlayOrderMode == ...E_TRAX_PLAY_ORDER_MODE_RANDOM || liPlayOrderMode
    //     == ...E_TRAX_PLAY_ORDER_MODE_SEQUENTIAL".
    //   * The same function indexes .rdata off_82F25328 with the value, and that table reads
    //     { "$EATRAX_PLAY_ORDER_SEQUENTIAL", "$EATRAX_PLAY_ORDER_RANDOM" } -- so SEQUENTIAL
    //     is 0 and RANDOM is 1.
    // The DEFAULT enumerator is what OptionsDataProfile::Construct resets to and is the same
    // value as SEQUENTIAL, which is what a fresh profile plays.
    // GUI event 502 -- the new-track record that drives the in-game EATrax chyron.
    //
    // ⭐ THE DWARF'S OWN NAME. references/DecFIGS/.../BrnGuiEventTypeDefs.h:11734 declares
    // `GuiEATraxNewTrackEvent : GuiEvent<492>`; 492 is the PS3 id and the X360 ships 502.
    // That +10 drift is already established in this tree -- BrnTrafficEntityModule.cpp:5115
    // records the same pair for GuiEventTrafficPoolEmptied (PS3 502 -> X360 512).
    //
    // Posted by MusicEffect's preview/track-change path as a 24-byte record, and consumed by
    // AlwaysAvailableComponentsManager::Update case 502, which reads miSongIndex at +0x10 and
    // mbPreview at +0x14 (X360 @0x8250990C `lwz r4, 0x10(r25)` / @0x82509958
    // `lbz r11, 0x14(r25)`). Given a home here -- rather than left as the local struct the
    // producer declared inline -- because BOTH ends now need it, and this file exists to stop
    // exactly that kind of EA Trax payload from being defined twice (see the banner).
    struct GuiEATraxNewTrackEvent
    {
        // The playlist's remaining-songs mask; the X360 copies it from the effect's
        // mEaTraxData at +0x1EC/+0x1F4 before stamping the two scalars.
        CgsContainers::FastBitArray<128> mRemainingSongs;   // +0x00
        s32 miSongIndex;                                    // +0x10

        // ⛔ NOT "is playing" -- the DWARF calls this mbPreview, and the consumer proves the
        // DWARF right: @0x82509958..0x82509960 SKIPS the profile persist when this byte is
        // non-zero, i.e. an AUDITION must not overwrite the player's last-played track. The
        // producer only posts from the preview path, so it always sets it.
        u8  mbPreview;                                      // +0x14

        s32 GetEventType() const { return 502; }
    };

    static_assert(sizeof(GuiEATraxNewTrackEvent) == 24,
                  "MusicEffect posts this record as AddEvent(&r, 502, 24)");

    // GUI event 462 -- the trax play-order selector (the profile stores it as a 4-byte word
    // at +0x7340).
    struct GuiEventAudioTraxPlayOrder
    {
        enum ETraxPlayOrderMode
        {
            E_TRAX_PLAY_ORDER_MODE_SEQUENTIAL = 0,
            E_TRAX_PLAY_ORDER_MODE_RANDOM     = 1,
            E_TRAX_PLAY_ORDER_MODE_COUNT      = 2,
            E_TRAX_PLAY_ORDER_MODE_DEFAULT    = E_TRAX_PLAY_ORDER_MODE_SEQUENTIAL,
        };

        s32 miPlayOrderMode;   // +0x00

        s32 GetEventType() const { return 462; }
    };
}
