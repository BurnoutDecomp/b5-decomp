#pragma once
// RenderWare 4 packed colour, rw::RGBA, in its own header: DecFIGS DWARF declares it in
// SDKs/EATech/include/native/<platform>/rw/core/base/detail/hosttype.h:129 (members :133-:252), and
// the LegacySrc reference carries the class in the same file. It lived in rwcore_structs.h until the
// debug renderers' colour parameters were retyped to it; a header of its own keeps that change from
// pulling rwcore_structs.h (and through it <xtl.h>) into every debug-render includer.
#include "types.hpp"

namespace rw {

struct RGBA {  // sizeof = 4
    // DWARF hosttype.h:133; the reference header's body is empty (`RGBA() { ; return; }`), and ARTIST
    // never zeroes a default-constructed colour (ReplayDirector::ReplayDirector 0x827E2020 builds its
    // DebugLog's 20 LogString slots, DWARF mRGBA, without a store).
    RGBA() {}
    struct NoInitTag {};
    explicit RGBA(NoInitTag) {}
    // ARTIST821F05B0: native ABGR input word. Inline placement preserves static colors.
    RGBA(uint8_t lu8Red, uint8_t lu8Green, uint8_t lu8Blue, uint8_t lu8Alpha)
        : m_rgba((static_cast<uint32_t>(lu8Alpha) << 24) |
                 (static_cast<uint32_t>(lu8Blue) << 16) |
                 (static_cast<uint32_t>(lu8Green) << 8) | static_cast<uint32_t>(lu8Red)) {}

    // DWARF rw/core/base/detail/hosttype.h:188/:204/:220/:236 (inline). The Xenon word is the native
    // ABGR order the constructor above packs: red in the low byte (AIDebugComponent::StateTableDrawEntry
    // 0x827673B4..0x827673D8 extracts each channel as a u8 and divides the promoted int: divw).
    uint8_t GetRed() const   { return static_cast<uint8_t>(m_rgba); }
    uint8_t GetGreen() const { return static_cast<uint8_t>(m_rgba >> 8); }
    uint8_t GetBlue() const  { return static_cast<uint8_t>(m_rgba >> 16); }
    uint8_t GetAlpha() const { return static_cast<uint8_t>(m_rgba >> 24); }

    // DWARF hosttype.h:195/:211/:227/:243 (inline); bodies as the reference header writes them
    // (clear the channel's byte, then or the value in), on the Xenon byte of each channel. ARTIST
    // Window::ScaleColour 0x82816134 (rlwimi into the top byte = SetAlpha) and the ErrorWindow::Render
    // repack 0x8282F028..0x8282F044 (and-not then or, red/green/blue in turn).
    void SetRed(uint8_t value)   { m_rgba &= ~(0xffu << 0);  m_rgba |= (static_cast<uint32_t>(value) << 0); }
    void SetGreen(uint8_t value) { m_rgba &= ~(0xffu << 8);  m_rgba |= (static_cast<uint32_t>(value) << 8); }
    void SetBlue(uint8_t value)  { m_rgba &= ~(0xffu << 16); m_rgba |= (static_cast<uint32_t>(value) << 16); }
    void SetAlpha(uint8_t value) { m_rgba &= ~(0xffu << 24); m_rgba |= (static_cast<uint32_t>(value) << 24); }

    // The reference header's member compare (hosttype.h, after SetAlpha).
    bool operator==(const RGBA& other) const { return (m_rgba == other.m_rgba); }

    uint32_t m_rgba;  // +0
};

}
