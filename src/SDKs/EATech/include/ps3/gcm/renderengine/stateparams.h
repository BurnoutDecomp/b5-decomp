#pragma once

#include "types.hpp"

// renderengine state parameter types.
// Source: EARenderWare/stable/platform/include/native/ps3-ppu-gcc/ps3/rw/graphics/core/stateparams.h
// DWARF: references/DecFIGS/dwarfdump/SDKs/EATech/include/ps3/gcm/renderengine/stateparams.h
//
// The Feb-2007 partial source uses rw::graphics::core; the X360 ARTIST / FIGS build
// collapsed it to renderengine:: (same type shapes, renamed namespace).
//
// Types declared here are parameter structs for SamplerState, TextureState, and
// RenderTargetState; they are referenced by DWARF but not yet fully implemented.
// The BlendState parameter types live in SDKs/RenderEngineClub/.../blendstate.h.

typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef float float32_t;

namespace renderengine
{
    // RGBA8 packed color (DWARF: stateparams.h:111)
    struct RGBA8
    {
        u8 r, g, b, a;
        RGBA8() : r(0), g(0), b(0), a(0) {}
        RGBA8(u8 r_, u8 g_, u8 b_, u8 a_) : r(r_), g(g_), b(b_), a(a_) {}
    };

    // -------------------------------------------------------------------------
    // renderengine::SamplerState::Parameters
    // DWARF source: stateparams.h:44
    // -------------------------------------------------------------------------
    class SamplerState
    {
    public:
        enum AddressingMode : u32
        {
            ADDRESSINGMODE_WRAP   = 0,
            ADDRESSINGMODE_MIRROR = 1,
            ADDRESSINGMODE_CLAMP  = 2,
            ADDRESSINGMODE_BORDER = 3,
        };
        enum FilterMode : u32
        {
            FILTERMODE_NONE    = 0,
            FILTERMODE_NEAREST = 1,
            FILTERMODE_LINEAR  = 2,
        };
        enum PS3Convolution : u32
        {
            PS3CONVOLUTION_QUINCUNX = 0,
        };
        enum PS3UnsignedRemap : u32
        {
            PS3UNSIGNEDREMAP_NORMAL = 0,
        };
        enum PS3DepthTextureFunction : u32
        {
            PS3DEPTHTEXTUREFUNCTION_ZFUNC_ALWAYS = 0,
        };
        enum PS3TextureGamma : u32
        {
            PS3TEXTURE_GAMMA_NONE = 0,
        };

        struct Parameters
        {
        protected:
            AddressingMode          addressU;
            AddressingMode          addressV;
            AddressingMode          addressW;
            float32_t               bias;
            u32                     maxLevel;
            u32                     minLevel;
            u32                     maxAnisotropy;
            FilterMode              magFilter;
            FilterMode              minFilter;
            FilterMode              mipFilter;
            PS3Convolution          convolution;
            RGBA8                   color;
            PS3UnsignedRemap        remap;
            PS3DepthTextureFunction depthTextureFunction;
            PS3TextureGamma         gamma;

        public:
            Parameters()
                : addressU(ADDRESSINGMODE_WRAP), addressV(ADDRESSINGMODE_WRAP)
                , addressW(ADDRESSINGMODE_WRAP), bias(0.0f)
                , maxLevel(0u), minLevel(15u), maxAnisotropy(1u)
                , magFilter(FILTERMODE_NEAREST), minFilter(FILTERMODE_NEAREST)
                , mipFilter(FILTERMODE_NONE), convolution(PS3CONVOLUTION_QUINCUNX)
                , color(), remap(PS3UNSIGNEDREMAP_NORMAL)
                , depthTextureFunction(PS3DEPTHTEXTUREFUNCTION_ZFUNC_ALWAYS)
                , gamma(PS3TEXTURE_GAMMA_NONE)
            {}
        };
    };

    // -------------------------------------------------------------------------
    // renderengine::RenderTargetState::Parameters (stub — members not yet used)
    // -------------------------------------------------------------------------
    class RenderTargetState
    {
    public:
        struct Parameters
        {
            Parameters() {}
        };
    };

} // namespace renderengine
