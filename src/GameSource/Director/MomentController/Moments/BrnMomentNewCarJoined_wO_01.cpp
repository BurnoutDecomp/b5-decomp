#include "GameSource/Director/MomentController/Moments/BrnMomentNewCarJoined.h"

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// @0x8225F408 -- cpp:34. Seed the moment: the inlined base construct, the three
// behaviour-handle clears (the loose-attachment one is stored TWICE by the
// X360), the interpolate blend style, the loose-attachment parameter defaults,
// and the cleared parameter pointer.
// ----------------------------------------------------------------------------
void MomentNewCarJoined::Construct()
{
    // The inlined base (0x8225F424..0x8225F448): stw 0,0x174 (meState = INACTIVE);
    // the live-vtable slot 7 call `lwz r11,0x1C(vtbl); bctrl` (GetInstanceType ->
    // 10) stored to 0x170 (meType); stb 0,0x17B (mbIsInhibited = false); and
    // `bl Camera::Camera::Construct` on this+0x10 (mCamera).
    Moment::Construct();

    mInterpolaterA.Clear();     // stb 0,0x184; stw 0 -> 0x194/0x190/0x18C/0x188
    mInterpolaterB.Clear();     // stb 0,0x198; stw 0 -> 0x1A8/0x1A4/0x1A0/0x19C
    mLooseAttachment.Clear();   // stb 0,0x1AC; stw 0 -> 0x1BC/0x1B8/0x1B4/0x1B0

    // The committed Parameters::Construct inline: stw 0,0x1C4 (debug name),
    // stw 8,0x1C0 (mType), stw 1,0x1CC (mapping = SINUSOIDAL),
    // stw 0,0x1C8 (method = SLERP).
    mInterpolateParams.Construct();
    // ...immediately overwritten: stw 1,0x1C8 and stw 3,0x1CC. Blend the camera
    // by rotating about the player car, on the exponential-out-x-cubed curve.
    // INFERENCE (inherited from BrnBehaviourInterpolate.h's own flag): the
    // method/mapping LABELS on +0x08/+0x0C are unproven -- which of the two is
    // "method" and which is "mapping" is not attested. Only the VALUES 1 and 3
    // and their offsets 0x1C8/0x1CC are measured.
    mInterpolateParams.meInterpolationMethod =
        Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR;   // 0x1C8 = 1
    mInterpolateParams.meInterpolationMapping =
        Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;  // 0x1CC = 3

    // The X360 emits the loose-attachment handle clear a SECOND time
    // (0x8225F4BC..0x8225F4CC, byte-for-byte the same five stores as above).
    // Redundant, but faithful -- kept.
    mLooseAttachment.Clear();

    // bl @0x8225F4D0 with r3 = this + 0x1D0 (== &mLooseAttachmentParameters).
    mLooseAttachmentParameters.Construct();

    mpParameters = 0;                                  // stw r8(=0), 0x180

    // The loose-attachment framing defaults, in the X360's store order:
    mLooseAttachmentParameters.mfHeight   = 0.75f;     // flt_82004018 -> 0x21C (params +0x4C)
    mLooseAttachmentParameters.mfField54  = 40.0f;     // flt_82004D0C -> 0x224 (params +0x54)
    mLooseAttachmentParameters.mfDistance = 3.0f;      // flt_82004270 -> 0x220 (params +0x50)
}

}
