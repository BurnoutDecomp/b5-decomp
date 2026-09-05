#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h
//
// cParticleMaterial -- the Lion (eauk_lion) per-material descriptor: the texture / mesh
// handles, frame-animation parameters and the render-state bytes (blend / alpha-test /
// z-test / coord options / shader) for one particle material.
//
// LAYOUT AUTHORITY: the member set, order and types are from the DecFIGS DWARF
// (ParticleMaterial.h:21) and the burnout.wiki "Particle Description" table, and every
// offset the LionParticleRender TU touches is verified against the X360 ARTIST asm:
//   mMaterialHandle  @+0x04  (TextureRegister stores it; SetMaterial reads it)
//   mTextureHandle   @+0x0C  (SetMaterial / FindTexture lookup key)
//   mpNormalMapName  @+0x18  (TextureRegister normal-map path guard)
//   mFlags           @+0x24  (SetMaterial derives the depth-state index from bits 3/5)
//   mBlendMode..mZTestMode @+0x3A..0x3D (HashMaterial FNV-1a input bytes)
//   mUCoordOption    @+0x3F  / mVCoordOption @+0x40 (TextureRegister flip-option asserts)
//   mShader          @+0x42  (GetShaderType -- eSHADER_LION assert in every entry point)
//
// ⭐ 2026-09-03: the nine string links are tLionSerialisedPtr, so the console offsets above
// are ALSO the host offsets (a .lef material is read verbatim), and the static_asserts at
// the bottom of this file check the nine cParticleMaterial::Relocate @0x8290E660 rebases
// word for word. Members are accessed BY NAME, never by raw offset. The method set is gated
// on the X360 ledger: GetShaderType / SetTextureMapHandle / SetNormalMapHandle are attested
// via the LionParticleRender call sites; the rest of the DWARF method set is declared
// (out-of-line) for the material's own TUs.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialisedPtr.h"

typedef float    FP32_t;   // local alias avoided -- use the engine FP32 below.

// Lion scalar spellings (kept local so this header is self-contained when included from
// the LionParticleRender TU, which also pulls ParticleRender.h with the same typedefs;
// the duplicate-typedef rule allows identical typedefs).
#ifndef LION_SCALAR_TYPEDEFS
#define LION_SCALAR_TYPEDEFS
typedef u32  U32;
typedef s32  S32;
typedef u8   U8;
typedef float FP32;
#endif

class cParticleMaterial
{
public:
    // ParticleMaterial.h:35 -- the shader variant a Lion particle material is built for.
    // LionParticleRender only ever accepts eSHADER_LION (the asserts fire otherwise).
    enum eLionMaterialShaderType
    {
        eSHADER_LION      = 0,
        eSHADER_LION_NORM = 1,
        eSHADER_LION_WARP = 2,
        eSHADER_LIMIT     = 3,
    };

    // mFlags (+0x24) bits. ⭐ NOT DERIVED NAMES -- these are the Lion authoring token table's
    // own, read out of the X360 image and transcribed in LionParticleParser.cpp:190/:191 as
    // FLAG_MULTIFRAME 0x1 and FLAG_INTERFRAMEBLEND 0x2 on the +36 == +0x24 flags word.
    // BrnEffects::Utils::BuildUVs @0x822781E0 tests MULTIFRAME to choose the atlas cell and
    // QuadDraw @0x82282330 tests INTERFRAMEBLEND to decide whether to write a blend weight;
    // MultiFrameBehaviour::Process @0x8290FC48 tests INTERFRAMEBLEND twice (`rlwinm r11, r11,
    // 0,30,30` @0x8290FD28 and @0x829100B0) to decide whether the "next" cell is a real second
    // cell or a copy of the current one. Those three users previously each carried their own
    // copy of the constant; this is the flags word's own home.
    enum eMaterialFlags
    {
        eFLAG_MULTIFRAME        = 0x1,
        eFLAG_INTERFRAMEBLEND   = 0x2,
        eFLAG_ALPHA_TEST_ENABLE = 0x4,
        eFLAG_Z_TEST_ENABLE     = 0x10,
        eFLAG_Z_WRITE_ENABLE    = 0x20,
        eFLAG_LAYERGROUP        = 0x80,
        eFLAG_WRAP_U            = 0x100,
        eFLAG_WRAP_V            = 0x200,
        eFLAG_DO_MESH0          = 0x2000,
        eFLAG_DO_MESH1          = 0x4000,
        eFLAG_DO_MESH2          = 0x8000,
        eFLAG_DO_MESH3          = 0x10000,
        eFLAG_DO_MESH4          = 0x20000,
    };

    // ⭐ THE THREE AUTHORED MODE ENUMS, READ OUT OF THE IMAGE -- NOT NAMED BY US AND NOT
    // GUESSED. Each of the three E_LION_MEMBER_U8 tokens in gaLionParticleParserMatTokens
    // (LionParticleParser.cpp:186-188) carries a pointer to a { u32 count; sLionEnumEntry* }
    // header, and each entry is { u32 value; const char* name; u32 pad } -- 12 bytes. The
    // three tables are ALPHA_TEST_MODE @0x82F34C48 (8), BLEND_MODE @0x82F34CA8 (26) and
    // Z_TEST_MODE @0x82F34DE0 (6), dumped with tools/re/x360rd.py. These are the strings a
    // .lef author typed, so they are the material's own vocabulary.
    //
    // ⭐⭐ AND THEY ARE ALSO THE PROOF OF WHAT THE PACKED BLEND WORD MEANS.
    // LionParticleRender::CreateInternalMaterial @0x82280C30 turns each of these into a Xenos
    // RB_BLENDCONTROL, and the factor numbers it writes agree with the NAMES here term for
    // term -- eBLEND_SRCINVALPHA writes (7,6) where eBLEND_SRCALPHA writes (6,7), and
    // eBLEND_DESTINVALPHA writes (11,10) where eBLEND_DESTALPHA writes (10,11). That mirror
    // is what pins 10/11 as DESTALPHA/INVDESTALPHA (see the note over the switch).
    enum eBlendMode
    {
        eBLEND_COPYRGB                = 0,
        eBLEND_SRCALPHA               = 1,
        eBLEND_SRCALPHA_KEEP          = 2,
        eBLEND_COPYALPHA              = 3,
        eBLEND_DESTALPHA              = 4,
        eBLEND_DESTALPHA_KEEP         = 5,
        eBLEND_SRCALPHA_ADD           = 6,
        eBLEND_DESTALPHA_ADD          = 7,
        eBLEND_SRCALPHA_SUB           = 8,
        eBLEND_DESTALPHA_SUB          = 9,
        eBLEND_SRCDEST_5050           = 10,
        eBLEND_SRCALPHA_ADD_KEEP      = 11,
        eBLEND_DESTALPHA_ADD_KEEP     = 12,
        eBLEND_COPYRGB_KEEP           = 13,
        eBLEND_SRCINVALPHA            = 14,
        eBLEND_SRCINVALPHA_KEEP       = 15,
        eBLEND_DESTINVALPHA           = 16,
        eBLEND_DESTINVALPHA_KEEP      = 17,
        eBLEND_SRCALPHA_LIGHTMAP      = 18,
        eBLEND_SRCALPHA_LIGHTMAP_KEEP = 19,
        eBLEND_ADD_SRCALPHA           = 20,
        eBLEND_ADD_SRCALPHA_KEEP      = 21,
        eBLEND_ZWRITE_ONLY            = 22,
        eBLEND_SRCMINUSDEST           = 23,
        eBLEND_DESTMINUSSRC           = 24,
        eBLEND_SRCADDDEST             = 25,
        eBLEND_LIMIT                  = 26,
    };

    // ⚠ THIS IS NOT THE GPU'S COMPARISON ORDER, which is exactly why it needs a translation
    // switch: CreateInternalMaterial's second switch maps each of these onto the Xenos
    // CompareFunction (NEVER 0, LESS 1, EQUAL 2, LEQUAL 3, GREATER 4, NOTEQUAL 5, GEQUAL 6,
    // ALWAYS 7), and that translation lands on the right name for all eight -- an independent
    // confirmation that the state word it builds really is the GPU's alpha-test field.
    enum eAlphaTestMode
    {
        eALPHATEST_NEVER    = 0,
        eALPHATEST_ALWAYS   = 1,
        eALPHATEST_LESS     = 2,
        eALPHATEST_LEQUAL   = 3,
        eALPHATEST_EQUAL    = 4,
        eALPHATEST_GEQUAL   = 5,
        eALPHATEST_GREATER  = 6,
        eALPHATEST_NOTEQUAL = 7,
    };

    enum eZTestMode
    {
        eZTEST_NEVER   = 0,
        eZTEST_ALWAYS  = 1,
        eZTEST_GEQUAL  = 2,
        eZTEST_GREATER = 3,
        eZTEST_LEQUAL  = 4,
        eZTEST_LESS    = 5,
    };

    // ParticleMaterial.h:78 -- the configured shader variant.
    eLionMaterialShaderType GetShaderType() const
    {
        return static_cast<eLionMaterialShaderType>(mShader);
    }

    // ParticleMaterial.h:186 / :202 -- store the resolved texture / normal-map resource
    // handles. Out-of-line in ARTIST (own TUs); declared here, the bodies are reconstructed
    // with the material's TU.
    void SetTextureMapHandle(U32 auHandle);
    void SetNormalMapHandle(U32 auHandle);

    // ---- the material load/serialise path (out-of-line in ARTIST; bodies in ParticleMaterial.cpp) ----
    // Build              @ 0x8290E500 -- finalise after load (mesh count, normal-map promote,
    //                                    texture register, frame count, flag cleanup).
    void Build();
    // Delocate           @ 0x82909A70 -- ptr->offset the owned strings + optional endian twiddle.
    void Delocate(U32 aEndianTwiddleFlag);
    // GetSerialiseSize   @ 0x82909C78 -- add this material's serialised size to the serialiser.
    void GetSerialiseSize(class cLionSerialiser& aSer) const;
    // Relocate           @ 0x8290E660 -- offset->ptr the owned strings (inverse of Delocate).
    void Relocate();
    // Serialise          @ 0x8290E720 -- copy the record + intern each owned string, return the copy.
    cParticleMaterial* Serialise(class cLionSerialiser& aSer) const;

    // ParticleMaterial.h:228+ -- the serialised material record.
    U32   mID;                  // +0x00
    U32   mMaterialHandle;      // +0x04
    U32   mMeshHandle;          // +0x08
    U32   mTextureHandle;       // +0x0C
    tLionSerialisedPtr<char> mpTextureName;     // +0x10  Relocate asm word 4
    U32   mNormalMapHandle;     // +0x14
    tLionSerialisedPtr<char> mpNormalMapName;   // +0x18  Relocate asm word 6
    tLionSerialisedPtr<char> mpMeshName;        // +0x1C  Relocate asm word 7
    tLionSerialisedPtr<char> mpLayerGroupName;  // +0x20  Relocate asm word 8
    U32   mFlags;               // +0x24
    U32   mFrameMask;           // +0x28
    S32   mFrameBase;           // +0x2C
    S32   mFrameVariance;       // +0x30
    S32   mFrameCount;          // +0x34
    U8    mXFrames;             // +0x38
    U8    mYFrames;             // +0x39
    U8    mBlendMode;           // +0x3A
    U8    mAlphaTestMode;       // +0x3B
    U8    mAlphaTestValue;      // +0x3C
    U8    mZTestMode;           // +0x3D
    U8    mPad;                 // +0x3E
    U8    mUCoordOption;        // +0x3F
    U8    mVCoordOption;        // +0x40
    U8    mAnimTexOptions;      // +0x41
    U8    mShader;              // +0x42
    U8    mNormalOption;        // +0x43
    U32   mLayer;               // +0x44
    FP32  mRibbonStretch;       // +0x48
    U32   mMeshHandles[5];      // +0x4C
    tLionSerialisedPtr<char> mpMeshNames[5];    // +0x60  Relocate asm words 24..28
    U32   mPercentages[5];      // +0x74
    U32   mNumMeshes;           // +0x88
    FP32  mNormalBlend;         // +0x8C
    FP32  mKeyLightAmount;      // +0x90
    FP32  mIBLAmount;           // +0x94
    FP32  mZBlendDistance;      // +0x98
    FP32  mFPS;                 // +0x9C
    FP32  mFPSVariance;         // +0xA0

    // mUCoordOption / mVCoordOption option the runtime rejects (a flipped coord option is
    // not supported by the Lion blend renderer).
    enum eUVOption
    {
        eUVOPTION_NORMAL  = 0,
        eUVOPTION_FLIPPED = 1,
    };
};

// ⛔ CORRECTION 2026-09-03. The wave that measured the .lef fault reported this record as
// "172 vs 164 -- needs a layout look". It does not: cParticleMaterial::Serialise
// @0x8290E720 does `cLionSerialiser::DataStore(a2, a1, 164)`, and every one of the nine
// offsets Relocate @0x8290E660 rebases (asm word indices 4, 6, 7, 8 and 24..28) matches
// the member set below exactly, as does Serialise's own `v14 = v13 + 96` mesh-name walk.
// The record that was four bytes short was cParticleBehaviour, for an unrelated reason
// (cVector alignment -- see ParticleBehaviour.h).
static_assert(sizeof(cParticleMaterial) == 164,
              "cParticleMaterial is the 164-byte serialised record "
              "(cParticleMaterial::Serialise @0x8290E720 DataStore(this, 164))");
static_assert(offsetof(cParticleMaterial, mpTextureName)    == 0x10, "Relocate asm word 4");
static_assert(offsetof(cParticleMaterial, mpNormalMapName)  == 0x18, "Relocate asm word 6");
static_assert(offsetof(cParticleMaterial, mpMeshName)       == 0x1C, "Relocate asm word 7");
static_assert(offsetof(cParticleMaterial, mpLayerGroupName) == 0x20, "Relocate asm word 8");
static_assert(offsetof(cParticleMaterial, mFlags)           == 0x24, "SetMaterial depth state");
static_assert(offsetof(cParticleMaterial, mBlendMode)       == 0x3A, "HashMaterial FNV-1a run");
static_assert(offsetof(cParticleMaterial, mShader)          == 0x42, "GetShaderType");
static_assert(offsetof(cParticleMaterial, mpMeshNames)      == 0x60, "Relocate asm words 24..28");
