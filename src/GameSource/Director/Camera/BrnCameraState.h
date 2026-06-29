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

    // Zero-initialise the state (clears the head book-keeping word and both bit sets).
    // The director Camera::Construct asm @0x82255E68 calls this out-of-line on the
    // state sub-object at camera +0x138 (`bl BrnDirector__Camera__CameraState__Construct`).
    // DECLARATION-ONLY here -- the per-TU `cl /c` gate does not link; the body lands with
    // CameraState's own ledger TU (BrnCameraState.cpp).
    void Construct();

    // Set flag luIndex to lbValue. lbValue!=0 -> SetBit on the current set; lbValue==0 ->
    // UnSetBit on the current set. @0x82204368.
    void SetFlag(u32 luIndex, bool lbValue);

    // Clear flag luIndex (UnSetBit on the current set). @0x822044B0.
    void ClearFlag(u32 luIndex);

    // True iff flag luIndex's current bit differs from its previous-frame bit. @0x8227D380.
    bool HasChanged(u32 luIndex) const;

    // FLAG: only the members the flag methods touch are modelled, at their asm-attested offsets;
    //   the 8-byte CameraState head at +0x00 (not touched by this slice) is a reserved span. The
    //   full member set lands with this type's own ledger TU. Members are public so the file-scope
    //   offsetof pins in the .cpp can verify the size-stable bit-field offsets (all are pointer-
    //   width-independent here, so the layout is exact).
    u8 maReserved00[0x08];                       // +0x00  CameraState head (not modelled here)

    // +0x08: the current per-flag state (SetFlag/ClearFlag target; HasChanged first operand).
    CgsContainers::BitArray<KU_NUM_FLAGS> mCurrentFlags;

    // +0x10: the previous-frame per-flag state (HasChanged second operand).
    CgsContainers::BitArray<KU_NUM_FLAGS> mPreviousFlags;
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_CAMERA_STATE_H
