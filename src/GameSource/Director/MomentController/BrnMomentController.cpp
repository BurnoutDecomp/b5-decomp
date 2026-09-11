// Out-of-line bodies for the BrnDirector::MomentController nested helpers.
// Reconstructed from the console executable, semantic-parity.
//
// Bodied here:
//   BrnDirector::MomentController::MomentHandle::GetMoment
//   BrnDirector::MomentController::MomentHandle::Release
//
// ⭐ Release MOVED HERE 2026-08-01, out of the project-invented split TU
// BrnMomentControllerNewMoment.cpp. The the declaration homes it at  -- this
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

// Asserts mbIsAllocated (+0x00), then returns the held moment pointer read from +0x04.
// With the declared layout
// that word is mMomentPoolHandle.mpObject -- the moment object the
// pool handed out -- so GetMoment returns the pool handle's stored object.
Moment* MomentController::MomentHandle::GetMoment() const
{
    CGS_ASSERT(mbIsAllocated, "mbIsAllocated");
    // The const handle's Get() yields const void*; the console build GetMoment returns the stored
    // moment pointer as a mutable Moment* (it only reads the slot's object-pointer word).
    return static_cast<Moment*>(const_cast<void*>(mMomentPoolHandle.Get()));
}

// Hand the held slot back to the owning pool
// and clear the allocated flag; a no-op when nothing is held. Returns TRUE unconditionally
// (both the taken and the not-taken branch reach the same `return true` tail).
// Console walk: read mbIsAllocated -- if clear, straight to the tail; otherwise
//   * the moment's own vtable slot 4 (+0x10) Release(), inside the tripwire
//               "GetMoment().Release()" (non-gating -- the console proceeds either way);
//   * the pool handle at +0x08 / its owner at +0x0C, then vtable slot 0 on the handle --
//     the pool handle's own release-through-the-owner, i.e. AbstractPoolVoidHandle::Release;
//   * store 0 into +0x00 -> mbIsAllocated = false.
bool MomentController::MomentHandle::Release()
{
    if (mbIsAllocated)
    {
        CGS_ASSERT(GetMoment()->Release(), "GetMoment().Release()");   //  (non-gating)
        mMomentPoolHandle.Release();
        mbIsAllocated = false;
    }
    return true;
}

} // namespace BrnDirector
