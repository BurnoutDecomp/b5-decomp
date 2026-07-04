// ============================================================================
// b5-decomp/src/GameSource/Network/SharedIO/BrnNetworkOutScoreboardHeadingList.h
// ============================================================================
// BrnNetwork::BrnNetworkModuleIO::NetworkOutScoreboardHeadingList -- the network-out event the
// host builds to broadcast the scoreboard column headings (category / index / variation names).
// BrnNetwork::ScoreboardManager::CopyCategories / CopyIndexes / CopyVariations
// (X360 0x82562590 / 0x82562638 / 0x825626D8) stack-allocate one, AddHeading() each name, then
// AddEvent() it onto the network output VariableEventQueue<14000,16> as event-type 0x34, size
// 0x808 (2056 bytes).
//
// LAYOUT (X360-AUTHORITATIVE; offsets from `this`, recovered from AddHeading @0x82541EB0 and the
// CopyCategories frame @0x82562590):
//   +0x00  miLength      s32  -- live heading count. AddHeading reads it (lwz 0(r30)), asserts
//                               0 <= miLength < 66, post-increments it (stw len+1, 0(r30)).
//   +0x04  meHeadingType s32  -- which heading list this is (category / index / variation). The
//                               ScoreboardDebugComponent::HandleScoreboardHeadingEvent consumer
//                               @0x82585A88 loads this word (lwz 4(r17)), asserts it != E_HEADING_COUNT
//                               (3), and switches on 0/1/2 to route into the category/index/variation
//                               column lists. (Previously labelled "miReserved" when only the AddHeading
//                               producer was reconstructed -- the consumer pins it as the heading type.)
//   +0x08  maHeadings[66][31] -- the heading slots. AddHeading writes slot
//                             &maHeadings[miLength] via strncpy(base + 8 + miLength*31, name, 31)
//                             (the asm computes 8 + (len<<5 - len) = 8 + len*31; stride 31).
//   +0x806 .. +0x808  trailing pad to the 0x808 (2056) AddEvent byte size.
//
// CONSTANTS (X360 compare immediates in AddHeading):
//   KI_MAX_CAT_VAR_INDEX_NAME_LENGTH = 31  (0x1F; strlen guard + per-slot stride / strncpy count)
//   KI_MAX_CAT_VAR_INDEX_COUNT       = 66  (0x42; miLength upper bound)
//
// No source / DWARF for this TU; shape is asm-only. The only function in scope is AddHeading
// (this TU); fields beyond the asm-evident ones are offset-keyed.
#pragma once

#include "types.hpp"

namespace BrnNetwork
{
namespace BrnNetworkModuleIO
{
    // X360 AddHeading compare immediates (CgsNetwork:: scoreboard limits).
    static const s32 KI_MAX_CAT_VAR_INDEX_NAME_LENGTH = 31; // 0x1F
    static const s32 KI_MAX_CAT_VAR_INDEX_COUNT       = 66; // 0x42

    // Which scoreboard heading list this event carries. The consumer
    // (ScoreboardDebugComponent::HandleScoreboardHeadingEvent @0x82585A88) switches on these and
    // asserts the value is never E_HEADING_COUNT; the column/index/variation ordering matches the
    // switch arms (0 -> category, 1 -> index, 2 -> variation).
    enum EHeadingType
    {
        E_HEADING_CATEGORY  = 0,
        E_HEADING_INDEX     = 1,
        E_HEADING_VARIATION = 2,
        E_HEADING_COUNT     = 3,   // sentinel; asserted-against in HandleScoreboardHeadingEvent
    };

    struct NetworkOutScoreboardHeadingList
    {
        s32  miLength;      // +0x00  live heading count (0 .. 66)
        s32  meHeadingType; // +0x04  EHeadingType -- category / index / variation list discriminator
        char maHeadings[KI_MAX_CAT_VAR_INDEX_COUNT][KI_MAX_CAT_VAR_INDEX_NAME_LENGTH]; // +0x08  66 x 31
        u8   maReservedPadTo0x808[0x808 - (0x08 + KI_MAX_CAT_VAR_INDEX_COUNT * KI_MAX_CAT_VAR_INDEX_NAME_LENGTH)]; // +0x806 .. +0x808

        // @ 0x82541EB0 -- append one heading string. Asserts strlen(name) < 31, 0 <= miLength,
        // miLength < 66; then strncpy's the name into maHeadings[miLength] and increments miLength.
        void AddHeading(const char* lpcHeading);

        // @ 0x823A62F8 -- read the heading-list discriminator (meHeadingType @ +4). Asserts the
        // type is not the E_HEADING_COUNT sentinel; the consumer (HandleScoreboardHeadingEvent)
        // switches on category / index / variation.
        EHeadingType GetHeadingType() const;
    };
    static_assert(sizeof(NetworkOutScoreboardHeadingList) == 0x808, "NetworkOutScoreboardHeadingList size (event-type 0x34, 2056 bytes)");
}
}
