#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FIXED_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FIXED_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"               // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"     // THE canonical Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"       // VisibilityCollisionPolicy (mCollisionPolicy)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h
//
// BrnDirector::Camera::BehaviourFixedCam -- the "fixed cam" camera behaviour. On its first frame
// it plants a camera a little way ahead of the player's car -- one second of its horizontal
// velocity, clamped to at least 5 m and failing past 20 m -- looks back at the car, and rolls
// the shot by a random dutch angle. After that it holds the camera where it was planted and
// keeps the car in frame. MomentStaticCamImpact (the static impact shot) and the arbitrator
// testbed pool it.
//
// ⭐ RE-BASED 2026-09-24 (FX-DIRECTOR). This used to be a HOLLOW SHELL: a class with no base and
// no virtuals that modelled only SetParameters' two stores. NewBehaviour<BehaviourFixedCam>
// therefore placed a vtable-less object in the behaviour pool, and BehaviourHelper::Prepare
// @0x82255F48, whose first act is to dispatch the pooled object's vtable slot 0 (Construct),
// read a null vptr. That is the access violation that stopped the moment tick
// (scratch/bugtest/runs/fxvoicepool/20260924_163149, where the same fault came through the
// bystander cam).
//
// LAYOUT AUTHORITY: the DecFIGS DWARF (BrnBehaviourFixedCam.h:52 `: public Behaviour`, members
// :90..:97, Parameters :103..:110), pinned by the ARTIST asm:
//   AllocateVoid<BehaviourFixedCam> @0x82253E10  stw off_8200A620 -> +0x000 (this vtable),
//                                                stw off_8200A158 -> +0x020 (the policy's),
//                                                slot size li r7, 0x2B0
//   +0x020  mCollisionPolicy   GetCollisionPolicy @0x821FB588 is `addi r3, r3, 0x20`
//   +0x260  mBaseTranform      Update copies it into the camera (0x8222A3BC..0x8222A3F0)
//   +0x2A0  mpParameters       SetParameters `stw r4, 0x2A0(r3)`; Construct `stw 0`
//   +0x2A4  mfDutch            Update `stfs f0, 0x2A4(r31)` (the random roll)
//   +0x2A8  mbPositionSet      Prepare `stb 0`, Update `stb 1` once the shot is planted
// (x64: the base's pointer and the policy widen, so absolute offsets are PROVENANCE -- every
//  access below is BY NAME.)
//
// VTABLE off_8200A620 (eight slots, the canonical Behaviour order):
//   0 Construct            0x82229D20        4 Release        0x8284CB38 (the base's empty body)
//   1 Prepare              0x821FAD28        5 GetCollisionPolicy 0x821FB588
//   2 Update               0x82229DE0        6 SetupTweaker   0x821FAD48
//   3 PostCollisionUpdate  0x82C296C8 (`li r3, 1` -- the base's)   7 GetName  0x821FAE48
// Slots 3 and 4 are the Behaviour base's own bodies (the DWARF declares no override for either).
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// The fixed-cam behaviour-type tag. SetParameters @0x821F4518 compares the block's first word
// against 0xF (`cmplwi r11, 0xF`); BehaviourParameterBank::Construct @0x8223DC90 stores 15 into the
// bank's fixed-cam block (+9012).
enum EBehaviourTypeFixedCam
{
    eBehaviourFixedCam = 15
};

class BehaviourFixedCam : public Behaviour
{
public:
    // DWARF BrnBehaviourFixedCam.h:103 -- the fixed-cam parameter block.
    class Parameters : public Behaviour::Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's two tunables into the
        // camera-tunings serialiser S. Body + instantiations: BrnBehaviourFixedCamSerialise.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        // DWARF :110 (BrnBehaviourFixedCam.cpp:34). No out-of-line console symbol: the one caller,
        // BehaviourParameterBank::Construct @0x8223DC90, inlines it over the bank's fixed-cam block
        // (0x8223DE6C..: +9016 = 0, +9020 = 70.0, +9012 = 15, +9024 = 10.0). Body in the .cpp.
        void Construct();

        f32 mfFOV;        // :106  console +0x08  (the "FOV" tweak, label rodata 0x820051C0)
        f32 mfMaxDutch;   // :107  console +0x0C  (the "Roll" tweak / "Max Dutch" menu entry)
    };

    // Adopt a fixed-cam parameter block. @0x821F4518 (h:122): assert the block's type tag, cache
    // its debug name in the base's +0x10 slot, store the pointer. NOT a virtual override -- it
    // takes the DERIVED Parameters type, so it HIDES the base's non-virtual pair.
    void SetParameters(const Parameters* lpParameters)
    {
        CGS_ASSERT(lpParameters->GetType() == eBehaviourFixedCam,
                   "lpParameters->GetType() == eBehaviourFixedCam");   // h:124
        mpParameters = lpParameters;                                    // stw r4, 0x2A0(this)
        SetDebugParametersName(lpParameters->GetDebugName());          // lwz 4(lp) ; stw 0x10(this)
    }

    // ---- the virtual interface (vtable off_8200A620) ------------------------------------------
    void Construct() override;                                                    // slot 0 (cpp:51)
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;        // slot 1 (cpp:69)
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;     // slot 2 (cpp:87)
    CollisionPolicy* GetCollisionPolicy() override;                                // slot 5 (cpp:162)
    void SetupTweaker(Utils::Tweaker& lrTweaker) override;                         // slot 6 (cpp:176)
    const char* GetName() const override;                                          // slot 7 (cpp:198)

private:
    // ---- layout (DWARF :90..:97) ----------------------------------------------------------------
    VisibilityCollisionPolicy mCollisionPolicy;   // :90  console +0x020
    Matrix44Affine            mBaseTranform;      // :92  console +0x260  (sic -- the DWARF's spelling)
    const Parameters*         mpParameters;       // :94  console +0x2A0
    f32                       mfDutch;            // :96  console +0x2A4
    bool                      mbPositionSet;      // :97  console +0x2A8
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FIXED_CAM_H
