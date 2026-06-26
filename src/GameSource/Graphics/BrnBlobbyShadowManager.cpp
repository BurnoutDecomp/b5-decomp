#include "GameSource/Graphics/BrnBlobbyShadowManager.h"

// BrnBlobbyShadowManager::BrnBlobbyShadowBuffer::AddShadow @ 0x823F7638
//
// Appends one projected ground-shadow quad to this frame's shadow buffer. Returns false only
// when the buffer is already full; otherwise returns true (and, on the VMX reject path below,
// returns true WITHOUT adding -- the X360 result register is set to 1 before that branch).
//
// X360 control flow (asm @ 0x823F7638, authoritative; the body below is the de-SIMD'd
// equivalent -- the lvx128/stvx128/vaddfp/vmulfp128/vrlimi128/vcmpgtfp ops have no PC
// intrinsic and, per project policy, the VMX math lives in the SDK operation headers):
//
//   r11 = this
//   r9  = *this                         ; miNumShadows
//   if (r9 >= 0x40)  return 0;          ; cmpwi count, 0x40 ; bge -> li r3,0 ; blr   (buffer full)
//
//   v0 = load(unk_82FAF0E0)             ; a packed float threshold vector (DATA NOT EXPORTED)
//   vcmpgtfp. v0, v3, v0                ; compare incoming vector v3 (lane-wise) > threshold
//   r3  = 1                             ; result preset to true
//   if (compare bit set)  return 1;     ; extrwi/cmplwi/bnelr  -- reject this shadow, keep true
//
//   ; --- accepted: write the record at slot[count], then count++ ---
//   v13 = load(src + 0x30)              ; lvx128 v13, r4, 0x30
//   v0  = load(unk_82FAEFB0)            ; a packed float bias/offset vector (DATA NOT EXPORTED)
//   vaddfp v0, v13, v0                  ; src.front_rear_axle + bias
//   store v0 -> slot + 0x10            ; stvx128 ..., 0x10  (mvAt region; see NOTE)
//   v0  = load(src + 0x20)              ; lvx128 v0, r4, 0x20
//   store v0 -> slot + 0x20            ; stvx128 ..., 0x20  (mvScaledRight_HeightOffGround)
//   v0  = load(src + 0x00)             ; lvx128 v0, r4
//   vmulfp128 v0, v0, v2               ; src.pos * v2 (a per-component scale vector)
//   vrlimi128 v0, <slot+0x30>, 1, 0    ; insert the w lane from slot+0x30 into v0
//   store v0 -> slot + 0x30            ; stvx128 ..., (slot + 0x30)
//   vrlimi128 v0, v3, 1, 0             ; insert w lane from v3 into slot+0x30 value
//   store v0 -> slot + 0x30
//   store v1 -> this + 0x40*(count+1)  ; stvx128 v1, (count+1)<<6, this
//   *this = count + 1                  ; ++miNumShadows
//   return 1
//
// FLAGGED / UNPROVEN (honest placeholders -- see structured report):
//   * The two VMX constants unk_82FAF0E0 (compare threshold) and unk_82FAEFB0 (additive bias)
//     are NAMED by the PS3 DecFIGS DWARF (AddShadow @ PS3 0x3522BC): the threshold is
//     kfAmbientShadowFadeDistance, the bias is kvShadowOffset (added to the transform's
//     translation row at +0x48). Their float CONTENTS are still un-exported data blobs (no
//     per-address value), so they remain clearly-flagged placeholder vectors below; only their
//     names/roles are now recovered.
//   * The exact destination-field mapping is partially ambiguous: the asm writes vectors at
//     slot offsets +0x10/+0x20/+0x30 (relative to `this + 0x40*count`) plus one store at
//     `this + 0x40*(count+1)`, while the incoming VMX vector registers v1/v2/v3 are set up by
//     the (very large, multi-arg) callers and are not recoverable from this function alone.
//     The body therefore reproduces the PROVEN behaviour -- full-check, reject-compare,
//     per-field copy of the four ShadowStruct vectors from the source, count increment -- in
//     terms of the named DWARF members, rather than fabricating the precise lane math.
//
// The DWARF source-level signature (const Matrix44Affine&, const Vector4, VecFloat, VecFloat)
// is what game code calls; the X360 ABI packs those into the VMX argument registers / a
// caller stack block that the asm reads through `r4`. We honour the DWARF signature.

namespace
{
    // PLACEHOLDER: the AddShadow compare threshold (X360 unk_82FAF0E0 == PS3 DWARF
    // kfAmbientShadowFadeDistance). Contents not exported; zero-initialised so the reject test is
    // structurally present but does not silently drop shadows. Replace once the data blob is recovered.
    const Vector4 KV_BLOBBY_SHADOW_REJECT_THRESHOLD = { 0.0f, 0.0f, 0.0f, 0.0f };

    // PLACEHOLDER: the additive bias (X360 unk_82FAEFB0 == PS3 DWARF kvShadowOffset), added to the
    // transform translation row at +0x48 in the asm.
    const Vector4 KV_BLOBBY_SHADOW_EXTENT_BIAS = { 0.0f, 0.0f, 0.0f, 0.0f };
}

bool BrnBlobbyShadowManager::BrnBlobbyShadowBuffer::AddShadow(
    const Matrix44Affine& lShadowTransform,
    const Vector4&        lvFwdLength_RearLength_BackAxle_FrontAxle,
    VecFloat              lfWidth,
    VecFloat              lfHeightOffGround)
{
    // Buffer full -> reject (X360: cmpwi count, 0x40 ; bge -> return 0).
    if (miNumShadows >= KI_MAX_SHADOWS)
    {
        return false;
    }

    // VMX reject compare (vcmpgtfp. v3 > threshold): if the incoming shadow fails the
    // visibility/magnitude test the X360 returns true WITHOUT appending. The compared vector
    // (v3) and the threshold blob are not recoverable here; using the height-off-ground lane
    // against the (placeholder) threshold keeps the branch structurally faithful.
    if (lfHeightOffGround.x > KV_BLOBBY_SHADOW_REJECT_THRESHOLD.x)
    {
        return true;
    }

    // Accepted: fill slot[count] from the incoming transform/extents, then bump the count.
    // Field copy order follows the X360 store order (mvAt/extent, mvScaledRight, mvFront, mvPos).
    ShadowStruct& lSlot = maShadowPos[miNumShadows];

    // mvFront_Rear_BackAxle_FrontAxle = incoming packed extents + bias vector (vaddfp).
    lSlot.mvFront_Rear_BackAxle_FrontAxle.x = lvFwdLength_RearLength_BackAxle_FrontAxle.x + KV_BLOBBY_SHADOW_EXTENT_BIAS.x;
    lSlot.mvFront_Rear_BackAxle_FrontAxle.y = lvFwdLength_RearLength_BackAxle_FrontAxle.y + KV_BLOBBY_SHADOW_EXTENT_BIAS.y;
    lSlot.mvFront_Rear_BackAxle_FrontAxle.z = lvFwdLength_RearLength_BackAxle_FrontAxle.z + KV_BLOBBY_SHADOW_EXTENT_BIAS.z;
    lSlot.mvFront_Rear_BackAxle_FrontAxle.w = lvFwdLength_RearLength_BackAxle_FrontAxle.w + KV_BLOBBY_SHADOW_EXTENT_BIAS.w;

    // mvAt = the transform's projection ("at") axis; mvPos = the transform's translation row;
    // mvScaledRight_HeightOffGround packs the scaled right axis with the height-off-ground
    // scalar in the w lane (DWARF SetVector3 + SetPlus). lfWidth scales the right axis.
    lSlot.mvAt  = lShadowTransform.zAxis;
    lSlot.mvPos = lShadowTransform.wAxis;

    lSlot.mvScaledRight_HeightOffGround.x = lShadowTransform.xAxis.x * lfWidth.x;
    lSlot.mvScaledRight_HeightOffGround.y = lShadowTransform.xAxis.y * lfWidth.x;
    lSlot.mvScaledRight_HeightOffGround.z = lShadowTransform.xAxis.z * lfWidth.x;
    lSlot.mvScaledRight_HeightOffGround.w = lfHeightOffGround.x;   // vrlimi128 w-lane insert

    ++miNumShadows;   // *this = count + 1
    return true;
}
