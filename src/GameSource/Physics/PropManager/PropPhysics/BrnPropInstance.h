// GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h
#pragma once

// BrnPhysics::Props::PropInstance -- a single physical prop instance owned by the
// PropManager. This header is authored from the DecFIGS DWARF layout for this exact
// source path (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/PropPhysics/
// BrnPropInstance.h), with every member OFFSET pinned against the X360 ARTIST asm of the
// out-of-line accessors in this batch:
//   GetJointIndex      @ 0x825B96B0  reads u8 @ +0x6C  (mu8JointIndex)
//   SetAngularVelocity @ 0x825DE5F8  stores 16B @ +0x50 (mAngularVelocity)
//   SetTransform       @ 0x825DE370  loads/stores four 16B rows @ +0x00..+0x30 (mWorldTransform)
//
// LAYOUT (DWARF member order + asm-attested offsets):
//   +0x00  Matrix44Affine mWorldTransform  (64B: four Vector3 rows, alignas(16))
//   +0x40  Vector3        mLinearVelocity  (16B)
//   +0x50  Vector3        mAngularVelocity (16B)
//   +0x60  PropEntityID   mEntityId        (4B, one packed EntityId word)
//   +0x64  uint32_t       muTypeId
//   +0x68  uint32_t       muInstanceId
//   +0x6C  uint8_t        mu8JointIndex    (KU_NOT_JOINTED == 255 => not jointed)
//   +0x6D  bool           mbIsStatic
//   +0x6E  uint8_t        muMovementState  (EPropMovementState)
//   +0x6F  uint8_t        mu8Flags         (KU_ADDED_THIS_FRAME_FLAG / KU_HAS_EXTRA_COM_OFFSET_FLAG)
//
// Only the three accessors reconstructed in this batch are DEFINED (in the sibling .cpp);
// the remaining DWARF-declared trivial accessors are declared for shape but left for a
// later batch (their bodies are inline field pokes not yet grounded against asm here).

#include "types.hpp"                                     // u8/u32/s32 primitives
#include "BrnCommonTypes.h"                              // Vector3, Matrix44Affine
#include "SharedClasses/Physics/Props/BrnPropEntityID.h" // BrnWorld::PropEntityID
#include <cstddef>                                       // offsetof

namespace BrnPhysics
{
namespace Props
{
    // BrnPropInstance.h:37 (DWARF) -- movement state stored in muMovementState.
    enum EPropMovementState
    {
        E_PROP_MOVESTATE_STATIONARY = 0,
        E_PROP_MOVESTATE_JUST_MOVED = 1,
        E_PROP_MOVESTATE_MOVING     = 2,
    };

    // The prop-entity handle type is BrnWorld::PropEntityID (SharedClasses home).
    typedef BrnWorld::PropEntityID PropEntityID;

    struct PropInstance
    {
        // --- file-scope constants (DWARF BrnPropInstance.h:151-153) ---
        static const uint8_t KU_NOT_JOINTED             = 255; // sentinel: no joint
        static const uint8_t KU_ADDED_THIS_FRAME_FLAG   = 1;   // mu8Flags bit 0
        static const uint8_t KU_HAS_EXTRA_COM_OFFSET_FLAG = 2; // mu8Flags bit 1

        // --- accessors reconstructed in this batch (bodies in BrnPropInstance.cpp) ---

        // 0x825DE370 -- validate + store the world transform (all four rows).
        void SetTransform(Matrix44Affine lTransform);

        // 0x825DE5F8 -- validate + store the angular velocity.
        void SetAngularVelocity(Vector3 lAngularVelocity);

        // ⭐ ADDED 2026-08-18 (breakable-props keystone). DWARF BrnPropInstance.h:91.
        // X360 0x825DE6C8 -- the LINEAR twin of SetAngularVelocity: the same three-lane
        // vcmpeqfp self-equality NaN test then `stvx128 v127, r31, 64` == mLinearVelocity
        // (+0x40). Its assert text/line are the binary's own: "RwMath::IsValid(
        // lLinearVelocity )", PropPhysics/BrnPropInstance.h:266.
        // ⚠️ THE ADDRESS HAS NO PER-ADDRESS JSON IN .ida-exports (an export hole, like
        // AddContactResultsToQueue below it) -- the 51-instruction body was pulled with
        // headless IDA 9.3 out of IDA Files/BURNOUT_X360_ARTIST.XEX.i64, where the symbol
        // IS present. It is CALLED by PropManager::ReadUpdatedBodies @0x82632918, which is
        // why the hole mattered. Body in BrnPropInstance.cpp.
        void SetLinearVelocity(Vector3 lLinearVelocity);

        // 0x825B96B0 -- assert IsJointed() then return the joint index.
        int32_t GetJointIndex() const;

        // Inline tripwire predicate used by GetJointIndex (DWARF :131). The X360 folds
        // this into the caller as (mu8JointIndex != KU_NOT_JOINTED).
        bool IsJointed() const { return mu8JointIndex != KU_NOT_JOINTED; }

        // ==================================================================================
        // ⭐ ADDED 2026-08-18 (breakable-props keystone wave -- BrnPropManager.cpp's eleven
        // un-bodied functions). The DWARF declares the full trivial-accessor set for this
        // class (BrnPropInstance.h:53..:147) and the X360 folds EVERY one of them into its
        // callers -- PropManager::ReadUpdatedBodies / CreateContactEvent / AddPropToSim read
        // and write these exact fields with inline lwz/stb/stvx at the offsets the banner
        // above already pins. They are given inline bodies here (same precedent, and same
        // "no out-of-line emission exists" reason, as PropPartInstance::GetType) so
        // implementers reach the members BY NAME instead of poking the struct.
        //
        // ⚠️ Construct() and Prepare() are DECLARATION-ONLY: no body exists anywhere in the
        // tree (re-grepped 2026-08-18, round 3 -- `PropInstance::Construct` matches only
        // comments), and unlike Release/Destruct below there is no folded call site that
        // would tell us what they do. Not invented.
        //
        // ⚠️ UPDATED round 3 -- "nothing in this wave's closure calls them" was true of the
        // TREE and is still true of the tree, but it is NOT true of the source. The DecFIGS
        // callee list for PropManager::ProcessAddPropInstanceEvents names
        // `PropInstance::Construct(...)` explicitly, immediately before its two
        // rw::math::vpu::Vector3::SetZero entries -- so the source really did write
        // `lpProp->Construct();` where this wave's landed body hand-spells the ten-store
        // instance seed. It is STILL not bodyable from that one site: the same callee list
        // contains SetTypeId but NOT SetEntityId even though the mEntityId store is plainly
        // emitted, so the Construct-vs-caller split is not recoverable from one call site.
        // A second site (PropManager::CreatePart or ::AddPropToSim) has to pin it first.
        // ==================================================================================
        void Construct();                                    // DWARF :53  (no body here)
        bool Prepare();                                      // DWARF :57  (no body here)

        // DWARF :61 / :65. MEASURED, not assumed: PropManager::Release @0x825BAC88 is the
        // whole loop `for(i<muNumberOfPropInstances) lbSuccess &= mpaPropInstances[i].Release()`
        // with the call folded away and `clrlwi r3,r3,31` (== `& 1`) as the only survivor, so
        // Release() is a side-effect-free constant `true`; PropManager::Destruct @0x825E3398
        // likewise keeps NO loop at all, so Destruct() is empty. (The DWARF locals luIndex /
        // lbSuccess at BrnPropManager.cpp:245/246 and luIndex at :275 prove the loops were in
        // the source, which is what makes "the callee is empty" the reading rather than "the
        // loop never existed".)
        bool Release()  { return true; }                     // DWARF :61
        void Destruct() {}                                   // DWARF :65

        uint32_t     GetTypeId()                     { return muTypeId; }        // :68
        PropEntityID GetEntityId()                   { return mEntityId; }       // :71
        void         SetEntityId(PropEntityID lId)   { mEntityId = lId; }        // :74
        void         SetTypeId(uint32_t luTypeId)    { muTypeId = luTypeId; }    // :77

        // :83. The whole affine.
        // ⚠️ DWARF :80 also declares `Vector3 & GetPos()`. NOT landed: in the committed
        // tree Matrix44Affine's rows are rw::math::vpu::Vector4, not Vector3, so the DWARF
        // return type cannot bind to mWorldTransform.wAxis without a reinterpret -- and
        // nothing in this wave's closure calls it. Reach the position row through
        // GetTransform().wAxis instead.
        const Matrix44Affine& GetTransform() const    { return mWorldTransform; }

        Vector3 GetLinearVelocity()                   { return mLinearVelocity; }   // :98
        Vector3 GetAngularVelocity() const            { return mAngularVelocity; }  // :101

        void     SetInstanceID(uint32_t luId)         { muInstanceId = luId; }      // :104
        uint32_t GetInstanceID() const                { return muInstanceId; }      // :107

        // :111 / :114. mu8JointIndex is the u8 slot; KU_NOT_JOINTED (255) is the sentinel.
        void SetJointIndex(int32_t liJointIndex)
        {
            mu8JointIndex = static_cast<uint8_t>(liJointIndex);
        }
        void SetNotJointed()                          { mu8JointIndex = KU_NOT_JOINTED; }

        void SetIsStatic(bool lbIsStatic)             { mbIsStatic = lbIsStatic; }  // :118
        bool IsStatic() const                         { return mbIsStatic; }        // :134

        // :122 / :125. mu8Flags bit 1. AddPropToSim @0x826274D8 gates the
        // K_PROP_EXTRA_COM_OFFSET add on the same bit, and ReadUpdatedBodies @0x82632918
        // re-reads it (`lbz +0x6F ; andi 2`) to undo the offset on the way back.
        void SetExtraComOffsetFlag(bool lbSet)
        {
            mu8Flags = static_cast<uint8_t>(lbSet ? (mu8Flags | KU_HAS_EXTRA_COM_OFFSET_FLAG)
                                                  : (mu8Flags & ~KU_HAS_EXTRA_COM_OFFSET_FLAG));
        }
        bool HasExtraComOffset()
        {
            return (mu8Flags & KU_HAS_EXTRA_COM_OFFSET_FLAG) != 0;
        }

        // :138 / :141. muMovementState holds an EPropMovementState; CreateContactEvent
        // @0x825A53A0 already compares/writes it as a raw u8 at +0x6E.
        void SetMovementState(EPropMovementState leState)
        {
            muMovementState = static_cast<uint8_t>(leState);
        }
        EPropMovementState GetMovementState() const
        {
            return static_cast<EPropMovementState>(muMovementState);
        }

        // :144 / :147. mu8Flags bit 0.
        bool WasAddedThisFrame() const
        {
            return (mu8Flags & KU_ADDED_THIS_FRAME_FLAG) != 0;
        }
        void ClearAddedThisFrameFlag()
        {
            mu8Flags = static_cast<uint8_t>(mu8Flags & ~KU_ADDED_THIS_FRAME_FLAG);
        }

        // ------------------------------------------------------------------
        // Members (DWARF order; offsets asm-pinned -- see banner).
        // ------------------------------------------------------------------
        Matrix44Affine mWorldTransform;   // +0x00  (64B)
        Vector3        mLinearVelocity;   // +0x40  (16B)
        Vector3        mAngularVelocity;  // +0x50  (16B)
        PropEntityID   mEntityId;         // +0x60  (4B)
        uint32_t       muTypeId;          // +0x64
        uint32_t       muInstanceId;      // +0x68
        uint8_t        mu8JointIndex;     // +0x6C
        bool           mbIsStatic;        // +0x6D
        uint8_t        muMovementState;   // +0x6E  (EPropMovementState)
        uint8_t        mu8Flags;          // +0x6F
    };

    // Pointer-free sub-structs / offset pins. mWorldTransform is the first member after
    // the (vtable-less) object head, and mu8JointIndex..mu8Flags are POD bytes with no
    // pointer widening on the 64-bit host, so these offsets are host-stable and assertable.
    static_assert(offsetof(PropInstance, mWorldTransform)  == 0x00, "mWorldTransform @0x00");
    static_assert(offsetof(PropInstance, mLinearVelocity)  == 0x40, "mLinearVelocity @0x40");
    static_assert(offsetof(PropInstance, mAngularVelocity) == 0x50, "mAngularVelocity @0x50");
    static_assert(offsetof(PropInstance, mu8JointIndex)    == 0x6C, "mu8JointIndex @0x6C");
    static_assert(offsetof(PropInstance, mbIsStatic)       == 0x6D, "mbIsStatic @0x6D");
    static_assert(offsetof(PropInstance, muMovementState)  == 0x6E, "muMovementState @0x6E");
    static_assert(offsetof(PropInstance, mu8Flags)         == 0x6F, "mu8Flags @0x6F");
}
}
