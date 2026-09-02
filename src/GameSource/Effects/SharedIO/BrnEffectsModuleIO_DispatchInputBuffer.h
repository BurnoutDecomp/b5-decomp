#pragma once

// ============================================================================
// b5-decomp/src/GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.h
//
// BrnEffects::EffectsIO::DispatchInputBuffer -- the per-frame input payload the effects
// dispatch pass reads while building the graphics DispatchFrame. Derives the shared
// CgsModule::IOBuffer (status-flag-guarded read/write locking; bit 4 = read lock,
// bit 3 = write lock at status byte @+0). Member ORDER + member TYPES are the DecFIGS
// DWARF for GameSource/Effects/EffectsModuleIO.h
// (struct BrnEffects::EffectsIO::DispatchInputBuffer : public IOBuffer, DWARF :196..:263);
// the accessor byte offsets are X360-pinned.
//
// Reconstructed with NAMED members accessed BY NAME (the committed sibling pattern in
// RendererIO::OutputBuffer, GameSource/Graphics/BrnRendererModuleIO.h/.cpp), NOT via a
// byte-image blob. [x64: pointers widen to 8 bytes so the interior byte offsets differ
// from the X360 32-bit layout below -- parity is BY NAMED MEMBER, per the x64-gate rule.]
//
// X360 (32-bit) byte offsets pinned by the accessor asm:
//   +0x000  IOBuffer status byte (base)
//   +0x004  mpBaseEffectsFrame          BrnEffectsFrame*        lwz/stw 4(r28)     DWARF :254
//   +0x008  mapFXEventsEffectsFrames[2] BrnEffectsFrame*[2]     stwx (slot+2)*4    DWARF :255
//   +0x010  mpDispatchFrame             CgsGraphics::DispatchFrame*                DWARF :257
//   +0x020  mKeyLightDirection          Vector3 (16B/16-aligned)                   DWARF :258
//   +0x030  mKeyLightColour             Vector3                                    DWARF :259
//   +0x040  mAverageIrradianceColour    Vector3                                    DWARF :260
//   +0x050  mCameraInput                BrnDirector::Camera::Camera (0x160)  addi 0x50  DWARF :261
//   +0x1B0  mpEnvironmentMap            const renderengine::Texture*  lwz/stw 0x1B0  DWARF :262
//   +0x1B4  mfWhiteLevel                float32_t                     lfs/stfs 0x1B4 DWARF :263
//
// (mCameraInput spans 0x50..0x1B0 = 0x160 = 352 bytes -- Camera.h ends at +0x15D rounded to
// 0x160 under alignas(16); the following member mpEnvironmentMap landing at 0x1B0 confirms it.
// The 12B between mpDispatchFrame(+0x10) and mKeyLightDirection(+0x20) is Vector3 16-byte
// alignment padding.) DispatchInputBuffer::Construct (EffectsModuleIO.cpp:83) calls
// Vector3::SetZero x3 -- confirming the three light Vector3 members.
//
// GROW this header additively (Construct/Destruct, SetDispatchFrame/GetDispatchFrame, the
// Vector3 light accessors, the non-const GetCameraInput @ DWARF :243, GetFXEventsEffectsFrame)
// when those X360-ledger TUs land; do NOT fork the type.
// ============================================================================

#include "types.hpp"                                          // u8, f32
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"        // CgsModule::IOBuffer base
#include "rw/math/vpu/types.h"                                // rw::math::vpu::Vector3
#include "GameSource/Director/Camera/Camera.h"                // BrnDirector::Camera::Camera (embedded by value)

// Forward decls -- these members are used by pointer only.
class BrnEffectsFrame;
namespace CgsGraphics { class DispatchFrame; }
namespace renderengine { class Texture; }

namespace BrnEffects
{
namespace EffectsIO
{
    // EffectsModuleIO.h:196 -- struct DispatchInputBuffer : public IOBuffer.
    struct DispatchInputBuffer : public CgsModule::IOBuffer
    {
        // --- construction bodied in BrnEffectsModuleIO_DispatchInputBuffer.cpp ----------
        void              Construct();                                                          // @ 0x82288120 (DWARF EffectsModuleIO.h:201)

        // --- accessors bodied in BrnEffectsModuleIO_DispatchInputBuffer.cpp -------------
        // Getters read-lock (status bit 4, "Not locked for reading\n"); setters write-lock
        // (status bit 3, "Not locked for writing\n") -- reproduced exactly as the X360 asm
        // tests them. (The X360 rodata carries the trailing \n on both lock strings.)
        void              SetBaseEffectsFrame(BrnEffectsFrame* lpFrame);                        // @ 0x823BADE8
        BrnEffectsFrame*  GetBaseEffectsFrame() const;                                          // @ 0x8227E328

        void              SetFXEventsEffectsFrame(u8 luSlot, BrnEffectsFrame* lpFrame);         // @ 0x823BAE90 (slot<2)

        void              SetCameraInput(const BrnDirector::Camera::Camera* lpCamera);          // @ 0x823C9988
        const BrnDirector::Camera::Camera* GetCameraInput() const;                             // @ 0x8227DE80

        void              SetEnvironmentMap(const renderengine::Texture* lpTexture);            // @ 0x823BAA98
        const renderengine::Texture* GetEnvironmentMap() const;                                // @ 0x8227DF28

        void              SetWhiteLevel(f32 lfWhiteLevel);                                      // @ 0x823BAB40
        f32               GetWhiteLevel() const;                                                // @ 0x8227DFD0

        // The four fields EffectsModule::GenerateDispatchLists @0x82296668 copies into the
        // particle dispatch input. The X360 does NOT call accessors there -- it loads them
        // inline (`*(dst+4) = *(src+0x10)` then three lvx128/stvx128 pairs off src+0x20 /
        // +0x30 / +0x40), i.e. these are the compiler-inlined reads of the same members the
        // Set* twins above write under the write lock. Exposed by name (additive, 2026-09-02)
        // so the producer stays off the raw offsets; no lock test, matching the asm.
        CgsGraphics::DispatchFrame*    GetDispatchFrame() const          { return mpDispatchFrame; }
        const rw::math::vpu::Vector3&  GetKeyLightDirection() const      { return mKeyLightDirection; }
        const rw::math::vpu::Vector3&  GetKeyLightColour() const         { return mKeyLightColour; }
        const rw::math::vpu::Vector3&  GetAverageIrradianceColour() const{ return mAverageIrradianceColour; }

        // NOTE: no offsetof _AssertLayout() -- the X360 byte offsets (mpBaseEffectsFrame @+0x4,
        // mCameraInput @+0x50, mpEnvironmentMap @+0x1B0, mfWhiteLevel @+0x1B4) are 32-bit-specific
        // (pointers widen on the x64 gate), so they cannot be pinned by static_assert. Parity is
        // BY NAMED MEMBER per the x64-gate rule; the named DWARF member order is authoritative.

    private:
        // --- Layout (DWARF member order; offsets X360-pinned; see header banner) -------
        BrnEffectsFrame*             mpBaseEffectsFrame;          // DWARF :254  (X360 +0x04)
        BrnEffectsFrame*             mapFXEventsEffectsFrames[2]; // DWARF :255  (X360 +0x08)
        CgsGraphics::DispatchFrame*  mpDispatchFrame;             // DWARF :257  (X360 +0x10)
        rw::math::vpu::Vector3       mKeyLightDirection;          // DWARF :258  (X360 +0x20)
        rw::math::vpu::Vector3       mKeyLightColour;             // DWARF :259  (X360 +0x30)
        rw::math::vpu::Vector3       mAverageIrradianceColour;    // DWARF :260  (X360 +0x40)
        BrnDirector::Camera::Camera  mCameraInput;                // DWARF :261  (X360 +0x50, 0x160)
        const renderengine::Texture* mpEnvironmentMap;            // DWARF :262  (X360 +0x1B0)
        f32                          mfWhiteLevel;                // DWARF :263  (X360 +0x1B4)
    };
}
}
