#include "GameSource/Graphics/BrnBlobbyShadowManager.h"

// BrnBlobbyShadowManager::BrnBlobbyShadowBuffer::AddShadow @ 0x823F7638
//
// Appends one projected ground-shadow quad to this frame's shadow buffer. Returns false only
// when the buffer is already full; otherwise returns true -- including on the fade-distance
// reject path, where the X360 sets r3 = 1 BEFORE the early `bnelr` and adds nothing.
//
// X360 control flow (asm @ 0x823F7638, authoritative; the body below is the de-SIMD'd
// equivalent -- the lvx128/stvx128/vaddfp/vmulfp128/vrlimi128/vcmpgtfp ops have no PC
// intrinsic and, per project policy, the VMX math lives in the SDK operation headers).
//
// ARGUMENT -> REGISTER MAPPING (confirmed at BOTH call sites, 2026-08-12):
//   r3 = this
//   r4 = const Matrix44Affine&   -- the caller builds it in four consecutive 16-byte stack
//                                   slots and passes the base; +0x00 xAxis, +0x10 yAxis,
//                                   +0x20 zAxis, +0x30 wAxis.
//                                   BrnTraffic::TrafficEntityModule::RenderTrafficCar fills
//                                   var_4D0/4C0/4B0/4A0 (x/y/z/w) and loads r4 = &var_4D0
//                                   @0x827294D4; BrnWorld::RaceCarEntityModule::
//                                   GenerateDispatchLists fills var_1E0/1D0/1C0/1B0 and loads
//                                   r4 = &var_1E0 @0x822E8024.
//   v1 = const Vector4  lvFwdLength_RearLength_BackAxle_FrontAxle  (`vmr128 v1, vNNN` at both
//                                   call sites: 0x8272954C and 0x822E8020)
//   v2 = VecFloat       lfWidth              (`vspltw v2, v0, 0` @0x82729550)
//   v3 = VecFloat       lfHeightOffGround    (`vspltw v3, v12, 3` @0x827294E8; the same value
//                                   the caller already used to drop the matrix translation
//                                   onto the ground -- wAxis = pos - up * height)
//
//   r11 = this ; r9 = *this                  ; miNumShadows
//   if (r9 >= 0x40)  return 0;               ; cmpwi 0x40 ; bge -> li r3,0 ; blr  (buffer full)
//
//   v0 = load(kfAmbientShadowFadeDistance)   ; splat(1.25f)
//   vcmpgtfp. v0, v3, v0                     ; lane-wise heightOffGround > 1.25
//   li r3, 1                                 ; result preset to TRUE
//   mfocrf/extrwi/cmplwi/bnelr               ; CR6[0] == ALL FOUR LANES TRUE -> return 1 without
//                                            ;   adding.  v3 is a broadcast, so this is simply
//                                            ;   "the car is more than 1.25 m off the ground".
//
//   ; --- accepted: the record base is `this + 0x10 + 64*count` (the count word occupies the
//   ; --- first 16 bytes of the buffer), so the four stores land at record +0x00/+0x10/+0x20/+0x30.
//   record.mvPos                            = matrix.wAxis + kvShadowOffset
//        lvx128 v13, r4, 0x30 / vaddfp v0, v13, kvShadowOffset / stvx128 v0, this+64*count, 0x10
//   record.mvAt                             = matrix.zAxis
//        lvx128 v0, r4, 0x20 / stvx128 v0, this+64*count, 0x20
//   record.mvScaledRight_HeightOffGround    = matrix.xAxis * lfWidth, w lane = lfHeightOffGround
//        lvx128 v0, r0, r4 / vmulfp128 v0, v0, v2 / vrlimi128 v0, v3, 1, 0
//        -> stvx128 at this + 64*count + 0x30
//        (the double read-modify-write at 0x823F76CC-0x823F76F0 is a compiler artifact of
//         SetVector3() followed by SetPlus(); the FIRST vrlimi128 re-inserts the stale w lane
//         that the second immediately overwrites, so it is dead and is collapsed here.)
//   record.mvFront_Rear_BackAxle_FrontAxle  = lvFwdLength_... stored VERBATIM, no arithmetic
//        stvx128 v1, this, (count+1)<<6   ==  this + 64*count + 0x40  ==  record + 0x30
//   ++*this ; return 1
//
// CONSTANTS -- RECOVERED 2026-08-12 (previously flagged as un-exported placeholder zeros).
// The three vector blobs read as 16 zero bytes in the loaded image because they are C++
// DYNAMIC initialisers; the values live in the initialiser code, not the .data blob:
//   * kfAmbientShadowFadeDistance (unk_82FAF0E0), init @0x82C4ED88:
//       lfs f0, flt_820092CC (0x3FA00000 = 1.25f) ; vspltw v0, v0, 0 ; stvx128 -> 82FAF0E0
//   * kvShadowOffset (unk_82FAEFB0), init @0x82C4ED48:
//       x = flt_82001CC0 (0x00000000 = 0.0f), y = flt_82009B8C (0x3CF5C28F = 0.03f),
//       z = flt_82001CC0 (0.0f), w = the integer 0 stored by `stw r9`
//   * kfAmbientShadowScaleFactor (unk_82FAFBF0), init @0x82C4EDB0:
//       lfs f0, flt_8200473C (0x3ECCCCCD = 0.4f) ; vspltw v0, v0, 0 ; stvx128 -> 82FAFBF0
// Names come from the PS3 DecFIGS DWARF for this TU (AddShadow @ PS3 0x3522BC); the values
// were dumped from BURNOUT_X360_ARTIST.XEX.i64.
//
// The DWARF source-level signature (const Matrix44Affine&, const Vector4, VecFloat, VecFloat)
// is what game code calls, and the register mapping above confirms it exactly.

// Names and types are the DecFIGS DWARF's for this exact source file, in its declared order
// (BrnBlobbyShadowManager.cpp:25/:26/:27); the values are dumped from the X360 initialisers.
namespace
{
    // DWARF :25 -- X360 unk_82FAEFB0 (init @0x82C4ED48). A 3 cm lift on the Y axis, added to
    // the shadow transform's translation row so the quad does not z-fight with the road.
    const Vector3 kvShadowOffset = { 0.0f, 0.03f, 0.0f, 0.0f };

    // DWARF :26 -- X360 unk_82FAF0E0 (init @0x82C4ED88), broadcast 1.25f.
    // A car more than 1.25 m off the ground casts no blobby shadow.
    const VecFloat kfAmbientShadowFadeDistance = { 1.25f, 1.25f, 1.25f, 1.25f };

    // DWARF :27 -- X360 unk_82FAFBF0 (init @0x82C4EDB0), broadcast 0.4f. A file-scope constant
    // of THIS TU; its only consumer is the manager's Render path, which is not in this TU's
    // ledger, so nothing here reads it yet. Recorded so the value is not lost again.
    const VecFloat kfAmbientShadowScaleFactor = { 0.4f, 0.4f, 0.4f, 0.4f };
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

    // Fade-distance reject (vcmpgtfp. v3 > kfAmbientShadowFadeDistance, then CR6[0] ==
    // "all four lanes greater"). lfHeightOffGround is a broadcast VecFloat so this reduces to
    // the scalar test height > 1.25 m, but the conjunction is spelled out to match the asm.
    // The X360 returns TRUE here -- the shadow is skipped, not an error.
    if (lfHeightOffGround.x > kfAmbientShadowFadeDistance.x &&
        lfHeightOffGround.y > kfAmbientShadowFadeDistance.y &&
        lfHeightOffGround.z > kfAmbientShadowFadeDistance.z &&
        lfHeightOffGround.w > kfAmbientShadowFadeDistance.w)
    {
        return true;
    }

    // Accepted: fill slot[count] from the incoming transform/extents, then bump the count.
    // Field order below follows the X360 store order.
    ShadowStruct& lSlot = maShadowPos[miNumShadows];

    // mvPos = the transform's translation row lifted by kvShadowOffset (vaddfp, all four lanes).
    lSlot.mvPos.x = lShadowTransform.wAxis.x + kvShadowOffset.x;
    lSlot.mvPos.y = lShadowTransform.wAxis.y + kvShadowOffset.y;
    lSlot.mvPos.z = lShadowTransform.wAxis.z + kvShadowOffset.z;
    lSlot.mvPos.w = lShadowTransform.wAxis.w + kvShadowOffset.w;

    // mvAt = the transform's projection ("at") axis, copied whole.
    lSlot.mvAt = lShadowTransform.zAxis;

    // mvScaledRight_HeightOffGround = right axis scaled by lfWidth (a broadcast, so the lanewise
    // vmulfp128 is a uniform scale), with the height off ground packed into the w lane
    // (DWARF SetVector3 + SetPlus == the two vrlimi128 inserts, the first of which is dead).
    lSlot.mvScaledRight_HeightOffGround.x = lShadowTransform.xAxis.x * lfWidth.x;
    lSlot.mvScaledRight_HeightOffGround.y = lShadowTransform.xAxis.y * lfWidth.y;
    lSlot.mvScaledRight_HeightOffGround.z = lShadowTransform.xAxis.z * lfWidth.z;
    lSlot.mvScaledRight_HeightOffGround.w = lfHeightOffGround.w;

    // mvFront_Rear_BackAxle_FrontAxle = the packed extents, stored VERBATIM (stvx128 v1).
    lSlot.mvFront_Rear_BackAxle_FrontAxle = lvFwdLength_RearLength_BackAxle_FrontAxle;

    ++miNumShadows;   // *this = count + 1
    return true;
}

// The external (published) half of the double-buffered pair. Bodied 2026-08-17 with
// BrnRendererModule::Update (boot audit F-P2-4), which is its only caller: the console forms
// `manager + mu8External * 0x1010` @0x824060C0-EC, i.e. exactly maBuffers[mu8External] --
// the byte it reads at manager+0x2021 is this member, and the 0x1010 stride is
// sizeof(BrnBlobbyShadowBuffer). Declared-only until now because nothing published it.
BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* BrnBlobbyShadowManager::GetExternalBuffer()
{
    return &maBuffers[mu8External];
}
