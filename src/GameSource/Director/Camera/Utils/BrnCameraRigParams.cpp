// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraRigParams.cpp
//
// The twenty authored BrnDirector::Camera::Utils::CameraRig presets (DWARF BrnCameraRig.h:63..:82,
// defined at BrnCameraRigParams.cpp:21..:287, fourteen lines apart). [FX-DIRECTOR2 2026-09-25]
//
// WHERE THE VALUES COME FROM (ARTIST .data, one 0x40-byte CameraRig::Params each, 0x82CDA810..0x82CDAD10):
//   * the two Vector3s (+0x00 mOffsetFromTarget, +0x10 mOffsetFromRotationCentre) are DYNAMIC-initialised: each
//     preset has its own CRT init thunk (0x82C48588 .. 0x82C48EE8, 0x80 apart) that builds the two vectors on its
//     stack from .rdata floats (each lane's source is cited below; the w lanes are `stw 0`) and stores them with
//     two stvx128. The static image of both vectors is zero.
//   * mfFOV / mfRoll / mfPitch / mfYaw / mbWidescreenOnly (+0x20..+0x30) are STATIC data, read straight from the
//     image (the word each one came from is quoted).
// NAMES. The PS3 build names three of them at their use sites -- BehaviourRig::Parameters::Construct
// (ParamsFrontQuarterClose, X360 0x82CDA890), MomentTakedownLookback::Update (ParamsBonnetLow, X360 0x82CDAA10) and
// BehaviourParameterBank::Construct (nine more, in the same order as the X360's nine loads 0x8223E27C..
// 0x8223E49C). Those pin the storage order: it is the DEFINITION order of this file, which is the header's order
// except that ParamsRearQFwd (BrnCameraRigParams.cpp:175 in the DWARF) sits between SideLookingForwards and
// RigFrontQBwd -- the bank's RearQFwd load hits 0x82CDAAD0, slot 11.
// Every float below round-trips to the exact word in the image (checked by generating the literals from the
// words); -0.0f is kept where the image has 0x80000000.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"   // Utils::CameraRig (the presets' home class)

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// ParamsRearLongFlat @0x82CDA810 (vectors: CRT thunk 0x82C48588, lanes flt_8200D534,flt_82004740,flt_8200D530 / flt_82001CC0,flt_82001CC0,flt_8200D52C)
CameraRig::Params CameraRig::ParamsRearLongFlat =
{
    { 0.875f, 0.3f, -0.1f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.55f, 0.0f },   // mOffsetFromRotationCentre
    50.0f,   // mfFOV    (0x42480000)
    1.5f,   // mfRoll   (0x3FC00000)
    7.7f,   // mfPitch  (0x40F66666)
    711.0f,   // mfYaw    (0x4431C000)
    false    // mbWidescreenOnly
};

// ParamsFrontQuarterLong @0x82CDA850 (vectors: CRT thunk 0x82C48608, lanes flt_8200D540,flt_8200D53C,flt_820047C8 / flt_82001CC0,flt_82001CC0,flt_8200D538)
CameraRig::Params CameraRig::ParamsFrontQuarterLong =
{
    { 0.49f, 0.18f, 0.05f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.5f, 0.0f },   // mOffsetFromRotationCentre
    63.5f,   // mfFOV    (0x427E0000)
    1.5f,   // mfRoll   (0x3FC00000)
    16.2f,   // mfPitch  (0x4181999A)
    577.0f,   // mfYaw    (0x44104000)
    false    // mbWidescreenOnly
};

// ParamsFrontQuarterClose @0x82CDA890 (vectors: CRT thunk 0x82C48688, lanes flt_8200D550,flt_8200D54C,flt_8200D548 / flt_82001CC0,flt_82001CC0,flt_8200D544)
CameraRig::Params CameraRig::ParamsFrontQuarterClose =
{
    { -0.28f, -0.11f, -0.56f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.6f, 0.0f },   // mOffsetFromRotationCentre
    73.5f,   // mfFOV    (0x42930000)
    12.0f,   // mfRoll   (0x41400000)
    9.6f,   // mfPitch  (0x4119999A)
    148.0f,   // mfYaw    (0x43140000)
    false    // mbWidescreenOnly
};

// ParamsFrontQuarterCloseDeep @0x82CDA8D0 (vectors: CRT thunk 0x82C48708, lanes flt_8200D550,flt_8200D55C,flt_8200D558 / flt_82001CC0,flt_82001CC0,flt_8200D554)
CameraRig::Params CameraRig::ParamsFrontQuarterCloseDeep =
{
    { -0.28f, -0.21f, -0.61f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.42f, 0.0f },   // mOffsetFromRotationCentre
    86.37f,   // mfFOV    (0x42ACBD71)
    -19.37f,   // mfRoll   (0xC19AF5C3)
    11.0f,   // mfPitch  (0x41300000)
    -147.3f,   // mfYaw    (0xC3134CCD)
    true    // mbWidescreenOnly
};

// ParamsHighSideFlat @0x82CDA910 (vectors: CRT thunk 0x82C48788, lanes flt_8200D564,flt_8200D560,flt_820047C8 / flt_82001CC0,flt_82001CC0,flt_8200D538)
CameraRig::Params CameraRig::ParamsHighSideFlat =
{
    { -0.8f, 1.65f, 0.05f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.5f, 0.0f },   // mOffsetFromRotationCentre
    74.5f,   // mfFOV    (0x42950000)
    0.0f,   // mfRoll   (0x00000000)
    55.0f,   // mfPitch  (0x425C0000)
    90.0f,   // mfYaw    (0x42B40000)
    false    // mbWidescreenOnly
};

// ParamsSideFlat @0x82CDA950 (vectors: CRT thunk 0x82C48808, lanes flt_82001CC0,flt_82001CC0,flt_82001CC0 / flt_82001CC0,flt_82001CC0,flt_8200D568)
CameraRig::Params CameraRig::ParamsSideFlat =
{
    { 0.0f, 0.0f, 0.0f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -2.5f, 0.0f },   // mOffsetFromRotationCentre
    70.0f,   // mfFOV    (0x428C0000)
    -0.0f,   // mfRoll   (0x80000000)
    0.0f,   // mfPitch  (0x00000000)
    -90.0f,   // mfYaw    (0xC2B40000)
    false    // mbWidescreenOnly
};

// ParamsBonnetHigh @0x82CDA990 (vectors: CRT thunk 0x82C48870, lanes flt_8200D564,flt_8200D560,flt_82003F40 / flt_82001CC0,flt_82001CC0,flt_8200D538)
CameraRig::Params CameraRig::ParamsBonnetHigh =
{
    { -0.8f, 1.65f, 0.25f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.5f, 0.0f },   // mOffsetFromRotationCentre
    74.5f,   // mfFOV    (0x42950000)
    0.0f,   // mfRoll   (0x00000000)
    55.0f,   // mfPitch  (0x425C0000)
    90.0f,   // mfYaw    (0x42B40000)
    false    // mbWidescreenOnly
};

// ParamsBootHigh @0x82CDA9D0 (vectors: CRT thunk 0x82C488F0, lanes flt_8200D564,flt_8200D560,flt_8200D56C / flt_82001CC0,flt_82001CC0,flt_8200D538)
CameraRig::Params CameraRig::ParamsBootHigh =
{
    { -0.8f, 1.65f, -0.25f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.5f, 0.0f },   // mOffsetFromRotationCentre
    74.5f,   // mfFOV    (0x42950000)
    0.0f,   // mfRoll   (0x00000000)
    55.0f,   // mfPitch  (0x425C0000)
    90.0f,   // mfYaw    (0x42B40000)
    false    // mbWidescreenOnly
};

// ParamsBonnetLow @0x82CDAA10 (vectors: CRT thunk 0x82C48970, lanes flt_82004014,flt_82004014,flt_82003F40 / flt_82001CC0,flt_82001CC0,flt_8200D538)
CameraRig::Params CameraRig::ParamsBonnetLow =
{
    { 0.1f, 0.1f, 0.25f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.5f, 0.0f },   // mOffsetFromRotationCentre
    74.5f,   // mfFOV    (0x42950000)
    0.0f,   // mfRoll   (0x00000000)
    5.0f,   // mfPitch  (0x40A00000)
    90.0f,   // mfYaw    (0x42B40000)
    false    // mbWidescreenOnly
};

// ParamsFrontQCuFwd @0x82CDAA50 (vectors: CRT thunk 0x82C489E8, lanes flt_8200D57C,flt_8200D578,flt_8200D574 / flt_82001CC0,flt_82001CC0,flt_8200D570)
CameraRig::Params CameraRig::ParamsFrontQCuFwd =
{
    { 0.24f, -0.46f, 1.45f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.4f, 0.0f },   // mOffsetFromRotationCentre
    104.0f,   // mfFOV    (0x42D00000)
    15.5f,   // mfRoll   (0x41780000)
    18.1f,   // mfPitch  (0x4190CCCD)
    -14.13f,   // mfYaw    (0xC162147B)
    false    // mbWidescreenOnly
};

// ParamsSideLookingForwards @0x82CDAA90 (vectors: CRT thunk 0x82C48A68, lanes flt_8200D534,flt_82004740,flt_8200D584 / flt_82001CC0,flt_82001CC0,flt_8200D580)
CameraRig::Params CameraRig::ParamsSideLookingForwards =
{
    { 0.875f, 0.3f, -0.0f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -0.55f, 0.0f },   // mOffsetFromRotationCentre
    60.0f,   // mfFOV    (0x42700000)
    1.5f,   // mfRoll   (0x3FC00000)
    7.7f,   // mfPitch  (0x40F66666)
    0.0f,   // mfYaw    (0x00000000)
    false    // mbWidescreenOnly
};

// ParamsRearQFwd @0x82CDAAD0 (vectors: CRT thunk 0x82C48AE8, lanes flt_8200D590,flt_82005574,flt_8200D58C / flt_82001CC0,flt_82001CC0,flt_8200D588)
CameraRig::Params CameraRig::ParamsRearQFwd =
{
    { -0.47f, 0.02f, 0.94f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.7f, 0.0f },   // mOffsetFromRotationCentre
    110.0f,   // mfFOV    (0x42DC0000)
    8.0f,   // mfRoll   (0x41000000)
    27.72f,   // mfPitch  (0x41DDC28F)
    -32.61f,   // mfYaw    (0xC20270A4)
    false    // mbWidescreenOnly
};

// ParamsRigFrontQBwd @0x82CDAB10 (vectors: CRT thunk 0x82C48B68, lanes flt_82003F40,flt_8200D598,flt_8200D594 / flt_82001CC0,flt_82001CC0,flt_8200D544)
CameraRig::Params CameraRig::ParamsRigFrontQBwd =
{
    { 0.25f, -0.058f, -0.716f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.6f, 0.0f },   // mOffsetFromRotationCentre
    95.5f,   // mfFOV    (0x42BF0000)
    19.0f,   // mfRoll   (0x41980000)
    13.73f,   // mfPitch  (0x415BAE14)
    150.36f,   // mfYaw    (0x43165C29)
    false    // mbWidescreenOnly
};

// ParamsFrontRearview @0x82CDAB50 (vectors: CRT thunk 0x82C48BE8, lanes flt_8200D5A0,flt_820047C8,flt_8200D59C / flt_82001CC0,flt_82001CC0,flt_820037C8)
CameraRig::Params CameraRig::ParamsFrontRearview =
{
    { 0.13f, 0.05f, -0.57f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.0f, 0.0f },   // mOffsetFromRotationCentre
    103.0f,   // mfFOV    (0x42CE0000)
    9.5f,   // mfRoll   (0x41180000)
    18.79f,   // mfPitch  (0x419651EC)
    -220.0f,   // mfYaw    (0xC35C0000)
    false    // mbWidescreenOnly
};

// ParamsBootViewFwd @0x82CDAB90 (vectors: CRT thunk 0x82C48C68, lanes flt_8200D5B0,flt_8200D5AC,flt_8200D5A8 / flt_82001CC0,flt_82001CC0,flt_8200D5A4)
CameraRig::Params CameraRig::ParamsBootViewFwd =
{
    { -0.08f, 0.66f, 0.39f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -0.9f, 0.0f },   // mOffsetFromRotationCentre
    97.5f,   // mfFOV    (0x42C30000)
    6.5f,   // mfRoll   (0x40D00000)
    27.21f,   // mfPitch  (0x41D9AE14)
    -7.32f,   // mfYaw    (0xC0EA3D71)
    false    // mbWidescreenOnly
};

// ParamsFrontQLowBwd @0x82CDABD0 (vectors: CRT thunk 0x82C48CE8, lanes flt_8200D5BC,flt_8200D5B8,flt_8200D5B4 / flt_82001CC0,flt_82001CC0,flt_82004C78)
CameraRig::Params CameraRig::ParamsFrontQLowBwd =
{
    { 0.476f, -0.12f, -0.06f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -0.5f, 0.0f },   // mOffsetFromRotationCentre
    78.5f,   // mfFOV    (0x429D0000)
    -22.5f,   // mfRoll   (0xC1B40000)
    -3.14f,   // mfPitch  (0xC048F5C3)
    -163.03f,   // mfYaw    (0xC32307AE)
    false    // mbWidescreenOnly
};

// ParamsRoofFwd @0x82CDAC10 (vectors: CRT thunk 0x82C48D68, lanes flt_8200D5C8,flt_8200D5C4,flt_8200D5C0 / flt_82001CC0,flt_82001CC0,flt_820037C8)
CameraRig::Params CameraRig::ParamsRoofFwd =
{
    { -0.02f, 0.44f, 0.86f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.0f, 0.0f },   // mOffsetFromRotationCentre
    87.0f,   // mfFOV    (0x42AE0000)
    7.0f,   // mfRoll   (0x40E00000)
    19.37f,   // mfPitch  (0x419AF5C3)
    -0.64f,   // mfYaw    (0xBF23D70A)
    false    // mbWidescreenOnly
};

// ParamsBootFwd @0x82CDAC50 (vectors: CRT thunk 0x82C48DE8, lanes flt_8200D5D4,flt_8200D5D0,flt_8200D5CC / flt_82001CC0,flt_82001CC0,flt_8200D580)
CameraRig::Params CameraRig::ParamsBootFwd =
{
    { -0.172f, 0.667f, -0.221f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -0.55f, 0.0f },   // mOffsetFromRotationCentre
    81.5f,   // mfFOV    (0x42A30000)
    -19.5f,   // mfRoll   (0xC19C0000)
    20.71f,   // mfPitch  (0x41A5AE14)
    22.66f,   // mfYaw    (0x41B547AE)
    false    // mbWidescreenOnly
};

// ParamsFrontQCuFwd2 @0x82CDAC90 (vectors: CRT thunk 0x82C48E68, lanes flt_8200D5E0,flt_8200D5DC,flt_8200D5D8 / flt_82001CC0,flt_82001CC0,flt_8200D564)
CameraRig::Params CameraRig::ParamsFrontQCuFwd2 =
{
    { 0.17f, 0.014f, 0.553f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -0.8f, 0.0f },   // mOffsetFromRotationCentre
    84.0f,   // mfFOV    (0x42A80000)
    17.0f,   // mfRoll   (0x41880000)
    15.03f,   // mfPitch  (0x41707AE1)
    -34.56f,   // mfYaw    (0xC20A3D71)
    false    // mbWidescreenOnly
};

// ParamsUnderbelly @0x82CDACD0 (vectors: CRT thunk 0x82C48EE8, lanes flt_8200D5EC,flt_8200D5E8,flt_8200D5E4 / flt_82001CC0,flt_82001CC0,flt_8200D544)
CameraRig::Params CameraRig::ParamsUnderbelly =
{
    { -0.021f, -1.179f, 1.525f, 0.0f },   // mOffsetFromTarget
    { 0.0f, 0.0f, -1.6f, 0.0f },   // mOffsetFromRotationCentre
    95.5f,   // mfFOV    (0x42BF0000)
    -11.0f,   // mfRoll   (0xC1300000)
    16.61f,   // mfPitch  (0x4184E148)
    0.18f,   // mfYaw    (0x3E3851EC)
    false    // mbWidescreenOnly
};

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
