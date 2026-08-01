// Out-of-line bodies for the BrnDirector::MomentController nested helpers.
// Reconstructed from BURNOUT_X360_ARTIST.XEX, semantic-parity.
//
// Bodied here:
//   BrnDirector::MomentController::MomentHandle::GetMoment  @0x821F5798
//   BrnDirector::MomentController::MomentHandle::Release    @0x821F7390
//
// ⭐ Release MOVED HERE 2026-08-01, out of the project-invented split TU
// BrnMomentControllerNewMoment.cpp. The DWARF homes it at BrnMomentController.cpp:277 -- this
// file -- and it is needed by the link the moment BrnMomentSelector.cpp joins it (Release()
// walks every handle). The split TU cannot be mounted yet: it also holds NewMoment, whose
// twelve AllocateVoid<MomentXxx>() arms drag the moment-subclass family (+9 unresolved
// measured 2026-08-01, two of the Moments/ TUs do not currently compile, and there is a
// class-key ODR fork on Moment::Parameters). Keeping the two together would have meant
// stubbing a function whose real body already exists.
//
// MomentDescription is a plain POD (no out-of-line member needs a body here); it is homed
// purely by the header and instantiated through Array<MomentDescription,10> in its own TU.

#include "GameSource/Director/MomentController/BrnMomentController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (mbIsAllocated guard)

namespace BrnDirector
{

// @0x821F5798. Asserts mbIsAllocated (this+0x00; "mbIsAllocated" at BrnMomentController.h:150)
// then returns the held moment pointer (lwz r3, 4(r31) == this+0x04). With the DWARF layout
// (BrnMomentController.h:124) that word is mMomentPoolHandle.mpObject -- the moment object the
// pool handed out -- so GetMoment returns the pool handle's stored object.
Moment* MomentController::MomentHandle::GetMoment() const
{
    CGS_ASSERT(mbIsAllocated, "mbIsAllocated");
    // The const handle's Get() yields const void*; the X360 GetMoment returns the stored
    // moment pointer as a mutable Moment* (it only reads the slot's object-pointer word).
    return static_cast<Moment*>(const_cast<void*>(mMomentPoolHandle.Get()));
}

// @0x821F7390 -- DWARF BrnMomentController.cpp:277. Hand the held slot back to the owning pool
// and clear the allocated flag; a no-op when nothing is held. Returns TRUE unconditionally
// (`li r3, 1` at 0x821F7410, reached from BOTH the taken and the not-taken branch).
// Asm walk: lbz mbIsAllocated -> if clear, straight to the `li r3,1` tail; otherwise
//   0x821F73B0  the moment's own vtable slot 4 (+0x10) Release(), inside the :281 tripwire
//               "GetMoment().Release()" (non-gating -- the console proceeds either way);
//   0x821F73F0  lwz r3, 8(this) / lwz r4, 0xC(this) then vtable slot 0 on r3 -- the pool
//               handle's own release-through-the-owner, i.e. AbstractPoolVoidHandle::Release;
//   0x821F740C  stb 0, 0(this)  -> mbIsAllocated = false.
bool MomentController::MomentHandle::Release()
{
    if (mbIsAllocated)
    {
        CGS_ASSERT(GetMoment()->Release(), "GetMoment().Release()");   // :281 (non-gating)
        mMomentPoolHandle.Release();
        mbIsAllocated = false;
    }
    return true;
}

} // namespace BrnDirector
