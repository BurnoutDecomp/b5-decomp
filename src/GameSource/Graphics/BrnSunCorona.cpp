#include "GameSource/Graphics/BrnSunCorona.h"

#include <cstdio>    // snprintf (the three one-shot bring-up lines)
#include <cstring>   // memcpy (the constant rows -- the row pointer carries no alignment guarantee)
#include <cmath>     // std::sqrt (the sun-direction normalise)

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"             // CgsRenderTarget
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"        // saBlendStates[0] / [5]
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h" // saDepthStencilStates[1]
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"   // saRasterizerStates[2]
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"    // shadow::Device
#include "pc/gcm/renderengine/renderstates.h"                            // TextureState / DepthStencilState
#include "pc/gcm/renderengine/texture.h"                                 // Texture::PCReadBackTexel0 (the one-shot visibility read-back)
#include "pc/gcm/renderengine/SunCoronaProgramsPC.h"                     // the four authored PC images
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h" // BlendMaterialState

// =================================================================================================
// BrnSunCorona -- the sun flare, reconstructed from BURNOUT_X360_ARTIST.XEX and realised on the
// D3D9 backend.
//
//   Construct                  @0x824009B0
//   Destruct                   @0x823F8298
//   ComputeSunPositionOnScreen @0x823FD798
//   GenerateOcclusionBuffer    @0x82400CF8
//   RenderOccludedFlare        @0x824010A8
//
// WHAT THIS REWRITE CHANGES vs the previous committed body, and why each change is a defect fix and
// not a preference:
//
//  1. THE PHANTOM CONSTRUCTION SURFACE IS GONE. The file used to declare a LOCAL
//     `BuildVertexDescriptor` / `BuildProgramBuffer` / `LookupVariableHandle` /
//     `ReleaseVertexDescriptor` / `FreeResourceBlock` family against three invented opaque types
//     (`renderengine::VertexDescriptorObject` / `ProgramBufferObject` /
//     `ProgramVariableHandleObject`), with anonymous-namespace definitions that returned nullptr --
//     i.e. Construct produced an object with FIVE null pointers and asserted on four of them. Its
//     banner justified this with "the two committed renderengine::VertexDescriptor homes are
//     mutually exclusive to include", which is not true: coronas step 1 retyped exactly this class
//     of phantom externals in rwgcoronarenderer.cpp onto the committed
//     pc/gcm/renderengine/VertexDescriptor.h + states/programbuffer.h pair, and this file now does
//     the same. (rwgcoronarenderer.h includes BOTH headers together, so "mutually exclusive" was
//     already disproved in-tree.)
//
//  2. THE MICROCODE WALL IS CROSSED THE WAY THE CORONA AND SKY DOME CROSSED IT (AGENTS.md rule 2).
//     The console's four programs are executable-embedded Xenos microcode (&unk_8203E118 204 B,
//     &unk_8203E208 524 B, &unk_8203E438 240 B, &unk_8203E528 464 B) and cannot run on D3D9. PC
//     adopts an authored quartet through renderengine::ProgramBufferPC_Adopt (programbuffer.h:123,
//     body ImmediateModePCLeaf.cpp:833). The console GetResourceDescriptor/Initialize route is NOT
//     kept as a fallback: on this backend both bodies call XGGetMicrocodeShaderParts, whose PC stub
//     returns 0 WITHOUT writing *lpParts, and then read that uninitialised block -- a crash, not a
//     gap (CgsIm3dSkyDome.cpp's own banner).
//
//  3. THE VERTEX DESCRIPTOR HAS TWO ELEMENTS, NOT "SIX". The previous body carried
//     `KU_VTX_ELEMENT_COUNT = 6` with a comment reading "an element count of 6". SIX IS THE SECOND
//     ELEMENT'S TYPE, not a count: the X360 stores 1 at stack+0x2AB and 6 at stack+0x2BB, and the
//     Parameters::Element stride is 16 (0x2A0 -> 0x2B0), so both bytes are the +0x0B ELEMENT TYPE
//     lane of elements 0 and 1 -- POSITION0 and TEXCOORD0 in the dumped
//     gauVertexFormatDefaults table. There are exactly two elements and no element count is stored
//     anywhere. (The mistake was invisible: the value went into a parameter nothing read.)
//
// ---- THE SUN-CORONA VERTEX FORMAT (fully pinned, no dump needed) --------------------------------
// Construct's two Parameters elements, read off the asm at 0x82400A18-0x82400A4C through the
// GROUND-TRUTH lane table in pc/gcm/renderengine/VertexDescriptor.h:44-58 (the committed member
// NAMES are shifted one lane -- `miOffset` IS the format word, `mu8UsageIndex` IS the element
// type; writing them by name instead of by lane is the silent-total-failure trap coronas step 1
// documented as C4):
//
//   el | stream | format word | MapVertexFormat  | width | elementType | default usage | offset
//   ---+--------+-------------+------------------+-------+-------------+---------------+-------
//    0 |   0    | 0x2A23B9    | D3DDECLTYPE_FLOAT3  |  12 |     1       | POSITION 0    |   0
//    1 |   0    | 0x2C23A5    | D3DDECLTYPE_FLOAT2  |   8 |     6       | TEXCOORD 0    |  12
//                                                                          STRIDE = 20
//
// with the offset lane left at the ctor's 0xFFFF auto-pack sentinel, which is what produces 0/12.
// 20 is exactly the stride BOTH draw functions hand D3DDevice_BeginVertices (`li r6, 0x14`
// @0x82400E98 and @0x8240130C) and exactly the DWARF's
// `typedef renderengine::VertexIterator2<VertexTypeFloat3, VertexTypeFloat2>
//  BrnSunCoronaVertexIterator` (DecFIGS BrnSunCorona.cpp:29). THREE INDEPENDENT WITNESSES.
//
// Both format words are ALREADY in both PC mapping tables (ImmediateModePCLeaf.cpp:182/183,
// XenonD3D9Shims.cpp:323/324), so unlike the corona pair's 0x014C86 there is no missing case:
//     $ grep -n "0x2C23A5\|0x2A23B9" b5-decomp/src/pc/gcm/renderengine/ImmediateModePCLeaf.cpp \
//                                   b5-decomp/src/pc/gcm/renderengine/XenonD3D9Shims.cpp
//     ImmediateModePCLeaf.cpp:182:  case 0x2C23A5: *lpu8Type = D3DDECLTYPE_FLOAT2; *lpuWidth = 8;
//     ImmediateModePCLeaf.cpp:183:  case 0x2A23B9: *lpu8Type = D3DDECLTYPE_FLOAT3; *lpuWidth = 12;
//     XenonD3D9Shims.cpp:323:       case 0x2C23A5: *lpu8Type = D3DDECLTYPE_FLOAT2; return true;
//     XenonD3D9Shims.cpp:324:       case 0x2A23B9: *lpu8Type = D3DDECLTYPE_FLOAT3; return true;
//
// ---- THE TWO PASSES' DEVICE STATE (asm-proven; every object is a named factory slot) ------------
//   GenerateOcclusionBuffer @0x82400DA4-DCC        RenderOccludedFlare @0x82401144-70
//     sub_82276AD0(dword_83010910)                   sub_82276AD0(dword_83010910)
//        == saDepthStencilStates[1] ZOFF_ZALL_ZWRITEOFF   (same)
//     sub_82276B38(dword_83010A40)                   sub_82276B38(dword_83010A40)
//        == saRasterizerStates[2] Scissor_CullModeNone    (same)
//     sub_82276A68(dword_83010F70)                   sub_82276A68(dword_83010F84)
//        == saBlendStates[0] Opaque_Modulate...           == saBlendStates[5]
//                                                            Transparent_AdditiveRGB...DestRGB
// i.e. NEITHER pass depth-tests (the occlusion test is done by sampling depth in the pixel shader,
// not by the depth unit) and neither culls. The occlusion pass OVERWRITES its single pixel; the
// flare pass is ADDITIVE with SRCALPHA as the colour source factor -- which is how the occlusion
// fraction, carried in the flare pixel shader's ALPHA, reaches the frame.
// =================================================================================================

namespace
{
    // The two constant names Construct resolves (asm 0x82400BD0 / 0x82400CE8 -- the console's own
    // literals). The CTAB interned in the console packages declares exactly these two, at c0 on
    // their respective PIXEL programs (tools/assets/shaders/ctab.py; see brn_suncorona.fx).
    const u8 gszUvStartAndOffset[] = "kUvStartAndOffset";
    const u8 gszColourAndPower[]   = "kColourAndPower";

    // The Xenos vertex-format words Construct stores (see the banner table).
    const u32 KU_ELEMENT_FORMAT_FLOAT3 = 0x2A23B9u;
    const u32 KU_ELEMENT_FORMAT_FLOAT2 = 0x2C23A5u;

    // The element-TYPE lane values (+0x0B) -- 1 -> POSITION0, 6 -> TEXCOORD0 through
    // gauVertexFormatDefaults.
    const u8 KU8_ELEMENT_TYPE_POSITION  = 1u;
    const u8 KU8_ELEMENT_TYPE_TEXCOORD0 = 6u;

    // ---- Construct's scalar seeds (X360 store constants; every one is in DATA_DUMP.md) ----------
    //   flt_82001DA0 = 0.5        -> mfSunVectorYMultiplier  (stfs 0x34 @0x824009F8)
    //   flt_82001D9C = 2.0        -> mfOcclusionSize / mfSunFlarePow (0x38/0x3C @0x82400A04/A08)
    //   flt_82004740 = 0.30000001 -> mfSunBrightness / mfSunSize     (0x40/0x44 @0x82400A10/A14)
    //   flt_82001CC0 = 0.0        -> mfXPos / mfYPos                 (0x48/0x4C @0x824009DC/E0)
    const f32 KF_SUN_VECTOR_Y_MULTIPLIER = 0.5f;
    const f32 KF_OCCLUSION_SIZE          = 2.0f;
    const f32 KF_SUN_FLARE_POW           = 2.0f;
    const f32 KF_SUN_BRIGHTNESS          = 0.30000001f;
    const f32 KF_SUN_SIZE                = 0.30000001f;
    const f32 KF_X_POS                   = 0.0f;
    const f32 KF_Y_POS                   = 0.0f;

    // ComputeSunPositionOnScreen's own literals.
    //   flt_82006C48 = -10000.0 -- how far along -sunDir the sun is placed (@0x823FD7D4)
    //   flt_82001DA0 =  0.5 / flt_82004C78 = -0.5 / flt_82001CC0 = 0.0 -- the NDC->screen map and
    //   the z>0 comparand (@0x823FD90C / @0x823FD914 / @0x823FD91C)
    const f32 KF_SUN_DISTANCE            = -10000.0f;
    const f32 KF_NDC_TO_SCREEN_SCALE_X   =  0.5f;
    const f32 KF_NDC_TO_SCREEN_SCALE_Y   = -0.5f;
    const f32 KF_NDC_TO_SCREEN_BIAS      =  0.5f;

    // RenderOccludedFlare's quad literals.
    //   flt_82001D9C = 2.0 and flt_82001C98 = 1.0 -- the screen->NDC map `x*2 - 1` (@0x82401324/2C)
    //   flt_82009A78 = 1.7777778 -- a HARD-CODED 16:9 aspect on the flare's HEIGHT (@0x82401348).
    //     ⚠ It is a literal in the console image, not a camera read: the X360 renders 1280x720 and
    //     the flare is a screen-space square only at that aspect. It is reproduced verbatim rather
    //     than replaced by the PC's live aspect -- substituting renderengine::gDisplayWidth/Height
    //     would be a behaviour this binary does not have. What it costs on a non-16:9 PC display is
    //     stated in the report's risk table.
    const f32 KF_SCREEN_TO_NDC_SCALE     = 2.0f;
    const f32 KF_SCREEN_TO_NDC_BIAS      = 1.0f;
    const f32 KF_FLARE_ASPECT            = 1.7777778f;

    // The Xenos primitive type both draws pass to D3DDevice_BeginVertices (`li r4, 6`
    // @0x82400EA0 / @0x82401320) -- 6 == TRIANGLESTRIP, which D3D9 HAS (unlike the corona pass's
    // QUADLIST 13); XenonD3D9Shims.cpp:125 maps it straight to D3DPT_TRIANGLESTRIP. No expansion
    // is needed and none is done.
    const u32 KU_XENOS_PRIMITIVE_TRIANGLESTRIP = 6u;
    const u32 KU_QUAD_VERTEX_COUNT             = 4u;

    // The 20-byte vertex both programs are drawn with (see the banner). Pinned the same way the
    // corona vertex is: the layout MUST byte-match what VertexDescriptor::Initialize auto-packs.
    struct SunCoronaVertex
    {
        f32 mafPosition[3];   // +0x00  POSITION0  -- ALREADY IN NDC
        f32 mafUv[2];         // +0x0C  TEXCOORD0
    };
    static_assert(sizeof(SunCoronaVertex) == 20, "the sun-corona vertex is the console's 20-byte record");

    void SunCoronaLog(const char* lpcMessage)
    {
        CgsDev::Log::WriteToLog(lpcMessage);
    }

    // [FLAG PC bring-up gate] The three render-state objects both passes push. On the console
    // BrnSunCorona's callers run long after CgsBlendStateFactory / CgsRasterizerStateFactory /
    // CgsDepthStencilStateFactory::Construct; on PC those three run from a deferred bring-up block
    // near the top of BrnRendererModule::Render (BrnRendererModule.cpp, "the three RENDER-STATE
    // FACTORIES"), so a table slot can still be NULL and shadow::Device::SetState would dereference
    // it. Refuse the pass and say so once -- the same refusal, for the same reason, that
    // BrnCoronaManager::Construct already carries ("the blend / depth-stencil state factories are
    // not Prepared yet"). DELETE-WHEN BrnRendererModule::Construct constructs the three factories.
    struct SunCoronaPassStates
    {
        renderengine::DepthStencilState*   mpDepthStencil;
        renderengine::RasterizerState*     mpRasterizer;
        renderengine::BlendMaterialState*  mpBlend;
    };

    bool AcquirePassStates(u32 luBlendSlot, SunCoronaPassStates* lpStates)
    {
        lpStates->mpDepthStencil = CgsDepthStencilStateFactory::GetState(
            E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF);
        lpStates->mpRasterizer   = CgsRasterizerStateFactory::GetState(
            E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE);
        lpStates->mpBlend        = CgsBlendStateFactory::GetState(luBlendSlot);
        if (lpStates->mpDepthStencil != 0 && lpStates->mpRasterizer != 0 && lpStates->mpBlend != 0)
            return true;

        static bool sbLoggedStates = false;
        if (!sbLoggedStates)
        {
            sbLoggedStates = true;
            SunCoronaLog("[suncorona] pass DEFERRED: the blend / rasteriser / depth-stencil state"
                         " factories are not Constructed yet\n");
        }
        return false;
    }
}

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- open one shader-constant
// row and return the write cursor (X360 r3 @0x822768D0). The SHARED declaration-only spelling this
// tree standardised on: the same line appears at CgsIm2dUntex.cpp:82, CgsIm2dColTex.cpp:91,
// CgsIm3d.cpp:91, CgsIm3dSkyDome.cpp:113, CgsIm3dZOnly.cpp:70, BrnIm3d.cpp:118 and
// rwgcoronarenderer.cpp; the ONE definition is pc/gcm/renderengine/ImmediateModePCLeaf.cpp:729
// (mounted, build_game_exe.bat:316).
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

// The Xenon immediate-vertex ring intrinsics (defined for PC in
// pc/gcm/renderengine/XenonD3D9Shims.cpp:3646/3775). Declared at file scope exactly as
// CgsIm2dUntex.cpp:102-104, BrnSkidVertex.cpp:103-104 and rwgcoronarenderer.cpp declare them.
struct D3DDevice;
extern "C" void* D3DDevice_BeginVertices(D3DDevice* lpDevice, u32 luPrimitiveType,
                                         u32 luVertexCount, u32 luVertexStreamZeroStride);
extern "C" void  D3DDevice_EndVertices(D3DDevice* lpDevice);

// =================================================================================================
// BrnSunCorona::Construct -- X360 @0x824009B0.
//
// Console order: seed mfXPos/mfYPos, store the allocator, clear mbVisible, set mbRenderSunCorona,
// seed the five tunables, build the vertex descriptor, then the four programs in the order
// occlusion-VERTEX, occlusion-PIXEL (+ "kUvStartAndOffset"), flare-VERTEX, flare-PIXEL
// (+ "kColourAndPower"), asserting each program non-null as it lands.
//
// PC: the four programs are ADOPTED (banner item 2) and the descriptor is built by the immediate-
// mode leaf's Initialize, which carves it from its own arena (its banner explains why the console
// sizer cannot be used on x64), so the allocator is unused. It is still stored, because the console
// stores it and Destruct asserts on it.
// =================================================================================================
void BrnSunCorona::Construct(rw::IResourceAllocator* lpAllocator)
{
    // The scalar seeds, in the console's own store order.
    mfXPos                 = KF_X_POS;                   // stfs 0x48 @0x824009DC
    mfYPos                 = KF_Y_POS;                   // stfs 0x4C @0x824009E0
    mpAllocator            = lpAllocator;                // stw  0x00 @0x824009E4
    mbVisible              = false;                      // stb  0x50 @0x824009E8
    mbRenderSunCorona      = true;                       // stb  0x51 @0x824009EC
    mfSunVectorYMultiplier = KF_SUN_VECTOR_Y_MULTIPLIER; // stfs 0x34 @0x824009F8
    mfOcclusionSize        = KF_OCCLUSION_SIZE;          // stfs 0x38 @0x82400A04
    mfSunFlarePow          = KF_SUN_FLARE_POW;           // stfs 0x3C @0x82400A08
    mfSunBrightness        = KF_SUN_BRIGHTNESS;          // stfs 0x40 @0x82400A10
    mfSunSize              = KF_SUN_SIZE;                // stfs 0x44 @0x82400A14

    // The resource block the console carves for the descriptor. Unused on PC (the leaf's arena
    // owns the object), so it is zeroed rather than left holding whatever the object was built on.
    mVertexDescriptorResource = rw::Resource();

    // --- the vertex declaration (two elements; see the banner table) ------------------------------
    // Written through the GROUND-TRUTH byte lanes. The offset lane (mu16Pad0) is deliberately LEFT
    // at the ctor's 0xFFFF -- >= 0x100 is the auto-pack request that produces 0 / 12.
    renderengine::VertexDescriptor::Parameters lVertexDecl;   // ctor seeds all 16 slots to empty

    lVertexDecl.maElements[0].mu16Stream    = 0;
    lVertexDecl.maElements[0].miOffset      = static_cast<s32>(KU_ELEMENT_FORMAT_FLOAT3);
    lVertexDecl.maElements[0].mu8UsageIndex = KU8_ELEMENT_TYPE_POSITION;

    lVertexDecl.maElements[1].mu16Stream    = 0;
    lVertexDecl.maElements[1].miOffset      = static_cast<s32>(KU_ELEMENT_FORMAT_FLOAT2);
    lVertexDecl.maElements[1].mu8UsageIndex = KU8_ELEMENT_TYPE_TEXCOORD0;

    mpVertexDescriptor = renderengine::VertexDescriptor::Initialize(0, &lVertexDecl);

    // --- the four programs, in the console's order ------------------------------------------------
    mpOcclusionVertexProgram = renderengine::ProgramBufferPC_Adopt(
        renderengine::gauSunCoronaOcclusionVertexProgramPC,
        renderengine::guSunCoronaOcclusionVertexProgramPCSize, 0u);
    CGS_ASSERT(NULL != mpOcclusionVertexProgram, "NULL != mpOcclusionVertexProgram");

    mpOcclusionPixelProgram = renderengine::ProgramBufferPC_Adopt(
        renderengine::gauSunCoronaOcclusionPixelProgramPC,
        renderengine::guSunCoronaOcclusionPixelProgramPCSize, 1u);
    CGS_ASSERT(NULL != mpOcclusionPixelProgram, "NULL != mpOcclusionPixelProgram");

    mOcclusionPixelVariableHandleUvStartOffset = renderengine::ProgramVariableHandle();
    if (mpOcclusionPixelProgram != 0)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            mpOcclusionPixelProgram, gszUvStartAndOffset,
            &mOcclusionPixelVariableHandleUvStartOffset);
    }

    mpFlareVertexProgram = renderengine::ProgramBufferPC_Adopt(
        renderengine::gauSunCoronaFlareVertexProgramPC,
        renderengine::guSunCoronaFlareVertexProgramPCSize, 0u);
    CGS_ASSERT(NULL != mpFlareVertexProgram, "NULL != mpFlareVertexProgram");

    mpFlarePixelProgram = renderengine::ProgramBufferPC_Adopt(
        renderengine::gauSunCoronaFlarePixelProgramPC,
        renderengine::guSunCoronaFlarePixelProgramPCSize, 1u);
    CGS_ASSERT(NULL != mpFlarePixelProgram, "NULL != mpFlarePixelProgram");

    mFlarePixelVariableHandleColourAndPower = renderengine::ProgramVariableHandle();
    if (mpFlarePixelProgram != 0)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            mpFlarePixelProgram, gszColourAndPower,
            &mFlarePixelVariableHandleColourAndPower);
    }

    {
        char lacMessage[256];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[suncorona] programs adopted: occlVs=%d occlPs=%d flareVs=%d flarePs=%d"
                      " decl=%d constants uv{reg=%u,count=%u} colour{reg=%u,count=%u}\n",
                      (int)(mpOcclusionVertexProgram != 0), (int)(mpOcclusionPixelProgram != 0),
                      (int)(mpFlareVertexProgram != 0), (int)(mpFlarePixelProgram != 0),
                      (int)(mpVertexDescriptor != 0),
                      (unsigned)mOcclusionPixelVariableHandleUvStartOffset.mu8RegisterSet,
                      (unsigned)mOcclusionPixelVariableHandleUvStartOffset.mu8RegisterCount,
                      (unsigned)mFlarePixelVariableHandleColourAndPower.mu8RegisterSet,
                      (unsigned)mFlarePixelVariableHandleColourAndPower.mu8RegisterCount);
        SunCoronaLog(lacMessage);
    }

    // [FLAG PC bring-up gate] A resolved-but-not-found handle reads mu8RegisterCount == 0, and
    // RenderEngineDeviceBeginShaderStates then routes the row to the discard row -- the constant
    // never reaches the shader, the occlusion pass samples uv (0,0) forever and the flare draws
    // black, with nothing erroring. Refuse the whole object instead (AGENTS.md rule 9); the
    // console cannot be in this state because its programs are the ones its CTAB came from.
    if (mOcclusionPixelVariableHandleUvStartOffset.mu8RegisterCount == 0u
        || mFlarePixelVariableHandleColourAndPower.mu8RegisterCount == 0u)
    {
        SunCoronaLog("[suncorona] Construct: the authored pixel programs do NOT declare"
                     " kUvStartAndOffset / kColourAndPower by those exact names - the sun-corona"
                     " pass stays off\n");
        mpOcclusionVertexProgram = 0;
        mpOcclusionPixelProgram  = 0;
        mpFlareVertexProgram     = 0;
        mpFlarePixelProgram      = 0;
    }
}

// =================================================================================================
// BrnSunCorona::Destruct -- X360 @0x823F8298.
//     assert(mpAllocator);
//     renderengine::VertexDescriptor::Release(mpVertexDescriptor);
//     (**mpAllocator + 0x14)(mpAllocator, &mVertexDescriptorResource);
// The trailing vtable call is the allocator's Free slot. On PC the descriptor never came out of the
// allocator (the leaf's arena owns it and mVertexDescriptorResource is an empty block), so the
// free is a call that would hand the allocator a block it never issued. It is NOT reproduced, and
// this is the only line of the console body that is dropped; the reason is stated here rather than
// hidden. Nothing on PC calls Destruct today (BrnRendererModule::Destruct is not reconstructed).
// =================================================================================================
void BrnSunCorona::Destruct()
{
    CGS_ASSERT(mpAllocator != NULL, "mpAllocator");

    renderengine::VertexDescriptor::Release(mpVertexDescriptor);
    mpVertexDescriptor = 0;
}

// =================================================================================================
// BrnSunCorona::ComputeSunPositionOnScreen -- X360 @0x823FD798.
//
// THE WHOLE CONSOLE BODY, instruction by instruction (the full annotated decode is
// scratch/coronas_step2/suncorona/work/DECODE.md 2.1):
//     0x823FD79C  lfs f0, 0x34(this)                   mfSunVectorYMultiplier
//     0x823FD7B0  vspltw v10, v2, 1                    lSunDir.y
//     0x823FD800  vmulfp v13, v10, <mult>              lSunDir.y * mfSunVectorYMultiplier
//     0x823FD814  vrlimi128 v12, v13, 4, 0             ...spliced back into the Y LANE ONLY
//     0x823FD818  vmsum3fp + vrsqrtefp + 2x Newton     Normalize(that)
//     0x823FD868  vmaddfp v13, v13, v1, v10            normalized * (-10000) + lViewPosition
//     0x823FD870-88  vspltw/vperm/vsldoi               float4(that.xyz, 1.0)
//     0x823FD89C-A8  4 x mul/madd against r4+0/10/20/30   * lViewProjectionMatrix (ROW-vector)
//     0x823FD8BC-FC  2 x vrefp + 2 Newton, x2          clip.x/clip.w and clip.y/clip.w
//     0x823FD92C  stfs 0x48(this)                      mfXPos = ndc.x *  0.5 + 0.5
//     0x823FD93C  stfs 0x4C(this)                      mfYPos = ndc.y * -0.5 + 0.5
//     0x823FD924/40/48  fcmpu clip.z vs 0.0 / ble / stb 0x50   mbVisible = (clip.z > 0.0f)
//
// The DecFIGS DWARF (BrnSunCorona.cpp:191-199) names every local this body has --
// lWorldSpacePosition / lProjectedPosition / lSunPos / lLocalSunDir / lfX / lfY / lfZNonProjected
// -- and lists exactly the operation set above (operator*<VectorAxisY>, Normalize, Vector3::SetY,
// Vector4::Set, operator+, operator*, operator/<VectorAxisX,VectorAxisW>,
// operator/<VectorAxisY,VectorAxisW>, VecFloatRef<VectorAxisZ>::operator float). Names below are
// the DWARF's.
//
// PC deviations: NONE of substance. The console's rsqrt/recip are VMX estimate+Newton sequences;
// here they are the exact IEEE divide/sqrt the host has, which is the same value to better
// precision -- the same treatment every other vpu-math reconstruction in this tree gets. The
// row-vector convention is the console's own (the four rows are combined as
// row0*x + row1*y + row2*z + row3*w), and it is the same convention gBrnSkyCameraBringUp and the
// corona pass already use.
// =================================================================================================
void BrnSunCorona::ComputeSunPositionOnScreen(const Matrix44& lrViewProjectionMatrix,
                                              Vector3 lViewPosition,
                                              Vector3 lSunDir)
{
    // lLocalSunDir = lSunDir with only its Y lane scaled (the vrlimi128 mask is 4 == the y lane).
    Vector3 lLocalSunDir = lSunDir;
    lLocalSunDir.y = lSunDir.y * mfSunVectorYMultiplier;

    // Normalize(lLocalSunDir) -- a 3-lane normalise (vmsum3fp is dot3).
    const f32 lfMagnitudeSquared = lLocalSunDir.x * lLocalSunDir.x
                                 + lLocalSunDir.y * lLocalSunDir.y
                                 + lLocalSunDir.z * lLocalSunDir.z;
    if (!(lfMagnitudeSquared > 0.0f))
    {
        // Not a console branch: the console's rsqrt estimate of 0 is +inf and the projected point
        // is a NaN, which would place the flare at an undefined screen position and never say so.
        // A zero sun direction means the shader-constants frame has not been published yet, which
        // is a PC bring-up state (see the caller's gate) -- refuse the frame rather than storing
        // NaN into mfXPos/mfYPos (AGENTS.md rule 9).
        mbVisible = false;
        return;
    }
    const f32 lfInverseMagnitude = 1.0f / std::sqrt(lfMagnitudeSquared);
    lLocalSunDir.x *= lfInverseMagnitude;
    lLocalSunDir.y *= lfInverseMagnitude;
    lLocalSunDir.z *= lfInverseMagnitude;

    // lSunPos = lViewPosition + lLocalSunDir * (-10000) -- the sun placed a long way along the
    // direction the key light comes FROM.
    Vector3 lSunPos;
    lSunPos.x = lLocalSunDir.x * KF_SUN_DISTANCE + lViewPosition.x;
    lSunPos.y = lLocalSunDir.y * KF_SUN_DISTANCE + lViewPosition.y;
    lSunPos.z = lLocalSunDir.z * KF_SUN_DISTANCE + lViewPosition.z;
    lSunPos.w = 0.0f;

    // lWorldSpacePosition.Set(lSunPos.xyz, 1.0f) -- the vperm/vsldoi triple.
    Vector4 lWorldSpacePosition;
    lWorldSpacePosition.x = lSunPos.x;
    lWorldSpacePosition.y = lSunPos.y;
    lWorldSpacePosition.z = lSunPos.z;
    lWorldSpacePosition.w = 1.0f;

    // lProjectedPosition = lWorldSpacePosition * lViewProjectionMatrix (ROW-vector).
    Vector4 lProjectedPosition;
    lProjectedPosition.x = lrViewProjectionMatrix.xAxis.x * lWorldSpacePosition.x
                         + lrViewProjectionMatrix.yAxis.x * lWorldSpacePosition.y
                         + lrViewProjectionMatrix.zAxis.x * lWorldSpacePosition.z
                         + lrViewProjectionMatrix.wAxis.x * lWorldSpacePosition.w;
    lProjectedPosition.y = lrViewProjectionMatrix.xAxis.y * lWorldSpacePosition.x
                         + lrViewProjectionMatrix.yAxis.y * lWorldSpacePosition.y
                         + lrViewProjectionMatrix.zAxis.y * lWorldSpacePosition.z
                         + lrViewProjectionMatrix.wAxis.y * lWorldSpacePosition.w;
    lProjectedPosition.z = lrViewProjectionMatrix.xAxis.z * lWorldSpacePosition.x
                         + lrViewProjectionMatrix.yAxis.z * lWorldSpacePosition.y
                         + lrViewProjectionMatrix.zAxis.z * lWorldSpacePosition.z
                         + lrViewProjectionMatrix.wAxis.z * lWorldSpacePosition.w;
    lProjectedPosition.w = lrViewProjectionMatrix.xAxis.w * lWorldSpacePosition.x
                         + lrViewProjectionMatrix.yAxis.w * lWorldSpacePosition.y
                         + lrViewProjectionMatrix.zAxis.w * lWorldSpacePosition.z
                         + lrViewProjectionMatrix.wAxis.w * lWorldSpacePosition.w;

    if (!(lProjectedPosition.w != 0.0f))
    {
        // Same class of refusal as the zero direction above: a degenerate (all-zero) view
        // projection gives w == 0 and both divides below are NaN.
        mbVisible = false;
        return;
    }

    const f32 lfX            = lProjectedPosition.x / lProjectedPosition.w;
    const f32 lfY            = lProjectedPosition.y / lProjectedPosition.w;
    const f32 lfZNonProjected = lProjectedPosition.z;

    // NDC [-1,1] -> screen [0,1], with Y FLIPPED (the console's -0.5 scale).
    mfXPos = lfX * KF_NDC_TO_SCREEN_SCALE_X + KF_NDC_TO_SCREEN_BIAS;
    mfYPos = lfY * KF_NDC_TO_SCREEN_SCALE_Y + KF_NDC_TO_SCREEN_BIAS;

    // The sun is only drawn when it is in FRONT of the camera. `ble -> 0` means the store is 1 only
    // on a strict greater-than, so the polarity is `> 0.0f`, not `>=`.
    mbVisible = (lfZNonProjected > 0.0f);
}

// =================================================================================================
// BrnSunCorona::GenerateOcclusionBuffer -- X360 @0x82400CF8.
//
// THE CONSOLE BODY, in order:
//     assert(lpOcclusionRt != NULL)                                  BrnSunCorona.cpp:250
//     assert(lpSourceDepthRt != NULL)                                BrnSunCorona.cpp:251
//     if (!mbRenderSunCorona) return;                                lbz 0x51(this) @0x82400D68
//     lpDepthRenderTarget = lpSourceDepthRt->GetRenderTarget();      lwz 0x108 @0x82400D74
//     assert(lpDepthRenderTarget)                                    BrnSunCorona.cpp:260
//     lpOcclusionRt->Begin();
//     shadow::Device::SetState(saDepthStencilStates[1]);             (see the state banner)
//     shadow::Device::SetState(saRasterizerStates[2]);
//     shadow::Device::SetState(saBlendStates[0]);
//     shadow::Device::SetVertexProgram(mpOcclusionVertexProgram);    (INLINED on the console --
//                                                                    the shadow compare + the
//                                                                    SetVertexProgramInternal call)
//     shadow::Device::SetPixelProgram(mpOcclusionPixelProgram);
//     lUVStartAndOffset.Set(mfXPos, mfYPos,
//                           (1/srcWidth)*mfOcclusionSize, (1/srcHeight)*mfOcclusionSize);
//     BeginShaderStates(&mOcclusionPixelVariableHandleUvStartOffset, ...); Write(that);
//     shadow::Device::SetState(lpDepthRenderTarget->GetDepthTextureState(), 0);   lwz 0x8C
//     shadow::Device::SetVertexDescriptor(mpVertexDescriptor);       (INLINED)
//     shadow::Device::FlushVertexProgramState();
//     D3DDevice_BeginVertices(dev, 6, 4, 20);  four NDC corners, uv (0,0) in all four
//     D3DDevice_EndVertices(dev);
//     lpOcclusionRt->End();
//
// The four corners the asm writes, read off the stvewx stores at 0x82400F0C-0x82401084 (each
// vertex is 20 bytes and the first cursor is `BeginVertices() - 4` + the +4 stvewx displacement,
// i.e. the run starts at the returned pointer):
//     v0 (-1,-1,0) uv(0,0)   v1 (1,-1,0) uv(0,0)   v2 (-1,1,0) uv(0,0)   v3 (1,1,0) uv(0,0)
// -- a full-screen NDC quad in TRIANGLESTRIP order. THE UVs ARE ALL ZERO AND UNUSED: the occlusion
// vertex program never fetches TEXCOORD0 and the pixel program builds its taps out of
// kUvStartAndOffset. Reproduced verbatim (the element is in the declaration, so the stride must
// carry it).
//
// PC DEVIATIONS: none in this body. The two "inlined setter" blocks are DE-INLINED onto the
// shadow::Device names this tree already owns (AGENTS.md "inlining reversal", and rule 3 -- those
// shadow words are shadow::Device PRIVATE statics), exactly as CoronaRenderer::Begin does.
// =================================================================================================
void BrnSunCorona::GenerateOcclusionBuffer(CgsRenderTarget* lpOcclusionRt,
                                           CgsRenderTarget* lpSourceDepthRt)
{
    CGS_ASSERT(lpOcclusionRt != NULL, "lpOcclusionRt != NULL");
    CGS_ASSERT(lpSourceDepthRt != NULL, "lpSourceDepthRt != NULL");
    if (lpOcclusionRt == 0 || lpSourceDepthRt == 0)
        return;

    if (!mbRenderSunCorona)
        return;

    rw::graphics::postfx::RenderTarget* const lpDepthRenderTarget =
        lpSourceDepthRt->GetRenderTarget();
    CGS_ASSERT(lpDepthRenderTarget != NULL, "lpDepthRenderTarget");
    if (lpDepthRenderTarget == 0)
        return;

    // [FLAG PC bring-up gate] The tap step is mfOcclusionSize/extent; a zero extent is a division
    // by zero and an infinite uv, and it is reachable on PC in a way it is not on the console (the
    // pool is built lazily and a target can exist before it is sized -- the "[postfx-rt] target
    // 0x0" failure PCBringUpCreatePostFxSceneTargets' own banner records). Refuse rather than
    // sample at infinity.
    const u32 luSourceWidth  = lpSourceDepthRt->GetWidth();
    const u32 luSourceHeight = lpSourceDepthRt->GetHeight();
    if (luSourceWidth == 0u || luSourceHeight == 0u)
    {
        static bool sbLoggedDegenerateSource = false;
        if (!sbLoggedDegenerateSource)
        {
            sbLoggedDegenerateSource = true;
            SunCoronaLog("[suncorona] occlusion pass REFUSED: the depth source has a 0 extent\n");
        }
        return;
    }

    // The depth TextureState the taps sample. On the console this is the word at RenderTarget+0x8C,
    // which rwgpfxrendertarget.h:319 documents IS `mDepthTarget.mpTextureState` -- reached here by
    // name through the accessor that exists for exactly this reason (so a caller cannot bind scene
    // colour where scene depth belongs).
    renderengine::TextureState* const lpDepthStencilTexture =
        lpDepthRenderTarget->GetDepthTextureState();
    if (lpDepthStencilTexture == 0)
    {
        static bool sbLoggedNoDepthState = false;
        if (!sbLoggedNoDepthState)
        {
            sbLoggedNoDepthState = true;
            SunCoronaLog("[suncorona] occlusion pass REFUSED: the depth source has no sampleable"
                         " depth TextureState (SetUseDepthStencilAsTexture was not set)\n");
        }
        return;
    }

    SunCoronaPassStates lStates;
    if (!AcquirePassStates(E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA, &lStates))
        return;

    lpOcclusionRt->Begin();

    shadow::Device::SetState(lStates.mpDepthStencil);
    shadow::Device::SetState(lStates.mpRasterizer);
    shadow::Device::SetState(lStates.mpBlend);

    shadow::Device::SetVertexProgram(mpOcclusionVertexProgram);
    shadow::Device::SetPixelProgram(mpOcclusionPixelProgram);

    // kUvStartAndOffset = { start u, start v, one tap step in u, one tap step in v }.
    const f32 lfStartU       = mfXPos;
    const f32 lfStartV       = mfYPos;
    const f32 lFullInvWidth  = (1.0f / static_cast<f32>(luSourceWidth))  * mfOcclusionSize;
    const f32 lFullInvHeight = (1.0f / static_cast<f32>(luSourceHeight)) * mfOcclusionSize;

    {
        void* lpRow = 0;
        RenderEngineDeviceBeginShaderStates(&mOcclusionPixelVariableHandleUvStartOffset, &lpRow);
        if (lpRow != 0)
        {
            const f32 lafUvStartAndOffset[4] =
                { lfStartU, lfStartV, lFullInvWidth, lFullInvHeight };
            std::memcpy(lpRow, lafUvStartAndOffset, sizeof(lafUvStartAndOffset));
        }
    }

    shadow::Device::SetState(lpDepthStencilTexture, 0u);
    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

    SunCoronaVertex* lpVertex = static_cast<SunCoronaVertex*>(
        D3DDevice_BeginVertices(0, KU_XENOS_PRIMITIVE_TRIANGLESTRIP,
                                KU_QUAD_VERTEX_COUNT, sizeof(SunCoronaVertex)));
    if (lpVertex != 0)
    {
        // The console's own four corners and their (unused, zero) uvs.
        static const f32 kaafOcclusionQuad[KU_QUAD_VERTEX_COUNT][3] =
        {
            { -1.0f, -1.0f, 0.0f },
            {  1.0f, -1.0f, 0.0f },
            { -1.0f,  1.0f, 0.0f },
            {  1.0f,  1.0f, 0.0f },
        };
        for (u32 luCorner = 0; luCorner < KU_QUAD_VERTEX_COUNT; ++luCorner)
        {
            lpVertex[luCorner].mafPosition[0] = kaafOcclusionQuad[luCorner][0];
            lpVertex[luCorner].mafPosition[1] = kaafOcclusionQuad[luCorner][1];
            lpVertex[luCorner].mafPosition[2] = kaafOcclusionQuad[luCorner][2];
            lpVertex[luCorner].mafUv[0]       = 0.0f;
            lpVertex[luCorner].mafUv[1]       = 0.0f;
        }
    }
    D3DDevice_EndVertices(0);

    lpOcclusionRt->End();

    {
        // [FLAG PC bring-up diagnostic] one line, the first time the measurement runs. It prints
        // the EXTENTS on both sides and the tap step, because the two silent failures here are a
        // 0-extent source (refused above) and a tap step so small that all 49 taps land in one
        // texel -- neither of which changes a pixel on screen. DELETE with the bring-up.
        static bool sbLoggedFirstOcclusion = false;
        if (!sbLoggedFirstOcclusion)
        {
            sbLoggedFirstOcclusion = true;
            char lacMessage[256];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[suncorona] first occlusion pass: buffer %ux%u, depth source %ux%u,"
                          " screen=(%.4f %.4f) tapStep=(%.6f %.6f) visible=%d\n",
                          (unsigned)lpOcclusionRt->GetWidth(), (unsigned)lpOcclusionRt->GetHeight(),
                          (unsigned)luSourceWidth, (unsigned)luSourceHeight,
                          lfStartU, lfStartV, lFullInvWidth, lFullInvHeight, (int)mbVisible);
            SunCoronaLog(lacMessage);
        }
    }
}

// =================================================================================================
// BrnSunCorona::RenderOccludedFlare -- X360 @0x824010A8.
//
// THE CONSOLE BODY, in order:
//     assert(lpOcclusionRt != NULL)                                  BrnSunCorona.cpp:369
//     if (!mbVisible || !mbRenderSunCorona) return;                  lbz 0x50 / 0x51 @0x82401104/10
//     lpOcclusionRenderTarget = lpOcclusionRt->GetRenderTarget();    lwz 0x108 @0x8240111C
//     assert(lpOcclusionRenderTarget)                                BrnSunCorona.cpp:378
//     shadow::Device::SetState(saDepthStencilStates[1]);
//     shadow::Device::SetState(saRasterizerStates[2]);
//     shadow::Device::SetState(saBlendStates[5]);                    <- ADDITIVE RGB, dest RGB
//     shadow::Device::SetVertexProgram(mpFlareVertexProgram);
//     shadow::Device::SetPixelProgram(mpFlarePixelProgram);
//     lfBrightness = lbDebugOverrideBrightness ? lfDebugOverrideBrightness : mfSunBrightness;
//     lColourAndPower.Set(lSunColour.xyz * lfWhiteLevel * lfBrightness, mfSunFlarePow);
//     BeginShaderStates(&mFlarePixelVariableHandleColourAndPower, ...); Write(that);
//     shadow::Device::SetState(lpOcclusionRenderTarget->maColourTargets[0].mpTextureState, 0);
//     shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
//     shadow::Device::FlushVertexProgramState();
//     D3DDevice_BeginVertices(dev, 6, 4, 20);  the flare quad
//     D3DDevice_EndVertices(dev);
//     lpOcclusionRt->End();
//
// THE QUAD, off the asm at 0x82401304-0x82401364 (`x*2 - 1` is fmsubs against 2.0 and 1.0):
//     left   = (mfXPos - mfSunSize) * 2 - 1
//     right  = (mfXPos + mfSunSize) * 2 - 1
//     top    = ((1 - mfYPos) + mfSunSize * 1.7777778) * 2 - 1
//     bottom = ((1 - mfYPos) - mfSunSize * 1.7777778) * 2 - 1
// and the four vertices, TRIANGLESTRIP order, uv on the [-1,1] square:
//     (left,top,0) uv(-1, 1)   (right,top,0) uv(1, 1)
//     (left,bottom,0) uv(-1,-1)   (right,bottom,0) uv(1,-1)
// `1 - mfYPos` undoes the Y flip ComputeSunPositionOnScreen applied, so the quad is centred on the
// sun again; the 1.7777778 is the console's HARD-CODED 16:9 (see the KF_FLARE_ASPECT note).
//
// ⚠ `lpOcclusionRt->End()` WITH NO MATCHING Begin IS THE CONSOLE'S OWN SHAPE, not a transcription
// slip: this function draws into whatever render target the caller left bound (on the console,
// the quarter-res particle buffer BrnRendererModule::BeginQuarterResBuffer @0x82408C38 opened),
// and the only CgsRenderTarget call it makes is that trailing End on the OCCLUSION target. On this
// backend CgsRenderTarget::End is RenderTarget::End(true) -> Resolve, and Resolve is a DOCUMENTED
// NO-OP (PostFxRenderTargetPCLeaf.cpp: on PC the rendered surface IS the sampled texture, there is
// nothing to copy), so reproducing it costs nothing and drops nothing.
// =================================================================================================
void BrnSunCorona::RenderOccludedFlare(CgsRenderTarget* lpOcclusionRt,
                                       Vector3 lSunColour,
                                       f32 lfWhiteLevel,
                                       f32 lfDebugOverrideBrightness,
                                       bool lbDebugOverrideBrightness)
{
    CGS_ASSERT(lpOcclusionRt != NULL, "lpOcclusionRt != NULL");
    if (lpOcclusionRt == 0)
        return;

    if (!mbVisible || !mbRenderSunCorona)
        return;

    rw::graphics::postfx::RenderTarget* const lpOcclusionRenderTarget =
        lpOcclusionRt->GetRenderTarget();
    CGS_ASSERT(lpOcclusionRenderTarget != NULL, "lpOcclusionRenderTarget");
    if (lpOcclusionRenderTarget == 0)
        return;

    // The occlusion buffer's COLOUR TextureState (RenderTarget +0x2C on the console image ==
    // maColourTargets[0].mpTextureState -- maColourTargets sits at +0x20 and a Target record is
    // 0x18 bytes). Reached by NAME.
    renderengine::TextureState* const lpColourTexture =
        lpOcclusionRenderTarget->maColourTargets[0].mpTextureState;
    if (lpColourTexture == 0)
    {
        static bool sbLoggedNoColourState = false;
        if (!sbLoggedNoColourState)
        {
            sbLoggedNoColourState = true;
            SunCoronaLog("[suncorona] flare pass REFUSED: the occlusion buffer has no colour"
                         " TextureState to sample\n");
        }
        return;
    }

    SunCoronaPassStates lStates;
    if (!AcquirePassStates(E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_RGB_NO_ALPHA_TEST_DEST_RGB,
                           &lStates))
        return;

    shadow::Device::SetState(lStates.mpDepthStencil);
    shadow::Device::SetState(lStates.mpRasterizer);
    shadow::Device::SetState(lStates.mpBlend);

    shadow::Device::SetVertexProgram(mpFlareVertexProgram);
    shadow::Device::SetPixelProgram(mpFlarePixelProgram);

    const f32 lfBrightness = lbDebugOverrideBrightness ? lfDebugOverrideBrightness
                                                       : mfSunBrightness;

    const f32 lafColourAndPower[4] =
    {
        lSunColour.x * lfWhiteLevel * lfBrightness,
        lSunColour.y * lfWhiteLevel * lfBrightness,
        lSunColour.z * lfWhiteLevel * lfBrightness,
        mfSunFlarePow,
    };

    {
        void* lpRow = 0;
        RenderEngineDeviceBeginShaderStates(&mFlarePixelVariableHandleColourAndPower, &lpRow);
        if (lpRow != 0)
            std::memcpy(lpRow, lafColourAndPower, sizeof(lafColourAndPower));
    }

    shadow::Device::SetState(lpColourTexture, 0u);
    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

    const f32 lfLeft   = (mfXPos - mfSunSize) * KF_SCREEN_TO_NDC_SCALE - KF_SCREEN_TO_NDC_BIAS;
    const f32 lfRight  = (mfXPos + mfSunSize) * KF_SCREEN_TO_NDC_SCALE - KF_SCREEN_TO_NDC_BIAS;
    const f32 lfHalfHeight = mfSunSize * KF_FLARE_ASPECT;
    const f32 lfTop    = ((1.0f - mfYPos) + lfHalfHeight) * KF_SCREEN_TO_NDC_SCALE
                       - KF_SCREEN_TO_NDC_BIAS;
    const f32 lfBottom = ((1.0f - mfYPos) - lfHalfHeight) * KF_SCREEN_TO_NDC_SCALE
                       - KF_SCREEN_TO_NDC_BIAS;

    SunCoronaVertex* lpVertex = static_cast<SunCoronaVertex*>(
        D3DDevice_BeginVertices(0, KU_XENOS_PRIMITIVE_TRIANGLESTRIP,
                                KU_QUAD_VERTEX_COUNT, sizeof(SunCoronaVertex)));
    if (lpVertex != 0)
    {
        const f32 laafFlareQuad[KU_QUAD_VERTEX_COUNT][5] =
        {
            { lfLeft,  lfTop,    0.0f, -1.0f,  1.0f },
            { lfRight, lfTop,    0.0f,  1.0f,  1.0f },
            { lfLeft,  lfBottom, 0.0f, -1.0f, -1.0f },
            { lfRight, lfBottom, 0.0f,  1.0f, -1.0f },
        };
        for (u32 luCorner = 0; luCorner < KU_QUAD_VERTEX_COUNT; ++luCorner)
        {
            lpVertex[luCorner].mafPosition[0] = laafFlareQuad[luCorner][0];
            lpVertex[luCorner].mafPosition[1] = laafFlareQuad[luCorner][1];
            lpVertex[luCorner].mafPosition[2] = laafFlareQuad[luCorner][2];
            lpVertex[luCorner].mafUv[0]       = laafFlareQuad[luCorner][3];
            lpVertex[luCorner].mafUv[1]       = laafFlareQuad[luCorner][4];
        }
    }
    D3DDevice_EndVertices(0);

    lpOcclusionRt->End();

    {
        // [FLAG PC bring-up diagnostic] one line, the first flare drawn, PLUS the one number that
        // says whether the pass works: the occlusion FRACTION, read back ONE-SHOT out of the 1x1
        // GPU texture (renderengine::Texture::PCReadBackTexel0 -- a GetRenderTargetData stall,
        // latched so it happens once per boot; verify_suncorona F1). visibility=1.000 with the
        // sun in the open sky and 0.000 with it behind a building is the acceptance; a
        // permanent 0.000 means the depth taps never see the far plane (see the .fx's raw-depth
        // warning). What else is printed is everything the CPU decides -- the screen position,
        // the NDC extent of the quad, and the exact colour the blend will scale by that fraction.
        // DELETE with the bring-up.
        static bool sbLoggedFirstFlare = false;
        if (!sbLoggedFirstFlare)
        {
            sbLoggedFirstFlare = true;
            u32 luTexel = 0u; s32 liFormat = 0; f32 lfVisibility = -1.0f;
            const bool lbRead = renderengine::Texture::PCReadBackTexel0(lpOcclusionRt->GetTexture(0), &luTexel, &liFormat);
            if (lbRead)
            {
                // The occlusion PS writes the fraction to every channel; decode the first byte of
                // an 8-bit format (A8R8G8B8 = 21 / X8R8G8B8 = 22: little-endian, byte 0 = blue)
                // or the low half of a 16-bit-float R16F (111) / G16R16F (112) format.
                if (liFormat == 111 || liFormat == 112)
                {
                    const u32 luHalf = luTexel & 0xFFFFu;
                    const u32 luSign = (luHalf >> 15) & 1u, luExp = (luHalf >> 10) & 0x1Fu, luMan = luHalf & 0x3FFu;
                    f32 lfValue = 0.0f;
                    if (luExp == 0u)       lfValue = (luMan / 1024.0f) * (1.0f / 16384.0f);
                    else if (luExp < 31u)  lfValue = (1.0f + luMan / 1024.0f) * static_cast<f32>(std::pow(2.0f, static_cast<f32>(luExp) - 15.0f));
                    lfVisibility = luSign ? -lfValue : lfValue;
                }
                else
                {
                    lfVisibility = static_cast<f32>(luTexel & 0xFFu) / 255.0f;
                }
            }
            char lacMessage[400];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[suncorona] first flare: screen=(%.4f %.4f) ndc=(%.3f %.3f)-(%.3f %.3f)"
                          " colour=(%.3f %.3f %.3f) power=%.2f whiteLevel=%.3f brightness=%.3f"
                          " visibility=%.4f (readback %s, texel=0x%08X fmt=%d)\n",
                          mfXPos, mfYPos, lfLeft, lfBottom, lfRight, lfTop,
                          lafColourAndPower[0], lafColourAndPower[1], lafColourAndPower[2],
                          mfSunFlarePow, lfWhiteLevel, lfBrightness,
                          lfVisibility, lbRead ? "OK" : "FAILED", (unsigned)luTexel, (int)liFormat);
            SunCoronaLog(lacMessage);
        }
    }
}
