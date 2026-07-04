#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO_DispatchOutputBuffer.h
//
// BrnWorldIO::DispatchOutputBuffer -- the World module's per-frame OUTPUT payload the graphics
// dispatch pass publishes: the resolved lighting/fog aggregates (quadric irradiance A/B matrices,
// fog colour+white-level / fog-scattering Vector4s, key-light direction/colour + average-irradiance
// Vector3s, and the white-level scalar). Derives the shared CgsModule::IOBuffer (status-flag-guarded
// read/write locking; bit 4 = read lock, bit 3 = write lock at status byte @+0). Member ORDER +
// member TYPES + offsets are the DecFIGS DWARF for GameSource/World/BrnWorldModuleIO.h
// (struct BrnWorldIO::DispatchOutputBuffer : public IOBuffer, DWARF :480..:487).
//
// [x64 WIDENING RULE: every member here is a 16-byte-aligned rw::math::vpu vector/matrix POD (no
// pointers), so the X360 32-bit offsets below DO carry over -- but the leading IOBuffer base pads
// identically on both targets. Members are accessed BY NAME (return-by-value copies); the X360
// offsets are documented for reference. No offsetof _AssertLayout() is required (all members are
// pointer-free 16-aligned vectors; parity is BY NAMED MEMBER).]
//
// X360 (32-bit) byte offsets pinned by the accessor asm:
//   +0x000  IOBuffer status byte (base)
//   +0x010  mmQuadricIrradianceA         rw::math::vpu::Matrix44 (64B)   DWARF :480
//   +0x050  mmQuadricIrradianceB         rw::math::vpu::Matrix44 (64B)   DWARF :481
//   +0x090  mvFogColourPlusWhiteLevel    rw::math::vpu::Vector4          DWARF :482
//   +0x0A0  mvFogScattering              rw::math::vpu::Vector4          DWARF :483
//   +0x0B0  mvKeyLightDirection          rw::math::vpu::Vector3          DWARF :484
//   +0x0C0  mvKeyLightColour             rw::math::vpu::Vector3          DWARF :485
//   +0x0D0  mvAverageIrradianceColour    rw::math::vpu::Vector3          DWARF :486
//   +0x0E0  mfWhiteLevel                 float32_t                       DWARF :487
// ============================================================================

#include "types.hpp"                                            // f32
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"          // CgsModule::IOBuffer base
#include "rw/math/vpu/types.h"                                  // rw::math::vpu::Vector3 / Vector4 / Matrix44
#include "BrnCommonTypes.h"                                     // bare Vector3 / Vector4 / Matrix44 typedefs (return-by-value)

namespace BrnWorldIO
{
    // DWARF BrnWorldModuleIO.h (struct DispatchOutputBuffer : public IOBuffer).
    struct DispatchOutputBuffer : public CgsModule::IOBuffer
    {
        // --- accessors bodied in BrnWorldModuleIO_DispatchOutputBuffer.cpp --------------
        // Getters read-lock (status bit 4); setters write-lock (status bit 3, "Not locked for
        // writing\n") -- reproduced exactly as the X360 asm tests them. NOTE: the getter rodata
        // carries NO trailing newline ("Not locked for reading"), the setter rodata DOES ("Not
        // locked for writing\n") -- both matched verbatim per the verifier's rodata notes.

        // ---- quadric irradiance matrices (Matrix44 by value) ----
        Matrix44 GetQuadricIrradianceA() const;                                                 // 0x827BC190 (:462 R)
        void     SetQuadricIrradianceA(rw::math::vpu::Matrix44 lmQuadricIrradianceA);            // 0x827BC268 (:463 W)
        Matrix44 GetQuadricIrradianceB() const;                                                 // 0x827BC340 (:464 R)
        void     SetQuadricIrradianceB(rw::math::vpu::Matrix44 lmQuadricIrradianceB);            // 0x827BC418 (:465 W)

        // ---- fog (Vector4 by value) ----
        void     SetFogColourPlusWhiteLevel(rw::math::vpu::Vector4 lvFogColourPlusWhiteLevel);   // 0x827BC5A8 (W)
        Vector4  GetFogScattering() const;                                                      // 0x827BC668 (:468 R)
        void     SetFogScattering(rw::math::vpu::Vector4 lvFogScattering);                       // 0x827BC720 (W)

        // ---- key light + average irradiance (Vector3 by value) ----
        Vector3  GetKeyLightDirection() const;                                                  // 0x823B54B0 (:470 R)
        void     SetKeyLightDirection(rw::math::vpu::Vector3 lvKeyLightDirection);               // 0x827BC7E0 (W)
        Vector3  GetKeyLightColour() const;                                                     // 0x823B5568 (:472 R)
        void     SetKeyLightColour(rw::math::vpu::Vector3 lvKeyLightColour);                     // 0x827BC8A0 (W)
        Vector3  GetAverageIrradianceColour() const;                                            // 0x823B5620 (:474 R)
        void     SetAverageIrradianceColour(rw::math::vpu::Vector3 lvAverageIrradianceColour);   // 0x827BC960 (W)

        // ---- white level (scalar) ----
        f32      GetWhiteLevel() const;                                                         // 0x823B56D8 (R)
        void     SetWhiteLevel(f32 lfWhiteLevel);                                               // 0x827BCA20 (:477 W)

    private:
        // --- Layout (DWARF :480..:487 member order; offsets X360-pinned in the banner) --
        rw::math::vpu::Matrix44 mmQuadricIrradianceA;       // :480  (X360 +0x10, 64B)
        rw::math::vpu::Matrix44 mmQuadricIrradianceB;       // :481  (X360 +0x50, 64B)
        rw::math::vpu::Vector4  mvFogColourPlusWhiteLevel;  // :482  (X360 +0x90)
        rw::math::vpu::Vector4  mvFogScattering;            // :483  (X360 +0xA0)
        rw::math::vpu::Vector3  mvKeyLightDirection;        // :484  (X360 +0xB0)
        rw::math::vpu::Vector3  mvKeyLightColour;           // :485  (X360 +0xC0)
        rw::math::vpu::Vector3  mvAverageIrradianceColour;  // :486  (X360 +0xD0)
        f32                     mfWhiteLevel;               // :487  (X360 +0xE0)
    };
}
