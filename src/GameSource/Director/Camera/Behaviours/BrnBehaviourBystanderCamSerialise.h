#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_BYSTANDER_CAM_SERIALISE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_BYSTANDER_CAM_SERIALISE_H

#include "types.hpp"
#include <cstddef>   // offsetof (the layout pins in _AssertLayout)

// The nested embedded parameter blocks this behaviour's Parameters carries, reused BY NAME:
//   Utils::CameraShake::Parameters -- fully defined in BehaviourRig.h (DWARF home BrnCameraShake.h)
//   Utils::Looker::Parameters      -- fully defined in BrnLooker.h (pulled in by BehaviourRig.h)
// Both expose the `template<class S> void Serialise(S&)` nested visitor the bystander block drives.
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCamSerialise.h
//
// Self-contained layout view of BrnDirector::Camera::BehaviourBystanderCam::Parameters used ONLY
// by the serialiser compile unit BrnBehaviourBystanderCamSerialise.cpp -- the HOME of the block's
// field-walk visitor TU (class:BrnDirector::Camera::BehaviourBystanderCam::Parameters):
//   - Parameters::Serialise<DebugMenuSerialiser>     @0x8224BF40
//   - Parameters::Serialise<TextFileReadSerialiser>  @0x82231B90
//   - Parameters::Serialise<TextFileWriteSerialiser> @0x8224DC28
//
// The full behaviour rig (Construct/Prepare/Update/SetupTweaker) and the scalar-only Parameters
// layout are already homed by the sibling TUs BehaviourBystanderCam.{h,cpp} and
// BrnBehaviourBystanderCam.{h,cpp}. To body the nested-block field walk (which recurses into the
// embedded CameraShake::Parameters @+0x08 and Looker::Parameters @+0x18 via the serialiser's
// nested-block Serialise<T> template) this view types those two sub-blocks BY NAME instead of the
// opaque byte spans the scalar-only view uses. It is a SELF-CONTAINED per-TU reconstruction (this
// header is included by exactly one .cpp) and agrees store-for-store on every pinned offset with
// the scalar-only view: mShakeParams @+0x08, mLookerParams @+0x18, the bystander scalars @+0x7C..
// +0x99. The two views are never mixed in one translation unit, so nothing is forked.
//
// Layout / offsets are pinned store-for-store from the three Serialise asm bodies (the Process/
// FormatName/fscanf displacements off r30/r31): the nested blocks at a1+8 (CameraShake) and a1+24
// (Looker), then the scalar walk at a1+124..a1+153.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: the camera-behaviour type tag. Each behaviour carries a type id in the leading word of its
//   Parameters block; the bystander-cam enumerator's console VALUE is 5 (the sibling SetParameters
//   @0x821F3F30 compares the block's first word against 5; Parameters::Construct stores 5 there).
//   Guarded so a TU that also pulls in a sibling bystander view does not re-declare it. Replace with
//   the real EBehaviourType enum when the Behaviour base TU lands; the enumerator VALUE (5) is pinned.
#ifndef BRNDIRECTOR_CAMERA_EBEHAVIOURTYPE_BYSTANDERCAM_DEFINED
#define BRNDIRECTOR_CAMERA_EBEHAVIOURTYPE_BYSTANDERCAM_DEFINED
enum EBehaviourTypeBystanderCam
{
    eBehaviourBystanderCam = 5
};
#endif

// ----------------------------------------------------------------------------
// Minimal outer-class shell whose sole purpose in this view is to scope the nested Parameters
// block the serialiser TU bodies. The behaviour's own rig is owned by BehaviourBystanderCam.{h,cpp}.
// ----------------------------------------------------------------------------
class BehaviourBystanderCam
{
public:

    // The bystander-cam parameter block (DWARF BehaviourBystanderCam.h:181, public Behaviour::
    // Parameters). Layout pinned from Parameters::Construct @0x821F9A00 and the three Serialise asm.
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's nested sub-blocks + scalar
        // fields into the camera-tunings serialiser S. Body + explicit instantiations live in
        // BrnBehaviourBystanderCamSerialise.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeBystanderCam GetType() const
        {
            return static_cast<EBehaviourTypeBystanderCam>(meType);
        }

        // ---- Behaviour::Parameters base head (DWARF :181) ----
        s32 meType;                                   // +0x00  type tag (= eBehaviourBystanderCam, 5)
        s32 miBaseField04;                            // +0x04  base param word (cleared to 0)

        // ---- embedded sub-blocks the field walk recurses into (nested-block Serialise<T>) ----
        Utils::CameraShake::Parameters mShakeParams;  // +0x08  "Shake parameters"  (4 f32 tunables)
        Utils::Looker::Parameters      mLookerParams; // +0x18  "Looker parameters" (0x64-byte block)

        // ---- bystander-specific scalars (DWARF :187..:198) ----
        f32  mfVelocityInfluenceOnPosition;           // +0x7C  "Target velocity influence on pos"
        f32  mfMaxInitialDistanceKM;                  // +0x80  "Max initial distance KM"
        f32  mfDistanceForFailKM;                     // +0x84  "Distance for fail KM"
        f32  mfTargetSpaceX;                          // +0x88  "Target Space X"
        f32  mfTargetSpaceY;                          // +0x8C  "Target Space Y"
        f32  mfTargetSpaceZ;                          // +0x90  "Target Space Z"
        f32  mfHeight;                                // +0x94  "Height"
        bool mbUseTargetSpaceInsteadOfPositionFinder; // +0x98  "Use target space"
        bool mbUseRangeTesting;                       // +0x99  "Use range testing"

    private:
        // Never called: pins the store-for-store offsets the Serialise asm reads. Every field named
        // here sits before any pointer member, so these offsets are LLP64-invariant (the X360 4-byte-
        // pointer displacements equal this 64-bit build's -- the block carries no pointer members).
        static void _AssertLayout()
        {
            static_assert(offsetof(Parameters, mShakeParams)  == 0x08, "mShakeParams @ +0x08");
            static_assert(offsetof(Parameters, mLookerParams) == 0x18, "mLookerParams @ +0x18");
            static_assert(offsetof(Parameters, mfVelocityInfluenceOnPosition) == 0x7C,
                          "mfVelocityInfluenceOnPosition @ +0x7C");
            static_assert(offsetof(Parameters, mfMaxInitialDistanceKM) == 0x80, "mfMaxInitialDistanceKM @ +0x80");
            static_assert(offsetof(Parameters, mfDistanceForFailKM)    == 0x84, "mfDistanceForFailKM @ +0x84");
            static_assert(offsetof(Parameters, mfTargetSpaceX)         == 0x88, "mfTargetSpaceX @ +0x88");
            static_assert(offsetof(Parameters, mfTargetSpaceY)         == 0x8C, "mfTargetSpaceY @ +0x8C");
            static_assert(offsetof(Parameters, mfTargetSpaceZ)         == 0x90, "mfTargetSpaceZ @ +0x90");
            static_assert(offsetof(Parameters, mfHeight)               == 0x94, "mfHeight @ +0x94");
            static_assert(offsetof(Parameters, mbUseTargetSpaceInsteadOfPositionFinder) == 0x98,
                          "mbUseTargetSpaceInsteadOfPositionFinder @ +0x98");
            static_assert(offsetof(Parameters, mbUseRangeTesting)     == 0x99, "mbUseRangeTesting @ +0x99");
        }
    };
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_BYSTANDER_CAM_SERIALISE_H
