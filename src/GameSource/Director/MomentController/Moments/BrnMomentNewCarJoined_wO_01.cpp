#include "GameSource/Director/MomentController/Moments/BrnMomentNewCarJoined.h"

namespace BrnDirector
{

// Seed the moment: the inlined base construct, the three
// behaviour-handle clears (the loose-attachment one is stored TWICE by the
// console), the interpolate blend style, the loose-attachment parameter defaults,
// and the cleared parameter pointer.
void MomentNewCarJoined::Construct()
{
    // The inlined base: 0 -> +0x174 (meState = INACTIVE); the live-vtable slot 7
    // call (GetInstanceType -> 10) stored to +0x170 (meType); 0 -> +0x17B
    // (mbIsInhibited = false); and Camera::Camera::Construct on +0x10 (mCamera).
    Moment::Construct();

    mInterpolaterA.Clear();     // zeroes +0x184 and +0x188/0x18C/0x190/0x194
    mInterpolaterB.Clear();     // zeroes +0x198 and +0x19C/0x1A0/0x1A4/0x1A8
    mLooseAttachment.Clear();   // zeroes +0x1AC and +0x1B0/0x1B4/0x1B8/0x1BC

    // The committed Parameters::Construct inline: 0 -> +0x1C4 (debug name),
    // 8 -> +0x1C0 (mType), 1 -> +0x1CC (mapping = SINUSOIDAL),
    // 0 -> +0x1C8 (method = SLERP).
    mInterpolateParams.Construct();
    // ...immediately overwritten: 1 -> +0x1C8 and 3 -> +0x1CC. Blend the camera
    // by rotating about the player car, on the exponential-out-x-cubed curve.
    // INFERENCE (inherited from BrnBehaviourInterpolate.h's own flag): the
    // method/mapping LABELS on +0x08/+0x0C are unproven -- which of the two is
    // "method" and which is "mapping" is not attested. Only the VALUES 1 and 3
    // and their offsets 0x1C8/0x1CC are measured.
    mInterpolateParams.meInterpolationMethod =
        Camera::BehaviourInterpolate::E_METHOD_ROTATE_ABOUT_PLAYER_CAR;   // 0x1C8 = 1
    mInterpolateParams.meInterpolationMapping =
        Camera::BehaviourInterpolate::E_MAPPING_EXPONENTIAL_OUT_X_CUBED;  // 0x1CC = 3

    // The console emits the loose-attachment handle clear a SECOND time
    // (byte-for-byte the same five stores as above).
    // Redundant, but faithful -- kept.
    mLooseAttachment.Clear();

    // Called on this + 0x1D0 (== &mLooseAttachmentParameters).
    mLooseAttachmentParameters.Construct();

    mpParameters = 0;                                  // 0 -> +0x180

    // The loose-attachment framing defaults, in the console build's store order:
    mLooseAttachmentParameters.mfHeight   = 0.75f;     // -> +0x21C (params +0x4C)
    mLooseAttachmentParameters.mfField54  = 40.0f;     // -> +0x224 (params +0x54)
    mLooseAttachmentParameters.mfDistance = 3.0f;      // -> +0x220 (params +0x50)
}

}
