#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the Start !mbStarted tripwire)

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
    //   field offsets are fabricated. The full parameter layout (the block's actual tunables) lands
    //   with the deathcam rig TU -- model it there when its own Serialise field-walk is attested.
    // ------------------------------------------------------------------------
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` for the camera-tunings serialiser S. Attested
        // EMPTY for the text writer (see the class FLAG): walks zero fields. Templated inline so
        // TextFileWriteSerialiser::Serialise<Parameters>'s odr-use inlines it away, matching the
        // degenerate instantiation asm (no inner field-walk call).
        template<class TSerialiser> void Serialise(TSerialiser& /*lrSerialiser*/) {}
    };

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
    u8  maReserved0C[0x2F4 - 0x0C];       // +0x0C .. +0x2F3 rig members not modelled here
    u8  mbStarted;                        // +0x2F4          latched by Start, asserted clear on entry
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_SPIRALLING_DEATHCAM_H
