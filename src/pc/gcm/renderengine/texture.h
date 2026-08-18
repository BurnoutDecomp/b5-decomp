#pragma once

#include "types.hpp"

// renderengine::Texture / Texture2D - the platform render-engine texture objects. The
// PC backend wraps a D3D9 texture; only the surface the in-scope renderers use is
// declared: the 2D-texture create path (GetResourceDescriptor + Initialize) and the
// lock/unlock upload path. Matches the renderengine resource-descriptor convention
// used by renderstates.h.
struct IDirect3DBaseTexture9;
namespace rw { struct Resource; }   // backing resource memory for the rw-resource create path

namespace renderengine
{
    class Texture
    {
    public:
        // X360 renderengine::Texture::GetType @0x82B60E68 dimension classes. The X360 spine decodes
        // them out of the GPU texture header; renderengine::Texture::GetParameters @0x82B60D80 maps
        // XGGetTextureDesc's resource type onto the SAME five values and stores the result in
        // Parameters WORD 0 -- asm 0x82B60DB0-0x82B60E1C, read instruction by instruction:
        //     desc type 0x14 -> 0 (line)    0x13 -> 2 (array)   0x12 -> 3 (cube)
        //     desc type 0x11 -> 4 (volume)  3    -> 1 (2D)      anything else -> -1
        // and rw::graphics::postfx::Tint::Initialize @0x82403B48 writes 4 into that word by hand
        // (`li r10, 4` @0x82403B68 -> `stw r10, 0x1A0+var_C0(r1)` @0x82403BB0).
        //
        // The PC backend CREATES three of them: E_TYPE_2D (IDirect3DTexture9); E_TYPE_VOLUME
        // (IDirect3DVolumeTexture9 -- the muSize^3 colour-lookup cube), since the post-fx step-10
        // tint wave; and E_TYPE_CUBE (IDirect3DCubeTexture9), since the reflections step-1 wave --
        // both the 234 shipped 64x64x6 world reflection rasters and the env-map RENDER target the
        // 22 vehicle pixel shaders sample at s13. LINE / ARRAY still have no create path here.
        //
        // ⚠ THE DIMENSION IS *CARRIED*, NEVER DERIVED (fix round, 2026-08-16). The first cut of the
        // volume path used `Parameters::muDepth > 1` as the discriminant. On this data that is
        // wrong: the serialised depth byte is the CUBE FACE COUNT of an X360 STACK texture --
        // tools/assets/bundles/x360_tex.py:128-140 decodes GPUTEXTURESIZE with the dimension as its
        // type ("STACK carries the cubemap face count in Depth", `if t == 3: d += (size_packed >>
        // 26) & 0x3F`) and :378 writes it out verbatim (`out[0x24] = max(1, f['depth']) & 0xFF`).
        // CENSUS over the shipped data: of 26,614 RwRaster resources under build/game, 234 have
        // that byte > 1 -- every track unit's DXT1 64x64x6 cube map among them -- and 0 are volumes
        // (scratch/postfx_step10_finish/tintfix/work/raster_dimension_census.py). So Create, Lock,
        // Unlock and GetType all branch on the CREATE-TIME dimension (Parameters::meType, recorded
        // in the raster header's mu8Dimension byte); nothing branches on muDepth ALONE.
        //
        // ⚠ ONE NARROW EXCEPTION, ADDED 2026-08-17 (reflections step 1) AND FENCED: a raster that
        // has NEVER been through Create carries mu8Dimension == 0, and the shipped bundles are
        // exactly that -- so the 234 CUBE rasters have to be recognised from the SERIALISED header
        // or they keep uploading face 0 as a 2D texture into a samplerCUBE slot (garbage/black
        // window and river reflections). The single classifier `TextureDimension()` in texture.cpp
        // therefore falls back, ONLY when mu8Dimension is 0, to the exact shape the converter
        // writes for a GPUDIMENSION_CUBEMAP: muDepth == 6 AND muWidth == muHeight != 0. This is
        // NOT the retired `muDepth > 1` volume test: it is a three-part test for a CUBE, it is the
        // ONLY dimension it can produce, and it is MEASURED to select exactly the 234 and nothing
        // else -- re-run census (scratch/reflections_step1/cubeleaf/work/cube_raster_census.py):
        //     depth histogram over all 26,614 rasters : {0: 51, 1: 26329, 6: 234}
        //     of the 234, NOT (depth==6 and width==height) : 0
        // and the converter only ever writes a depth above 1 for the cube dimension
        // (tools/assets/bundles/x360_tex.py:128-140, `if t == 3: d += (size_packed >> 26) & 0x3F`,
        // t == 3 == GPUDIMENSION_CUBEMAP; :378 writes it verbatim).
        // DELETE-WHEN the converter writes an explicit dimension byte at +0x28 (it is free -- the
        // byte is zero in 26,614 of 26,614 shipped rasters); see the CROSS-GROUP note in the
        // reflections step-1 cubeleaf report.
        //
        // The fixed signed underlying type is not decoration: GetParameters' unknown-type arm is
        // `li r11, -1` and CgsRwRasterResourceTypePS3.cpp seeds that same -1, which for a plain
        // enum with enumerators 0..4 would be an out-of-range (unspecified) value.
        enum Type : s32
        {
            E_TYPE_UNKNOWN = -1,  // X360 default arm (`li r11, -1` @0x82B60DD8)
            E_TYPE_LINE   = 0,    // X360 desc type 0x14
            E_TYPE_2D     = 1,    // X360 desc type 3
            E_TYPE_ARRAY  = 2,    // X360 desc type 0x13
            E_TYPE_CUBE   = 3,    // X360 desc type 0x12
            E_TYPE_VOLUME = 4     // X360 desc type 0x11
        };

        // renderengine::Texture::LockInfo -- a PC-ONLY LEAN DESCRIPTOR, and this banner exists so
        // the next reader does not mistake it for a console type. The DecFIGS DWARF declares
        // exactly ONE lock pair on this class
        // (references/DecFIGS/dwarfdump/SDKs/EATech/include/ps3/gcm/renderengine/texture.h:271/274)
        //     bool Lock(uint32_t, uint32_t, uint32_t, renderengine::Texture::Locked &);
        //     void Unlock(const renderengine::Texture::Locked &);
        // and the X360 image agrees (Lock @0x82B62B20 / Unlock @0x82B62CB8 both take the 28-byte
        // Locked below). LockInfo is this tree's own {bits, pitch} shorthand for the three PC
        // callers that want nothing else (CgsMoviePlayer, BrnLoadingScreenRenderer,
        // BrnFlaptRenderer); it is a STRICT SUBSET of Locked, field-for-field.
        //
        // ⚠ THE TWO TRAILING BYTES ARE NOT NEW STATE -- THEY ARE Locked's OWN (added 2026-08-17,
        // reflections step 2, closing cubeleaf run-2 F7). Before them, Lock honoured liLevel and
        // liFace while Unlock unlocked level 0 / D3DCUBEMAP_FACE_POSITIVE_X, because the descriptor
        // could not carry them: lock face 4 of a cube, unlock face 0, and D3D9 leaves face 4 locked
        // for ever with no error at either call site. The console has no such hole -- its Unlock
        // reads `lbz r5, 0x12(r4)` (Level) and `lbz r4, 0x13(r4)` (FaceType/ArrayIndex) straight
        // off the descriptor, which are Locked::mu8MipLevel and Locked::mu8Index. Same names here,
        // written by Lock and read by Unlock, so the pair is symmetric BY NAME.
        struct LockInfo
        {
            void* mpBits;
            u32   muPitch;
            u8    mu8MipLevel;   // == Locked::mu8MipLevel (console descriptor +0x12)
            u8    mu8Index;      // == Locked::mu8Index     (console descriptor +0x13)
        };

        // renderengine::Texture::Locked - the descriptor a Lock fills: where the locked surface's
        // pixels live plus its geometry. Layout from the DecFIGS DWARF (EATech renderengine
        // texture.h:132); the X360 CgsNetworkImageConverter unpack path reads pixelData (+0x04),
        // stride (+0x08) and height (+0x0E) off it. Pointers widen to the x64 target.
        struct Locked
        {
            Texture* mpTexture;     // +0x00
            void*    mpPixelData;   // the locked surface's bits
            u32      muStride;      // row pitch in bytes
            u16      muWidth;
            u16      muHeight;
            u16      muVolumeDepth;
            u8       mu8MipLevel;
            u8       mu8Index;
            u32      muSliceStride;
            u32      muLockFlags;
        };

        // ⚠ THE PARAMETER ORDER HERE IS THE PC LEAF'S, *NOT* THE CONSOLE'S -- read this before
        // adding a caller or porting one. X360 renderengine::Texture::Lock @0x82B62B20 is
        //     Lock(texture, FLAGS, LEVEL, FACE, out)
        // -- r4 is the FLAG word (`mr r25, r4` @0x82B62B34 then `rlwinm. r11, r25, 0,30,30`
        // @0x82B62B4C: bit 0x2 CLEAR is what takes the read-only branch, `li r28, 0x10` =
        // D3DLOCK_READONLY), r5 is the mip LEVEL (`mr r4, r29 # Level` into D3DVolumeTexture_LockBox
        // @0x82B62BA0 and `mr r5, r29 # Level` into each D3D*Texture_LockRect), r6 is the face /
        // array index, r7 is the out descriptor.
        // THIS leaf takes (level, face, flags) and hands liFlags to D3D9 verbatim as the D3DLOCK_*
        // word, so the console's flags word 2 ("writable", i.e. do NOT take the read-only branch) is
        // spelled here as liFlags = 0. Reading the console's `li r4, 2` as a LEVEL is what made the
        // tint LUT lock mip 2 of a one-level volume and write nothing at all (step-10 fix round).
        static void Lock(Texture* lpTexture, s32 liLevel, s32 liFace, s32 liFlags, LockInfo* lpLockInfoOut);
        // Unlocks the level/face the matching Lock recorded in the descriptor -- see the LockInfo
        // banner. (The lean overload; the console's own entry point is the Locked one below.)
        static void Unlock(Texture* lpTexture, LockInfo* lpLockInfo);

        // THE CONSOLE'S OWN Unlock -- X360 @0x82B62CB8, DWARF `void Unlock(const Locked&)`
        // (texture.h:274). It dispatches on GetType and reads the LEVEL and the FACE/ARRAY INDEX
        // out of the descriptor for every shape:
        //     GetType 0 line   : D3DLineTexture_UnlockRect  (this, Level=lbz 0x12)
        //     GetType 1 2D     : D3DTexture_UnlockRect      (this, Level=lbz 0x12)
        //     GetType 2 array  : D3DArrayTexture_UnlockRect (this, ArrayIndex=lbz 0x13, Level=0x12)
        //     GetType 3 cube   : D3DCubeTexture_UnlockRect  (this, FaceType=lbz 0x13,   Level=0x12)
        //     GetType 4 volume : D3DVolumeTexture_UnlockBox (this, Level=lbz 0x12)
        // +0x12 / +0x13 are Locked::mu8MipLevel / Locked::mu8Index (Lock writes them: `*(a5 + 18)
        // = a3` / `*(a5 + 19) = a4` @0x82B62B20). rw::graphics::postfx::Tint::EndBlendJob reaches
        // this form through BrnPostFx::Render 0x8240A4EC.
        static void Unlock(Texture* lpTexture, Locked* lpLocked);

        // X360 Lock @0x82B62B20 overload: fill the full Locked descriptor (texture +
        // surface bits + geometry), not just the lean {bits,pitch} LockInfo. The
        // CgsNetworkImageConverter unpack path and BrnNetworkPlayerImageRenderer::Prepare
        // pass a Texture::Locked* here; ADDITIVE GROW over the LockInfo overload so the
        // existing lean callers (CgsMoviePlayer / BrnLoadingScreenRenderer) are unchanged.
        static void Lock(Texture* lpTexture, s32 liLevel, s32 liFace, s32 liFlags, Locked* lpLockedOut);

        // X360 Destruct @0x82B62D50: release a created texture's GPU/D3D resources in
        // place (the rw-resource sibling of Destroy; BrnNetworkPlayerImageRenderer::Release
        // destructs every buffered texture). ADDITIVE GROW.
        static void Destruct(Texture* lpTexture);

        // The serialised raster parameters: GetParameters fills them from a Texture, then
        // GetResourceDescriptor sizes the resource from them. Field set + order match the X360
        // renderengine::Texture Parameters (a _DWORD[8]); on X360 GetParameters reads them back
        // from the GPU texture descriptor (XGGetTextureDesc), on PC it reads the stored raster
        // header below (the PC D3D9 raster keeps format/size explicitly, having no GPU desc).
        //
        // ⚠ WORD 0 IS THE DIMENSION AND WORD 6 IS THE FORMAT -- CORRECTED 2026-08-16 off the X360
        // asm, and that correction is what makes the volume path safe. GetParameters @0x82B60D80
        // stores, in this order: word0 = the dimension class (or -1 for an unrecognised resource
        // type), word1 = sysmem (always 0 out of GetParameters), word2/word3 = width/height
        // (`stw r11, 8(r31)` / `stw r9, 0xC(r31)`), word4 = depth (`stw r9/r11, 0x10(r31)`),
        // word5 = ((*(a1+44) >> 6) & 0xF) + 1 = levels (`stw r11, 0x14(r31)`), word6 = FORMAT
        // (`stw r8, 0x18(r31)`, r8 = the texture desc's format field). Tint::Initialize @0x82403B48
        // fills the same eight words by hand: word0 = 4 (E_TYPE_VOLUME), word2..4 = muSize,
        // word5 = 1, word6 = 0x18280186 (the console's GPU spelling of 8_8_8_8, PC D3DFMT_A8R8G8B8).
        // The tree used to call word0 "miFormat" and word6 "muReserved0", which is exactly why the
        // console's 4 once arrived at CreateTexture as D3DFORMAT(4). Every member below is reached
        // BY NAME at every call site, so putting miFormat on its real word changes no meaning.
        struct Parameters
        {
            Type meType = E_TYPE_2D;   // +0   the DIMENSION class. DEFAULTED so that a caller that
                                       //      never heard of volumes cannot accidentally ask for one
            u32 muSysMem;     // +4   bit0 set => system-memory texture (no graphics-local pixels)
            u32 muWidth;      // +8
            u32 muHeight;     // +12
            u32 muDepth;      // +16  a VOLUME's depth -- and, for a serialised X360 STACK raster,
                              //      the CUBE FACE COUNT, which is why it is not a dimension test
            u32 muNumLevels;  // +20
            s32 miFormat;     // +24  D3DFORMAT on PC (-1 = none); the GPU format word on the X360
            u32 muReserved1;  // +28
        };

        // Faithful to the X360 renderengine::Texture descriptor path:
        //   GetParameters          0x82B60D80
        //   GetResourceDescriptor  0x823FF848  (writes rw::BaseResourceDescriptors<5>: slot0 =
        //                          the Texture object, slot2 = the page-aligned pixel data)
        //   GetPixelDataSize       0x82B61088
        // lpDescriptorOut points at the five {u32 size, u32 align} entries (ten u32s); taken as
        // u32* so this header need not pull in the rw resource-descriptor template.
        static void GetParameters(const Texture* lpTexture, Parameters* lpParamsOut);
        static void GetResourceDescriptor(u32* lpDescriptorOut, const Parameters* lpParams);

        // ⚠ SIGNATURE CORRECTED 2026-08-17 (reflections step 1) -- THE DIMENSION IS THE FIRST
        // ARGUMENT, AND IT IS RECOVERED, NOT ADDED FOR CONVENIENCE. X360 GetPixelDataSize
        // @0x82B61088 is a six-argument forwarder into an anonymous-namespace helper:
        //     Xbox2SetTextureHeader(0, a2, a3, a4, a6, a5, a1, &out);   // pseudocode
        //     0x82B6109C  mr r11, r7 ; 0x82B610A0  mr r9, r3            // r9 == a7 == a1
        // and that helper (`anonymous namespace'::Xbox2SetTextureHeader @0x82B60B98) SWITCHES ON
        // ITS 7th ARGUMENT -- i.e. on GetPixelDataSize's FIRST:
        //     case 3: XGSetCubeTextureHeader(a2, a5, 0, a6, 0, 0, -1, a1)     // CUBE
        //     case 4: XGSetVolumeTextureHeader(a2, a3, a4, a5, 0, a6, 0, 0)   // VOLUME
        //     case 2: XGSetArrayTextureHeader (a2, a3, a4, a5, 0, a6, 0, 0)   // ARRAY
        //     default: a7 ? XGSetTextureHeader(...) : XGSetLineTextureHeader(...)
        // The remaining two positions fall out of the same call: the helper's Levels slot is fed
        // GetPixelDataSize's a6 and its Format slot its a5, so the console order is
        // (Type, Width, Height, Depth, Format, Levels) -- which is what this declaration now is.
        // The old PC spelling (liFormat, w, h, depth, levels) had no dimension at all and so sized
        // every CUBE with the VOLUME rule (depth halved per level): 14,008 bytes for a 64x64x6
        // mips=7 raster whose shipped pixel block is 16,464 -- 2,456 short, on the number
        // GetResourceDescriptor puts in slot 2. MEASURED: 234/234 shipped cube rasters match
        // SUM(2D mip chain) * 6; 3/234 matched the old formula (the mips==1 shape, where the two
        // coincide). Note XGSetCubeTextureHeader takes ONE edge + levels + format and no depth --
        // six faces, each carrying the whole 2D chain, the face count never halved.
        static u32  GetPixelDataSize(Type meType, u32 luWidth, u32 luHeight, u32 luDepth,
                                     s32 liFormat, u32 luNumLevels);

        // Dimension accessors (X360 GetType @0x82B60E68, GetWidth @0x82B60EC8, GetHeight @0x82B60F38,
        // GetDepth @0x82B60FA8): the X360 spine decodes the GPU texture-fetch-constant header; the PC
        // raster stores width/height/depth/levels explicitly (the same precedent GetParameters uses),
        // so these read the stored header. Static (take the texture by pointer) to match the X360
        // call form `GetWidth(texture)`. Bodied in the renderengine texture TU.
        static Type GetType(const Texture* lpTexture);
        static u32  GetWidth(const Texture* lpTexture);
        static u32  GetHeight(const Texture* lpTexture);
        static u32  GetDepth(const Texture* lpTexture);

        // X360 GetFormat @0x82B61000: on the X360 spine this re-packs the GPU texture-fetch-constant
        // swizzle/format bitfields back into a D3DFORMAT; the PC raster keeps the D3DFORMAT explicitly
        // (miFormat), so this returns it directly. Used by the screenshot / massive-texture paths.
        static s32  GetFormat(const Texture* lpTexture);

        // X360 Release @0x82B62E18: tear the texture's GPU resources down (Destruct) and clear its
        // live bit, leaving the object zeroed for reuse. The rw-resource sibling of Destroy.
        static Texture* Release(Texture* lpTexture);

        // X360 Xbox2CheckPhysicalMemoryFlags @0x82B60D28: query the physical-memory protection of the
        // texture's pixel page and set/clear the "write-combined" header flag accordingly. An X360
        // EDRAM/physical-memory operation; on PC (managed D3D pool, no physical page exposed) it is a
        // no-op returning 0. Called by Texture::Initialize and the post-fx depth-target create.
        static u32  Xbox2CheckPhysicalMemoryFlags(Texture* lpTexture);

        // Realise the texture: create the managed D3D9 texture into mpD3DTexture from the
        // parameters and (if lpPixelData != null) upload the mip chain. This is the PC body of
        // CgsResource::RwRasterResourceType::FixUp (the console FixUp rebases packed GPU offsets;
        // the PC raster instead creates the D3D texture). Destroy is the matching FixDown body.
        static void Create(Texture* lpTexture, const Parameters* lpParams, const void* lpPixelData);
        static void Destroy(Texture* lpTexture);

        // 0x82403D-region create path: build the texture object into the rw resource memory from the
        // serialised parameters and return it (the rw-resource sibling of TextureState::Initialize;
        // the post-fx Tint lookup-texture create calls this). Body lives in its own renderengine TU.
        static Texture* Initialize(rw::Resource* lpResourceMemory, const Parameters* lpParams);

        // [PC diagnostic -- NOT an X360 function] Read back texel (0,0) of mip 0 of a 2D
        // render-target texture through a system-memory staging surface (GetRenderTargetData +
        // LockRect). It STALLS the GPU; for ONE-SHOT diagnostics only (the sun corona's 1x1
        // occlusion buffer is the first user: verify_suncorona F1 -- the one number that says
        // whether that pass works lives only on the GPU). Returns false when the texture is not a
        // readable 2D render target (multisampled, cube, volume, no device). *lpuTexel receives
        // the raw first 4 bytes of the texel; *lpiFormat the D3DFORMAT, so the caller can decode.
        static bool PCReadBackTexel0(const Texture* lpTexture, u32* lpuTexel, s32* lpiFormat);

        // --- Burnout PC raster header (wiki page "Texture/Burnout Paradise/PC"; field order is
        // the wiki's, offsets are the x64 target's). mpD3DTexture is the wiki +0 "texture data"
        // pointer (the D3D texture on the PC backend); SetTexture / Lock bind it directly. ----
        IDirect3DBaseTexture9* mpD3DTexture;        // wiki +0x00  texture data (D3D texture)
        void*    mpTextureDataStruct;               // wiki +0x04  -> struct holding the data ptr
        void*    mpReserved8;                        // wiki +0x08
        bool     mbFlagC;                            // wiki +0x0C
        bool     mbFlagD;                            // wiki +0x0D
        bool     mbFlagE;                            // wiki +0x0E
        bool     mbFlagF;                            // wiki +0x0F
        s32      miFormat;                           // wiki +0x10  D3DFORMAT
        u16      muWidth;                            // wiki +0x14
        u16      muHeight;                           // wiki +0x16
        u8       muDepth;                            // wiki +0x18  (a STACK raster's FACE COUNT)
        u8       muNumMipLevels;                     // wiki +0x19
        u16      muFlags;                            // wiki +0x1A

        // THE CREATE-TIME DIMENSION -- the one fact Lock / Unlock / GetType ask. Create writes it
        // from Parameters::meType on EVERY arm and nothing else ever writes it, so 0 (the
        // 2D answer) is also the right answer for a texture that never went through Create
        // (Texture2D::Initialize makes its D3D texture directly).
        //
        // ⚠ WIDENED FROM A BOOLEAN TO A DIMENSION 2026-08-17 (reflections step 1), same byte, same
        // offset, no new storage: it used to be `mu8IsVolumeTexture` (1 == volume). Now it holds
        // the renderengine::Texture::Type VALUE, with the one encoding rule that 0 means E_TYPE_2D
        // -- which is what the serialised image and every never-Create'd object already contain,
        // and is why the widening costs nothing. So: 0 = 2D, 3 = E_TYPE_CUBE, 4 = E_TYPE_VOLUME.
        // (E_TYPE_LINE is also 0 and E_TYPE_ARRAY is 2, but neither has a create path, so neither
        // can be recorded; Create stores 0 for anything it did not itself create as cube/volume,
        // rather than storing a dimension it did not build.) A second boolean would have worked
        // too and was rejected: two flags for one mutually-exclusive fact is how the next
        // dimension gets a third, and the branch `if (isVolume) ... else if (isCube) ...` is
        // exactly the shape whose ordering can be got wrong silently.
        //
        // WHY ITS OWN FIELD RATHER THAN A BIT IN muFlags -- two measured reasons. (1) muFlags is
        // not spare in the CODE: the console's depth/hi-Z path writes the EDRAM TILE word through
        // that slot -- rw::graphics::postfx::Target::CreateDepth @0x82403688 does `lwz r11, 0x20(r3)`
        // / `srwi 12` / `insrwi 20,0` / `stw r10, 0x20(r3)`, carried in rwgpfxrendertarget.cpp as
        // `lpHiZ->muFlags = ((lpDepthTex->muFlags >> 12) << 12) | (lpHiZ->muFlags & 0xFFF)`.
        // (2) muFlags is not spare in the DATA either: 51 shipped PARTICLES.BUNDLE rasters carry a
        // non-zero muFlags (0x7fe0 / 0x3fe0 / 0xffe0 / 0x07e0 / 0x1fe0) -- their serialised object
        // is 0x40 bytes, not the converter's 0x30.
        //
        // WHY +0x28 IS SAFE ON SERIALISED DATA: it is zero in EVERY shipped raster and inside every
        // shipped payload. MEASURED over all 26,614 rasters under build/game -- object sizes are
        // 0x30 (26,563) or 0x40 (51), so +0x28 is always in bounds, and the byte there is 0 in
        // 26,614 of 26,614; 234 would have taken the volume arm under the retired depth test
        // (scratch/postfx_step10_finish/tintfix/work/raster_dimension_census.py). So every bundle
        // raster keeps the 2D path by construction, which is exactly the property the depth-derived
        // discriminant did not have.
        u8       mu8Dimension;                       // +0x28  0 => 2D, 3 => CUBE, 4 => VOLUME
    };

    class Texture2D : public Texture
    {
    public:
        struct Parameters
        {
            u32 muWidth;
            u32 muHeight;
            u32 muDepth;
            u32 muNumLevels;
            u32 muFormat;
            u32 muUsage;
            u32 mauReserved[2];
        };

        struct ResourceDescriptor
        {
            u32 mauData[10];
        };

        static ResourceDescriptor* GetResourceDescriptor(ResourceDescriptor* lpDescriptorOut,
                                                         const Parameters* lpParams);
        static Texture2D* Initialize(const ResourceDescriptor* lpDescriptor, const Parameters* lpParams);

        // X360 Xbox2GetPhysicalMemorySize @0x82B610D8: lay out a GPU texture header for the
        // serialised descriptor and return its size (XGSetTextureHeader's HRESULT-sized result).
        // The X360 GetResourceDescriptor calls this to size slot2 of the resource descriptor when
        // the descriptor is NOT system-memory (bit0 of word0 clear). The input is the X360
        // descriptor dword block (word1=Width, word2=Height, word3=Levels, word4=Format); taken as
        // const u32* because the X360 marshalling reads it positionally and its field layout is the
        // X360 descriptor's, not the PC Texture2D::Parameters above. ADDITIVE GROW (X360 path only).
        static u32 Xbox2GetPhysicalMemorySize(const u32* lpDescriptorDwords);
    };

    // renderengine::PixelBuffer -- the platform render-target / depth-stencil SURFACE object. On
    // X360 a PixelBuffer wraps a GPU D3DSurface header (a tiled EDRAM surface) and carries the
    // EDRAM placement: Initialize lays the surface header out in place via XGSetSurfaceHeader and
    // sub-allocates EDRAM tiles + a hierarchical-Z region from the two running EDRAM allocators;
    // Xbox2SetBaseEDRAM / Xbox2SetBaseHierarchicalZ patch the EDRAM base words; Xbox2ResolveTo
    // resolves the tiled EDRAM surface out to a linear texture. These are X360 EDRAM/tile address
    // & format operations (NOT VMX pipelines). Reconstructed store-for-store from the X360 ASM:
    //   Initialize                  0x82B621A8
    //   Xbox2ResolveTo              0x82B62300
    //   Xbox2SetBaseEDRAM           0x82B60870
    //   Xbox2SetBaseHierarchicalZ   0x82B60898
    // PixelBuffer derives from Texture (it is a render-engine texture object); the surface header
    // it manages is a separate GPU-resident block reached through the wrapper.
    class PixelBuffer : public Texture
    {
    public:
        // The on-GPU surface header XGSetSurfaceHeader fills, 52 (0x34) bytes (memset(*a1,0,0x34)).
        // Named after the dwords the renderengine surface paths read/write; the GPU writes the rest.
        // muEDRAMBase (+0x1C) low 12 bits = EDRAM tile base; muHierZ (+0x20) high bits (<<17) =
        // hi-Z base; muTileWord (+0x24) packs the tiled width(<<18)/height(<<14 ,15 bits) the
        // resolve path reads back; muKind (+0x30) = 1 for a depth-stencil surface, else colour.
        struct SurfaceHeader
        {
            u32 muCommon;      // +0x00  D3DResource Common (bit31 0x80000000 set => EDRAM-resident)
            u32 muUnused04;    // +0x04
            u32 muUnused08;    // +0x08  (Initialize zeroes this)
            u32 muUnused0C;    // +0x0C
            u32 muUnused10;    // +0x10
            u32 muFence;       // +0x14  (Initialize seeds 0xFFFF0000)
            u32 muUnused18;    // +0x18
            u32 muEDRAMBase;   // +0x1C  low 12 bits = EDRAM tile base address
            u32 muHierZ;       // +0x20  high 15 bits (<<17) = hierarchical-Z base
            u32 muTileWord;    // +0x24  packed tiled width (>>18) / height (>>14, 15 bits)
            u32 muUnused28;    // +0x28
            u32 muUnused2C;    // +0x2C
            u32 muKind;        // +0x30  surface kind (1 = depth-stencil, else colour render target)
        };

        // The outer wrapper Initialize is handed: +0 -> the GPU surface header to fill.
        struct Wrapper
        {
            SurfaceHeader* mpSurface;   // +0x00
        };

        // The parameters block Initialize reads (a2[0..6]):
        //   word0 = kind (1 => depth-stencil; drives the EDRAM-tile reservation)
        //   word1 = flags (bit0 set => no EDRAM placement, just a header)
        //   word2 = width, word3 = height, word4 = format, word5 = multisample type, word6 = mip base
        struct Parameters
        {
            u32 muKind;          // +0x00
            u32 muFlags;         // +0x04
            u32 muWidth;         // +0x08
            u32 muHeight;        // +0x0C
            u32 muFormat;        // +0x10
            u32 muMultiSample;   // +0x14
            u32 muMipBase;       // +0x18
        };

        static SurfaceHeader* Initialize(Wrapper* lpWrapper, const Parameters* lpParams);

        // Resolve the tiled EDRAM surface (lpThis) out to a linear destination texture, clipping the
        // resolve rect to the destination texture's width/height. luFlags is the D3D resolve-flags
        // word; lfClearZ the clear-Z value; liDestX/liDestY the dest point and liDestLevel/
        // liDestSlice the destination mip / slice. Returns 1 (the X360 body's constant return).
        //
        // TWO PARAMETERS CORRECTED 2026-08-13 (post-fx frame-bracket wave), both from the X360 asm
        // at 0x82B62300:
        //   * lpClearColour is a POINTER, not an s32. The incoming 8th GPR argument (r10, saved by
        //     `mr r25, r10` @0x82B6231C) is forwarded verbatim into the pClearColor position of both
        //     outgoing calls -- `mr r7, r25 # pClearColor` @0x82B623A4 and `mr r10, r25 #
        //     pClearColor` @0x82B62450 -- and is never treated as a value. Both current callers pass
        //     null. A 32-bit console word is not a host address, so this must not be bridged through
        //     an s32 on the LLP64 target.
        //   * luClearStencil is the TENTH parameter and was missing. The body reads it from the
        //     first incoming overflow slot (`lwz r9, 0xE0+arg_5C(r1)` @0x82B623A0 and
        //     `lwz r10, 0xE0+arg_5C(r1)` @0x82B62428 -- r1+0x5C is the right-justified word of slot
        //     0x58, i.e. argument 10, since ClearZ rides f1 and reserves no slot) and forwards it to
        //     D3DDevice_EndTiling's ClearStencil and to D3DDevice_Resolve's own 0x5C. Both callers
        //     supply 0 (`stw r5, 0x70+var_14` @0x823F914C and `stw r5, 0x80+var_24` @0x823F9388,
        //     r5 = 0 in both).
        static int Xbox2ResolveTo(SurfaceHeader* lpThis, Texture* lpDestTexture, u32 luFlags,
                                  s32 liDestX, s32 liDestY, s32 liDestLevel, s32 liDestSlice,
                                  const void* lpClearColour, f32 lfClearZ, u32 luClearStencil);

        // Patch the EDRAM tile base (low 12 bits of muEDRAMBase) -- only for colour/depth kinds
        // (muKind <= 1); higher kinds are left untouched.
        static SurfaceHeader* Xbox2SetBaseEDRAM(SurfaceHeader* lpThis, s16 li16Base);

        // Patch the hierarchical-Z base (high 15 bits, <<17, of muHierZ).
        static SurfaceHeader* Xbox2SetBaseHierarchicalZ(SurfaceHeader* lpThis, s32 liBase);
    };
}
