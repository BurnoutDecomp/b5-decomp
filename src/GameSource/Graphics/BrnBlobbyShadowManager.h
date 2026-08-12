#ifndef BRN_BLOBBY_SHADOW_MANAGER_H
#define BRN_BLOBBY_SHADOW_MANAGER_H

#include <cstddef>            // offsetof (layout pins in _AssertLayout below)

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Vector3Plus / Vector4 / VecFloat (rw::math::vpu)

// ---------------------------------------------------------------------------
// BrnBlobbyShadowManager
//
// DWARF home: GameSource/Graphics/BrnBlobbyShadowManager.h (DecFIGS dwarfdump for this TU's
// source path). Layout/member names below are taken from that DWARF; behaviour is verified
// against the X360 retail XEX pseudocode/asm.
//
// Double-buffered collector of per-frame "blobby" (soft, projected-quad) vehicle ground
// shadows. Game code fills the external buffer each frame via BrnBlobbyShadowBuffer::AddShadow,
// then the manager swaps buffers and renders the internal one. Only ONE function is attested
// in the X360 retail XEX ledger for this TU:
//   BrnBlobbyShadowManager::BrnBlobbyShadowBuffer::AddShadow @ 0x823F7638
// The other DWARF-listed methods (Construct / Swap / Render / GetExternalBuffer ...) are
// declared here from the DWARF as the owning surface but are out of scope (not in this TU's
// ledger) and are left unbodied.
//
// ShadowStruct layout (DWARF, corroborated by the AddShadow slot stride of 64 bytes -- the
// X360 addresses a record as `buffer + 64 * index`, i.e. slwi r10, count, 6):
//   +0x00  mvPos                          Vector3      (projected shadow centre)
//   +0x10  mvAt                           Vector3      (facing / projection direction)
//   +0x20  mvScaledRight_HeightOffGround  Vector3Plus  (xyz = scaled right vector, w = height off ground)
//   +0x30  mvFront_Rear_BackAxle_FrontAxle Vector4     (packed quad extents along the at axis)
//   sizeof == 0x40 (64 bytes)
//
// KI_MAX_SHADOWS == 64 (DWARF + the X360 bound `cmpwi count, 0x40` in AddShadow).
// ---------------------------------------------------------------------------

// Soft-shadow alpha and the per-frame shadow count cap, from the DWARF header.
//
// KF_BLOBBY_SHADOW_ALPHA -- RECOVERED 2026-08-12, value 0.7f, by two INDEPENDENT reads that agree:
//   1. X360 asm. BrnRendererModule::Construct seeds mfBlobbyShadowAlpha from it:
//      @0x8240BC8C `lfs f13, -0x2D04(r26)` / `stfs f13, 0x37F4(r31)` with r26 = 0x8203E414
//      (loaded @0x8240B480, spilled/reloaded via var_1F0), so the source is flt_8203B710 =
//      0x3F333333 = 0.7f. That the member at +0x37F4 IS the blobby-shadow alpha is confirmed by
//      BrnGraphics::DebugComponent::OnActivate @0x823F7DF0, which registers `this->0x37F4` as the
//      debug variable "Blobby Shadow Alpha" with SetRange(0.0f, 1.0f).
//   2. DecFIGS DWARF. The constant-value record spells it out directly:
//      `KF_BLOBBY_SHADOW_ALPHA = [63, 51, 51, 51]` == 0x3F333333 == 0.7f.
// NOTE for the BrnRendererModule owner: BrnRendererModule.h:602 still seeds
// `mfBlobbyShadowAlpha = 0.0f`; the X360 seeds it from this constant. Not fixed here (that file
// belongs to another agent this wave).
static const f32 KF_BLOBBY_SHADOW_ALPHA = 0.7f;  // DWARF :25 (name+value); X360 flt_8203B710
static const s32 KI_MAX_SHADOWS = 64;            // DWARF :49; AddShadow asm bound (cmpwi 0x40)

class BrnBlobbyShadowManager
{
public:
    class BrnBlobbyShadowBuffer
    {
    public:
        // One projected ground-shadow quad's parameters. 64 bytes; see header note above.
        struct ShadowStruct
        {
            Vector3     mvPos;
            Vector3     mvAt;
            Vector3Plus mvScaledRight_HeightOffGround;
            Vector4     mvFront_Rear_BackAxle_FrontAxle;
        };

        // AddShadow @ 0x823F7638
        // Appends one shadow to this buffer, returning false only when the buffer is full
        // (count >= KI_MAX_SHADOWS) and true otherwise. The X360 also early-outs (still
        // returning true, leaving the buffer unchanged) when the incoming transform fails a
        // VMX magnitude/visibility compare against a constant threshold (see .cpp note).
        //
        // The DWARF spells the source-level signature as
        //   AddShadow(const Matrix44Affine& lShadowTransform,
        //             const Vector4 lvFwdLength_RearLength_BackAxle_FrontAxle,
        //             VecFloat lfWidth, VecFloat lfHeightOffGround)
        // and that is reproduced here.
        bool AddShadow(const Matrix44Affine& lShadowTransform,
                       const Vector4& lvFwdLength_RearLength_BackAxle_FrontAxle,
                       VecFloat lfWidth,
                       VecFloat lfHeightOffGround);

        s32         miNumShadows;            // +0x00 : count read/written by AddShadow (lwz/stw 0(this))
        ShadowStruct maShadowPos[KI_MAX_SHADOWS];

        // Never called; pins the pointer-invariant facts AddShadow's addressing depends on.
        // The asm derives a record address as `this + 0x10 + 64*index` -- i.e. the count word
        // owns the whole first 16 bytes and the array starts at 0x10 (the stores are spelled
        // `stvx128 v, this + (count<<6), {0x10,0x20,0x30}` and `stvx128 v1, this, (count+1)<<6`).
        // Nothing here holds a pointer, so both facts transfer verbatim from the X360 to x64.
        static void _AssertLayout()
        {
            static_assert(offsetof(BrnBlobbyShadowBuffer, maShadowPos) == 0x10,
                          "maShadowPos @0x10 -- AddShadow addresses record i as this+0x10+64*i");
            static_assert(sizeof(BrnBlobbyShadowBuffer) == 0x1010,
                          "buffer stride 0x1010 == 0x10 header + 64 * 0x40 records");
            static_assert(sizeof(ShadowStruct) == 0x40,
                          "ShadowStruct == 64 bytes (slwi count, 6)");
        }
    };

    // ---- Remaining DWARF-declared surface (NOT in this TU's ledger; left unbodied) ----
    void Construct();
    void Destruct();
    bool Prepare();
    bool Release();
    void Swap();
    BrnBlobbyShadowBuffer* GetExternalBuffer();

    BrnBlobbyShadowBuffer maBuffers[2];
    u8                    mu8Internal;
    u8                    mu8External;
};

#endif  // BRN_BLOBBY_SHADOW_MANAGER_H
