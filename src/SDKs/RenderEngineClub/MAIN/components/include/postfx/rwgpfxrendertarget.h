#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator (the render-target's allocator)
#include "pc/gcm/renderengine/texture.h"        // renderengine::Texture / PixelBuffer (owned surfaces)
// renderengine::TextureState is held only by pointer here (mpColourTextureState / Target::mpTextureState);
// forward-declared below rather than pulling its (header-less) home in, so consumers that re-declare
// the vendor render-state types locally (e.g. GameSource/Graphics/BrnRendererMemory.cpp) don't ODR-clash.

// rw::graphics::postfx::RenderTarget - the EATech RenderEngineClub post-fx render-target object
// (SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h). A RenderTarget owns
// a set of colour Targets plus a depth/stencil Target and the per-section RenderTargetState the GPU
// binds; Begin/End wrap a draw pass into it, GetTexture / GetDepthStencilTexture hand back the
// resolved textures, and GetRenderTargetState returns the D3D surface-state to bind.
//
// Only the surface this build's CgsRenderTarget facade calls is declared here (the full class body
// + layout is reconstructed with its own TU). The member set / signatures come from the DecFIGS
// DWARF (rwgpfxrendertarget.h); the call shapes are confirmed against the X360 ARTIST asm that the
// CgsRenderTarget thin wrappers forward into:
//   RenderTarget::Begin(uint32_t)  @ guest rw__graphics__postfx__RenderTarget__Begin
//   RenderTarget::End(bool)        @ guest rw__graphics__postfx__RenderTarget__End
// This is a declaration-only home header (the compile gate needs the type's shape, not its body).

namespace renderengine
{
    class Texture;
    class RenderTargetState;   // the D3D surface-state object Device::SetState binds
    class TextureState;        // the colour sampler state (held by pointer only)
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // rwgpfxrendertarget.h:35 (DWARF) - one colour or depth surface owned by a RenderTarget: its GPU
    // PixelBuffer surface, the sampleable texture the surface resolves to, the sampler texture-state,
    // and (depth path) a hierarchical-Z companion texture. The X360 build (Target::CreateColor
    // @0x824034D0 / CreateDepth @0x82403688) fills these store-for-store; the offsets in the comments
    // are the X360 4-byte-pointer image (size 0x18). CgsRenderTarget also holds Target*/const Target*
    // by pointer (a provided / shared surface), so the full definition lives here.
    struct Target
    {
        // rwgpfxrendertarget.h:38 (DWARF) - the serialised description of ONE surface (a colour
        // section or the depth/stencil section) that Target::CreateColor / CreateDepth consume. The
        // member set, names and types are the DecFIGS DWARF; the offsets in the comments are the X360
        // image (4-byte pointers, sizeof == 0x48) and are confirmed store-for-store against
        // CgsRenderTarget::Construct @0x827ECC28, which fills one of these per section.
        //
        // 2026-08-12: RETYPED from the pre-DWARF placeholder spelling (muReserved14 / muMipBase /
        // maReserved2C[6] / a 4-byte `u32 mpAllocator`). The old block could not hold an x64 layout at
        // all - mpBufferAddress (+0x38) widens 4->8 and would have collided with mn8TileIndex (+0x3C),
        // and the u32 allocator slot TRUNCATED a 64-bit pointer. Offsets no longer transfer on the
        // host; every member is reached BY NAME.
        struct Parameters
        {
            rw::IResourceAllocator* mpAllocator;             // +0x00
            u32   mu32Width;                                  // +0x04
            u32   mu32Height;                                 // +0x08
            u32   mu32Pitch;                                  // +0x0C
            u32   mBufferFormat;                              // +0x10  renderengine::PixelFormat
            u32   mTextureFormat;                             // +0x14  renderengine::PixelFormat
            u32   mTextureType;                               // +0x18  renderengine::Texture::Type
            u32   mu32MultiSampleFormat;                      // +0x1C
            u32   mFilterMode;                                // +0x20  SamplerState::FilterMode
            bool  mbUseStencil;                               // +0x24  (gates the hierarchical-Z companion)
            s32   mn32BaseEDRAM;                              // +0x28
            s32   mn32HierarchicalZAddress;                   // +0x2C
            s32   mn32CompressionBase;                        // +0x30  (PS3)
            bool  mbSystemMemory;                             // +0x34
            void* mpBufferAddress;                            // +0x38  shared-memory base, when shared
            s8    mn8TileIndex;                               // +0x3C
            s8    mn8ZCullIndex;                              // +0x3D
            s32   mn32ZCullAddress;                           // +0x40  (PS3)
            u8    mu8NumSections;                             // +0x44

            // The DWARF setter family (rwgpfxrendertarget.h:114-248). The X360 inlines these into its
            // callers (CgsRenderTarget::Construct appears as a flat run of stores); un-inlined here so
            // callers reach the named fields. Only the ones an X360-attested caller uses are added.
            void SetAllocator(rw::IResourceAllocator* lpAllocator) { mpAllocator = lpAllocator; }
            void SetWidth(u32 lu32Width)                           { mu32Width = lu32Width; }
            void SetHeight(u32 lu32Height)                         { mu32Height = lu32Height; }
            void SetBufferFormat(u32 lFormat)                      { mBufferFormat = lFormat; }
            void SetTextureFormat(u32 lFormat)                     { mTextureFormat = lFormat; }
            void SetTextureType(u32 lType)                         { mTextureType = lType; }
            void SetMultisampleFormat(u32 lu32Format)              { mu32MultiSampleFormat = lu32Format; }
            void SetFilterMode(u32 lMode)                          { mFilterMode = lMode; }
            void SetBaseEDRAM(s32 ln32Base)                        { mn32BaseEDRAM = ln32Base; }
            void SetUseSystemMemory(bool lbUse)                    { mbSystemMemory = lbUse; }
            void SetTileIndex(s8 ln8Index)                         { mn8TileIndex = ln8Index; }
            void SetZCullIndex(s8 ln8Index)                        { mn8ZCullIndex = ln8Index; }
            void SetZCullAddress(s32 ln32Address)                  { mn32ZCullAddress = ln32Address; }
            void SetNumSections(u32 lu32NumSections)               { mu8NumSections = static_cast<u8>(lu32NumSections); }

            // rwgpfxrendertarget.h:114 (DWARF) - point this surface at another target's memory. The
            // X360 branch stores the shared base address and marks the surface as un-tiled (-1); it
            // does NOT store the shared Target* itself (the pointer is only tested), so neither does
            // this - the parameter is kept because the DWARF signature carries it.
            void ShareMemoryWithTarget(const Target* /*lpTarget*/, void* lpAddress)
            {
                mpBufferAddress = lpAddress;
                mn8TileIndex    = -1;
            }
        };

        u32                                       mpReserved0;     // +0x00
        renderengine::PixelBuffer::SurfaceHeader* mpPixelBuffer;   // +0x04  PixelBuffer::Initialize result
        renderengine::Texture*       mpTexture;       // +0x08  AllocateAndInitializeTexture result
        renderengine::TextureState*  mpTextureState;  // +0x0C  TextureState::Initialize result
        renderengine::Texture*       mpHiZTexture;    // +0x10  depth path: hierarchical-Z companion
        // ⚠ MISNAMED: THIS BYTE IS THE SECTION COUNT, NOT A SURFACE FORMAT (noted 2026-08-17,
        // reflections step 1). Its two writers in this class's own TU already store one --
        // rwgpfxrendertarget.cpp:115 (CreateColor) and :231 (CreateDepth) both do
        // `mu8Format = lrParams.mu8NumSections;` -- its other reader multiplies by it to get a
        // texture height (`lrParams.mu8NumSections * lrParams.mu32Height`, :201), and the DecFIGS
        // DWARF names the same slot outright: rwgpfxrendertarget.h:490 `uint8_t m_numSections;`,
        // sitting between m_pStencilTexture (:488) and m_bIsShared (:493). The X360 image reads it
        // with `lbz r11, 0x14(r3)` in Target::Resolve(u32) @0x823F9170 and branches on `<= 1`,
        // i.e. "one section or an atlas of several", which is a section-count test.
        // RENAME PENDING to mu8NumSections: the two stores live in rwgpfxrendertarget.cpp, which is
        // deliberately NOT on tools/build/build_game_exe.bat and is owned by the postfx TU, so the
        // rename is a one-file follow-up rather than a header-only edit (see the CROSS-GROUP note
        // in the reflections step-1 cubeleaf report). Until then: silently testing a member called
        // "Format" against 1 is how the next reader inverts it -- do not.
        u8                           mu8Format;       // +0x14  == the SECTION COUNT (see above)
        u8                           mau8Pad15[3];    // +0x15

        // X360 0x824034D0 -- build a colour surface (PixelBuffer + sampleable texture + sampler state).
        void CreateColor(const Parameters* lpParameters);
        // X360 0x82403688 -- build a depth/stencil surface (+ a hierarchical-Z companion when asked).
        void CreateDepth(const Parameters* lpParameters);
        // X360 0x823F9118 -- resolve the surface's tiled EDRAM PixelBuffer out to its texture.
        void Resolve();

        // rwgpfxrendertarget.h:388 (DWARF) `void Resolve(uint32_t);` -- the PER-FACE / per-section
        // overload, sitting beside :383's `void Resolve();` on this same class. X360 0x823F9170.
        //
        // WHAT THE CONSOLE BODY DOES, from the asm (the Hex-Rays rendering under-counts the
        // arguments -- it prints `Xbox2ResolveTo(*(result + 4), 1.0)`, two of ten):
        //   0x823F917C  mr   r29, r4          ; r29 = the face/section index, live in BOTH arms
        //   0x823F9180  lwz  r4, 8(r3)        ; mpTexture -- null means nothing to resolve
        //   0x823F918C  lbz  r11, 0x14(r3)    ; the SECTION COUNT byte above
        //   0x823F9198  ble  cr6, loc_823F9220; <= 1 sections -> arm A
        //  arm A (the ENV MAP's -- CreateEnvmapBuffer leaves the count at the constructor's 1):
        //   r3=mpPixelBuffer r4=mpTexture r5=Flags 0 r6=DestX 0 r7=DestY 0 r8=DestLevel 0
        //   0x823F9230  mr r9, r29           ; ***DestSlice = the FACE***
        //   f1 = 1.0 (ClearZ), r10 = 0 (pClearColor), stack slot 10 = 0 (ClearStencil)
        //   -> renderengine::PixelBuffer::Xbox2ResolveTo -- i.e. "resolve into FACE `luFace`".
        //  arm B (> 1 sections -- the SHADOW cascade atlas): D3DDevice_Resolve with
        //   DestSliceOrFace = 0 (`li r9, 0` @0x823F91B0) and destPoint.y = height * index
        //   (`extrwi/addi/mullw r11, r11, r29` @0x823F9204-0x823F920C), i.e. a vertical strip.
        // So the index rides the SLICE for a cube and the Y OFFSET for an atlas; it is NOT
        // "only used in the >1 arm", which is what an earlier read of the pseudocode concluded.
        //
        // PC translation, defined in pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp (the MOUNTED
        // leaf -- the faithful console sibling rwgpfxrendertarget.cpp is not on the link):
        // StretchRect the target's scratch colour surface into GetCubeMapSurface(luFace, 0) of its
        // IDirect3DCubeTexture9. For every NON-cube target it is the same documented no-op
        // Resolve() is, and for the same reason -- on PC the shadow cascade atlas is rendered
        // straight into its own texture, so there is nothing to copy out.
        void Resolve(u32 luFace);
    };

    // rwgpfxrendertarget.h:502 (DWARF). The fields are owned by this class's own TU; only the
    // accessors CgsRenderTarget uses are declared so callers reach the colour / depth textures and
    // the render-target state by name rather than by raw offset.
    class RenderTarget
    {
    public:
        // rwgpfxrendertarget.h:504 (DWARF) - per-target creation mode (flag set).
        enum RenderTargetMode
        {
            eRenderTarget_NONE                 = 0,
            eRenderTarget_CREATE               = 1,
            eRenderTarget_USE_DEVICE_FOR_WRITE = 2,
            eRenderTarget_USE_PROVIDED         = 4,
        };

        // rwgpfxrendertarget.h:516 (DWARF) - the whole render target's serialised description: the
        // shared dimensions/allocator, the per-target creation modes, and one Target::Parameters
        // record per colour section plus one for the depth/stencil section. This is the block
        // CgsRenderTarget::Construct @0x827ECC28 builds on its stack and hands to Initialize.
        //
        // ARRAY SIZES ARE THE X360 SET, NOT THE PS3 DWARF's. The DWARF (Internal PS3) declares
        // m_aColorTargetParams[5] / m_apProvidedColorTarget[5]; the X360 ARTIST build carries FOUR of
        // each. Proven from the Construct asm, which is self-consistent only at four:
        //   colour loop  = 4 iterations (0x827ECD10..0x827ECDA0, dst stride 0x48 from +0x1C)
        //   depth block  @ +0x13C == 0x1C + 4*0x48   (var_C4 .. var_80)
        //   provided DS  @ +0x194 == 0x13C + 0x48 + 4*4   (var_6C)
        // The same 5->4 merge-window delta shows in CgsRenderTarget (4 colour surfaces + 1 depth,
        // sizeof 0x110), which is the class that fills this block.
        struct Parameters
        {
            rw::IResourceAllocator* mpAllocator;               // +0x000
            u32                mu32Width;                      // +0x004
            u32                mu32Height;                     // +0x008
            u32                mu32NumMipMaps;                 // +0x00C
            u32                mColourMode;                    // +0x010  RenderTargetMode
            u32                mDepthStencilMode;              // +0x014  RenderTargetMode
            bool               mbUseDepthStencilAsTexture;     // +0x018
            u8                 mu8NumSections;                 // +0x019
            Target::Parameters maColourTargetParams[4];        // +0x01C  (stride 0x48)
            Target::Parameters mDepthStencilParams;            // +0x13C
            Target*            mapProvidedColourTarget[4];     // +0x184
            Target*            mpProvidedDepthStencilTarget;   // +0x194

            // rwgpfxrendertarget.h:535 (DWARF). Seeds the block to its "nothing requested" defaults.
            // NOT reconstructed yet - the body lives in the postfx TU (the X360 calls it out of line,
            // rw__graphics__postfx__RenderTarget__Parameters__Parameters, from Construct @0x827ECCCC).
            Parameters();

            void SetAllocator(rw::IResourceAllocator* lpAllocator) { mpAllocator = lpAllocator; }
            void SetWidth(u32 lu32Width)                           { mu32Width = lu32Width; }
            void SetHeight(u32 lu32Height)                         { mu32Height = lu32Height; }
            void SetNumMipMaps(u32 lu32NumMipMaps)                 { mu32NumMipMaps = lu32NumMipMaps; }
            void SetColorMode(RenderTargetMode lMode)              { mColourMode = static_cast<u32>(lMode); }
            void SetDepthStencilMode(RenderTargetMode lMode)       { mDepthStencilMode = static_cast<u32>(lMode); }
            void SetUseDepthStencilAsTexture(bool lbUse)           { mbUseDepthStencilAsTexture = lbUse; }
            void SetNumSections(u32 lu32NumSections)               { mu8NumSections = static_cast<u8>(lu32NumSections); }

            Target::Parameters& GetColorTargetParameters(u32 luIndex) { return maColourTargetParams[luIndex]; }
            Target::Parameters& GetDepthStencilParameters()           { return mDepthStencilParams; }

            void SetProvidedDepthStencilTarget(Target* lpTarget) { mpProvidedDepthStencilTarget = lpTarget; }
        };

        // Bind this render-target for a draw pass writing into the given slice/face, then resolve
        // (luDestSliceOrFace selects the cube face / array slice; the 2D path passes 0).
        void Begin(u32 luDestSliceOrFace);
        // End the pass; lbResolve resolves the colour surface back to its sampleable texture.
        void End(bool lbResolve);

        // The resolved colour texture for colour target luIndex.
        renderengine::Texture* GetTexture(u32 luIndex);
        // The resolved depth/stencil texture (the depth target sampled as a texture).
        renderengine::Texture* GetDepthStencilTexture();

        // The DEPTH sampler state -- the whole TextureState (sampler words + raster) a shader
        // binds to read this target's depth. Null unless the target asked for a sampleable depth
        // (Parameters::mbUseDepthStencilAsTexture) with a depth mode other than USE_PROVIDED,
        // which is exactly the gate RenderTarget::CreateStates @0x82403A18 applies.
        //
        // WHY IT EXISTS, given the console has no such accessor. The X360 reads the word directly
        // -- BrnPostFx::Render @0x8240A5E4 `lwz r25, 0x8C(r7)` -> BrnPostFxShader::Render's last
        // TextureState argument -> sampler unit 4, SamplerDepth. On the 4-byte-pointer image that
        // +0x8C is BOTH `mDepthTarget.mpTextureState` (mDepthTarget sits at +0x80 and a Target
        // record is 0x18 bytes) and the `mpColourTextureState` this header names below; the two
        // cannot overlap once pointers widen, and CreateStates proves which one it is (it binds
        // mDepthTarget's hi-Z-else-depth texture into the state it stores there). Reaching the
        // depth state by NAME through this accessor is what stops a caller binding scene colour
        // to SamplerDepth. Defined in pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp.
        renderengine::TextureState* GetDepthTextureState();

        // The per-section D3D surface state to bind (Device::SetState). luSection selects the
        // section; the wrapper binds section 0.
        renderengine::RenderTargetState* GetRenderTargetState(u32 luSection);

        // The per-section render-target-state pointer array (X360: +0x1C + luSection*4, read by
        // RenderTarget::Begin). CreateBackBuffer patches section 0 to the engine default state and
        // copies section 4 (the resolved depth-stencil state) from the down-sample buffer so the back
        // buffer shares its depth surface. Declaration-only here (no asm in this TU); naming the
        // operation keeps the caller off raw offsets.
        renderengine::RenderTargetState* GetSectionRenderTargetState(u32 luSection);
        void SetSectionRenderTargetState(u32 luSection, renderengine::RenderTargetState* lpState);

        // --- the build-side surface (X360 ARTIST asm; reconstructed in rwgpfxrendertarget.cpp) -------
        // X360 0x82409B60 -- allocate the render-target object through the parameters' allocator (or the
        // registry default) and construct it; returns null when the allocation fails.
        //
        // rwgpfxrendertarget.h:744 (DWARF) is the REAL signature: it takes the whole
        // RenderTarget::Parameters block, and that is what CgsRenderTarget::Construct @0x827ECC28
        // passes. It has no body yet.
        static RenderTarget* Initialize(const Parameters& lrParameters);

        // SUSPECT (pre-DWARF model, kept because rwgpfxrendertarget.cpp still defines this form):
        // the same X360 function declared with `const Target::Parameters*`. That TU's body reaches the
        // colour/depth/provided sub-blocks by memcpy at the raw X360 offsets +0x1C / +0x13C / +0x194
        // out of the pointer, i.e. it is already treating the argument as a RenderTarget::Parameters -
        // and those byte offsets do not survive the x64 pointer widening. Reconciling that TU onto the
        // overload above (and splitting RenderTarget's own conflated +0x18/+0x19 member) is follow-on
        // work owned by the postfx TU; nothing calls this form.
        static RenderTarget* Initialize(const Target::Parameters* lpParameters);

        // X360 0x824088B0 -- construct the render-target into `this` from the parameters (the X360
        // out-of-line constructor body; returns `this`).
        RenderTarget* Construct(const Target::Parameters* lpParameters);

        // X360 0x823F9338 -- resolve the colour and/or depth surfaces back to their textures.
        void Resolve(bool lbResolveDepthStencil, bool lbResolveColour);

        // X360 0x824037E8 -- build the per-section RenderTargetState and (when the colour surface wants
        // one) the colour-sampling TextureState.
        void CreateStates(const Target::Parameters* lpParameters);

        // --- layout (X360 4-byte-pointer image; size 0x98). Reached by name; offsets are docs. -------
        rw::IResourceAllocator*          mpAllocator;          // +0x00
        u32                              muWidth;              // +0x04
        u32                              muHeight;             // +0x08
        u32                              muReserved0C;         // +0x0C
        u32                              muColourMode;         // +0x10  RenderTargetMode
        u32                              muDepthStencilMode;   // +0x14  RenderTargetMode
        u8                               mu8HasColour;         // +0x18
        u8                               mau8Pad19[3];         // +0x19
        renderengine::RenderTargetState* mpSection0State;      // +0x1C  CreateStates' section-0 state
        Target                           maColourTargets[3];   // +0x20  (3 * 0x18)
        Target                           mDepthTarget;         // +0x80
        renderengine::RenderTargetState* mpProvidedState;      // +0x84  provided state (USE_PROVIDED)
        u32                              muProvidedTextureId;  // +0x88
        // ⚠ MISNAMED, and the name is load-bearing enough to be worth this note (2026-08-15).
        // The X360 word at +0x8C is `mDepthTarget.mpTextureState` -- the DEPTH sampler state --
        // not a colour one: mDepthTarget starts at +0x80 and a Target record is 0x18 bytes, so
        // the "provided state / provided texture id / colour texture state / provided colour
        // state" quartet modelled at +0x84..+0x90 is the SAME storage as that record's
        // pixel-buffer / texture / texture-state / hi-Z fields. RenderTarget::CreateStates
        // @0x82403A18-0x82403B44 settles it: the state stored here binds mDepthTarget's hi-Z-else-
        // depth texture, and it is built ONLY when Parameters::mbUseDepthStencilAsTexture is set.
        // PostFxRenderTargetPCLeaf.cpp therefore points BOTH host members at the one state object
        // and new callers should use GetDepthTextureState(). RENAME PENDING: this member and its
        // one reader (BrnPostFx.cpp:813, the SamplerDepth argument) belong to the post-fx TU.
        renderengine::TextureState*      mpColourTextureState; // +0x8C  == mDepthTarget.mpTextureState
        renderengine::RenderTargetState* mpProvidedColourState;// +0x90
        u8                               mu8UsesProvidedState; // +0x95
        u8                               mau8Pad96[2];         // +0x96

        // FLAG PC-platform leaf: the per-section render-target-state ARRAY, given a real name.
        // The X360 reaches section n's state as `(&mpSection0State)[n]` -- a pointer-array PUN that
        // walks off mpSection0State (+0x1C) into maColourTargets/mDepthTarget on the 4-byte-pointer
        // image, where those members happen to line up on a 4-byte grid. That pun CANNOT survive the
        // x64 widening (the Target records are no longer a whole number of pointers wide), and
        // CreateBackBuffer legitimately reads/writes section 4, so replicating it would corrupt the
        // object. The five slots therefore get their own array here; mpSection0State above stays as
        // the console's documented +0x1C member and mirrors slot 0.
        // Five slots because the X360 caller set uses 0 (the bind) and 4 (CreateBackBuffer's shared
        // depth-stencil state).
        static const u32 KU_NUM_SECTION_STATES = 5;
        renderengine::RenderTargetState* mapSectionState[KU_NUM_SECTION_STATES];
    };

    // The engine's default render-target state (X360 dword_83271614, installed by
    // renderengine::Device::Start). Used as section 0's state for targets that have none of their own.
    extern renderengine::RenderTargetState* gpDefaultRenderTargetState;
}
}
}
