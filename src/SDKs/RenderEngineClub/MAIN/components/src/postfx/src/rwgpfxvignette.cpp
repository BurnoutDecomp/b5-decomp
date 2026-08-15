#include "types.hpp"

#include <cmath>     // pow -- the vignette sharpness gradient (X360: a call to the CRT pow)
#include <cstring>   // memset

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxvignette.h"   // Vignette
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"     // PfxHelper::CreateProgram
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"           // BlendState
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"        // ProgramBuffer
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                         // CgsDev::Log::WriteToLog
#include "pc/gcm/renderengine/renderstates.h"                                      // DepthStencilState / RasterizerState
#include "pc/gcm/renderengine/VertexDescriptor.h"                                  // VertexDescriptor
#include "rw/rwcore_structs.h"                                                     // rw::IResourceAllocator / Resource
#include "rw/math/vpu/types.h"                                                     // rw::math::vpu::Vector4

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::Vignette::Parameters::Parameters @ 0x823F5420
//   rw::graphics::postfx::Vignette::Release                @ 0x823F93D0
//   rw::graphics::postfx::Vignette::SetState               @ 0x823FE8B0
//   rw::graphics::postfx::Vignette::Vignette               @ 0x82403DB8
//
// Vignette is the post-fx screen-border darkening effect. The constructor builds the four render
// states the full-screen quad draws through (opaque-with-blend BlendState, Position+UV
// VertexDescriptor, no-depth DepthStencilState, no-cull RasterizerState) via the standard render-engine
// create idiom (build a Parameters block -> GetResourceDescriptor sizes it -> the allocator's
// vtable+0x10 call carves the rw::Resource -> Initialize constructs in place), compiles the vignette
// vertex + pixel programs and binds their shader-constant handles, then seeds the shader constants
// from the supplied State. Release hands every one of those resources back.
//
// ==================================================================================================
// ⚠ THE PREVIOUS REVISION OF SetState WAS A FABRICATION. READ THIS BEFORE "RESTORING" IT.
//
// It declared and called a twelve-parameter helper
//     f32 ComputeVignetteGradient(Vignette*, const State*, u32, u32, u32, const void* lpGradientTable,
//                                 const void*, void*, u32, u32, f32, f32);
// described as "the X360 SetState mapping helper (sub_82C09970) ... maps the sharpness through a
// 0.5-centred blend and a GRADIENT TABLE (unk_82040000)", and it minted three file-scope placeholder
// blobs (gauVignetteGradientTable and the two microcode symbols) to feed it.
//
// sub_82C09970 IS THE CRT `pow`. Its body calls _powhlp / _decomp / _d_inttype / _get_exp / _set_exp /
// log / copysign against the standard 16-entry log/exp tables at dbl_82188D28..dbl_82188E68, and its
// 31 xrefs include `pow` @0x82674CD0 itself plus XAUDIO::CSourceStream::SetPitch,
// BrnMath::RoundWithNumSignificantFigures, rw::audio::core::Butterworth::CalculateFilterCoefficients
// and CgsInput::DeviceX360Pad::SetRumble. There is no vignette gradient table: `unk_82040000` never
// appears in SetState's assembly at all -- it (and the nine phantom integer arguments) came from
// Hex-Rays inventing a prototype for an unnamed CRT routine that reads globals.
//
// What the assembly actually does, in nine instructions:
//     lvx128 v0, r0, r30 / vspltw v0, v0, 0     v0 = broadcast(state.m_gradientScalars.x)
//     vmaddfp v0, v0, v13(0.5), v12(0.5)        x = sharpness * 0.5 + 0.5
//     lfs f1, var_30(r1) ; lfd f2, dbl_82046258 f1 = x (promoted), f2 = 16.0
//     bl sub_82C09970                           f1 = pow(x, 16.0)
//     frsp f13, f1 ; fmuls f0, f13, flt_8200A034 (500.0)
//     fadds f13, f0, flt_82001C98 (1.0)         -> m_gradientScalars.z = g*500 + 1
//     fneg  f0, f0                              -> m_gradientScalars.w = -(g*500)
// The sibling BrnPostFxShader.cpp:1218-1221 had already decoded the SAME curve independently, from
// the composite's own copy of it, naming the two constants KF_VIGNETTE_GRADIENT_POWER (16.0,
// dbl_82046258) and KF_VIGNETTE_GRADIENT_LIMIT (500.0, flt_8200A034). Two functions, one curve, and
// no table.
// ==================================================================================================

// --------------------------------------------------------------------------------------------------
// [FLAG PC bring-up: BRN_POSTFX_VIGNETTE_PROGRAMS_AVAILABLE]
//
// The X360 constructor compiles its vertex and pixel programs from XENOS MICROCODE embedded in the
// executable image (`addi r4, r31, unk_82044E88 - 0x82044FA8` / `li r5, 0x120` @0x8240412C-0x82404128
// for the vertex program, `mr r4, r31` = unk_82044FA8 / `li r5, 0x1A0` @0x82404154-0x82404150 for the
// pixel program). Those bytes are not in this tree and cannot be bound by the PC backend even if they
// were -- renderengine::ProgramBuffer::GetResourceDescriptor / ::Initialize both route through
// XGGetMicrocodeShaderParts, whose PC stub returns 0 WITHOUT writing *lpParts
// (ImmediateModePCLeaf.cpp:626-646).
//
// The previous revision declared each blob as a SINGLE `const u8 = 0` and passed 288 / 416 as its
// size. That is not an honest placeholder: it hands the program factory a one-byte object and a
// 288-byte length, i.e. an out-of-bounds read followed by a program built from whatever .rdata
// follows. Both symbols are deleted here.
//
// The gate follows the precedent this tree already set in BrnPostFxShader::Shader::Construct: when a
// PC program image exists, adopt it with renderengine::ProgramBufferPC_Adopt and keep the console
// route as the fallthrough; when none exists, leave the program slot HONESTLY EMPTY -- null pointers,
// default (register-count-0) handles, one report line naming the gate -- so nothing can draw the
// vignette with a wrong program. No PC image for these two programs exists yet, so the gate is 0 and
// the console body sits inside the #if for the wave that supplies them.
//
// FOR THAT LATER SHADER WAVE, the identity of the two programs:
//     vertex  X360 &unk_82044E88, 0x120 = 288 bytes, shader type 0, interns "uvOffset"
//     pixel   X360 &unk_82044FA8, 0x1A0 = 416 bytes, shader type 1, interns "controlScalars",
//                                                                   "innerColour", "outerColour"
// Neither is in scratch/postfx_wave1b_dossiers/DATA_DUMP.md (that dump covers 0x8203F0B8..0x82043F88,
// the twelve composite permutations, plus unk_82044D30 and unk_82040000). Extracting them needs an
// IDA session against IDA Files/BURNOUT_X360_ARTIST.XEX.i64; `idat.exe` is not on this machine.
// --------------------------------------------------------------------------------------------------
#define BRN_POSTFX_VIGNETTE_PROGRAMS_AVAILABLE 0

namespace
{
    // The X360 assembly immediates, each traced to its constant.
    const f32 KF_ZERO = 0.0f;    // flt_82001CC0
    const f32 KF_ONE  = 1.0f;    // flt_82001C98
    const f32 KF_HALF = 0.5f;    // flt_82001DA0

    // The sharpness curve. Both values are DATA_DUMP entries and both are already named by the
    // sibling composite (BrnPostFxShader.cpp:151-153) with these same names.
    const f64 KF_VIGNETTE_GRADIENT_POWER = 16.0;     // dbl_82046258 == 0x4030000000000000
    const f32 KF_VIGNETTE_GRADIENT_LIMIT = 500.0f;   // flt_8200A034 == 0x43FA0000

#if BRN_POSTFX_VIGNETTE_PROGRAMS_AVAILABLE
    const u32 KU_VIGNETTE_VERTEX_PROGRAM_SIZE = 288;     // X360 li 0x120
    const u32 KU_VIGNETTE_PIXEL_PROGRAM_SIZE  = 416;     // X360 li 0x1A0
#endif

    // The packed VertexDescriptor element words the constructor seeds (X360 immediates: v60 / v63).
    const u32 KU_VIGNETTE_VTX_ELEMENT0 = 2761657u;       // X360 0x2A23B9 (Position float3)
    const u32 KU_VIGNETTE_VTX_ELEMENT1 = 2892709u;       // X360 0x2C23A5 (UV float2)
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // Allocate a render-state resource through an allocator and copy the carved handle in (the X360
    // vtable+0x10 DoAllocate call + the 5-word copy loop; modelled as a single rw::Resource by value).
    static rw::Resource AllocateStateResource(rw::IResourceAllocator* lpAllocator,
                                              const rw::BaseResourceDescriptors<5>& lrDescriptor)
    {
        return lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lrDescriptor), nullptr);
    }

    // ============================================================================================
    // Vignette::Parameters::Parameters  @ 0x823F5420      (DWARF rwgpfxvignette.h:92)
    //
    // Seeds the construction block. Two things the assembly says that are easy to get wrong:
    //
    // 1. THE OBJECT IS 0x50 BYTES, NOT 0x90. Hex-Rays shows twelve 16-byte stack slots (v20..v31);
    //    only FOUR of them reach the object. The write set is
    //        stw r9(=0), 0(r3)                                       -> m_allocator = NULL
    //        stvx128 v0,  r0, r11        (r11 = r3 + 0x10)           -> m_state.m_gradientScalars
    //        stvx128 v13, r11, r7(0x10)                              -> m_state.m_innerColour
    //        stvx128 v12, r11, r6(0x20)                              -> m_state.m_outerColour
    //        stvx128 v11, r11, r5(0x30)                              -> m_state.m_centerScale
    //    and then a `mtctr 8` / `ld;std` loop that copies 64 bytes from the stack image at var_40
    //    back over r3+0x10..r3+0x4F. The loop's source (v28/v29/v30/v31) holds byte-for-byte the same
    //    four vectors the stvx group just stored -- it is the compiler emitting both the inlined
    //    State() default construction and the `Parameters::m_state = <temp>` copy. Sixteen bytes of
    //    object, four vectors, one redundant memcpy.
    //
    // 2. THE FOUR VALUES. v0 = {0,0,0,0} (flt_82001CC0), v13 = v12 = {1,1,1,1} (flt_82001C98),
    //    v11 = {0.5,0.5,1,1} (flt_82001DA0 for the first two lanes). Independently corroborated by
    //    BrnPostFx's own constructor, which seeds its cached copy of this state with exactly
    //    {0,0,0,0} / {1,1,1,1} / {1,1,1,1} / {0.5,0.5,1,1} -- and by the composite, which reads the
    //    last one as centre (0.5, 0.5) with scale (1, 1), the screen centre at unit scale.
    //
    // NOTE ON THE DUPLICATE DEFINITION THAT USED TO EXIST. GameShared/GameClasses/RenderWare/PostFX/
    // RwVignetteParameters.{h,cpp} defined this same mangled symbol against a SECOND, forked
    // rw::graphics::postfx::Vignette whose Parameters held `u32 muFlags` plus EIGHT Vector4s -- all
    // twelve Hex-Rays stack temporaries promoted to members. Both files were DELETED in the 2026-08-15
    // gate-flip wave; this is the one definition.
    // ============================================================================================
    Vignette::Parameters::Parameters()
        : m_allocator(nullptr)                                        // stw r9(=0), 0(r3)
    {
        m_state.m_gradientScalars = { KF_ZERO, KF_ZERO, KF_ZERO, KF_ZERO };   // v0
        m_state.m_innerColour     = { KF_ONE,  KF_ONE,  KF_ONE,  KF_ONE  };   // v13
        m_state.m_outerColour     = { KF_ONE,  KF_ONE,  KF_ONE,  KF_ONE  };   // v12
        m_state.m_centerScale     = { KF_HALF, KF_HALF, KF_ONE,  KF_ONE  };   // v11
    }

    // ============================================================================================
    // Vignette::SetState  @ 0x823FE8B0                    (DWARF rwgpfxvignette.h:131)
    // ============================================================================================
    void Vignette::SetState(const State& lrState)
    {
        // sharpness -> [0,1] about the midpoint, then the 16th power. The console promotes to double
        // for the CRT pow and rounds the result back with `frsp`; reproduced exactly. (The sibling
        // BrnPostFxShader.cpp:1219 spells the same curve with single-precision powf -- a sub-ulp
        // divergence between two sites that compute the same value; unifying them is a follow-up.)
        const f32 lfSharpParam = (lrState.m_gradientScalars.x * KF_HALF) + KF_HALF;
        const f32 lfGradient   = static_cast<f32>(
            std::pow(static_cast<f64>(lfSharpParam), KF_VIGNETTE_GRADIENT_POWER));
        const f32 lfScaled     = lfGradient * KF_VIGNETTE_GRADIENT_LIMIT;

        // The controlScalars constant: { 0, 0, gradient*500 + 1, -(gradient*500) } -- the MUL/ADD
        // pair the pixel program applies to the gradient ramp. Lanes x and y are the two 0.0 stores
        // at var_40 / var_3C; the vector is assembled on the stack and stored with one stvx.
        m_gradientScalars.x = KF_ZERO;
        m_gradientScalars.y = KF_ZERO;
        m_gradientScalars.z = lfScaled + KF_ONE;
        m_gradientScalars.w = -lfScaled;

        // The three remaining vectors copy straight across (three lvx/stvx pairs at +0x10/+0x20/+0x30).
        m_innerColour = lrState.m_innerColour;
        m_outerColour = lrState.m_outerColour;
        m_centerScale = lrState.m_centerScale;

        // `lfs f0, 4(r30)` / `stfs f0, 0x40(r31)`: the state's SECOND gradient scalar is the rotation
        // angle, and it lands in the object's own m_angle.
        m_angle = lrState.m_gradientScalars.y;
    }

    // ============================================================================================
    // Vignette::Vignette  @ 0x82403DB8                    (DWARF rwgpfxvignette.h:139)
    // ============================================================================================
    Vignette::Vignette(const Parameters& lrParameters)
    {
        // Zero the four render-state resource backing blocks (four unrolled 5-word zero runs at
        // 0x82403DE0-0x82403E70).
        std::memset(&m_blendStateResource, 0, sizeof(m_blendStateResource));
        std::memset(&m_vertexDescriptorResource, 0, sizeof(m_vertexDescriptorResource));
        std::memset(&m_depthStencilResource, 0, sizeof(m_depthStencilResource));
        std::memset(&m_rasterizerStateResource, 0, sizeof(m_rasterizerStateResource));

        // The allocator the effect keeps (`stw r10, 0xC0(r30)` @0x82403E88).
        m_allocator = lrParameters.m_allocator;

        // --- the opaque-with-blend BlendState (the shared post-fx blend, with blend-enable set) -------
        renderengine::BlendStateParameters lBlendParams;
        std::memset(&lBlendParams, 0, sizeof(lBlendParams));
        lBlendParams.maBlendFactor[0] = 0x07060706u;   // 117835526
        lBlendParams.maBlendFactor[1] = 0x07060706u;
        lBlendParams.maBlendFactor[2] = 0x07060706u;
        lBlendParams.maBlendFactor[3] = 0x07060706u;
        lBlendParams.muState15        = 7;             // v36[4]
        lBlendParams.muState4         = 15;            // v36[5]
        lBlendParams.muState5         = 15;            // v36[6]
        lBlendParams.muState6         = 15;            // v36[7]
        lBlendParams.muState7         = 15;            // v36[8]
        lBlendParams.muState8         = 135;           // v36[9] = 0x87
        lBlendParams.muState17        = 0;             // v36[10]
        lBlendParams.muState9         = static_cast<u32>(-1); // v36[11]
        lBlendParams.mbHasCustomBlendFactors = 1;      // v37 = 1
        // v36[0] = ROL(1,10) | (v36[0] & 0xFFFF0000): set the low-half blend-enable bit (0x400). The
        // factor word's low half holds the blend mode/enable; the high half is the factor's high half.
        lBlendParams.maBlendFactor[0] = 0x400u | (lBlendParams.maBlendFactor[0] & 0xFFFF0000u);

        rw::BaseResourceDescriptors<5> lBlendDescriptor;
        renderengine::BlendState::GetResourceDescriptor(&lBlendDescriptor, &lBlendParams);
        m_blendStateResource = AllocateStateResource(m_allocator, lBlendDescriptor);
        m_blendState = renderengine::BlendState::Initialize(
            reinterpret_cast<renderengine::BlendMaterialState**>(&m_blendStateResource), &lBlendParams);

        // --- the Position+UV VertexDescriptor --------------------------------------------------------
        // Parameters() @0x82276870 (`bl` @0x82403F28) seeds the block -- every record's format
        // 0xFFFFFFFF (empty), offset 0xFFFF (auto-pack), usage / usage-index 0xFF (take the default)
        // -- and the console then makes exactly SIX stores into it (asm 0x82403F2C-0x82403F50):
        //   sth r31(0), +0x00      maElements[0].mu16Stream
        //   stw 0x2A23B9, +0x04    maElements[0].miOffset      (the FLOAT3 format lane)
        //   stb r24(1),  +0x0B     maElements[0].mu8UsageIndex (the element-TYPE lane: 1 = XYZ)
        //   sth r31(0), +0x10      maElements[1].mu16Stream
        //   stw 0x2C23A5, +0x14    maElements[1].miOffset      (FLOAT2)
        //   stb r11(6),  +0x1B     maElements[1].mu8UsageIndex (6 = TEX0)
        // (Verify pass 2026-08-15: an earlier revision memset the whole block -- an instruction the
        // asm does not have, which destroyed the ctor's sixteen empty sentinels and made the leaf
        // see sixteen live records with format 0 -- wrote the 6 into maElementFlags[0] instead of
        // element 1's type lane, and dropped the `stb 1` for element 0's type. The boot log showed
        // it: fourteen "unknown vertex format code" lines then `elements=2 stride0=6 decl=FAILED`.)
        renderengine::VertexDescriptor::Parameters lVertexParams;
        lVertexParams.maElements[0].mu16Stream    = 0;
        lVertexParams.maElements[0].miOffset      = static_cast<s32>(KU_VIGNETTE_VTX_ELEMENT0);
        lVertexParams.maElements[0].mu8UsageIndex = 1;   // ELEMENTTYPE_XYZ  (stb r24, var_165)
        lVertexParams.maElements[1].mu16Stream    = 0;
        lVertexParams.maElements[1].miOffset      = static_cast<s32>(KU_VIGNETTE_VTX_ELEMENT1);
        lVertexParams.maElements[1].mu8UsageIndex = 6;   // ELEMENTTYPE_TEX0 (stb r11, var_155)

        rw::BaseResourceDescriptors<5> lVertexDescriptor;
        renderengine::VertexDescriptor::GetResourceDescriptor(&lVertexDescriptor, &lVertexParams);
        m_vertexDescriptorResource = AllocateStateResource(m_allocator, lVertexDescriptor);
        m_vertexDescriptor = renderengine::VertexDescriptor::Initialize(
            reinterpret_cast<rw::Resource*>(&m_vertexDescriptorResource), &lVertexParams);

        // --- the no-depth DepthStencilState ----------------------------------------------------------
        renderengine::DepthStencilState::Parameters lDepthParams;
        std::memset(&lDepthParams, 0, sizeof(lDepthParams));
        lDepthParams.muFunction        = 3;             // v44[0] = 3
        lDepthParams.muState4          = 7;             // v44[4] = 7 (E_FUNCTION_ALWAYS)
        lDepthParams.muState8          = 7;             // v44[8] = 7
        lDepthParams.muStencilReadMask = static_cast<u32>(-1);  // v44[11] = -1
        lDepthParams.muStencilWriteMask= static_cast<u32>(-1);  // v44[12] = -1
        lDepthParams.muState14         = static_cast<u32>(-1);  // v44[14] = -1
        lDepthParams.muState15         = static_cast<u32>(-1);  // v44[15] = -1

        rw::BaseResourceDescriptors<5> lDepthDescriptor;
        renderengine::DepthStencilState::GetResourceDescriptor(
            reinterpret_cast<renderengine::ResourceDescriptor5*>(&lDepthDescriptor), &lDepthParams);
        m_depthStencilResource = AllocateStateResource(m_allocator, lDepthDescriptor);
        m_depthStencilState = renderengine::DepthStencilState::Initialize(
            reinterpret_cast<renderengine::DepthStencilState**>(&m_depthStencilResource), &lDepthParams);

        // --- the no-cull RasterizerState -------------------------------------------------------------
        renderengine::RasterizerState::Parameters lRasterParams;
        std::memset(&lRasterParams, 0, sizeof(lRasterParams));
        lRasterParams.muFillMode               = 0;     // v30[0] = 0
        lRasterParams.muCullMode               = 0;     // v30[1] = 0
        lRasterParams.muMultisampleEnable      = static_cast<u32>(-1);     // v30[4] = -1 (asm stw r26 @0x82404088)
        lRasterParams.muAntialiasedLineEnable  = static_cast<u32>(0xFFFF); // v30[5] = 0xFFFF
        lRasterParams.mu8DepthClipEnable       = 1;     // v32 = 1
        lRasterParams.mu8FrontCounterClockwise = 1;     // v33 = 1
        lRasterParams.mu8PaddingMode           = 1;     // v35 = 1

        rw::BaseResourceDescriptors<5> lRasterDescriptor;
        renderengine::RasterizerState::GetResourceDescriptor(
            reinterpret_cast<renderengine::ResourceDescriptor5*>(&lRasterDescriptor), &lRasterParams);
        m_rasterizerStateResource = AllocateStateResource(m_allocator, lRasterDescriptor);
        m_rasterizerState = renderengine::RasterizerState::Initialize(
            reinterpret_cast<renderengine::RasterizerState**>(&m_rasterizerStateResource), &lRasterParams);

        // --- the vignette vertex + pixel programs and their shader-constant handles --------------------
#if BRN_POSTFX_VIGNETTE_PROGRAMS_AVAILABLE
        // ======== THE CONSOLE ROUTE (X360 0x82404118-0x8240419C), PRESERVED =========================
        // NOTE: this arm DELIBERATELY does not compile as it stands -- the two microcode symbols it
        // names have no declaration anywhere, because inventing one is what the previous revision did.
        // Flipping the gate is therefore a hard error until a real program image is supplied, which is
        // the intent: the gate cannot be turned on by accident.
        // Both CreateProgram calls pass this->m_allocator as the allocator override
        // (`lwz r6, 0xC0(r30)` @0x82404120 / @0x82404158), NOT 0.
        m_vertexProgram = PfxHelper::CreateProgram(0, gauVignetteVertexProgramMicrocode,
                                                   KU_VIGNETTE_VERTEX_PROGRAM_SIZE, m_allocator);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            m_vertexProgram, reinterpret_cast<const u8*>("uvOffset"), &m_uvOffsetHandle);

        m_pixelProgram = PfxHelper::CreateProgram(1, gauVignettePixelProgramMicrocode,
                                                  KU_VIGNETTE_PIXEL_PROGRAM_SIZE, m_allocator);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            m_pixelProgram, reinterpret_cast<const u8*>("controlScalars"), &m_controlScalarsHandle);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            m_pixelProgram, reinterpret_cast<const u8*>("innerColour"), &m_innerColourHandle);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            m_pixelProgram, reinterpret_cast<const u8*>("outerColour"), &m_outerColourHandle);
#else
        // HONESTLY EMPTY: no program pointer, no bound handle. A ProgramVariableHandle default-
        // constructs to register COUNT 0, which every uploader in this tree treats as "not present",
        // so nothing can be bound and nothing can be uploaded through a program that does not exist.
        m_vertexProgram        = nullptr;
        m_pixelProgram         = nullptr;
        m_uvOffsetHandle       = renderengine::ProgramVariableHandle();
        m_controlScalarsHandle = renderengine::ProgramVariableHandle();
        m_innerColourHandle    = renderengine::ProgramVariableHandle();
        m_outerColourHandle    = renderengine::ProgramVariableHandle();

        static bool sbReportedNoVignettePrograms = false;
        if (!sbReportedNoVignettePrograms)
        {
            sbReportedNoVignettePrograms = true;
            CgsDev::Log::WriteToLog(
                "[postfx-vignette] constructed with NO program pair -- the X360 vertex/pixel programs"
                " are Xenos microcode (&unk_82044E88, 288 bytes / &unk_82044FA8, 416 bytes) and no PC"
                " image exists. Both program slots are null and all four constant handles have"
                " register count 0, so the vignette quad cannot be drawn."
                " [FLAG PC bring-up: BRN_POSTFX_VIGNETTE_PROGRAMS_AVAILABLE]\n");
        }
#endif

        // DWARF rwgpfxvignette.h:167 -- never written by the X360 constructor.
        m_textureProgram = nullptr;

        // Seed the shader constants from the supplied state (`addi r4, r22, 0x10` @0x824041A0).
        SetState(lrParameters.m_state);
    }

    // ============================================================================================
    // Vignette::Release  @ 0x823F93D0                     (DWARF rwgpfxvignette.h:125)
    //
    // Seven operations, in the assembly's order, and the order matters only in that it is the order
    // the binary uses:
    //     lwz r3, 0xA8 -> ProgramBuffer::Release            the PIXEL program first
    //     lwz r3, 0xA4 -> ProgramBuffer::Release            then the vertex program
    //     m_allocator->vtbl[0x14](this + 0x90)              the rasterizer state's resource
    //     m_allocator->vtbl[0x14](this + 0x7C)              the depth/stencil state's resource
    //     lwz r3, 0xB8 -> VertexDescriptor::Release         the vertex descriptor object
    //     m_allocator->vtbl[0x14](this + 0x68)              the vertex descriptor's resource
    //     m_allocator->vtbl[0x14](this + 0x54)              the blend state's resource
    // vtbl+0x14 is the established DoFree slot (rwcore_structs.h: IResourceAllocator_vtbl::DoFree).
    //
    // ASYMMETRY THAT IS REAL, NOT A DROPPED CALL: only the VertexDescriptor gets a type-specific
    // Release. Blend / depth-stencil / rasterizer states are plain marshalled word blocks with
    // nothing to tear down, so the console frees their memory and stops. Nothing is set to null --
    // Release is only ever called from BrnPostFx::Destruct @0x824081FC, on an object about to go.
    // ============================================================================================
    void Vignette::Release()
    {
        // [FLAG PC bring-up] The console has no null test here (`lwz r3, 0xA8(r31); bl Release`
        // straight through) because its programs are never null. On this build the program gate
        // (BRN_POSTFX_VIGNETTE_PROGRAMS_AVAILABLE 0) makes them CERTAINLY null, and
        // ProgramBuffer::Release writes through its argument (programbuffer.cpp:311) -- so the honest-
        // empty gate is exactly what makes the console's assumption false. Same guard the sibling
        // DepthOfField::Release carries. (Verify pass 2026-08-15.)
        if (m_pixelProgram != nullptr)
            renderengine::ProgramBuffer::Release(m_pixelProgram);
        if (m_vertexProgram != nullptr)
            renderengine::ProgramBuffer::Release(m_vertexProgram);

        m_allocator->DoFree(m_rasterizerStateResource);
        m_allocator->DoFree(m_depthStencilResource);

        renderengine::VertexDescriptor::Release(
            static_cast<renderengine::VertexDescriptorData*>(m_vertexDescriptor));

        m_allocator->DoFree(m_vertexDescriptorResource);
        m_allocator->DoFree(m_blendStateResource);
    }
}
}
}
