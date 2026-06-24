// Per-instantiation .cpp for DataJournal<rw::math::vpu::Vector3, 8> -- the director's
// 8-frame Vector3 history ring (BrnDirector::VehicleTracker's position / linear-velocity
// journals). The generic DataJournal<T,N> body is fully inline in BrnDirectorDataJournal.h;
// the X360 emits one out-of-line copy of each used member. This TU is the explicit
// per-member instantiation only (do NOT re-define the generic). The demangled X360 symbols
// spell the class as `rw::math::vpu::Vector3,8>` (the element type plus the `,8>` of the
// truncated template argument list); they are DataJournal<rw::math::vpu::Vector3,8> members:
//
//   GetCurrent()        @ 0x821FBBE0  (BehaviourManager / ImpactSlomoController / MainDirector
//                                       Update paths) -- the NON-const newest-sample accessor.
//   operator[](int)     @ 0x821FBC40  (BrnDirector::VehicleTracker::GetLinearVelocityLastFrame)
//   GetPrevious(int)    @ 0x82210C50  (BrnDirector::Camera::ImpactSlomoController::Update)
//
// Layout/asm parity: 8 elements x 16 bytes = 0x80 of inline storage, then miWriteIndex @ +0x80
// (==128) and miSize @ +0x84 (==132), matching the committed DataJournal<T,N> layout and the
// VehicleTracker journal map (cursor +0x080 / size +0x084).
//   GetCurrent() @0x821FBBE0: asserts miSize > 0 (BrnDirectorDataJournal.h:103) then returns
//     `16 * miWriteIndex + base` == &maEntries[miWriteIndex] -- the committed non-const GetCurrent().
//   operator[](int) @0x821FBC40: asserts liIndex >= 0 (:208) + liIndex < miSize (:209) then
//     returns `16 * ((miWriteIndex + liIndex) % 8) + base` == &maEntries[(miWriteIndex+liIndex)%N]
//     -- the committed const operator[] (its single bounds assert is the benign vacuous-CGS_ASSERT
//     reduction of the X360's pair; the index arithmetic is identical).
//   GetPrevious(int) @0x82210C50: asserts liIndex >= 0 (:114), miSize > 0 (:115),
//     liIndex < miSize-1 (:116) then returns operator[](liIndex + 1) -- the committed indexed
//     GetPrevious(int) overload added to BrnDirectorDataJournal.h for this TU.
#include "GameSource/Director/Utils/BrnDirectorDataJournal.h"
#include "rw/math/vpu/types.h"

template const rw::math::vpu::Vector3&
    DataJournal<rw::math::vpu::Vector3, 8>::operator[](int) const;
template rw::math::vpu::Vector3&
    DataJournal<rw::math::vpu::Vector3, 8>::GetCurrent();
template const rw::math::vpu::Vector3&
    DataJournal<rw::math::vpu::Vector3, 8>::GetPrevious(int) const;
