#include "GameSource/Graphics/BrnRendererModule.h"
#include "pc/gcm/renderengine/device.h"   // renderengine::Device frame bracket
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"           // CgsRenderTarget::GetDepthTexture (the s15 bind)
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"  // shadow::Device::SetResource (the global texture binds)
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"                      // renderengine::PCSurfaceBracket_* (the scene-target bracket)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"  // CgsDev::DebugManager (debug HUD overlay)
#include "GameSource/Gui/BrnGuiModule.h"         // BrnGui::gpActiveGuiModule (the GUI render drive)
#include "GameSource/Graphics/BrnRendererModulePostFx.h"  // Render's effects-frame -> BrnPostFx apply block
#include "GameSource/Director/Camera/Camera.h"            // BrnDirector::Camera::Camera -- the staged camera-input record

// ---------------------------------------------------------------------------------------------
// BRN_SHADOW_MAP_TARGET_AVAILABLE -- the shadow-map RENDER TARGET gate (PC bring-up, 2026-08-12).
//
// OPENED 2026-08-12 (the render-target wave). The shadow-map pass and the s15 bind below call
// into BrnShadowMapRenderManager.cpp (Begin/EndRenderShadowMap), BrnRendererMemory.cpp
// (GetShadowMapBuffer) and CgsRenderTarget.cpp (GetDepthTexture). Until this wave none of those
// three could be mounted, because the layer BENEATH them did not exist: the postfx RenderTarget /
// RenderTargetState surface (postfx::gpDefaultRenderTargetState,
// RenderTarget::{Get,Set}SectionRenderTargetState, RenderTarget::Parameters::Parameters(),
// RenderTarget::Initialize, renderengine::Device::SetState(const RenderTargetState*)) was
// declared everywhere and defined nowhere. The console's own version of that layer is EDRAM-based
// and has no PC counterpart, so it is now realised as a Direct3D 9 bring-up leaf:
//     pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp
// which creates a real depth-sampleable INTZ texture (1280x1920, the 1x3 cascade atlas) and binds
// it as the depth-stencil surface. All four TUs are in tools/build/build_game_exe.bat and the
// closure was proved with dumpbin over the linked object set, NOT with the compile gate --
// `cl /c` cannot see unresolved externals and /OPT:REF does NOT excuse them (measured, twice).
//
// ⚠ STILL OUTSTANDING, deliberately and by the conductor's sequencing:
// WorldModule::PublishWorldShadingConstantsBringUp still force-writes a shadows-off c14/c15/c16
// block every frame, so the shadow factor stays pinned at 1.0 and this pass renders into a target
// nothing reads YET. That safety is retired in a separate, separately-verified step: if the s15
// bind silently failed, removing it early would make 92 pixel shaders sample an unbound sampler
// (returns 0 => the key light is REMOVED), which looks far worse than the current flatness.
// Expect no visible shadows on the first boot after this change -- that is the intended two-step.
#define BRN_SHADOW_MAP_TARGET_AVAILABLE 1

// ---------------------------------------------------------------------------------------------
// BRN_ANTIALIAS_BRACKET_AVAILABLE -- the ANTI-ALIASED SCENE-PASS BRACKET gate.
// Opened 2026-08-13 at 0 (bodies landed, compiled out). TURNED ON 2026-08-14.
//
// BeginRenderAntiAliased @0x823FFA18 and ResolveMSAA @0x823FFBE0 are reconstructed further down this
// file (after RenderShadowMapPasses), together with the PC bring-up blit that presents the scene
// target. Render now CALLS them, at the console's own position, so from this build the world passes
// render into the off-screen anti-alias buffer and are blitted back to the swap chain.
//
// ⚠ THIS TEXT WAS NEVER COMPILED BEFORE THIS FLIP. While the macro was 0 the compiler read none of
// it, so the per-TU gate, the faithfulness lint and the reviewer packet all passed on text they had
// never seen. Any ordinary compile error in :911-:1476 is a FIRST sighting, not a regression.
//
// ⚠ THE ONE EXPECTED VISIBLE DIFFERENCE, so it is not mistaken for a defect: THE BACKGROUND COLOUR.
// Before this flip the world drew onto the swap chain, which renderengine::Device::FrameBegin clears
// to BLACK (device.cpp:117, D3DCOLOR_XRGB(0,0,0)). Now it draws onto the anti-alias buffer, whose
// clean-slate colour is mvBackgroundColour -- and with mbGreyBackgroundColour false (Construct's
// default, BrnRendererModule.h) that is whiteLevel * (0.72, 0.83, 0.89), a PALE BLUE. It is the
// console's own clear colour, read off flt_820473AC/A8/A4 (see the constants block below), so the
// change is FAITHFUL, not a bug. Anywhere the world, the sky dome and the 2D tail all fail to cover
// a pixel, that pixel goes from black to pale blue. Treat it as the instrument it is: pale blue
// means "the bracket and the blit work and nothing drew there"; black means the blit did not run.
//
// WHAT EACH OF THE SIX PREVIOUSLY-UNRESOLVED SYMBOLS GOT:
//
//  (a) THE FOUR Xenos entry points -- SATISFIED. renderengine::D3DDevice_BeginTiling /
//      _SetPredication / _Resolve / _EndTiling are declared in
//      pc/gcm/renderengine/Xbox2SurfaceShims.h:92/118/109/100 and are now DEFINED in
//      pc/gcm/renderengine/XenonD3D9Shims.cpp, which is on the exe source list
//      (build_game_exe.bat:321). ⚠ THIS MACRO MUST NOT BE 1 WITHOUT THOSE DEFINITIONS: check with
//      `grep -n "^void D3DDevice_BeginTiling\|^int D3DDevice_Resolve" XenonD3D9Shims.cpp` before
//      trusting a build. On this build only _Resolve actually runs (mbMultisampledBackbuffer is
//      false everywhere, so both bodies below take their UNTILED branch, which calls neither
//      BeginTiling nor EndTiling nor SetPredication); the other three are correct and dead.
//
//  (b) THE TWO GPU perf-monitor bodies -- NOT SATISFIED, AND NOT PRETENDED OTHERWISE. Their eight
//      call sites below, and the header that declares them, are behind BRN_GPU_PERFMON_AVAILABLE
//      (next banner). The console's calls are kept verbatim in the source; they are not renamed,
//      forwarded or deleted.
//
//  (c) THE POOL -- SATISFIED. Render gates both calls on EnsurePostFxSceneTargets() returning true
//      (this file, :348-365) and NOT by a guard inside these bodies, which have no null test
//      because the X360 asm at 0x823FFB08-0x823FFB34 has none. That gate is also what makes
//      GetDownSampleBuffer() safe to dereference: PCBringUpCreatePostFxSceneTargets creates the
//      down-sample buffer BEFORE the anti-alias buffer and refuses to create either at a zero
//      extent (BrnRendererMemory.cpp), and the gate latches on the anti-alias slot, so a true
//      return implies both slots are filled.
//
//  (d) SOMETHING MUST PUT THE SCENE BACK ON THE BACK BUFFER -- SATISFIED by
//      PCBringUpBlitSceneTargetToBackBuffer below, called from Render inside the same gate and
//      AFTER ResolveMSAA (the order is load-bearing; see its own banner). It is NOT the post-fx
//      composite -- BrnPostFx::Render @0x8240A468 is the next wave and retires it.
//
// STILL NOT CALLED, and deliberately: EndRenderAntiAliased @0x82408B00 and BeginQuarterResBuffer
// @0x82408C38 are DECLARATION-ONLY in this tree (BrnRendererModule.h:385/:389 and the banner above
// them), so calling either would be an unresolved external. renderengine::PCSceneBlit_Begin/_End
// stand in for the surface half of EndRenderAntiAliased until it is mounted.
//
// HOW TO REVERT, in one edit: set this macro back to 0. Everything the wiring step added lives
// inside `#if BRN_ANTIALIAS_BRACKET_AVAILABLE`, so at 0 this translation unit preprocesses to
// exactly the code it produced before the flip.
#define BRN_ANTIALIAS_BRACKET_AVAILABLE 1

// ---------------------------------------------------------------------------------------------
// BRN_POSTFX_COMPOSITE_AVAILABLE -- THE REAL POST-FX COMPOSITE (2026-08-14).
//
// At 1, BrnPostFx::Render @0x8240A468 replaces the FLAG PC bring-up blit below at the console's own
// position, and the options-menu brightness/contrast reach the picture (they are the GlobalParams
// shader constant inside BrnPostFxShader::Render, and this function already reads both off the
// dispatch buffer). At 0 -- today -- this file preprocesses to exactly the bring-up blit it did
// before: the include, the constants and the call are all inside the `#if`.
//
// WHY IT IS 1 (2026-08-15): the two preconditions the shipped `#error` named are both met.
//   (1) A PC vertex/pixel program pair exists for permutation 0 -- pc/gcm/renderengine/
//       PostFxProgramsPC.cpp, RECOVERED from the Xenos microcode (tools/assets/shaders/xenos.py,
//       proven against the SHADERS.BNDL / SHADERS_PC.BNDL bundle-pair oracle) and adopted by
//       BrnPostFxShader::Shader::Construct through ProgramBufferPC_Adopt. Only permutation 0 --
//       the one this build's constant block ever selects (effects off, motion blur off).
//   (2) BrnPostFx.cpp is link-closed and mounted: the RenderEngineClub post-fx effect TUs, the
//       bloom passes and the two cached state pointers landed in the gate-flip wave (scratch/
//       postfx_step3_effects/, verified per group).
// What the flip needs on the PC side, all in this wave: gpDefaultRenderTargetState installed from
// Device::Start (the back-buffer target is USE_DEVICE_FOR_WRITE and binds through it), the bloom /
// depth-of-field / back-buffer pool slots created by PCBringUpCreatePostFxSceneTargets (the console
// body samples the first two and composites into the third without a test), and BrnPostFx::Construct
// run once from EnsurePostFxSceneTargets through the seam. THE PICTURE MUST NOT CHANGE at the
// default sliders: permutation 0's neutrals were derived from the recovered math (GlobalParams
// {1,0,0,0} so the white level must be 1; BloomColour 0 collapses the screen blend to the source;
// inner == outer vignette; Tint2d 0), and brightness/contrast at the game's default 50/50 pre-scale to
// exactly 0.0f / 1.0f. The FIRST VISIBLE CHANGE is the options-menu brightness slider taking effect.
//
// REVERT: set the macro back to 0. One character; measured to preprocess back to the pre-composite
// bring-up blit save the mfAspectCorrection initialiser (driver REPORT.md, step 5B).
#define BRN_POSTFX_COMPOSITE_AVAILABLE 1

#if BRN_POSTFX_COMPOSITE_AVAILABLE
// The composite seam. NOT BrnPostFx.h: that header needs the real EA::Jobs::Job and this file's own
// header still defines a placeholder one (BrnRendererModule.h:21-31) -- including both is C2011.
// The seam header explains the whole constraint and names what retires it.
#include "GameSource/Graphics/PostFx/BrnPostFxPCComposite.h"

namespace
{
    // The options-menu calibration sliders reach the composite pre-scaled, and BOTH constants are
    // RECOVERED off the X360 call site (asm 0x8240DCF8 `fmsubs f28, f0, f30, f29` and 0x8240DD40
    // `fmadds f30, f0, f30, f29`, with f30 = flt_82002138 and f29 = flt_82001DA0):
    //     brightness -> setting * 0.01f - 0.5f          contrast -> setting * 0.01f + 0.5f
    //   * flt_82001DA0 == 0.5f is dumped (scratch/postfx_wave1b_dossiers/DATA_DUMP.md:1552).
    //   * flt_82002138 == 0.01f is read off BrnDirector::Camera::Utils::Looker::Parameters::Construct
    //     @0x821F8D80, where `lfs f11, flt_82002138@l(r10)` @0x821F8D8C feeds `stfs f11, 0x28(r3)`
    //     @0x821F8D94 and Hex-Rays prints that store as `*(result + 40) = 0.0099999998;`.
    const f32 KF_CALIBRATION_SLIDER_SCALE = 0.01f;   // flt_82002138
    const f32 KF_CALIBRATION_SLIDER_BIAS  = 0.5f;    // flt_82001DA0

    // ⚠ THE NO-DISPATCH-BUFFER FALLBACK IS A PC CHOICE, AND IT IS DERIVED RATHER THAN PICKED.
    // The console has no such path: it reads GetBrightness/GetContrast off the buffer unconditionally
    // inside the gate at 0x8240DC7C. On PC Render is entered with a null buffer on the frames before
    // the dispatch ring comes up (see the `lpDispatchThreadInputBuffer != 0` test at the top of
    // Render), so a value is needed. It is the game's OWN default slider position, run through the
    // formula above by the compiler rather than by hand: BrnGui::KI_DEFAULT_BRIGHTNESS and
    // KI_DEFAULT_CONTRAST are both 50 (BrnGuiOptionsDataProfile.h:29/:35, applied at
    // BrnGuiOptionsDataProfile.cpp:57-58). That yields exactly 0.0f and 1.0f -- the neutral pair --
    // but the derivation is the point: if the scale or the bias is ever corrected, the fallback moves
    // with them instead of silently disagreeing. Spelled locally rather than by including
    // BrnGuiOptionsDataProfile.h, which would drag the whole options/profile slice into the renderer.
    const s32 KI_DEFAULT_CALIBRATION_SETTING = 50;
}
#endif  // BRN_POSTFX_COMPOSITE_AVAILABLE

// ---------------------------------------------------------------------------------------------
// BRN_GPU_PERFMON_AVAILABLE -- the GPU perf-monitor sub-gate (2026-08-14).
//
// CgsDev::PerfMonGpu::StartMonitor / StopMonitor are the last two of the six symbols the bracket
// bodies reference, and they are the two the bracket flip could NOT satisfy. They are DECLARED in
// GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h:47-48 and the only definitions in
// the tree are in .../PerfMon/Gpu/PS3/CgsPerfMonGpuPS3.cpp:165 and :183. THREE separate reasons this
// is not a one-step fix, all measured rather than assumed:
//
//   1. NEITHER Gpu TU IS ON tools/build/build_game_exe.bat. `grep -n -i "PerfMon"` over that file
//      returns lines 1267/1268 (rem comments), 1800/1801 -- the two **Cpu** TUs -- and 2934/2936
//      (BrnGuiPerfmons). There is no Gpu line. So this is a real LNK2019, not a stale banner.
//   2. THE TWO FILES ARE A PAIR. CgsPerfMonGpuPS3.cpp holds the bodies; CgsPerfMonGpu.cpp:15-20
//      holds the six static data members (maMonitors, miMaxMonitor, meGameFrequency,
//      mbProfilingRunning, mbActiveMonitor, miActiveMonitorID). Mounting either alone trades two
//      unresolved externals for six.
//   3. MOUNTING BOTH FIRES FOUR ASSERTS A FRAME. StartMonitor asserts `mbProfilingRunning == true`
//      (CgsPerfMonGpuPS3.cpp:175) and StopMonitor the same (:191), and nothing in this tree calls
//      PerfMonGpu::StartProfiling / Construct / Swap. Worse, EVERY id in mGpuMonitors is 0
//      (BrnGpuMonitors::Construct, BrnRendererModule.h:302-307, memsets the whole struct to 0 and
//      nothing calls PerfMonGpu::AddMonitor anywhere), so miScreenClear == miDownsampleMSAAAndComp-
//      Particles == 0 and the frame's SECOND StartMonitor would also trip
//      `!gValues.mabUsedThisFrame[0]` (:173). Mounting the pair is its own step, and it needs
//      Construct + StartProfiling + AddMonitor wired first.
//
// THE PRECEDENT FOR DROPPING GPU PERFMON INSTRUMENTATION IS THIS VERY FUNCTION. Render @0x8240BFA8
// brackets its own frame with CgsDev__PerfMonGpu__StartProfiling (@0x8240C4C8) and about twenty
// Start/StopMonitor pairs; the reconstructed PC Render below carries NONE of them. The eight calls
// inside the two bracket bodies survived only because the bodies were compiled out. This gate makes
// them consistent with the rest of the file WITHOUT deleting the console's calls: the text stays,
// greppable and in the console's order, including the monitor-order flip between
// BeginRenderAntiAliased's two branches that the comments there explain.
//
// TO TURN THIS ON: mount CgsPerfMonGpu.cpp and PerfMon/Gpu/PS3/CgsPerfMonGpuPS3.cpp on
// build_game_exe.bat, wire PerfMonGpu::Construct + AddMonitor (so the ids stop all being 0) and
// StartProfiling/StopProfiling/Swap into the frame, then set this to 1. Profiling only -- it moves
// no pixels either way.
#define BRN_GPU_PERFMON_AVAILABLE 0

// Minimal constructor for the off-path job placeholder embedded in BrnRendererModule
// (Option B). The job system is reconstructed with the threading core; on the
// single-threaded boot it carries no behaviour, so this definition keeps the link
// closed without faking functionality. (BufferedDispatchFrame is the REAL type now --
// its stub ctor is gone with the world-pass mount.)
EA::Jobs::Job::Job(s32 /*liPriority*/) {}
#include "GameSource/Gui/BrnGuiMovieManager.h"   // BrnGui::gpActiveMovieManager (the PC presentation draw)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [diag] BRN_IM2D_TRACE probes
#include "rw/rwcore_structs.h"                   // rw::LinearResourceAllocator (world dispatch bin memory)
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"  // ShaderConstantTable::BeginFrame (StartOfFrame)
#include <Windows.h>   // [diag] GetEnvironmentVariableA
#include <cstdio>      // [diag] snprintf
#include <cstring>     // memcpy (per-pass DispatchObjectContext copies)
#include <new>         // world dispatch bring-up heap

// [diag] present counter (device.cpp) - stamps the trace lines with their frame.
namespace renderengine { extern u32 guPresentCount; }

// High-res frame timer (CgsTimeUtils.cpp), forward-declared - drives the thread-monitor health.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

// The engine-global shader-constant table (bodied by the CgsShaderConstants TU); the X360
// StartOfFrame @0x823FC160 opens its frame on the GDL write bin.
namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

namespace
{
    // The clear colour BrnRendererModule::BeginQuarterResBuffer @0x82408C38 clears every colour
    // target to before the quarter-resolution particle pass. The console loads ONE float and stores
    // it four times -- `lfs f0, flt_82001CC0@l(r11)` @0x82408CC4, then `stfs` @0x82408CCC /
    // 0x82408CD0 / 0x82408CD4 / 0x82408CD8 -- so the four components are one value, which is why
    // this is one constant.
    // ATTESTED, not defaulted: scratch/postfx_wave1_dossiers/DATA_DUMP.md dumps flt_82001CC0 as
    // +0x0000 = 0x00000000 = 0.0f, and only +0x0000 belongs to the symbol (that block runs on into
    // the string "Monitor " at +0x0008, and the assembly addresses no other displacement).
    const f32 KF_QUARTER_RES_CLEAR_COMPONENT = 0.0f;

    // --- [FLAG PC bring-up] the LAYER-0 base effects frame's shipped values -----------------------
    // Every one of these is READ DATA, not a chosen number: the bloom five come from POSTFX vault
    // asset "191270" (key C37BF4F3458A6DB5, class B632EC178CFDE613 "bloomasset", data at bin+0x1270
    // in build/game/POSTFX/POSTFXVAULT.BIN, resource 0x627894D7) as BloomData::Construct @0x82678070
    // reads it -- mfLuminance = data+20, mfThreshold = data+16, mv4Scale = data+0..16. Conductor
    // extraction, scratch/postfx_step4_bloom/DATA_NOTE.md section 3. Consumed by
    // PCBringUpProduceBaseEffectsFrame below.
    const f32 KF_BASE_FRAME_BLOOM_LUMINANCE = 1.8f;
    const f32 KF_BASE_FRAME_BLOOM_THRESHOLD = 0.655738f;
    const f32 KF_BASE_FRAME_BLOOM_SCALE_R   = 0.954116f;
    const f32 KF_BASE_FRAME_BLOOM_SCALE_G   = 0.947919f;
    const f32 KF_BASE_FRAME_BLOOM_SCALE_B   = 0.886839f;
    const f32 KF_BASE_FRAME_BLOOM_SCALE_A   = 1.0f;

    // The weight the console's producer writes for an effect it enabled (`frame+8 = 1.0` etc.).
    const f32 KF_BASE_FRAME_ENABLED_WEIGHT  = 1.0f;

    // --- [FLAG PC bring-up] the EffectsDebugComponent flags the console's producer gates on ------
    // The base-frame producer reads six enable flags and four motion-blur "user settings" fields
    // off BrnEffects::EffectsDebugComponent mDebugComponent (DWARF EffectsModule.h:580), embedded
    // in BrnEffects::EffectsModule at module +181040. Names come from the DWARF
    // (BrnEffectsDebugComponent.h:215-224, in this exact order at module +181100..+181116):
    //   +181100 mbBloom  +181101 mbVignette  +181102 mbDepthOfField  +181103 mbTint
    //   +181104 mbTint2d +181105 mbMotionBlur +181106 mbMotionBlurEnableUserSettings
    //   +181108 mfMotionBlurUserAmountCars   +181112 mfMotionBlurUserAmountWorld
    //   +181116 mbMotionBlurUserHighQuality
    // and the VALUES are BrnEffects::EffectsDebugComponent::Construct @0x82278C98, verbatim from
    // the pseudocode (it names them itself):
    //   mbEnableBloom = 1;  mbEnableVignette = 1;  mbEnableDOF = 1;  mbEnableTint = 1;
    //   mbEnable2dTint = 1; mbEnableMotionBlur = 1;
    //   mbEnableMotionBlurUserSettings = 0;   mbEnableMotionBlurUserHighQuality = 1;
    //   mbEnableMotionBlurUserCars = 0.0;     mbEnableMotionBlurUserWorld = 1.0;
    // (the last two are Hex-Rays mis-typing the DWARF's two f32 user AMOUNTS as bools -- the
    //  0.0/1.0 initialisers give them away, and the DWARF names them mfMotionBlurUserAmountCars /
    //  mfMotionBlurUserAmountWorld). The module does not exist on this build, so the producer reads
    //  the Construct defaults, which is what a console frame with the debug UI untouched reads too.
    //  The FOUR always-on ones (bloom/vignette/tint/tint2d) were already spelled as literal `true`
    //  in the producer before this wave and are left that way.
    const bool KB_DEBUG_ENABLE_DOF                     = true;    // mbDepthOfField
    const bool KB_DEBUG_ENABLE_MOTION_BLUR             = true;    // mbMotionBlur
    const bool KB_DEBUG_ENABLE_MOTION_BLUR_USER_SETTINGS = false; // mbMotionBlurEnableUserSettings
    const bool KB_DEBUG_MOTION_BLUR_USER_HIGH_QUALITY  = true;    // mbMotionBlurUserHighQuality
    const f32  KF_DEBUG_MOTION_BLUR_USER_AMOUNT_CARS   = 0.0f;    // mfMotionBlurUserAmountCars
    const f32  KF_DEBUG_MOTION_BLUR_USER_AMOUNT_WORLD  = 1.0f;    // mfMotionBlurUserAmountWorld

    // --- [FLAG PC bring-up] the B4-BLUR block: POSTFX vault asset "218901" ----------------------
    // Read data, not chosen numbers, exactly like the bloom five above. The console does
    // `BlurData::Construct(v51, hash64("218901"))` and memcpy's the 96-byte result into the frame.
    // BlurData::Construct @0x826781C8 is nine loads, and the asm pins each one:
    //   lvx data+0x30 -> blur+0x20 mv2BlendAmount     lfs data+0x48 -> blur+0x00 mfOpacity
    //   lvx data+0x10 -> blur+0x30 mv2BlurAmount      lfs data+0x40 -> blur+0x04 mfVelocity
    //   lvx data+0x20 -> blur+0x40 mv2BlendCentre     lfs data+0x44 -> blur+0x08 mfSharpness
    //   lvx data+0x00 -> blur+0x50 mv2BlurCentre      lfs data+0x4C -> blur+0x0C mfNoise
    //                                                 lfs data+0x50 -> blur+0x10 mfAngle
    // The 112 decoded bytes of asset 218901 (b4blurasset, class EF9F6F047362D8CF, POSTFXVAULT.BIN
    // bin+0x1290; conductor extraction, scratch/postfx_step6_producers/WAVE_NOTE.md):
    //   +0x00 (0.5, 0.5, 0, 0)   +0x10 (1, 1, 0, 0)   +0x20 (0.5, 0, 0, 0)
    //   +0x30 (1.3, 0.555, 0, 0) +0x40 (2.622951, 0.327869, 0.666, 0.007049)  +0x50 (0, 0, 0, 0)
    // (the +0x60 row is past everything Construct reads and is therefore not modelled).
    const f32 KF_BASE_FRAME_BLUR_OPACITY        = 0.666f;      // data +0x48
    const f32 KF_BASE_FRAME_BLUR_VELOCITY       = 2.622951f;   // data +0x40
    const f32 KF_BASE_FRAME_BLUR_SHARPNESS      = 0.327869f;   // data +0x44
    const f32 KF_BASE_FRAME_BLUR_NOISE          = 0.007049f;   // data +0x4C
    const f32 KF_BASE_FRAME_BLUR_ANGLE          = 0.0f;        // data +0x50
    const f32 KF_BASE_FRAME_BLUR_BLEND_AMOUNT_X = 1.3f;        // data +0x30 lane 0
    const f32 KF_BASE_FRAME_BLUR_BLEND_AMOUNT_Y = 0.555f;      // data +0x30 lane 1
    const f32 KF_BASE_FRAME_BLUR_BLUR_AMOUNT_X  = 1.0f;        // data +0x10 lane 0
    const f32 KF_BASE_FRAME_BLUR_BLUR_AMOUNT_Y  = 1.0f;        // data +0x10 lane 1
    const f32 KF_BASE_FRAME_BLUR_BLEND_CENTRE_X = 0.5f;        // data +0x20 lane 0
    const f32 KF_BASE_FRAME_BLUR_BLEND_CENTRE_Y = 0.0f;        // data +0x20 lane 1
    const f32 KF_BASE_FRAME_BLUR_BLUR_CENTRE_X  = 0.5f;        // data +0x00 lane 0
    const f32 KF_BASE_FRAME_BLUR_BLUR_CENTRE_Y  = 0.5f;        // data +0x00 lane 1

    // The camera-state flag index the effects module reads as "this is the racing gameplay camera".
    // Derivation (see the producer's own note): EffectsModule::Update @0x8229EC28 stores
    // `(*(CameraInput + 81) & 8) != 0` into the cache's mbIsGameCamera, CameraInput is typed
    // `_DWORD*` there so +81 words == camera +0x144, the CameraState sub-object is at camera +0x138
    // and its mCurrentFlags BitArray<30> at state +0x08 == camera +0x140, and BitArray masks with
    // `(u64)1 << index` -- so on the big-endian console camera +0x144 is that qword's low half and
    // mask 8 is bit index 3.
    const u32 KU_CAMERA_STATE_FLAG_IS_RACING_GAMEPLAY = 3u;

    // --- [FLAG PC bring-up] the staged CAMERA INPUT RECORD ---------------------------------------
    // STANDS IN FOR BrnEffects::EffectsIO::DispatchInputBuffer::mCameraInput (DWARF
    // EffectsModuleIO.h:261, `Camera mCameraInput`, X360 buffer +0x50): a BY-VALUE copy of the
    // director's published camera, written once per dispatch by SetCameraInput @0x823C9988 and read
    // by GenerateRenderRequests @0x8227FF10. None of the EffectsIO buffers is created on this
    // build, so the copy lives here and BrnRendererModule::PCBringUpSetCameraInput writes it.
    // A COPY, not a pointer, because the console copies too: DoDispatch holds the director output
    // buffer's read lock only across its own call, while the producer runs at StartOfFrame.
    // NOT A SPLIT BRAIN, and the check was made rather than assumed: the owning TYPE *is*
    // reconstructed (GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.{h,cpp},
    // with the real SetCameraInput/GetCameraInput bodies and `BrnDirector::Camera::Camera
    // mCameraInput` as a member) -- but that TU is not on the build list and no instance of it is
    // ever created:
    //   $ grep -c "BrnEffectsModuleIO" tools/build/build_game_exe.bat
    //   0
    //   $ grep -rn "EffectsIO::DispatchInputBuffer" b5-decomp/src --include=*.cpp \
    //         | grep -v SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.cpp
    //   ...BrnEffectsModuleIO_DispatchInputBuffer_IOHelper.cpp:25: (the IOHelper ctor only)
    // so there is no live storage to share. The signature of PCBringUpSetCameraInput is
    // DELIBERATELY the buffer's own (`const BrnDirector::Camera::Camera*`), so when the EffectsIO
    // set is created the swap to the real SetCameraInput/GetCameraInput is one line at each end.
    // Routing through the real buffer TODAY was considered and rejected for this wave: its
    // accessors assert on IOBuffer read/write locks, and adding lock discipline to the renderer's
    // StartOfFrame is exactly the kind of change that costs the wave its "0 asserts" gate.
    // LAZY Construct: until DoDispatch stages a camera the record must hold the DIRECTOR'S OWN
    // defaults, which is exactly BrnDirector::Camera::Camera::Construct @0x82255E68 -- the same
    // thing the console's buffer would hold before the first SetCameraInput. Doing it on first use
    // keeps BrnRendererModule::Construct untouched.
    // DELETE-WHEN the EffectsIO dispatch buffer set is real on PC.
    BrnDirector::Camera::Camera gPCBringUpCameraInput;
    bool                        gbPCBringUpCameraInputConstructed = false;
    bool                        gbPCBringUpCameraInputStaged      = false;

    BrnDirector::Camera::Camera& PCBringUpGetCameraInput()
    {
        if (!gbPCBringUpCameraInputConstructed)
        {
            gbPCBringUpCameraInputConstructed = true;
            gPCBringUpCameraInput.Construct();
        }
        return gPCBringUpCameraInput;
    }

    // --- [FLAG PC bring-up diagnostic] the line that proves the camera-side producers -------------
    // CHANGE-LATCHED, not purely periodic: these are event-driven producers (a director state has
    // to request a blur), so a fixed sample would almost certainly miss the transition. Emits when
    // any of the five bools flips, and every 900th call besides so a long steady state still shows
    // its live values; capped at 24 lines so a flapping state cannot flood BrnGame.log.
    // DELETE with the bring-up.
    void PCBringUpLogBaseEffectsFrameCameraState(const BrnEffectsFrame& lrFrame,
                                                 const BrnDirector::Camera::Camera& lrCamera)
    {
        static u32 suCalls         = 0u;
        static u32 suPrints        = 0u;
        static u32 suLastSignature = 0xFFFFFFFFu;

        const u32 luCall = suCalls++;
        const BrnDirector::Camera::MotionBlurData& lrBlur = lrFrame.GetMotionBlurData();

        const u32 luSignature =
              (lrFrame.GetUseDepthOfField()          ?  1u : 0u)
            | (lrFrame.GetUseBlur()                  ?  2u : 0u)
            | (lrBlur.IsActive()                     ?  4u : 0u)
            | (lrBlur.IsExpensiveMotionBlur()        ?  8u : 0u)
            | (lrFrame.GetIsRacingGameplayCamera()   ? 16u : 0u)
            | (gbPCBringUpCameraInputStaged          ? 32u : 0u);
        const bool lbChanged = (luSignature != suLastSignature);
        suLastSignature = luSignature;

        if ((!lbChanged && (luCall % 900u) != 0u) || suPrints >= 24u)
            return;
        if (CgsDev::Log::gpDebugPrint == 0)
            return;
        ++suPrints;

        *CgsDev::Log::gpDebugPrint
            << "[postfx-cam] produce " << static_cast<s32>(luCall)
            << ": staged=" << (gbPCBringUpCameraInputStaged ? 1 : 0)
            << " dof=" << (lrFrame.GetUseDepthOfField() ? 1 : 0)
            << " amount=" << lrCamera.GetDepthOfField().GetBlurriness()
            << " b4blur=" << (lrFrame.GetUseBlur() ? 1 : 0)
            << " mb=" << (lrBlur.IsActive() ? 1 : 0)
            << " hq=" << (lrBlur.IsExpensiveMotionBlur() ? 1 : 0)
            << " cars=" << lrBlur.GetCarsBlendAmount()
            << " world=" << lrBlur.GetWorldBlendAmount()
            << " speed=" << lrFrame.GetSpeedMPH()
            << " gamecam=" << (lrFrame.GetIsRacingGameplayCamera() ? 1 : 0)
            << "\n";
    }

    u32  gu32LastMonitorTick = 0;
    bool gbMonitorTickValid  = false;

    // [PC presentation leaf] the movie screen-ownership linger (see the movie block in
    // Render): tick of the last frame the MovieManager's presentation cycle was active.
    u32  gu32LastMoviePresentTick = 0;
    bool gbMoviePresentTickValid  = false;

    // Submit one solid-coloured quad (4-vertex triangle strip) through the Im2d, in 1280x720 logical px.
    void EmitColouredQuad(CgsGraphics::Im2d* lpIm2d, f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, CgsGraphics::RGBA8 lColour)
    {
        CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
        const f32 laPos[4][2] = { {lfX0, lfY0}, {lfX1, lfY0}, {lfX0, lfY1}, {lfX1, lfY1} };   // TL,TR,BL,BR
        for (s32 liVertex = 0; liVertex < 4; ++liVertex)
        {
            laVerts[liVertex].mv2Pos    = { laPos[liVertex][0], laPos[liVertex][1] };
            laVerts[liVertex].mv2Tex0UV = { 0.0f, 0.0f };
            laVerts[liVertex].mv4Colour = lColour;
        }
        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
    }
}

// @ 0x82405A30 - BrnRendererModule::RenderThreeThreadMonitors. Three squares bottom-centre, one per
// worker thread: green when the thread is running in real time, red when it has fallen behind. The X360
// draws them via the untextured Basic2dColouredVertex renderer at normalised coords (x 0.55/0.57/0.59,
// y 0.91-0.94); reconstructed through mIm2dRenderer untextured (SetTexture(null) -> solid colour), with
// the normalised coords scaled to the 1280x720 logical space.
void BrnRendererModule::RenderThreeThreadMonitors(bool lbThread0, bool lbThread1, bool lbThread2)
{
    const f32 KF_W = 1280.0f;
    const f32 KF_H = 720.0f;
    const CgsGraphics::RGBA8 KC_GREEN = { 0, 255, 0, 255 };
    const CgsGraphics::RGBA8 KC_RED   = { 255, 0, 0, 255 };

    const f32  laLeftX[3]      = { 0.55f, 0.57f, 0.59f };   // normalised left edge; width 0.015
    const bool labThreadOk[3]  = { lbThread0, lbThread1, lbThread2 };

    mIm2dRenderer.BeginRendering();
    mIm2dRenderer.SetState(static_cast<const CgsGraphics::BlendState*>(nullptr));
    mIm2dRenderer.SetTexture(nullptr);   // untextured -> solid vertex colour
    for (s32 liThread = 0; liThread < 3; ++liThread)
    {
        const CgsGraphics::RGBA8 lColour = labThreadOk[liThread] ? KC_GREEN : KC_RED;
        EmitColouredQuad(&mIm2dRenderer,
                         laLeftX[liThread] * KF_W,           0.91f * KF_H,
                         (laLeftX[liThread] + 0.015f) * KF_W, 0.94f * KF_H, lColour);
    }
    mIm2dRenderer.EndRendering();
}

// @ 0x82406410 - BrnRendererModule::RenderLetterBoxBars. Draw the two solid-black bars that frame a
// widescreen (letterboxed) view - one across the top, one across the bottom. lfDestAspectRatio is the
// visible/kept vertical fraction of the screen; the cropped-away remainder (1 - lfDestAspectRatio) is
// split evenly between the two bars, so each bar is (1 - lfDestAspectRatio) * 0.5 of the height and
// spans the full width. The X360 draws them through the immediate-mode 2D renderer in normalised
// [0,1] screen space: BeginRendering -> SetTransform(cached screen transform) -> Render(top bar) ->
// Render(bottom bar) -> EndRendering, with each quad's four vertices coloured from a const RGBA black
// (DWARF locals lLetterboxY / lBlack / lTransform). Each quad is a 4-vertex triangle strip (prim 6).
void BrnRendererModule::RenderLetterBoxBars(CgsGraphics::Im2d& lIm2d, f32 lfDestAspectRatio)
{
    using namespace CgsGraphics;

    const RGBA8 KC_BLACK = { 0, 0, 0, 255 };
    const f32   lfLetterboxY = (1.0f - lfDestAspectRatio) * 0.5f;   // height of each bar (top + bottom)

    lIm2d.BeginRendering();

    // X360 SetTransform of the renderer's cached [0,1]->screen transform (module static @0x830112D0).
    // The exact matrix bytes are not recovered from the ARTIST rodata, so the default-constructed
    // Im2dTransform stands in for that cached screen transform here.
    Im2dTransform lTransform;
    lIm2d.SetTransform(lTransform);

    Basic2dColouredTexturedVertex laVerts[4];
    for (s32 liVertex = 0; liVertex < 4; ++liVertex)
    {
        laVerts[liVertex].mv4Colour  = KC_BLACK;
        laVerts[liVertex].mv2Tex0UV  = { 0.0f, 0.0f };
    }

    // Top bar: full width (x 0..1), y in [0, lfLetterboxY]. Triangle-strip order TL, BL, TR, BR.
    laVerts[0].mv2Pos = { 0.0f, 0.0f };
    laVerts[1].mv2Pos = { 0.0f, lfLetterboxY };
    laVerts[2].mv2Pos = { 1.0f, 0.0f };
    laVerts[3].mv2Pos = { 1.0f, lfLetterboxY };
    lIm2d.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);

    // Bottom bar: full width, y in [1 - lfLetterboxY, 1].
    laVerts[0].mv2Pos = { 0.0f, 1.0f - lfLetterboxY };
    laVerts[1].mv2Pos = { 0.0f, 1.0f };
    laVerts[2].mv2Pos = { 1.0f, 1.0f - lfLetterboxY };
    laVerts[3].mv2Pos = { 1.0f, 1.0f };
    lIm2d.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);

    lIm2d.EndRendering();
}

// @ 0x8240A778 - BrnRendererModule::Construct. Reconstructed from the X360 ARTIST build.
//
// Option B (layout-faithful incremental): the loading-screen render path is reconstructed
// for real here - the double-buffered shader-constant frames and the loading-screen
// renderer, which is what actually draws during boot. The remaining subsystems the full
// Construct builds (effects arbitrator, dispatch frames, the Im2d/Im3d family, render-
// target memory, corona/postfx/occlusion/shadow/sun managers) are held as opaque storage
// and their construction is reconstructed incrementally; none of them draws during the
// loading screen, so the screen boots through the real module without them.
namespace
{
    // [PC bring-up] The dispatch bins' backing memory. The X360 carves them from
    // the renderer's graphics allocator (BrnRendererMemory::Construct ->
    // mpGraphicsAllocator, this+14668) whose reconstruction is still open; until
    // it lands, a renderer-owned rw::LinearResourceAllocator over one heap block
    // supplies DoAllocate with identical semantics.
    rw::LinearResourceAllocator sWorldDispatchAllocator;
    bool                        sbWorldDispatchAllocatorReady = false;

    // The X360 render-frame bin size is the rodata global dword_82F24238, which
    // the function-only exports leave UNVALUED; this PC sizing is a documented
    // choice (world-city frames: object commands + expanded mesh commands +
    // constant scratch + sort arrays all live in the frame bin).
    const u32 KU_PC_DISPATCH_BIN_BYTES     = 12u * 1024u * 1024u;
    const u32 KU_PC_GDL_DISPATCH_BIN_BYTES = 8u * 1024u * 1024u;
    const u32 KU_NUM_DISPATCH_LISTS        = 25u;   // X360 Construct: GetList ids 0..24

    bool EnsureWorldDispatchAllocator()
    {
        if (sbWorldDispatchAllocatorReady)
            return true;

        const u32 luHeapBytes = KU_PC_DISPATCH_BIN_BYTES
                              + 2u * KU_PC_GDL_DISPATCH_BIN_BYTES
                              + (3u * 4096u)   // per-bin align128(size)+128 slop + headroom
                              + (64u * 1024u); // + the small renderengine objects that share
                                               //   this allocator (the sky dome's four buffer
                                               //   headers); without it their DoAllocate came
                                               //   back empty and tripped CreateGeometry's
                                               //   GetMemoryResource asserts
        void* lpHeap = ::operator new(luHeapBytes, std::nothrow);
        if (lpHeap == 0)
            return false;

        rw::Resource lHeapResource;
        rw::ResourceDescriptor lHeapCapacity;
        for (u32 luLane = 0; luLane < 4; ++luLane)
        {
            lHeapResource.m_baseResources[luLane] = (luLane == 0) ? lpHeap : 0;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_size      = (luLane == 0) ? luHeapBytes : 0u;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_alignment = (luLane == 0) ? 128u : 1u;
        }
        sWorldDispatchAllocator.Initialize(lHeapResource, lHeapCapacity);
        sbWorldDispatchAllocatorReady = true;
        return true;
    }

    // ============================================================================================
    // [FLAG PC bring-up] CONSTRUCT THE EFFECTS ARBITRATOR.
    //
    // CONSOLE POSITION: BrnRendererModule::Construct @0x8240A778, pseudocode line 126 --
    //     BrnGraphics::EffectsArbitrator::Construct(this + 1152, off_82F2C814)
    // (this+1152 == this+0x480 == mEffectsArbitrator). off_82F2C814 is a GLOBAL OBJECT whose vtable
    // is off_820A09F0, i.e. an rw::IResourceAllocator instance -- the process-wide GlobalGraphics
    // BrnResource::LinearResourceAllocator (BrnResourceAllocator.h:86). That object is
    // DECLARATION-ONLY in this tree: BrnResource::Allocators::GetGlobalGraphicsAllocator() has no
    // body, which is the same blocker BrnRendererMemory::Construct's own gate banner names.
    //
    // WHY IT MOVED: BrnRendererModule::Construct runs before that allocator exists on PC, exactly as
    // it runs before the D3D9 device exists. So the arbitrator is Constructed lazily, once, on the
    // SAME bring-up allocator every other deferred console Construct in this file already runs
    // through (sWorldDispatchAllocator -- the three state factories, mIm3dRendererSkyDome,
    // BrnPostFx::Construct). It needs no device, so unlike those it can come up on the very first
    // call, which is what lets the layer-0 producer write a frame before the first Render.
    //
    // ORDERING CONTRACT, and it is the whole reason this is a function rather than a line in
    // Construct: the arbitrator must exist BEFORE (a) the first producer write
    // (PCBringUpProduceBaseEffectsFrame, from StartOfFrame), (b) the first world hand-over
    // (GetWorldEffectsFrameBringUp, from BrnGameModule::DoDispatch) and (c) the first read
    // (Render's apply block). All three call this first, so whichever runs first builds it.
    //
    // DELETE-WHEN GetGlobalGraphicsAllocator() has a body and Construct can make the console's own
    // one-line call.
    // ============================================================================================
    bool sbEffectsArbitratorConstructed = false;

    bool EnsureEffectsArbitratorBringUp(BrnGraphics::EffectsArbitrator& lrArbitrator)
    {
        if (sbEffectsArbitratorConstructed)
            return true;
        if (!EnsureWorldDispatchAllocator())
            return false;              // no heap yet -- retry next call

        lrArbitrator.Construct(&sWorldDispatchAllocator);
        sbEffectsArbitratorConstructed = true;
        CgsDev::Log::WriteToLog("[postfx-fx] BrnGraphics::EffectsArbitrator Constructed"
                                " (deferred PC bring-up, X360 Construct @0x8240A778 line 126)\n");
        return true;
    }

    // [PC bring-up] The shadow-map render target, created on the first frame that HAS a device.
    //
    // Value-latched on the created target, not on a `static bool tried` -- a one-shot flag set
    // during the loading screen (before renderengine::gDevice exists) would burn the single
    // attempt and the target would never be built. The pointer below only becomes non-null once
    // a real CgsRenderTarget is in pool slot 1.
    CgsRenderTarget* gpShadowMapTarget = nullptr;

    bool EnsureShadowMapTarget(BrnRendererMemory& lrRendererMemory)
    {
        if (gpShadowMapTarget != nullptr)
            return true;
        if (renderengine::gDevice == 0)
            return false;              // no device yet -- retry next frame
        if (!EnsureWorldDispatchAllocator())
            return false;

        lrRendererMemory.PCBringUpCreateShadowMapBufferOnly(&sWorldDispatchAllocator);
        gpShadowMapTarget = lrRendererMemory.GetShadowMapBuffer(0);
        return gpShadowMapTarget != nullptr;
    }

    // [PC bring-up] The POST-FX SCENE TARGETS, created the same lazy way and for the same reason.
    //
    // WHY THIS IS NOT JUST `Construct()`. BrnRendererMemory::Construct @0x823FCA38 is the console's one
    // entry point into the pool and it is now much closer to linkable -- the post-fx spine wave bodied
    // the eight sibling Create*Buffer helpers, which were the bulk of the fourteen unresolved externals
    // in its BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE banner. Two blockers remain, and neither is
    // honestly closeable yet:
    //   * BrnResource::Allocators::GetGlobalGraphicsAllocator() is still declaration-only,
    //   * the four gacIm2d{Depth,Composite}Blit{Vertex,Pixel}Program blobs are XENOS MICROCODE. Their
    //     bytes ARE recoverable (they are at unk_8203DAA8 / DC00 / DCF0 / DE80 and this wave dumped
    //     them), but linking Xenos microcode into a D3D9 build would satisfy the linker with data the
    //     GPU cannot execute -- a green build that draws garbage. They wait for a PC program leaf.
    // So the gate stays at 0 and the spine goes through this bring-up entry point, exactly as the
    // shadow slice does. DELETE BOTH once those two land and Construct() can be called.
    //
    // ⚠️ ORDER IS LOAD-BEARING: PCBringUpCreateShadowMapBufferOnly NULLS EVERY POOL SLOT before it
    // fills the shadow one, so it must run FIRST. EnsureShadowMapTarget is called before this on every
    // path below, and this function additionally refuses to run until that target exists.
    CgsRenderTarget* gpAntiAliasTarget = nullptr;

    bool EnsurePostFxSceneTargets(BrnRendererMemory& lrRendererMemory)
    {
        if (gpAntiAliasTarget != nullptr)
            return true;
        if (renderengine::gDevice == 0)
            return false;              // no device yet -- retry next frame
        if (gpShadowMapTarget == nullptr)
            return false;              // the slot-nulling pass has not run yet (see above)
        if (!EnsureWorldDispatchAllocator())
            return false;

        // The eight Create*Buffer helpers are private to BrnRendererMemory (only Construct calls them
        // on the console), so the bring-up goes through the one public entry point that owns the order.
        lrRendererMemory.PCBringUpCreatePostFxSceneTargets(&sWorldDispatchAllocator);

        gpAntiAliasTarget = lrRendererMemory.GetAntiAliasBuffer();

        // ⚠️ THE GATE, NOT THE BODY, IS WHERE THE NULL TEST BELONGS. BeginRenderAntiAliased reaches
        // lpTarget->GetRenderTarget()->GetSectionRenderTargetState(0) with no test on either result,
        // and that is FAITHFUL -- the X360 asm has no null test there either, because on the console
        // the pool cannot fail. On PC it can: rw::graphics::postfx::RenderTarget::Initialize returns
        // nullptr when CarveZeroed fails (PostFxRenderTargetPCLeaf.cpp), and CgsRenderTarget::Construct
        // stores that straight through. So a failed allocation would turn a degraded frame into a
        // null-pointer crash inside a console-faithful body.
        //
        // Requiring the post-fx RenderTarget here keeps the console body untouched and leaves the
        // failure where every other PC bring-up guard in this file already lives: the bracket simply
        // never opens, the world keeps drawing straight to the back buffer, and the frame degrades
        // instead of crashing.
        const bool lbTargetsReady = gpAntiAliasTarget != nullptr
            && gpAntiAliasTarget->GetRenderTarget() != nullptr;

#if BRN_POSTFX_COMPOSITE_AVAILABLE
        // The console's BrnPostFx::Construct runs from BrnRendererModule::Construct @0x8240A778
        // (`bl BrnPostFx__Construct` @0x8240B78C, on this->mpGraphicsAllocator). On PC that Construct
        // has no device and a null graphics allocator, so -- like every pool object above and like
        // mIm3dRendererSkyDome.Construct further down -- it runs here, once, on the bring-up
        // allocator, the frame the post-fx pool exists. Idempotent inside. Under the composite gate
        // because BrnRendererModule.cpp reaches BrnPostFx only through the seam header (see the
        // include under the gate above).
        if (lbTargetsReady)
        {
            PCBringUpConstructPostFx(&sWorldDispatchAllocator);
        }
#endif
        return lbTargetsReady;
    }
}

void BrnRendererModule::Construct()
{
    // Double-buffered per-frame shader constants (maShaderConstantsFrames[2]).
    maShaderConstantsFrames[0].Construct();
    maShaderConstantsFrames[1].Construct();

    // ---- the GAME-side named shader constants (X360 Construct, in this order) --
    // Slots 0..7 belong to the engine and are registered by the table's own ctor
    // (@0x827EDDC8); these 27 are the game set. The registration ORDER is the
    // console's -- AddShaderConstant bumps mu8NumUsedConstants once per call, and
    // the setters bounds-check the slot index against that COUNT, so the whole set
    // has to be registered before the first SetShaderConstantData.
    CgsGraphics::mShaderConstantTable.AddShaderConstant( 8u, "ViewPosition",                 16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant( 9u, "KeyLightColour",               16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(10u, "KeyLightDirection",            16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(11u, "KeyLightSpecularColour",       16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(12u, "KeyLightClampedColour",        16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(13u, "Time",                         16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(34u, "ViewProjectionModified",       64);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(27u, "ScattCoeffs",                  16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(28u, "FogColourPlusWhiteLevel",      16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(33u, "SkyReflectionColour",          16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(29u, "HDRConstants",                 16);
    CgsGraphics::mShaderConstantTable.AddShaderConstantArray(14u, "ShadowMap_WorldToLight", 64, 3);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(15u, "ShadowMap_Constants",          16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(16u, "ShadowMap_Constants2",         16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(17u, "ShadowMap_ObjectCsmSelect",    16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(20u, "g_paintColour",                16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(21u, "g_pearlescentColour",          16);
    CgsGraphics::mShaderConstantTable.AddShaderConstantArray(22u, "g_verletOffsets", 16, 128);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(23u, "g_damageConstants",            16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(24u, "g_selfIlluminationMask",       16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(25u, "g_wheelConstants",             16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(26u, "g_PerVehicleFog",              16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(18u, "IrradianceQuadricA",           64);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(19u, "IrradianceQuadricB",           64);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(30u, "g_glassFractureStrength",      16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(31u, "g_glassFractureUVOffsets",     16);
    CgsGraphics::mShaderConstantTable.AddShaderConstant(32u, "g_glassFractureFresnelRanges", 16);

    // ---- The render-dispatch machinery (X360 Construct mid-section) ----------
    // DispatchFrame::Construct(&this+768, 25, dword_82F24238, mpGraphicsAllocator)
    // + SetupBuiltinInterpreters(&maInterpretFunctions) + the interpreter object.
    // The GDL side (mDoubleBufferedDispatchFrame, this+680) is built through the
    // real BufferedDispatchFrame with 2 slots so the game thread can fill the
    // write frame while Render walks the read frame.
    if (EnsureWorldDispatchAllocator())
    {
        mSingleBufferedDispatchFrame.Construct(KU_NUM_DISPATCH_LISTS,
                                               KU_PC_DISPATCH_BIN_BYTES,
                                               &sWorldDispatchAllocator);

        mDoubleBufferedDispatchFrame.SetNumDispatchFrames(2);
        mDoubleBufferedDispatchFrame.Construct(KU_NUM_DISPATCH_LISTS,
                                               KU_PC_GDL_DISPATCH_BIN_BYTES,
                                               &sWorldDispatchAllocator);

        CgsGraphics::SetupBuiltinInterpreters(maInterpretFunctions);
        mpInterpreter = new CgsGraphics::DispatchPacketInterpreter(maInterpretFunctions, 4);
        mpInterpreter->SetSingleBufferedDispatchFrame(&mSingleBufferedDispatchFrame);
        mpInterpreter->SetTime(0.0f);
    }

    // The loading-screen renderer (creates its textures + scratch buffer, picks language).
    mLoadingScreenRenderer.Construct();
}

// @ 0x82405E28 (BrnRendererModule::Update, the SetDispatchFrame expression)
// The GDL frame the game side fills this update frame:
//   v26 = (*(*(this + 680) + 28))(this + 680);   // vtable slot 7
//   RendererIO::OutputBuffer::SetDispatchFrame(lpOutput, v26);
// The full Update (camera copy, the fourteen other OutputBuffer publications,
// the render-switch/effects-frame plumbing) lands with the renderer IO buffers;
// this accessor is the one lane the world dispatch feed needs, named so the
// renderer -> world bridge binds to a real seam instead of poking the member.
CgsGraphics::DispatchFrame* BrnRendererModule::GetDispatchFrameForWrite()
{
    return &mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite();
}

// @ 0x823FC160 - BrnRendererModule::StartOfFrame.
// X360 order: Reset the GDL write frame, rewind the seven immediate-mode render
// buffers, ShaderConstantTable::BeginFrame on the GDL write bin, clear the 7x7
// texture-scope scratch (unk_83011A8C), rewind the corona submission interface.
// Reconstructed here: the two GDL halves (the parts whose subsystems exist).
// FLAG [PC gate]: the im-buffer rewinds / texture-scope clear / corona rewind
// land with CgsTextureScopeTable and the corona manager.
//
// THE EFFECTS ARBITRATOR IS NOT TOUCHED HERE, AND THAT IS THE BINARY, NOT AN OMISSION. The step-4
// brief expected BrnGraphics::EffectsArbitrator::StartOfFrame to be inlined into this function. It
// is not: the whole X360 body contains NO reference to a1 + 1152 (== this + 0x480 ==
// mEffectsArbitrator). The only members it reaches are the GDL frame (+680), the seven im-render
// buffers (+2828 / +4244 / +4756 / +5492 / +5532 / +5936 / +6040), the shader-constant table and the
// corona pair (+14336 / +14640). Nor does such a function exist anywhere in the image:
//     $ grep -rho "BrnGraphics::EffectsArbitrator::[A-Za-z0-9_]*" \
//           .ida-exports/BURNOUT_X360_ARTIST.XEX/ | sort -u
//     BrnGraphics::EffectsArbitrator::Construct
//     BrnGraphics::EffectsArbitrator::Construct_DWORD
//     BrnGraphics::EffectsArbitrator::EndOfFrame
//     BrnGraphics::EffectsArbitrator::EndOfFrameint
//     BrnGraphics::EffectsArbitrator::EvalTint
//     BrnGraphics::EffectsArbitrator::EvalTintint
// Three names, no StartOfFrame. The arbitrator's per-frame open IS its EndOfFrame (called from
// SwapBuffers), which both flips the double buffer and re-Constructs the new write slot. So no
// arbitrator call is added here.
void BrnRendererModule::StartOfFrame()
{
    // [FLAG PC bring-up] The layer-0 (base) effects frame's producer, at the earliest point of the
    // renderer's own frame bracket -- see PCBringUpProduceBaseEffectsFrame's banner for why this is
    // where the console's dispatch-thread producer maps to. Deliberately BEFORE the mpInterpreter
    // early-out below: the effects frames have nothing to do with the GDL ring, and on a build where
    // the ring never came up the post-fx composite still runs.
    if (EnsureEffectsArbitratorBringUp(mEffectsArbitrator))
    {
        PCBringUpProduceBaseEffectsFrame();
    }

    if (mpInterpreter == 0)
        return;   // Construct's allocator gate did not open -- no GDL ring.

    mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite().Reset();
    CgsGraphics::mShaderConstantTable.BeginFrame(
        &mDoubleBufferedDispatchFrame.GetDispatchBinForWrite());
}

// ==================================================================================================
// [FLAG PC bring-up] PCBringUpProduceBaseEffectsFrame -- NOT an X360 function.
//
// STANDS IN FOR BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10 (lines 40-120), which
// writes the LAYER-0 EXTERNAL BrnEffectsFrame every dispatch update; BrnRendererModule::Update
// @0x82405E28 (line 105) then publishes that frame through
// RendererIO::OutputBuffer::SetBaseEffectsFrame. NEITHER runs on this build:
//     $ grep -rn "GenerateRenderRequests" b5-decomp/src
//     (no hits)
//     $ grep -n "EffectsModule" tools/build/build_game_exe.bat
//     (no hits)
// and the RendererIO buffers are not created either (BrnGameModule.cpp:1339-1360 says so at length).
//
// WHERE IT RUNS. The console's producer runs on the DISPATCH thread, between Update and SwapBuffers;
// what matters is only that it writes the EXTERNAL slot before EffectsArbitrator::EndOfFrame flips
// it. On PC the frame is BrnGameModule::OnStartOfUpdateFrame -> StartOfFrame (here) ... DoDispatch
// ... DispatchThread -> Render (reads the INTERNAL slot) ... OnEndOfUpdateFrame -> EndOfFrame ->
// SwapBuffers -> EffectsArbitrator::EndOfFrame (the flip). So a write here reaches Render on the
// NEXT frame -- which is exactly the console's own one-frame producer/consumer pipeline, not a
// deviation. (Measured, first bloom-lit boot: the very first `[postfx-fx] apply-call 0` line already
// reads bloom=1 -- the composite's first apply-block call happens after at least one StartOfFrame/
// EndOfFrame pair has run, so there is no visible "all-false" first line; the counter in that log
// line counts apply-block CALLS, not game frames.)
//
// WHAT IT WRITES -- every value is the console's, on a frame with NO camera effects and the effects
// module's debug component at its Construct @0x82278C98 defaults (scratch/postfx_step4_bloom/
// DATA_NOTE.md section 3, conductor-extracted):
//   mbUseTint     = mbEnableTint     (module +181103)  -> true
//   mbUseTint2d   = mbEnable2dTint                     -> true
//   mbUseVignette = mbEnableVignette                   -> true
//   mbUseBloom    = mbEnableBloom                      -> true
//   mbUseBlur     = camera+180 flag | (module +181106 & +181105)   -> false with no camera effects
//   mbUseDepthOfField = (camera+308 > 0) && mbEnableDOF            -> false with no camera effects
// then, per ENABLED effect, the data block is built from an AttribSys asset and the weight set to
// 1.0f. The three assets and their SHIPPED values (build/game/POSTFX/POSTFXVAULT.BIN, resource
// 0x627894D7; the keys are Attrib::StringToKey of the decimal ids):
//   bloom    "191270" -> BloomData::Construct @0x82678070 reads data+20 / data+16 / data+0..16, i.e.
//                        mfLuminance 1.8f, mfThreshold 0.655738f, mv4Scale (0.954116, 0.947919,
//                        0.886839, 1.0). The console then ADDS camera+252 to the luminance and
//                        camera+248 to the threshold; both are 0 with no camera effects.
//   tint2d   "374388" -> TintData2d::Construct @0x82678268 copies 16 bytes: (0,0,0,0) -- NEUTRAL.
//   vignette "198102" -> NOT PRESENT in POSTFXVAULT.BIN nor in any other shipped bnd2 (the conductor
//                        searched every bundle under build/game). See the vignette block below.
//
// mbUseTint IS WRITTEN AND IS INERT ON THIS BUILD, deliberately. The colour-cube tint is consumed by
// Render's OTHER effects block (pseudocode lines 502-534: EffectsArbitrator::EvalTint ->
// BrnPostFx::SetTint -> BrnPostFx::BeginTintBlend), which this wave does NOT reconstruct, for a
// measured reason: E_FX_TINT (0x20) is the ONLY one of the five bits that moves the composite's
// PERMUTATION INDEX (BrnPostFxShader.cpp:1377-1380, `leShader = 4*blur | 2*dof | tint3d`), and only
// permutation 0 has a PC program pair -- BrnPostFxShader::Render hard-returns without drawing on any
// other (BrnPostFxShader.cpp:1389-1397). Lighting tint before its programs exist would present the
// frame un-composited. Bloom and vignette are IN permutation 0 (E_SHADER_BLOOM_VIGNETTE_TINT2D) and
// move no index, which is why this wave can light them. Writing the bool faithfully costs nothing
// while nothing reads it, and is what the tint step will need.
//
// DELETE-WHEN BrnEffects::EffectsModule::GenerateRenderRequests and the RendererIO buffers are live.
// ==================================================================================================
void BrnRendererModule::PCBringUpProduceBaseEffectsFrame()
{
    BrnEffectsFrame* const lpFrame = mEffectsArbitrator.GetExternalEffectsFrame(
        static_cast<u8>(BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_BASE), 0u);
    if (lpFrame == 0)
        return;

    // ==============================================================================================
    // THE CAMERA INPUT RECORD -- GenerateRenderRequests line 39, `CameraInput =
    // DispatchInputBuffer::GetCameraInput(a2)`. Its type is a BY-VALUE
    // BrnDirector::Camera::Camera (DWARF EffectsModuleIO.h:261 `Camera mCameraInput`), and that is
    // not an inference from the name: every raw displacement the producer reads off the record
    // lands exactly on an existing named member of the tree's Camera (mEffects @+0x68, 0xBC bytes;
    // mDepthOfField @+0x124, 0x14 bytes):
    //   CameraInput +172 (0xAC) = mEffects.mMotionBlurData.mfCarsBlurAmount        (effects +0x44)
    //   CameraInput +176 (0xB0) = mEffects.mMotionBlurData.mfWorldBlurAmount       (effects +0x48)
    //   CameraInput +180 (0xB4) = mEffects.mMotionBlurData.mbIsActive              (effects +0x4C)
    //   CameraInput +181 (0xB5) = mEffects.mMotionBlurData.mbIsExpensiveMotionBlur (effects +0x4D)
    //   CameraInput +248 (0xF8) = mEffects.mfBloomThreshold                        (effects +0x90)
    //   CameraInput +252 (0xFC) = mEffects.mfBloomLuminance                        (effects +0x94)
    //   CameraInput +292..+308  = mDepthOfField's five floats, in member order (+0x124..+0x134)
    // Seven displacements, seven exact member hits, nothing left over -- and SetCameraInput
    // @0x823C9988 settles it independently: its whole body is one
    // `BrnDirector::Camera::Camera::operator=(this + 0x50, lpCamera)`.
    // ==============================================================================================
    const BrnDirector::Camera::Camera&         lrCamera     = PCBringUpGetCameraInput();
    const BrnDirector::Camera::CameraEffects&  lrCameraFx   = lrCamera.GetEffects();
    const BrnDirector::Camera::MotionBlurData& lrCameraBlur = lrCameraFx.mMotionBlurData;
    const BrnDirector::Camera::DepthOfField&   lrCameraDof  = lrCamera.GetDepthOfField();

    // ---- the six bools (GenerateRenderRequests lines 40-56) --------------------------------------
    // Four are the debug component's always-on enables; the two CAMERA-DRIVEN ones are:
    //   v6  = mbMotionBlurEnableUserSettings && mbMotionBlur              (the debug override)
    //   v9  = camera.mMotionBlurData.mbIsActive | v6      -> frame+3 mbUseBlur
    //   v12 = (camera.mDepthOfField.mfBlurriness > 0) && mbDepthOfField
    //                                                     -> frame+2 mbUseDepthOfField
    // NOTE the asymmetry, and it IS the asm: mbUseBlur (the B4 radial/zoom blur) is gated on the
    // CAMERA's motion-blur-active flag alone -- `v9 = *(CameraInput + 180) | v6`, no module term.
    const bool lbMotionBlurUserOverride =
        KB_DEBUG_ENABLE_MOTION_BLUR_USER_SETTINGS && KB_DEBUG_ENABLE_MOTION_BLUR;
    const bool lbUseBlur         = lrCameraBlur.IsActive() || lbMotionBlurUserOverride;
    const bool lbUseDepthOfField = (lrCameraDof.GetBlurriness() > 0.0f) && KB_DEBUG_ENABLE_DOF;

    lpFrame->SetUseBloom(true);
    lpFrame->SetUseVignette(true);
    lpFrame->SetUseDepthOfField(lbUseDepthOfField);
    lpFrame->SetUseBlur(lbUseBlur);
    lpFrame->SetUseTint(true);
    lpFrame->SetUseTint2d(true);

    // ---- bloom: vault asset 191270, plus the camera's two ADDITIVE modifiers --------------------
    // GenerateRenderRequests, immediately after BloomData::Construct:
    //   *v47       = *(CameraInput + 252) + *v47;         // mfLuminance += mEffects.mfBloomLuminance
    //   *(v47 + 1) = *(CameraInput + 248) + *(v47 + 1);   // mfThreshold += mEffects.mfBloomThreshold
    // Both are 0.0f on a Constructed camera, so nothing changes today; the expression is the
    // console's, landed so a director state that raises them reaches the frame.
    {
        BrnEffects::BloomData lBloom;
        lBloom.mfLuminance = KF_BASE_FRAME_BLOOM_LUMINANCE + lrCameraFx.GetBloomLuminanceModifier();
        lBloom.mfThreshold = KF_BASE_FRAME_BLOOM_THRESHOLD + lrCameraFx.GetBloomThresholdModifier();
        lBloom.mv4Scale    = Vector4{ KF_BASE_FRAME_BLOOM_SCALE_R, KF_BASE_FRAME_BLOOM_SCALE_G,
                                      KF_BASE_FRAME_BLOOM_SCALE_B, KF_BASE_FRAME_BLOOM_SCALE_A };
        lpFrame->SetBloomData(lBloom, KF_BASE_FRAME_ENABLED_WEIGHT);   // data + weight in one call (DWARF BrnEffectsFrame.h:71)
    }

    // ---- 2D tint: vault asset 374388 (all four lanes zero -- neutral) --------------------------
    {
        BrnEffects::TintData2d lTint2d;
        lTint2d.mv4Colour.SetZero();
        lpFrame->SetTintData2d(lTint2d, KF_BASE_FRAME_ENABLED_WEIGHT);
    }

    // ---- vignette: the weight is written, the DATA BLOCK IS NOT ---------------------------------
    // [FLAG BLOCKED: the base-layer vignette asset is absent from every shipped bundle]
    //
    // The console's producer does VignetteData::Construct(&frame+0x40, hash64("198102")) and then
    // sets the weight to 1.0f. Asset "198102" is in NO shipped bundle -- not POSTFXVAULT.BIN, not
    // any other bnd2 under build/game (the conductor searched all of them) -- so on the retail
    // console VignetteData::Construct @0x826780D0 runs over an Attrib instance with NO collection
    // behind it and the block it writes is the vignetteasset's DefaultDataArea (0x50 bytes), whose
    // CONTENTS ARE NOT ATTESTED BY ANYTHING WE HAVE. So the data block is not written here: any
    // value would be invented, and this is the one effect where an invented value is not cosmetic
    // (see the measurement note below). What the frame carries instead is what
    // BrnEffectsFrame::Construct seeded -- VignetteData::Construct's own kv4Def* / kv2Def* statics.
    //
    // THE WEIGHT IS WRITTEN AT THE CONSOLE'S 1.0f, and that is not a compromise -- it is very nearly
    // a no-op, which the asm settles. EffectsArbitrator::EvalVignette (sub_823F9DE0) does NOT use
    // the base layer's weight at all:
    //   * it SEEDS the out block with the base layer's VignetteData verbatim -- a ten-iteration
    //     `ld`/`std` loop over 80 bytes from `mapaEffectsFrames[0] + 496*internal + 0x40`
    //     (asm 0x823F9E08-0x823F9E30), before it looks at any weight;
    //   * then, per layer 1..2, it folds that layer in with
    //     `VignetteData::SetToBlend(out, out, 1.0f - layerWeight, layerWeight, layerBlend)`
    //     (asm 0x823FA020-0x823FA034: `fsubs f1, f29(1.0), f31` / `fmr f2, f31` / r3 = r4 = out).
    // So the base layer IS the seed and its residual weight is (1 - the OTHER layers' sum).
    // EvalBloom (sub_823F9AA8) has the same shape -- it copies 32 bytes from frame+0x20 into the out
    // block first (asm 0x823F9AE0-0x823F9B00) -- which is ALSO why the bloom block written above is
    // what reaches BrnPostFx unchanged while the world layer contributes weight 0 on this build.
    //
    // WHAT THE CONDUCTOR MUST MEASURE, because this is the one arm that can black the frame: with no
    // world keyframes the vignette state IS the base frame's VignetteData, i.e. VignetteData's
    // kv4DefInnerColour / kv4DefOuterColour. Those two statics are DECLARED in
    // SharedClasses/Graphics/BrnEffectsData.h and DEFINED NOWHERE
    // (`grep -rn "kv4DefInnerColour" --include=*.cpp b5-decomp/src` -> no hits), so their values are
    // this wave's other open question. If they land as zeros, the composite's
    // lerp(inner, outer, gradient) is black everywhere and the frame goes black.
    //   * `[ImVerts pixels run N]` in BrnGame.log prints the render-target centre against the source
    //     texture's centre. With bloom on the two MUST differ. An rt centre of 000000 while tex0 is
    //     bright is the vignette going black.
    //   * The `[postfx-fx]` line reports vig=1 either way; it says the arbitrator evaluated, not that
    //     the result is sane.
    // UNBLOCK by attesting asset 198102 (or the vignetteasset DefaultDataArea bytes) and writing the
    // block here, next to the bloom one.
    // The frame API sets data+weight together (DWARF BrnEffectsFrame.h:76); the DATA is left as
    // BrnEffectsFrame::Construct seeded it (VignetteData::Construct() -> the kv*Def* statics dumped from
    // the CRT initialisers, BrnEffectsData.cpp), so only the weight changes here.
    lpFrame->SetVignetteData(lpFrame->GetVignetteData(), KF_BASE_FRAME_ENABLED_WEIGHT);

    // ---- depth of field: the camera's own focus band, five floats + weight 1.0 -------------------
    // GenerateRenderRequests lines 132-146: a five-iteration WORD loop copying
    // `*(CameraInput + 292..308)` into `frame + 144..160`, then `*(frame + 16) = 1.0f`. The source
    // IS the camera's DepthOfField sub-object in member order; the destination IS
    // BrnEffectsFrame::mDepthOfFieldData's five floats in member order. Five words only -- the 12
    // trailing pad bytes of DepthOfFieldData are NOT written, so the local is seeded from the
    // frame's current block rather than default-constructed.
    if (lbUseDepthOfField)
    {
        BrnEffects::DepthOfFieldData lDof = lpFrame->GetDepthOfFieldData();
        lDof.mfNearPlane   = lrCameraDof.GetFocusStartDistanceMeters();          // camera +292
        lDof.mfFocalPlane  = lrCameraDof.GetPerfectFocusStartDistanceMeters();   // camera +296
        lDof.mfFocalPlane2 = lrCameraDof.GetPerfectFocusEndDistanceMeters();     // camera +300
        lDof.mfFarPlane    = lrCameraDof.GetFocusEndDistanceMeters();            // camera +304
        lDof.mfDofAmount   = lrCameraDof.GetBlurriness();                        // camera +308
        lpFrame->SetDepthOfFieldData(lDof, KF_BASE_FRAME_ENABLED_WEIGHT);
    }

    // ---- B4 blur: vault asset 218901, the whole 96-byte block + weight 1.0 -----------------------
    // GenerateRenderRequests lines 160-166: `BlurData::Construct(v51, hash64("218901"))`, then
    // `memcpy(frame + 176, v51, 96)` (the WHOLE BlurData) and `*(frame + 20) = 1.0f`. The console
    // constructs the b4blurasset Attrib instance UNCONDITIONALLY one line earlier and destructs it
    // at the end of the function; only the Construct+copy is inside the `if`. On PC the AttribSys
    // read is replaced by the shipped values (the same choice the bloom arm already makes) -- see
    // the KF_BASE_FRAME_BLUR_* block for the byte provenance and the field mapping.
    if (lbUseBlur)
    {
        BrnEffects::BlurData lBlur = lpFrame->GetBlurData();
        lBlur.mfOpacity        = KF_BASE_FRAME_BLUR_OPACITY;          // data +0x48
        lBlur.mfVelocity       = KF_BASE_FRAME_BLUR_VELOCITY;         // data +0x40
        lBlur.mfSharpness      = KF_BASE_FRAME_BLUR_SHARPNESS;        // data +0x44
        lBlur.mfNoise          = KF_BASE_FRAME_BLUR_NOISE;            // data +0x4C
        lBlur.mfAngle          = KF_BASE_FRAME_BLUR_ANGLE;            // data +0x50
        lBlur.mv2BlendAmount.x = KF_BASE_FRAME_BLUR_BLEND_AMOUNT_X;   // data +0x30
        lBlur.mv2BlendAmount.y = KF_BASE_FRAME_BLUR_BLEND_AMOUNT_Y;
        lBlur.mv2BlendAmount.z = 0.0f;
        lBlur.mv2BlendAmount.w = 0.0f;
        lBlur.mv2BlurAmount.x  = KF_BASE_FRAME_BLUR_BLUR_AMOUNT_X;    // data +0x10
        lBlur.mv2BlurAmount.y  = KF_BASE_FRAME_BLUR_BLUR_AMOUNT_Y;
        lBlur.mv2BlurAmount.z  = 0.0f;
        lBlur.mv2BlurAmount.w  = 0.0f;
        lBlur.mv2BlendCentre.x = KF_BASE_FRAME_BLUR_BLEND_CENTRE_X;   // data +0x20
        lBlur.mv2BlendCentre.y = KF_BASE_FRAME_BLUR_BLEND_CENTRE_Y;
        lBlur.mv2BlendCentre.z = 0.0f;
        lBlur.mv2BlendCentre.w = 0.0f;
        lBlur.mv2BlurCentre.x  = KF_BASE_FRAME_BLUR_BLUR_CENTRE_X;    // data +0x00
        lBlur.mv2BlurCentre.y  = KF_BASE_FRAME_BLUR_BLUR_CENTRE_Y;
        lBlur.mv2BlurCentre.z  = 0.0f;
        lBlur.mv2BlurCentre.w  = 0.0f;
        lpFrame->SetBlurData(lBlur, KF_BASE_FRAME_ENABLED_WEIGHT);
    }

    // ---- motion blur: UNCONDITIONAL (GenerateRenderRequests lines 168-190) -----------------------
    // The console builds a stack MotionBlurData with the type's own canonical setter and copies its
    // three words into frame +0x1D8/+0x1DC/+0x1E0 whether or not anything is active -- so a frame
    // that has just turned motion blur OFF really does write mbIsActive = false rather than leaving
    // last frame's value standing. Two arms, selected by the debug override:
    //   if (mbMotionBlurEnableUserSettings)
    //       Set(mbMotionBlur, mbMotionBlurUserHighQuality,
    //           mfMotionBlurUserAmountCars, mfMotionBlurUserAmountWorld)
    //   else
    //       Set(mbMotionBlur && camera.mMotionBlurData.mbIsActive,
    //           camera.mMotionBlurData.mbIsExpensiveMotionBlur,
    //           camera.mMotionBlurData.mfCarsBlurAmount,
    //           camera.mMotionBlurData.mfWorldBlurAmount)
    // MotionBlurData::Set @0x8220AED8 is `void Set(bool, bool, f32, f32)` -- flags first, and it
    // clamps both amounts to [0,1] internally. This producer is its ONLY caller on the console too.
    // The local is seeded from the frame so the two pad bytes Set does not write keep the frame's
    // value instead of stack noise (the console's memcpy takes them off an uninitialised stack
    // slot; nothing reads them).
    {
        BrnDirector::Camera::MotionBlurData lMotionBlur = lpFrame->GetMotionBlurData();
        if (KB_DEBUG_ENABLE_MOTION_BLUR_USER_SETTINGS)
        {
            lMotionBlur.Set(KB_DEBUG_ENABLE_MOTION_BLUR,
                            KB_DEBUG_MOTION_BLUR_USER_HIGH_QUALITY,
                            KF_DEBUG_MOTION_BLUR_USER_AMOUNT_CARS,
                            KF_DEBUG_MOTION_BLUR_USER_AMOUNT_WORLD);
        }
        else
        {
            lMotionBlur.Set(KB_DEBUG_ENABLE_MOTION_BLUR && lrCameraBlur.IsActive(),
                            lrCameraBlur.IsExpensiveMotionBlur(),
                            lrCameraBlur.GetCarsBlendAmount(),
                            lrCameraBlur.GetWorldBlendAmount());
        }
        lpFrame->SetMotionBlurData(lMotionBlur);
    }

    // ---- the "is this the racing gameplay camera" flag -------------------------------------------
    // GenerateRenderRequests writes frame +0x1E4 from the effects module's own
    // TempRaceCarStateCache.mbIsGameCamera (module +181032), and that cache field is filled in
    // exactly ONE place -- BrnEffects::EffectsModule::Update @0x8229EC28, the player-car arm:
    //     this->field_2C328 = (*(CameraInput + 81) & 8) != 0;
    // CameraInput is typed `_DWORD*` at that site, so +81 words == +324 bytes == camera +0x144. The
    // camera's CameraState is at +0x138 and its mCurrentFlags BitArray<30> at state +0x08 == camera
    // +0x140; BitArray sets bits with `(u64)1 << index`, so on the big-endian console camera +0x144
    // is that qword's LOW half and mask 8 is bit index 3. The same word and a sibling bit are read
    // by BrnGameModule::DoDispatch @0x823DC458 lines 74-75 (`GetCameraOutput(...) + 324` tested
    // against 8 and 0x8000000), which corroborates both the word and the addressing.
    // The two-hop module path therefore collapses to a pure function of the record, which makes
    // this the ONE TempRaceCarStateCache field the PC producer can reproduce faithfully.
    lpFrame->SetIsRacingGameplayCamera(
        lrCamera.GetState().IsFlagSet(KU_CAMERA_STATE_FLAG_IS_RACING_GAMEPLAY));

    // ==============================================================================================
    // [FLAG BLOCKED: no PC source for the effects module's TempRaceCarStateCache]
    //
    // GenerateRenderRequests copies SIX more fields into the frame, and none of them comes from the
    // camera record -- they all come from BrnEffects::EffectsModule::TempRaceCarStateCache
    // mCarStateCache (DWARF EffectsModule.h:577, module +180864):
    //     frame +0x130 mCarTransform     <- cache mCarTransform      (module +180864)
    //     frame +0x170 mCameraTransform  <- cache mCameraTransform   (module +180928)
    //     frame +0x1B0 mLinearVelocity   <- cache mvLinearVelocity   (module +180992)
    //     frame +0x1C0 mAngularVelocity  <- cache mvAngularVelocity  (module +181008)
    //     frame +0x1D0 mfSpeedMPH        <- cache mfSpeedMPH         (module +181024)
    //     frame +0x1D4 mfSteering        <- cache mfSteering         (module +181028)
    // The four dynamic ones are filled by EffectsModule::Update @0x8229EC28 from the PLAYER's
    // ActiveRaceCarState (state +816 linear velocity, +832 angular velocity, +972 speed mph,
    // +1044 steering), reached through RCEntityActiveRaceCarOutputInterface off the effects INPUT
    // buffer. Neither the effects module nor that IO buffer exists on this build, so there is no
    // faithful value and none is invented.
    // The two TRANSFORMS are a stronger statement -- NOTHING IN THE IMAGE EVER WRITES THEM:
    //     $ grep -rl "180928" .ida-exports/BURNOUT_X360_ARTIST.XEX/
    //     .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8227FF10.json     (this producer -- the READ)
    //     $ grep -rl "180864" .ida-exports/BURNOUT_X360_ARTIST.XEX/
    //     .ida-exports/BURNOUT_X360_ARTIST.XEX/0x8227FF10.json     (this producer -- the READ)
    //     .ida-exports/BURNOUT_X360_ARTIST.XEX/0x823BC450.json     (an unrelated BrnNetworkModuleIO
    //                                                               accessor, `return a1 + 180864`)
    //     .ida-exports/BURNOUT_X360_ARTIST.XEX/0x825883F0.json     (unrelated)
    //     .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82593420.json     (unrelated)
    // and EffectsModule::Construct @0x8228FE98 does not initialise the cache either, so even on
    // retail those two frame transforms carry whatever the module allocation held.
    //
    // WHAT THE FRAME CARRIES INSTEAD, and it is measured rather than chosen: ZERO.
    // BrnEffectsFrame::Construct @0x822791E8 (and the tree's copy) writes NONE of the seven -- it
    // stops after the tint2d block and the MotionBlurData::Construct tail -- so the frames keep
    // their storage's initial bytes, and the storage is inside
    //     $ grep -rn "static BrnGame::BrnGameModule" b5-decomp/src
    //     b5-decomp/src/GameSource/Main/BrnMain.cpp:45:static BrnGame::BrnGameModule gGameModule;
    // i.e. a statically zero-initialised object.
    // THE ONE CONSUMER THAT CARES, and what zero does to it: the B4-blur arm in
    // BrnRendererModulePostFx.cpp reads GetSpeedMPH() and GetAngularVelocity().y whenever mbUseBlur
    // is set. Speed 0 makes its lfSpeedFactor 0, hence m_blurVelocity = mfVelocity * 0 * 0 = 0, and
    // yaw 0 leaves both blur centres unbiased at the authored (0.5, 0.5) / (0.5, 0). A B4 blur that
    // turns on today is therefore a still, centred, zero-velocity blur rather than a speed-driven
    // one -- wrong in DEGREE, not in kind, and it cannot read uninitialised memory.
    // UNBLOCK by giving the renderer a live player-car speed/velocity/steering source (the world
    // module's race-car state, or the real EffectsIO::InputBuffer) and writing the four here.
    // ==============================================================================================

    PCBringUpLogBaseEffectsFrameCameraState(*lpFrame, lrCamera);
}

// [FLAG PC bring-up] see the declaration in BrnRendererModule.h. The PC stand-in for
// BrnEffects::EffectsIO::DispatchInputBuffer::SetCameraInput @0x823C9988: one copy-assign of the
// director's published camera into the staged record the producer reads on its next run. The
// console's own body is that same single `Camera::operator=` behind a locked-for-writing assert;
// there is no lock here because there is no IO buffer to lock.
// DELETE-WHEN the EffectsIO dispatch buffer set is real on PC.
void BrnRendererModule::PCBringUpSetCameraInput(const BrnDirector::Camera::Camera* lpCamera)
{
    if (lpCamera == 0)
        return;   // [FLAG PC bring-up] the console cannot be handed a null here; DoDispatch can.
    PCBringUpGetCameraInput() = *lpCamera;
    gbPCBringUpCameraInputStaged = true;
}

// [FLAG PC bring-up] see the declaration in BrnRendererModule.h. Hands the world module the
// arbitrator's EXTERNAL world-layer frame for the slot, or nullptr while the arbitrator has not been
// Constructed. The world layer has FOUR slots (kau8SlotsPerEffectsLayer[1] == 4).
BrnEffectsFrame* BrnRendererModule::GetWorldEffectsFrameBringUp(u8 luSlot)
{
    if (!EnsureEffectsArbitratorBringUp(mEffectsArbitrator))
        return 0;
    return mEffectsArbitrator.GetExternalEffectsFrame(
        static_cast<u8>(BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_WORLD), luSlot);
}

// @ 0x823FC678 - BrnRendererModule::SwapBuffers (called by EndOfFrame @0x823FFE28).
// X360 order: the GDL ring Swap (vtable slot 4), two ShaderConstantTable
// Destruct calls, EffectsArbitrator::EndOfFrame, the shader-constants frame
// flip (+2768 <- +2769, +2769 <- 1 - old, BrnShaderConstantsFrame::Construct on
// the new write slot and the two +1964 flags), the seven im-buffer Swaps and the
// blobby-shadow / corona index flips.
// Reconstructed here: the GDL Swap + the EFFECTS-ARBITRATOR FLIP + the shader-constants frame flip.
// FLAG [PC gate]: the rest lands with those subsystems.
//
// THE ARBITRATOR FLIP IS AT THE CONSOLE'S POSITION IN THE ORDER, and the order is the point: the
// X360 body @0x823FC678 runs
//     (*(*(a1 + 680) + 16))(a1 + 680);                      <- the GDL ring Swap (vtable slot 4)
//     ...Destruct(&mShaderConstantTable); ...Destruct(v2);   <- the two ShaderConstantTable Destructs
//     BrnGraphics::EffectsArbitrator::EndOfFrame(a1 + 1152); <- HERE
//     v3 = *(a1 + 2769); *(a1 + 2768) = v3; ...              <- the shader-constants frame flip
// EndOfFrame promotes this frame's EXTERNAL slot to INTERNAL and re-Constructs the new external one,
// so it must run AFTER every producer wrote (StartOfFrame / DoDispatch, both earlier in the update
// frame) and BEFORE the next frame's Render reads. Moving it either side of the shader-constants
// flip would be harmless today and wrong tomorrow; it is kept where the asm has it.
void BrnRendererModule::SwapBuffers()
{
    // NOT behind the mpInterpreter early-out below. That gate is about the GDL ring; the effects
    // frames are a separate double buffer, and skipping their flip would freeze the internal slot on
    // whatever the first frame left -- so bloom would latch to frame 0's all-false frame forever.
    if (sbEffectsArbitratorConstructed)
    {
        mEffectsArbitrator.EndOfFrame();
    }

    if (mpInterpreter == 0)
        return;

    mDoubleBufferedDispatchFrame.Swap();

    mu8ShaderConstantsFrameInternal = mu8ShaderConstantsFrameExternal;
    mu8ShaderConstantsFrameExternal =
        static_cast<u8>(1u - mu8ShaderConstantsFrameInternal);
    maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].Construct();
}

// @ 0x823FFE28 - BrnRendererModule::EndOfFrame, called from
// BrnGame::BrnGameModule::OnEndOfUpdateFrame @0x823DBBA0.
//
// The X360 body takes a `freeze rendering` bool and runs a 3-state latch over
// this+50548 / this+50552 that suppresses SwapBuffers while the freeze is held
// (0 = running -> swap; 1 = entering, swap once the 2-frame counter expires;
// 2 = frozen-but-still-swapping-once). It then consumes the this+50276 ->
// this+50277 camera-cut edge Update sets. FLAG [PC gate]: neither the freeze
// latch pair nor the camera-cut pair is in the PC member layout yet, and no PC
// caller passes the bool -- this is the freeze=false path, which is the only one
// the game runs outside the debug freeze-frame feature.
void BrnRendererModule::EndOfFrame()
{
    SwapBuffers();
}

// @ 0x823F5898 - BrnRendererModule::ConvertObjectsToMeshes. The X360 runs 16
// object-to-mesh jobs when the MT switch (byte_82F2423C) is on, else the
// single-threaded fallback: per GDL object list j in 0..12, reset the constant
// table's dispatch shadow, take a fresh copy of the 240-byte object context and
// expand the read-side frame's list into the render frame's mesh lists.
// The PC bring-up runs that ST fallback (the job scheduler is not up).
void BrnRendererModule::ConvertObjectsToMeshes(CgsGraphics::BufferedDispatchFrame* lpGdlFrames,
                                               CgsGraphics::DispatchFrame* /*lpMeshFrame*/,
                                               CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
                                               const CgsGraphics::DispatchObjectContext* lpContext)
{
    for (u32 luListId = 0; luListId < 13u; ++luListId)
    {
        // X360 per-pass prologue: mShaderConstantTable.ResetShadowingForDispatch()
        // + the 7x7 texture-scope scratch clear (unk_83011A90). Both shadow the
        // PRODUCER-side dirty tracking; the expansion context below is a fresh
        // copy each pass, so the bring-up defers them with the texture-scope
        // reconstruction. FLAG [deferred with CgsTextureScopeTable].

        CgsGraphics::DispatchObjectContext lContextCopy;
        std::memcpy(&lContextCopy, lpContext, sizeof(lContextCopy));

        CgsGraphics::DispatchFrame& lrGdlFrame = lpGdlFrames->GetDispatchFrameForRead();
        lrGdlFrame.GetList(luListId)->DispatchAllObjectToMesh(
            lpInterpreter, lpInterpreter->GetSingleBufferedDispatchFrame(),
            &lContextCopy, 0, -1);
    }
}

// @ 0x823F5F70 - BrnRendererModule::SortDispatchLists. The X360 preps 16
// RadixSort jobs over lists {0,2,1,3,4, 5..10, 21, 11, 19, 15, 20}; the PC
// bring-up sorts the same lists synchronously.
void BrnRendererModule::SortDispatchLists(CgsGraphics::DispatchFrame* lpMeshFrame)
{
    static const u32 KAU_SORTED_LISTS[16] =
        { 0u, 2u, 1u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 21u, 11u, 19u, 15u, 20u };
    for (u32 luIndex = 0; luIndex < 16u; ++luIndex)
    {
        lpMeshFrame->GetList(KAU_SORTED_LISTS[luIndex])->SortForDispatch();
    }
}

// @0x8240BFA8 (Render:389-396) - the render frame reset, the per-frame object context, the
// object->mesh expansion and the pass sorts. This block used to open RenderWorldPasses; it is
// hoisted into its own method (and called from Render) because the console runs it BEFORE the
// shadow-map pass, which consumes mesh lists 0..4.
bool BrnRendererModule::BuildDispatchLists(CgsGraphics::DispatchObjectContext* lpContext)
{
    using namespace CgsGraphics;

    std::memset(lpContext, 0, sizeof(*lpContext));

    if (mpInterpreter == 0)
        return false;

    // Start-of-frame: reset the render frame + point the interpreter at it
    // (X360: DispatchFrame::Reset(this+768); interp+12 = frame; interp+8 = 0).
    mSingleBufferedDispatchFrame.Reset();
    mpInterpreter->SetSingleBufferedDispatchFrame(&mSingleBufferedDispatchFrame);
    mpInterpreter->SetTime(0.0f);

    // The 240-byte object context (X360 builds it on the Render stack):
    // constant shadow cleared, list base 0, the pre-Z config from the module.
    lpContext->ResetShadowing();
    lpContext->miListIdBase       = 0;
    lpContext->mbPreZEnabled      = mbRenderPreZ;
    lpContext->mbPreZAlphaEnabled = mbRenderPreZAlpha;
    const f32 lfPreZDistance = mbPreZNearOnly ? mfPreZDistanceThreshold : 100000.0f;
    for (u32 luLane = 0; luLane < 4; ++luLane)
        lpContext->mvPreZDistanceThreshold[luLane] = lfPreZDistance * lfPreZDistance;

    // Object -> mesh expansion + the pass sorts.
    ConvertObjectsToMeshes(&mDoubleBufferedDispatchFrame, &mSingleBufferedDispatchFrame,
                           mpInterpreter, lpContext);
    SortDispatchLists(&mSingleBufferedDispatchFrame);
    return true;
}

// =============================================================================
// @0x8240BFA8 (Render:545-640) - BrnRendererModule's SHADOW-MAP PASS.
//
// Three cascades, each one Begin/EndRenderShadowMap around a Z-only walk of its mesh
// lists. The list-to-cascade map is the X360's, read straight off the unrolled body:
//   cascade 0 -> GetList(0) then GetList(2)
//   cascade 1 -> GetList(1) then GetList(3)
//   cascade 2 -> GetList(4)
// The console clears on every BeginRenderShadowMap (the literal 1 in each call) and hands
// the manager &mAllocatedRenderTargets. The cascade count is not a variable on the console
// either -- the body is written out three times.
//
// TWO PIECES OF THE CONSOLE BODY ARE DELIBERATELY ABSENT; both are marked below:
//   (a) the six EA::Jobs::Job::WaitOn(maShadowMapSortJob[n]) calls, and
//   (b) the front/back-face cull bracket around one list per cascade.
// =============================================================================
void BrnRendererModule::RenderShadowMapPasses(CgsGraphics::DispatchObjectContext* lpContext)
{
    using namespace CgsGraphics;

    // The gate the console reads at Render:545 (*(this+50188)). Until this wave the switch was
    // WRITTEN by ConstructRenderSwitches and never READ anywhere -- this is its first reader.
    if (!mRenderSwitches.mbRenderShadows)
        return;

    // The cascade -> mesh-list map, as three explicit rows (the X360 body is unrolled the same
    // way). -1 = the cascade has no second list.
    static const s32 KAI_CASCADE_LISTS[3][2] = { { 0, 2 }, { 1, 3 }, { 4, -1 } };

    // Nothing to draw: with no shadow-caster records the whole pass is a target bind, a clear and
    // a resolve of an empty depth buffer. The console pays that every frame; this build skips it
    // so that a boot with no world data leaves the device exactly as it found it (the same
    // data-gating the world passes below use).
    u32 lauCascadeCounts[3] = { 0u, 0u, 0u };
    u32 luTotalShadowRecords = 0u;
    for (s32 liCascade = 0; liCascade < 3; ++liCascade)
    {
        for (s32 liSlot = 0; liSlot < 2; ++liSlot)
        {
            const s32 liList = KAI_CASCADE_LISTS[liCascade][liSlot];
            if (liList < 0)
                continue;
            lauCascadeCounts[liCascade] +=
                mSingleBufferedDispatchFrame.GetList(static_cast<u32>(liList))->GetCount();
        }
        luTotalShadowRecords += lauCascadeCounts[liCascade];
    }

    // [shadow-pass diagnostic] the per-cascade record counts actually dispatched.
    //
    // LATCHED ON THE VALUE, never on a "printed once" bool: a one-shot here fires on the boot
    // loading screen -- where every list is empty and the pass has not even run -- and then
    // never again, so it would permanently report zeroes (the wheel-render wave's bug, and the
    // reason the mesh-list probe below is written the same way). Reprints whenever the triple
    // changes, which is exactly when casters enter or leave a cascade.
    {
        static u32 sauLastCounts[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        if ((lauCascadeCounts[0] != sauLastCounts[0]
          || lauCascadeCounts[1] != sauLastCounts[1]
          || lauCascadeCounts[2] != sauLastCounts[2])
            && CgsDev::Log::gpDebugPrint != 0)
        {
            sauLastCounts[0] = lauCascadeCounts[0];
            sauLastCounts[1] = lauCascadeCounts[1];
            sauLastCounts[2] = lauCascadeCounts[2];
            *CgsDev::Log::gpDebugPrint
                << "[shadow-pass] cascade records: c0(lists 0,2)=" << static_cast<s32>(lauCascadeCounts[0])
                << " c1(lists 1,3)="                               << static_cast<s32>(lauCascadeCounts[1])
                << " c2(list 4)="                                  << static_cast<s32>(lauCascadeCounts[2])
                << "\n";
        }
    }

    if (luTotalShadowRecords == 0u)
        return;

#if !BRN_SHADOW_MAP_TARGET_AVAILABLE
    // No shadow-map render target exists on this build (see the gate banner at the top of this
    // file). Returning HERE -- after the diagnostic, before the target bind -- is deliberate:
    // dispatching the cascades with no shadow target bound would draw every caster straight into
    // the back buffer, which is far worse than rendering no shadows at all.
    return;
#else

    // Pass stats, the same way the world block below accumulates its four (the raw totals feed
    // the debug HUD; mu32NumShadowObjects is the 60-frame average the console derives from them,
    // and is left to the averaging pass that owns the other four).
    mu32NumShadowObjectTotals += luTotalShadowRecords;

    // FLAG PC-platform leaf: save the scene surfaces before the manager binds the shadow target,
    // and put them back after the last cascade. The console does not need this -- its
    // BeginRenderAntiAliased (Render:725) rebinds the scene target after this pass and the env-map
    // pass -- but on PC renderengine::Device::FrameBegin bound the back buffer before Render
    // started and nothing else ever rebinds it. See ShadowPassPCLeaf.h.
    renderengine::PCSurfaceBracket_Save();

    // [FLAG PC bring-up probe] draw-call snapshot for the [shadow-fetch] line below.
    const u64 luDrawCallsBeforePass = renderengine::WorldDrawCallCount();
    u32       lauCascadeDrawCalls[3] = { 0u, 0u, 0u };

    for (s32 liCascade = 0; liCascade < 3; ++liCascade)
    {
        mShadowMapRenderManager.BeginRenderShadowMap(liCascade, /*lbClear*/ true,
                                                     &mAllocatedRenderTargets);

        // [FLAG PC bring-up probe] bracket this cascade's draws in an occlusion query. See
        // ShadowPassPCLeaf.h -- this is the ground truth for "is the map being written".
        // Issued AFTER the clear so the clear's own fill is not counted.
        const u64 luDrawCallsBeforeCascade = renderengine::WorldDrawCallCount();
        renderengine::ShadowProbe_Begin(static_cast<u32>(liCascade));

        for (s32 liSlot = 0; liSlot < 2; ++liSlot)
        {
            const s32 liList = KAI_CASCADE_LISTS[liCascade][liSlot];
            if (liList < 0)
                continue;

            // [X360 ONLY] EA::Jobs::Job::WaitOn(&maShadowMapSortJob[liList], 0, 0, -1) sits here
            // on the console -- one wait per list, immediately before its GetList. It is a
            // rendezvous with the RadixSort job SortDispatchLists kicked off for that list, and
            // it has NO PC counterpart to write: SortDispatchLists runs the sorts SYNCHRONOUSLY
            // on this very thread (see its body), so by the time control reaches here every list
            // is already sorted and a wait would be a wait on nothing. The maShadowMapSortJob
            // array is still carried in the layout, unused, for when the job scheduler lands --
            // at which point these five waits come back with it.

            mSingleBufferedDispatchFrame.GetList(static_cast<u32>(liList))
                ->DispatchAllMeshesZOnly(mpInterpreter, lpContext);

            // [PARKED - unattested data] The console wraps ONE list per cascade (the first when
            // mbForceFrontFaceCull is clear, the second when it is set) in the cull-state bracket
            //   sub_82276B38(dword_83010A3C); shadow::Device::LockRasteriserState();
            //   ... UnlockRasteriserState(); sub_82276B38(dword_83010A38);
            // which the DWARF names ShadowMapRenderManager::Begin/EndFrontFaceCullRender and
            // Begin/EndBackFaceCullRender (both inlined by the X360 compiler, so neither has an
            // exported body). sub_82276B38 is ImRendererBase::SetState(const RasterizerState*),
            // and the two arguments are DATA globals: the IDA exports carry no data section, so
            // the contents of dword_83010A3C / dword_83010A38 -- the whole point of the bracket,
            // its cull mode and depth bias -- are unattested. Standing up a plausible
            // "cull front faces" RasterizerState here would be a fabricated constant, and locking
            // the rasteriser WITHOUT first setting one is strictly worse than not locking (the
            // pass would inherit whatever state the previous frame left and refuse every
            // per-material rasteriser bind for the rest of the walk). So the bracket is omitted:
            // casters render with their own material rasteriser state, which is what the console
            // does for the OTHER list of every cascade anyway. Restore this when the two globals'
            // bytes are recovered.
        }

        renderengine::ShadowProbe_End(static_cast<u32>(liCascade));
        lauCascadeDrawCalls[liCascade] =
            static_cast<u32>(renderengine::WorldDrawCallCount() - luDrawCallsBeforeCascade);

        mShadowMapRenderManager.EndRenderShadowMap(liCascade, &mAllocatedRenderTargets);
    }

    renderengine::PCSurfaceBracket_Restore();

    // =========================================================================
    // [FLAG PC bring-up probe] "[shadow-fetch]" -- the shadow-map GROUND TRUTH line.
    //
    // Three independent facts, on one line, because the failure modes are only
    // distinguishable together:
    //
    //   fmt=/compare=   which depth format the target actually got, and whether its
    //                   fetch COMPARES (the Xenos semantic all 92 s15 shaders assume)
    //                   or returns RAW depth (then `lit *= rawDepth`, and a cleared
    //                   map reads as "fully lit" -- indistinguishable in the frame
    //                   from having no shadow map at all).
    //   s15=            whether the D3D9 runtime really holds a texture at unit 15
    //                   (asked of the device, not of the engine's own bind cache).
    //   cN draws/px     per cascade: how many draws were submitted into its band, and
    //                   how many FRAGMENTS survived the depth test there. px>0 proves
    //                   the map is written. draws>0 with px==0 means the geometry is
    //                   being submitted but lands outside the band (viewport, cull
    //                   mode, or a cascade matrix) -- a completely different bug from
    //                   draws==0, and the two look identical on screen.
    //
    // ⚠ LATCHED ON A SIGNATURE, never on a `static bool` one-shot (that fires on the
    // loading screen, before the world exists, and then never again -- this project's
    // most repeated diagnostic bug). The latch is the zero/non-zero pattern plus a
    // power-of-two bucket of each pixel count, so it reprints when something
    // meaningful changes and stays quiet while the camera merely moves.
    // =========================================================================
    {
        u32 lauPixels[3] = { 0u, 0u, 0u };
        u32 lauHave[3]   = { 0u, 0u, 0u };
        for (u32 luCascade = 0; luCascade < 3u; ++luCascade)
        {
            lauHave[luCascade] =
                renderengine::ShadowProbe_LastPixels(luCascade, &lauPixels[luCascade]) ? 1u : 0u;
        }

        const bool lbSampler15  = renderengine::ShadowProbe_TextureBound(15u);
        const u32  luFormat     = renderengine::ShadowDepthFormat();
        const bool lbHwCompare  = renderengine::ShadowDepthFormatIsHardwareCompare();

        // Power-of-two bucket: 0 stays 0, everything else collapses to its magnitude, so a
        // camera pan does not reprint the line but "this cascade stopped drawing" does.
        u32 luSignature = (lbSampler15 ? 1u : 0u) | (lbHwCompare ? 2u : 0u);
        for (u32 luCascade = 0; luCascade < 3u; ++luCascade)
        {
            u32 luBucket = 0u;
            for (u32 luValue = lauPixels[luCascade]; luValue != 0u; luValue >>= 1)
                ++luBucket;
            u32 luDrawBucket = 0u;
            for (u32 luValue = lauCascadeDrawCalls[luCascade]; luValue != 0u; luValue >>= 1)
                ++luDrawBucket;
            luSignature = (luSignature * 131u) + (luBucket * 37u) + luDrawBucket + lauHave[luCascade];
        }

        static u32 suLastSignature = 0xFFFFFFFFu;
        const bool lbEmit = (luSignature != suLastSignature) && (CgsDev::Log::gpDebugPrint != 0);
        suLastSignature = luSignature;
        if (lbEmit)
        {
            *CgsDev::Log::gpDebugPrint
                << "[shadow-fetch] fmt=0x" << static_cast<s32>(luFormat)
                << " compare=" << (lbHwCompare ? "HW" : "RAW")
                << " s15=" << (lbSampler15 ? 1 : 0)
                << " passDraws=" << static_cast<s32>(renderengine::WorldDrawCallCount()
                                                     - luDrawCallsBeforePass);
            for (u32 luCascade = 0; luCascade < 3u; ++luCascade)
            {
                *CgsDev::Log::gpDebugPrint
                    << " c" << static_cast<s32>(luCascade)
                    << "(draws=" << static_cast<s32>(lauCascadeDrawCalls[luCascade])
                    << " px=";
                if (lauHave[luCascade] != 0u)
                    *CgsDev::Log::gpDebugPrint << static_cast<s32>(lauPixels[luCascade]);
                else
                    *CgsDev::Log::gpDebugPrint << "pending";
                *CgsDev::Log::gpDebugPrint << ")";
            }
            *CgsDev::Log::gpDebugPrint << "\n";
        }

        // ---------------------------------------------------------------------
        // [FLAG PC bring-up probe] "[shadow-clip]" -- WHY the fragments are lost.
        //
        // The occlusion line above answered "thousands of draws, no fragments"; this
        // answers where the geometry actually went, per cascade, plus the device state
        // the cascade's first draw really ran under. Slot 3 is the WORLD-OPAQUE
        // CONTROL, bracketed in RenderWorldPasses: it is there to prove the instrument.
        // If the control shows a large px and a sane viewport while the cascades show
        // nothing, the probe is sound and the cascades are genuinely empty; if the
        // control ALSO shows px=0, the probe itself is lying and nothing else on this
        // line may be trusted.
        //
        // Latched on the SAME signature as the line above (the shared lbEmit), so the
        // two always describe the same frame.
        // ---------------------------------------------------------------------
        if (lbEmit)
        {
            for (u32 luSlot = 0; luSlot < 4u; ++luSlot)
            {
                renderengine::ShadowClipReport lReport;
                if (!renderengine::ShadowProbe_ClipTally(luSlot, &lReport))
                    continue;

                u32 luControlPixels = 0;
                const bool lbControlHave =
                    renderengine::ShadowProbe_LastPixels(luSlot, &luControlPixels);

                *CgsDev::Log::gpDebugPrint
                    << "[shadow-clip] " << (luSlot < 3u ? "c" : "CONTROL-worldOpaque c")
                    << static_cast<s32>(luSlot)
                    << " sampled=" << static_cast<s32>(lReport.muSampled)
                    << " inside="  << static_cast<s32>(lReport.muInside)
                    << " outXY="   << static_cast<s32>(lReport.muOutXY)
                    << " outZnear=" << static_cast<s32>(lReport.muOutZNear)
                    << " outZfar=" << static_cast<s32>(lReport.muOutZFar)
                    << " behindW=" << static_cast<s32>(lReport.muBehindW)
                    << " noWvp="   << static_cast<s32>(lReport.muNoWvp)
                    << " firstObj(" << lReport.mafFirstObject[0]
                    << "," << lReport.mafFirstObject[1]
                    << "," << lReport.mafFirstObject[2] << ")"
                    << " firstClip(" << lReport.mafFirstClip[0]
                    << "," << lReport.mafFirstClip[1]
                    << "," << lReport.mafFirstClip[2]
                    << ",w" << lReport.mafFirstClip[3] << ")"
                    << " px=";
                if (lbControlHave)
                    *CgsDev::Log::gpDebugPrint << static_cast<s32>(luControlPixels);
                else
                    *CgsDev::Log::gpDebugPrint << "pending";
                *CgsDev::Log::gpDebugPrint
                    << " | vp(" << static_cast<s32>(lReport.muVpX)
                    << ","      << static_cast<s32>(lReport.muVpY)
                    << ","      << static_cast<s32>(lReport.muVpW)
                    << "x"      << static_cast<s32>(lReport.muVpH)
                    << " z"     << lReport.mfVpMinZ << ".." << lReport.mfVpMaxZ << ")"
                    << " sc("   << static_cast<s32>(lReport.miScissorL)
                    << ","      << static_cast<s32>(lReport.miScissorT)
                    << ","      << static_cast<s32>(lReport.miScissorR)
                    << ","      << static_cast<s32>(lReport.miScissorB)
                    << " en="   << static_cast<s32>(lReport.muScissorEnable) << ")"
                    << " zen="  << static_cast<s32>(lReport.muZEnable)
                    << " zwr="  << static_cast<s32>(lReport.muZWrite)
                    << " zfunc=" << static_cast<s32>(lReport.muZFunc)
                    << "/eff"    << static_cast<s32>(lReport.muZFuncEffective)
                    << " cull=" << static_cast<s32>(lReport.muCull)
                    << "/eff"   << static_cast<s32>(lReport.muCullEffective)
                    << " cwe="  << static_cast<s32>(lReport.muColourWrite)
                    << "\n";

                // THE EXTENT LINE -- the number this wave exists to read. `fit` is the
                // cascade's own half-width/height in METRES, taken off the matrix columns
                // rather than inferred; compare it against the slab the cascade is supposed
                // to cover (0..10.5, 10.5..34, 34..120 m). `clipAABB` is the scale-free
                // cross-check: a correct fit puts the casters the cascade SELECTED across
                // most of [-1,1]. `subPixTris` is the verdict on whether the geometry is
                // simply too small to raster.
                *CgsDev::Log::gpDebugPrint
                    << "[shadow-extent] " << (luSlot < 3u ? "c" : "CONTROL c")
                    << static_cast<s32>(luSlot)
                    << " fitHalfW=" << lReport.mfHalfWidthMetres << "m"
                    << " fitHalfH=" << lReport.mfHalfHeightMetres << "m"
                    << " depthSpan=" << lReport.mfDepthSpanMetres << "m"
                    << " clipAABB x[" << lReport.mafClipMin[0] << "," << lReport.mafClipMax[0]
                    << "] y["         << lReport.mafClipMin[1] << "," << lReport.mafClipMax[1]
                    << "] z["         << lReport.mafClipMin[2] << "," << lReport.mafClipMax[2]
                    << "] subPixTris=" << static_cast<s32>(lReport.muTrisSubPixel)
                    << "/"             << static_cast<s32>(lReport.muTrisSampled)
                    << " maxTriPx="    << lReport.mfMaxTriPixelArea
                    << "\n";
            }
        }
    }
#endif  // BRN_SHADOW_MAP_TARGET_AVAILABLE
}

#if BRN_ANTIALIAS_BRACKET_AVAILABLE

#include "pc/gcm/renderengine/Xbox2SurfaceShims.h"   // renderengine::D3DDevice_BeginTiling / EndTiling / Resolve / SetPredication + gpD3DDevice
#include "GameSource/Graphics/BrnAntiAliasTiling.h"  // BrnGraphics::KMSAA_TILING_PLAN / KNO_MSAA_TILING_PLAN (the recovered tile rects)
#if BRN_GPU_PERFMON_AVAILABLE
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"  // CgsDev::PerfMonGpu::Start/StopMonitor
#endif

// ================================================================================================
// THE FRAME BRACKET -- BeginRenderAntiAliased @0x823FFA18 / ResolveMSAA @0x823FFBE0.
//
// THE SEAM, one line per function (this is the thing the PC leaf decision hangs on):
//
//   BeginRenderAntiAliased -- HALF AND HALF. Console EDRAM machinery with no D3D9 counterpart:
//     D3DDevice_BeginTiling + D3DDevice_SetPredication. Platform-neutral frame logic the PC build
//     MUST execute: the background-colour maths, the mvBackgroundColour publish, and the section-0
//     RenderTargetState bind -- that bind IS what makes the world pass render off-screen.
//
//   ResolveMSAA -- NOTHING in it is platform-neutral; every call it makes is the EDRAM copy-out
//     (SetPredication, both Resolves, EndTiling). But the PC leaf still OWES the CLEAR, because the
//     0x300 colour resolve is the ONLY clear on the untiled path -- BeginRenderAntiAliased's untiled
//     branch deliberately clears nothing at all.
//
// WHAT THAT MEANS FOR THE FOUR SHIMS, stated as answers rather than options:
//   * D3DDevice_SetPredication and D3DDevice_EndTiling can be legitimately EMPTY on PC. With one
//     pass over the whole surface there is no tile replay to select and no tiling pass to close
//     (D3DDevice_Begin/EndConditionalRendering @XenonD3D9Shims.cpp:3123 are the existing precedent
//     for a legitimately empty D3DDevice_* shim).
//   * D3DDevice_BeginTiling CANNOT be empty. The console got TWO things out of it that nothing else
//     in this function supplies: (1) the VIEWPORT + SCISSOR, which on the Xenos come from the tile
//     rects -- BeginRenderAntiAliased issues no D3DDevice_SetViewportF / SetScissorRect anywhere
//     (0x823FFA18-0x823FFBD8 contains neither call), so on PC the pass would inherit whatever
//     viewport the shadow or env-map pass left; and (2) the colour/Z/stencil CLEAR.
//     ⚠ (1) is INTERPRETATION from the documented Xenon tiling model, NOT recovered from this
//     image; (2) is FACT, read off the three clear arguments at 0x823FFB44-0x823FFB54.
//   * D3DDevice_Resolve CANNOT be empty either, for the clear in the paragraph above.
//
// (A) NO FORKED SYMBOLS. All four Xenos entry points are declared in their committed home,
// pc/gcm/renderengine/Xbox2SurfaceShims.h, inside namespace renderengine -- D3DDevice_EndTiling and
// D3DDevice_Resolve were already there (lines 34-42) and are CORRECTED in place to the ABI decoded
// below; D3DDevice_BeginTiling and D3DDevice_SetPredication are genuinely new and are added beside
// their siblings. Nothing is re-declared here. renderengine::gpD3DDevice comes from the same header
// (line 45), so the local `namespace renderengine { extern void* gpD3DDevice; }` an earlier draft
// carried is gone too.
// ================================================================================================

namespace
{
    // --- the bracket's recovered constants ------------------------------------------------------
    // Every value below is matched to the DISPLACEMENT the assembly addresses. Each .rdata block in
    // DATA_DUMP.md runs PAST its symbol into unrelated rodata (flt_82001CC0 +0x08 spells "Monitor ",
    // flt_82004740 +0x08 spells "mpIceWra", flt_820473A4 +0x14 spells "kColourAndPo"), so a value
    // taken from a block's first dword instead of its addressed displacement would be wrong.

    // Grey-background channel: flt_82004740 +0x00 == 0x3E99999A == 0.300000012f, addressed by
    // `lfs f0, flt_82004740@l(r11)` @0x823FFA44.
    const f32 KF_BACKGROUND_COLOUR_GREY  = 0.3f;

    // The normal background tint -- one displacement each into the flt_820473A4 block, matched to
    // the store that consumes it:
    //   flt_820473AC = block +0x08 = 0x3F3851EC = 0.720000029f -> RED   (lfs @0x823FFA70, stfs .x @0x823FFA7C)
    //   flt_820473A8 = block +0x04 = 0x3F547AE1 = 0.829999983f -> GREEN (lfs @0x823FFA80, stfs .y @0x823FFA8C)
    //   flt_820473A4 = block +0x00 = 0x3F63D70A = 0.889999986f -> BLUE  (lfs @0x823FFA90, stfs .z @0x823FFA98)
    // R < G < B: it is a pale blue.
    const f32 KF_BACKGROUND_COLOUR_RED   = 0.72f;
    const f32 KF_BACKGROUND_COLOUR_GREEN = 0.83f;
    const f32 KF_BACKGROUND_COLOUR_BLUE  = 0.89f;

    // Alpha: flt_82001CC0 +0x00 == 0x00000000 == 0.0f (asm @0x823FFA5C / @0x823FFA9C). A DUMPED
    // zero, read off a cited displacement -- not a placeholder standing in for an unknown.
    const f32 KF_BACKGROUND_COLOUR_ALPHA = 0.0f;

    // Every clear and resolve in the bracket clears Z to flt_82001C98 +0x00 == 0x3F800000 == 1.0f
    // (`lfs f1, flt_82001C98@l(r10)` @0x823FFB54; `lfs f31, ...` @0x823FFC78 / @0x823FFDAC).
    const f32 KF_CLEAR_Z = 1.0f;

    // The HALF-PIXEL scale the console's own render targets compute their UV offset with.
    // rw::graphics::postfx::RenderTarget::GetHalfPixelOffset @0x823FE668 builds
    // (1.0f/width, 1.0f/height, 0, 0) -- `fdivs f12, f0, f12` / `fdivs f0, f0, f13` @0x823FE6D4/D8
    // with f0 = flt_82001C98 = 1.0f -- and multiplies the whole vector by a splat of flt_82001DA0
    // (`lvx128 v0` on the block whose word 0 is that constant, `vspltw v0, v0, 0` @0x823FE6AC/B4,
    // then `vmulfp128 v0, v13, v0` @0x823FE6EC). BrnPostFx::Render @0x8240A468 inlines exactly the
    // same sequence for the same purpose (@0x8240A574-0x8240A630).
    //
    // ⚠ flt_82001DA0 IS NOT IN scratch/postfx_wave1_dossiers/DATA_DUMP.md -- 0x82001DA0 needs to be
    // added to the next dump. It is NOT a placeholder: the value is settled in-image, twice, by
    // functions where a load from it is stored with no arithmetic in between and Hex-Rays renders
    // the store as a literal --
    //   BrnDirector::Camera::Utils::Looker::Parameters::Construct @0x821F8D80:
    //     `lfs f0, flt_82001DA0@l(r10)` @0x821F8E28 -> `stfs f0, 0x40(r3)` @0x821F8E30 and
    //     `stfs f0, 0x50(r3)` @0x821F8E34, pseudocode `*(result + 64) = 0.5;` / `*(result + 80) = 0.5;`
    //   BrnDirector::Camera::BehaviourRig::Parameters::Construct @0x821F9680:
    //     `lfs f10, flt_82001DA0@l(r11)` @0x821F96C8 -> `stfs f10, 0xEC(r3)` @0x821F96D0,
    //     pseudocode `*(result + 236) = 0.5;`
    // 0x40 == 64, 0x50 == 80, 0xEC == 236. Two unrelated functions, three stores, one value.
    const f32 KF_HALF_PIXEL = 0.5f;

    // The multisampled path always drives TWO EDRAM tiles. IMMEDIATE in both functions:
    // `li r5, 2` @0x823FFB50 (BeginTiling's Count) and `cmplwi cr6, r31, 2` @0x823FFD2C (the resolve
    // loop bound). Neither reads BrnGraphics::KMSAA_TILING_PLAN.mu32NumTiles, which happens to hold
    // the same 2 -- the immediate is what the binary does and the immediate is what is reproduced.
    const u32 KU_NUM_MSAA_TILES = 2u;

    // Two predication bits per tile (`li r24, 3` @0x823FFC80, shifted by 2*tile via
    // `slwi r11, r31, 1` + `slw r4, r24, r11` @0x823FFC84/8C).
    const u32 KU_PREDICATION_BITS_PER_TILE = 3u;

    // The two D3DRESOLVE_* masks, exactly as the X360 immediates (`li r4, 0x14` @0x823FFCDC /
    // @0x823FFDCC and `li r4, 0x300` @0x823FFD18 / @0x823FFE08).
    //
    // WHAT THE BITS MEAN, separated by strength of evidence -- the PC leaf must key off the two mask
    // VALUES, not off my decomposition:
    //   0x04  = "the depth/stencil surface". PROVEN INSIDE THIS IMAGE:
    //           renderengine::PixelBuffer::Xbox2ResolveTo @0x82B62300 does `ori r28, r28, 4`
    //           (@0x82B62358) on exactly the branch where the surface kind is 1 == depth-stencil.
    //   0x100 / 0x200 = the colour and depth/stencil CLEARS. SUPPORTED INSIDE THIS IMAGE: the 0x300
    //           call is the one that passes a non-null clear colour (`addi r10, r1, var_70`
    //           @0x823FFCF8) while the 0x14 call passes null (`li r10, 0` @0x823FFCC0). A resolve
    //           that took no clear colour would have no use for one.
    //   0x10  = FRAGMENT0 (take sample 0 rather than averaging, because depth samples cannot be
    //           averaged). INTERPRETATION from the documented Xenon D3DRESOLVE_* set -- an external
    //           platform API, NOT recovered from this image. Nothing below depends on it.
    const u32 KU_RESOLVE_DEPTH_STENCIL_FRAGMENT0 = 0x14u;
    const u32 KU_RESOLVE_COLOUR_AND_CLEAR        = 0x300u;

    // The Xenon D3DPOINT (destination corner) D3DDevice_Resolve takes: two dwords, x then y, built
    // on the stack (`stw r10, var_80` / `stw r11, var_7C` @0x823FFCAC/B0 and the zero pair
    // @0x823FFD7C/D84). TU-local on purpose, exactly like the ViewportF / ScissorRect argument
    // blocks that CgsRenderTarget.cpp, BrnShadowMapRenderManager.cpp and rwgpfxrendertarget.cpp each
    // keep file-local: it is a call-argument block, not a shared type. If a second TU ever needs it,
    // it moves to Xbox2SurfaceShims.h beside the shim that consumes it -- it does not get copied.
    struct XenonPoint
    {
        s32 miX;   // +0x00
        s32 miY;   // +0x04
    };

    // The render-target-state bind BeginRenderAntiAliased performs in BOTH of its branches: take
    // section 0's RenderTargetState off the target's post-fx render target and install it, skipping
    // the device call when it is already the installed one.
    //
    // WHAT THIS IS, per the DWARF: BrnGraphicsUnity.cpp:4597 lists shadow::Device::SetState TWICE
    // inside BeginRenderAntiAliased -- exactly the two inlined instances the X360 shows at
    // 0x823FFB20-0x823FFB34 and 0x823FFBB4-0x823FFBC8. So the original operation is
    // shadow::Device::SetState(const renderengine::RenderTargetState*). It has NO home in this tree:
    // shadowingdevice.h declares only SetState(void*, u32 luSamplerId) (line 58). This function is a
    // disclosed TU-LOCAL stand-in for that missing overload -- named for the operation rather than
    // claiming the DWARF name at the wrong scope -- and it is written once instead of twice because
    // AGENTS.md requires inlining reversal. DELETE it and call shadow::Device::SetState the day that
    // overload lands (the proposal, with its four other open-coded copies, is in this task's notes).
    //
    // IT IS DELIBERATELY NOT CgsRenderTarget::SetRenderTargetState @0x827E7588, and the difference is
    // itself part of the seam: that function ALSO sets the viewport and scissor to the target's full
    // extent and falls back to postfx::gpDefaultRenderTargetState when the section state is null.
    // BeginRenderAntiAliased does neither -- 0x823FFB08-0x823FFB34 contains no D3DDevice_SetViewportF,
    // no D3DDevice_SetScissorRect and no null test -- because on the Xenos the viewport comes from the
    // tile rects BeginTiling is handed. (The DWARF's PS3 body DOES call
    // CgsRenderTarget::SetRenderTargetState; the X360 asm does not, and rung 1 arbitrates.)
    //
    // THE CACHE WORD IS X360 dword_83010A30 and it is reached at its ONE canonical host home,
    // renderengine::gpLastRenderTargetState (declared ShadowPassPCLeaf.h:52, defined
    // PostFxRenderTargetPCLeaf.cpp:463) -- the same variable CgsRenderTarget::SetRenderTargetState*
    // and rw::graphics::postfx::RenderTarget::Begin read. No second copy is minted here. (There IS a
    // pre-existing second host home for that word, shadow::Device::muMisc30, which has a WRITE and no
    // readers; it is recorded as an open defect in REPORT.md and is deliberately NOT touched by this
    // wave, because shadowingdevice.cpp is mounted and ResetShadowing runs per frame.)
    //
    // THE STORE IS INSIDE THE BRANCH, and that is faithful, not tidied: the asm's `stw r29,
    // dword_83010A30` @0x823FFB34 sits AFTER the `beq` at 0x823FFB28, i.e. on the not-equal path
    // only. This is the compare-and-SKIP shape, NOT the unconditional-write-back shape of the
    // blend/depth/rasterizer cached-state setters elsewhere in this wave -- do not "harmonise" them.
    void ShadowedSetRenderTargetState(CgsRenderTarget* lpTarget)
    {
        const renderengine::RenderTargetState* const lpState =
            lpTarget->GetRenderTarget()->GetSectionRenderTargetState(0);
        if (renderengine::gpLastRenderTargetState != lpState)
        {
            renderengine::Device::SetState(lpState);
            renderengine::gpLastRenderTargetState = lpState;
        }
    }

    // ============================================================================================
    // [FLAG PC bring-up] THE UNCONDITIONAL SURFACE HANDOFF -- put the SWAP CHAIN back on the device.
    //
    // WHY THIS EXISTS AS ITS OWN FUNCTION. BeginRenderAntiAliased binds the anti-alias buffer's
    // colour+depth surfaces and NOTHING in this tree unbinds them -- that is EndRenderAntiAliased
    // @0x82408B00, which is declaration-only (BrnRendererModule.h:385). Until it lands, the only
    // code that rebinds the back buffer is renderengine::PCSceneBlit_Begin
    // (XenonD3D9Shims.cpp), and that call used to sit BELOW the present blit's two
    // "is there anything to present" guards. So a frame that tripped either guard left the scene
    // target bound for the entire 2D/GUI tail: the loading screen, the Apt GUI, the movie and the
    // debug HUD would all be drawn off-screen and the frame would present the black
    // renderengine::Device::FrameBegin clear. THE INVARIANT THIS RESTORES: if
    // BeginRenderAntiAliased bound the scene target this frame, something puts the back buffer back
    // this frame -- on EVERY path, including both early returns and the BRN_WORLD_ONLY early-out in
    // Render (which sits after the blit call).
    //
    // WHY Begin+End AND NOT A NEW SHIM. PCSceneBlit_Begin does the rebind FIRST (step 1: GetBackBuffer
    // -> SetRenderTarget(0, backbuffer) -> SetDepthStencilSurface(nullptr) -> invalidate
    // renderengine::gpLastRenderTargetState) and only THEN saves and overwrites the sixteen device
    // states the quad needs; PCSceneBlit_End restores exactly those sixteen and deliberately leaves
    // the back buffer bound. So the pair, run back to back with no draw between, is precisely "hand
    // the swap chain back and change nothing else" -- and it needs no new symbol, no new
    // declaration, and no edit to another wave's file. (The alternative the verifier floated --
    // hoisting PCSceneBlit_Begin out to bracket the whole call from Render -- would BREAK the blit:
    // Im2d::BeginRendering re-enables alpha blending and SetTexture sets the stage ops to MODULATE,
    // so the blit state has to be installed AFTER both, which is why the call sits where it does
    // inside the quad path below.)
    //
    // RETIRED BY EndRenderAntiAliased @0x82408B00: delete this the day that body is mounted, since
    // it is the console's own surface handoff.
    void PCBringUpHandBackTheBackBuffer()
    {
        renderengine::PCSceneBlit_Begin();
        renderengine::PCSceneBlit_End();
    }

    // ============================================================================================
    // [FLAG PC bring-up] PRESENT THE SCENE TARGET -- one full-screen textured quad that puts the
    // off-screen scene colour back on the back buffer before the 2D/GUI tail.
    //
    // ⚠ THIS IS NOT THE POST-FX COMPOSITE. BrnPostFx::Render @0x8240A468 is what really consumes the
    // resolved scene (tone map, bloom, depth of field, motion blur, the colour grade) and it is the
    // NEXT wave. This exists because of a hard sequencing fact: the moment
    // BRN_ANTIALIAS_BRACKET_AVAILABLE goes to 1, BeginRenderAntiAliased binds the anti-alias buffer
    // and the world stops drawing into the swap chain. If nothing hands the result back, the screen
    // is the black renderengine::Device::FrameBegin cleared it to and the wave reads as a
    // regression. RETIRED BY BrnPostFx::Render -- delete this function, its call in Render,
    // renderengine::PCSceneBlit_Begin/_End and their definitions in XenonD3D9Shims.cpp together.
    //
    // WHICH TARGET IT READS, and why it is the DOWN-SAMPLE buffer: BeginRenderAntiAliased binds
    // mapRenderTarget[0] (GetAntiAliasBuffer) section 0, so that is the surface the world renders
    // INTO; ResolveMSAA then copies it into mapRenderTarget[4] (GetDownSampleBuffer) and CLEARS the
    // anti-alias buffer behind the copy -- the clear the console folded into its 0x300 resolve, and
    // the only clear the untiled path has. Both halves are real as of the PC bodies of
    // renderengine::D3DDevice_Resolve (XenonD3D9Shims.cpp), so by the time this runs the anti-alias
    // buffer has already been cleared for the NEXT frame and the finished frame lives in the
    // down-sample buffer. That is also the surface the real composite (BrnPostFx::Render
    // @0x8240A468) reads, as the console does -- so this blit is already reading what retires it.
    //
    // ⚠ ORDER: this therefore has to run AFTER ResolveMSAA. If it runs before, it presents an
    // un-resolved (previous-frame or never-written) down-sample buffer, and ResolveMSAA then finds
    // the swap chain bound and refuses its clear (D3DDevice_Resolve logs "[postfx-resolve]
    // ResolveMSAA ran with the SWAP CHAIN bound"), which leaves the scene depth uncleared.
    //
    // THE HALF-TEXEL OFFSET IS THE ONE THING THAT IS EASY TO GET WRONG HERE, and getting it wrong
    // does NOT produce an obvious failure -- it produces a picture that is uniformly, slightly soft,
    // which reads as "the render target format is a bit lossy" rather than as a bug. Two separate
    // claims, at two different strengths:
    //   * FACT, from the image: the console's own offset for a render target is
    //     (0.5/width, 0.5/height, 0, 0) -- rw::graphics::postfx::RenderTarget::GetHalfPixelOffset
    //     @0x823FE668 reads width/height from the target (+0x04 / +0x08, `lwz r9, 4(r4)` /
    //     `lwz r8, 8(r4)`), reciprocates them against 1.0f and scales by 0.5f. BrnPostFx::Render
    //     @0x8240A468 inlines the identical sequence and feeds the result to the post-fx passes.
    //     So the MAGNITUDE below is recovered, not chosen.
    //   * INTERPRETATION, from the Direct3D 9 rasterisation rule and NOT from this image: that the
    //     offset is ADDED to the UVs of a screen-space quad. D3D9 places a pixel's sample point at
    //     integer screen coordinates while texel i's centre is at texel coordinate i + 0.5, so a
    //     0..W quad with 0..1 UVs samples exactly on texel BOUNDARIES and a LINEAR fetch averages
    //     two texels everywhere. Adding 0.5/W to u and 0.5/H to v moves every sample onto a texel
    //     centre. (This is the same correction as shifting the vertices by -0.5 px, done in UV space
    //     instead -- which is the right place here, because ImRenderer<V>::Render multiplies the
    //     LOGICAL 1280x720 position by gDisplayWidth/1280 before it reaches the device, so a
    //     position-space shift would be scaled by the display ratio and a UV-space one is not.)
    // TRIPWIRE, so this is checkable rather than argued: at the sizing this runs at -- the scene
    // target is created at renderengine::gDisplayWidth x gDisplayHeight
    // (BrnRendererMemory::PCBringUpCreatePostFxSceneTargets) and the swap chain is the same extent
    // (device.cpp:90-91), i.e. exactly 1:1 -- a CORRECT offset makes the LINEAR filter land on texel
    // centres and return each texel unblended. So a correct blit is pixel-exact and a wrong one is
    // softly blurred EVERYWHERE. If the frame looks soft, this sign or this denominator is wrong;
    // do not reach for the filter state.
    void PCBringUpBlitSceneTargetToBackBuffer(BrnRendererMemory& lrRendererMemory,
                                              CgsGraphics::Im2d* lpIm2d)
    {
        CgsRenderTarget* const lpSceneTarget = lrRendererMemory.GetDownSampleBuffer();
        if (lpSceneTarget == 0 || lpIm2d == 0)
        {
            // No scene target, or no immediate renderer to draw the quad with -- but the world was
            // still redirected off-screen by BeginRenderAntiAliased, so the swap chain MUST come
            // back before the 2D/GUI tail or the GUI is drawn where nobody will ever see it.
            PCBringUpHandBackTheBackBuffer();
            return;
        }

        renderengine::Texture* const lpSceneTexture = lpSceneTarget->GetTexture(0u);
        const u32 luSceneWidth  = lpSceneTarget->GetWidth();
        const u32 luSceneHeight = lpSceneTarget->GetHeight();
        if (lpSceneTexture == 0 || luSceneWidth == 0u || luSceneHeight == 0u)
        {
            // The lazy pool has not built the target yet -- nothing to present. Same reasoning as
            // the guard above: no quad, but the surface handoff is NOT optional.
            PCBringUpHandBackTheBackBuffer();
            return;
        }

        // The console's own half-pixel offset for THIS target (see the banner above): the same
        // 0.5/width, 0.5/height GetHalfPixelOffset @0x823FE668 computes, from the same two
        // dimensions it reads off the render target.
        const f32 lfHalfTexelU = KF_HALF_PIXEL / static_cast<f32>(luSceneWidth);
        const f32 lfHalfTexelV = KF_HALF_PIXEL / static_cast<f32>(luSceneHeight);

        // The Im2d path's logical screen space -- the PC leaf scales these to the real back buffer
        // (CgsIm2d.cpp:50-51, :182-183). Host convention, not a console constant; the same pair the
        // existing quads in this file use (RenderThreeThreadMonitors :98-99, the movie underlay
        // :1384).
        const f32 KF_LOGICAL_WIDTH  = 1280.0f;
        const f32 KF_LOGICAL_HEIGHT = 720.0f;

        // Opaque white. The stage ops PCSceneBlit_Begin installs are SELECTARG1(TEXTURE), so this
        // colour is not read -- it is set so that a future regression to a MODULATE stage degrades
        // to "modulated by white" (i.e. still correct) rather than to black.
        const CgsGraphics::RGBA8 KC_OPAQUE_WHITE = { 255, 255, 255, 255 };

        // TL, TR, BL, BR -- the same 4-vertex triangle-strip order as EmitColouredQuad in this file.
        CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
        const f32 lafPos[4][2] = { { 0.0f,              0.0f              },
                                   { KF_LOGICAL_WIDTH,  0.0f              },
                                   { 0.0f,              KF_LOGICAL_HEIGHT },
                                   { KF_LOGICAL_WIDTH,  KF_LOGICAL_HEIGHT } };
        const f32 lafUV[4][2]  = { { 0.0f + lfHalfTexelU, 0.0f + lfHalfTexelV },
                                   { 1.0f + lfHalfTexelU, 0.0f + lfHalfTexelV },
                                   { 0.0f + lfHalfTexelU, 1.0f + lfHalfTexelV },
                                   { 1.0f + lfHalfTexelU, 1.0f + lfHalfTexelV } };
        for (s32 liVertex = 0; liVertex < 4; ++liVertex)
        {
            laVerts[liVertex].mv2Pos    = { lafPos[liVertex][0], lafPos[liVertex][1] };
            laVerts[liVertex].mv2Tex0UV = { lafUV[liVertex][0], lafUV[liVertex][1] };
            laVerts[liVertex].mv4Colour = KC_OPAQUE_WHITE;
        }

        // ORDER IS LOAD-BEARING. BeginRendering re-enables alpha blending and SetTexture sets the
        // stage ops to MODULATE, so PCSceneBlit_Begin has to run AFTER both or its opaque
        // texture-only state is immediately overwritten and the quad draws with the scene's alpha
        // (which BeginRenderAntiAliased clears to 0) -- i.e. invisible. See ShadowPassPCLeaf.h.
        lpIm2d->BeginRendering();
        lpIm2d->SetTexture(lpSceneTexture);
        renderengine::PCSceneBlit_Begin();
        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);   // triangle strip
        renderengine::PCSceneBlit_End();
        lpIm2d->EndRendering();
    }
}

// 0x823FFA18 -- open the frame's ANTI-ALIASED scene pass.
//
// Member identification (offset authority = the X360 asm; every member is reached BY NAME):
//   this+0xC400 -> mbMultisampledBackbuffer      this+0xC434 -> mbGreyBackgroundColour
//   this+0xC4D0 -> mvBackgroundColour (.x/.y/.z/.w at 0xC4D0/D4/D8/DC)
//   this+0xC9C4 -> mGpuMonitors.miScreenClear    this+0x238  -> mapRenderTarget[0] (GetAntiAliasBuffer())
// The three flag/vector offsets are pinned by walking the committed header's member order from
// 0xC400 (four flag bytes + s32 + f32 + the 6-byte RenderSwitches + 19 bools + 3 dwords lands
// mbGreyBackgroundColour at exactly 0xC434; continuing through the counters, macScreenShotText[32]
// and the four 16-byte light Vector3s lands mvBackgroundColour at exactly 0xC4D0), and PrepareAgain's
// already-committed note (BrnRendererModule.h:392) corroborates from the other side by putting the
// two cloud textures at 0xC4E0 / 0xC4E4, i.e. immediately after that Vector4.
// The monitor is pinned arithmetically: `addis r31, r31, 1; addi r31, r31, -0x363C` @0x823FFB64/68
// gives +0xC9C4, ResolveMSAA's gives +0xC9E4, and 0x20 == 8 dwords == BrnGpuMonitors index 0 ->
// index 8, which the committed header spells miScreenClear (line 264) -> miDownsampleMSAAAndCompParticles
// (line 272) with exactly eight members between them.
void BrnRendererModule::BeginRenderAntiAliased(f32 lfWhiteLevel, bool lbClearStencil,
                                               u8 luStencilClearValue)
{
    // lbClearStencil is NOT read by the X360 body: nothing in 0x823FFA18-0x823FFBD8 touches r5, and
    // the only appearance of r5 in the whole listing is `li r5, 2` -- an OUTGOING argument. On this
    // platform the clear rides inside the tiling pass, which clears colour + Z + stencil and takes
    // the stencil value unconditionally. The flag gates a stencil clear on the PS3/GCM path only,
    // which the DWARF body hint shows as ClearDepthStencilParameters::SetStencil inside a
    // ClearColorParameters / ClearDepthStencilParameters block the X360 does not have. Reproduced as
    // unused, never repurposed.
    (void)lbClearStencil;

    // This frame's background colour, scaled by the white level. Grey mode drives all three channels
    // off one constant; otherwise it is the pale-blue tint. Alpha is always 0. (DWARF: the first of
    // the two rw::math::vpu::Vector4::Set calls it lists.)
    //
    // WRITTEN BY LANE, not through a setter, and that is a PC-vocabulary fact rather than a change
    // of behaviour: this tree's rw::math::vpu::Vector4 (vendor rw/math/vpu/types.h:26) is the
    // portable reconstruction of the console's single 16-byte VectorIntrinsic -- an alignas(16) POD
    // of four named floats x/y/z/w whose ONLY member function is SetZero(). The SDK's SIMD
    // Set/GetX..GetW live in the rwmath *_operation headers, which that vendor header's own banner
    // says are deliberately not reproduced ("The SIMD operations live in the SDK's *_operation
    // headers and are not reproduced here"). Same four lanes, same order, same alignment; the
    // console's `stvx128 v0, r0, r11` @0x823FFAC4 is one 16-byte store either way.
    if (mbGreyBackgroundColour)
    {
        const f32 lfGrey = lfWhiteLevel * KF_BACKGROUND_COLOUR_GREY;
        mvBackgroundColour = Vector4{ lfGrey, lfGrey, lfGrey, KF_BACKGROUND_COLOUR_ALPHA };
    }
    else
    {
        mvBackgroundColour = Vector4{ lfWhiteLevel * KF_BACKGROUND_COLOUR_RED,
                                      lfWhiteLevel * KF_BACKGROUND_COLOUR_GREEN,
                                      lfWhiteLevel * KF_BACKGROUND_COLOUR_BLUE,
                                      KF_BACKGROUND_COLOUR_ALPHA };
    }

    // ...then read straight back OUT of the member into the D3DVECTOR4 the clear consumes. This is
    // the DWARF's SECOND rw::math::vpu::Vector4::Set, and the asm shows why the pair is not
    // redundant: the branches above build four floats in the stack block var_40..var_34, publish
    // them to mvBackgroundColour with one `stvx128 v0, r0, r11` @0x823FFAC4 (r11 = this+0xC4D0), and
    // then reload the four floats back into the SAME stack block component by component
    // (@0x823FFACC-0x823FFAFC, from 0xC4D0/D4/D8/DC), whose address is what BeginTiling is handed
    // (`addi r7, r1, var_40` @0x823FFB48). Modelled as a real rw::math::vpu::Vector4 rather than a
    // bare float[4]: Vector4 owns a single alignas(16) four-lane VectorIntrinsic, so &lvClearColour
    // is the same four contiguous x/y/z/w floats the console passes, with no invented struct.
    // Written and read BY LANE for the reason given at the publish above: this tree's Vector4 has
    // x/y/z/w and SetZero() and nothing else.
    const Vector4 lvClearColour = { mvBackgroundColour.x, mvBackgroundColour.y,
                                    mvBackgroundColour.z, mvBackgroundColour.w };

    if (mbMultisampledBackbuffer)
    {
        void* const lpDevice = renderengine::gpD3DDevice;

        // Bind the anti-alias buffer's surfaces BEFORE opening the tiling pass (asm order:
        // 0x823FFB08-0x823FFB34, then the BeginTiling call).
        ShadowedSetRenderTargetState(mAllocatedRenderTargets.GetAntiAliasBuffer());

        // ...then open the predicated-tiling pass over the two EDRAM tiles, clearing to the
        // background colour / Z 1.0 / the caller's stencil value. The rect list is the MSAA tiling
        // plan's rectangle array -- `addi r6, r11, (unk_8203E088 - 0x8203E080)` @0x823FFB4C, i.e.
        // &KMSAA_TILING_PLAN.maTile[0], the +0x08 rectangle array of the 0x48-byte record. That
        // record already has a home: GameSource/Graphics/BrnAntiAliasTiling.h, landed by the pool
        // wave, which recovered both plans byte-exact AND names these two functions as its readers.
        // The stencil byte is zero-extended into the DWORD parameter (`clrlwi r9, r27, 24`
        // @0x823FFB44).
        renderengine::D3DDevice_BeginTiling(lpDevice, 0u, KU_NUM_MSAA_TILES,
                                            BrnGraphics::KMSAA_TILING_PLAN.maTile,
                                            &lvClearColour, KF_CLEAR_Z, luStencilClearValue);

        // The screen-clear GPU monitor brackets the predication RESET only -- the clear itself rode
        // inside the tiling pass opened above. BeginTiling turns predication on for the tile it
        // opens; clearing the mask submits every following draw to every tile, so the game does not
        // predicate its GEOMETRY, only its resolves (see ResolveMSAA).
#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miScreenClear);
#endif
        renderengine::D3DDevice_SetPredication(lpDevice, 0u);
#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miScreenClear);
#endif
    }
    else
    {
        // Untiled: there is NO clear here at all, and the device pointer is never even loaded (the
        // untiled branch 0x823FFB90-0x823FFBD8 contains no reference to off_83271608). The previous
        // frame's ResolveMSAA already left both EDRAM surfaces cleared -- its colour resolve carries
        // the 0x300 mask -- so opening the pass is just the bind. Note the monitor order flips
        // relative to the tiled branch: StartMonitor comes FIRST here (@0x823FFB9C, before the bind
        // at 0x823FFBA0-0x823FFBC8), which is why the bind cannot be hoisted out of the if.
#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miScreenClear);
#endif
        ShadowedSetRenderTargetState(mAllocatedRenderTargets.GetAntiAliasBuffer());
#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miScreenClear);
#endif
    }
}

// 0x823FFBE0 -- resolve the anti-aliased scene out of EDRAM into the down-sample buffer.
//
// ================================================================================================
// THE ABI DECODE, RESTATED. My previous submission got the argument MAPPING right and the
// JUSTIFICATION wrong: it called the two stack-spilled Resolve arguments "slots 8 and 9 of an area
// based at r1+0x18", which reads as ClearZ's own positional slot plus one. Here is the honest
// derivation, which is stronger than what it replaced because every step is attested more than once.
//
// STEP 1 -- THE OUTGOING-PARAMETER AREA IS ANCHORED AT r1+0x18 WITH EIGHT 8-BYTE GPR HOMES, so the
// first overflow slot is 0x58 and the second 0x60. Proven WITHOUT reference to any other function,
// by frame-size invariance across four attested call sites with four different frames:
//     rw::graphics::postfx::Target::Resolve       @0x823F9118  frame 0x70  `stw r5, 0x70+var_14` -> 0x5C
//     rw::graphics::postfx::RenderTarget::Resolve @0x823F9338  frame 0x80  `stw r5, 0x80+var_24` -> 0x5C
//     renderengine::PixelBuffer::Xbox2ResolveTo   @0x82B62300  frame 0xE0  -> 0x5C and 0x64
//     BrnRendererModule::ResolveMSAA              @0x823FFBE0  frame 0xF0  -> 0x5C and 0x64
// Four frame sizes, one pair of absolute offsets. Only a fixed base can do that. (It also matches
// the independent homing evidence in BrnRendererMemory::Construct @0x823FCA44-0x823FCA6C, where
// `std r4..r10` lands at 0x20/0x28/.../0x50 -- stride 8 from a base of 0x18.) The stores are at 0x5C
// and 0x64 rather than 0x58 and 0x60 because a 32-bit `stw` into a big-endian 8-byte slot is
// right-justified at slot+4 -- the same convention Construct reads back with `arg_8C` = 0x88+4 for a
// word and `arg_97` = 0x90+7 for a byte.
//
// STEP 2 -- WITHIN r3-r10, A FLOAT ARGUMENT CONSUMES ITS POSITIONAL GPR AND LEAVES IT UNWRITTEN.
// D3DDevice_EndTiling settles this and needs no interpretation: at both attested call sites r8 is
// never written in the call block (here it still holds the stale 0xC4DC displacement from
// @0x823FFC14; in Xbox2ResolveTo likewise), ClearZ rides f1, and the argument AFTER ClearZ lands in
// r9. So EndTiling is (pDevice, ResolveFlags, pResolveRects, pDestTexture, pClearColor, ClearZ,
// ClearStencil, pParameters) -- eight arguments, r10 = pParameters = 0 at both sites. IDA's own
// "pParameters" comment on r9 @0x823FFD44 is therefore off by one, which is what my previous
// submission said and remains true.
//
// STEP 3 -- PAST r10, AN FPR-PASSED ARGUMENT RESERVES NO STACK SLOT; the remaining arguments pack
// from 0x58 upward. This is the correction, and the discriminating evidence is that both
// postfx::Target::Resolve and postfx::RenderTarget::Resolve write their LAST argument at 0x5C, i.e.
// into the FIRST overflow slot, while ClearZ (`lfs f1, flt_82001C98`) rides f1. Had ClearZ reserved
// slot 0x58 the following argument would have gone to 0x60/0x64 instead. So for D3DDevice_Resolve:
//     r3..r10 = args 1-8 (pDevice, Flags, pSourceRect, pDestTexture, pDestPoint, DestLevel,
//               DestSliceOrFace, pClearColor)
//     f1      = arg 9  ClearZ,        no stack slot
//     0x58    = arg 10 ClearStencil   (written as a word at 0x5C)
//     0x60    = arg 11 pParameters    (written as a word at 0x64, zero everywhere)
// This is exactly the emitted argument order below; only the reasoning changed.
//
// STEP 4 -- THE VALUE CHAIN CONFIRMS THAT arg 10 AND EndTiling's r9 ARE THE SAME PARAMETER.
// In Xbox2ResolveTo the ONE incoming value read from `arg_5C` is forwarded to EndTiling's r9
// (@0x82B623A0) on one branch and stored to the Resolve call's 0x5C (@0x82B62444) on the other -- the
// same value into both positions. In ResolveMSAA the stencil argument (r26) goes to exactly those two
// places as well (`stw r26, 0x5C` @0x823FFCBC/@0x823FFCFC and `mr r9, r26` @0x823FFD44). Two
// unrelated functions, the same pairing: EndTiling's r9 == Resolve's arg 10 == ClearStencil.
// ================================================================================================
//
// PLATFORM-NEUTRAL half: none. Every call this function makes is EDRAM mechanics -- the resolve IS
// the copy out of EDRAM, and on PC the scene target already is the texture the rest of the frame
// samples. CONSOLE-ONLY half: all of it.
// WHAT THE PC LEAF STILL OWES, because the console folded it in here: the CLEAR. The colour resolve
// carries the 0x300 mask and a live clear colour, and on the untiled path it is the ONLY clear in the
// whole bracket (BeginRenderAntiAliased's untiled branch clears nothing, deliberately). A PC shim
// that no-ops Resolve outright must still clear colour to mvBackgroundColour and Z to 1.0, or nothing
// ever clears the scene target.
//
// Member identification (offset authority = the X360 asm; reached BY NAME):
//   this+0xC400 -> mbMultisampledBackbuffer   this+0xC4D0 -> mvBackgroundColour
//   this+0xC9E4 -> mGpuMonitors.miDownsampleMSAAAndCompParticles (0x20 == 8 dwords past
//                  miScreenClear, i.e. BrnGpuMonitors index 8, which the committed header spells
//                  exactly eight members after index 0)
//   this+0x248  -> mapRenderTarget[4] (GetDownSampleBuffer()). The base and the slot numbering are
//                  corroborated from the other side by CreateBackBuffer, which reads
//                  mapRenderTarget[4] and asserts "GetDownSampleBuffer() != NULL"; 0x248 - 0x238 =
//                  0x10 = four slots past GetAntiAliasBuffer's slot 0.
void BrnRendererModule::ResolveMSAA(f32 lfWhiteLevel, u8 luStencilValue)
{
    // lfWhiteLevel is NOT read by the X360 body. The white level was already baked into
    // mvBackgroundColour by BeginRenderAntiAliased -- which is exactly what this function reads back
    // -- and every ClearZ here is the constant 1.0f (flt_82001C98, loaded into f31 @0x823FFC78 /
    // @0x823FFDAC and `fmr f1, f31`'d into place before each call, OVERWRITING the incoming f1).
    // Reproduced as unused, never repurposed.
    (void)lfWhiteLevel;

    // The colour the resolve leaves the EDRAM colour surface at, ready for the next pass: this
    // frame's background colour, read back component by component out of the member (the four `lfsx`
    // from this+0xC4D0/D4/D8/DC @0x823FFC18-0x823FFC48 into the stack block var_70..var_64). Same
    // rw::math::vpu::Vector4 modelling as BeginRenderAntiAliased, for the same reason: one
    // alignas(16) four-lane VectorIntrinsic is the four contiguous x/y/z/w floats the console passes.
    // Written and read BY LANE, exactly as in BeginRenderAntiAliased: this tree's Vector4 exposes
    // x/y/z/w and SetZero() only.
    const Vector4 lvClearColour = { mvBackgroundColour.x, mvBackgroundColour.y,
                                    mvBackgroundColour.z, mvBackgroundColour.w };

    void* const lpDevice = renderengine::gpD3DDevice;

    if (mbMultisampledBackbuffer)
    {
#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miDownsampleMSAAAndCompParticles);
#endif

        for (u32 luTile = 0; luTile < KU_NUM_MSAA_TILES; ++luTile)
        {
            // Predicate the pair of resolves below so each executes only during its own tile's
            // replay of the command stream (two predication bits per tile).
            renderengine::D3DDevice_SetPredication(lpDevice,
                                                   KU_PREDICATION_BITS_PER_TILE << (2u * luTile));

            // The tile's screen rectangle, out of the MSAA tiling plan's rectangle array. The asm
            // walks it as base + 16*tile + 8 (`slwi r11, r31, 4` / `add r11, r11, r25` /
            // `addi r27, r11, 8` @0x823FFC94-0x823FFCA0, r25 = &unk_8203E080) -- i.e. maTile[tile] of
            // the 0x48-byte record whose committed home is GameSource/Graphics/BrnAntiAliasTiling.h.
            const BrnGraphics::AntiAliasTilingPlan::TileRect& lrTile =
                BrnGraphics::KMSAA_TILING_PLAN.maTile[luTile];

            // Each tile lands back at its own screen position: the destination point is the tile
            // rect's top-left corner. The asm reads the rect's FIRST TWO dwords -- `lwz r10, 8(r11)`
            // and `lwz r11, 0xC(r11)` @0x823FFCA4/A8, i.e. maTile[tile].mu32Left / .mu32Top -- and
            // stores them as the two-dword point @0x823FFCAC/B0.
            const XenonPoint lDestPoint = { static_cast<s32>(lrTile.mu32Left),
                                            static_cast<s32>(lrTile.mu32Top) };

            // Depth/stencil first, fragment 0 only. pClearColor is NULL on this one (`li r10, 0`
            // @0x823FFCC0) -- a depth resolve has no colour to clear to.
            renderengine::D3DDevice_Resolve(lpDevice, KU_RESOLVE_DEPTH_STENCIL_FRAGMENT0, &lrTile,
                                            mAllocatedRenderTargets.GetDownSampleBuffer()->GetDepthTexture(),
                                            &lDestPoint, 0u, 0u, nullptr,
                                            KF_CLEAR_Z, luStencilValue, nullptr);

            // ...then colour target 0 with the samples averaged (that IS the downsample), clearing
            // both EDRAM surfaces behind it to the background colour / Z 1.0 / the caller's stencil.
            // The down-sample buffer is re-fetched for the second resolve exactly as the console does
            // (`lwz r3, 0x248(r30)` appears once per resolve, @0x823FFC98 and @0x823FFCEC).
            renderengine::D3DDevice_Resolve(lpDevice, KU_RESOLVE_COLOUR_AND_CLEAR, &lrTile,
                                            mAllocatedRenderTargets.GetDownSampleBuffer()->GetTexture(0u),
                                            &lDestPoint, 0u, 0u, &lvClearColour,
                                            KF_CLEAR_Z, luStencilValue, nullptr);
        }

        // StopMonitor comes BEFORE EndTiling (@0x823FFD38 then @0x823FFD5C) -- the tiling close is
        // outside the downsample monitor's bracket. Order preserved.
#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miDownsampleMSAAAndCompParticles);
#endif

        // Close the tiling pass. Nothing is left to resolve or clear at this level -- the per-tile
        // resolves above did both -- so every pointer argument is null and only ClearZ and the
        // stencil value ride along.
        renderengine::D3DDevice_EndTiling(lpDevice, 0u, nullptr, nullptr, nullptr,
                                          KF_CLEAR_Z, luStencilValue, nullptr);
    }
    else
    {
        // Untiled: one resolve pair over the whole screen, landing at the origin. No predication to
        // set and no tiling pass to close, because none was opened. The destination point is an
        // explicitly ZEROED stack pair (`li r29, 0` @0x823FFD74 then `stw r29, var_78` /
        // `stw r29, var_74` @0x823FFD7C/D84), not the rect's corner.
        const XenonPoint lDestPoint = { 0, 0 };

        // The single full-screen rectangle: `addi r5, r26, (unk_8203E0D0 - 0x8203E0C8)` @0x823FFDC8
        // with r26 = &unk_8203E0C8, i.e. &KNO_MSAA_TILING_PLAN.maTile[0].
        const BrnGraphics::AntiAliasTilingPlan::TileRect& lrScreen =
            BrnGraphics::KNO_MSAA_TILING_PLAN.maTile[0];

#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StartMonitor(mGpuMonitors.miDownsampleMSAAAndCompParticles);
#endif

        renderengine::D3DDevice_Resolve(lpDevice, KU_RESOLVE_DEPTH_STENCIL_FRAGMENT0, &lrScreen,
                                        mAllocatedRenderTargets.GetDownSampleBuffer()->GetDepthTexture(),
                                        &lDestPoint, 0u, 0u, nullptr,
                                        KF_CLEAR_Z, luStencilValue, nullptr);

        renderengine::D3DDevice_Resolve(lpDevice, KU_RESOLVE_COLOUR_AND_CLEAR, &lrScreen,
                                        mAllocatedRenderTargets.GetDownSampleBuffer()->GetTexture(0u),
                                        &lDestPoint, 0u, 0u, &lvClearColour,
                                        KF_CLEAR_Z, luStencilValue, nullptr);

#if BRN_GPU_PERFMON_AVAILABLE
        CgsDev::PerfMonGpu::StopMonitor(mGpuMonitors.miDownsampleMSAAAndCompParticles);
#endif
    }
}

#endif  // BRN_ANTIALIAS_BRACKET_AVAILABLE

// The world/car/sky pass block of Render (@0x8240BFA8 :725+). Pass order and list ids are the
// X360's (see the renderer wave log for the full map):
//   shadow cascades (lists 0,2,1,3,4)  -> LIVE, and no longer here: they run in
//     RenderShadowMapPasses above, called from Render BEFORE this block, which is the
//     console's own order (Render:545-640 vs. the world passes at :725+)
//   env-map faces (lists 5..10)        -> gated OFF on PC (env-map targets)
//   pre-Z (list 21)         -> DispatchAllMeshesZOnly (mbRenderPreZ)
//   CARS OPAQUE  (list 19)  -> DispatchAllMeshes
//   WORLD OPAQUE (list 11)  -> DispatchAllMeshes
//   sky                     -> BrnSkyDomeManager::Render (bring-up gated)
//   WORLD TRANSPARENT (15)  -> DispatchAllMeshes
//   CARS TRANSPARENT  (20)  -> DispatchAllMeshes (blobby shadows gated off)
// Occlusion-query interleaving is the mbOcclusionCull* path (default false).
// The context is the one Render built through BuildDispatchLists, on Render's stack.
void BrnRendererModule::RenderWorldPasses(const BrnGame::DispatchThreadInputBuffer* /*lpDispatchThreadInputBuffer*/,
                                          CgsGraphics::DispatchObjectContext* lpContext)
{
    using namespace CgsGraphics;

    if (mpInterpreter == 0)
        return;

    DispatchObjectContext& lContext = *lpContext;

    // Pass stats (X360 60-frame averages; the raw totals feed the debug HUD).
    const u32 luPreZ             = mSingleBufferedDispatchFrame.GetList(21)->GetCount();
    const u32 luCarOpaque        = mSingleBufferedDispatchFrame.GetList(19)->GetCount();
    const u32 luWorldOpaque      = mSingleBufferedDispatchFrame.GetList(11)->GetCount();
    const u32 luWorldTransparent = mSingleBufferedDispatchFrame.GetList(15)->GetCount();
    const u32 luCarTransparent   = mSingleBufferedDispatchFrame.GetList(20)->GetCount();
    mu32NumWorldOpaqueObjectTotals      += luWorldOpaque;
    mu32NumCarOpaqueObjectTotals        += luCarOpaque;
    mu32NumWorldTransparentObjectTotals += luWorldTransparent;
    mu32NumCarTransparentObjectTotals   += luCarTransparent;

    // [FLAG PC bring-up] One-shot map of where the world producer's records actually
    // landed. The world feed's AddToBin list bytes were flagged provisional by the
    // renderer wave (BrnWorldEntityModule's camera call site), so name every non-empty
    // mesh list the first frame ANY of them is non-empty. DELETE once the producer's
    // list ids are pinned against the X360 call-site asm.
    //
    // ⚠️ LATCHED ON THE VALUE, not on a "printed once" bool (wheel-render wave 2026-08-03).
    // As a pure one-shot this fired on the boot loading screen, ~200 log lines before the
    // first car existed, and then never again -- so it reported "[11] [15] [21]" for the
    // whole run and could NEVER show the car lists 19/20 whatever they did. It now reprints
    // whenever the car-opaque count changes, which is exactly when a body-part or wheel
    // draw appears or disappears.
    {
        static bool sbLoggedLists = false;
        static u32  suLastCarOpaque = 0xFFFFFFFFu;
        if ((!sbLoggedLists || luCarOpaque != suLastCarOpaque) && CgsDev::Log::gpDebugPrint != 0)
        {
            u32 luTotal = 0;
            for (u32 luList = 0; luList < 25u; ++luList)
                luTotal += mSingleBufferedDispatchFrame.GetList(luList)->GetCount();
            if (luTotal != 0)
            {
                sbLoggedLists = true;
                suLastCarOpaque = luCarOpaque;
                *CgsDev::Log::gpDebugPrint << "[FLAG PC bring-up] MESH lists:";
                for (u32 luList = 0; luList < 25u; ++luList)
                {
                    const u32 luCount = mSingleBufferedDispatchFrame.GetList(luList)->GetCount();
                    if (luCount != 0)
                        *CgsDev::Log::gpDebugPrint << " [" << static_cast<s32>(luList)
                                                   << "]=" << static_cast<s32>(luCount);
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }
        }
    }

    const bool lbPreZWork   = (mbRenderPreZ && luPreZ != 0);
    const bool lbOpaqueWork = (mbRenderCarsOpaque && luCarOpaque != 0)
                           || (mbRenderWorldOpaque && luWorldOpaque != 0);
    const bool lbTransparentWork = (mbRenderWorldTransparent && luWorldTransparent != 0)
                                || (mbRenderCarsTransparent && luCarTransparent != 0);

    // [PC bring-up states] The per-pass render states normally come from the
    // technique state groups the walk binds on each technique change
    // (MaterialState -- its porter + the x64 state-object seam are still open).
    // Opaque passes: Z test+write on, no blending. FLAG: replace with the real
    // state-group binds when the MaterialState path lands.
    //
    // Applied ONLY when a pass will actually walk records: with no world data
    // (boot, menus, every frame before the streamer delivers geometry) the whole
    // block leaves the device state untouched, so the 2D/GUI tail below sees
    // exactly the state it saw before this pass existed.
    // PRE-Z (X360 Render @0x8240BFA8: right after BeginRenderAntiAliased, gated on
    // mbRenderPreZ, mesh list 21 walked with DispatchAllMeshesZOnly). The list is fed
    // by DrawRenderable::Interpret's pre-Z re-emit, which only runs when the object
    // context carries mbPreZEnabled and the producer stamped a pre-Z list id.
    if (lbPreZWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(false);
        mSingleBufferedDispatchFrame.GetList(21)->DispatchAllMeshesZOnly(mpInterpreter, &lContext);
    }

    if (lbOpaqueWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(false);

        if (mbRenderCarsOpaque)
        {
            mSingleBufferedDispatchFrame.GetList(19)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
        if (mbRenderWorldOpaque)
        {
            // [FLAG PC bring-up probe] THE CONTROL for the shadow pass's occlusion probe
            // (slot 3). The shadow cascades report thousands of draws and no fragments; the
            // only way to know that is a fact about the cascades and not about the probe is to
            // run the same instrument over a pass that DEMONSTRABLY puts pixels on screen.
            // The world-opaque walk is that pass. Read one frame late like the others, so it
            // costs a query issue and nothing else. DELETE with the shadow bring-up probes.
            renderengine::ShadowProbe_Begin(3u);
            mSingleBufferedDispatchFrame.GetList(11)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
            renderengine::ShadowProbe_End(3u);
        }
    }

    // ---- THE SKY (X360 Render @0x8240BFA8: BrnSkyDomeManager::Render right here, between
    // the opaque and the transparent passes, gated on mbRenderSky). --------------------
    // The dome is camera-centred and 9500 units across, so it is drawn AFTER the opaque
    // geometry and depth-tests against it (its depth/stencil state writes no depth) --
    // it fills only the pixels the city left empty.
    if (mbRenderSky && (lbPreZWork || lbOpaqueWork || lbTransparentWork)
        && gBrnSkyCameraBringUp.mbValid && EnsureSkyDomeBringUp())
    {
        BrnShaderConstantsFrame& lrFrame =
            maShaderConstantsFrames[mu8ShaderConstantsFrameExternal];
        PublishSkyConstantsBringUp(&lrFrame);
        mSkyDome.Render(&mIm3dRendererSkyDome,
                        mpCloudDensity0Texture, mpCloudLighting0Texture,
                        &lrFrame);
        // The sky binds its own blend / raster / depth-stencil states; hand the device
        // back to the pass default so the transparent pass below starts where it expects.
        renderengine::Device::SetWorldPassDefaultStates(false);
    }

    // Transparent passes: Z test on / write off, alpha blend on. Same FLAG.
    if (lbTransparentWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(true);

        if (mbRenderWorldTransparent)
        {
            mSingleBufferedDispatchFrame.GetList(15)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
        if (mbRenderCarsTransparent)
        {
            mSingleBufferedDispatchFrame.GetList(20)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
    }

    // Back to the opaque default before the 2D overlay tail (the Im2d path re-sets
    // its own states, so this only matters for the frames a world pass ran).
    if (lbPreZWork || lbOpaqueWork || lbTransparentWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(false);
    }
}

// @ 0x823FF8F8 - BrnRendererModule::PrepareAgain. Store the five global textures
// GamePrepare's stage-3 acquires resolved. The X360 body is five stores; the two cloud
// slots (this+0xC4E0 / +0xC4E4) are the pair Render passes to BrnSkyDomeManager::Render,
// which hard-returns if either is null.
void BrnRendererModule::PrepareAgain(renderengine::Texture* lpBlobbyShadow,
                                     renderengine::Texture* lpCloudDensity,
                                     renderengine::Texture* lpCloudLighting,
                                     renderengine::Texture* lpCoronaAtlas,
                                     renderengine::Texture* lpGlassFracture)
{
    mpBlobbyShadowTexture   = lpBlobbyShadow;
    mpCloudDensity0Texture  = lpCloudDensity;
    mpCloudLighting0Texture = lpCloudLighting;
    mpGlassFractureTexture  = lpGlassFracture;
    // The corona atlas' slot is the CoronaManager's, not one of this module's members --
    // the X360 hands it on rather than storing it. The corona pass is not live, so it is
    // accepted and dropped here; wire it when BrnCoronaManager comes online.
    (void)lpCoronaAtlas;
}

// =============================================================================
// [FLAG PC bring-up] The sky-dome bring-up pair. NEITHER is an X360 function.
// =============================================================================
//
// The console builds the sky renderer in BrnRendererModule::Construct and its geometry in
// Prepare, and fills the per-frame constants in WorldModule::SetupShaderConstantsBeforeRendering
// @0x827D1410 from EnvironmentManager::GenerateShaderConstants @0x827D0098.
//
// Two reasons the Construct/Prepare pair is deferred to the first world frame instead:
//   * both need a live IDirect3DDevice9 (the vertex descriptor becomes a D3D9 vertex
//     declaration and the programs become D3D9 shader objects), and the renderer module is
//     constructed before the device exists;
//   * BrnRendererModule::Prepare is not reconstructed at all yet.
//
// DELETE both when the environment manager publishes for real.

// [FLAG PC bring-up] see BrnShaderConstantsFrame.h. Written by
// WorldModule::GenerateDispatchListsBringUp once per dispatch frame.
BrnSkyCameraBringUp gBrnSkyCameraBringUp = { {}, {}, false };

bool BrnRendererModule::EnsureSkyDomeBringUp()
{
    if (mbSkyDomeReady)
        return true;
    if (mbSkyDomeTried)
        return false;
    if (renderengine::gDevice == 0 || !EnsureWorldDispatchAllocator())
        return false;          // retry next frame -- the device arrives later than Construct

    mbSkyDomeTried = true;

    // X360 Construct: mIm3dRendererSkyDome.Construct(mpGraphicsAllocator) -- builds the
    // 20-byte sky vertex descriptor, uploads the one vertex/pixel program pair and resolves
    // the seventeen named constants on it.
    mIm3dRendererSkyDome.Construct(&sWorldDispatchAllocator);
    if (!mIm3dRendererSkyDome.HasPrograms())
    {
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint
                << "[Sky] sky-dome programs unavailable - the sky pass stays off\n";
        return false;
    }

    // X360 Prepare: mSkyDome.Construct() + mSkyDome.Prepare(&renderer, allocator) -- the
    // 22x45 main dome and the 5x10 env-map dome.
    mSkyDome.Construct();
    mSkyDome.Prepare(&mIm3dRendererSkyDome, &sWorldDispatchAllocator);

    mbSkyDomeReady = true;
    if (CgsDev::Log::gpDebugPrint != 0)
        *CgsDev::Log::gpDebugPrint << "[Sky] sky dome constructed + prepared\n";
    return true;
}

// The per-frame sky/cloud constants.
//
// The values are the SHIPPED environment keyframe ENV_KF_Paradise_ingame_junk_city_1200
// (build/game/ENVIRONMENTSETTINGS/PARADISE_INGAME_JUNK.BUNDLE -- the timeline pins city_1200
// to exactly 12:00:00), decoded against the asm-attested BrnEnvironmentKeyframe layout and
// pushed through the real producer maths read out of
// EnvironmentManager::GenerateShaderConstants @0x827D0098. Three of those transforms are NOT
// identities and are the difference between a sky and a bug:
//   * g_layerCloudiness is 1 - LayerDensity   (`v159 = 1.0 - a50[0]` before the +0x280 store)
//   * g_layerInvFeather is 1 / LayerFeathering(`v159 = 1.0 / a52[0]` before the +0x290 store)
//   * the cloud texture scale is LayerScale * 0.00012500001 (flt_820CD130), not a world size
//
// WHITE LEVEL. The console multiplies every colour by EnvironmentManager::mfWhiteLevel
// (+0x11C0), which Construct @0x827CA408 seeds to 0.5 and nothing else writes, and the post-FX
// tonemapper divides it back out. This build has no tonemapper, and the world's own bring-up
// publisher already publishes its colours at white level 1.0 (HDRConstants = (1,1,1,1)), so the
// sky is published the same way -- the RAW keyframe colours. The clouds are the one place that
// choice is visible: BrnSkyDomeManager multiplies both cloud colours by KF_CLOUD_COLOUR_SCALE
// (2.0), which on the console exactly cancels the 0.5, so they are pre-divided by 2 here and
// reach the shader at their authored values. (That round trip is also what pins mfWhiteLevel
// to 0.5 independently of the asm.)
void BrnRendererModule::PublishSkyConstantsBringUp(BrnShaderConstantsFrame* lpFrame)
{
    if (lpFrame == 0)
        return;

    lpFrame->LockForWriting();

    // ---- camera (the console's WorldModule::SetupShaderConstantsBeforeRendering half) ----
    lpFrame->SetViewProjectionMatrix(gBrnSkyCameraBringUp.mViewProjection);
    lpFrame->SetViewPosition(gBrnSkyCameraBringUp.mViewPosition);

    // ---- the key light -------------------------------------------------------------------
    // MUST match the direction WorldModule::PublishWorldShadingConstantsBringUp publishes at
    // shader-constant slot 10, or the sun in the sky sits somewhere other than where the world
    // is lit from. It is the direction the light TRAVELS; Im3dSkyDome::SetConstants negates it
    // and appends its XZ length for the shader's KeyLightDirAndXZLength.
    //
    // FLAG: this is the bring-up direction, NOT the console's. ComputeKeyLightDirection
    // @0x82678AB0 derives the real one from the time of day and three manager tuning angles
    // (SunRigRotation 45 deg, SunTiltAtHorizon 20 deg, SunTiltAtMidday 50 deg, all seeded by
    // EnvironmentManager::Construct); at city_1200's 43200 s that gives
    // (-0.60916963, -0.66981047, -0.42457779) -- a different azimuth and a 12-degree lower
    // sun. Adopt it in the same change that adopts it for the world, not before.
    lpFrame->SetKeyLightDirection(Vector3{ 0.406f, -0.812f, 0.419f, 0.0f });
    lpFrame->SetKeyLightColour(Vector3{ 1.700000f, 1.700000f, 1.054000f, 0.0f });
    lpFrame->SetWhiteLevel(1.0f);

    // ---- the sky gradient (ScatteringData @keyframe+0x090) --------------------------------
    // rgb from Sky{Top,Hor,Sun}Colour; .w from SkyDrk / SkyHorPow / SkySunPow (the .w lanes
    // are exponents and offsets, and the console's white-level vector is (wl,wl,wl,1) exactly
    // so they are NOT scaled).
    lpFrame->SetTopColourDrk  (Vector4{ 0.05321430f, 0.41437697f, 0.71731997f,  0.0f });
    lpFrame->SetHorColourPow  (Vector4{ 1.01527800f, 0.88277322f, 0.80715537f,  0.5f });
    lpFrame->SetSunColourPow  (Vector4{ 1.00100000f, 1.00100000f, 0.97702491f, 13.1f });
    // SkyHorBleedScl / SkyHorBleedPow / SkySunBleedPow (keyframe +0x0CC/+0x0D0/+0x0D4).
    lpFrame->SetHorBleedSclPow(Vector3{ 5.0f, 4.3f, 6.5f, 0.0f });

    // ---- fog / scattering: {1/(far-near), near/(far-near), ScattPow, ScattCap} ------------
    // ScattDist = (25, 1500), ScattPow = 1, ScattCap = 0.87. Same four numbers the world's
    // ScattCoeffs (slot 27) carries, so the dome's horizon haze matches the city's.
    lpFrame->SetFogScattering(Vector4{ 0.000677966102f, 0.0169491525f, 1.0f, 0.87f });

    // ---- the clouds (CloudsData @keyframe+0x1D0) -----------------------------------------
    // Pre-divided by KF_CLOUD_COLOUR_SCALE (see the white-level note above).
    lpFrame->SetCloudDarkColour0(Vector4{ 0.50000000f, 0.49262166f, 0.46200001f, 0.5f });
    lpFrame->SetCloudLiteColour0(Vector4{ 0.10284000f, 0.10284000f, 0.10284000f, 0.5f });
    // (xy) the cloud drift offset -- EnvironmentManager::Update accumulates it from
    // LayerSpeed/DirectionAngle; Construct's t=0 value is (0,0), so the clouds do not drift.
    // (zw) LayerScale[0] (1.0) * 0.00012500001.
    lpFrame->SetCloudTextureScaleAndOffsets0(Vector4{ 0.0f, 0.0f, 0.00012500001f, 0.00012500001f });
    lpFrame->SetCloudLayerDensity   (Vector4{ 0.0f,          1.0f,  0.0f, 0.0f });  // 1 - (1.0, 0.0)
    lpFrame->SetCloudLayerInvFeather(Vector4{ 0.909090889f, 10.0f,  0.0f, 0.0f });  // 1 / (1.1, 0.1)
    lpFrame->SetCloudLayerOpacity   (Vector4{ 0.5f,          0.0f,  0.0f, 0.0f });
    // g_domeRanges.z. EnvironmentManager::Construct seeds +0x6F0 to 1.0 and nothing else
    // writes it; no keyframe field feeds it.
    lpFrame->SetCloudDistanceCurve(1.0f);

    lpFrame->UnlockForWriting();
}

// @ 0x8240BFA8 - BrnRendererModule::Render. Reconstructed from the X360 ARTIST build.
//
// The full Render walks the whole frame (shadow maps, env map, world/car opaque +
// transparent, sky, coronas, particles, post-fx, MSAA resolve) and finishes with the
// loading-screen overlay and the present. During boot none of the world systems have
// data, so those passes are data-gated off; Option B reconstructs the part that actually
// runs - frame begin, the loading-screen foreground overlay, and the present. The gameplay
// passes are reconstructed incrementally as their subsystems come online.
void BrnRendererModule::Render(const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer)
{
    if (!renderengine::Device::FrameBegin())
    {
        return;
    }

    // Forward the dispatch buffer's loading-screen command into the renderer - the X360
    // Render does exactly this each frame (@0x8240BFA8: AddCommand(*(lpDispatchIn+9828)),
    // the `lwzx r4, r26, 0x9990` at 0x8240C17C). The command is one-shot: the manager's
    // end-of-frame Swap re-Constructs each new write buffer, so the slot reads
    // E_LSC_NONE (AddCommand's no-op default) on the frames between events.
    //
    // (The old PC video gate died with the movie pass re-home: fullscreen movies now
    // present inside the GUI pass exactly like the console, and the flow states manage
    // the loading screen around them through the real 19/20 protocol -- BootLoading::
    // OnLeave and PostTitleScreenLoad post StopAptLoadingMovie before playing a video.)
    if (lpDispatchThreadInputBuffer != 0)
        mLoadingScreenRenderer.AddCommand(
            lpDispatchThreadInputBuffer->GetLoadingScreenCommand());

    // The console holds the dispatch input buffer's READ LOCK for the whole frame: `mr r3, r26 /
    // bl CgsModule::IOBuffer::LockForRead` @0x8240C38C, before the first world pass, and the matching
    // UnlockForRead @0x8240E304 at the tail of Render (r26 = lpDispatchThreadInputBuffer, the same
    // register the `lwzx r4, r26, 0x9990` GetLoadingScreenCommand read above uses). Every read-locked
    // getter this function calls sits inside that window -- GetBrightness @0x8240DCC8 / GetContrast
    // @0x8240DCFC for the composite, and the effects-frame reads -- and each asserts
    // "Not locked for reading" outside it. The lock was missing here until the composite lit its
    // first locked getter (2026-08-15); GetLoadingScreenCommand is an inline read with no assert,
    // which is why nothing had noticed. (There is also a shorter first window, LockForRead
    // @0x8240C1C4 / UnlockForRead @0x8240C220, around the console's tint-vector reads at
    // 0x8240C1C8-0x8240C21C, which this build does not perform yet.)
    if (lpDispatchThreadInputBuffer != 0)
        lpDispatchThreadInputBuffer->LockForRead();

    // ---- X360 Render:389-396 -- the object->mesh expansion and the pass sorts. ------------
    // Hoisted up here (it used to live at the top of RenderWorldPasses) because the SHADOW
    // pass below consumes mesh lists 0..4 and, on the console, runs before the world passes.
    // The context object is on Render's stack exactly as the X360 keeps it.
    CgsGraphics::DispatchObjectContext lDispatchContext;
    const bool lbDispatchReady = BuildDispatchLists(&lDispatchContext);

    // [PC bring-up] Realise the shadow-map render target. The console builds the whole
    // render-target pool in BrnRendererMemory::Construct during BrnRendererModule::Construct;
    // that pool is not linkable on this build (see the BRN_RENDERER_MEMORY_FULL_POOL_AVAILABLE
    // banner in BrnRendererMemory.cpp), and module Construct runs before the D3D9 device exists
    // anyway. So the shadow slice is created lazily, here, on the first frame that has a device
    // -- the same shape as the sky dome's PrepareSkyDome gate. DELETE with the pool.
    EnsureShadowMapTarget(mAllocatedRenderTargets);

    // [PC bring-up, post-fx spine wave 2026-08-13] Realise the off-screen SCENE target (and the
    // down-sample buffer beside it) the same lazy way, on the first frame that has a device.
    //
    // NOTHING RENDERS INTO IT YET, and that is deliberate. This wave lands the pool half of the spine
    // only: the console's frame bracket (BeginRenderAntiAliased @0x823FFA18 / ResolveMSAA @0x823FFBE0 /
    // EndRenderAntiAliased @0x82408B00) is what actually redirects the world passes off-screen, and all
    // three are still absent from the tree -- their ledger "reviewed" status is false, like the eight
    // pool creators' was. Until they land, the world keeps drawing straight to the back buffer and this
    // target is created, logged ("[postfx-rt] ...") and left idle. The point of landing it separately is
    // that a wrong scene target and a wrong bracket look identical on screen; this half is provable on
    // its own, from the log line and an unchanged frame.
    EnsurePostFxSceneTargets(mAllocatedRenderTargets);

    // [PC bring-up, gate-flip wave 2026-08-15] The three RENDER-STATE FACTORIES. The console
    // constructs them in BrnRendererModule::Construct @0x8240A778 -- three vtbl[0] calls at
    // 0x8240A950 / 0x8240A968 / 0x8240A980 on the by-value members at this+0x3940 / +0x3944 /
    // +0x3948 (mBlendStateFactory / mRasterizerStateFactory / mDepthStencilStateFactory), each with
    // r4 = this->mpGraphicsAllocator (`lwz r4, 0x394C(r31)`). On PC the module's Construct has no
    // device and a null graphics allocator, so -- like the pool above and like BrnPostFx::Construct
    // below -- they run here, once, on the bring-up allocator, the first frame that has a device.
    // Every state the world / post-fx / corona code pushes by table slot (saBlendStates[n],
    // saDepthStencilStates[n], saRasterizerStates[n]) is null until this runs; the composite's
    // cull-mode-NONE push (saRasterizerStates[2]) was the first consumer to show it -- a compare-
    // then-skip on null left the world's back-face cull in force and the full-screen quad was
    // culled whole. Order = the console's (blend, rasterizer, depth-stencil).
    {
        static bool sbStateFactoriesConstructed = false;
        if (!sbStateFactoriesConstructed && renderengine::gDevice != 0 && EnsureWorldDispatchAllocator())
        {
            sbStateFactoriesConstructed = true;
            mBlendStateFactory.Construct(&sWorldDispatchAllocator);          // 0x8240A950
            mRasterizerStateFactory.Construct(&sWorldDispatchAllocator);     // 0x8240A968
            mDepthStencilStateFactory.Construct(&sWorldDispatchAllocator);   // 0x8240A980
            CgsDev::Log::WriteToLog("[postfx-composite] the three render-state factories are Constructed"
                                    " (deferred PC bring-up)\n");
        }
    }

    // ---- X360 Render:536-542 -- the three GLOBAL texture binds. --------------------------
    //
    // THIS IS THE SHADOW RECEIVER'S MISSING HALF. 92 of the 110 pixel shaders in the shipped
    // SHADERS.BNDL declare `dcl_2d s15` and do `texldp r1, r1, s15`; the three
    // ShadowMap_* constants they read alongside it are registered, name-bound and uploaded --
    // but nothing in this build had ever bound a TEXTURE to sampler 15, so every one of those
    // shaders was sampling an unbound sampler.
    //
    // The console runs these unconditionally, after shadow::Device::ResetShadowing() and before
    // the shadow-map pass, through sub_8227D158 -- the shadow cache's TEXTURE STATE bind, which
    // installs a renderengine::TextureState (sampler parameters + raster) built once in
    // Construct @0x8240A778. This build has no TextureState objects for them (Construct's two
    // TextureState::Initialize calls need the render-target pool and the resource allocator), so
    // the binds go through the cache's other entry point, shadow::Device::SetResource @0x82276C70
    // -> D3DDevice_SetTexture: the same D3D call at the end of the same shadow cache, minus the
    // sampler-state half. FLAG: the sampler state (filter/address/comparison) therefore comes
    // from whatever the unit last held; wire the TextureState pair when Construct's pool lands.
    //
    //   s15 <- GetDepthTexture(mapRenderTarget[SHADOW_MAP_0])   (this+5924 on the console)
    //   s14 <- the blobby-shadow texture                        (this+5804, PrepareAgain's arg)
    //   s13 <- GetTexture(mapRenderTarget[ENV_MAP], 0)          (this+5700)  [PARKED, see below]
    {
        // s15 -- the shadow map's resolved depth texture. Both guards are the console's own
        // asserts in BeginRenderShadowMap; here they are a gate, because the pool is built
        // lazily on this build and GetDepthTexture would assert on a target with no
        // post-fx RenderTarget behind it yet.
        //
        // LIVE since the render-target wave (2026-08-12): GetShadowMapBuffer and GetDepthTexture
        // now have a target behind them (EnsureShadowMapTarget above builds it on the first
        // frame with a device, and PostFxRenderTargetPCLeaf.cpp creates the D3D9 depth texture).
        // GetRenderTarget() being non-null is still the gate -- a device-less or
        // depth-texture-less machine leaves sampler 15 unbound rather than binding garbage.
#if BRN_SHADOW_MAP_TARGET_AVAILABLE
        CgsRenderTarget* const lpShadowBuffer = mAllocatedRenderTargets.GetShadowMapBuffer(0);
        if (lpShadowBuffer != 0 && lpShadowBuffer->GetRenderTarget() != 0)
        {
            shadow::Device::SetResource(lpShadowBuffer->GetDepthTexture(), 15u);

            // FLAG PC-platform leaf: the SAMPLER half of the console's bind. sub_8227D158
            // installs a renderengine::TextureState (filter + address + comparison) alongside
            // the resource; shadow::Device::SetResource is the resource-only entry point, and
            // it CACHES -- once the texture pointer stops changing it makes no D3D call at all,
            // so a sampler state set inside it would be applied once and then never refreshed.
            // Applying it here, unconditionally, every frame is the honest stand-in until
            // Construct's TextureState pair lands. See ShadowPassPCLeaf.h.
            renderengine::ShadowSampler_ApplyState(15u);
        }
#endif

        // s14 -- the blobby-shadow texture. The console guards this one too (`if (v79)` at
        // Render:540); it is null until GamePrepare stage 3 hands it to PrepareAgain.
        if (mpBlobbyShadowTexture != 0)
        {
            shadow::Device::SetResource(mpBlobbyShadowTexture, 14u);
        }

        // s13 -- PARKED, no source. The console binds the ENV-MAP target's colour texture
        // (CgsRenderTarget::GetTexture(mapRenderTarget[E_RENDER_TARGET_ENV_MAP], 0)), but
        // BrnRendererMemory exposes only GetShadowMapBuffer -- mapRenderTarget is private and
        // the env-map slot has no accessor, because no X360-attested caller needed one before
        // now. Binding some other texture here would be an invention, and leaving the unit
        // unbound is what the build already does. Unpark it with the env-map accessor (that
        // header is another agent's this wave) once the env-map pass exists to fill the target.
    }

    // ---- X360 Render:545-640 -- THE SHADOW-MAP PASS. -------------------------------------
    // Before RenderWorldPasses and before anything binds the scene target, exactly as the
    // console orders it. Gated on mRenderSwitches.mbRenderShadows.
    if (lbDispatchReady)
    {
        RenderShadowMapPasses(&lDispatchContext);
    }

#if BRN_ANTIALIAS_BRACKET_AVAILABLE
    // ---- X360 Render:725 -- OPEN THE ANTI-ALIASED SCENE PASS (call @0x8240CDB8). ----------
    //
    // POSITION. The console calls BeginRenderAntiAliased after the shadow-map pass and after the
    // env-map face loop, and before every world-pass dispatch: in Render's listing the last
    // BrnGraphics__ShadowMapRenderManager__EndRenderShadowMap is @0x8240CB1C, the env-map loop's
    // BrnRendererModule__EndRenderEnvironmentMapFace is @0x8240CD74, and the call is @0x8240CDB8.
    // The env-map faces are not reconstructed on this build (see the RenderWorldPasses banner), so
    // this point between the shadow pass and the world pass IS the console's position.
    //
    // THE GATE. Neither this body nor ResolveMSAA null-tests the pool -- faithfully; the X360 asm at
    // 0x823FFB08-0x823FFB34 has no null test -- and on PC the pool is built LAZILY, so the call must
    // not be made before EnsurePostFxSceneTargets() has returned true. It is additionally gated on
    // lbDispatchReady, the same PC bring-up precondition the world passes it brackets already carry
    // (BuildDispatchLists returns false only when the GDL ring never came up), so the bracket can
    // never open around a world pass that did not run. Both are ONE condition, evaluated once, so
    // ResolveMSAA below cannot run without its Begin.
    //
    // WHY EnsurePostFxSceneTargets IS CALLED AGAIN HERE rather than reusing the result of the call
    // at the top of Render: it keeps every runtime effect of this wiring inside this `#if`, so
    // setting BRN_ANTIALIAS_BRACKET_AVAILABLE back to 0 restores the previous code exactly. The
    // function is value-latched on a file static and returns on a pointer compare when the targets
    // already exist.
    const bool lbSceneBracketOpen =
        lbDispatchReady && EnsurePostFxSceneTargets(mAllocatedRenderTargets);

    // ---- the three arguments, all read rather than chosen -----------------------------------
    //
    // lfWhiteLevel -- f26 in the console's Render, loaded ONCE near the top:
    //     0x8240C4B0  lbz    r11, 0xAD0(r31)
    //     0x8240C4B4  mulli  r11, r11, 0x320
    //     0x8240C4B8  add    r11, r11, r31
    //     0x8240C4BC  lfs    f26, 0x7A8(r11)
    // and handed to BeginRenderAntiAliased (`fmr f1, f26` @0x8240CDB4) and to ResolveMSAA
    // (`fmr f1, f26` @0x8240D5B8) unchanged -- which is why one local serves both calls here.
    // WHAT IT IS: this+0xAD0 is mu8ShaderConstantsFrameInternal and 0x320 is
    // sizeof(BrnShaderConstantsFrame), so r11 = this + 0x320*index, and +0x7A8 off that is the
    // frame's member at +0x318 -- mfWhiteLevel (BrnShaderConstantsFrame.h:99, "@0x318"). The
    // arithmetic closes on itself: maShaderConstantsFrames is at this+0x490, 0x490 + 0x318 == 0x7A8,
    // and 0x490 + 2*0x320 == 0xAD0, i.e. the array ends exactly where the two index bytes begin.
    // On this build the value is 1.0f, because BrnShaderConstantsFrame::Construct seeds it
    // (BrnShaderConstantsFrame.cpp:40, ARTIST 0x823F7478 storing flt_82001C98 == 1.0) and nothing
    // writes the INTERNAL frame's white level. It is read from the member anyway, not hard-coded.
    const f32 lfFrameWhiteLevel =
        maShaderConstantsFrames[mu8ShaderConstantsFrameInternal].GetWhiteLevel();

    // lbClearStencil / luStencilClearValue -- the console reads BOTH off the LAYER-0 INTERNAL
    // BrnEffectsFrame, at Render @0x8240C290-0x8240C314: `addi r21, r31, 0x480` (mEffectsArbitrator),
    // `lwz r8, 0(r21)` (mapaEffectsFrames[0]), `lbz r11, 0xC(r21)` (mu8EffectsFrameInternal),
    // `mulli r11, r11, 0x1F0` (== the GUEST sizeof(BrnEffectsFrame)), then
    //   `lbz r9, 0x1E0(r9)`  -> mMotionBlurData.mbIsActive        -> var_CD0 -> r5 @0x8240CDB0
    //   `lfs f13, 0x1DC(r8)` * flt_82010C20, fctiwz, low byte     -> var_CCE -> r6 @0x8240CDAC
    //                        -> that member is mMotionBlurData.mfWorldBlurAmount.
    // ResolveMSAA is handed the SAME var_CCE (`lbz r5, 0xD40+var_CCE(r1)` @0x8240D5B4), which is why
    // one value serves both calls, and BrnPostFx::Render is handed the SAME var_CD0 (`lbz r7,
    // 0xD40+var_CD0(r1)` @0x8240DE18). The motion blur is masked into the STENCIL buffer: the stencil
    // clear value IS the world blur amount quantised to a byte, and the clear is gated on motion blur
    // being active. A third byte, var_CCF, is (u8)(mfCarsBlurAmount * 255.0f) and feeds the CAR
    // passes' stencil reference (@0x8240CED0 / @0x8240D344), which this build does not reconstruct.
    //
    // LIVE SINCE THE BLOOM WAVE (2026-08-15). This used to be a documented all-zero floor whose note
    // read "mEffectsArbitrator is NEVER Constructed on this build". It IS Constructed now
    // (EnsureEffectsArbitratorBringUp, above), so the bytes are READ -- through the arbitrator's
    // GetInternalEffectsFrame accessor and the frame's named members, never through 496*index.
    // flt_82010C20 is attested at 255.0f as well (conductor idat dump; the old note asked for it).
    // The values are still 0/0/false on this build, and that is not a floor either: the layer-0
    // producer posts no motion-blur event, so what the frame carries is what
    // MotionBlurData::Construct @0x821F84E8 seeds -- mfCarsBlurAmount = mfWorldBlurAmount = 0.0f,
    // mbIsActive = false -- which is exactly what the console reads on any frame without one.
    //
    // POSITION: the console performs these reads much earlier (0x8240C290, right after
    // SortDispatchLists) and carries them in stack slots to four consumers. They are read here
    // instead, at the first consumer, because nothing writes the layer-0 INTERNAL frame between
    // those two points -- the only writers are the producers (StartOfFrame / DoDispatch, both in the
    // UPDATE frame) and EffectsArbitrator::EndOfFrame (SwapBuffers, after Render), all outside this
    // function.
    //
    // Neither byte is observable on this build in any case: lbClearStencil is not read by the X360
    // BeginRenderAntiAliased body (see its own note), and the PC scene target's depth surface is
    // picked by a format ladder whose first candidate is D3DFMT_D24X8 -- no stencil bits -- so the PC
    // D3DDevice_Resolve strips D3DCLEAR_STENCIL from its clear.
    BrnRendererPostFxFrameBytes lPostFxFrameBytes;
    BrnRendererReadPostFxFrameBytes(
        sbEffectsArbitratorConstructed ? &mEffectsArbitrator : 0, &lPostFxFrameBytes);

    const bool lbSceneClearStencil      = lPostFxFrameBytes.mbMotionBlurActive;
    const u8   luSceneStencilClearValue = lPostFxFrameBytes.mu8WorldBlurStencil;

    if (lbSceneBracketOpen)
    {
        BeginRenderAntiAliased(lfFrameWhiteLevel, lbSceneClearStencil, luSceneStencilClearValue);
    }
#endif  // BRN_ANTIALIAS_BRACKET_AVAILABLE

    // The gameplay render walk (the world/car/sky passes). On the X360 this whole
    // block precedes the 2D overlay tail; with no world GDL data the lists are empty
    // and every pass no-ops.
    if (lbDispatchReady)
    {
        RenderWorldPasses(lpDispatchThreadInputBuffer, &lDispatchContext);
    }

#if BRN_ANTIALIAS_BRACKET_AVAILABLE
    if (lbSceneBracketOpen)
    {
        // ---- X360 Render:725+ -- CLOSE THE SCENE PASS (call @0x8240D5BC). --------------------
        // Resolve the scene out of the anti-alias buffer into the down-sample buffer AND clear the
        // anti-alias buffer behind the copy. That clear is the whole reason this call cannot be
        // skipped: on the untiled path it is the ONLY clear in the bracket, because
        // BeginRenderAntiAliased's untiled branch (0x823FFB90-0x823FFBD8) deliberately clears
        // nothing -- the previous frame's resolve is what left the surface clean. Drop it and the
        // scene's DEPTH is never cleared again.
        ResolveMSAA(lfFrameWhiteLevel, luSceneStencilClearValue);

        // [FLAG PC bring-up] PRESENT THE SCENE TARGET. The world passes above drew into the
        // off-screen anti-alias buffer and ResolveMSAA has just copied the result into the
        // down-sample buffer; nothing has handed it back to the swap chain, because that is
        // EndRenderAntiAliased @0x82408B00 and it is declaration-only. So one full-screen textured
        // quad copies the scene onto the back buffer here, before the 2D/GUI tail draws over it.
        // NOT the post-fx composite: BrnPostFx::Render @0x8240A468 is the next wave and is what
        // retires this call, the helper it calls, and renderengine::PCSceneBlit_Begin/_End.
        //
        // ⚠ AFTER ResolveMSAA, NOT BEFORE. renderengine::PCSceneBlit_Begin rebinds the back buffer
        // and deliberately leaves it bound; the PC D3DDevice_Resolve REFUSES to clear when it finds
        // the swap chain bound (it logs "[postfx-resolve] ResolveMSAA ran with the SWAP CHAIN
        // bound") precisely so that this misordering cannot silently cost the frame its clear.
        //
        // Placed BEFORE the BRN_WORLD_ONLY early-out below on purpose: that diagnostic exists to
        // show the world pass with the 2D tail suppressed, and with the world off-screen it would
        // otherwise show black.
        //
        // ⚠ AND IT HANDS THE BACK BUFFER BACK ON EVERY PATH. BeginRenderAntiAliased above bound the
        // scene target and nothing else unbinds it, so if this call could return without rebinding
        // the swap chain the WHOLE 2D/GUI tail -- and the BRN_WORLD_ONLY early-out below -- would
        // draw off-screen and the frame would present the black FrameBegin clear. The helper
        // therefore calls PCBringUpHandBackTheBackBuffer() on both of its no-scene early returns;
        // see its banner. That is why this call is UNCONDITIONAL inside the bracket gate: the
        // condition that authorised the bind is the condition that guarantees the unbind.
#if BRN_POSTFX_COMPOSITE_AVAILABLE
        // ---- THE REAL COMPOSITE, at the console's own position (call @0x8240DE3C). -------------
        //
        // ARGUMENTS, all read off the X360 call site (asm 0x8240DE0C-0x8240DE3C) rather than chosen:
        //   r4 = this + 0x238                  -> mAllocatedRenderTargets
        //   r5 = *(mapRenderTarget[4] + 0x108) -> the DOWN-SAMPLE buffer's postfx RenderTarget (SRC)
        //   r6 = *(mapRenderTarget[5] + 0x108) -> the BACK-BUFFER target's                     (DST)
        //   v1 = the 2D tint colour Vector4, zeroed unless the layer-0 effects frame supplies one
        //   r7 = the motion-blur-active byte off that same effects frame
        //   f1 = GetBrightness() * 0.01f - 0.5f      f2 = GetContrast() * 0.01f + 0.5f
        //   f3 = f26, the frame white level -- the SAME value BeginRenderAntiAliased / ResolveMSAA take
        //   f4 = *(this + 0xC408)              -> mfAspectCorrection
        //   stack = the calibration texture handle, or null
        // The three that come off mEffectsArbitrator (tint vector, motion-blur flag, calibration
        // texture) are the console's own NO-EVENT values, not invented ones, and the seam function
        // documents each; mEffectsArbitrator is never Constructed on this build, so reading through
        // mapaEffectsFrames[0] would crash rather than give a wrong answer.
        //
        // THE GATE IS THE CONSOLE'S. `lbz r11, 0(r26)` @0x8240DC6C / `beq cr6, loc_8240DE44` skips
        // the entire block; r26 is this+0xC41C, which BrnGraphics::DebugComponent::OnActivate
        // @0x823F7B98 registers as the "Render PostFX" toggle -- i.e. mbRenderPostFX
        // (BrnRendererModule.h:593, defaulted true at :692).
        //
        // ⚠ BOTH ARMS END WITH THE SWAP CHAIN BOUND, and that is why the decision is here rather
        // than inside the helper. BeginRenderAntiAliased redirected the world off-screen; if this
        // point can be passed without rebinding, the whole 2D/GUI tail -- and the BRN_WORLD_ONLY
        // early-out below -- draws where nobody will see it and the frame presents the black
        // FrameBegin clear. The composite arm rebinds with PCBringUpHandBackTheBackBuffer(); the
        // fallback arm calls the blit, which rebinds on all of its own paths.
        //
        // ⚠ AND THE FALLBACK IS NOT DEFENSIVE PADDING. PCBringUpRenderPostFxComposite returns false
        // whenever the pool cannot supply BOTH surfaces, which is this build's state -- only
        // DOWN_SAMPLE and ANTI_ALIAS are created. Handing the swap chain back without the composite
        // COPIES NOTHING, so without this else-arm the frame would be black with the GUI on top.
        // lbComposited is ALSO false when mbRenderPostFX is off, and the fallback covers that too:
        // on the console, turning that debug toggle off leaves the frame with no presenter at all,
        // which is a debug-menu behaviour, not one worth reproducing as a black screen on PC. That
        // is the ONE deviation from the console's conditional and it is confined to the else-arm --
        // the composite itself runs exactly when the console runs it.
        //
        // RETIRES THE BLIT: the day this gate is permanently 1 and EndRenderAntiAliased @0x82408B00
        // lands, delete PCBringUpBlitSceneTargetToBackBuffer, PCBringUpHandBackTheBackBuffer,
        // renderengine::PCSceneBlit_Begin/_End and their XenonD3D9Shims.cpp bodies together.
        // ---- X360 Render lines 964..1232 -- THE EFFECTS-FRAME APPLY BLOCK. --------------------
        //
        // This is the block that turns the layer-0 internal BrnEffectsFrame into BrnPostFx's
        // m_enabledFx bits and its four effect state blocks. Its statements live in the sibling TU
        // BrnRendererModulePostFx.cpp for one translation-unit reason (the EA::Jobs::Job placeholder
        // in BrnRendererModule.h) that that file's banner sets out in full; it is called from here,
        // at the console's position, with the console's gate.
        //
        // THE GATE IS `if (mbRenderPostFX)` (`if (*HIDWORD(v301[0]))` at pseudocode line 965, i.e.
        // *(this + 50204) == this+0xC41C, which BrnGraphics::DebugComponent::OnActivate @0x823F7B98
        // registers as the "Render PostFX" toggle) -- the SAME word that gates the composite below.
        // The console tests it twice, once for each half; both tests are reproduced.
        //
        // v296 == lbEffectsAllowed == DispatchThreadInputBuffer::GetCalibrationUnfriendlyEnablePostFx
        // (pseudocode lines 440-441 -- read ONCE near the top of Render and ANDed into every effect's
        // active flag). The no-dispatch-buffer fallback is the buffer's OWN Construct default rather
        // than a pick: DispatchThreadInputBuffer::Construct seeds
        // mbCalibrationUnfriendlyEnablePostFx = true (BrnDispatchThreadInputBuffer.cpp:212).
        const bool lbEffectsAllowed = (lpDispatchThreadInputBuffer != 0)
            ? lpDispatchThreadInputBuffer->GetCalibrationUnfriendlyEnablePostFx()
            : true;

        BrnGraphics::EffectsArbitrator* const lpEffectsArbitrator =
            sbEffectsArbitratorConstructed ? &mEffectsArbitrator : 0;

        // [FLAG PC bring-up] GATING NOTE (rung-5 verifier): on the console this block runs
        // unconditionally inside Render; here it sits inside `if (lbSceneBracketOpen)` and under
        // BRN_ANTIALIAS_BRACKET_AVAILABLE / BRN_POSTFX_COMPOSITE_AVAILABLE, so those two PC gates
        // carry the effects apply with them -- reverting either reverts bloom. It is not hoisted out
        // because BrnPostFx's states are only meaningful once the composite that consumes them draws.
        if (mbRenderPostFX)
        {
            // D3DDevice_SetShaderGPRAllocation(off_83271608, 0, 0x40, 0x40) @0x8240D6FC opens the
            // console's block. It is a Xenos GPR-partition hint with no D3D9 counterpart, and this
            // file already drops the other three occurrences in Render (0x8240C7F0, 0x8240CB38,
            // 0x8240DE68), so it is dropped here too rather than routed through a shim that would be
            // a no-op. Named so it stays greppable.
            BrnRendererApplyEffectsFrameToPostFx(lpEffectsArbitrator, lbEffectsAllowed);
        }

        // ---- X360 Render lines 1237..1252 -- the second gate: tint colour + motion blur. -------
        // The 2D tint vector is ALWAYS four floats (the console zeroes it before the conditional
        // read), so a false mbRenderPostFX simply leaves the neutral zero the composite has been fed
        // since it landed.
        f32 lafTint2dColour[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        if (mbRenderPostFX)
        {
            BrnRendererEvalPostFxTint2dColour(lpEffectsArbitrator, lbEffectsAllowed,
                                              lafTint2dColour);

            // MotionBlurState::Update -- BLOCKED, and gated OFF inside the callee (its body does not
            // exist in this tree, and its two matrix arguments live inside the still-opaque
            // DispatchThreadInputBuffer::ParticleRenderData). The call is written at the console's
            // position so it lights the day both land; see the callee's banner for the evidence.
            (void)BrnRendererUpdatePostFxMotionBlur(lpEffectsArbitrator, 0);
        }

        // [FLAG PC bring-up diagnostic] the sampled [postfx-fx] line -- six lines, 500 frames apart,
        // proving base frame -> Eval* -> BrnPostFx. Sits beside the [postfx-composite] diagnostics in
        // BrnPostFx.cpp. DELETE with the bring-up.
        BrnRendererLogPostFxEffectState();

        const s32 liBrightnessSetting = (lpDispatchThreadInputBuffer != 0)
            ? lpDispatchThreadInputBuffer->GetBrightness()
            : KI_DEFAULT_CALIBRATION_SETTING;
        const s32 liContrastSetting = (lpDispatchThreadInputBuffer != 0)
            ? lpDispatchThreadInputBuffer->GetContrast()
            : KI_DEFAULT_CALIBRATION_SETTING;

        // The last two arguments are the console's own effects-frame pair, which the seam used to
        // hard-code (zero tint / motion blur false) while mEffectsArbitrator was never Constructed:
        // v1 == the tint vector evaluated just above, r7 == the layer-0 internal frame's
        // mMotionBlurData.mbIsActive read at the top of the bracket (lPostFxFrameBytes).
        //
        // mbMotionBlurActive REACHING true SELECTS A PERMUTATION WITH NO PC PROGRAM:
        // BrnPostFxShader::Render builds `leShader = 4*(quality+1 if motion blur) | 2*dof | tint3d`
        // and hard-returns without drawing on anything but slot 0 (BrnPostFxShader.cpp:1389-1397), so
        // the frame would present un-composited. It cannot happen on this build -- the layer-0
        // producer posts no motion-blur event, so the byte is false -- and it is passed through
        // rather than pinned to false because pinning it would hide the transition the day an event
        // is posted.
        const bool lbComposited =
            mbRenderPostFX &&
            PCBringUpRenderPostFxComposite(
                mAllocatedRenderTargets,
                static_cast<f32>(liBrightnessSetting) * KF_CALIBRATION_SLIDER_SCALE
                    - KF_CALIBRATION_SLIDER_BIAS,
                static_cast<f32>(liContrastSetting) * KF_CALIBRATION_SLIDER_SCALE
                    + KF_CALIBRATION_SLIDER_BIAS,
                lfFrameWhiteLevel,
                mfAspectCorrection,
                lafTint2dColour,
                lPostFxFrameBytes.mbMotionBlurActive);

        if (lbComposited)
        {
            PCBringUpHandBackTheBackBuffer();
        }
        else
        {
            PCBringUpBlitSceneTargetToBackBuffer(mAllocatedRenderTargets, &mIm2dRenderer);
        }
#else
        PCBringUpBlitSceneTargetToBackBuffer(mAllocatedRenderTargets, &mIm2dRenderer);
#endif
    }
#endif  // BRN_ANTIALIAS_BRACKET_AVAILABLE

    // [FLAG PC diagnostic] BRN_WORLD_ONLY=1 suppresses the whole 2D overlay tail
    // (loading-screen background/foreground, GUI, movie) for ONE purpose: seeing the
    // world pass. The loading screen paints an opaque full-screen quad here, so during
    // bring-up -- when the flow is still parked on the loading screen -- the world
    // geometry the passes above just drew is completely covered. Environment-gated and
    // read once; the default path is untouched. DELETE with the bring-up.
    static int siWorldOnly = -1;
    if (siWorldOnly < 0)
    {
        char lacWorldOnly[8];
        siWorldOnly = (GetEnvironmentVariableA("BRN_WORLD_ONLY", lacWorldOnly, sizeof(lacWorldOnly)) > 0) ? 1 : 0;
    }
    if (siWorldOnly == 1)
    {
        if (lpDispatchThreadInputBuffer != 0)
            lpDispatchThreadInputBuffer->UnlockForRead();   // the frame-long read lock, both exits
        renderengine::Device::ShowPixelBuffer();
        return;
    }

    // Save/load background layer: in E_LSC_SHOWSAVELOADBG mode the loading screen renders
    // BENEATH the GUI, so the SaveLoadComponent prompt draws over the dimmed loading art.
    // Layer order is the console Render tail @0x8240BFA8: RenderBackground -> the GUI
    // dispatch flush -> RenderForeground.
    mLoadingScreenRenderer.RenderBackground(&mIm2dRenderer);

    // GUI render drive (the Apt/view frame): the X360 render pass runs the GUI module's
    // Render (BrnGui::GuiModule::Render @0x825146B8 -> CgsGui::GuiModule::Render
    // @0x8285AF38 -> ViewModule::Render @0x82858810 -> RenderInternal @0x82858AF8 ->
    // AptAux::Render -> the engine render walk), which fills the published Apt command
    // buffer, then the PC dispatch leaf flushes it to D3D9 and the movie pass presents
    // the active fullscreen video over it (UpdateAndRenderMovieManager, inside the GUI
    // pass, exactly the console order). Clean no-op until the GUI module is prepared.
    // This is how BootLegal's Title_Screen02 movie reaches the screen. [GUI render path]
    if (BrnGui::gpActiveGuiModule != 0)
        BrnGui::gpActiveGuiModule->Render(&mIm2dRenderer);

    // (gameplay-render passes here when reconstructed; gated off during the loading screen)

    mLoadingScreenRenderer.RenderForeground(&mIm2dRenderer);

    // Full-screen movie presentation. FLAG PC-platform: on the X360 the movie frame is
    // drawn inside the GUI pass (UpdateAndRenderMovieManager) and the XMV presentation
    // then owns the screen ABOVE the whole 2D frame -- the boot logos play over the
    // still-latched loading screen (BootVideos @0x82478778 posts no hide; the first 20
    // is BootLegal::OnEnter's). The PC FFmpeg substitute has no overlay plane, so its
    // presentation quad draws here, after the loading-screen foreground, to reproduce
    // that layering. The manager's Update stays in its real GUI-pass home.
    if (BrnGui::gpActiveMovieManager != 0)
    {
        // The XMV presentation owns the screen for the WHOLE video cycle, not just the
        // frames a picture is up: the console shows BLACK between the boot logos (player
        // teardown + the 10+10-frame memory-return delays before the next video is
        // queued) and across each crossfade tail -- never the latched loading screen.
        // The PC stand-in reproduces that ownership with an opaque black underlay while
        // the manager's presentation cycle is active (IsMoviePresentationActive), held
        // for a short linger past the cycle's end to cover the event-queue hops between
        // one video's finish-report and the next play command (logo -> logo) or the
        // title state's hide/589-overlay takeover (last logo -> BF_LEGAL).
        const bool lbPresenting = BrnGui::gpActiveMovieManager->IsMoviePresentationActive();
        const u32  lu32PresentNow  = CgsSystem::GetSystemTimerBaseTime();
        const u32  lu32PresentFreq = CgsSystem::GetSystemTimerFrequency();
        if (lbPresenting)
        {
            gu32LastMoviePresentTick = lu32PresentNow;
            gbMoviePresentTickValid  = true;
        }
        const bool lbOwnsScreen = lbPresenting ||
            (gbMoviePresentTickValid && lu32PresentFreq != 0u &&
             (lu32PresentNow - gu32LastMoviePresentTick) < lu32PresentFreq / 4u);
        // [diag] BRN_IM2D_TRACE: surface the underlay latch state on the same cadence as
        // the Im2d draw trace (queued id + manager state + owns-screen).
        {
            static int siTrace = -1;
            if (siTrace < 0)
            {
                char lacBuf[8];
                siTrace = (GetEnvironmentVariableA("BRN_IM2D_TRACE", lacBuf, sizeof(lacBuf)) > 0) ? 1 : 0;
            }
            if (siTrace == 1 && (renderengine::guPresentCount % 60u) == 0u)
            {
                char lacMsg[160];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[MovieOwn] f=%u presenting=%d owns=%d queued=%d state=%d\n",
                              renderengine::guPresentCount, lbPresenting ? 1 : 0, lbOwnsScreen ? 1 : 0,
                              BrnGui::gpActiveMovieManager->IsMovieQueued() ? 1 : 0,
                              static_cast<s32>(BrnGui::gpActiveMovieManager->GetState()));
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
        if (lbOwnsScreen)
        {
            const CgsGraphics::RGBA8 KC_MOVIE_BLACK = { 0, 0, 0, 255 };
            mIm2dRenderer.BeginRendering();
            mIm2dRenderer.SetState(static_cast<const CgsGraphics::BlendState*>(nullptr));
            mIm2dRenderer.SetTexture(nullptr);   // untextured -> solid vertex colour
            EmitColouredQuad(&mIm2dRenderer, 0.0f, 0.0f, 1280.0f, 720.0f, KC_MOVIE_BLACK);
            mIm2dRenderer.EndRendering();
        }
        BrnGui::gpActiveMovieManager->Render(&mIm2dRenderer);
    }

    // Debug HUD overlay (the on-screen perf squares) - drawn on top of the loading screen, before the
    // present. The debug manager is the BrnGameModule-owned singleton (constructed at boot); the X360
    // Render path issues this each frame between the foreground overlay and ShowPixelBuffer. RenderWorld
    // (3D) is deferred, so the view/camera args are unused; the 2D buffer is the real Im2d the loading
    // screen renders through (mIm2dRenderer).
    if (CgsDev::DebugManager* lpDebugManager = CgsDev::DebugManager::ThreadSafeAquire())
    {
        Matrix44 lViewProjection;
        lViewProjection.SetIdentity();
        Vector3 lCameraPosition;
        lCameraPosition.SetZero();
        lpDebugManager->Render(lViewProjection, lCameraPosition, nullptr, &mIm2dRenderer);
        CgsDev::DebugManager::ThreadSafeRelease(lpDebugManager);
    }

    // The three per-thread monitor squares (X360 RenderThreeThreadMonitors). The real per-thread
    // "running in real time" flags need the threading system (deferred), so they are derived here from
    // the present-to-present frame time - matching the observed behaviour (green at framerate, reddening
    // as the game/CPU slows). The X360 gates this on a debug-display flag.
    {
        const u32 lu32Now  = CgsSystem::GetSystemTimerBaseTime();
        const u32 lu32Freq = CgsSystem::GetSystemTimerFrequency();
        f32 lfFrameMs = 0.0f;
        if (gbMonitorTickValid && lu32Freq != 0u)
            lfFrameMs = static_cast<f32>(static_cast<double>(lu32Now - gu32LastMonitorTick) * 1000.0 / static_cast<double>(lu32Freq));
        gu32LastMonitorTick = lu32Now;
        gbMonitorTickValid  = true;

        const f32 lfBudgetMs = 1000.0f / 60.0f;
        s32 liBehind = 0;
        if (lfFrameMs > lfBudgetMs * 1.10f) liBehind = 1;
        if (lfFrameMs > lfBudgetMs * 1.50f) liBehind = 2;
        if (lfFrameMs > lfBudgetMs * 2.00f) liBehind = 3;
        RenderThreeThreadMonitors(liBehind < 3, liBehind < 2, liBehind < 1);
    }

    // UnlockForRead @0x8240E304 -- the end of the console's frame-long read window (taken above).
    if (lpDispatchThreadInputBuffer != 0)
        lpDispatchThreadInputBuffer->UnlockForRead();

    renderengine::Device::ShowPixelBuffer();
}

// Renders the on-screen assert overlay (forwarded from BrnGameModule::RenderAssert). The real
// body draws the assert text via the immediate-mode renderer; minimal until the assert overlay
// path is reconstructed (asserts are inert on the boot/loading path).
void BrnRendererModule::RenderAssert(const AssertData* /*lpAssertData*/)
{
}
