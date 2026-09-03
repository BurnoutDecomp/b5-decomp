// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleParser.cpp
//
// The four Lion member-token tables the particle runtime's endian path walks --
// gLionParticleParserDesTokenTable / ...Beh... / ...Mat... / ...Wave..., the tables
// cLionParticleParser::GetpDescriptorTokenTable / GetpBehaviourTokenTable /
// GetpMaterialTokenTable / GetpWaveFormTokenTable hand out (DecFIGS DWARF
// LionParticleParser.cpp:132-141 names all four; this TU is their DWARF home).
//
// WHAT THEY ARE. Each is a null-terminated sLionMemberToken array describing one Lion
// struct field by field: value TYPE, the flag BIT for a struct/flag token, the byte
// OFFSET inside the struct, and the member's authoring NAME. They are static .rdata in
// the X360 image and are transcribed here VERBATIM with tools/re/x360rd.py from:
//
//   cLionTokenTable @0x82F36A34 -> sLionMemberToken[22] @0x82F34F30   cParticleDescriptor
//   cLionTokenTable @0x82F36A38 -> sLionMemberToken[88] @0x82F35100   cParticleBehaviour
//   cLionTokenTable @0x82F36A3C -> sLionMemberToken[46] @0x82F357F8   cParticleMaterial
//   cLionTokenTable @0x82F36A40 -> sLionMemberToken[13] @0x82F35BC8   cParticleWaveForm
//
// WHY THEY HAD TO BE HOMED. cParticleBehaviour::Delocate @0x8290C9E0 and
// cParticleMaterial::Delocate @0x82909A70 are reconstructed and call
// cLionTokenTable::EndianTwiddle on three of them; both TUs declared the tables `extern`
// with a "not yet homed" flag, so neither could ever LINK. (They also used invented
// names -- gLionParticleBehaviourTokenTable / ...Material... / ...WaveForm... -- which
// this pass replaces with the DWARF's.)
//
// CROSS-CHECK, not decoration: the descriptor table's ten STRUCT tokens carry the flag
// bit each name selects, and every one matches the DecFIGS enum
// cParticleDescriptor::eParticleDescriptorFlag exactly (CELL_RENDER_FLAG 8 ==
// eDO_CELLRENDER, DO_REPEAT 4 == eDO_REPEAT, DYNAMIC_PLACEMENT_FLAG 0x10 ==
// eDO_DYNAMICPLACE, ORIENT_TO_CAMERA_FLAG 0x40 == eDO_FACECAMERA, DO_USE_MATRICES 0x20,
// DO_WORLD_ACC 0x80, DO_IGNORE_ROT 0x100, DO_PHYSICS 0x200, DO_PREFORM 0x4000,
// DISABLED_FLAG 0x8000). Two independent builds agree on the whole set.
//
// The parser half of this TU (the text .lef reader and its cKeyStringTable enum tables)
// is NOT reconstructed here -- only the four member-token tables, which are the data the
// endian path needs. Grow the file additively when the parser lands.
// ============================================================================

// The header must come first: a namespace-scope `const` object has INTERNAL linkage in
// C++ unless a prior `extern const` declaration gives it external linkage, so without
// this include the four tables below would compile but not resolve for any other TU.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleParser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"

// sLionMemberToken::mpString is a non-const char* (DWARF LionTokeniser.h:38) and the
// tokens are static string literals in .rdata, so each initialiser casts once. The
// runtime never writes through it (BuildHashes and EndianTwiddle only read).
#define LTOK(s) const_cast<char*>(s)

// gLionParticleParserDesTokenTable -- the 22 member tokens of cParticleDescriptor.
// X360: cLionTokenTable @0x82F36A34 -> sLionMemberToken[22] @0x82F34F30. Every token's
// mHash is 0 in the image (cLionTokenTable::BuildHashes @0x82908D98 fills them in
// from mpString at init).
static sLionMemberToken gaLionParticleParserDesTokens[] =
{
    { E_LION_MEMBER_STRUCT    , 0x00000008,    32, LTOK("CELL_RENDER_FLAG"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000100,    32, LTOK("DO_IGNORE_ROT"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00004000,    32, LTOK("DO_PREFORM"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000004,    32, LTOK("DO_REPEAT"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000010,    32, LTOK("DYNAMIC_PLACEMENT_FLAG"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000040,    32, LTOK("ORIENT_TO_CAMERA_FLAG"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000020,    32, LTOK("DO_USE_MATRICES"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000080,    32, LTOK("DO_WORLD_ACC"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000200,    32, LTOK("DO_PHYSICS"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00008000,    32, LTOK("DISABLED_FLAG"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    20, LTOK("EMITTER_LIFE_BASE"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,    28, LTOK("EMITTER_LIFE_INFINITE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    24, LTOK("EMITTER_LIFE_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,    36, LTOK("LODGROUP"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,     4, LTOK("PAUSE_TIME"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,     8, LTOK("PAUSE_TIME_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    12, LTOK("REPEAT_TIME"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    16, LTOK("REPEAT_TIME_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,    40, LTOK("RENDERGROUP"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,    56, LTOK("NAME"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x82F369D4,    44, LTOK("SHAPE"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x82F36A14,    48, LTOK("COLLISION_TYPE"), 0x00000000 },
    { E_LION_MEMBER_NONE, 0, 0, 0, 0 },   // terminator: the walk stops on mType == 0
};
const cLionTokenTable gLionParticleParserDesTokenTable = { gaLionParticleParserDesTokens };

// gLionParticleParserBehTokenTable -- the 88 member tokens of cParticleBehaviour.
// X360: cLionTokenTable @0x82F36A38 -> sLionMemberToken[88] @0x82F35100. Every token's
// mHash is 0 in the image (cLionTokenTable::BuildHashes @0x82908D98 fills them in
// from mpString at init).
static sLionMemberToken gaLionParticleParserBehTokens[] =
{
    { E_LION_MEMBER_STRUCT    , 0x00000200,   708, LTOK("DO_BURST"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000100,   708, LTOK("DO_DRAG"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000800,   708, LTOK("DO_INHERITVEL"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000001,   708, LTOK("DO_ROTATE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000008,   708, LTOK("DO_REVERSE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000010,   708, LTOK("DO_RADIAL"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000020,   708, LTOK("DO_OFFSETROT"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000040,   708, LTOK("DO_ROTXYZ"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000080,   708, LTOK("DO_SIZEXYZ"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00001000,   708, LTOK("DO_CLONE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00002000,   708, LTOK("DO_WAVEALPHA"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00004000,   708, LTOK("DO_WAVERGB"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00008000,   708, LTOK("DO_WAVEX"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00010000,   708, LTOK("DO_WAVEY"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00020000,   708, LTOK("DO_WAVEZ"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00040000,   708, LTOK("DO_COLOURSTEP0"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00080000,   708, LTOK("DO_COLOURSTEP1"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00100000,   708, LTOK("DO_COLOURSTEP2"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00200000,   708, LTOK("DO_COLOURSTEP3"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00400000,   708, LTOK("DO_ENDON_SPRITE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00800000,   708, LTOK("DO_ENDON_ACTIVE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x01000000,   708, LTOK("DO_PROPORTIONAL"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x02000000,   708, LTOK("DO_EMITTER_WEIGHTING"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,     0, LTOK("ACC_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,    16, LTOK("ACC_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,    32, LTOK("AXIS_BASE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1128, LTOK("END_ON_ALPHA_FADE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1132, LTOK("END_ON_SCALE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1136, LTOK("END_ON_START_ANGLE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1140, LTOK("END_ON_END_ANGLE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,    48, LTOK("OFFSETROTXYZ_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,    64, LTOK("OFFSETROTXYZ_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,    80, LTOK("OFFSETROTXYZ_VEL_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,    96, LTOK("OFFSETROTXYZ_VEL_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   112, LTOK("OFFSETROTXYZ_ACC_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   128, LTOK("OFFSETROTXYZ_ACC_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   256, LTOK("POS_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   272, LTOK("POS_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   240, LTOK("PIVOT_POINT"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   288, LTOK("RING_RADIUS"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   144, LTOK("ROTXYZ_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   160, LTOK("ROTXYZ_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   176, LTOK("ROTXYZ_VEL_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   192, LTOK("ROTXYZ_VEL_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   208, LTOK("ROTXYZ_ACC_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   224, LTOK("ROTXYZ_ACC_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   304, LTOK("SIZEXYZ_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   320, LTOK("SIZEXYZ_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   336, LTOK("SIZEXYZ_VEL_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   352, LTOK("SIZEXYZ_VEL_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   368, LTOK("SIZEXYZ_ACC_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   384, LTOK("SIZEXYZ_ACC_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   400, LTOK("VEL_BASE"), 0x00000000 },
    { E_LION_MEMBER_QUAT      , 0x00000000,   416, LTOK("VEL_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_COLOUR    , 0x00000000,   544, LTOK("COLOUR0"), 0x00000000 },
    { E_LION_MEMBER_COLOUR    , 0x00000000,   548, LTOK("COLOUR1"), 0x00000000 },
    { E_LION_MEMBER_COLOUR    , 0x00000000,   552, LTOK("COLOUR2"), 0x00000000 },
    { E_LION_MEMBER_COLOUR    , 0x00000000,   556, LTOK("COLOUR3"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   560, LTOK("COLOUR_TIME0"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   564, LTOK("COLOUR_TIME1"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   568, LTOK("COLOUR_TIME2"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   572, LTOK("COLOUR_TIME3"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x82F36A04,   612, LTOK("RGBA_VARIANCE_MODE"), 0x00000000 },
    { E_LION_MEMBER_COLOUR    , 0x00000000,   528, LTOK("RGBA0"), 0x00000000 },
    { E_LION_MEMBER_COLOUR    , 0x00000000,   532, LTOK("RGBA1"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   632, LTOK("ALPHA_FADEIN"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   636, LTOK("ALPHA_FADEOUT"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   640, LTOK("CELL_SIZE"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,  1152, LTOK("RIBBON_PARTICLE_COUNT"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   644, LTOK("CLONE_SCALEIN_TIME"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1156, LTOK("DRAG_FACTOR_VEL"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1160, LTOK("DRAG_FACTOR_ROT"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1164, LTOK("DRAG_FACTOR_SCALE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   648, LTOK("DRAG_FACTOR"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   652, LTOK("MASS"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   680, LTOK("EMISSION_RATE_BASE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   684, LTOK("EMISSION_RATE_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1168, LTOK("EMITTER_START_WEIGHT"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1172, LTOK("EMITTER_END_WEIGHT"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1176, LTOK("EMITTER_VEL_WEIGHT"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,   704, LTOK("EMISSION_COUNT_CLAMP"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,  1124, LTOK("EMISSION_CLAMP_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   688, LTOK("LIFE_BASE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   692, LTOK("LIFE_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   696, LTOK("RADIUS"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   700, LTOK("SCALE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1144, LTOK("TIME_SCALE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,  1148, LTOK("TIME_SCALE_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_NONE, 0, 0, 0, 0 },   // terminator: the walk stops on mType == 0
};
const cLionTokenTable gLionParticleParserBehTokenTable = { gaLionParticleParserBehTokens };

// gLionParticleParserMatTokenTable -- the 46 member tokens of cParticleMaterial.
// X360: cLionTokenTable @0x82F36A3C -> sLionMemberToken[46] @0x82F357F8. Every token's
// mHash is 0 in the image (cLionTokenTable::BuildHashes @0x82908D98 fills them in
// from mpString at init).
static sLionMemberToken gaLionParticleParserMatTokens[] =
{
    { E_LION_MEMBER_U8        , 0x82F369DC,    59, LTOK("ALPHA_TEST_MODE"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x82F369E4,    58, LTOK("BLEND_MODE"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x82F369EC,    61, LTOK("Z_TEST_MODE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000001,    36, LTOK("FLAG_MULTIFRAME"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000002,    36, LTOK("FLAG_INTERFRAMEBLEND"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000004,    36, LTOK("ALPHA_TEST_ENABLE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000010,    36, LTOK("Z_TEST_ENABLE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000020,    36, LTOK("Z_WRITE_ENABLE"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000080,    36, LTOK("FLAG_LAYERGROUP"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000100,    36, LTOK("FLAG_WRAP_U"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00000200,    36, LTOK("FLAG_WRAP_V"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00002000,    36, LTOK("DO_MESH0"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00004000,    36, LTOK("DO_MESH1"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00008000,    36, LTOK("DO_MESH2"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00010000,    36, LTOK("DO_MESH3"), 0x00000000 },
    { E_LION_MEMBER_STRUCT    , 0x00020000,    36, LTOK("DO_MESH4"), 0x00000000 },
    { E_LION_MEMBER_U32       , 0x00000000,    44, LTOK("FRAME_BASE"), 0x00000000 },
    { E_LION_MEMBER_U32       , 0x00000000,    48, LTOK("FRAME_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   156, LTOK("FPS"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   160, LTOK("FPS_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x00000000,    56, LTOK("XFRAMES"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x00000000,    57, LTOK("YFRAMES"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    72, LTOK("RIBBON_TEX_STRETCH"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x00000000,    60, LTOK("ALPHA_TEST_VALUE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   140, LTOK("NORMAL_BLEND_VALUE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   144, LTOK("KEY_LIGHT_VALUE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,   148, LTOK("IBL_VALUE"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,    96, LTOK("MESH0"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,   100, LTOK("MESH1"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,   104, LTOK("MESH2"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,   108, LTOK("MESH3"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,   112, LTOK("MESH4"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,   116, LTOK("MESH_PERCENT0"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,   120, LTOK("MESH_PERCENT1"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,   124, LTOK("MESH_PERCENT2"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,   128, LTOK("MESH_PERCENT3"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x00000000,   132, LTOK("MESH_PERCENT4"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,    28, LTOK("MESH"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,    16, LTOK("TEXTURE"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,    32, LTOK("LAYERGROUPNAME"), 0x00000000 },
    { E_LION_MEMBER_POINTER   , 0x00000000,    24, LTOK("NORMAL_MAP"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x82F36A1C,    65, LTOK("TEX_ANIM_OPTIONS"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x82F36A24,    66, LTOK("SHADER"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x82F36A2C,    67, LTOK("NORMAL_OPTION"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x82F36A0C,    63, LTOK("U_COORD_OPTION"), 0x00000000 },
    { E_LION_MEMBER_U8        , 0x82F36A0C,    64, LTOK("V_COORD_OPTION"), 0x00000000 },
    { E_LION_MEMBER_NONE, 0, 0, 0, 0 },   // terminator: the walk stops on mType == 0
};
const cLionTokenTable gLionParticleParserMatTokenTable = { gaLionParticleParserMatTokens };

// gLionParticleParserWaveTokenTable -- the 13 member tokens of cParticleWaveForm.
// X360: cLionTokenTable @0x82F36A40 -> sLionMemberToken[13] @0x82F35BC8. Every token's
// mHash is 0 in the image (cLionTokenTable::BuildHashes @0x82908D98 fills them in
// from mpString at init).
static sLionMemberToken gaLionParticleParserWaveTokens[] =
{
    { E_LION_MEMBER_F32       , 0x00000000,    20, LTOK("AMP"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,     8, LTOK("BASE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    24, LTOK("CLAMP_MIN"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    28, LTOK("CLAMP_MAX"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    16, LTOK("FREQ"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    12, LTOK("PHASE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    44, LTOK("AMP_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    32, LTOK("BASE_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    48, LTOK("CLAMPMIN_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    52, LTOK("CLAMPMAX_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    40, LTOK("FREQ_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_F32       , 0x00000000,    36, LTOK("PHASE_VARIANCE"), 0x00000000 },
    { E_LION_MEMBER_S32       , 0x82F369F4,     4, LTOK("TYPE"), 0x00000000 },
    { E_LION_MEMBER_NONE, 0, 0, 0, 0 },   // terminator: the walk stops on mType == 0
};
const cLionTokenTable gLionParticleParserWaveTokenTable = { gaLionParticleParserWaveTokens };

#undef LTOK
