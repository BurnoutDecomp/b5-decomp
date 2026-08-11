#include <cstddef>                                                                 // offsetof
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::AddEvent (inline generic)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"   // BrnPhysics::Deformation::AddDeformationModelEvent (console 160, host 176)

// CgsModule::BaseEventQueue<BrnPhysics::Deformation::AddDeformationModelEvent>::AddEvent @ 0x825E52B0
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The generic
// BaseEventQueue<T>::AddEvent body is already inline in CgsBaseEventQueue.h; this is the thin
// explicit instantiation. The X360 body appends UNCONDITIONALLY (the two asserts are non-gating
// tripwires, not a bounds gate):
//   - assert mpEvents != NULL (CgsBaseEventQueue.h:312, `lwz r11,0(this)`; bne skips the assert);
//   - the overflow tripwire (miLength >= miMaxLength: `lwz r11,8(this)` (miLength) vs
//     `lwz r10,4(this)` (miMaxLength), `blt` skips) builds the StrStream message
//     "CgsModule::BaseEventQueue<class BrnPhysics::Deformation::AddDeformationModelEvent>::AddEvent
//      \nReached Max length <miMaxLength>\n" and fires it (:313) -- non-gating; the copy below runs
//     regardless.
//   - copies the 160-byte element to mpEvents[miLength] at a 160-byte stride
//     (`slwi r7,r11,2` == miLength*4; `add r11,r11,r7` == miLength*5; `slwi r11,r11,5` == *32 ==
//     miLength*160; `add r11,r11,r9` == mpEvents + miLength*160), then bumps miLength
//     (`stw r28,8(this)` with r28 == miLength+1) and returns 1.
// The X360 element copy is a head of scalar stores (stw +0, stw +4, std +8, stw +0x10), a body of
// seven 16-byte VMX lanes (stvx128 at +0x20/+0x30/+0x40/+0x50/+0x60/+0x70/+0x80) and a tail
// (stw +0x90, stfs +0x94, stw +0x98, stb +0x9C) -- i.e. it copies the whole 160-byte element image
// (the +0x14..0x1F gap is dead inter-member padding the asm skips). Modelled here as the generic
// `mpEvents[miLength] = lEvent` whole-struct copy-assignment, which copies every member of the
// 160-byte element.
//
// X360-attested element stride: miLength*160, sizeof(AddDeformationModelEvent) == 0xA0 ON THE
// CONSOLE. Callers (X360 xrefs): Deformation::DeformationInputInterface::AddDeformationModel,
// Vehicle::PhysicalTrafficManager::SendCreateRemoveTrafficEvents.
//
// ⭐⭐ THE HOST STRIDE IS 176, AND IT IS RE-DERIVED, NOT BUMPED (2026-08-11, handle-widening wave).
// This gate read 160 until the two pointer-bearing members were de-forked off their 4-byte
// stand-ins. Both widen on x64 and NOTHING ELSE MOVED; the console total is reproduced member by
// member and then re-added with host pointer widths:
//
//    member                             console            host x64
//    mModelHandle                +0x00   8  (2 x 4B ptr)   +0x00  16  (2 x 8B ptr)
//    mHandlingBodyID             +0x08   8  (`std`)        +0x10   8   (unchanged -- not a ptr)
//    mGlobalEntityId             +0x10   4  (`stw`)        +0x18   4   (unchanged -- 32-bit id)
//    (pad to the SIMD block)     +0x14  12                 +0x1C   4
//    mCOMOffset                  +0x20  16                 +0x20  16   ** SAME OFFSET **
//    mInitialWorldSpaceTransform +0x30  64                 +0x30  64   ** SAME OFFSET **
//    mInitialWorldSpaceVelocity  +0x70  16                 +0x70  16   ** SAME OFFSET **
//    mInitialWorldSpaceAngular   +0x80  16                 +0x80  16   ** SAME OFFSET **
//    mpVehiclePhysics            +0x90   4                 +0x90   8
//    mfInitialDamageAmount       +0x94   4                 +0x98   4
//    meBaseDeformationType       +0x98   4                 +0x9C   4
//    mbDoSweptSphereTests        +0x9C   1                 +0xA0   1
//    (alignas(16) tail pad)      +0x9D   3                 +0xA1  15
//                                     = 0xA0 = 160              = 0xB0 = 176
//
// ⭐ The four SIMD members land on their EXACT console offsets, which the pre-widening host layout
// did not (it had them at +16/+32/+96/+112 and only reached 160 by coincidence). So this change
// makes the record MORE console-faithful, and there is no new alignment hazard: every SIMD member
// is 16-aligned, alignas(16) is unchanged, and EventQueue<T,20> carves its storage as a plain
// C++ `T maEvents[20]`, so the compiler owns the buffer alignment (no hand-carved offsets exist).
// ⭐ The stride is never spelled as a literal anywhere -- BaseEventQueue<T> indexes `mpEvents[i]`
// and copies by assignment, so the queue follows sizeof automatically. Same re-derivation the
// sibling TinyStructs_embed_check.cpp:33 already did for ValidateRaceCarEvent ("X360 32;
// ResourceHandle widens" -> host 48). ⛔ If this ever fires again, re-derive it the same way.
static_assert(sizeof(BrnPhysics::Deformation::AddDeformationModelEvent) == 176,
              "AddDeformationModelEvent host stride 176 (X360 160; ResourceHandle + VehiclePhysics* widen)");

// The three width facts the 176 rests on, pinned individually so a regression names its own cause
// rather than only moving the total. ⭐ These are the SIZES, not a count (the console evidence for
// each is in BrnCommonTypes.h's eight-witness banner and BrnDeformationEvents.h).
static_assert(sizeof(BrnPhysics::Deformation::AddDeformationModelEvent::mModelHandle) == 16,
              "mModelHandle is CgsResource::ResourceHandle -- two host pointers (console 2 x 4)");
static_assert(sizeof(BrnPhysics::Deformation::AddDeformationModelEvent::mHandlingBodyID) == 8,
              "mHandlingBodyID is CgsPhysics::RigidBodyId -- the console `std`/`ld` at event +8");
static_assert(sizeof(BrnPhysics::Deformation::AddDeformationModelEvent::mGlobalEntityId) == 4,
              "mGlobalEntityId stays 32-bit -- the console `stw`/`lwz` at event +0x10");
static_assert(offsetof(BrnPhysics::Deformation::AddDeformationModelEvent, mCOMOffset) == 0x20,
              "the SIMD block starts at the console's own +0x20 once the head is the right width");

template bool
CgsModule::BaseEventQueue<BrnPhysics::Deformation::AddDeformationModelEvent>::AddEvent(
    const BrnPhysics::Deformation::AddDeformationModelEvent&);
