#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h"

#include <cstdio>    // snprintf (the three one-shot bring-up lines)
#include <cstring>   // memcpy (the constant rows -- the row pointer carries no alignment guarantee)
#include <cstddef>   // offsetof (the CoronaVertex layout pins)

#include "GameSource/Graphics/BrnCoronaManager.h"                          // renderengine::Corona / CoronaBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"      // shadow::Device
#include "pc/gcm/renderengine/renderstates.h"                              // TextureState / DepthStencilState
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"   // BlendMaterialState

// =================================================================================================
// renderengine::CoronaRenderer -- the corona pass, reconstructed from BURNOUT_X360_ARTIST.XEX and
// realised on the D3D9 backend.
//
//   Initialize                @0x822850F8
//   Begin<shadow::Device>     @0x823FF2C0
//   Dispatch<shadow::Device>  @0x82404F30
//   End<shadow::Device>       INLINED -- BrnCoronaManager::Render's tail, `stb 0, byte_82FAB695`
//                             @0x824076B4-BC (the byte Begin raises at 0x823FF2D4)
//   SetBlendState / SetDepthStencilStates  INLINED into BrnCoronaManager::Construct @0x823FCDD8-F8
//   SetTextureAtlas                        INLINED into BrnCoronaManager::SetTextureAtlas @0x823FD108-2C
//
// WHAT THIS REWRITE CHANGES vs the previous committed body, and why each change is a defect fix and
// not a preference:
//
//  1. THE SIX PHANTOM EXTERNALS ARE GONE. The file used to declare LOCAL `renderengine::ProgramBuffer`
//     / `VertexDescriptor` slices whose mangled names matched no committed home, so mounting it was
//     six guaranteed LNK2019 (carlights step 1 report section 4.3; the 1a verifier's F3 gives the
//     real homes). Both local slices are deleted and the real headers are included:
//         states/programbuffer.h:107/109/111        (states/programbuffer.cpp, build_game_exe.bat:629)
//         pc/gcm/renderengine/VertexDescriptor.h:76/83/85
//           Parameters::Parameters()                -> VertexDescriptorParameters.cpp:23   bat:314
//           GetResourceDescriptor(void*, const P*)  -> CgsRwVertexDescResourceType.cpp:91  bat:315
//           Initialize(rw::Resource*, const P*)     -> ImmediateModePCLeaf.cpp:926         bat:316
//     ...and the console GetResourceDescriptor/Initialize route for the two PROGRAMS is not used at
//     all on PC (see 2), so those two externals are not even referenced any more.
//
//  2. THE MICROCODE WALL IS CROSSED THE WAY THE SKY DOME CROSSED IT. The console's two corona
//     programs are executable-embedded Xenos microcode (&unk_8200F1B8, 228 B pixel; &unk_8200F2A0,
//     788 B vertex) and cannot run on D3D9 (AGENTS.md rule 2). PC adopts an authored pair through
//     renderengine::ProgramBufferPC_Adopt (programbuffer.h:123, body ImmediateModePCLeaf.cpp:833),
//     exactly as CgsIm3dSkyDome.cpp:159/162 does for the sky's pair. The console
//     ProgramBuffer::GetResourceDescriptor/Initialize route is NOT kept as a fallback here: on this
//     backend both bodies call XGGetMicrocodeShaderParts, whose PC stub returns 0 WITHOUT writing
//     *lpParts, and then read that uninitialised block -- a crash, not a gap (CgsIm3dSkyDome.cpp's
//     own banner). If the authored images are absent the pass reports itself and refuses to draw.
//
//  3. THE VERTEX DESCRIPTOR IS WRITTEN THROUGH THE GROUND-TRUTH BYTE LANES. VertexDescriptor.h:44-58
//     documents that the committed Parameters::Element member NAMES are shifted one lane from what
//     they hold. The previous body wrote its four elements through a LOCAL struct whose `muFormat`
//     happened to land on the format lane but whose `mu8Enabled` was really the ELEMENT TYPE lane --
//     right bytes, wrong names, and the offset lane was left at a value the local ctor never wrote.
//     Written by lane here, against the real Parameters (whose ctor seeds offset = 0xFFFF, i.e.
//     AUTO-PACK, which is what produces the console's packed 48-byte vertex).
//
//  4. muFunction NO LONGER TRUNCATES A HOST POINTER (AGENTS.md rule 1). It is not written at all --
//     the adopt route does not use ProgramBufferParameters.
//
// ---- THE CORONA VERTEX FORMAT (fully pinned, no dump needed) -----------------------------------
// Initialize's four Parameters elements, read off its own pseudocode (v30..v41 at stack +0x110,
// +0x120, +0x130, +0x140 -- a 16-byte stride, so one Element each; v3x = the +0x00 stream u16,
// v3x+1 = the +0x04 format u32, the trailing char = the +0x0B element type):
//
//   el | stream | format word | MapVertexFormat  | width | elementType | default usage
//   ---+--------+-------------+------------------+-------+-------------+-----------------
//    0 |   0    | 0x1A23A6    | D3DDECLTYPE_FLOAT4  |  16 |     1       | POSITION 0
//    1 |   0    | 0x2A23B9    | D3DDECLTYPE_FLOAT3  |  12 |     3       | NORMAL   0
//    2 |   0    | 0x1A23A6    | D3DDECLTYPE_FLOAT4  |  16 |     6       | TEXCOORD 0
//    3 |   0    | 0x14C86     | (UBYTE4N -- see 5) |   4 |     4       | COLOR    0
//
// with the offset lane left at the ctor's auto-pack sentinel, so ImmediateModePCLeaf's
// VertexDescriptor::Initialize packs them at 0 / 16 / 28 / 44 -- a 48-BYTE VERTEX, which is exactly
// the stride the console passes to D3DDevice_BeginVertices (`li r?, 48` in Dispatch @0x82404F30) and
// exactly the DWARF's VertexIterator4<VertexTypeFloat4, VertexTypeFloat3, VertexTypeFloat4,
// VertexTypePS3Color>. Three independent witnesses, one format.
//
//  5. FORMAT WORD 0x14C86 IS NOT IN ImmediateModePCLeaf's MapVertexFormat TABLE. Without it the
//     COLOUR element is dropped ("unknown vertex format code - element dropped") and every corona
//     draws with an undefined COLOR0 -- i.e. no distance fade and no per-light tint, with nothing
//     erroring. The mapping is added in a companion edit to ImmediateModePCLeaf.cpp; five committed
//     TUs already annotate that word as the immediate-mode UBYTE4N colour element
//     (CgsIm2dUntex.cpp:69, CgsIm2dColTex.cpp:77/80, CgsIm3d.cpp:77/80, CgsIm3dUntex.cpp:72/74,
//     BrnLionBlendIm3d.cpp:71), so the identification is the tree's own, not this file's.
//     ⚠ RISK, stated rather than buried: if the console word is really a D3DCOLOR (ARGB-ordered)
//     rather than a UBYTE4N (memory-ordered), red and blue swap. The one-shot `[corona] first draw`
//     line below prints the first corona's colour word so a night screenshot can settle it.
//
// ---- THE PASS STATE (asm-proven, carlights step 1 finding 1.7, re-derived this wave) -----------
// BrnCoronaManager::Construct @0x823FCDD8-F8 captures three device-state-factory globals:
//     s_blendState         <- dword_83010F78 == CgsBlendStateFactory saBlendStates[2]
//                             == E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA
//     s_enabledZTestState  <- dword_83010914 == saDepthStencilStates[2] == ZON_ZLEQ_ZWRITEOFF
//     s_disabledZTestState <- dword_83010910 == saDepthStencilStates[1] == ZOFF_ZALL_ZWRITEOFF
// and BrnCoronaManager::Render always passes BatchParameters.m_flags = 1 == FLAG_OCCLUSION_ZTEST
// (`li r10,1` / `stw r10, var_8C` @0x8240769C/@0x824076AC), so the LIVE corona pass is
// ADDITIVE, NO ALPHA TEST, DEPTH-TESTED LEQ, NO DEPTH WRITE.
// =================================================================================================

namespace
{
    // ---------------------------------------------------------------------------------------------
    // SEAM S1 (group `coronaprogs`) -- THE AUTHORED PC PROGRAM PAIR.
    //
    // Declared here as file-scope externs rather than through a header, which is the SKY DOME'S
    // EXACT SHAPE: BrnIm3d.cpp:53-54 declares `extern const u8 gauSkyDomeVertexProgramPC[]; extern
    // const u32 guSkyDomeVertexProgramPCSize;` at file scope and calls ProgramBufferPC_Adopt itself.
    // The names below follow that convention verbatim with SkyDome -> Corona.
    //
    // If `coronaprogs` lands a different surface (WAVE_NOTE offers
    // `renderengine::CoronaProgramsPC_Adopt(ProgramBufferData**, ProgramBufferData**)` as an
    // alternative), the conductor reconciles THESE FOUR NAMES -- nothing else in this file moves.
    // See the report's CROSS-GROUP section.
    // ---------------------------------------------------------------------------------------------

    // The three named vertex-program constants CoronaRenderer::Initialize resolves. The strings are
    // the console's own literals, at the call sites @0x82285250 / 0x82285268 / 0x82285280.
    const u8 gszViewProjectionMatrix[]         = "viewProjectionMatrix";
    const u8 gszCameraPositionPlusBrightness[] = "cameraPositionPlusBrightness";
    const u8 gszViewXyScale[]                  = "viewXyScale";

    // The Xenos vertex-format words Initialize stores (see the banner table).
    const u32 KU_ELEMENT_FORMAT_FLOAT4  = 0x1A23A6u;
    const u32 KU_ELEMENT_FORMAT_FLOAT3  = 0x2A23B9u;
    const u32 KU_ELEMENT_FORMAT_COLOUR  = 0x14C86u;

    // The element-TYPE lane values (+0x0B), which VertexDescriptor::Initialize turns into the D3D9
    // usage through gauVertexFormatDefaults: 1 = XYZ -> POSITION0, 3 = NORMAL0, 6 = TEX0 ->
    // TEXCOORD0, 4 = COLOR0.
    const u8 KU8_ELEMENT_TYPE_POSITION  = 1u;
    const u8 KU8_ELEMENT_TYPE_NORMAL    = 3u;
    const u8 KU8_ELEMENT_TYPE_TEXCOORD0 = 6u;
    const u8 KU8_ELEMENT_TYPE_COLOUR0   = 4u;

    // The corona vertex the console packs, 48 bytes, in the order the four VertexDescriptor
    // elements auto-pack (see the banner). This mirrors the console's
    // VertexIterator4<Float4, Float3, Float4, PS3Color>::Write argument order exactly:
    // Write(cursor, position /*Vector4*/, record+0x10 /*Vector3 direction*/, corner /*Vector4*/,
    //       record+0x34 /*RGBA8*/).
    struct CoronaVertex
    {
        f32 mafPositionAndDistance[4];  // +0x00  POSITION0  xyz = world position, w = the +0x30 bias
        f32 mafDirection[3];            // +0x10  NORMAL0    the corona's facing direction
        f32 mafUvAndOffset[4];          // +0x1C  TEXCOORD0  xy = atlas uv, zw = the quad corner offset
        u32 muColour;                   // +0x2C  COLOR0     RGBA8
    };
    // The layout pins (the campaign's _AssertLayout discipline): the vertex MUST byte-match what
    // VertexDescriptor::Initialize auto-packs for the four elements (0 / 16 / 28 / 44, stride 48).
    static_assert(sizeof(CoronaVertex) == 48, "the corona vertex is the console's 48-byte packed record");
    static_assert(offsetof(CoronaVertex, mafPositionAndDistance) ==  0, "POSITION0 at +0");
    static_assert(offsetof(CoronaVertex, mafDirection)           == 16, "NORMAL0 at +16");
    static_assert(offsetof(CoronaVertex, mafUvAndOffset)         == 28, "TEXCOORD0 at +28");
    static_assert(offsetof(CoronaVertex, muColour)               == 44, "COLOR0 at +44");

    // The Xenos immediate-vertex stride the console passes to D3DDevice_BeginVertices.
    const u32 KU_CORONA_VERTEX_STRIDE = 48u;

    // ---------------------------------------------------------------------------------------------
    // [FLAG PC-platform leaf] THE QUADLIST TRANSLATION.
    //
    // The console emits `D3DDevice_BeginVertices(dev, 13, 4 * n, 48)` -- Xenos primitive type 13 is
    // QUADLIST, four vertices per quad, which the GPU splits into (0,1,2) + (0,2,3). D3D9 HAS NO
    // QUADLIST, and this backend's own translation table refuses it by name:
    //     XenonD3D9Shims.cpp:117-131 MapPrimitive -- "Types MapPrimitive refuses -- Xenos RECTLIST
    //     (8) and QUADLIST (13) -- have no DrawPrimitiveUP form".
    // So the PC pass emits the SAME FOUR CORNERS as SIX vertices in the QUADLIST's own triangle
    // order (0,1,2, 0,2,3) and asks for Xenos type 4 == D3DPT_TRIANGLELIST. The vertex CONTENT is
    // byte-identical to the console's; only the count changes, and the two duplicated corners are
    // the QUADLIST expansion the Xenos does in hardware.
    //
    // DELETE-WHEN: never -- this is a permanent platform difference, not a bring-up shim. It is
    // flagged so a reader does not "restore" the 13.
    // ---------------------------------------------------------------------------------------------
    const u32 KU_XENOS_PRIMITIVE_TRIANGLELIST = 4u;
    const u32 KU_VERTICES_PER_CORONA          = 6u;

    // ---------------------------------------------------------------------------------------------
    // [FLAG PC-platform leaf] THE BATCH CAP.
    //
    // The console computes it from the immediate ring's byte size:
    //     v14 = (dword_82F87B40 >> 2) / 0x30;  if (v14 > 0xFFFF) v14 = 0xFFFF;  v15 = v14 >> 2;
    // i.e. (ringBytes/4) / stride, clamped, / 4 vertices per corona. dword_82F87B40 is the CONSOLE
    // ring's size and is not recovered -- and it is the wrong number here anyway: on PC the run goes
    // into XenonD3D9Shims' own 1 MiB immediate scratch, and BeginVertices REFUSES (returns nullptr)
    // any run bigger than that rather than overrunning.
    //
    // The cap below is the corona buffer's own capacity (BrnCoronaManager::BrnSubmissionInterface::
    // KI_MAX_CORONAS == 512, DWARF-attested, and the size Construct passes to
    // CoronaBuffer::Parameters::SetNumCoronas @0x823FCE1C `li r11, 0x200`), which needs
    // 512 * 6 * 48 == 147,456 bytes -- comfortably inside the 1 MiB scratch. The chunking LOOP is
    // kept (it is the console's shape and it is what makes a future larger buffer safe); with this
    // cap it simply runs once.
    // ---------------------------------------------------------------------------------------------
    const u32 KU_MAX_CORONAS_PER_BATCH = 512u;

    // ---------------------------------------------------------------------------------------------
    // [DIAG corona-calib -- coronas step 2] THE CALIBRATION CAPTURE.
    //
    // Begin() is handed the two values the calibration turns on (the frame white level, in
    // RenderParameters::mvCameraPositionPlusWhiteLevel.w, and viewXyScale) and Dispatch() is where
    // the batch count is known, so Begin parks them here for Dispatch's one-shot line. Not console
    // state: the console has no such capture.
    // DELETE-WHEN the corona calibration is signed off.
    // ---------------------------------------------------------------------------------------------
    f32 gfCalibWhiteLevel   = 0.0f;
    f32 gfCalibViewXyScaleX = 0.0f;
    f32 gfCalibViewXyScaleY = 0.0f;

    void CoronaLog(const char* lpcMessage)
    {
        CgsDev::Log::WriteToLog(lpcMessage);
    }
}

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- open one shader-constant
// row and return the write cursor (X360 r3 @0x822768D0). The SHARED declaration-only spelling this
// tree standardised on: the same line appears at CgsIm2dUntex.cpp:82, CgsIm2dColTex.cpp:91,
// CgsIm3d.cpp:91, CgsIm3dSkyDome.cpp:113, CgsIm3dZOnly.cpp:70 and BrnIm3d.cpp:118, and the ONE
// definition is pc/gcm/renderengine/ImmediateModePCLeaf.cpp:729 (mounted, build_game_exe.bat:316).
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

// The Xenon immediate-vertex ring intrinsics (defined for PC in
// pc/gcm/renderengine/XenonD3D9Shims.cpp:3646/3775). Declared at file scope exactly as
// CgsIm2dUntex.cpp:102-104, BrnSkidVertex.cpp:103-104 and BrnPostFxShader.cpp:177-179 declare them.
struct D3DDevice;
extern "C" void* D3DDevice_BeginVertices(D3DDevice* lpDevice, u32 luPrimitiveType,
                                         u32 luVertexCount, u32 luVertexStreamZeroStride);
extern "C" void  D3DDevice_EndVertices(D3DDevice* lpDevice);

namespace renderengine
{
    // SEAM S1 -- see the anonymous-namespace banner above.
    extern const u8  gauCoronaVertexProgramPC[];
    extern const u32 guCoronaVertexProgramPCSize;
    extern const u8  gauCoronaPixelProgramPC[];
    extern const u32 guCoronaPixelProgramPCSize;

    // ---- the DWARF-named private statics --------------------------------------------------------
    u32                       CoronaRenderer::s_numSubImages               = 0;
    bool                      CoronaRenderer::s_beginLock                  = false;
    bool                      CoronaRenderer::s_beginBatchLock             = false;
    VertexDescriptorData*     CoronaRenderer::s_vertexDescriptor           = 0;
    ProgramBufferData*        CoronaRenderer::s_pixelProgram               = 0;
    ProgramBufferData*        CoronaRenderer::s_vertexProgram              = 0;
    ProgramVariableHandle     CoronaRenderer::s_cameraPositionHandle       = {};
    ProgramVariableHandle     CoronaRenderer::s_viewProjectionMatrixHandle = {};
    ProgramVariableHandle     CoronaRenderer::s_viewXyScaleHandle          = {};
    const Vector2*            CoronaRenderer::s_atlasUVs                   = 0;
    const TextureState*       CoronaRenderer::s_textureState               = 0;
    const BlendMaterialState* CoronaRenderer::s_blendState                 = 0;
    const DepthStencilState*  CoronaRenderer::s_enabledZTestState          = 0;
    const DepthStencilState*  CoronaRenderer::s_disabledZTestState         = 0;

    // =============================================================================================
    // CoronaRenderer::Initialize -- X360 @0x822850F8.
    //
    // Console: build a PIXEL ProgramBuffer from &unk_8200F1B8 (shaderType 1, 228 bytes), then a
    // VERTEX one from &unk_8200F2A0 (shaderType 0, 788 bytes), resolve the three named constants out
    // of the VERTEX program, then build the four-element VertexDescriptor. Each of the three
    // resources goes GetResourceDescriptor -> allocator vtable +0x10 -> Initialize.
    //
    // (VERTEX/PIXEL WERE INVERTED IN THE FIRST RECONSTRUCTION and were corrected in carlights step 1;
    // four independent proofs -- programbuffer.h:71's `!= 0 -> pixel`, Begin's
    // SetPixelProgram(dword_82FAB6B0) / vertex-shadow compare against dword_82FAB6B4, all three
    // GetVariableHandleByName calls taking dword_82FAB6B4, and the DWARF declaring s_pixelProgram
    // (:106) before s_vertexProgram (:107). The order here is the asm's: pixel first.)
    //
    // PC: the two programs are ADOPTED (see banner item 2) and the allocator is therefore unused for
    // them. It is still used for nothing else either -- ImmediateModePCLeaf's
    // VertexDescriptor::Initialize carves the descriptor from its own arena and documents why
    // (the x64 VertexDescriptorData is 0x124 bytes while the console sizer returns 50). The
    // parameter is kept because it is the DWARF signature and because the console route returns the
    // moment a real resource allocator matters again.
    // =============================================================================================
    void CoronaRenderer::Initialize(rw::IResourceAllocator& /*lrAllocator*/)
    {
        // --- pixel program (built FIRST on the console -- shaderType 1) ---------------------------
        s_pixelProgram = ProgramBufferPC_Adopt(gauCoronaPixelProgramPC,
                                               guCoronaPixelProgramPCSize, 1u);

        // --- vertex program (built SECOND -- shaderType 0) ----------------------------------------
        s_vertexProgram = ProgramBufferPC_Adopt(gauCoronaVertexProgramPC,
                                                guCoronaVertexProgramPCSize, 0u);

        if (s_vertexProgram == 0 || s_pixelProgram == 0)
        {
            // ProgramBufferPC_Adopt already logged WHICH check failed. Refuse the pass rather than
            // half-build it: a bound null vertex program faults inside
            // shadow::Device::FlushVertexProgramState (its own banner says GetD3DVertexShader() is
            // program + 0x14, so null yields the address 0x14).
            s_vertexProgram = 0;
            s_pixelProgram  = 0;
            CoronaLog("[corona] CoronaRenderer::Initialize: the authored PC program pair was"
                      " REFUSED - the corona pass stays off\n");
            return;
        }

        // The three named constants come out of the VERTEX program.
        ProgramBuffer::GetVariableHandleByName(s_vertexProgram, gszViewProjectionMatrix,
                                               &s_viewProjectionMatrixHandle);
        ProgramBuffer::GetVariableHandleByName(s_vertexProgram, gszCameraPositionPlusBrightness,
                                               &s_cameraPositionHandle);
        ProgramBuffer::GetVariableHandleByName(s_vertexProgram, gszViewXyScale,
                                               &s_viewXyScaleHandle);

        // --- the vertex declaration ----------------------------------------------------------------
        // Written through the GROUND-TRUTH byte lanes (VertexDescriptor.h:44-58): the committed
        // member names are shifted one lane, so `miOffset` IS the format word and `mu8UsageIndex` IS
        // the element type. The offset lane (mu16Pad0) is deliberately LEFT at the ctor's 0xFFFF --
        // >= 0x100 is the auto-pack request that produces the console's packed 0/16/28/44 layout.
        VertexDescriptor::Parameters lVertexDecl;   // ctor seeds all 16 slots to empty

        lVertexDecl.maElements[0].mu16Stream    = 0;
        lVertexDecl.maElements[0].miOffset      = static_cast<s32>(KU_ELEMENT_FORMAT_FLOAT4);
        lVertexDecl.maElements[0].mu8UsageIndex = KU8_ELEMENT_TYPE_POSITION;

        lVertexDecl.maElements[1].mu16Stream    = 0;
        lVertexDecl.maElements[1].miOffset      = static_cast<s32>(KU_ELEMENT_FORMAT_FLOAT3);
        lVertexDecl.maElements[1].mu8UsageIndex = KU8_ELEMENT_TYPE_NORMAL;

        lVertexDecl.maElements[2].mu16Stream    = 0;
        lVertexDecl.maElements[2].miOffset      = static_cast<s32>(KU_ELEMENT_FORMAT_FLOAT4);
        lVertexDecl.maElements[2].mu8UsageIndex = KU8_ELEMENT_TYPE_TEXCOORD0;

        lVertexDecl.maElements[3].mu16Stream    = 0;
        lVertexDecl.maElements[3].miOffset      = static_cast<s32>(KU_ELEMENT_FORMAT_COLOUR);
        lVertexDecl.maElements[3].mu8UsageIndex = KU8_ELEMENT_TYPE_COLOUR0;

        // The console sizes the descriptor and carves it from the allocator; the PC leaf's
        // Initialize ignores the resource and uses its own arena (its banner explains why the
        // console sizer cannot be used on x64), so the GetResourceDescriptor/Create pair is not run.
        s_vertexDescriptor = VertexDescriptor::Initialize(0, &lVertexDecl);

        {
            char lacMessage[224];
            std::snprintf(lacMessage, sizeof(lacMessage),
                          "[corona] renderer initialised: vs=%d ps=%d decl=%d"
                          " constants vp{reg=%u,count=%u} cam{reg=%u,count=%u} xy{reg=%u,count=%u}\n",
                          (int)(s_vertexProgram != 0), (int)(s_pixelProgram != 0),
                          (int)(s_vertexDescriptor != 0),
                          (unsigned)s_viewProjectionMatrixHandle.mu8RegisterSet,
                          (unsigned)s_viewProjectionMatrixHandle.mu8RegisterCount,
                          (unsigned)s_cameraPositionHandle.mu8RegisterSet,
                          (unsigned)s_cameraPositionHandle.mu8RegisterCount,
                          (unsigned)s_viewXyScaleHandle.mu8RegisterSet,
                          (unsigned)s_viewXyScaleHandle.mu8RegisterCount);
            CoronaLog(lacMessage);
        }

        // A resolved-but-not-found handle reads mu8RegisterCount == 0 and
        // RenderEngineDeviceBeginShaderStates sends it to the discard row -- i.e. the constant would
        // never reach the shader and every corona would billboard against a zero matrix. Say so
        // once instead of drawing a garbage pass (AGENTS.md rule 9).
        if (s_viewProjectionMatrixHandle.mu8RegisterCount == 0u
            || s_cameraPositionHandle.mu8RegisterCount == 0u
            || s_viewXyScaleHandle.mu8RegisterCount == 0u)
        {
            CoronaLog("[corona] CoronaRenderer::Initialize: the authored vertex program does NOT"
                      " declare viewProjectionMatrix / cameraPositionPlusBrightness / viewXyScale"
                      " by those exact names - the corona pass stays off\n");
            s_vertexProgram = 0;
            s_pixelProgram  = 0;
        }
    }

    // rwgcoronarenderer.h:75 -- inlined into BrnCoronaManager::Construct (`stw r11,
    // dword_82FAB6C0@l(r9)` @0x823FCDDC).
    void CoronaRenderer::SetBlendState(const BlendMaterialState* lpBlendState)
    {
        s_blendState = lpBlendState;
    }

    // rwgcoronarenderer.h:77 -- inlined into BrnCoronaManager::Construct (@0x823FCDF0 / @0x823FCDF8,
    // in that order: the ZON_ZLEQ_ZWRITEOFF object first, then ZOFF_ZALL_ZWRITEOFF).
    void CoronaRenderer::SetDepthStencilStates(const DepthStencilState* lpEnabledZTest,
                                               const DepthStencilState* lpDisabledZTest)
    {
        s_enabledZTestState  = lpEnabledZTest;
        s_disabledZTestState = lpDisabledZTest;
    }

    // rwgcoronarenderer.h:80 -- inlined into BrnCoronaManager::SetTextureAtlas (@0x823FD108-2C:
    // dword_82FAB6BC = the TextureState*, dword_82FAB6A8 = 4, dword_82FAB6B8 = &s_atlasUVs[0]) and,
    // with (0, 1, &s_atlasUVs[0]), into BrnCoronaManager::Construct (@0x823FCE04-18).
    void CoronaRenderer::SetTextureAtlas(const TextureState* lpTextureState, u32 luNumSubImages,
                                         const Vector2* lpAtlasUVs)
    {
        s_textureState  = lpTextureState;
        s_numSubImages  = luNumSubImages;
        s_atlasUVs      = lpAtlasUVs;
    }

    // [FLAG PC bring-up gate] -- see the header. Not an X360 function.
    bool CoronaRenderer::IsReady()
    {
        return s_vertexProgram != 0 && s_pixelProgram != 0 && s_vertexDescriptor != 0
            && s_textureState != 0 && s_atlasUVs != 0
            && s_blendState != 0 && s_enabledZTestState != 0 && s_disabledZTestState != 0;
    }

    // =============================================================================================
    // CoronaRenderer::Begin -- X360 @0x823FF2C0.
    //
    // The asm, in order:
    //     byte_82FAB695 = 1                                     -- s_beginLock
    //     sub_8227D158(dword_82FAB6BC, 0)                        -- SetState(const TextureState*, unit 0)
    //     if (!byte_83010907 && dword_83010964 != dword_82FAB6C0)
    //         Xbox2SetStateLowLevelShadowed(dword_82FAB6C0, dword_83010964 == 0);
    //         dword_83010964 = dword_82FAB6C0;
    //     if (dword_8301095C != dword_82FAB6B4) { SetVertexProgramInternal(); dword_8301095C = ...; }
    //     SetPixelProgram(dword_82FAB6B0)
    //     BeginShaderStates(&byte_82FAB61C, cursor); write 64 bytes from params+0x20; cursor += 64
    //     BeginShaderStates(&byte_82FAB620, cursor); write 16 bytes from params+0x10; cursor += 16
    //     BeginShaderStates(&byte_82FAB618, cursor); write 16 bytes from params+0x60
    //
    // TWO OF THOSE BLOCKS ARE INLINED SETTERS THIS TREE ALREADY OWNS BY NAME, and they are
    // DE-INLINED here rather than transliterated (AGENTS.md "inlining reversal", and rule 3 -- the
    // shadow words are shadow::Device PRIVATE statics, so reaching them from here would mean a
    // second host home for state the class already owns):
    //   * the blend block IS shadow::Device::SetState(const BlendMaterialState*) @0x82276A68 --
    //     shadowingdevice.h:157/:299 ("lock gate -> pointer compare against its own slot ->
    //     low-level apply with lbWasUnset == (cached == nullptr) -> cache the wanted state"), which
    //     is byte-for-byte the four lines above (byte_83010907 = mbBlendStateLocked,
    //     dword_83010964 = mpBlendState);
    //   * the vertex-program block IS shadow::Device::SetVertexProgram @0x82276BA0
    //     (shadowingdevice.cpp:329: compare mpVertexProgramShadow -> SetVertexProgramInternal() ->
    //     store), which is why the pseudocode shows an argument on a function that takes none.
    // =============================================================================================
    void CoronaRenderer::Begin(const RenderParameters& lrParameters)
    {
        // [FLAG PC bring-up gate] Initialize refuses the pass (nulls s_vertexProgram/s_pixelProgram)
        // when the authored pair or a named constant is missing; the console cannot be in that
        // state. Do not touch the device (SetVertexProgram(nullptr) / SetPixelProgram(nullptr))
        // from a refused pass -- Dispatch is gated on the same predicate.
        if (!IsReady())
            return;

        s_beginLock = true;                                             // byte_82FAB695 = 1

        shadow::Device::SetState(s_textureState, 0u);                   // the atlas on sampler unit 0
        shadow::Device::SetState(s_blendState);                         // additive, no alpha test
        shadow::Device::SetVertexProgram(s_vertexProgram);
        shadow::Device::SetPixelProgram(s_pixelProgram);

        // The three constant publications. RenderEngineDeviceBeginShaderStates hands back the row to
        // write into and the console advances its cursor by the constant's size between calls; on
        // this backend each call returns its OWN staged row (ImmediateModePCLeaf.cpp:729), so the
        // console's cursor arithmetic is a no-op here and is not reproduced -- the rows are written
        // where the call says to write them. shadow::Device::FlushVertexProgramState (issued by
        // Dispatch, below) is the drain, which is the same point in the frame the console's
        // command-buffer rows become visible to the GPU.
        {
            void* lpRow = 0;
            RenderEngineDeviceBeginShaderStates(&s_viewProjectionMatrixHandle, &lpRow);
            if (lpRow != 0)
                std::memcpy(lpRow, &lrParameters.mViewProjectionMatrix, 64);          // params +0x20, 64 B
        }
        {
            void* lpRow = 0;
            RenderEngineDeviceBeginShaderStates(&s_cameraPositionHandle, &lpRow);
            if (lpRow != 0)
                std::memcpy(lpRow, &lrParameters.mvCameraPositionPlusWhiteLevel, 16); // +0x10, 16 B
        }
        {
            void* lpRow = 0;
            RenderEngineDeviceBeginShaderStates(&s_viewXyScaleHandle, &lpRow);
            if (lpRow != 0)
                std::memcpy(lpRow, &lrParameters.mvViewXyScale, 16);                   // +0x60, 16 B
        }

        // [DIAG corona-calib] park the two published values for Dispatch's one-shot line.
        gfCalibWhiteLevel   = lrParameters.mvCameraPositionPlusWhiteLevel.w;
        gfCalibViewXyScaleX = lrParameters.mvViewXyScale.x;
        gfCalibViewXyScaleY = lrParameters.mvViewXyScale.y;
    }

    // =============================================================================================
    // CoronaRenderer::End -- INLINED on the console; recovered from its one call site,
    // BrnCoronaManager::Render @0x824076B4-BC (`lis r10, byte_82FAB695@ha` / `li r11, 0` /
    // `stb r11, byte_82FAB695@l(r10)`), i.e. the exact undo of Begin's first act.
    //
    // s_beginBatchLock (DWARF :104) has NO attested writer anywhere in the corona call graph -- Begin
    // does not touch it and neither does Dispatch. It is declared because the DWARF declares it and
    // left alone; nothing here asserts on it.
    // =============================================================================================
    void CoronaRenderer::End()
    {
        s_beginLock = false;
    }

    // =============================================================================================
    // CoronaRenderer::Dispatch -- X360 @0x82404F30.
    //
    // The asm, in order:
    //     state = (BatchParameters.m_flags & 1) ? dword_82FAB6C4 : dword_82FAB6C8;
    //     sub_82276AD0(state)                                  -- SetState(const DepthStencilState*)
    //     maxPerBatch = min((ring>>2)/0x30, 0xFFFF) >> 2        -- see the KU_MAX_CORONAS_PER_BATCH note
    //     if (off_83010958 != dword_82FAB6AC) { byte_83010A34 = 1; off_83010958 = dword_82FAB6AC; }
    //                                                          -- SetVertexDescriptor, inlined
    //     FlushVertexProgramState()
    //     for each chunk n:
    //         if (192 * n > 0x80000) D3DDevice_InsertFence(dev)
    //         cursor = D3DDevice_BeginVertices(dev, 13, 4*n, 48) - 4
    //         per record: four Write() calls, corners in this order --
    //             0: uv[0], (-sizeX, +sizeY)   1: uv[1], (+sizeX, +sizeY)
    //             2: uv[2], (+sizeX, -sizeY)   3: uv[3], (-sizeX, -sizeY)
    //           position = record[+0x00].xyz with w := record[+0x30] (the vrlimi128 w-lane splice)
    //           uv row   = &s_atlasUVs[record[+0x38] * 4]   (`v23 << 6` == 4 x a 16-byte Vector2)
    //         D3DDevice_EndVertices(dev)
    //
    // PC DEVIATIONS, each disclosed:
    //   * QUADLIST -> TRIANGLELIST, six vertices per corona in the QUADLIST's own (0,1,2)+(0,2,3)
    //     order (see the KU_XENOS_PRIMITIVE_TRIANGLELIST banner).
    //   * D3DDevice_InsertFence is a console GPU-ring fence with no D3D9 counterpart and no home in
    //     this tree; the PC ring is XenonD3D9Shims' own scratch and BeginVertices bounds-checks it
    //     itself, so the fence arm is dropped rather than declared-and-never-defined.
    //   * the vertex-descriptor bind is shadow::Device::SetVertexDescriptor (shadowingdevice.h:70,
    //     which documents that the console does exactly this inline).
    // =============================================================================================
    void CoronaRenderer::Dispatch(const BatchParameters& lrParameters, CoronaBuffer* lpBuffer)
    {
        CGS_ASSERT(lpBuffer != 0, "lpBuffer != NULL");
        if (lpBuffer == 0 || !IsReady())
            return;

        // FLAG_OCCLUSION_ZTEST picks the depth-TESTED state. BrnCoronaManager::Render always passes
        // it, so the live arm is ZON_ZLEQ_ZWRITEOFF.
        shadow::Device::SetState((lrParameters.muFlags & BatchParameters::FLAG_OCCLUSION_ZTEST) != 0u
                                     ? s_enabledZTestState
                                     : s_disabledZTestState);

        shadow::Device::SetVertexDescriptor(s_vertexDescriptor);
        shadow::Device::FlushVertexProgramState();

        // =========================================================================================
        // [DIAG corona-calib -- coronas step 2] THE CALIBRATION LINE (WAVE_NOTE's own format,
        // extended with the fields that are actually measurable here).
        //
        // Every number below is READ from the live objects, not restated from a comment:
        //   * whiteLevel / viewXyScale come from the RenderParameters Begin published this frame
        //     (BrnCoronaManager::Render fills them from maShaderConstantsFrames[internal]
        //     .GetWhiteLevel() and from the interface's mViewXyScale);
        //   * blendRT0 is s_blendState->maState[E_WORD_BLEND_RT0], the packed Xenos RB_BLENDCONTROL
        //     word shadow::Device::Xbox2SetStateLowLevelShadowed pushes through
        //     D3DDevice_SetBlendState -- decoded here with the SAME field split that function uses
        //     (XenonD3D9Shims.cpp:4120-4122: COLOR_SRCBLEND [4:0], COLOR_COMB_FCN [7:5],
        //     COLOR_DESTBLEND [12:8], ALPHA_SRCBLEND [20:16], ALPHA_COMB_FCN [23:21],
        //     ALPHA_DESTBLEND [28:24]). CgsBlendStateFactory::Construct builds slot 2
        //     (TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA) with maBlendFactor[0] == 0x07060106
        //     (CgsBlendStateFactory.cpp:203, X360 `li r9,0x83` @0x827EB524 + `rlwimi` @0x827EB548),
        //     so this must print src=6 (SRC_ALPHA) op=0 (ADD) dst=1 (ONE);
        //   * alphaTest / colourWrite are maState[16] / maState[4] of the same object;
        //   * subImages is s_numSubImages (SetTextureAtlas's second argument);
        //   * drawsThisFrame is the batch count BrnCoronaManager::Render read out of the
        //     submission iterator -- the number that must equal the producer's submitted count.
        // The two `leaf-const` lanes are NOT measured: this backend never sets
        // D3DSAMP_SRGBTEXTURE or D3DRS_SRGBWRITEENABLE to anything but FALSE
        // (XenonD3D9Shims.cpp:3423/3462/3478/5042 and :6913), and the corona TU has no honest
        // route to the IDirect3DDevice9 to read them back, so they are printed with that label
        // rather than as a measurement. The console-side answer is INFERRED, not measured: the
        // corona atlas (corona_atlas.TextureConfig2d ID 297312) is not in GLOBALTEXTUREDICTIONARY
        // .BIN (searched, 0 hits) and no shipped bundle in build/game carries that resource id, so
        // its Xenos fetch-constant GPUSIGN field has not been read. What IS measured: all 8
        // textures of GLOBALTEXTUREDICTIONARY are GPUSIGN_UNSIGNED (no gamma channel), and this
        // backend never sets D3DSAMP_SRGBTEXTURE / D3DRS_SRGBWRITEENABLE to anything but FALSE.
        // [FLAG] the atlas's own GPUSIGN stays a named park until its bundle is located.
        // DELETE-WHEN the corona calibration is signed off.
        // =========================================================================================
        {
            static bool sbLoggedCalib = false;
            if (!sbLoggedCalib && s_blendState != 0)
            {
                sbLoggedCalib = true;
                const u32 luBlend = s_blendState->maState[BlendMaterialState::E_WORD_BLEND_RT0];
                char lacMessage[352];
                std::snprintf(lacMessage, sizeof(lacMessage),
                              "[corona-calib] whiteLevel=%.3f blendRT0=0x%08X"
                              " (colour src=%u op=%u dst=%u | alpha src=%u op=%u dst=%u)"
                              " alphaTest=%u colourWrite=0x%X depth=%s"
                              " srgbTex=0(leaf-const) srgbWrite=0(leaf-const)"
                              " viewXyScale=(%.4f %.4f) subImages=%u drawsThisFrame=%u\n",
                              gfCalibWhiteLevel, (unsigned)luBlend,
                              (unsigned)( luBlend        & 0x1Fu),
                              (unsigned)((luBlend >>  5) & 0x07u),
                              (unsigned)((luBlend >>  8) & 0x1Fu),
                              (unsigned)((luBlend >> 16) & 0x1Fu),
                              (unsigned)((luBlend >> 21) & 0x07u),
                              (unsigned)((luBlend >> 24) & 0x1Fu),
                              (unsigned)s_blendState->maState[BlendMaterialState::E_WORD_ALPHA_TEST_ENABLE],
                              (unsigned)s_blendState->maState[BlendMaterialState::E_WORD_COLOUR_WRITE_ENABLE],
                              ((lrParameters.muFlags & BatchParameters::FLAG_OCCLUSION_ZTEST) != 0u)
                                  ? "ZON_ZLEQ_ZWRITEOFF" : "ZOFF_ZALL_ZWRITEOFF",
                              gfCalibViewXyScaleX, gfCalibViewXyScaleY,
                              (unsigned)s_numSubImages,
                              (unsigned)lrParameters.muNumCoronas);
                CoronaLog(lacMessage);
            }
        }

        u32           luCoronasLeft = lrParameters.muNumCoronas;
        const Corona* lpCorona      = lpBuffer->GetCoronas();
        if (lpCorona == 0)
            return;

        // [FLAG PC bring-up diagnostic] one line, the first time the pass actually emits geometry.
        // It prints the EXTENT of what is drawn and the first corona's atlas row, because the two
        // ways this pass can fail silently are (a) a zeroed atlas UV table -- every quad samples one
        // texel and the frame still "looks fine", and (b) a wrong colour lane. DELETE with the
        // bring-up.
        static bool sbLoggedFirstDraw = false;

        while (luCoronasLeft != 0u)
        {
            u32 luChunk = KU_MAX_CORONAS_PER_BATCH;
            if (luCoronasLeft < luChunk)
                luChunk = luCoronasLeft;

            CoronaVertex* lpVertex = static_cast<CoronaVertex*>(
                D3DDevice_BeginVertices(0, KU_XENOS_PRIMITIVE_TRIANGLELIST,
                                        KU_VERTICES_PER_CORONA * luChunk,
                                        KU_CORONA_VERTEX_STRIDE));

            if (lpVertex != 0)
            {
                for (u32 luIndex = 0; luIndex < luChunk; ++luIndex)
                {
                    const Corona& lrCorona = lpCorona[luIndex];

                    // position.xyz with the bias distance spliced into the w lane (the console's
                    // `vrlimi128 v0, v13, 1, 1` after splatting record[+0x30]).
                    const f32 lafPosition[4] = { lrCorona.mvPosition.x, lrCorona.mvPosition.y,
                                                 lrCorona.mvPosition.z, lrCorona.mfDistance };
                    const f32 lfSizeX = lrCorona.mvSize.x;
                    const f32 lfSizeY = lrCorona.mvSize.y;

                    const Vector2* const lpUvRow = &s_atlasUVs[static_cast<u32>(lrCorona.miTextureID) << 2];

                    // The four QUADLIST corners, in the console's own order.
                    const f32 lafCorner[4][4] =
                    {
                        { lpUvRow[0].x, lpUvRow[0].y, -lfSizeX,  lfSizeY },
                        { lpUvRow[1].x, lpUvRow[1].y,  lfSizeX,  lfSizeY },
                        { lpUvRow[2].x, lpUvRow[2].y,  lfSizeX, -lfSizeY },
                        { lpUvRow[3].x, lpUvRow[3].y, -lfSizeX, -lfSizeY },
                    };

                    // (0,1,2) + (0,2,3) -- the Xenos QUADLIST's own triangle split.
                    static const u32 kauQuadOrder[KU_VERTICES_PER_CORONA] = { 0u, 1u, 2u, 0u, 2u, 3u };

                    for (u32 luVertex = 0; luVertex < KU_VERTICES_PER_CORONA; ++luVertex)
                    {
                        const u32 luCorner = kauQuadOrder[luVertex];
                        for (u32 luLane = 0; luLane < 4u; ++luLane)
                        {
                            lpVertex->mafPositionAndDistance[luLane] = lafPosition[luLane];
                            lpVertex->mafUvAndOffset[luLane]         = lafCorner[luCorner][luLane];
                        }
                        lpVertex->mafDirection[0] = lrCorona.mvDirection.x;
                        lpVertex->mafDirection[1] = lrCorona.mvDirection.y;
                        lpVertex->mafDirection[2] = lrCorona.mvDirection.z;
                        lpVertex->muColour        = lrCorona.muColour;
                        ++lpVertex;
                    }

                    if (!sbLoggedFirstDraw && luIndex == 0u)
                    {
                        sbLoggedFirstDraw = true;
                        char lacMessage[288];
                        std::snprintf(lacMessage, sizeof(lacMessage),
                                      "[corona] first draw: %u quads (%u tris), stride=%u, sampler unit 0,"
                                      " corona0 pos=(%.2f %.2f %.2f) size=(%.3f %.3f) tex=%d"
                                      " colour=0x%08X uvRow=(%.3f,%.3f)(%.3f,%.3f)(%.3f,%.3f)(%.3f,%.3f)\n",
                                      (unsigned)luChunk, (unsigned)(luChunk * 2u),
                                      (unsigned)KU_CORONA_VERTEX_STRIDE,
                                      lafPosition[0], lafPosition[1], lafPosition[2],
                                      lfSizeX, lfSizeY, (int)lrCorona.miTextureID,
                                      (unsigned)lrCorona.muColour,
                                      lpUvRow[0].x, lpUvRow[0].y, lpUvRow[1].x, lpUvRow[1].y,
                                      lpUvRow[2].x, lpUvRow[2].y, lpUvRow[3].x, lpUvRow[3].y);
                        CoronaLog(lacMessage);
                    }
                }
            }

            D3DDevice_EndVertices(0);

            lpCorona      += luChunk;
            luCoronasLeft -= luChunk;
        }
    }
}
