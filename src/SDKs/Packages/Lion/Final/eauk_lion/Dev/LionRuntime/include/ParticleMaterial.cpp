// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.cpp
//
// cParticleMaterial runtime: the build/serialise/relocate path for one Lion
// (eauk_lion) particle material. Sibling to ParticleBehaviour.cpp and the committed
// Lion homes LionSerialiser.* / LionTokeniser.* it calls into by name.
//
// Reconstructed store-for-store from the X360 asm for:
//   cParticleMaterial::Build               @ 0x8290E500
//   cParticleMaterial::Delocate            @ 0x82909A70
//   cParticleMaterial::GetSerialiseSize    @ 0x82909C78
//   cParticleMaterial::Relocate            @ 0x8290E660
//   cParticleMaterial::Serialise           @ 0x8290E720
//   cParticleMaterial::SetNormalMapHandle  @ 0x82909DE0
//   cParticleMaterial::SetTextureMapHandle @ 0x82909DD8
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleParser.h"
#include "GameSource/Effects/Particles/LionParticleRender.h"

#include <cstring>

// ----------------------------------------------------------------------------
//   gpLionParticleRender -- the Lion particle-render singleton (X360 dword_83121D60).
//     HOMED 2026-09-03 in LionFX.cpp, whose cLionFX::Init takes the iParticleRender*
//     that writes it. ⚠ Init is NOT reconstructed, so the pointer is still NULL at
//     runtime and cParticleMaterial::Build's TextureRegister call does not fire -- the
//     console guards that call with the same null test, so the shape here is faithful;
//     what is missing is the writer, and LionFX.cpp says so out loud.
//   gLionParticleParserMatTokenTable (X360 off_82F36A3C) -- the cParticleMaterial
//     member token table. HOMED 2026-09-03 in LionParticleParser.cpp, transcribed from
//     the X360 .rdata, under the name the DecFIGS DWARF gives it. It used to be an
//     `extern` under an INVENTED name with no definition anywhere in the tree, so this
//     TU could never link.
// ----------------------------------------------------------------------------
extern BrnParticle::LionParticleRender* gpLionParticleRender;          // dword_83121D60

// ----------------------------------------------------------------------------
// cParticleMaterial::Build  @ 0x8290E500
//
// Finalises a material after load: counts how many of the animated-texture option
// bits are set into mNumMeshes; if a normal-map name is present and is not the
// sentinel "(NULL)" string it promotes the shader to the normal variant (else it
// drops the name); registers the base texture with the Lion particle renderer; and
// derives mFrameCount = mYFrames * mXFrames (clamped to >= 1). Lastly it clears the
// stand-alone bit 0x2 of mFlags when bit 0x1 is not also set.
// ----------------------------------------------------------------------------
void cParticleMaterial::Build()
{
    const U32 luFlags = mFlags;

    mNumMeshes = 0;
    if ((luFlags & 0x2000) != 0)
        mNumMeshes = 1;
    if ((luFlags & 0x4000) != 0)
        ++mNumMeshes;
    if ((luFlags & 0x8000) != 0)
        ++mNumMeshes;
    if ((luFlags & 0x10000) != 0)
        ++mNumMeshes;
    if ((luFlags & 0x20000) != 0)
        ++mNumMeshes;

    if (mpNormalMapName)
    {
        if (::strcmp(mpNormalMapName.Get(), "(NULL)") != 0)
            mShader = eSHADER_LION_NORM;   // stb 1, +0x42
        else
            mpNormalMapName.SetRaw(0);
    }

    if (mpTextureName)
    {
        if (gpLionParticleRender != 0)
            gpLionParticleRender->TextureRegister(this, mpTextureName.Get());
    }

    mFrameCount = static_cast<S32>(mYFrames) * static_cast<S32>(mXFrames);
    if (mFrameCount == 0)
        mFrameCount = 1;

    const U32 luFlags2 = mFlags;
    if ((luFlags2 & 0x2) != 0 && (luFlags2 & 0x1) == 0)
        mFlags = luFlags2 & 0xFFFFFFFDu;
}

// ----------------------------------------------------------------------------
// cParticleMaterial::Delocate  @ 0x82909A70
//
// Prepares the material for serialisation to a (possibly different-endian) target.
// The four owned string pointers (texture / mesh / layer-group / normal-map names)
// are converted from absolute pointers to base-relative byte offsets; when the
// endian-twiddle flag is set the material's own member image is byte-swapped through
// the Lion material token table (off_82F36A3C); each of the five mesh-name pointer
// words is then converted ptr->offset and (when twiddling) byte-reversed in place.
// ----------------------------------------------------------------------------
void cParticleMaterial::Delocate(U32 aEndianTwiddleFlag)
{
    const bool lbTwiddle = (aEndianTwiddleFlag != 0);

    mpTextureName.Delocate(this);
    mpMeshName.Delocate(this);
    mpLayerGroupName.Delocate(this);
    mpNormalMapName.Delocate(this);

    if (lbTwiddle)
        gLionParticleParserMatTokenTable.EndianTwiddle(this);

    for (u32 luIndex = 0; luIndex < 5; ++luIndex)
    {
        mpMeshNames[luIndex].Delocate(this);

        if (lbTwiddle)
        {
            // Big->little byte reversal of the now-offset pointer word in place.
            u8* lp = reinterpret_cast<u8*>(mpMeshNames[luIndex].RawAddress());
            const u8 b0 = lp[0];
            const u8 b1 = lp[1];
            const u8 b2 = lp[2];
            const u8 b3 = lp[3];
            const u32 luSwapped =
                ((((((static_cast<u32>(b0) << 8) | b1) << 8) | b2) << 8) | b3);
            *reinterpret_cast<u32*>(lp) = luSwapped;
        }
    }
}

// ----------------------------------------------------------------------------
// cParticleMaterial::GetSerialiseSize  @ 0x82909C78
//
// Adds this material's serialised size to the serialiser's running totals: a fixed
// 176 (0xB0) bytes for the material record in the data area, plus strlen+1 for each
// present owned string (texture / mesh / layer-group / normal-map names and the five
// mesh-name variants) in the string area.
// ----------------------------------------------------------------------------
void cParticleMaterial::GetSerialiseSize(cLionSerialiser& aSer) const
{
    aSer.mDataSize += 176;

    if (mpTextureName)
        aSer.mStringSize += static_cast<u32>(::strlen(mpTextureName.Get())) + 1;
    if (mpMeshName)
        aSer.mStringSize += static_cast<u32>(::strlen(mpMeshName.Get())) + 1;
    if (mpLayerGroupName)
        aSer.mStringSize += static_cast<u32>(::strlen(mpLayerGroupName.Get())) + 1;
    if (mpNormalMapName)
        aSer.mStringSize += static_cast<u32>(::strlen(mpNormalMapName.Get())) + 1;

    for (u32 luIndex = 0; luIndex < 5; ++luIndex)
    {
        if (mpMeshNames[luIndex])
            aSer.mStringSize += static_cast<u32>(::strlen(mpMeshNames[luIndex].Get())) + 1;
    }
}

// ----------------------------------------------------------------------------
// cParticleMaterial::Relocate  @ 0x8290E660
//
// Inverse of the pointer->offset conversion in Delocate: re-bases each present owned
// string offset back into an absolute pointer relative to `this`.
// ----------------------------------------------------------------------------
void cParticleMaterial::Relocate()
{
    // asm words 4, 7, 8, 6 then 24..28 -- each a 32-bit slot re-based against `this`.
    mpTextureName.Relocate(this);
    mpMeshName.Relocate(this);
    mpLayerGroupName.Relocate(this);
    mpNormalMapName.Relocate(this);

    for (u32 luIndex = 0; luIndex < 5; ++luIndex)
        mpMeshNames[luIndex].Relocate(this);
}

// ----------------------------------------------------------------------------
// cParticleMaterial::Serialise  @ 0x8290E720
//
// Copies the material record (164 bytes / 0xA4) into the serialiser's data area via
// DataStore, then interns each present owned string through StringStore and stores
// the relocated string pointer back into the just-written copy. Returns the copy, or
// null if this is null. (The 176-byte figure in GetSerialiseSize is reserve slack;
// DataStore copies the 0xA4 record proper.)
// ----------------------------------------------------------------------------
cParticleMaterial* cParticleMaterial::Serialise(cLionSerialiser& aSer) const
{
    if (this == 0)
        return 0;

    cParticleMaterial* lpCopy =
        reinterpret_cast<cParticleMaterial*>(aSer.DataStore(this, 164));

    lpCopy->mpTextureName.Set(aSer.StringStore(mpTextureName.Get()));
    lpCopy->mpMeshName.Set(aSer.StringStore(mpMeshName.Get()));
    lpCopy->mpLayerGroupName.Set(aSer.StringStore(mpLayerGroupName.Get()));
    lpCopy->mpNormalMapName.Set(aSer.StringStore(mpNormalMapName.Get()));

    for (u32 luIndex = 0; luIndex < 5; ++luIndex)
        lpCopy->mpMeshNames[luIndex].Set(aSer.StringStore(mpMeshNames[luIndex].Get()));

    return lpCopy;
}

// ----------------------------------------------------------------------------
// cParticleMaterial::SetNormalMapHandle  @ 0x82909DE0
//   store the resolved normal-map resource handle (+0x14).
// ----------------------------------------------------------------------------
void cParticleMaterial::SetNormalMapHandle(U32 auHandle)
{
    mNormalMapHandle = auHandle;
}

// ----------------------------------------------------------------------------
// cParticleMaterial::SetTextureMapHandle  @ 0x82909DD8
//   store the resolved texture-map resource handle (+0x0C).
// ----------------------------------------------------------------------------
void cParticleMaterial::SetTextureMapHandle(U32 auHandle)
{
    mTextureHandle = auHandle;
}
