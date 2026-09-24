// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleArray.cpp
//
// BrnParticle::Native::BrnSimpleParticleArray -- the CPU half of the native simple-particle
// family (impact smoke, crash impact dust, the ten skid-smoke surfaces). Reconstructed from
// BURNOUT_X360_ARTIST.XEX (the console's home is BrnSimpleParticleRenderer.cpp -- DWARF):
//
//   BrnSimpleParticleArray::CB4ParticleBank::Construct  @0x8227AD30   (DWARF :279)
//   BrnSimpleParticleArray::CB4ParticleBank::Prepare     inlined into ParticleModule::Prepare
//                                                        @0x8229C2D0..0x8229C398 (DWARF :328)
//   BrnSimpleParticleArray::Initialize                  @0x8227ADD8   (DWARF :887)
//   BrnSimpleParticleArray::Construct                   @0x8228C5A0   (DWARF :926)
//   BrnSimpleParticleArray::UpdateParams                @0x8228C6E0   (DWARF :1014)
//   BrnSimpleParticleArray::SpawnParticle               @0x8227B000   (DWARF :1069)
//   BrnSimpleParticleArray::AcquireTexture              @0x8227E468
//   BrnSimpleParticleArray::GetTexture / GetStandardParams  (inlined table reads)
//
// 2026-09-24 (FX-CRASHVFX). Before this change only AcquireTexture had a body, against a
// two-field placeholder, and the file was not on the link at all -- so ParticleModule::Prepare,
// LoadFXBundle stage 12 and EffectsModule::LoadNativeParticleParams each announced their simple-
// particle leg instead of running it, and every SpawnSimple / SpawnWheelSmoke producer had
// nowhere to write. See BrnSimpleParticleArray.h for the recovered layout.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "GameSource/AttribSys/Generated/classes/nativeparticleparams.h"   // Attrib::Gen::nativeparticleparams
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"                   // CgsMemory::HeapMalloc::Malloc
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"             // TextureNameMap::Entry::HashString
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT

namespace BrnParticle
{
namespace Native
{
    namespace
    {
        // flt_82010C1C == 0x3B808081 -- the 1/255 Initialize scales each RwRGBA byte by.
        const f32 KF_COLOUR_BYTE_TO_UNIT = 0.0039215689f;
        // flt_82010C20 == 0x437F0000 -- the 255.0 UpdateParams scales each colour lane by.
        const f32 KF_COLOUR_UNIT_TO_BYTE = 255.0f;
        // flt_8200D970 == 0x40C90FDB -- SpawnParticle's revolutions/s -> radians/s factor.
        const f32 KF_TWO_PI = 6.2831855f;
        // flt_82010CA4 == 0xC61C3C00 -- the "never spawned" time ParticleModule::Prepare stamps
        // into every record and into each bank's mrLastSpawnTime (CB4ParticleBank::Prepare).
        const f32 KF_NEVER_SPAWNED_TIME = -9999.0f;
        // flt_82001CC0 == 0.0f -- the zero lanes of the acceleration and the scale/alpha quads.
        const f32 KF_ZERO = 0.0f;
        // The bank ring is sized in whole blocks of 16 (`addi r9, r11, 0xF ; clrrwi r9, r9, 4`).
        const u32 KU_BANK_GRANULE = 16u;
        // The heap alignment CB4ParticleBank::Construct asks for (`li r5, 0x10`).
        const s32 KI_PARTICLE_ALIGNMENT = 16;

        // BrnSimpleParticleRenderer.cpp:118 -- Vector4 Convert(const RwRGBA&): each byte through
        // fcfid/frsp, then fmuls by 1/255 (Initialize @0x8227ADF4..0x8227AE58, three times).
        rw::math::vpu::Vector4 ConvertColour(const u8 laColour[4])
        {
            rw::math::vpu::Vector4 lResult;
            lResult.x = static_cast<f32>(laColour[0]) * KF_COLOUR_BYTE_TO_UNIT;
            lResult.y = static_cast<f32>(laColour[1]) * KF_COLOUR_BYTE_TO_UNIT;
            lResult.z = static_cast<f32>(laColour[2]) * KF_COLOUR_BYTE_TO_UNIT;
            lResult.w = static_cast<f32>(laColour[3]) * KF_COLOUR_BYTE_TO_UNIT;
            return lResult;
        }

        // BrnSimpleParticleRenderer.cpp:108 -- void Convert(const Vector4&, RwRGBA&): each lane
        // times 255 (fmuls), `fctidz` (truncate to a 64-bit integer), then the LOW byte of that
        // doubleword (`lbz +7` on the big-endian stack slot). A lane above 1.0 therefore WRAPS
        // modulo 256 rather than saturating; that is the console's store and it is kept.
        void ConvertColour(const rw::math::vpu::Vector4& lrColour, u8 laColour[4])
        {
            laColour[0] = static_cast<u8>(static_cast<s64>(lrColour.x * KF_COLOUR_UNIT_TO_BYTE));
            laColour[1] = static_cast<u8>(static_cast<s64>(lrColour.y * KF_COLOUR_UNIT_TO_BYTE));
            laColour[2] = static_cast<u8>(static_cast<s64>(lrColour.z * KF_COLOUR_UNIT_TO_BYTE));
            laColour[3] = static_cast<u8>(static_cast<s64>(lrColour.w * KF_COLOUR_UNIT_TO_BYTE));
        }

        // dword_82FACBA0 / dword_82FACBA4 -- the handle AcquireTexture publishes for an array
        // with no texture name. ⚠ NOTHING IN THE IMAGE WRITES IT: findinit.py over 0x82FACBA0
        // returns exactly one site, AcquireTexture's own read at 0x8227E500, and x360rd reads
        // both words as 0. It is a .bss handle that stays null (static storage: zero-initialised).
        CgsResource::SafeResourceHandle<renderengine::Texture> gsDefaultTexture;
    }

    // gBrnParticleBankSize @0x82CDAEF8 (13 x {type, regular, crash}, read with x360rd.py). Every
    // crash bank is empty on this build; the regular banks are 200/400 for the two crash smokes
    // and 512/256 for the skid smokes.
    const BrnParticleBankSize gBrnParticleBankSize[eParticleArray_Max] =
    {
        { eParticleArray_None,               0, 0 },
        { eParticleArray_ImpactSmoke,      200, 0 },
        { eParticleArray_CrashImpactDust,  400, 0 },
        { eParticleArray_SkidSmokeNormal,  512, 0 },
        { eParticleArray_SkidSmokeGravel,  256, 0 },
        { eParticleArray_SkidSmokeDirt,    256, 0 },
        { eParticleArray_SkidSmokeSand,    256, 0 },
        { eParticleArray_SkidSmokeGrass,   256, 0 },
        { eParticleArray_SkidSmokeNormal2, 512, 0 },
        { eParticleArray_SkidSmokeGravel2, 256, 0 },
        { eParticleArray_SkidSmokeDirt2,   256, 0 },
        { eParticleArray_SkidSmokeSand2,   256, 0 },
        { eParticleArray_SkidSmokeGrass2,  256, 0 },
    };

    // [DIAG] NOT IN THE X360 BINARY -- the ladder's first rung (see the header). DELETE-WHEN-STABLE.
    u32 gauSimpleParticleSpawned = 0;

    // The two static per-type tables (DWARF :372 / :373). maTextures @0x82FAC150 (13 x 8-byte
    // handles), maStandardParams @0x82FABA80 (13 x 0x80). Zero-initialised .bss on the console.
    CgsResource::SafeResourceHandle<renderengine::Texture> BrnSimpleParticleArray::maTextures[eParticleArray_Max];
    CB4ParticleArrayStandardParams                         BrnSimpleParticleArray::maStandardParams[eParticleArray_Max];

    // =========================================================================================
    // CB4ParticleBank::Construct @0x8227AD30.
    //   muNextParticle = 0; muNumParticles = (count + 15) & ~15;
    //   count == 0 -> mpaParticles = NULL, return 0;
    //   else mpaParticles = Malloc(count * 48, 16) (asserted), return count * 48.
    //
    // ⚠ CONSOLE-QUIRK, AND THE ONE PC DEVIATION IN THIS FILE: the console asks the heap for
    // `count * 48` bytes (`slwi r10, r11, 1 ; add r11, r11, r10 ; slwi r30, r11, 4`) but sizes
    // the ring by the ROUNDED count, and both SpawnParticle's cursor and the Prepare stamp walk
    // the rounded count. For every array but one the count is already a multiple of 16; for
    // eParticleArray_ImpactSmoke it is 200, so the console's ring is 208 records over a
    // 200-record block and writes 8 records (384 bytes) past the end of its allocation --
    // silently, into whatever the heap put next. On the host that same write corrupts the CRT
    // heap, so the block is sized to the ring the cursor actually walks (muNumParticles
    // records). Nothing the console computes changes: the ring length, the cursor sequence,
    // the stamp and the returned byte count are all the console's.
    // =========================================================================================
    u32 BrnSimpleParticleArray::CB4ParticleBank::Construct(CgsMemory::HeapMalloc* lpHeapMalloc,
                                                           u32 luNumParticles)
    {
        muNextParticle = 0;
        muNumParticles = (luNumParticles + (KU_BANK_GRANULE - 1u)) & ~(KU_BANK_GRANULE - 1u);

        if (luNumParticles == 0)
        {
            mpaParticles = 0;
            return 0;
        }

        const u32 luConsoleSize = luNumParticles * static_cast<u32>(sizeof(CB4Particle));
        // [FLAG PC memory safety] the ring's own length -- see the banner.
        const u32 luRingSize    = muNumParticles * static_cast<u32>(sizeof(CB4Particle));
        mpaParticles = static_cast<CB4Particle*>(
            lpHeapMalloc->Malloc(static_cast<s32>(luRingSize), KI_PARTICLE_ALIGNMENT));
        CGS_ASSERT(mpaParticles != 0, "mpaParticles");   // BrnSimpleParticleRenderer.cpp:293
        return luConsoleSize;
    }

    // =========================================================================================
    // CB4ParticleBank::Prepare (DWARF :328) -- inlined into ParticleModule::Prepare, twice per
    // array (regular bank @0x8229C2D0.., crash bank @0x8229C31C..): every record's spawn-time
    // lane is stamped -9999 (the lvx/stfs-into-lane-3/stvx round trip only rewrites w), the
    // bank's newest spawn time likewise, and the cursor is put at the TOP of the ring. An empty
    // bank takes muNumParticles - 1 == 0xFFFFFFFF, exactly as the console's unsigned `addi -1`.
    // The array argument is the DWARF's; the inlined body never reads it.
    // =========================================================================================
    void BrnSimpleParticleArray::CB4ParticleBank::Prepare(BrnSimpleParticleArray* /*lpArray*/)
    {
        for (u32 luCount = 0; luCount < muNumParticles; ++luCount)
        {
            mpaParticles[luCount].mPositionTime.SetPlus(KF_NEVER_SPAWNED_TIME);
        }
        mrLastSpawnTime = KF_NEVER_SPAWNED_TIME;
        muNextParticle  = muNumParticles - 1u;
    }

    // =========================================================================================
    // Construct @0x8228C5A0.
    // =========================================================================================
    void BrnSimpleParticleArray::Construct(CgsMemory::HeapMalloc* lpHeapMalloc,
                                           ENativeParticleType leParticleType)
    {
        CGS_ASSERT(lpHeapMalloc != 0, "lpHeapMalloc != NULL");   // :930
        CGS_ASSERT((leParticleType >= 0) && (leParticleType < eParticleArray_Max),
                   "( leParticleType >= 0 ) && ( leParticleType < AttribSys::Enums::NativeParticleType::eParticleArray_Max )"); // :931

        mpStandardParams = &maStandardParams[leParticleType];       // `stw ..., 0x24(r?)` (&unk_82FABA80 + 128*type)
        maTextures[leParticleType] = gsDefaultTexture;               // both words zeroed (0x8228C618)

        // The table walk: find this type's row, construct both banks from it. The console's
        // loop compares against the 0x9C (13 * 12) byte bound; a row that is never found skips
        // both Constructs and fires the assert below.
        u32 luLoop = 0;
        for (; luLoop < eParticleArray_Max; ++luLoop)
        {
            if (gBrnParticleBankSize[luLoop].meParticleType == leParticleType)
            {
                mBankRegular.Construct(lpHeapMalloc, gBrnParticleBankSize[luLoop].muRegularBankSize);
                mBankCrash.Construct(lpHeapMalloc, gBrnParticleBankSize[luLoop].muCrashBankSize);
                break;
            }
        }
        CGS_ASSERT(luLoop < eParticleArray_Max,
                   "Cannot find Particle Bank Size for enum type.  Have they changed ?");   // :954

        Initialize();
    }

    // =========================================================================================
    // Initialize @0x8227ADD8 -- derive the render-ready block from the standard parameters.
    // Store order is the console's: the three colours, the acceleration, the six scalars, the
    // ready byte (cleared), and the size quad last.
    // =========================================================================================
    void BrnSimpleParticleArray::Initialize()
    {
        const CB4ParticleArrayStandardParams* const lpParams = mpStandardParams;

        mParticleData.mStartColour = ConvertColour(lpParams->mStartColour);   // +0x30
        mParticleData.mMidColour   = ConvertColour(lpParams->mMidColour);     // +0x40
        mParticleData.mEndColour   = ConvertColour(lpParams->mEndColour);     // +0x50

        // (0, gravity, 0, 0): `stfs f13(0.0), var_40 ; stfs 0x38(r11), var_40+4 ;
        // stfs f13, var_38 ; stw r9(0), var_34` -- the w lane is an INTEGER zero store.
        mParticleData.mAcceleration.x = KF_ZERO;
        mParticleData.mAcceleration.y = lpParams->mrGravity;
        mParticleData.mAcceleration.z = KF_ZERO;
        mParticleData.mAcceleration.w = KF_ZERO;

        mParticleData.mrLifeTime  = lpParams->mrLifeTime;    // +0x70 <- +0x08
        mParticleData.mrMidTime   = lpParams->mrMidTime;     // +0x74 <- +0x0C
        mParticleData.mrNearClip  = lpParams->mrNearClip;    // +0x80 <- +0x28
        mParticleData.mrFarClip   = lpParams->mrFarClip;     // +0x84 <- +0x2C
        mParticleData.mrNearFade  = lpParams->mrNearFade;    // +0x78 <- +0x30
        mParticleData.mrFarFade   = lpParams->mrFarFade;     // +0x7C <- +0x34

        mbIsReady = false;                                   // `stb r9(0), 0(r3)`

        mParticleData.mSizeParams.x = lpParams->mrStartSize;      // +0x1C
        mParticleData.mSizeParams.y = lpParams->mrMidSize;        // +0x20
        mParticleData.mSizeParams.z = lpParams->mrEndSize;        // +0x24
        mParticleData.mSizeParams.w = lpParams->mrMaxScreenSize;  // +0x50
    }

    // =========================================================================================
    // UpdateParams @0x8228C6E0 -- one nativeparticleparams collection into the static standard
    // parameters (the console re-reads mpAttributeData and mpStandardParams before every store;
    // the order below is the console's), assert the blend mode, then Initialize.
    // =========================================================================================
    void BrnSimpleParticleArray::UpdateParams(const Attrib::Gen::nativeparticleparams& lParams)
    {
        CB4ParticleArrayStandardParams* const lpParams = mpStandardParams;

        ConvertColour(lParams.StartColour(), lpParams->mStartColour);   // attrib +0x00 -> +0x10
        ConvertColour(lParams.MidColour(),   lpParams->mMidColour);     // attrib +0x10 -> +0x14
        ConvertColour(lParams.EndColour(),   lpParams->mEndColour);     // attrib +0x20 -> +0x18

        lpParams->mpacTextureName = lParams.TextureName();              // +0x30 -> +0x00
        lpParams->mbUseDrag       = (lParams.UseDrag() != 0);           // +0x34 -> +0x68 (byte)
        lpParams->meBlendMode     = static_cast<EParticleBlend>(lParams.BlendMode());   // +0x44 -> +0x04

        // (+0x64, +0x68, 0, 0): two lfs/stfs then `std r8(0)` over the upper half, one stvx.
        lpParams->mLightingMinMax.x = lParams.LightingMin();
        lpParams->mLightingMinMax.y = lParams.LightingMax();
        lpParams->mLightingMinMax.z = KF_ZERO;
        lpParams->mLightingMinMax.w = KF_ZERO;

        lpParams->mrDragDuration              = lParams.DragDuration();              // +0x88 -> +0x64
        lpParams->mrDragInitialVelocityScale  = lParams.DragInitialVelocityScale();  // +0x84 -> +0x5C
        lpParams->mrDragTerminalVelocityScale = lParams.DragTerminalVelocityScale(); // +0x80 -> +0x60
        lpParams->mrGravity                   = lParams.Gravity();                   // +0x70 -> +0x38
        lpParams->mrLifeTime                  = lParams.LifeTime();                  // +0x6C -> +0x08
        lpParams->mrRotationSpeedMin          = lParams.RotationSpeedMin();          // +0x3C -> +0x54
        lpParams->mrRotationSpeedMax          = lParams.RotationSpeedMax();          // +0x40 -> +0x58
        lpParams->mrStartSize                 = lParams.StartSize();                 // +0x38 -> +0x1C
        lpParams->mrMidSize                   = lParams.MidSize();                   // +0x5C -> +0x20
        lpParams->mrEndSize                   = lParams.EndSize();                   // +0x7C -> +0x24
        lpParams->mrMidTime                   = lParams.MidTime();                   // +0x58 -> +0x0C
        lpParams->mrNearFade                  = lParams.NearFade();                  // +0x50 -> +0x30
        lpParams->mrFarFade                   = lParams.FarFade();                   // +0x74 -> +0x34
        lpParams->mrNearClip                  = lParams.NearClip();                  // +0x54 -> +0x28
        lpParams->mrFarClip                   = lParams.FarClip();                   // +0x78 -> +0x2C
        lpParams->mrMaxScreenSize             = lParams.MaxScreenSize();             // +0x60 -> +0x50
        lpParams->muTilesWide                 = lParams.TilesWide();                 // +0x48 -> +0x6C
        lpParams->muTilesHigh                 = lParams.TilesHigh();                 // +0x4C -> +0x70

        CGS_ASSERT((lpParams->meBlendMode == eParticleBlendNormal)
                || (lpParams->meBlendMode == eParticleBlendAdditive),
                   "( mpStandardParams->meBlendMode == AttribSys::Enums::ParticleBlend::eParticleBlendNormal ) || "
                   "( mpStandardParams->meBlendMode == AttribSys::Enums::ParticleBlend::eParticleBlendAdditive )"); // :1045

        Initialize();
    }

    // =========================================================================================
    // SpawnParticle @0x8227B000 -- write one record at the bank's cursor and step the cursor
    // DOWN, wrapping from 0 to the top of the ring. Arguments arrive v1/v2 (position/velocity),
    // f1 time, f2 size scale, f3 rotational velocity (revolutions/s), r7 the crash-bank bool,
    // f4 alpha -- the DWARF order, confirmed by the four `fmr` into f31..f28 at 0x8227B034..48.
    // =========================================================================================
    bool BrnSimpleParticleArray::SpawnParticle(rw::math::vpu::Vector3 lSpawnPosition,
                                               rw::math::vpu::Vector3 lSpawnVelocity,
                                               f32 lrSpawnTime,
                                               f32 lrSizeScale,
                                               f32 lrRotationalVelocity,
                                               bool lbIsCrash,
                                               f32 lrAlpha)
    {
        CB4ParticleBank* const lpBank = lbIsCrash ? &mBankCrash : &mBankRegular;
        CGS_ASSERT(lpBank->mpaParticles != 0, "lpBank->mpaParticles != NULL");   // :1077

        CB4Particle* const lpParticle = &lpBank->mpaParticles[lpBank->muNextParticle];

        lpParticle->mPositionTime.SetVector3(lSpawnPosition);
        lpParticle->mPositionTime.SetPlus(lrSpawnTime);

        lpParticle->mVelocityRotation.SetVector3(lSpawnVelocity);
        lpParticle->mVelocityRotation.SetPlus(lrRotationalVelocity * KF_TWO_PI);

        lpParticle->mScaleAlpha.x = KF_ZERO;
        lpParticle->mScaleAlpha.y = KF_ZERO;
        lpParticle->mScaleAlpha.z = lrSizeScale;
        lpParticle->mScaleAlpha.w = lrAlpha;

        // `fsubs f13, last, t ; fsel f0, f13, last, t` -- Max<float>, with fsel's NaN polarity
        // (an unordered difference selects the new time).
        lpBank->mrLastSpawnTime = ((lpBank->mrLastSpawnTime - lrSpawnTime) >= 0.0f)
                                ? lpBank->mrLastSpawnTime : lrSpawnTime;

        if (lpBank->muNextParticle == 0)
        {
            lpBank->muNextParticle = lpBank->muNumParticles;
        }
        --lpBank->muNextParticle;
        ++gauSimpleParticleSpawned;   // [diag] NOT IN THE X360 BINARY -- see the header
        return true;
    }

    // =========================================================================================
    // AcquireTexture @0x8227E468.
    // =========================================================================================
    void BrnSimpleParticleArray::AcquireTexture(u32 luRequestedHash,
                                                CgsResource::SafeResourceHandle<renderengine::Texture> lTexture,
                                                ENativeParticleType leArrayID)
    {
        CGS_ASSERT((leArrayID >= 0) && (leArrayID < eParticleArray_Max),
                   "( leArrayID >= 0 ) && ( leArrayID < AttribSys::Enums::NativeParticleType::eParticleArray_Max )");

        const char* const lpcTextureName = mpStandardParams->mpacTextureName;
        if (lpcTextureName == 0)
        {
            CGS_ASSERT(leArrayID == eParticleArray_None, "Missing Texture name for Native Particle Params");
            maTextures[leArrayID] = gsDefaultTexture;
            mbIsReady = true;
            return;
        }

        if (luRequestedHash == TextureNameMap::Entry::HashString(lpcTextureName))
        {
            maTextures[leArrayID] = lTexture;
            mbIsReady = true;
        }
    }

    // DWARF :366 -- inlined into BrnSimpleParticleRenderer::Dispatch (`&unk_82FAC150 + 8 * type`
    // through the SafeResourceHandle conversion operator).
    renderengine::Texture* BrnSimpleParticleArray::GetTexture(ENativeParticleType leParticleType)
    {
        return maTextures[leParticleType];
    }

    // DWARF :278.
    CB4ParticleArrayStandardParams* BrnSimpleParticleArray::GetStandardParams(ENativeParticleType leParticleType)
    {
        return &maStandardParams[leParticleType];
    }
}
}
