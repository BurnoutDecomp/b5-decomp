#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_STATE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_STATE_H

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<30>
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT (the index-bound guards)

// ============================================================================
// GameSource/Director/Camera/BrnCameraState.h
//
// BrnDirector::Camera::CameraState -- the director camera's dirty-flag double buffer.
// It carries TWO fixed-capacity bit sets of 30 bits each: the CURRENT per-flag state
// and the PREVIOUS-frame state. SetFlag/ClearFlag mutate the current set; HasChanged
// compares one flag's current bit against its previous bit (returns true iff they differ).
//
// HOME for the CameraState slice this TU owns: SetFlag @0x82204368,
// ClearFlag @0x822044B0, HasChanged @0x8227D380.
//
// ----------------------------------------------------------------------------
// Layout pinned by the X360 asm (NUMBITS = 30 == 0x1E, the index-bound the asserts compare):
//   - The current bit set is the 64-bit field at +0x08 (SetFlag/ClearFlag base is
//     `addi r,this,8`; the std/ldx target). HasChanged's first operand reads the qword at
//     `this + ((index>>6)+1)*8` == +0x08 for index<30.
//   - The previous bit set is the 64-bit field at +0x10. HasChanged's second operand reads
//     `this + ((index>>6)+2)*8` == +0x10 for index<30.
// Each BitArray<30> rounds up to one 64-bit field, so each member is exactly 8 bytes.
// The 8 bytes at +0x00 precede the bit sets (CameraState head not modelled by this slice).
//
// The X360 index-bound asserts cite CgsBitArray.h (lines 203/222/241) because the bound checks
// are emitted at these inlined call sites (the callers own the CgsDev::Assert API); the bit
// math itself matches CgsContainers::BitArray's inlined SetBit/UnSetBit/IsBitSet.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

class CameraState
{
public:
    // The bit sets are 30-bit (the X360 bound the asserts compare: luIndex < 30).
    static const u32 KU_NUM_FLAGS = 30;

    // The flag-index enum (DecFIGS DWARF BrnCameraState.h:56 -- the declaration shape;
    // E_FLAG_COUNT == 30 is exactly the KU_NUM_FLAGS bound the X360 asserts compare
    // against). Corroborated in the ARTIST asm at the three indices its readers inline:
    // EffectsModule::Update @0x8229EC28 tests `(*(camera+81) & 8)` == index 3
    // (RACING_GAMEPLAY_CAMERA), `& 0x10` == index 4 (BUMPER_CAM) and `& 0x400000` ==
    // index 22 (IN_JY_CAMERA). Added 2026-09-02 so consumers stop passing bare integers.
    enum EFlag
    {
        E_FLAG_WIDESCREEN                 = 0,
        E_FLAG_VALID                      = 1,
        E_FLAG_HIDE_PLAYER                = 2,
        E_FLAG_RACING_GAMEPLAY_CAMERA     = 3,
        E_FLAG_BUMPER_CAM                 = 4,
        E_FLAG_SUPER_WIDE                 = 5,
        E_FLAG_NEW_THIS_FRAME             = 6,
        E_FLAG_JUMP_CAMERA                = 7,
        E_FLAG_HARD_STOP_CAMERA           = 8,
        E_FLAG_TUMBLING_PLAYER_FOCUSED    = 9,
        E_FLAG_CRASH_CAMERA               = 10,
        E_FLAG_TAKEDOWN_CAMERA            = 11,
        E_FLAG_INTERNAL_CAR_CAMERA        = 12,
        E_FLAG_DONT_UPDATE_SCENESPACE     = 13,
        E_FLAG_IS_PICTURE_PARADISE        = 14,
        E_FLAG_CRASH_STUNT                = 15,
        E_FLAG_SMALL_NEAR_CLIP            = 16,
        E_FLAG_RECORD_METRICS             = 17,
        E_FLAG_SEND_METRICS               = 18,
        E_FLAG_APPLYING_RACE_END_EFFECT   = 19,
        E_FLAG_JUMP_PHOTO                 = 20,
        E_FLAG_JY_FLASH                   = 21,
        E_FLAG_IN_JY_CAMERA               = 22,
        E_FLAG_JY_NEW_CAR_INTRO           = 23,
        E_FLAG_ONLINE_RACE_INTRO_START    = 24,
        E_FLAG_ONLINE_RACE_INTRO_RIVAL    = 25,
        E_FLAG_ONLINE_RACE_INTRO_FINISHED = 26,
        E_FLAG_ROAD_FOLLOWING_CAM         = 27,
        E_FLAG_SHOWING_ONLINE_INTRO       = 28,
        CONSISTENCY_TEST                  = 29,
        E_FLAG_COUNT                      = 30
    };

    // Zero-initialise the state (clears the head book-keeping word and both bit sets).
    // The director Camera::Construct asm @0x82255E68 calls this out-of-line on the
    // state sub-object at camera +0x138 (`bl BrnDirector__Camera__CameraState__Construct`).
    // DECLARATION-ONLY here -- the per-TU `cl /c` gate does not link; the body lands with
    // CameraState's own ledger TU (BrnCameraState.cpp).
    void Construct();

    // Reset the state (X360 @0x82220950, called by Camera::Clear @0x8223CE70 and by
    // Construct's tail): zero both frame bit sets, mask the head set by the
    // ValidityAccount fail-flag mask (asserting the mask was set up), then raise
    // bit 0 (the CONSISTENCY_TEST flag) on both frame sets. Body in BrnCameraState.cpp.
    void Clear();

    // Set flag luIndex to lbValue. lbValue!=0 -> SetBit on the current set; lbValue==0 ->
    // UnSetBit on the current set. @0x82204368.
    void SetFlag(u32 luIndex, bool lbValue);

    // Clear flag luIndex (UnSetBit on the current set). @0x822044B0.
    void ClearFlag(u32 luIndex);

    // True iff flag luIndex's current bit differs from its previous-frame bit. @0x8227D380.
    bool HasChanged(u32 luIndex) const;

    // @0x82220BC0 (DWARF BrnCameraState.cpp:121 -- the assert line). Merge two states for a
    // camera blend. NOT a numeric interpolation: the flag sets are combined with AND, so a
    // flag survives only while BOTH endpoints hold it -- EXCEPT five bits that are OR-ed, so
    // they propagate if EITHER endpoint has them. See the .cpp for which and why.
    static CameraState Interpolate(const CameraState& lrFrom, const CameraState& lrTo, f32 lfT);

    // ADDITIVE GROW (BrnDirector::InertiaController::Update @0x8221ECD0): read one
    // current flag. The X360 inlines it as the 64-bit field load + mask test (the
    // same bit math as BitArray::IsBitSet); exposed by name so consumers stay off the
    // raw field.
    bool IsFlagSet(u32 luIndex) const { return mCurrentFlags.IsBitSet(luIndex); }

    // ADDITIVE GROW (BrnDirector::MomentFailSafe::Update @0x8220A2B0): set one bit of
    // the head bookkeeping set (see mHeadFlags below).
    void SetHeadFlag(u32 luIndex) { mHeadFlags.SetBit(luIndex); }

    // FLAG: only the members the flag methods touch are modelled, at their asm-attested offsets;
    //   the 8-byte CameraState head at +0x00 (not touched by this slice) is a reserved span. The
    //   full member set lands with this type's own ledger TU. Members are public so the file-scope
    //   offsetof pins in the .cpp can verify the size-stable bit-field offsets (all are pointer-
    //   width-independent here, so the layout is exact).
    // +0x00: the 8-byte head bookkeeping field. Remodelled from the previous opaque
    // reserved span to a third BitArray-shaped field (same 8 bytes, same placement):
    // BrnDirector::MomentFailSafe::Update @0x8220A2B0 sets one bit of it with the same
    // ld/oris/std qword bit math the two attested BitArrays use (bit index 18 at that
    // site). FLAG: the head set's ROLE is not yet recovered -- named neutrally.
    CgsContainers::BitArray<KU_NUM_FLAGS> mHeadFlags;

    // +0x08: the current per-flag state (SetFlag/ClearFlag target; HasChanged first operand).
    CgsContainers::BitArray<KU_NUM_FLAGS> mCurrentFlags;

    // +0x10: the previous-frame per-flag state (HasChanged second operand).
    CgsContainers::BitArray<KU_NUM_FLAGS> mPreviousFlags;
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_STATE_H
