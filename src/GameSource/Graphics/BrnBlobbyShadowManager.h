#ifndef BRN_BLOBBY_SHADOW_MANAGER_H
#define BRN_BLOBBY_SHADOW_MANAGER_H

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
static const f32 KF_BLOBBY_SHADOW_ALPHA = 0.0f;  // PLACEHOLDER value: const is named in the DWARF
                                                 // (BrnBlobbyShadowManager.h:25) but the X360 data
                                                 // blob holding it is not exported; only Render
                                                 // (out of scope) consumes it.
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
