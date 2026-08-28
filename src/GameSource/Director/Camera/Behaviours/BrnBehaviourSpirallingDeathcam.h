#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the Start !mbStarted tripwire)
#include "GameSource/Director/Camera/Utils/BrnLooker.h"       // Utils::Looker::Parameters (by value)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"  // Utils::CameraShake::Parameters (by value)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h
//
// BrnDirector::Camera::BehaviourSpirallingDeathcam -- the post-crash "spiralling deathcam"
// behaviour the crashing/testbed arbitrator states drive. HOME for the class slices owned by
// this TU set:
//   - BehaviourSpirallingDeathcam::Prepare @0x821FB3F8  (resets a state field; defined in the
//     .cpp)
//   - BehaviourSpirallingDeathcam::Start   @0x821F5620  (latches mbStarted; defined in the .cpp)
//
// The full behaviour (Construct/Update and the rest of the rig) lands with its own TU; this
// header models only the members these two functions touch, BY NAME, at their asm-attested
// offsets.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

class BehaviourSpirallingDeathcam
{
public:

    // ------------------------------------------------------------------------
    // The deathcam parameter block. HOME here because the ledger nests it under this behaviour
    // (BrnDirector::Camera::BehaviourSpirallingDeathcam::Parameters) and the camera-tunings bank
    // saves it through TextFileWriteSerialiser::Serialise<Parameters> @0x82214DE0.
    //
    // FLAG: the text-serialise field-walk for this block is ATTESTED EMPTY. The X360 instantiation
    //   @0x82214DE0 emits only the section-header label line + recursion-depth accounting; it
    //   discards the parameter-block register (mr r5,r4 overwrites the params ptr before FormatName)
    //   and makes NO `bl` to any inner Parameters::Serialise field walker -- i.e. the compiler
    //   inlined the inner visitor to nothing because it serialises zero fields to text. The
    //   visitor below is therefore an empty (zero-field) walk, faithful to the attested asm; NO
    //   field offsets are fabricated.
    //
    // ⭐ THE LAYOUT IS NO LONGER "lands with the deathcam rig TU" (2026-08-29, crash-camera wave).
    // ArbStateCrashing::Prepare @0x822655E8 hands this behaviour a block out of the shared
    // NamedParameters bank, so SetParameters -- and with it this type -- had to become real.
    // MEMBER NAMES AND ORDER are the DecFIGS DWARF for this file (:158..:176), verbatim;
    // DEFAULTS are Parameters::Construct @0x821FB498, store for store. The pin is that the
    // thirteen trailing floats the asm writes and the DWARF's thirteen trailing names line up
    // one-to-one in ascending offset:
    //   +0x7C 60.0  mfRotationSpeedDegs            +0x80 0.5  mfHeightIncreaseSpeed
    //   +0x84 0.0   mfRadiusIncreaseSpeed          +0x88 0.5  mfInitialHeight
    //   +0x8C 1.5   mfInitialRadius                +0x90 4.0  mfTimeBeforeFullLookOffsetSpeed
    //   +0x94 1.0   mfLookOffsetSpeed              +0x98 15.0 mfHeightIncreaseDuration
    //   +0x9C 5.0   mfRotationSpeedDecreaseRate    +0xA0 20.0 mfMinRotationSpeed
    //   +0xA4 0.8   mfMinAttachAmount              +0xA8 7.0  mfBlurTime
    //   +0xAC 7.0   mfShakeTime
    // ------------------------------------------------------------------------
    class Parameters
    {
    public:
        // The behaviour type tag SetParameters asserts on (asm: `cmplwi r11, 0x13` == 19).
        s32 GetType() const { return meType; }

        // @0x821FB498 -- seed the block to its authored defaults. Body in the .cpp.
        void Construct();

        // X360 visitor: `void Serialise<S>(S&)` for the camera-tunings serialiser S. Attested
        // EMPTY for the text writer (see the class FLAG): walks zero fields. Templated inline so
        // TextFileWriteSerialiser::Serialise<Parameters>'s odr-use inlines it away, matching the
        // degenerate instantiation asm (no inner field-walk call).
        template<class TSerialiser> void Serialise(TSerialiser& /*lrSerialiser*/) {}

        // ---- Behaviour::Parameters base head ----
        s32 meType;                                // +0x00  = eBehaviourSpirallingDeathcam (19)
        s32 miBaseField04;                         // +0x04  base param word (cleared to 0)

        // ---- the embedded sub-blocks (DWARF :158 / :159) ----
        Utils::Looker::Parameters      mLookerParams;  // +0x08  (seeded by its own Construct)
        Utils::CameraShake::Parameters mShakeParams;   // +0x6C  (0.06 / 0.0 / 1.15 / 0.11)

        // ---- the deathcam's own tunables (DWARF :161..:176) ----
        f32 mfRotationSpeedDegs;                   // +0x7C
        f32 mfHeightIncreaseSpeed;                 // +0x80
        f32 mfRadiusIncreaseSpeed;                 // +0x84
        f32 mfInitialHeight;                       // +0x88
        f32 mfInitialRadius;                       // +0x8C
        f32 mfTimeBeforeFullLookOffsetSpeed;       // +0x90
        f32 mfLookOffsetSpeed;                     // +0x94
        f32 mfHeightIncreaseDuration;              // +0x98
        f32 mfRotationSpeedDecreaseRate;           // +0x9C
        f32 mfMinRotationSpeed;                    // +0xA0
        f32 mfMinAttachAmount;                     // +0xA4
        f32 mfBlurTime;                            // +0xA8
        f32 mfShakeTime;                           // +0xAC
    };

    // The behaviour type tag this class's parameter blocks carry. The VALUE is asm
    // (SetParameters @0x821F5680 compares the block's first word against 0x13).
    enum EBehaviourTypeSpirallingDeathcam
    {
        eBehaviourSpirallingDeathcam = 19
    };

    // Adopt an authored parameter block. @0x821F5680: assert the block's type tag, then store
    // the pointer at +0x2D0.
    void SetParameters(const Parameters* lpParameters);

    // Has this activation already been started. ⭐ ADDED 2026-08-29: ArbStateCrashing::Update
    // gates its Start() call on exactly this byte (`lbz r11, 0x2F4(behaviour)`), and the byte is
    // private -- a named reader keeps that gate off a raw offset.
    bool IsStarted() const { return mbStarted != 0; }

    // Reset the per-activation state word. @0x821FB3F8. The asm writes 0 to a state field near
    // the head of the object (`*(this + 8) = 0`) and returns true.
    bool Prepare();

    // Latch the "started" flag for this activation. @0x821F5620: asserts !mbStarted (the behaviour
    // must not be double-started), then stores mbStarted = 1.
    void Start();

private:

    // FLAG: only the members Prepare/Start write are modelled at their asm-attested offsets; the
    //   rest of the deathcam rig lands with the full behaviour TU. Reserved spans place the two
    //   written members exactly:
    //     +0x00 .. +0x07  opaque base head (Behaviour vtable + shared head)
    //     +0x08           muStateField  (Prepare: stw 0, 8(this))
    //     +0x09 .. +0x2F3 rig members not modelled here
    //     +0x2F4          mbStarted     (Start: stb 1, 0x2F4(this); asserted clear on entry)
    u8  maReserved00[0x08];               // +0x00 .. +0x07  opaque base head
    u32 muStateField;                     // +0x08           reset on Prepare
    u8  maReserved0C[0x2D0 - 0x0C];       // +0x0C .. +0x2CF rig members not modelled here
    // The adopted parameter block (DWARF :141). SetParameters @0x821F5680 stores it with
    // `stw r31, 0x2D0(this)`. The host pointer is 8 bytes against the console's 4, so the span
    // below is sized from the END of the pointer -- which keeps mbStarted at +0x2F4 on both.
    const Parameters* mpParameters;       // +0x2D0
    u8  maReserved2D8[0x2F4 - 0x2D8];     // +0x2D8 .. +0x2F3 rig members not modelled here
    u8  mbStarted;                        // +0x2F4          latched by Start, asserted clear on entry
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H
