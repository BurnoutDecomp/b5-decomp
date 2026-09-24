#ifndef BRN_SIMPLE_PARTICLE_ARRAY_H
#define BRN_SIMPLE_PARTICLE_ARRAY_H

// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h
//
// BrnParticle::Native::BrnSimpleParticleArray -- the platform ("Native") SIMPLE-PARTICLE
// array: one per AttribSys NativeParticleType (impact smoke, crash impact dust and the ten
// skid-smoke surfaces). A simple particle is not simulated: it is a 48-byte spawn record
// (CB4Particle -- where, when, how fast, how big, how opaque) and the renderer evaluates its
// age, path, colour and size analytically every frame from that record and the array's
// parameter block. So the whole CPU side is SPAWN (write one record into a ring) and the
// whole visible side is CB4ParticleBank::Render.
//
// ⭐ 2026-09-24 (FX-CRASHVFX): THE FULL DWARF SHAPE, replacing the "honest placeholder" that
// modelled only AcquireTexture's two fields (mbDirty + mpTextureNameRef). Both of those were
// real members under other names -- the byte at +0x00 is mbIsReady and the pointer at +0x24 is
// mpStandardParams, whose FIRST member is the texture-name pointer AcquireTexture reads -- and
// everything between and after them was opaque, which is why no simple particle could ever be
// spawned: SpawnParticle writes mBankRegular/mBankCrash, which did not exist.
//
// LAYOUT AUTHORITY -- the DecFIGS DWARF (BrnSimpleParticleRenderer.h:168-373), every offset
// confirmed against an ARTIST instruction:
//   BrnSimpleParticleArray (console stride 0xA0 == `mulli 160` in LoadNativeParticleParams,
//                           ParticleModule::Prepare's `addi r29, r29, 0xA0` loop)
//     +0x00 bool mbIsReady                AcquireTexture `stb r8, 0(r30)` (1)
//     +0x04 CB4ParticleBank mBankRegular  SpawnParticle `addi r31, r3, 4`
//     +0x14 CB4ParticleBank mBankCrash    SpawnParticle `addi r31, r3, 0x14`
//     +0x24 CB4ParticleArrayStandardParams* mpStandardParams  Construct `stw ..., 0x24`
//     +0x30 CB4ParticleArrayXenon mParticleData  Initialize's stvx128 at +0x30..+0x90
//   CB4ParticleBank (16 bytes)
//     +0x00 CB4Particle* mpaParticles  +0x04 muNumParticles  +0x08 muNextParticle
//     +0x0C mrLastSpawnTime            (CB4ParticleBank::Construct @0x8227AD30, SpawnParticle)
//   CB4Particle (48 bytes, 16-aligned; SpawnParticle's `mulli 48` cursor)
//   CB4ParticleArrayStandardParams (0x80 bytes: `mulli 128` from &maStandardParams, 0x82FABA80)
//   CB4ParticleArrayXenon (0x70 bytes at array +0x30)
//
// X360 pointers are 32-bit and widen on the host, so the absolute offsets above are the
// console's; every member is reached BY NAME.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                                         // Vector2/3/4, Vector3Plus
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::SafeResourceHandle

namespace CgsMemory { class HeapMalloc; }     // CgsHeapMalloc.h (pointer-only here)
namespace renderengine { class Texture; }      // pointer-only
namespace Attrib { namespace Gen { class nativeparticleparams; } }   // UpdateParams' argument

namespace BrnParticle
{
namespace Native
{
    // AttribSys::Enums::NativeParticleType (DecFIGS NativeParticleType.h:12). The values are the
    // array indices ParticleModule::maSimpleParticles[] and the 13-entry tables below use.
    enum ENativeParticleType
    {
        eParticleArray_None             = 0,
        eParticleArray_ImpactSmoke      = 1,
        eParticleArray_CrashImpactDust  = 2,
        eParticleArray_SkidSmokeNormal  = 3,
        eParticleArray_SkidSmokeGravel  = 4,
        eParticleArray_SkidSmokeDirt    = 5,
        eParticleArray_SkidSmokeSand    = 6,
        eParticleArray_SkidSmokeGrass   = 7,
        eParticleArray_SkidSmokeNormal2 = 8,
        eParticleArray_SkidSmokeGravel2 = 9,
        eParticleArray_SkidSmokeDirt2   = 10,
        eParticleArray_SkidSmokeSand2   = 11,
        eParticleArray_SkidSmokeGrass2  = 12,
        eParticleArray_Max              = 13
    };

    // AttribSys::Enums::ParticleBlend (DecFIGS ParticleBlend.h:12).
    enum EParticleBlend
    {
        eParticleBlendNormal      = 0,
        eParticleBlendSubtractive = 1,
        eParticleBlendAdditive    = 2,
        eParticleBlendMax         = 3
    };

    // BrnSimpleParticleRenderer.h:168 -- one spawn record. Nothing ever moves it: the renderer
    // evaluates the particle at render time from these three quads.
    struct CB4Particle
    {
        rw::math::vpu::Vector3Plus mPositionTime;      // xyz spawn position, w spawn time
        rw::math::vpu::Vector3Plus mVelocityRotation;  // xyz spawn velocity, w rotational velocity (rad/s)
        rw::math::vpu::Vector4     mScaleAlpha;        // (0, 0, size scale, alpha) -- SpawnParticle's stack quad
    };

    // BrnSimpleParticleRenderer.h:78 -- the authored parameter block, one per array type, in the
    // static maStandardParams[13] (0x82FABA80, stride 0x80). Filled by UpdateParams from the
    // array's nativeparticleparams collection. Console offsets in the comments.
    struct CB4ParticleArrayStandardParams
    {
        const char*                 mpacTextureName;             // +0x00
        EParticleBlend              meBlendMode;                 // +0x04
        f32                         mrLifeTime;                  // +0x08
        f32                         mrMidTime;                   // +0x0C
        u8                          mStartColour[4];             // +0x10 RwRGBA (r,g,b,a)
        u8                          mMidColour[4];               // +0x14
        u8                          mEndColour[4];               // +0x18
        f32                         mrStartSize;                 // +0x1C
        f32                         mrMidSize;                   // +0x20
        f32                         mrEndSize;                   // +0x24
        f32                         mrNearClip;                  // +0x28
        f32                         mrFarClip;                   // +0x2C
        f32                         mrNearFade;                  // +0x30
        f32                         mrFarFade;                   // +0x34
        f32                         mrGravity;                   // +0x38
        rw::math::vpu::Vector2      mLightingMinMax;             // +0x40 (a 16-byte VPU Vector2; x min, y max)
        f32                         mrMaxScreenSize;             // +0x50
        f32                         mrRotationSpeedMin;          // +0x54 (revolutions / s)
        f32                         mrRotationSpeedMax;          // +0x58
        f32                         mrDragInitialVelocityScale;  // +0x5C
        f32                         mrDragTerminalVelocityScale; // +0x60
        f32                         mrDragDuration;              // +0x64
        bool                        mbUseDrag;                   // +0x68
        u32                         muTilesWide;                 // +0x6C
        u32                         muTilesHigh;                 // +0x70
    };

    // BrnSimpleParticleRenderer.h:181 -- the render-ready (float) form Initialize derives from
    // the standard parameters. Console offsets are relative to the array (+0x30 base).
    struct CB4ParticleArrayXenon
    {
        rw::math::vpu::Vector4 mStartColour;   // +0x30 (the RwRGBA bytes / 255)
        rw::math::vpu::Vector4 mMidColour;     // +0x40
        rw::math::vpu::Vector4 mEndColour;     // +0x50
        rw::math::vpu::Vector3 mAcceleration;  // +0x60 (0, mrGravity, 0)
        f32                    mrLifeTime;     // +0x70
        f32                    mrMidTime;      // +0x74
        f32                    mrNearFade;     // +0x78
        f32                    mrFarFade;      // +0x7C
        f32                    mrNearClip;     // +0x80
        f32                    mrFarClip;      // +0x84
        rw::math::vpu::Vector4 mSizeParams;    // +0x90 (start, mid, end, max screen size)
    };

    // BrnSimpleParticleRenderer.cpp:82 -- one row of gBrnParticleBankSize[13] (0x82CDAEF8).
    struct BrnParticleBankSize
    {
        ENativeParticleType meParticleType;
        u32                 muRegularBankSize;
        u32                 muCrashBankSize;
    };
    extern const BrnParticleBankSize gBrnParticleBankSize[eParticleArray_Max];

    struct BrnSimpleParticleArray
    {
        // BrnSimpleParticleRenderer.h:286 -- one ring of spawn records.
        struct CB4ParticleBank
        {
            CB4Particle* mpaParticles;     // :342
            u32          muNumParticles;   // :343 (the requested count rounded UP to 16)
            u32          muNextParticle;   // :344 (the ring cursor, counts DOWN)
            f32          mrLastSpawnTime;  // :345 (the newest spawn time in the bank)

            // @0x8227AD30 (DWARF :294). Returns the byte size it asked the heap for.
            u32  Construct(CgsMemory::HeapMalloc* lpHeapMalloc, u32 luNumParticles);
            // DWARF :302 -- inlined into ParticleModule::Prepare @0x8229C2D0..0x8229C398:
            // stamp every record "never spawned" and put the cursor at the top of the ring.
            void Prepare(BrnSimpleParticleArray* lpArray);

            // @0x8291E600 -- evaluate every live record and write its quad. Body in
            // BrnSimpleParticleArray_CB4ParticleBank_Render.cpp.
            void Render(void* lpIterator, BrnSimpleParticleArray* lpArray, void* lpCamera);
        };

        // @0x8228C5A0 (DWARF :226).
        void Construct(CgsMemory::HeapMalloc* lpHeapMalloc, ENativeParticleType leParticleType);

        // @0x8228C6E0 (DWARF :240) -- copy one nativeparticleparams collection into the static
        // standard parameters, then Initialize.
        void UpdateParams(const Attrib::Gen::nativeparticleparams& lParams);

        // @0x8227B000 (DWARF :251).
        bool SpawnParticle(rw::math::vpu::Vector3 lSpawnPosition,
                           rw::math::vpu::Vector3 lSpawnVelocity,
                           f32 lrSpawnTime,
                           f32 lrSizeScale,
                           f32 lrRotationalVelocity,
                           bool lbIsCrash,
                           f32 lrAlpha);

        // @0x8227E468 (DWARF :258). Publish lTexture as this type's texture when this array's
        // own texture name hashes to luRequestedHash; an array with no texture name (only the
        // eParticleArray_None slot) takes the default handle.
        void AcquireTexture(u32 luRequestedHash,
                            CgsResource::SafeResourceHandle<renderengine::Texture> lTexture,
                            ENativeParticleType leArrayID);

        // DWARF :278 / :366 -- the two table reads (inlined on the console).
        static CB4ParticleArrayStandardParams* GetStandardParams(ENativeParticleType leParticleType);
        static renderengine::Texture*          GetTexture(ENativeParticleType leParticleType);

        bool IsReady() const { return mbIsReady; }

        bool                            mbIsReady;          // :348 (+0x00)
        CB4ParticleBank                 mBankRegular;       // :350 (+0x04)
        CB4ParticleBank                 mBankCrash;         // :351 (+0x14)
        CB4ParticleArrayStandardParams* mpStandardParams;   // :353 (+0x24)
        CB4ParticleArrayXenon           mParticleData;      // :354 (+0x30)

    private:
        // @0x8227ADD8 (DWARF :361).
        void Initialize();

        // :372 / :373 -- the two static per-type tables (0x82FAC150 / 0x82FABA80).
        static CgsResource::SafeResourceHandle<renderengine::Texture> maTextures[eParticleArray_Max];
        static CB4ParticleArrayStandardParams                         maStandardParams[eParticleArray_Max];
    };
}
}

#endif
