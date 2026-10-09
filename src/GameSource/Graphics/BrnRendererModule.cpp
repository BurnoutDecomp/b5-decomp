#include <algorithm>
#include "pc/gcm/renderengine/MeshJobOwnerWaitPCLeaf.h"
#include "GameSource/Game/BrnGameModule.hpp"
#include "GameShared/GameClasses/Core/CgsAssertProbePC.h"
#include "GameSource/Graphics/BrnRendererModule.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include "pc/gcm/renderengine/MeshPreparationPCLeaf.h"
#include "pc/gcm/renderengine/device.h"   // renderengine::Device frame bracket
#include "GameShared/GameClasses/Development/BrnDiagFilmLatch.h" // optional repair-frame observation
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/jobs.h"
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"           // CgsRenderTarget::GetDepthTexture (the s15 bind)
#include "GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"  // shadow::Device::SetResource (the global texture binds)
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"                      // renderengine::PCSurfaceBracket_* (the scene-target bracket)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"  // CgsDev::DebugManager (debug HUD overlay)
#include "GameSource/Gui/BrnGuiModule.h"         // BrnGui::gpActiveGuiModule (the GUI render drive)
#include "GameSource/Graphics/BrnEffectsArbitrator.h"      // BrnGraphics::EffectsArbitrator
#include "SharedClasses/Graphics/BrnEffectsData.h"         // BrnEffectsFrame + the five data blocks
#include "GameSource/Graphics/PostFx/BrnPostFx.h"          // msPostFx + every setter/getter the apply block drives
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"  // ColourCube::GetSize
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttributeKey.h"  // Attrib::StringToKey ("198102", the base-frame vignette asset)
#include "GameSource/Director/Camera/Camera.h"            // BrnDirector::Camera::Camera -- the staged camera-input record
#include "GameSource/Effects/Particles/ParticleModule.h"     // BrnParticle::ParticleModule::RenderFullResParticles (the full-res particle pass)
#include "GameShared/GameClasses/Development/BrnDiagBoundSurfaces.h"  // [diag] BrnDiag::LogBoundSurfaces (the pass-boundary RT probe)
#include "pc/gcm/renderengine/renderstates.h"    // renderengine::TextureState::Parameters (the sampler-13 env-map state)

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
// (The two-step this paragraph used to describe is CLOSED, twice over. The safety it named --
// WorldModule::PublishWorldShadingConstantsBringUp force-writing a shadows-off c14/c15/c16
// block every frame, which pinned the shadow factor at 1.0 -- was retired with the shadow
// producer on 2026-08-12, and the whole bring-up publisher went with it in the env-manager
// go-live wave on 2026-08-16: WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410 is
// the producer now and it does not write c14..c17 at all (BrnWorldModule.cpp:4792-4794), so
// the retirement holds BY CONSTRUCTION rather than by ordering. The real writers are
// ShadowMap::SetConstants' slots 15/16 (BrnShadowMap.cpp:372/375) and the cascades reach the
// screen.

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
//      trusting a build.
//
//      ⚠ UPDATED 2026-08-16 (anti-aliasing wave): ALL FOUR NOW RUN. This paragraph used to end
//      "on this build only _Resolve actually runs (mbMultisampledBackbuffer is false everywhere ...
//      the other three are correct and dead)". That was true only because the tree's inline
//      BrnRendererModule::Construct initialised mbMultisampledBackbuffer to `false` -- a DEFECT: the
//      X360 stores 1 (`li r28, 1` @0x8240A7B4 / `stb r28, 0(this+0xC400)` @0x8240A7C4) and hands the
//      same byte to the pool as lbEnableMSAA. With that corrected (BrnRendererModule.h) both bodies
//      below take their TILED branch, so BeginTiling opens the frame with the two-rect clear,
//      SetPredication is called three times a frame, the two per-tile resolve pairs run, and
//      EndTiling closes. On PC the two rectangles of BrnGraphics::KMSAA_TILING_PLAN partition a
//      1280x720 surface EXACTLY, so "per tile" degrades to "over its own band" with no gap.
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
// BRN_ENVMAP_PASS_AVAILABLE -- THE ENVIRONMENT-MAP (CAR REFLECTION) PASS (reflections step 1,
// 2026-08-17).
//
// At 1 this file carries BrnRendererModule::BeginRenderEnvironmentMapFace @0x823F63E0,
// EndRenderEnvironmentMapFace @0x823FC5E8 and Render's own six-face loop
// (@0x8240BFA8 pseudocode 645-722, asm 0x8240CB64-0x8240CD8C), at the console's own position:
// after the shadow-map pass and BEFORE BeginRenderAntiAliased. It also unparks the sampler-13
// bind that has been a documented hole since the shadow wave.
//
// WHAT REVERTING TO 0 LEAVES BEHIND, measured rather than claimed. Both positions were
// preprocessed (cl /P) and diffed against HEAD, ignoring #line/#pragma and blank lines; at 0 this
// translation unit differs from HEAD by exactly TWO things and nothing else:
//   * the header DECLARATIONS this wave adds -- GetEnvMapBuffer, PCBringUpCreateEnvMapBuffer,
//     Begin/EndRenderEnvironmentMapFace, gEnvironmentMap, the third Device::Clear overload,
//     Target::Resolve(u32), CgsRenderTarget::GetMultisampleFormat, shadow::Device::
//     Lock/UnlockDepthStencilState. Declarations emit no code and add no link requirement.
//   * PublishSkyConstantsBringUp's env-map half (the six face matrices + mEnvMapViewPosition),
//     which is deliberately NOT under this macro: it completes the copy of the world's frame and
//     is inert with nothing to consume it. Reverting the macro does not revert that, by design.
// The loop, the two bodies, the pool accessor's USE, the s13 bind and the knob seed are all
// inside the `#if` and all disappear.
//
// ⚠ THIS TEXT HAS NEVER BEEN COMPILED AT 0 IN A SHIPPED TREE. It is authored at 1 and gated so the
// conductor has a one-character revert, not so it can sit dark: the anti-alias wave's banner above
// records what happens when a body ships behind a 0 (the per-TU gate, the lint and the reviewer all
// pass on text nobody compiled). Both positions ARE compiled in this wave's gate.
//
// WHAT IT NEEDS THAT THIS TU DOES NOT OWN -- three symbols, all cross-group in the same wave:
//   (a) rw::graphics::postfx::Target::Resolve(u32 luFace)  -- the PER-FACE overload, X360
//       0x823F9170, owned by the `cubeleaf` group (rwgpfxrendertarget.h + the D3D9 leaf). On the
//       Xenos every face renders into the SAME EDRAM tile and the face only matters at resolve
//       time, which is exactly why CgsRenderTarget::SetRenderTargetStateInvertDepth binds section
//       0 regardless of its argument (CgsRenderTarget.cpp:336). EndRenderEnvironmentMapFace is
//       its only caller here.
//   (b) BrnGame::DispatchThreadInputBuffer::GetEnvMapFaceRender(u32) const -- owned by the
//       `envproducer` group. The console reads the same byte inline, WITH a bounds assert: Render
//       @0x8240CC04 does `cmplwi r29, 6 / blt` else FireAssert("luIndex<6",
//       "..\GameSource\Game/BrnDispatchThreadInputBuffer.h", 0xF4) -- so the console's own
//       accessor is an inline header getter at h:244 with that assert, which is what the seam
//       reproduces. The byte itself is buffer+0x99B4 (`addis r17, r11, 1 / addi r17, r17, -0x664C`
//       @0x8240CB94/0x8240CBC0, i.e. +0x10000-0x664C), which is mabEnvMapFaceRender[6] --
//       BrnDispatchThreadInputBuffer.h:194, six bytes ending exactly where mbIsWriteBuffer starts
//       at +0x99BA.
//   (c) renderengine::Device::Clear(const ClearColorParameters&, const ClearDepthStencilParameters&,
//       ETargetId) -- X360 0x82B61E18, DECLARED by this wave in pc/gcm/renderengine/device.h beside
//       its two already-declared siblings; its PC body belongs beside DeviceClearDepthStencil (the
//       depth/stencil-only member of the same console family). See CROSS-GROUP in the wave report.
// A missing (a)/(b)/(c) is an LNK2019, not a wrong picture: `cl /c` cannot see them.
#define BRN_ENVMAP_PASS_AVAILABLE 1

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

#include "GameSource/Gui/BrnGuiMovieManager.h"   // BrnGui::gpActiveMovieManager (the PC presentation draw)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [diag] BRN_IM2D_TRACE probes
#include "rw/rwcore_structs.h"                   // rw::LinearResourceAllocator (world dispatch bin memory)
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"  // ShaderConstantTable::BeginFrame (StartOfFrame)
#include <Windows.h>   // [diag] GetEnvironmentVariableA
#include <cstdio>      // [diag] snprintf
#include <cstring>     // memcpy (per-pass DispatchObjectContext copies)
#include <cmath>       // std::sqrt (PCBringUpPublishCoronaCamera's two projection scalars)
#include <new>         // world dispatch bring-up heap
#include <cstdlib>     // [diag] std::getenv (BRN_LION_QRES_OFF, the quarter-res A/B pin)

// [diag] present counter (device.cpp) - stamps the trace lines with their frame.
namespace renderengine { extern u32 guPresentCount; extern bool gbDiagLastPresentBlack; }   // [DIAG] issue #30

// High-res frame timer (CgsTimeUtils.cpp), forward-declared - drives the thread-monitor health.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }
namespace CgsSystem { u64 GetSystemTimerBaseTime64(); u64 GetSystemTimerFrequency64(); }   // the console's width (CgsTimeUtils.cpp)

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

#if BRN_ENVMAP_PASS_AVAILABLE
    // --- BrnRendererModule::BeginRenderEnvironmentMapFace @0x823F63E0's four clear constants ------
    // Each is matched to the DISPLACEMENT the assembly addresses, and each is a value this file
    // ALREADY attests at that displacement for another consumer, which is why none of them is a
    // fresh read:
    //   flt_82004740 +0x00 == 0x3E99999A == 0.300000012f -- addressed by `lfs f0, flt_82004740@l(r11)`
    //     @0x823F6494 and multiplied by the white level (`fmuls f0, f31, f0` @0x823F649C). The SAME
    //     displacement is KF_BACKGROUND_COLOUR_GREY below (BeginRenderAntiAliased @0x823FFA44).
    //   flt_82001CC0 +0x00 == 0.0f -- `lfs f13, flt_82001CC0@l(r11)` @0x823F64B0, stored to the clear
    //     colour's ALPHA (@0x823F64B8) and to the depth field (@0x823F64C0). Same symbol as
    //     KF_QUARTER_RES_CLEAR_COMPONENT above.
    // The depth constant is spelled separately from the alpha one on purpose: they are the same
    // number today because the console loads one register twice, but they answer two unrelated
    // questions (what an unwritten reflection texel's alpha is; where the inverted-depth far plane
    // is), and a later correction to either must not silently move the other. The two EDRAM-era
    // helpers in this file are commented the same way for the same reason.
    const f32 KF_ENV_MAP_CLEAR_COLOUR_SCALE = 0.3f;   // flt_82004740 +0x00 -- the SAME .rdata word as
                                                      // KF_BACKGROUND_COLOUR_GREY below (the env-map
                                                      // clear IS the background grey x white level);
                                                      // kept as its own name deliberately so a later
                                                      // correction to one cannot silently move the
                                                      // other (verify F13)
    const f32 KF_ENV_MAP_CLEAR_ALPHA        = 0.0f;   // flt_82001CC0 +0x00
    const f32 KF_ENV_MAP_CLEAR_DEPTH        = 0.0f;   // flt_82001CC0 +0x00, the INVERTED far plane

    // The XENON D3DCLEAR_* bits. NOT PC Direct3D 9's: the console reserves bits 0..3 for its four
    // colour targets and puts ZBUFFER at 0x10 / STENCIL at 0x20, which is why the clear word is 0x30
    // and not 0x6. The same two constants are already modelled twice in this tree -- once in
    // ShadowPassPCLeaf.h's ClearDepthStencilParameters banner and once in ImmediateModePCLeaf.cpp's
    // DeviceClearDepthStencil, which translates them -- so these are named here rather than spelled
    // as a literal 0x30 that a reader would have to re-derive.
    const u32 KU_XENON_CLEAR_ZBUFFER = 0x10u;
    const u32 KU_XENON_CLEAR_STENCIL = 0x20u;
#endif  // BRN_ENVMAP_PASS_AVAILABLE

#if BRN_ENVMAP_PASS_AVAILABLE
    // The first of the six ENV-MAP mesh lists. Render @0x8240CCA4 forms the list id as
    // `addi r4, r29, 5` with r29 the face index, i.e. GetList(5 + face) -- lists 5..10. The world
    // producer fills the same six (WorldModule::GenerateDispatchLists @0x827D1CE8 ->
    // WorldEntityModule::GenerateDispatchListsForEnvironmentMap into list 5+face). They are the
    // env-map SHADER-LOD lists, so their materials are the cheap permutations, which is why six
    // 128x128 faces are affordable at all.
    const u32 KU_ENV_MAP_FIRST_MESH_LIST = 5u;
#endif  // BRN_ENVMAP_PASS_AVAILABLE

    u64  gu64LastMonitorTick = 0;
    bool gbMonitorTickValid  = false;

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
    // FLAG PC-platform leaf: native pointers enlarge 2D commands. The GUI's
    // existing 512 KiB streams bound the main buffer; debug retains ARTIST's
    // 2 MiB vertex stream and doubles its 80 KiB command budget for x64.
    const u32 KU_PC_IM2D_COMMAND_BYTES = 512u * 1024u;
    const u32 KU_PC_IM2D_VERTEX_BYTES = 512u * 1024u;
    const u32 KU_PC_IM2D_DEBUG_COMMAND_BYTES = 160u * 1024u;
    const u32 KU_PC_IM2D_DEBUG_VERTEX_BYTES = 2u * 1024u * 1024u;
    const u32 KU_PC_IM3D_COMMAND_BYTES = 0x400u;
    const u32 KU_PC_IM3D_VERTEX_BYTES = 0x8000u;
    const u32 KU_PC_IM3D_UNTEX_COMMAND_BYTES = 0x4000u;
    const u32 KU_PC_IM3D_UNTEX_VERTEX_BYTES = 0x80000u;
    const u32 KU_PC_IM3D_DEBUG_COMMAND_BYTES = 0x100000u;
    const u32 KU_PC_IM3D_DEBUG_VERTEX_BYTES = 0x100000u;
    const u32 KU_PC_IM3D_RACE_COMMAND_BYTES = 0x20000u;
    const u32 KU_PC_IM3D_RACE_VERTEX_BYTES = 0x20000u;
    const u32 KU_PC_IM3D_MENUS_COMMAND_BYTES = 512u;
    const u32 KU_PC_IM3D_MENUS_VERTEX_BYTES = 4u;

    bool MeshPreparationEnabledPC()
    {
        static const bool sbEnabled = [] {
            const char* lpValue = std::getenv("BRN_MESH_PREPARE");
            return !lpValue || lpValue[0] != '0';
        }();
        return sbEnabled;
    }

    bool EnsureWorldDispatchAllocator()
    {
        if (sbWorldDispatchAllocatorReady)
            return true;

        const u32 luHeapBytes = (MeshPreparationEnabledPC() ? 2u : 1u) * KU_PC_DISPATCH_BIN_BYTES
                              + 2u * KU_PC_GDL_DISPATCH_BIN_BYTES
                              + 2u * (KU_PC_IM2D_COMMAND_BYTES + KU_PC_IM2D_VERTEX_BYTES
                                    + KU_PC_IM2D_DEBUG_COMMAND_BYTES + KU_PC_IM2D_DEBUG_VERTEX_BYTES)
                              + 2u * (KU_PC_IM2D_DEBUG_COMMAND_BYTES + KU_PC_IM2D_DEBUG_VERTEX_BYTES) // modal banks
                              // Every prepared3D buffer shares this allocator.
                              + 2u * (KU_PC_IM3D_COMMAND_BYTES + KU_PC_IM3D_VERTEX_BYTES
                                    + KU_PC_IM3D_UNTEX_COMMAND_BYTES + KU_PC_IM3D_UNTEX_VERTEX_BYTES
                                    + KU_PC_IM3D_DEBUG_COMMAND_BYTES + KU_PC_IM3D_DEBUG_VERTEX_BYTES
                                    + KU_PC_IM3D_RACE_COMMAND_BYTES + KU_PC_IM3D_RACE_VERTEX_BYTES
                                    + KU_PC_IM3D_MENUS_COMMAND_BYTES + KU_PC_IM3D_MENUS_VERTEX_BYTES)
                              + 32u * 128u // four allocations per each of eight immediate buffers
                              + (4u * 4096u)   // per-bin align128(size)+128 slop + headroom
                              + (192u * 1024u); // + the small renderengine objects that share
                                               //   this allocator (the sky dome's four buffer
                                               //   headers); without it their DoAllocate came
                                               //   back empty and tripped CreateGeometry's
                                               //   GetMemoryResource asserts. Raised 64->192 KB
                                               //   for the corona manager's TWO 32,784-byte
                                               //   corona buffers (512 slots x 64 B + header)
                                               //   so its Construct does not starve the sky dome.
        void* lpHeap = ::operator new(luHeapBytes, std::nothrow);
        if (lpHeap == 0)
            return false;

        rw::Resource lHeapResource;
        rw::ResourceDescriptor lHeapCapacity;
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lHeapResource.m_baseResources[luLane] = (luLane == 0) ? lpHeap : 0;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_size      = (luLane == 0) ? luHeapBytes : 0u;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_alignment = (luLane == 0) ? 128u : 1u;
        }
        sWorldDispatchAllocator.Initialize(lHeapResource, lHeapCapacity);
        sbWorldDispatchAllocatorReady = true;
        return true;
    }

    // FLAG PC-platform leaf: a failed native allocation has no dispatchable
    // command bank. Do not Reset it and attempt a key-block store through null.
    bool DispatchStorageAvailablePC(CgsGraphics::DispatchFrame* lpFrame)
    {
        return lpFrame && lpFrame->GetBin().GetBase() != nullptr;
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

    // ============================================================================================
    // [FLAG PC bring-up] BRING THE SUN CORONA UP (coronas step 2, 2026-08-18).
    //
    // CONSOLE POSITION: BrnRendererModule::Construct @0x8240A778 calls
    // `mSunCorona.Construct(<the global graphics allocator>)`, and BrnRendererMemory::Construct
    // @0x823FCA38 builds pool slot 10 through CreateSunCoronaBuffer @0x823F73C8. NEITHER is
    // reachable on this build -- BrnRendererModule::Construct is not reconstructed and
    // BrnResource::Allocators::GetGlobalGraphicsAllocator() is declaration-only -- which is the
    // same pair of blockers EnsureCoronaManagerBringUp / EnsurePostFxSceneTargets / EnsureEnvMapTarget
    // already carry.
    //
    // So both halves come up here, on the first frame that HAS a device and a down-sample buffer,
    // over the same sWorldDispatchAllocator every other deferred subsystem uses. VALUE-LATCHED on
    // IsConstructed() and on the pool slot, never on a `static bool tried`: a one-shot flag set
    // during the loading screen -- before renderengine::gDevice exists -- would burn the single
    // attempt (EnsureShadowMapTarget's banner, above, records that bug).
    //
    // THE DOWN-SAMPLE BUFFER IS A PRECONDITION, not an ordering assumption: it is the target whose
    // depth texture the occlusion taps sample, and PCBringUpCreateSunCoronaBuffer asserts the
    // slot-nulling bring-up has already run. Render calls EnsurePostFxSceneTargets before this
    // point on every path; testing the SLOT rather than calling that function again is what keeps
    // this gate independent of it.
    //
    // DELETE-WHEN BrnRendererModule::Construct and BrnRendererMemory::Construct are reconstructed.
    // ============================================================================================
    bool EnsureSunCoronaBringUp(BrnRendererMemory& lrRendererMemory, BrnSunCorona& lrSunCorona)
    {
        if (lrSunCorona.IsConstructed() && lrRendererMemory.GetSunCoronaBuffer() != nullptr)
            return true;
        if (renderengine::gDevice == 0)
            return false;                                  // no device -> no D3D9 objects yet
        if (lrRendererMemory.GetShadowMapBuffer(0) == nullptr)
            return false;                                  // the slot-nulling bring-up has not run
        if (lrRendererMemory.GetDownSampleBuffer() == nullptr)
            return false;                                  // no depth source to measure against
        if (!EnsureWorldDispatchAllocator())
            return false;

        if (lrRendererMemory.GetSunCoronaBuffer() == nullptr)
        {
            lrRendererMemory.PCBringUpCreateSunCoronaBuffer(&sWorldDispatchAllocator);
        }

        if (!lrSunCorona.IsConstructed())
        {
            lrSunCorona.Construct(&sWorldDispatchAllocator);

            // ---- THE SUN-CORONA SWITCH, seeded from config.ini ---------------------------------
            // BrnSunCorona::Construct sets mbRenderSunCorona true, which IS the console's value
            // (`stb r27(=1), 0x51(r31)` @0x824009EC). The knob only SEEDS that member, once, right
            // where Construct wrote it -- NO SECOND SWITCH IS MINTED (AGENTS.md rule 3); the
            // console's other writer is BrnGraphics::DebugComponent, which is an empty placeholder
            // in this tree. DELETE-WHEN the debug component owns the switch.
            lrSunCorona.PCBringUpSetRenderSunCorona(renderengine::gSunCorona != 0);
            if (renderengine::gSunCorona == 0)
            {
                CgsDev::Log::WriteToLog("[suncorona] config.ini [Settings] SunCorona=0 --"
                                        " the sun-corona pass is OFF\n");
            }
        }

        const CgsRenderTarget* const lpBuffer = lrRendererMemory.GetSunCoronaBuffer();
        if (lpBuffer == nullptr || !lrSunCorona.IsConstructed())
            return false;                                  // both halves reported why; retry next frame

        {
            // AGENTS.md rule 9: print the EXTENT and refuse a degenerate target rather than
            // rendering into nothing and erroring nowhere. One line, latched.
            static bool sbLoggedBuffer = false;
            if (!sbLoggedBuffer)
            {
                sbLoggedBuffer = true;
                char lacMessage[192];
                std::snprintf(lacMessage, sizeof(lacMessage),
                              "[suncorona] buffer created %ux%u fmt=0x%08X target=%p\n",
                              (unsigned)lpBuffer->GetWidth(), (unsigned)lpBuffer->GetHeight(),
                              (unsigned)0x18280186u,
                              (const void*)lpBuffer->GetRenderTarget());
                CgsDev::Log::WriteToLog(lacMessage);
            }
        }
        return lpBuffer->GetWidth() != 0u && lpBuffer->GetHeight() != 0u
            && lpBuffer->GetRenderTarget() != 0;
    }

    // ============================================================================================
    // [FLAG PC bring-up] BUILD THE QUARTER-RES PARTICLE CHAIN (2026-09-05).
    //
    // Pool slot 9 + the four blit shader programs. Same shape, same latching rule and the same two
    // blockers as EnsureSunCoronaBringUp above: VALUE-LATCHED on what actually exists, never on a
    // `static bool tried` (a one-shot flag set during the loading screen, before renderengine::
    // gDevice exists, burns the single attempt -- EnsureShadowMapTarget's banner records that bug).
    //
    // THE DOWN-SAMPLE BUFFER IS A PRECONDITION and not an ordering assumption: BeginQuarterResBuffer
    // blits ITS depth into the particle buffer, and BlitComposite reads its extent for the
    // half-texel offsets. Render calls EnsurePostFxSceneTargets before this point on every path;
    // testing the SLOT keeps this gate independent of that.
    //
    // DELETE-WHEN BrnRendererMemory::Construct is callable -- it builds slot 9 and compiles the
    // four blit programs itself.
    // ============================================================================================
    bool EnsureQuarterResParticleChain(BrnRendererMemory& lrRendererMemory)
    {
        // [FLAG PC bring-up diagnostic] BRN_LION_QRES_OFF=1 keeps the chain from ever coming up,
        // so ONE build can be run twice differing in ONE SCALAR: with it, the Lion pass takes the
        // full-res arm and adds onto the scene (the pre-2026-09-05 behaviour); without it, it
        // accumulates on the cleared quarter-res buffer and is composited over the scene. That
        // is the A/B the wave's position-matched film compares, and it is the same discipline
        // BRN_LION_WHITE_PIN carried for the white-level fix. Registered in flow_run.ps1's
        // DEFAULT-RUN clear list in the SAME change. DELETE-WHEN STABLE.
        static const bool sbQResPinnedOff = (std::getenv("BRN_LION_QRES_OFF") != 0);
        if (sbQResPinnedOff)
        {
            static bool sbLoggedPin = false;
            if (!sbLoggedPin)
            {
                sbLoggedPin = true;
                CgsDev::Log::WriteToLog("[qres] BRN_LION_QRES_OFF=1 -- the quarter-res particle"
                                        " chain is PINNED OFF; the Lion pass takes the full-res"
                                        " arm and adds straight onto the scene\n");
            }
            return false;
        }

        if (lrRendererMemory.PCBringUpParticleCompositeChainReady())
            return true;
        if (renderengine::gDevice == 0)
            return false;                                  // no device -> no D3D9 objects yet
        if (lrRendererMemory.GetShadowMapBuffer(0) == nullptr)
            return false;                                  // the slot-nulling bring-up has not run
        if (lrRendererMemory.GetDownSampleBuffer() == nullptr)
            return false;                                  // no depth source, no composite extent
        if (!EnsureWorldDispatchAllocator())
            return false;

        return lrRendererMemory.PCBringUpCreateParticleCompositeChain(&sWorldDispatchAllocator);
    }

    // ============================================================================================
    // [FLAG PC bring-up] CONSTRUCT THE CORONA MANAGER (coronas step 1, 2026-08-17).
    //
    // CONSOLE POSITION: BrnRendererModule::Prepare's eRendererPrepareCoronas stage
    // (BrnRendererModule.h:214) runs `mCoronaManager.Construct(<the global graphics allocator>)`,
    // and PrepareAgain @0x823FF8F8's corona-atlas argument then reaches
    // `mCoronaManager.SetTextureAtlas(allocator, atlas)`. NEITHER is reachable on this build:
    // BrnRendererModule::Prepare is not reconstructed at all, and
    // BrnResource::Allocators::GetGlobalGraphicsAllocator() is declaration-only (the same blocker
    // EnsureEffectsArbitratorBringUp and the render-target pair already carry).
    //
    // So the manager comes up here, on the first frame that HAS a device and an atlas, over the
    // same sWorldDispatchAllocator every other deferred subsystem uses. VALUE-LATCHED on
    // IsConstructed(), not on a `tried` bool, for the reason EnsureShadowMapTarget's banner gives:
    // a one-shot flag set during the loading screen would burn the single attempt before the
    // device (and the atlas) existed.
    //
    // DELETE-WHEN BrnRendererModule::Prepare is reconstructed: then Construct runs at the console's
    // own point in the prepare state machine and PrepareAgain calls SetTextureAtlas directly.
    // ============================================================================================
    // The corona atlas texture PrepareAgain is handed (`corona_atlas.TextureConfig2d?ID=297312`,
    // pool slot 10, acquired by BrnGameModule::GamePrepare and asserted non-null there).
    renderengine::Texture* gpCoronaAtlasTexture = nullptr;

    bool EnsureCoronaManagerBringUp(BrnCoronaManager& lrCoronaManager)
    {
        if (lrCoronaManager.IsConstructed())
            return true;
        if (renderengine::gDevice == 0)
            return false;                      // no device -> no D3D9 vertex declaration yet
        if (gpCoronaAtlasTexture == nullptr)
            return false;                      // GamePrepare has not delivered the atlas yet
        if (!EnsureWorldDispatchAllocator())
            return false;

        lrCoronaManager.Construct(sWorldDispatchAllocator);
        if (!lrCoronaManager.IsConstructed())
            return false;                      // Construct reported why; retry next frame

        // PrepareAgain's own call, at the console's position in the order (Construct first, then
        // the atlas -- Construct clears m_textureStateAtlas and SetTextureAtlas fills it).
        lrCoronaManager.SetTextureAtlas(sWorldDispatchAllocator, gpCoronaAtlasTexture);
        return lrCoronaManager.IsConstructed();
    }

    // ============================================================================================
    // [FLAG PC bring-up] PUBLISH THE CORONA CAMERA.
    //
    // Stands in for the block BrnRendererModule::Update @0x82405EC4-0x82405FA0 runs on the console,
    // which is BrnSubmissionInterface::SetCameraInfo plus the three values it is handed. The console
    // block, decoded instruction by instruction:
    //     0x82405ED0  lfs  f13, 0x58(camera)              camera->mfFOV
    //     0x82405ED8  fcmpu / ble                          if (mfFOV <= flt_82004014) skip everything
    //                                                      (flt_82004014 == 0.1f -- DATA_DUMP.md)
    //     0x82405EF0  bl   CopyToCgsCamera(&lCgs)
    //     0x82405EF8  lfs  f0,  lCgs+0x144                 maProjectionScalars[1] ==
    //                                                        m_oneOverTanHalfFovHorizontal
    //     0x82405F00  lfs  f12, lCgs+0x150                 maProjectionScalars[4] ==
    //                                                        m_oneOverTanHalfFovVertical
    //     0x82405F04  fdivs f10, f12, f0                   ootV / ootH
    //     0x82405F10  fsubs f11, f0, 1.0                 \ fsel f0, f11, f0, 1.0
    //     0x82405F20  fsel  f0,  f11, f0, f13            / == max(ootH, 1.0f)
    //     0x82405F24  stfs  f0            -> viewXyScale.x
    //     0x82405F28  fmuls f0, f10, f0                    (ootV / ootH) * max(ootH, 1.0f)
    //     0x82405F2C  stfs  f0            -> viewXyScale.y
    //     0x82405F18/1C stfs flt_82001CC0 -> viewXyScale.z / .w   (== 0.0f)
    //     0x82405F48..F98  the four mViewProjection rows from lCgs+0x80..+0xBF
    //     0x82405F54/F9C   `lvx128 v0, camera, 0x30` -> the camera position (Camera.h GetPosition's
    //                                                    own "X360 +48" lane)
    // The two scalars ARE the projection's screen-space x/y scale (1/tan(fov/2) each), which is what
    // the corona vertex program multiplies the quad corner offsets by -- so viewXyScale is
    // "world units -> clip units" for the billboard expansion, and the max(.,1) is a lower clamp
    // that only bites on a fov wider than 90 degrees horizontally.
    //
    // ⚠ WHAT DEVIATES, AND WHY (both halves flagged, neither invented):
    //   * THE CAMERA OBJECT. Not a LINK problem -- BrnDirector::Camera::Camera::{GetFOV,
    //     GetPosition, CopyToCgsCamera} are all bodied in GameSource/Director/Camera/Camera.cpp
    //     (:179 / :562 / :508) and that file IS on tools/build/build_game_exe.bat (line 394). The
    //     problem is that there is no camera to read: Update takes its camera from the RendererIO
    //     INPUT buffer, which is created empty at its ONE call site (BrnGameModule.cpp:2647,
    //     GamePrepare's not-done tail) because the dispatch IO buffer set is not real on this build.
    //     The live substitute is gBrnSkyCameraBringUp, filled every dispatch by
    //     WorldModule::GenerateDispatchListsBringUp from the SAME camera the world and sky draw
    //     with (BrnWorldModule.cpp:5554-5556), so the view-projection and the eye are exact.
    //   * THE TWO FOV SCALARS have no substitute member, so they are RECOVERED FROM THE MATRIX
    //     rather than guessed: for a row-vector VP = view * perspective with an ORTHONORMAL view
    //     basis (CgsCamera.h:244 states the cameras this engine builds are orthonormal), the length
    //     of VP's first column is exactly 1/tan(fovH/2) and of its second column 1/tan(fovV/2) --
    //     the perspective scale survives the orthonormal rotation. This is an identity, not a
    //     heuristic, and the log line below prints both numbers once so a boot can check them
    //     against the expected pair (a 1280x720 60-degrees-horizontal camera reads ~1.73 / ~3.08).
    // DELETE BOTH when Camera.cpp mounts: then this whole function is the console's ten instructions.
    // ============================================================================================
    const f32 KF_CORONA_CAMERA_FOV_GATE = 0.1f;   // X360 flt_82004014 (DATA_DUMP.md)



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

    // lbEnableMSAA is BrnRendererModule::mbMultisampledBackbuffer, threaded from the caller instead
    // of re-derived here: on the console that ONE byte both selects the anti-alias buffer's tiling
    // plan (via BrnRendererMemory::Construct's arg_97 -> CreateAntiAliasBuffer @0x823F6B40) and
    // picks the branch BeginRenderAntiAliased / ResolveMSAA take. Deriving it twice is how a
    // multisampled target ends up driven by the untiled bracket, which nothing would report.
    bool EnsurePostFxSceneTargets(BrnRendererMemory& lrRendererMemory, bool lbEnableMSAA)
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
        lrRendererMemory.PCBringUpCreatePostFxSceneTargets(&sWorldDispatchAllocator, lbEnableMSAA);

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

#if BRN_ENVMAP_PASS_AVAILABLE
    // =============================================================================================
    // [PC bring-up] THE ENVIRONMENT-MAP RENDER TARGET -- a 128x128 CUBE colour target with its own
    // multisampled depth, pool slot 3.
    //
    // Same lazy shape, and the same DELETE-WHEN, as EnsureShadowMapTarget / EnsurePostFxSceneTargets
    // above: BrnRendererMemory::Construct @0x823FCA38 builds the whole pool on the console and is
    // gated out here, and it would run before the D3D9 device exists anyway.
    //
    // ⚠ RULE 9 OF THIS WAVE, ENFORCED RATHER THAN LOGGED: "the frame looks identical" is not a gate
    // (verify F3, envface: the E_TYPE_CUBE test below is renderengine::Texture::GetType, which on this
    // backend reports the CREATE-TIME dimension the leaf recorded in the raster header -- a metadata
    // test, not an IDirect3DBaseTexture9::GetType() query. The object's real D3DRTYPE is printed by
    // the side that created it: PostFxRenderTargetPCLeaf.cpp's `[envmap-rt] cube ...` line. Read
    // BOTH lines off the boot log; this one alone proves extent + object + requested dimension.)
    // for an off-screen target. This function REFUSES -- once, loudly, latched so it never retries --
    // when the leaf hands back anything other than a non-degenerate CUBE colour texture, because a
    // 0x0 or 2D "cube" would render six faces into nothing and error nowhere. That refusal keeps
    // sampler 13 unbound and the six-face loop off, which is precisely the build this wave started
    // from: a regression to the previous behaviour rather than to a wrong picture.
    //
    // The latch is the created TARGET (a pointer), never a `static bool tried`: a one-shot set during
    // the loading screen -- before renderengine::gDevice exists -- would burn the single attempt and
    // the cube would never be built. That is this project's most repeated bring-up bug and both
    // neighbours above carry the same note. `gbEnvMapTargetRefused` is a SEPARATE latch and is only
    // ever set after a real creation attempt has been made and judged.
    // =============================================================================================
    CgsRenderTarget* gpEnvMapTarget       = nullptr;
    bool             gbEnvMapTargetRefused = false;

    bool EnsureEnvMapTarget(BrnRendererMemory& lrRendererMemory)
    {
        if (gpEnvMapTarget != nullptr)
            return true;
        if (gbEnvMapTargetRefused)
            return false;
        if (renderengine::gDevice == 0)
            return false;              // no device yet -- retry next frame
        if (gpShadowMapTarget == nullptr)
            return false;              // the slot-nulling pass has not run yet (see the note above it)
        if (!EnsureWorldDispatchAllocator())
            return false;

        lrRendererMemory.PCBringUpCreateEnvMapBuffer(&sWorldDispatchAllocator);

        CgsRenderTarget* const lpTarget = lrRendererMemory.GetEnvMapBuffer();
        rw::graphics::postfx::RenderTarget* const lpRenderTarget =
            (lpTarget != nullptr) ? lpTarget->GetRenderTarget() : nullptr;
        renderengine::Texture* const lpColourTexture =
            (lpRenderTarget != nullptr) ? lpRenderTarget->GetTexture(0) : nullptr;

        const u32 luWidth  = (lpTarget != nullptr) ? lpTarget->GetWidth()  : 0u;
        const u32 luHeight = (lpTarget != nullptr) ? lpTarget->GetHeight() : 0u;
        const s32 lnMultisampleRequested =
            (lpTarget != nullptr) ? lpTarget->GetMultisampleFormat() : 0;
        const s32 lnTextureType = (lpColourTexture != nullptr)
            ? static_cast<s32>(renderengine::Texture::GetType(lpColourTexture))
            : -1;
        // The D3D object itself, reported as a pointer rather than as a D3DRTYPE. Reading
        // IDirect3DBaseTexture9::GetType() here would need <d3d9.h> in the renderer TU -- texture.h
        // deliberately keeps the type a forward declaration -- and the answer belongs to the side
        // that CREATED the object: the cube create path logs its own D3DRTYPE at creation (cubeleaf).
        // What this line owes, and pays, is the OTHER half of rule 9: a non-degenerate EXTENT and a
        // real object, or a refusal.
        void* const lpD3DObject =
            (lpColourTexture != nullptr) ? static_cast<void*>(lpColourTexture->mpD3DTexture) : 0;

        const bool lbUsable = (lpTarget != nullptr) && (lpRenderTarget != nullptr)
                           && (lpColourTexture != nullptr)
                           && (lpD3DObject != 0)
                           && (lnTextureType == static_cast<s32>(renderengine::Texture::E_TYPE_CUBE))
                           && (luWidth != 0u) && (luHeight != 0u);

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[envmap] target " << static_cast<s32>(luWidth) << "x" << static_cast<s32>(luHeight)
                << " sections=" << static_cast<s32>((lpTarget != nullptr) ? lpTarget->GetNumSections() : 0u)
                << " msaaReqFormat=" << lnMultisampleRequested << "(console; 2==4x)"
                << " colourTexture=type" << lnTextureType << " (3==E_TYPE_CUBE, create-time metadata)"
                << " depthTex=" << static_cast<void*>(
                       (lpRenderTarget != nullptr) ? lpRenderTarget->GetDepthStencilTexture() : 0)
                << " d3dObject=" << lpD3DObject
                << " -> "
                << (lbUsable ? "USABLE" : "REFUSED -- the env-map pass and sampler 13 stay off")
                << "\n";
        }

        if (!lbUsable)
        {
            gbEnvMapTargetRefused = true;
            return false;
        }

        gpEnvMapTarget = lpTarget;
        // ARTIST 8240BE80..9C registers EnvMap slot3's colour texture0.
        // PC allocation occurs here on the D3D owner after the device exists.
        CgsGraphics::gTextureScopeTable.SetTexture(
            CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP,
            lpTarget->GetRenderTarget()->GetTexture(0));

        return true;
    }

    // =============================================================================================
    // [PC bring-up] THE SAMPLER-13 TEXTURE STATE -- BrnRendererModule::Construct's second
    // TextureState, deferred exactly like every other Construct step in this file.
    //
    // WHAT THE CONSOLE BUILDS, decoded off Construct @0x8240A778 (pseudocode 556-574) rather than
    // guessed. The three triples the WAVE_NOTE quotes as byte indices resolve, against the committed
    // member ORDER in BrnRendererModule.h, to exactly this object: taking `mpTextureState` as the
    // block's origin, mTextureStateParams sits at +4, mTextureStateResource at +80,
    // mpEnvMapTextureState at +100, mEnvMapTextureStateParams at +104,
    // mEnvMapTextureStateResource at +180 -- and Hex-Rays' gap15DE[] indices are those numbers plus
    // 2, which is what makes [102]/[106]/[182] the env-map triple and [2]/[6]/[82] the shadow one.
    // The identification is CLOSED at both ends: the same arithmetic puts mpGlassFractureTextureState
    // at [206] (line 597 zeroes it, and the ctor in BrnRendererModule.h zeroes it too) and
    // mpShadowMapTextureState[0]/[1] at [326]/[330] (lines 554/555) -- and Render's own binds then
    // read `*(this + 5924)` for s15 and `*(this + 5700)` for s13, whose difference is 224, which is
    // exactly [326] - [102].
    //
    //   mEnvMapTextureStateParams.muAddressU = muAddressV = muAddressW = 2   (lines 559-561)
    //   mEnvMapTextureStateParams.muMagFilter = muMinFilter = muMipFilter = 1 (lines 556-558)
    //   mEnvMapTextureStateParams.mpTexture   = GetEnvMapBuffer()->GetTexture(0)  (line 562, and
    //       `*&v1->gap15DE[178]` == params + 72 == renderengine::TextureState::Parameters::mpTexture)
    //   mpEnvMapTextureState = TextureState::Initialize(&mEnvMapTextureStateResource,
    //                                                   &mEnvMapTextureStateParams)  (line 574)
    // The 2s and 1s are not decoded here from first principles: the tree already decodes this exact
    // vocabulary twice, against the SAME field order, and both agree -- CLAMP is 2 and LINEAR is 1
    // (XenonD3D9Shims.cpp ApplyVolumeSamplerState, built from rw::graphics::postfx::Tint::Initialize
    // @0x82403B48's addressU=V=W=2 / mag=min=1; and PostFxDepthSampler_ApplyState, built from
    // RenderTarget::CreateStates @0x82403A18's mag=min=0 == POINT). So the env map asks for a
    // trilinear, clamped cube sampler -- which is what a 128x128 reflection needs and what the 22
    // shipped vehicle pixel shaders sample at s13.
    //
    // TWO DISCLOSED PC DEVIATIONS, both forced and neither hidden:
    //  1. THE PARAMS AND THE RESOURCE BLOCK LIVE ON THE STACK, not in mEnvMapTextureStateParams /
    //     mEnvMapTextureStateResource. Those two members exist in BrnRendererModule.h but their types
    //     are still the file's EMPTY placeholder structs (`struct TextureStateParameters {};` /
    //     `struct Resource {};`, BrnRendererModule.h:132-138), and retyping them to
    //     renderengine::TextureState::Parameters / rw::Resource means including renderstates.h from
    //     BrnRendererModule.h -- a header pulled into the game module and the immediate-mode layer.
    //     That cascade is its own change. Nothing is lost: on PC TextureState::Initialize
    //     (pc/gcm/renderengine/texturestate.cpp) IGNORES lpResourceMemory entirely (it `new`s the
    //     object and memcpy's the leading sampler words out of the params), so the two members are
    //     dead storage on this backend either way. The OUTPUT -- mpEnvMapTextureState -- is the
    //     tree's own member, so there is no parallel global and no split brain.
    //  2. THE SAMPLER WORDS DO NOT REACH D3D9 THROUGH THIS OBJECT. shadow::Device::SetState(const
    //     TextureState*, u32) applies them through SetSamplerStateLowLevel, which is the documented
    //     no-op on this backend (XenonD3D9Shims.cpp, the banner over ApplyVolumeSamplerState). The
    //     tree's answer for the two earlier cases was to key the state off WHAT WAS BOUND inside
    //     D3DDevice_SetTexture -- a volume texture gets LINEAR/CLAMP, a raw-depth texture gets
    //     POINT/CLAMP -- and a CUBE texture is the third case of the same shape. That arm is a
    //     CROSS-GROUP REQUEST to cubeleaf (it also fixes the 234 shipped world cube rasters), and it
    //     is why this function does NOT declare a new leaf entry point: the seam already exists.
    //     Until that arm lands, unit 13 keeps whatever filter it last held.
    //
    // Built through the REAL TextureState so that the whole-unit shadow key
    // (shadow::Device::mapTextureState[13]) is what the console's own bind writes -- Render
    // @0x8240CD?? binds it with `sub_8227D158(*(this + 5700), 13)`, the TextureState overload, not
    // the bare-resource one.
    // =============================================================================================
    bool EnsureEnvMapTextureState(renderengine::TextureState** lppEnvMapTextureState,
                                 CgsRenderTarget* lpEnvMapTarget)
    {
        if (lppEnvMapTextureState == 0)
            return false;
        if (*lppEnvMapTextureState != 0)
            return true;
        if (lpEnvMapTarget == 0 || lpEnvMapTarget->GetRenderTarget() == 0)
            return false;

        renderengine::Texture* const lpColourTexture =
            lpEnvMapTarget->GetRenderTarget()->GetTexture(0);
        if (lpColourTexture == 0)
            return false;

        renderengine::TextureState::Parameters lParameters;
        std::memset(&lParameters, 0, sizeof(lParameters));
        lParameters.muAddressU  = 2;   // Construct line 559 -- CLAMP
        lParameters.muAddressV  = 2;   // Construct line 560
        lParameters.muAddressW  = 2;   // Construct line 561
        lParameters.muMagFilter = 1;   // Construct line 556 -- LINEAR
        lParameters.muMinFilter = 1;   // Construct line 557
        lParameters.muMipFilter = 1;   // Construct line 558
        lParameters.mpTexture   = lpColourTexture;   // Construct line 562

        // lpResourceMemory: the console passes &mEnvMapTextureStateResource, carved from the
        // graphics allocator; the PC Initialize ignores the argument (see deviation 1 above).
        *lppEnvMapTextureState = renderengine::TextureState::Initialize(0, &lParameters);
        CgsGraphics::gTextureScopeTable.SetTextureState(
            CgsGraphics::E_TEXTURE_PURPOSE_ENVIRONMENT_MAP, *lppEnvMapTextureState);
        return *lppEnvMapTextureState != 0;
    }

    // =============================================================================================
    // [FLAG PC bring-up TEST HOOK -- OFF BY DEFAULT]  BRN_ENVMAP_DEBUG=1
    //
    // WHAT IT ANSWERS. D3D9's D3DCUBEMAP_FACE_POSITIVE_X..NEGATIVE_Z are the same 0..5 order as the
    // console's KAV_ENV_MAP_LOOK_DIRECTIONS (BrnEnvironmentMap.cpp:16-34), so face INDEX needs no
    // translation -- but face ORIENTATION does not follow from that. The Xenos resolves a face with
    // its own convention and D3D9 has its own; whether the resolved faces need a flip or a mirror to
    // sample right is an open cubeleaf question (WAVE_NOTE seam S4) and it is NOT guessable from
    // this side. This makes it readable off a single screenshot: with the knob on, each face is
    // cleared to a distinct colour and NOTHING is drawn into it -- no world, no sky -- so a
    // reflective car paints the cube's raw orientation onto itself. Red on the car's right means
    // +X samples right; red on its left means the faces need mirroring; red where GREEN should be
    // means the index order is wrong rather than the orientation.
    //
    // The six colours are the standard cube-face debug set, one saturated channel pair per axis,
    // and they are DELIBERATELY not derived from anything -- they are a test pattern, not data:
    //   0 +X red      1 -X cyan     2 +Y green    3 -Y magenta   4 +Z blue     5 -Z yellow
    // Read ONCE: an environment variable cannot change under a running process and this is on the
    // per-frame render path (same shape as BRN_POSTFX_MASK_TEST above).
    //
    // DELETE-WHEN the face orientation is settled and boot-verified.
    // =============================================================================================
    const f32 KAF_ENV_MAP_DEBUG_FACE_COLOURS[6][3] =
    {
        { 1.0f, 0.0f, 0.0f },   // 0  +X  red
        { 0.0f, 1.0f, 1.0f },   // 1  -X  cyan
        { 0.0f, 1.0f, 0.0f },   // 2  +Y  green
        { 1.0f, 0.0f, 1.0f },   // 3  -Y  magenta
        { 0.0f, 0.0f, 1.0f },   // 4  +Z  blue
        { 1.0f, 1.0f, 0.0f },   // 5  -Z  yellow
    };

    bool EnvMapDebugFaceColours()
    {
        static int siDebug = -1;      // -1 = not looked at yet, 0 = off, 1 = on
        if (siDebug < 0)
        {
            char lacValue[16] = { 0 };
            siDebug = (GetEnvironmentVariableA("BRN_ENVMAP_DEBUG", lacValue, sizeof(lacValue)) > 0
                       && lacValue[0] != '0') ? 1 : 0;
            if (siDebug != 0)
            {
                CgsDev::Log::WriteToLog(
                    "[envmap] BRN_ENVMAP_DEBUG=1 -- every cube face is cleared to its own colour and"
                    " NO world geometry or sky is drawn into it (0 +X red, 1 -X cyan, 2 +Y green,"
                    " 3 -Y magenta, 4 +Z blue, 5 -Z yellow)\n");
            }
        }
        return siDebug != 0;
    }
#endif  // BRN_ENVMAP_PASS_AVAILABLE
}

void BrnRendererModule::Construct()
{
    mIm2dRenderBuffer.Construct();
    mIm2dDebugRenderBuffer.Construct();
    mIm2dAssertRenderBufferPC.Construct();
    mIm3dRenderBuffer.Construct();
    mIm3dRenderBufferUntex.Construct();
    mIm3dDebugRenderBuffer.Construct();
    mIm3dBufferRacePosition.Construct();
    mIm3dBufferMenusAndHud.Construct();
    CgsGraphics::PrepareIm3dStateLibraryPC();
    // Double-buffered per-frame shader constants (maShaderConstantsFrames[2]).
    // ⭐ THE REUSABLE LOADING-SCREEN ALLOCATOR, 2026-08-17 (boot audit F-P2-4/F-P6-12). The
    // console owns this as an embedded member at renderer+0xC8FC and lends its address to the
    // game module through RendererIO::OutputBuffer every GamePrepare pass; the loading flow
    // FreeAll's it once per world drive and hands it to the world virtual. That whole
    // mechanism -- renderer owns, renderer publishes, game latches, flow recycles -- is now
    // real, which retires the invented file-static the world drive was using instead.
    //
    // [FLAG] the SIZE is still ours. The console's backing carve does not appear in Construct
    // @0x8240A778 or Prepare @0x82409C20 (no LinearMalloc::Construct call in either), so it
    // comes from the allocator layer this build does not have. 512 KiB is what the retired
    // static used, kept so the swap changes ownership and not behaviour; re-source it with the
    // renderer's real memory carve when the allocator gate lands.
    static const size_t KN_LOADING_SCREEN_ALLOCATOR_SIZE = 512u * 1024u;
    static u8 saLoadingScreenAllocatorBacking[KN_LOADING_SCREEN_ALLOCATOR_SIZE];
    mReusableLoadingScreenAllocator.Construct();
    mReusableLoadingScreenAllocator.Create(saLoadingScreenAllocatorBacking,
                                           KN_LOADING_SCREEN_ALLOCATOR_SIZE);

    maShaderConstantsFrames[0].Construct();
    maShaderConstantsFrames[1].Construct();
    // ARTIST 8240A7E0..7FC: external1 starts writable, internal0 readable.
    mu8ShaderConstantsFrameExternal = 1;
    mu8ShaderConstantsFrameInternal = 0;
    maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].LockForWriting();
    maShaderConstantsFrames[mu8ShaderConstantsFrameInternal].UnlockForWriting();
    maShaderConstantsFrameValidPC[0] = false;
    maShaderConstantsFrameValidPC[1] = false;

    muCommandGenerationPC = 0;
    muCompletedCommandGenerationPC = 0;
    muPublishedCommandGenerationPC = 0;
    // ARTIST 8240ACD0..ACDC: two empty blobby banks, internal0/external1.
    mBlobbyShadowManager.maBuffers[0].miNumShadows = 0;
    mBlobbyShadowManager.maBuffers[1].miNumShadows = 0;
    mBlobbyShadowManager.mu8Internal = 0;
    mBlobbyShadowManager.mu8External = 1;


    // ---- The display class (X360 Construct @0x8240A778, right after the stage seeds) ----
    //     renderengine::Device::Parameters::Initialize(&params, 4);   // 1280x720@60
    //     mu16FrontBufferHeight = LOWORD(params.height);              // +0x234
    //     mbIsHD                = mu16FrontBufferHeight >= 0x2D0;     // +0x236
    //     XGetVideoMode(&mode); *lpbIsHiDefOut = mode.fIsHiDef != 0;  // -> GuiModule::Construct
    // The console asks mode 4 for its OWN class and the TV for the GUI's; on this host the
    // swap chain IS the display (renderengine::gDisplayWidth/Height -- the values device.cpp
    // hands D3DPRESENT_PARAMETERS, config.ini-overridable), so both questions read the same
    // extent. Until 2026-09-07 neither member left the ctor's `false`, which is one half of
    // BurnoutDecomp/b5-decomp#11 (the GUI cache's HD byte was never written -- see
    // BrnGameModule::Construct's GuiModule::Construct call for the other half).
    {
        const s32 liHeight = renderengine::gDisplayHeight;
        mu16FrontBufferHeight = static_cast<u16>((liHeight > 0 && liHeight < 0x10000) ? liHeight : 0);
        mbIsHD                = mu16FrontBufferHeight >= 0x2D0u;
    }

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
        const bool lbIm2dReady = mIm2dRenderBuffer.Prepare(KU_PC_IM2D_COMMAND_BYTES,
            KU_PC_IM2D_VERTEX_BYTES, &sWorldDispatchAllocator, false);
        const bool lbIm2dDebugReady = mIm2dDebugRenderBuffer.Prepare(KU_PC_IM2D_DEBUG_COMMAND_BYTES,
            KU_PC_IM2D_DEBUG_VERTEX_BYTES, &sWorldDispatchAllocator, true);
        const bool lbAssertReady = mIm2dAssertRenderBufferPC.Prepare(KU_PC_IM2D_DEBUG_COMMAND_BYTES,
            KU_PC_IM2D_DEBUG_VERTEX_BYTES, &sWorldDispatchAllocator, true);
        CGS_ASSERT(lbIm2dReady, "mIm2dRenderBuffer.Prepare");
        CGS_ASSERT(lbIm2dDebugReady, "mIm2dDebugRenderBuffer.Prepare");
        CGS_ASSERT(lbAssertReady, "mIm2dAssertRenderBufferPC.Prepare");
        // ARTIST Prepare82409C20: textured3D source vertex banks are32-byte
        // records, including RacePosition's128KiB commands/vertices. MenusHud
        // uses512 command bytes and static vertex runs (4-byte vertex bank).
        const bool lbIm3dReady = mIm3dRenderBuffer.Prepare(KU_PC_IM3D_COMMAND_BYTES, KU_PC_IM3D_VERTEX_BYTES, &sWorldDispatchAllocator, false);
        const bool lbIm3dUntexReady = mIm3dRenderBufferUntex.Prepare(KU_PC_IM3D_UNTEX_COMMAND_BYTES, KU_PC_IM3D_UNTEX_VERTEX_BYTES, &sWorldDispatchAllocator, false);
        const bool lbIm3dDebugReady = mIm3dDebugRenderBuffer.Prepare(KU_PC_IM3D_DEBUG_COMMAND_BYTES, KU_PC_IM3D_DEBUG_VERTEX_BYTES, &sWorldDispatchAllocator, true);
        const bool lbRacePositionReady = mIm3dBufferRacePosition.Prepare(KU_PC_IM3D_RACE_COMMAND_BYTES, KU_PC_IM3D_RACE_VERTEX_BYTES, &sWorldDispatchAllocator, false);
        const bool lbMenusHudReady = mIm3dBufferMenusAndHud.Prepare(KU_PC_IM3D_MENUS_COMMAND_BYTES, KU_PC_IM3D_MENUS_VERTEX_BYTES, &sWorldDispatchAllocator, false);
        CGS_ASSERT(lbIm3dReady, "mIm3dRenderBuffer.Prepare");
        CGS_ASSERT(lbIm3dUntexReady, "mIm3dRenderBufferUntex.Prepare");
        CGS_ASSERT(lbIm3dDebugReady, "mIm3dDebugRenderBuffer.Prepare");
        CGS_ASSERT(lbRacePositionReady, "mIm3dBufferRacePosition.Prepare");
        CGS_ASSERT(lbMenusHudReady, "mIm3dBufferMenusAndHud.Prepare");
        mSingleBufferedDispatchFrame.Construct(KU_NUM_DISPATCH_LISTS,
                                               KU_PC_DISPATCH_BIN_BYTES,
                                               &sWorldDispatchAllocator);

        mDoubleBufferedDispatchFrame.SetNumDispatchFrames(2);
        mDoubleBufferedDispatchFrame.Construct(KU_NUM_DISPATCH_LISTS,
                                               KU_PC_GDL_DISPATCH_BIN_BYTES,
                                               &sWorldDispatchAllocator);
        if (!DispatchStorageAvailablePC(&mSingleBufferedDispatchFrame)
            || !DispatchStorageAvailablePC(&mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite())
            || !DispatchStorageAvailablePC(&mDoubleBufferedDispatchFrame.GetDispatchFrameForRead()))
        {
            mbDispatchStorageFailedPC = true;
            CgsDev::Log::WriteToLog("[renderer] dispatch command allocation failed; shutting down.\n");
            CgsSystem::HardwareInit::RequestShutdown();
            return;
        }

        CgsGraphics::SetupBuiltinInterpreters(maInterpretFunctions);
        mpInterpreter = new CgsGraphics::DispatchPacketInterpreter(maInterpretFunctions, 4);
        mpInterpreter->SetSingleBufferedDispatchFrame(&mSingleBufferedDispatchFrame);
        mpInterpreter->SetTime(0.0f);
        if (MeshPreparationEnabledPC())
        {
            mSecondMeshFramePC.Construct(KU_NUM_DISPATCH_LISTS,
                KU_PC_DISPATCH_BIN_BYTES, &sWorldDispatchAllocator);
            if (!DispatchStorageAvailablePC(&mSecondMeshFramePC))
            {
                mbDispatchStorageFailedPC = true;
                CgsDev::Log::WriteToLog("[renderer] second mesh command allocation failed; shutting down.\n");
                CgsSystem::HardwareInit::RequestShutdown();
                return;
            }
            maPreparedMeshFramesPC[0].mpFrame = &mSingleBufferedDispatchFrame;
            maPreparedMeshFramesPC[1].mpFrame = &mSecondMeshFramePC;
            mpMeshProducerInterpreterPC = new CgsGraphics::DispatchPacketInterpreter(maInterpretFunctions, 4);
            CgsDev::Log::WriteToLog("[renderer] update-side mesh preparation enabled (two native bins).\n");
        }
    }

    // ARTIST @8240BF30..BF54 initializes the inlined shadow manager before the loading screen.
    mShadowMapRenderManager.Construct(2);

    // The loading-screen renderer (creates its textures + scratch buffer, picks language).
    mLoadingScreenRenderer.Construct();

    // X360 Construct @0x8240BF74-0x8240BF88: `ld r11, qword_82FAFF20` (CgsResource::NULLResourceHandle)
    // / `stdx r11, r31, 0xC920` -- the calibration ramp texture handle Render latches (see the
    // composite call below) starts null. Follows LoadingScreenRenderer::Construct @0x8240BF70 as
    // on the console.
    mCalibrationTextureHandle = CgsResource::NULLResourceHandle;

    // [FLAG PC diagnostic] BRN_RENDER_POSTFX=0 clears mbRenderPostFX -- the console's own "Render
    // PostFX" debug-menu toggle (BrnGraphics::DebugComponent::OnActivate @0x823F7B98 registers
    // this+0xC41C), which the PC build has no debug menu to reach. With it off Render skips the
    // effects apply, the tint blend and the composite and presents through the bring-up blit
    // (both gates test the same byte, as on the console). Exists for the post-fx fps A/B; the
    // default is the header's `true`.
    {
        char lacRenderPostFx[8] = { 0 };
        if (GetEnvironmentVariableA("BRN_RENDER_POSTFX", lacRenderPostFx, sizeof(lacRenderPostFx)) > 0
            && lacRenderPostFx[0] == '0')
        {
            mbRenderPostFX = false;
            CgsDev::Log::WriteToLog("[postfx] BRN_RENDER_POSTFX=0 -> mbRenderPostFX cleared "
                                    "(effects apply, tint blend and composite skipped)\n");
        }
    }
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

// FLAG PC-platform leaf: retain the actual completed command metadata with
// the bank it describes. Control IO continues to rotate every presentation
// for its one-shot loading/calibration/stall protocol.
void BrnRendererModule::CompleteCommandFramePC(const BrnGame::DispatchThreadInputBuffer* lpInput,
                                               ECommandProducerPC leProducer)
{
    lpInput->LockForRead();
    CommandFrameInputPC& lrInput = maCommandInputsPC[mu8ShaderConstantsFrameExternal];
    // RenderGUI has no world/particle producer and leaves those IO bytes
    // untouched. Ownership follows the completed entry, never payload contents.
    lrInput.mbParticleRecordProduced = leProducer == E_COMMAND_PRODUCER_WORLD_AND_EFFECTS;
    if (lrInput.mbParticleRecordProduced)
        lrInput.mParticleRenderData = *lpInput->GetParticleRenderData();
    for (u32 luFace = 0; luFace < 6; ++luFace)
        lrInput.mabEnvMapFaceRender[luFace] = leProducer == E_COMMAND_PRODUCER_WORLD_AND_EFFECTS
            && lpInput->GetEnvMapFaceRender(luFace);
    lpInput->UnlockForRead();
    muCompletedCommandGenerationPC = muCommandGenerationPC;
}

// ARTIST 823FC160. A native presentation opens a write generation, but
// only a completed command producer makes that generation publishable.
void BrnRendererModule::StartOfFrame()
{
    // FLAG PC-platform leaf: a resource stall can cancel an unpublished write
    // generation. Discard its mutable effects/shader/shadow records before
    // another producer reuses the same bank; the immutable read bank stays live.
    if (muCompletedCommandGenerationPC != muPublishedCommandGenerationPC)
    {
        maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].Construct();
        maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].LockForWriting();
        if (sbEffectsArbitratorConstructed)
        {
            const u32 lauSlots[] = {
                BrnGraphics::EffectLayerDefinition<BrnGraphics::E_EFLAYER_BASE>::KU_NUM_SLOTS,
                BrnGraphics::EffectLayerDefinition<BrnGraphics::E_EFLAYER_WORLD>::KU_NUM_SLOTS,
                BrnGraphics::EffectLayerDefinition<BrnGraphics::E_EFLAYER_FXEVENTS>::KU_NUM_SLOTS};
            for (u8 luLayer = 0; luLayer < BrnGraphics::E_EFLAYER_CNT; ++luLayer)
                for (u8 luSlot = 0; luSlot < lauSlots[luLayer]; ++luSlot)
                    mEffectsArbitrator.GetExternalEffectsFrame(luLayer, luSlot)->Construct();
        }
        mBlobbyShadowManager.maBuffers[mBlobbyShadowManager.mu8External].miNumShadows = 0;
    }
    ++muCommandGenerationPC;
    muCompletedCommandGenerationPC = muPublishedCommandGenerationPC;
    maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameExternal] = false;

    EnsureEffectsArbitratorBringUp(mEffectsArbitrator);
    if (mpInterpreter != nullptr)
        mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite().Reset();

    // Original seven write-buffer rewinds, in their original order.
    if (mIm2dRenderBuffer.IsPreparedPC()) mIm2dRenderBuffer.Clear();
    if (mIm3dRenderBuffer.IsPreparedPC()) mIm3dRenderBuffer.Clear();
    if (mIm3dRenderBufferUntex.IsPreparedPC()) mIm3dRenderBufferUntex.Clear();
    if (mIm3dDebugRenderBuffer.IsPreparedPC()) mIm3dDebugRenderBuffer.Clear();
    if (mIm2dDebugRenderBuffer.IsPreparedPC()) mIm2dDebugRenderBuffer.Clear();
    if (mIm3dBufferRacePosition.IsPreparedPC()) mIm3dBufferRacePosition.Clear();
    if (mIm3dBufferMenusAndHud.IsPreparedPC()) mIm3dBufferMenusAndHud.Clear();

    if (mpInterpreter != nullptr)
        CgsGraphics::mShaderConstantTable.BeginFrame(
            &mDoubleBufferedDispatchFrame.GetDispatchBinForWrite());
    // ARTIST 823FC2D0..2F0 clears the cache word of all seven purposes.
    for (auto& lrEntry : CgsGraphics::gTextureScopeTable.maEntries)
        lrEntry.mClearState = 0;
    mCoronaManager.Clear();
    if (mpInterpreter != nullptr)
        BeginMeshFramePC();
}

// ARTIST 823FC678. All original banks publish together; the native generation
// check preserves their last immutable frame across extra host presentations.
void BrnRendererModule::SwapBuffers()
{
    // FLAG PC-platform leaf: no producer ran for this write generation, or
    // this completed generation was already published. Rotating here would
    // replace the last immutable command frame with a cleared producer bank.
    if (muCompletedCommandGenerationPC != muCommandGenerationPC
        || muPublishedCommandGenerationPC == muCompletedCommandGenerationPC)
        return;

    if (mpInterpreter != nullptr)
    {
        PublishMeshFramePC();
        mDoubleBufferedDispatchFrame.Swap();
    }
    // The two ICF-folded EndFrame calls at 823FC6AC/B0 have empty bodies.
    if (sbEffectsArbitratorConstructed)
        mEffectsArbitrator.EndOfFrame();

    mu8ShaderConstantsFrameInternal = mu8ShaderConstantsFrameExternal;
    mu8ShaderConstantsFrameExternal = static_cast<u8>(1u - mu8ShaderConstantsFrameInternal);
    maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].Construct();
    maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].LockForWriting();
    maShaderConstantsFrames[mu8ShaderConstantsFrameInternal].UnlockForWriting();
    maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameExternal] = false;

    if (mIm2dRenderBuffer.IsPreparedPC()) mIm2dRenderBuffer.Swap();
    if (mIm3dRenderBuffer.IsPreparedPC()) mIm3dRenderBuffer.Swap();
    if (mIm3dRenderBufferUntex.IsPreparedPC()) mIm3dRenderBufferUntex.Swap();
    if (mIm3dDebugRenderBuffer.IsPreparedPC()) mIm3dDebugRenderBuffer.Swap();
    if (mIm2dDebugRenderBuffer.IsPreparedPC()) mIm2dDebugRenderBuffer.Swap();
    if (mIm3dBufferRacePosition.IsPreparedPC()) mIm3dBufferRacePosition.Swap();
    if (mIm3dBufferMenusAndHud.IsPreparedPC()) mIm3dBufferMenusAndHud.Swap();

    // Original inlined BrnBlobbyShadowManager::Swap, 823FC748..770.
    mBlobbyShadowManager.mu8Internal = mBlobbyShadowManager.mu8External;
    mBlobbyShadowManager.mu8External = static_cast<u8>(1u - mBlobbyShadowManager.mu8Internal);
    mBlobbyShadowManager.maBuffers[mBlobbyShadowManager.mu8External].miNumShadows = 0;
    mCoronaManager.Swap();
    muPublishedCommandGenerationPC = muCompletedCommandGenerationPC;
}

// ARTIST 823FFE28. This is the real resource-stall synchronisation path,
// followed by the screenshot-request handoff; it is not a debug-camera freeze.
void BrnRendererModule::EndOfFrame(bool lbStalled)
{
    if (lbStalled)
    {
        if (meFrameStallStage == E_FRAMESTALL_NOT_STALLED)
        {
            meFrameStallStage = E_FRAMESTALL_SYNCING_BUFFERS;
            miFrameStallCountdown = 2;
        }
    }
    else if (meFrameStallStage != E_FRAMESTALL_NOT_STALLED)
    {
        meFrameStallStage = E_FRAMESTALL_NOT_STALLED;
        miFrameStallCountdown = 0;
    }

    if (meFrameStallStage == E_FRAMESTALL_NOT_STALLED)
        SwapBuffers();
    else if (meFrameStallStage == E_FRAMESTALL_SYNCING_BUFFERS)
    {
        if (--miFrameStallCountdown == 0)
        {
            meFrameStallStage = E_FRAMESTALL_STALLED;
            SwapBuffers();
        }
    }
    else if (static_cast<u32>(meFrameStallStage) < 3u)
    {
        meFrameStallStage = E_FRAMESTALL_STALLED;
        SwapBuffers();
    }

    if (mbUpdateThreadTakeScreenshot)
    {
        mbUpdateThreadTakeScreenshot = false;
        mbDispatchThreadTakeScreenshot = true;
    }
}

namespace
{
    // ARTIST 82FAEDD8/82FAEDD4/82FAEE00. The render owner publishes these before
    // submission and does not reclaim the bin until every conversion has joined.
    CgsGraphics::DispatchCommand* spObjectToMeshSharedMemory;
    u32 suObjectToMeshSharedBlockMax;
    alignas(128) u32 suObjectToMeshNextBlock;
}

// ARTIST 823F5670: copy the job record, clear the job, then set code/data/name.
static void FillInObjectToMeshJobData(EA::Jobs::Job* lpOutJob,
                                     ObjectToMeshJobInfo* lpOutJobData,
                                     const ObjectToMeshJobInfo* lpInput)
{
    CGS_ASSERT(lpOutJobData != nullptr, "lpOutJobData");
    CGS_ASSERT(lpOutJob != nullptr, "lpOutJob");
    *lpOutJobData = *lpInput;
    lpOutJob->Clear();
    lpOutJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                      reinterpret_cast<const void*>(&ObjectToMeshEntry), 0);
    lpOutJob->SetData(lpOutJobData, sizeof(*lpOutJobData));
    lpOutJob->SetName("ObjectToMesh");
}

// ARTIST 823F5748. World input list11 is partitioned on complete 128-object
// constant-refresh groups. Starting a worker at an arbitrary key loses inherited
// constants; the four original partitions preserve those producer boundaries.
void BrnRendererModule::CreateObjectToMeshJob(u32 luJobIndex,
        const CgsGraphics::DispatchObjectContext* lpContext,
        CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
        u32 luInputList, s32 liGroupIndex, u32 luGroupSize)
{
    CreateObjectToMeshJobPC(&mDoubleBufferedDispatchFrame.GetDispatchFrameForRead(),
        luJobIndex, lpContext, lpInterpreter, luInputList, liGroupIndex, luGroupSize);
}

// FLAG PC-platform leaf: explicit source bank permits update-side conversion
// without advancing the GDL read cursor while the renderer still owns it.
void BrnRendererModule::CreateObjectToMeshJobPC(CgsGraphics::DispatchFrame* lpInputFrame,
        u32 luJobIndex, const CgsGraphics::DispatchObjectContext* lpContext,
        CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
        u32 luInputList, s32 liGroupIndex, u32 luGroupSize)
{
    auto* lpInput = lpInputFrame->GetList(luInputList);
    maObjectToMeshJobContext[luJobIndex] = *lpContext;
    maObjectToMeshJobContext[luJobIndex].miListIdBase = liGroupIndex;
    CGS_ASSERT(liGroupIndex >= 0 && static_cast<u32>(liGroupIndex) < luGroupSize,
               "0 <= luGroupIndex && luGroupIndex < luGroupSize");
    const u32 luGroups = (lpInput->GetCount() + 127u) >> 7;
    ObjectToMeshJobInfo lInput{};
    lInput.muListID = luJobIndex;
    lInput.mpDispatchInterpreter = lpInterpreter;
    lInput.mpDispatchObjectContext = &maObjectToMeshJobContext[luJobIndex];
    lInput.mpDispatchListInput = lpInput;
    lInput.mpaDispatchListOutputArray = mapaObjectToMeshJobOutputDispatchLists[luJobIndex];
    lInput.muDispatchListOutputCount = 25u;
    lInput.mpDispatchBinMasterAddress = lpInterpreter->GetSingleBufferedDispatchFrame()->GetBin().GetBase();
    lInput.muSharedMemoryStartAddress = reinterpret_cast<uintptr_t>(spObjectToMeshSharedMemory);
    lInput.mpSharedMemoryBlockNextFreeAtomic = &suObjectToMeshNextBlock;
    lInput.muSharedMemoryBlockMax = suObjectToMeshSharedBlockMax;
    lInput.miStartIndex = static_cast<s32>((luGroups * static_cast<u32>(liGroupIndex) / luGroupSize) << 7);
    lInput.miEndIndex = static_cast<s32>((luGroups * (static_cast<u32>(liGroupIndex) + 1u) / luGroupSize) << 7);
    FillInObjectToMeshJobData(&maObjectToMeshJob[luJobIndex], &maObjectToMeshJobData[luJobIndex], &lInput);
}

// ARTIST 823F5898. Both original switches82F2423C/D initialize to1. Keep the
// serial fallback and dependency-chain controls for native comparisons.
void BrnRendererModule::ConvertObjectsToMeshes(CgsGraphics::BufferedDispatchFrame* lpGdlFrames,
                                               CgsGraphics::DispatchFrame* lpMeshFrame,
                                               CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
                                               const CgsGraphics::DispatchObjectContext* lpContext)
{
    ConvertObjectsToMeshesPC(&lpGdlFrames->GetDispatchFrameForRead(),
        lpMeshFrame, lpInterpreter, lpContext);
}

// FLAG PC-platform leaf: both schedules execute the same original conversion.
void BrnRendererModule::ConvertObjectsToMeshesPC(CgsGraphics::DispatchFrame* lpInputFrame,
        CgsGraphics::DispatchFrame* lpMeshFrame,
        CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
        const CgsGraphics::DispatchObjectContext* lpContext)
{
    // Profile the owner's full conversion interval. Per-object timer writes
    // cannot safely accumulate into the same frame from multiple workers.
    renderengine::FrameProfile::DetailScope lConversionProfile(renderengine::FrameProfile::OBJECT_TO_MESH);
    // FLAG PC-platform leaf: BRN_MESH_JOBS=1 enables the original job path.
    // Matched native runs found no consistent FPS benefit and higher total CPU
    // time, so retain the serial default until further pipeline work improves it.
    static const bool sbJobsEnabled = [] {
        const char* lpcValue = std::getenv("BRN_MESH_JOBS");
        return lpcValue && lpcValue[0] && lpcValue[0] != '0';
    }();
    static const bool sbChainJobs = [] {
        const char* lpcValue = std::getenv("BRN_MESH_JOBS_CHAIN");
        return lpcValue && lpcValue[0] && lpcValue[0] != '0';
    }();
    if (sbJobsEnabled)
    {
        using namespace CgsGraphics;
        DispatchBin& lrBin = lpInterpreter->GetSingleBufferedDispatchFrame()->GetBin();
        for (u32 luJob = 0; luJob < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++luJob)
        {
            lrBin.Align(128u);
            mapaObjectToMeshJobOutputDispatchLists[luJob] = static_cast<DispatchList*>(
                lrBin.AllocateMemoryFast((25u * sizeof(DispatchList) + 15u) / 16u));
        }
        lrBin.Align(128u);
        const u32 luAvailable = lrBin.GetSizeQwords() - lrBin.GetUsedQwords();
        const u32 luSharedQwords = (luAvailable & ~7u) - 1u;
        spObjectToMeshSharedMemory = static_cast<DispatchCommand*>(lrBin.AllocateMemoryFast(luSharedQwords));
        EA::Jobs::AtomicStore(&suObjectToMeshNextBlock, 0);
        suObjectToMeshSharedBlockMax = luSharedQwords / DispatchBin::KU_BLOCK_SIZE_IN_QUAD_WORDS;
        for (u32 luGroup = 0; luGroup < 4u; ++luGroup)
            CreateObjectToMeshJobPC(lpInputFrame, luGroup, lpContext, lpInterpreter, 11u, luGroup, 4u);
        CreateObjectToMeshJobPC(lpInputFrame, 4u, lpContext, lpInterpreter, 12u, 0, 1u);
        for (u32 luList = 0; luList < 11u; ++luList)
            CreateObjectToMeshJobPC(lpInputFrame, luList + 5u, lpContext, lpInterpreter, luList, 0, 1u);

        if (sbChainJobs)
        {
            for (u32 luJob = 1; luJob < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++luJob)
                maObjectToMeshJob[luJob].DependsOn(maObjectToMeshJob[luJob - 1u], EA::Jobs::Event::EVENT_WHEN_JOB_END);
            CgsSystem::JobManager()->AddTree(&maObjectToMeshJob[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS - 1u]);
        }
        else
        {
            for (u32 luJob = 0; luJob < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++luJob)
                CgsSystem::JobManager()->AddJobs(&maObjectToMeshJob[luJob], 1);
        }
        // Only update-side preparation owns the window/message queue. The
        // normal render-thread join retains the original callback-free wait.
        renderengine::MeshJobOwnerWaitPC lOwnerWait;
        const bool lbOwnerWait = lpInterpreter == mpMeshProducerInterpreterPC;
        for (u32 luJob = 0; luJob < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++luJob)
            maObjectToMeshJob[luJob].WaitOn(
                lbOwnerWait ? &renderengine::MeshJobOwnerWaitPC::Poll : nullptr,
                lbOwnerWait ? &lOwnerWait : nullptr);

        u32 luBlocksUsed = suObjectToMeshNextBlock;
        if (luBlocksUsed > suObjectToMeshSharedBlockMax)
        {
            CGS_ASSERT(false, "Object-to-mesh conversion jobs failed due to insufficient memory.");
            luBlocksUsed = suObjectToMeshSharedBlockMax;
        }
        lrBin.SetBinCurrent(spObjectToMeshSharedMemory
            + static_cast<size_t>(luBlocksUsed) * DispatchBin::KU_BLOCK_SIZE_IN_QUAD_WORDS);
        for (u32 luJob = 0; luJob < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++luJob)
        {
            for (u32 luList = 0; luList < 25u; ++luList)
            {
                DispatchList& lrOutput = mapaObjectToMeshJobOutputDispatchLists[luJob][luList];
                lrOutput.ReconnectChainBlocks();
                lpMeshFrame->GetList(luList)->Append(&lrOutput);
            }
        }
        static const u32 KAU_BASE_LISTS[] = { 11u, 15u, 21u };
        for (u32 luBase : KAU_BASE_LISTS)
            for (u32 luGroup = 1; luGroup < 4u; ++luGroup)
                lpMeshFrame->GetList(luBase)->Append(lpMeshFrame->GetList(luBase + luGroup));
        return;
    }
    for (u32 luListId = 0; luListId < 13u; ++luListId)
    {
        // X360 per-pass prologue: mShaderConstantTable.ResetShadowingForDispatch()
        // + the 7x7 texture-scope scratch clear (unk_83011A90). Both shadow the
        // PRODUCER-side dirty tracking; the expansion context below is a fresh
        // copy each pass, so the bring-up defers them with the texture-scope
        // reconstruction. FLAG [deferred with CgsTextureScopeTable].

        CgsGraphics::DispatchObjectContext lContextCopy;
        std::memcpy(&lContextCopy, lpContext, sizeof(lContextCopy));

        lpInputFrame->GetList(luListId)->DispatchAllObjectToMesh(
            lpInterpreter, lpInterpreter->GetSingleBufferedDispatchFrame(),
            &lContextCopy, 0, -1);
    }
}

// ARTIST 823F5EA0 / DecFIGS _FillInJobData(Job*,SortInfo*,DispatchList*).
static void FillInSortJobData(EA::Jobs::Job* lpOutJob, SortInfo* lpOutJobData,
                             CgsGraphics::DispatchList* lpDispatchList)
{
    lpDispatchList->PrepareSortJobInfo(&lpOutJobData->mInputOutputInfo);
    CGS_ASSERT(lpOutJobData, "lpOutJobData");
    CGS_ASSERT(lpOutJob, "lpOutJob");
    const auto luAddress = reinterpret_cast<uintptr_t>(lpOutJobData->mInputOutputInfo.mpaFlatKeys);
    lpOutJobData->mu64KeyInAddress = luAddress;
    lpOutJobData->mu64KeyOutAddress = luAddress;
    lpOutJobData->mu16Count = static_cast<u16>(lpOutJobData->mInputOutputInfo.muKeyCount);
    lpOutJob->Clear();
    lpOutJob->mEntryPoint.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
        reinterpret_cast<const void*>(&RadixSortEntry), 0);
    lpOutJob->SetData(lpOutJobData, sizeof(SortInfo));
    lpOutJob->mEntryPoint.SetName("RadixSort");
}

namespace renderengine
{
    DispatchSortJobsPC::DispatchSortJobsPC()
        : maJobs{EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr),
                 EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr),
                 EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr),
                 EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr), EA::Jobs::Job(nullptr)}
        , mOwnerThread(EA::Thread::GetThreadId())
    {}

    DispatchSortJobsPC::~DispatchSortJobsPC() { WaitAll(); }

    // FLAG PC-platform leaf: extended scene settings can exceed the console's
    // 16-bit count. Preserve the prior PC full-list sort for those lists.
    static void WideSortEntryPC(EA::Jobs::Param, EA::Jobs::Param lData,
                                EA::Jobs::Param, EA::Jobs::Param)
    {
        const auto* lpData = static_cast<const SortInfo*>(lData.mpValue);
        u64* lpaKeys = lpData->mInputOutputInfo.mpaFlatKeys;
        const u32 luCount = lpData->mInputOutputInfo.muKeyCount;
        if (luCount > 1) std::sort(lpaKeys, lpaKeys + luCount);
    }

    void DispatchSortJobsPC::Begin(CgsGraphics::DispatchFrame* lpFrame,
                                  EA::Jobs::JobScheduler* lpScheduler, bool lbWide)
    {
        WaitAll();
        // Flattening allocates from one shared frame bin, so it stays on the
        // producer. Workers only sort disjoint, already-published key arrays.
        for (u32 luJob = 0; luJob < KU_COUNT; ++luJob)
        {
            FillInSortJobData(&maJobs[luJob], &maData[luJob], lpFrame->GetList(KAU_LISTS[luJob]));
            if (maData[luJob].mInputOutputInfo.muKeyCount > 0xffffu)
                maJobs[luJob].SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                    reinterpret_cast<const void*>(&WideSortEntryPC), 0);
        }
        muPending = (1u << KU_COUNT) - 1u;
        if (!lbWide)
        {
            for (u32 luJob = 1; luJob < KU_COUNT; ++luJob)
                maJobs[luJob].DependsOn(maJobs[luJob - 1], EA::Jobs::Event::EVENT_WHEN_JOB_END);
            lpScheduler->AddTree(&maJobs[KU_COUNT - 1]);
        }
        else
        {
            // FLAG PC-platform leaf: ARTIST's optional wide branch submits only
            // four of its five shadow lists. Submit all prepared lists here so
            // the third cascade cannot consume an unsorted/unsubmitted list4.
            for (u32 luJob = 0; luJob < KU_COUNT; ++luJob)
                lpScheduler->AddJobs(&maJobs[luJob], 1);
        }
    }

    void DispatchSortJobsPC::WaitIndex(u32 luIndex)
    {
        const u32 luBit = 1u << luIndex;
        if (!(muPending & luBit)) return;
        const bool lbOwner = EA::Thread::GetThreadId() == mOwnerThread;
        FrameProfile::Scope lWaitProfile(lbOwner ? FrameProfile::UPDATE_SORT_WAIT : FrameProfile::RENDER_SORT_WAIT);
        MeshJobOwnerWaitPC lOwnerWait;
        maJobs[luIndex].WaitOn(lbOwner ? &MeshJobOwnerWaitPC::Poll : nullptr,
                              lbOwner ? &lOwnerWait : nullptr);
        muPending &= ~luBit;
    }

    void DispatchSortJobsPC::WaitList(u32 luList)
    {
        if (!muPending) return;
        for (u32 luIndex = 0; luIndex < KU_COUNT; ++luIndex)
            if (KAU_LISTS[luIndex] == luList) { WaitIndex(luIndex); return; }
    }

    void DispatchSortJobsPC::WaitAll()
    {
        for (u32 luIndex = 0; muPending && luIndex < KU_COUNT; ++luIndex)
            WaitIndex(luIndex);
    }
}

// ARTIST 823F5F70 prepares sixteen jobs. Its default is the dependency chain;
// the optional wide branch submits independent jobs. Native frame banks retain
// their own descriptors until their consumers or a rebuild join them.
void BrnRendererModule::SortDispatchLists(CgsGraphics::DispatchFrame* lpMeshFrame)
{
    static const bool sbJobs = [] {
        const char* lpcValue = std::getenv("BRN_SORT_JOBS");
        return lpcValue && lpcValue[0] == '1';
    }();
    static const bool sbWide = [] {
        const char* lpcValue = std::getenv("BRN_SORT_JOBS_WIDE");
        return lpcValue && lpcValue[0] == '1';
    }();
    if (sbJobs)
    {
        renderengine::FrameProfile::Scope lSortProfile(renderengine::FrameProfile::DISPATCH_SORT);
        GetMeshSortJobsPC(lpMeshFrame).Begin(lpMeshFrame, CgsSystem::JobManager(),
            mbSortDisplayListsWideNotLong || sbWide);
        return;
    }
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
void BrnRendererModule::InitializeDispatchContextPC(CgsGraphics::DispatchObjectContext* lpContext) const
{
    std::memset(lpContext, 0, sizeof(*lpContext));
    // The 240-byte object context (X360 builds it on the Render stack):
    // constant shadow cleared, list base 0, the pre-Z config from the module.
    lpContext->ResetShadowing();
    lpContext->miListIdBase       = 0;
    lpContext->mbPreZEnabled      = mbRenderPreZ;
    lpContext->mbPreZAlphaEnabled = mbRenderPreZAlpha;
    // ARTIST 8240C0FC..8240C130 copies the LINEAR threshold into context+E0.
    // Interpret827FD62C..654 compares the mesh centre's clip w against it.
    // The near-only switch controls a separate global used by occlusion;
    // it neither squares nor overrides this object-to-mesh threshold.
    // FLAG PC-platform leaf: retain the old range for controlled measurements.
    static const bool sbLegacyPreZRange = [] {
        const char* value = std::getenv("BRN_PREZ_ALL");
        return value && value[0] && value[0] != '0';
    }();
    const f32 lfPreZDistance = sbLegacyPreZRange ? 10000000000.0f : mfPreZDistanceThreshold;
    for (u32 luLane = 0; luLane < 4; ++luLane)
        lpContext->mvPreZDistanceThreshold[luLane] = lfPreZDistance;

}

bool BrnRendererModule::BuildDispatchLists(CgsGraphics::DispatchObjectContext* lpContext)
{
    InitializeDispatchContextPC(lpContext);
    if (mpInterpreter == nullptr)
        return false;
    if (mpMeshProducerInterpreterPC != nullptr)
    {
        // Prepared and published before releasing the render worker. Never
        // expand on both owners: diagnostics and the shared job arena have one
        // conversion owner even when this frame is rendered without overlap.
        CGS_ASSERT(maPreparedMeshFramesPC[muMeshReadFramePC].mbReady, "mesh frame not published");
        return maPreparedMeshFramesPC[muMeshReadFramePC].mbReady;
    }
    mSingleBufferedDispatchFrame.Reset();
    mpInterpreter->SetSingleBufferedDispatchFrame(&mSingleBufferedDispatchFrame);
    mpInterpreter->SetTime(0.0f);
    ConvertObjectsToMeshes(&mDoubleBufferedDispatchFrame, &mSingleBufferedDispatchFrame,
                           mpInterpreter, lpContext);
    SortDispatchLists(&mSingleBufferedDispatchFrame);
    return true;
}

// FLAG PC-platform leaf: only the main/update owner expands mesh commands.
// Each output bank stays paired with the GDL containing its constant data.
void BrnRendererModule::PrepareMeshFramePC(u32 luBank, CgsGraphics::DispatchFrame* lpInput)
{
    PreparedMeshFramePC& lrPrepared = maPreparedMeshFramesPC[luBank];
    if (!DispatchStorageAvailablePC(lrPrepared.mpFrame) || !DispatchStorageAvailablePC(lpInput))
    {
        lrPrepared.mbReady = false;
        mbDispatchStorageFailedPC = true;
        CgsDev::Log::WriteToLog("[renderer] mesh preparation requires allocated command banks; shutting down.\n");
        CgsSystem::HardwareInit::RequestShutdown();
        return;
    }
    const u64 luEpoch = renderengine::MeshPreparationPC::ResourceEpoch();
    if (lrPrepared.mbReady && lrPrepared.muResourceEpoch == luEpoch
        && lrPrepared.mbPreZ == mbRenderPreZ && lrPrepared.mbPreZAlpha == mbRenderPreZAlpha
        && lrPrepared.mfPreZDistance == mfPreZDistanceThreshold)
        return;

    renderengine::FrameProfile::CycleScope lProfile(renderengine::FrameProfile::UPDATE_MESH_PREPARE);
    if (auto* lpProfile = renderengine::FrameProfile::gCapture.mpCurrent)
    {
        ++lpProfile->muMeshPrepared;
        if (lrPrepared.mbReady) ++lpProfile->muMeshRebuilt;
    }
    CgsGraphics::DispatchObjectContext lContext;
    InitializeDispatchContextPC(&lContext);
    lrPrepared.mSortJobs.WaitAll();
    lrPrepared.mpFrame->Reset();
    mpMeshProducerInterpreterPC->SetSingleBufferedDispatchFrame(lrPrepared.mpFrame);
    mpMeshProducerInterpreterPC->SetTime(0.0f);
    ConvertObjectsToMeshesPC(lpInput, lrPrepared.mpFrame, mpMeshProducerInterpreterPC, &lContext);
    SortDispatchLists(lrPrepared.mpFrame);
    lrPrepared.muResourceEpoch = luEpoch;
    lrPrepared.mbPreZ = mbRenderPreZ;
    lrPrepared.mbPreZAlpha = mbRenderPreZAlpha;
    lrPrepared.mfPreZDistance = mfPreZDistanceThreshold;
    lrPrepared.mbReady = true;
}

void BrnRendererModule::BeginMeshFramePC()
{
    if (mpMeshProducerInterpreterPC == nullptr) return;
    // Called before releasing the render worker: cover cold start and controls
    // changed since publication. The other bin can now be recycled by update.
    maPreparedMeshFramesPC[1u - muMeshReadFramePC].mbReady = false;
    PrepareMeshFramePC(muMeshReadFramePC, &mDoubleBufferedDispatchFrame.GetDispatchFrameForRead());
    mpInterpreter->SetSingleBufferedDispatchFrame(&GetMeshFrameForReadPC());
    mpInterpreter->SetTime(0.0f);
}

void BrnRendererModule::PrepareMeshFrameForWritePC()
{
    if (mpMeshProducerInterpreterPC == nullptr) return;
    PrepareMeshFramePC(1u - muMeshReadFramePC, &mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite());
}

void BrnRendererModule::PublishMeshFramePC()
{
    if (mpMeshProducerInterpreterPC == nullptr) return;
    // Render has joined and resource updates have completed. Rebuild if their
    // fixups/imports/relocations invalidated the speculative CPU preparation.
    PrepareMeshFrameForWritePC();
    muMeshReadFramePC = 1u - muMeshReadFramePC;
    mpInterpreter->SetSingleBufferedDispatchFrame(&GetMeshFrameForReadPC());
    mpInterpreter->SetTime(0.0f);
}

void BrnRendererModule::EndMeshFramesPC()
{
    // FLAG PC-platform leaf: the last published bank may never be rendered.
    // Join both banks while the scheduler and frame-owner assertion service
    // still exist, before the later static renderer destructor sees them.
    for (auto& lrPrepared : maPreparedMeshFramesPC)
        lrPrepared.mSortJobs.WaitAll();
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
// Each of the five lists joins its sort job before consumption. The front/back
// helpers bracket the corresponding rasterizer state just as on the console.
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
                GetMeshFrameForReadPC().GetList(static_cast<u32>(liList))->GetCount();
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
    if (renderengine::ShadowProbe_Enabled())
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

            WaitForMeshSortPC(static_cast<u32>(liList));

            // ARTIST Render @8240C84C..CB0C brackets lists 0/1/4 with the Front helpers
            // and 2/3 with the Back helpers. The manager flag selects the locked group.
            if (liSlot == 0)
                mShadowMapRenderManager.BeginFrontFaceCullRender();
            else
                mShadowMapRenderManager.BeginBackFaceCullRender();

            GetMeshFrameForReadPC().GetList(static_cast<u32>(liList))
                ->DispatchAllMeshesZOnly(mpInterpreter, lpContext);

            if (liSlot == 0)
                mShadowMapRenderManager.EndFrontFaceCullRender();
            else
                mShadowMapRenderManager.EndBackFaceCullRender();
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
    if (renderengine::ShadowProbe_Enabled())
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

#if BRN_ENVMAP_PASS_AVAILABLE
// =================================================================================================
// THE ENVIRONMENT-MAP FACE BRACKET -- BeginRenderEnvironmentMapFace @0x823F63E0 /
// EndRenderEnvironmentMapFace @0x823FC5E8.
//
// Both were ledger-`reviewed` and ABSENT from this tree until this wave -- the fourth subsystem in
// which that has been true, so the status is worth nothing and the asm is worth everything.
//
// WHAT Begin DOES, in the asm's order (0x823F63E0-0x823F65AC):
//   1. the two asserts, whose strings ARE the accessor's name (see BrnRendererMemory.h);
//   2. GetEnvMapBuffer()->SetRenderTargetStateInvertDepth(luFace) -- bind the target's section-0
//      surface state and set a viewport whose depth range is INVERTED, MinZ=1 / MaxZ=0
//      (CgsRenderTarget.cpp:336, already in the tree). The face argument is deliberately ignored by
//      that body, faithfully: on the Xenos every face renders into the SAME EDRAM tile and the face
//      only selects the RESOLVE destination, which is End's job;
//   3. ONE combined clear, `sub_82B61E18(&colour, &clearParams, 4)`.
//   4. the three cached render-state applies.
//
// (3) DECODED, value by value, with the displacement each constant is addressed at -- this file
// already pays for the lesson that a .rdata block runs past its symbol (see the bracket constants
// further down):
//   colour.rgb = lfWhiteLevel * flt_82004740 (`lfs f0, flt_82004740@l(r11)` @0x823F6494, `fmuls f0,
//     f31, f0` @0x823F649C, stored three times @0x823F64A0/A4/AC). flt_82004740 +0x00 is ALREADY
//     attested in this file as 0.300000012f -- it is KF_BACKGROUND_COLOUR_GREY, the grey-background
//     channel BeginRenderAntiAliased loads from the same displacement. The env-map face therefore
//     clears to 30% of the frame's white level in all three channels: a mid-grey stand-in for the
//     sky wherever neither the world nor the dome covers a texel.
//   colour.a   = flt_82001CC0 (`stfs f13, ...var_54` @0x823F64B8) == 0.0f, attested in this file
//     as KF_QUARTER_RES_CLEAR_COMPONENT.
//   clearParams = { flags 0x30, depth, stencil 0 }: `li r11, 0x30 / stw r11, var_70` @0x823F63F0-
//     0x823F6404 and `stw r28(0), var_68` @0x823F6410. 0x30 is the XENON D3DCLEAR mask
//     ZBUFFER(0x10) | STENCIL(0x20) -- the same word, with the same meaning, that the shadow pass
//     already builds (ShadowPassPCLeaf.h's ClearDepthStencilParameters banner).
//   depth      = 0.0f, AND THE TWO-STEP IS THE PROOF: the console stores flt_82001C98 == 1.0f into
//     var_6C at 0x823F6420 -- before the asserts, i.e. the struct's default construction, which is
//     exactly what the shadow pass's own {0x30, 1.0f, 0} block is -- and then OVERWRITES it with
//     flt_82001CC0 == 0.0f at 0x823F64C0, immediately before the call. So the value that reaches
//     the device is 0.0, and the override is the whole point: under the INVERTED depth range set one
//     step earlier, 0.0 is the FAR plane. Clearing to 1.0 here would clear the face to the NEAR
//     plane and reject every triangle. The reconstruction writes the settled value once and records
//     the two-step here rather than replaying a store that is immediately dead.
//   the `4`    = renderengine::E_TARGETID_COLOUR_ALL, which the helper turns into `flags |= 0xF`
//     (all four Xenon colour targets). Combined with the 0x30 already in the word, one call clears
//     colour + depth + stencil. (device.h's ETargetId banner establishes that on X360 the value 4
//     means ALL COLOUR TARGETS and NOT depth/stencil -- the PS3 DWARF numbers this enum differently
//     and importing its numbering would turn this into a depth-only clear.)
//
// (4) DECODED. The tail is three copies of one pattern:
//     `v = <a state global>; if (!<its lock byte> && <the shadow slot> != v) { <apply>; <shadow> = v; }`
// which is character for character shadow::Device::SetState(const DepthStencilState*) @0x82276AD0,
// (const RasterizerState*) @0x82276B38 and (const BlendMaterialState*) @0x82276A68 -- all three
// already reconstructed in shadowingdevice.cpp with those exact gates and shadows. The X360
// compiler INLINED them here, which is why the pseudocode shows the raw globals; un-inlining them
// is the reconstruction rule, and the DecFIGS DWARF independently confirms it -- BrnGraphicsUnity.cpp
// :4633 lists this function's body as `ClearDepthStencilParameters lClearZbuffer;` followed by
// exactly three `shadow::Device::SetState(...)` calls.
//   dword_8301091C = CgsDepthStencilStateFactory::saDepthStencilStates[4]
//                    == E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON (CgsDepthStencilStateFactory.h:93)
//                    -- Z on, Z func GREATER-EQUAL, Z write on. GREATER-EQUAL is the INVERTED-depth
//                    test, and it is the one state in the whole five-slot table that is: the pass
//                    clears depth to 0.0 and keeps whatever is nearer, i.e. larger. The clear value,
//                    the viewport inversion and this slot are three independent witnesses to the same
//                    fact, which is why none of them is a guess.
//   dword_83010A3C = CgsRasterizerStateFactory::saRasterizerStates[1]
//                    == E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT (CgsRasterizerStateFactory.h:148)
//   dword_83010F70 = CgsBlendStateFactory::saBlendStates[0]
//                    == E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA (CgsBlendStateFactory.h:148)
// and the three gate bytes (mbDepthStencilStateLocked, mbRasteriserStateLocked, byte_83010907 ==
// mbBlendStateLocked) plus the three shadow slots (dword_83010A28 / dword_83010A2C / dword_83010964)
// are the setters' OWN state, which is why nothing here reads or writes them: calling the setter is
// how the console's stores happen.
//
// ⚠ WHY THIS DOES NOT COPY THE SHADOW PASS'S TAIL. BrnGraphics::ShadowMapRenderManager::
// BeginRenderShadowMap ends with `ImDeviceSetDepthStencilState(gpShadowDepthStencilState)` -- ONE
// state, through ImRendererBase::SetState @0x82276AD0's immediate-mode spelling, and its slot is
// saDepthStencilStates[0] (Z <= ). That is a DIFFERENT console call with a DIFFERENT argument; the
// env-map face applies all three thirds and picks the GREATER-EQUAL slot. Reusing the shadow pass's
// line here -- which the brief's "mirror what the tree's shadow pass does" could be read as asking
// for -- would bind the wrong depth function and reject the whole face.
// =================================================================================================
void BrnRendererModule::BeginRenderEnvironmentMapFace(u32 luFace, f32 lfWhiteLevel)
{
    CgsRenderTarget* const lpEnvMapBuffer = mAllocatedRenderTargets.GetEnvMapBuffer();
    CGS_ASSERT(lpEnvMapBuffer != nullptr, "mAllocatedRenderTargets.GetEnvMapBuffer()");
    CGS_ASSERT(lpEnvMapBuffer != nullptr && lpEnvMapBuffer->GetRenderTarget() != nullptr,
               "mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget()");

    // Bind the cube target's surfaces + an INVERTED-depth viewport (see the banner).
    lpEnvMapBuffer->SetRenderTargetStateInvertDepth(luFace);

    renderengine::ClearColorParameters lClearColour;
    lClearColour.mafColourRGBA[0] = lfWhiteLevel * KF_ENV_MAP_CLEAR_COLOUR_SCALE;
    lClearColour.mafColourRGBA[1] = lClearColour.mafColourRGBA[0];
    lClearColour.mafColourRGBA[2] = lClearColour.mafColourRGBA[0];
    lClearColour.mafColourRGBA[3] = KF_ENV_MAP_CLEAR_ALPHA;

    // [FLAG PC bring-up TEST HOOK, OFF BY DEFAULT] BRN_ENVMAP_DEBUG=1 replaces the console's grey
    // with this face's own colour so the cube's orientation can be read off a screenshot of a
    // reflective car. See EnvMapDebugFaceColours in this file. The alpha, the depth value and every
    // state below are UNTOUCHED by the knob -- only the three colour channels move, so the pass
    // being measured is still the pass that ships.
    if (EnvMapDebugFaceColours() && luFace < 6u)
    {
        lClearColour.mafColourRGBA[0] = KAF_ENV_MAP_DEBUG_FACE_COLOURS[luFace][0];
        lClearColour.mafColourRGBA[1] = KAF_ENV_MAP_DEBUG_FACE_COLOURS[luFace][1];
        lClearColour.mafColourRGBA[2] = KAF_ENV_MAP_DEBUG_FACE_COLOURS[luFace][2];
    }

    renderengine::ClearDepthStencilParameters lClearZbuffer;
    lClearZbuffer.mu32Flags   = KU_XENON_CLEAR_ZBUFFER | KU_XENON_CLEAR_STENCIL;   // 0x30
    lClearZbuffer.mfDepth     = KF_ENV_MAP_CLEAR_DEPTH;                            // 0.0 == FAR, inverted
    lClearZbuffer.mu32Stencil = 0;

    renderengine::Device::Clear(lClearColour, lClearZbuffer, renderengine::E_TARGETID_COLOUR_ALL);

    // The three cached render-state applies (see the banner for each slot's identification).
    shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
                                 E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZGTEQ_ZWRITEON));
    // FLAG PC-platform leaf: the native cube projection mirrors clip X to
    // compensate for the original LookAt horizontal basis. That reverses screen
    // winding relative to the former PC projection, so restore the original
    // CULL FRONT state attested by BeginRenderEnvironmentMapFace @0x823F63E0.
    // Projection and cull state must change together; ordinary world passes keep
    // their existing camera and rasterizer states.
    shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
                                 E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT));
    shadow::Device::SetState(CgsBlendStateFactory::GetState(
                                 E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA));
}

// @0x823FC5E8 -- resolve colour target 0 of the env-map render target into cube face luFace.
// The console body is the two asserts (same strings, file lines 4204/4205) and one call:
//     sub_823F9170(GetEnvMapBuffer()->GetRenderTarget() + 0x20, luFace)
// rt+0x20 IS maColourTargets[0] on the 4-byte-pointer image (rwgpfxrendertarget.h's layout block:
// mpSection0State at +0x1C, maColourTargets at +0x20), so the call is
// `maColourTargets[0].Resolve(luFace)` -- the PER-FACE overload of Target::Resolve, whose other
// xref is BrnGraphics::ShadowMapRenderManager::EndRenderShadowMap. The member is reached BY NAME
// here; the +0x20 is documentation, and does not survive the x64 widening in any case.
void BrnRendererModule::EndRenderEnvironmentMapFace(u32 luFace)
{
    CgsRenderTarget* const lpEnvMapBuffer = mAllocatedRenderTargets.GetEnvMapBuffer();
    CGS_ASSERT(lpEnvMapBuffer != nullptr, "mAllocatedRenderTargets.GetEnvMapBuffer()");
    CGS_ASSERT(lpEnvMapBuffer != nullptr && lpEnvMapBuffer->GetRenderTarget() != nullptr,
               "mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget()");

    lpEnvMapBuffer->GetRenderTarget()->maColourTargets[0].Resolve(luFace);
}
#endif  // BRN_ENVMAP_PASS_AVAILABLE

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
        const auto lPlan = BrnGraphics::ScaleTilingPlan(BrnGraphics::KMSAA_TILING_PLAN,
            mAllocatedRenderTargets.GetAntiAliasBuffer()->GetWidth(),
            mAllocatedRenderTargets.GetAntiAliasBuffer()->GetHeight());
        renderengine::D3DDevice_BeginTiling(lpDevice, 0u, KU_NUM_MSAA_TILES,
                                            lPlan.maTile,
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

    // [DIAG] THE WORLD PASS'S SURFACE PAIR, at the moment the bracket opens it. This is the
    // reference the trail pass (BrnTrailSystem.cpp) and the resolve are compared against; on its
    // own it says nothing. See BrnDiagBoundSurfaces.h. DELETE-WHEN-STABLE.
    BrnDiag::LogBoundSurfaces("world-begin");
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

    // [DIAG] THE SURFACE PAIR THE RESOLVE IS ABOUT TO READ. Together with "world-begin" and
    // "trail" this closes the identity question: a trail drawn into a surface this call does not
    // read is a draw nothing ever presents. DELETE-WHEN-STABLE.
    BrnDiag::LogBoundSurfaces("resolve-in");

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
                BrnGraphics::ScaleTilingPlan(BrnGraphics::KMSAA_TILING_PLAN,
                    mAllocatedRenderTargets.GetAntiAliasBuffer()->GetWidth(),
                    mAllocatedRenderTargets.GetAntiAliasBuffer()->GetHeight()).maTile[luTile];

            // Each tile lands back at its own screen position: the destination point is the tile
            // rect's top-left corner. The asm reads the rect's FIRST TWO dwords -- `lwz r10, 8(r11)`
            // and `lwz r11, 0xC(r11)` @0x823FFCA4/A8, i.e. maTile[tile].mu32Left / .mu32Top -- and
            // stores them as the two-dword point @0x823FFCAC/B0.
            const XenonPoint lDestPoint = { static_cast<s32>(lrTile.mu32Left),
                                            static_cast<s32>(lrTile.mu32Top) };

            // Depth/stencil first, fragment 0 only. pClearColor is NULL on this one (`li r10, 0`
            // @0x823FFCC0) -- a depth resolve has no colour to clear to.
            //
            // ⚠ WHAT THIS PAIR-PER-TILE MEANS ON PC, stated here because the CALLER is what makes it
            // load-bearing and the leaf is where it is handled (anti-aliasing wave, 2026-08-16).
            // The console's per-tile depth resolve copies THAT TILE's EDRAM depth; the PC depth
            // resolve is the D3D9 vendor "RESZ" mechanism, which resolves the WHOLE bound
            // depth-stencil and CANNOT be rectangle-clamped. That is fine for tile 0 -- and only for
            // tile 0. The 0x300 colour resolve immediately below carries D3DCLEAR_ZBUFFER over its
            // own band, so by the time tile 1's depth resolve runs, rows 0..384 of the bound depth
            // surface have ALREADY BEEN CLEARED TO Z=1, and repeating the whole-surface RESZ would
            // overwrite the good copy with a half-cleared one. The down-sample buffer's INTZ would
            // then hand the depth-of-field / motion-blur permutations a flat top band -- plausible
            // enough on screen to be missed.
            // THE RULE THE LEAF IMPLEMENTS (D3DDevice_Resolve, XenonD3D9Shims.cpp): perform the RESZ
            // for the FIRST depth resolve of the bracket only -- discriminated with no state by the
            // source rect's TOP being 0, which is true of KMSAA_TILING_PLAN.maTile[0] {0,0,1280,384}
            // and of KNO_MSAA_TILING_PLAN.maTile[0] {0,0,1280,720} and false for maTile[1]. The
            // console's calls are reproduced verbatim here; the collapse belongs to the platform
            // that cannot express a partial depth resolve, not to this body.
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
            BrnGraphics::ScaleTilingPlan(BrnGraphics::KNO_MSAA_TILING_PLAN,
                mAllocatedRenderTargets.GetAntiAliasBuffer()->GetWidth(),
                mAllocatedRenderTargets.GetAntiAliasBuffer()->GetHeight()).maTile[0];

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

// =================================================================================================
// BrnRendererModule::ComputeSunCoronaVisibility -- X360 @0x82405D80.
// (Ledger `reviewed`, and ABSENT from this file until coronas step 2 -- the second recurring trap
// this campaign records: a ledger `reviewed` does not mean the body exists.)
//
// THE WHOLE CONSOLE BODY, instruction for instruction:
//     0x82405D98  lwz r3, 0(this+0xC9EC) ; bl PerfMonGpu::StartMonitor    mGpuMonitors.miSunCoronaVisibilityTest
//     0x82405DA0  lbz r11, 0xAD0(this)                                    mu8ShaderConstantsFrameInternal
//     0x82405DA8  mulli r11, r11, 0x320                                   sizeof(BrnShaderConstantsFrame)
//     0x82405DB0  addi r30, r11, 0x490                                    &maShaderConstantsFrames[internal]
//     0x82405DB8  bl BrnShaderConstantsFrame::GetViewProjectionMatrix     -> the stack Matrix44
//     0x82405DBC  lbz 0x31C(r30) / assert "false == mbLockedForWriting"   the INLINED getters' assert
//     0x82405DFC  lvx128 v2, r30, 0x300                                   mUnbiasedKeyLightDirection
//     0x82405E00  lvx128 v1, r30, 0x040                                   mViewPosition
//     0x82405E04  bl BrnSunCorona::ComputeSunPositionOnScreen             (r3 = this+0xC520 = mSunCorona)
//     0x82405E08  lwz r5, 0x248(this)                                     mapRenderTarget[4]  DOWN_SAMPLE
//     0x82405E0C  lwz r4, 0x260(this)                                     mapRenderTarget[10] SUN_CORONA
//     0x82405E10  bl BrnSunCorona::GenerateOcclusionBuffer
//     0x82405E18  bl PerfMonGpu::StopMonitor
//
// The two `lvx128` loads are the INLINED accessors GetUnbiasedKeyLightDirection() (+0x300) and
// GetViewPosition() (+0x40) -- BrnShaderConstantsFrame.h:96/78 -- which is why only ONE of the
// three reads has a `bl` and why the lock assert appears once, at the first inlined getter. They
// are DE-INLINED onto the named accessors here (AGENTS.md "inlining reversal").
//
// ⚠ THE PERFMON BRACKET IS NOT REPRODUCED, for the reason this file's banner at :250 gives: every
// id in mGpuMonitors is 0 on this build because nothing calls PerfMonGpu::AddMonitor, so a bracket
// here would time one monitor id shared with every other pass.
//
// The world fills the original external bank through RendererIO; SwapBuffers
// publishes that complete bank for this render owner to read.
// =================================================================================================
void BrnRendererModule::ComputeSunCoronaVisibility()
{
    BrnShaderConstantsFrame& lrFrame = maShaderConstantsFrames[mu8ShaderConstantsFrameInternal];

    const Matrix44 lViewProjection = lrFrame.GetViewProjectionMatrix();
    const Vector3  lViewPosition   = lrFrame.GetViewPosition();
    const Vector3  lSunDir         = lrFrame.GetUnbiasedKeyLightDirection();

    // [FLAG PC bring-up gate] see the banner: an unpublished internal frame is all zeros, and a
    // zero view projection puts the flare at a NaN screen position with nothing erroring.
    const bool lbFrameLive =
        (lViewProjection.xAxis.x != 0.0f || lViewProjection.yAxis.x != 0.0f
         || lViewProjection.zAxis.x != 0.0f)
        && (lSunDir.x != 0.0f || lSunDir.y != 0.0f || lSunDir.z != 0.0f);
    if (!lbFrameLive)
    {
        static bool sbLoggedDeadFrame = false;
        if (!sbLoggedDeadFrame)
        {
            sbLoggedDeadFrame = true;
            CgsDev::Log::WriteToLog(
                "[suncorona] SKIPPED: the INTERNAL shader-constants frame carries no view"
                " projection / key-light direction yet -- its producer is"
                " WorldModule::GenerateDispatchLists (external slot) + SwapBuffers (external -> internal)\n");
        }
        return;
    }

    mSunCorona.ComputeSunPositionOnScreen(lViewProjection, lViewPosition, lSunDir);

    mSunCorona.GenerateOcclusionBuffer(mAllocatedRenderTargets.GetSunCoronaBuffer(),
                                       mAllocatedRenderTargets.GetDownSampleBuffer());
}

// =================================================================================================
// BrnRendererModule::BeginQuarterResBuffer -- X360 @0x82408C38.
//
// Open the QUARTER-RESOLUTION particle buffer. Console body, in order:
//     assert(mAllocatedRenderTargets.GetParticleBuffer() != NULL)                       (:4302)
//     assert(mAllocatedRenderTargets.GetParticleBuffer()->GetRenderTarget() != NULL)    (:4303)
//     CgsRenderTarget::SetRenderTargetState(particleBuffer, 0)
//     clear colour = {0,0,0,0} (four consecutive stack floats @0x82408CCC..D8)
//     renderengine::Device::Clear(thatColour, 4)          -- 4 == COLOUR_ALL, not depth
//     push saDepthStencilStates[3] ZON_ZALL_ZWRITEON      (dword_83010918)
//     push saRasterizerStates[2]   Scissor_CullModeNone   (dword_83010A40)
//     push saBlendStates[7]        NoColourWrite_NoAlphaTest (dword_83010F8C, @0x82408D9C)
//     BrnRendererMemory::BlitDepth(mIm2dRenderer, GetDownSampleBuffer())
//     CgsRenderTarget::SetRenderTargetState(particleBuffer, 0)
//
// THE CLEAR IS THE WHOLE POINT OF THE BUFFER, and it is what this build has been missing: the
// particles accumulate on ZERO, so the composite that follows can put them OVER the scene with a
// (1 - accumulatedAlpha) term. Adding them straight onto the scene -- which is what this backend
// did until 2026-09-05 -- makes a saturating plume ADD its background instead of REPLACING it.
//
// THE STATE TRIPLE IS THE DEPTH BLIT'S, not the particles': write depth, write no colour, cull
// nothing. The particle pass that follows sets its own per-material states.
//
// ⚠ THE TRAILING RE-BIND IS THE CONSOLE'S OWN and is reproduced: BlitDepth ends with
// shadow::Device::ResetShadowing, so the "last state installed" cache no longer knows what is
// bound, and a SetRenderTargetState that would otherwise be skipped as redundant has to run.
//
// ⚠ THE PERFMON BRACKET IS NOT REPRODUCED (this file's banner at :250): every id in mGpuMonitors
// is 0 on this build because nothing calls PerfMonGpu::AddMonitor.
// =================================================================================================
void BrnRendererModule::BeginQuarterResBuffer()
{
    CgsRenderTarget* const lpParticleBuffer = mAllocatedRenderTargets.GetParticleBuffer();
    CGS_ASSERT(lpParticleBuffer != nullptr,
               "mAllocatedRenderTargets.GetParticleBuffer() != NULL");
    if (lpParticleBuffer == nullptr || lpParticleBuffer->GetRenderTarget() == nullptr)
    {
        return;
    }
    CGS_ASSERT(lpParticleBuffer->GetRenderTarget() != nullptr,
               "mAllocatedRenderTargets.GetParticleBuffer()->GetRenderTarget() != NULL");

    lpParticleBuffer->SetRenderTargetState(0);

    renderengine::ClearColorParameters lClearColour;
    lClearColour.mafColourRGBA[0] = 0.0f;
    lClearColour.mafColourRGBA[1] = 0.0f;
    lClearColour.mafColourRGBA[2] = 0.0f;
    lClearColour.mafColourRGBA[3] = 0.0f;
    renderengine::Device::Clear(lClearColour, renderengine::E_TARGETID_COLOUR_ALL);

    renderengine::DepthStencilState* const lpDepthStencil =
        CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZALL_ZWRITEON);
    renderengine::RasterizerState* const lpRasterizer =
        CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE);
    renderengine::BlendMaterialState* const lpBlend =
        CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_NO_COLOUR_WRITE_NO_ALPHA_TEST);
    if (lpDepthStencil == nullptr || lpRasterizer == nullptr || lpBlend == nullptr)
    {
        // [FLAG PC bring-up gate] the same refusal, for the same reason, BrnSunCorona's passes
        // carry: on PC the three factories are Constructed from a deferred block in Render, so a
        // slot can still be null and shadow::Device::SetState would dereference it.
        static bool sbLoggedStates = false;
        if (!sbLoggedStates)
        {
            sbLoggedStates = true;
            CgsDev::Log::WriteToLog("[qres] BeginQuarterResBuffer DEFERRED: the blend /"
                                    " rasteriser / depth-stencil state factories are not"
                                    " Constructed yet\n");
        }
        return;
    }

    shadow::Device::SetState(lpDepthStencil);
    shadow::Device::SetState(lpRasterizer);
    shadow::Device::SetState(lpBlend);

    mAllocatedRenderTargets.BlitDepth(mIm2dRenderer, mAllocatedRenderTargets.GetDownSampleBuffer());

    lpParticleBuffer->SetRenderTargetState(0);
}

// =================================================================================================
// BrnRendererModule::EndRenderAntiAliased -- X360 @0x82408B00.
//
// Close the anti-aliased pass: composite the quarter-res particle buffer back over the scene.
// Console body, in order:
//     PerfMonGpu::StartMonitor(mGpuMonitors.miQuarterResParticles)     (a1[12922])
//     v3 = mapRenderTarget[4]                                          the DOWN-SAMPLE buffer
//     CgsRenderTarget::SetRenderTargetState(v3, 0)
//     push saDepthStencilStates[1] ZOFF_ZALL_ZWRITEOFF  (dword_83010910, @0x82408BCC)
//     push saRasterizerStates[2]   Scissor_CullModeNone (dword_83010A40)
//     push saBlendStates[0]        Opaque_Modulate_NoAlphaTest_DestRGBA (dword_83010F70)
//     BrnRendererMemory::BlitComposite(mIm2dRenderer, v3, mapRenderTarget[9], false, false)
//     rw::graphics::postfx::RenderTarget::Resolve(v3->GetRenderTarget(), false, true)
//     PerfMonGpu::StopMonitor
//
// THE TWO SLOT NUMBERS ARE READ, NOT GUESSED: mAllocatedRenderTargets sits at this+0x238, the body
// loads the composite's destination as `lwz r28, 0x248(r29)` -- (0x248-0x238)/4 == 4 == DOWN_SAMPLE
// -- and its overlay as `lwz r6, 0x25C(r29)` -- (0x25C-0x238)/4 == 9 == PARTICLE.
//
// ⚠ Resolve IS CALLED and IS A NO-OP HERE, deliberately. On the Xenos it copies the EDRAM tile back
// over the sampleable texture BlitComposite just read; on PC that texture IS the surface
// (PostFxRenderTargetPCLeaf.cpp's Resolve() banner), so there is nothing to copy. It is reproduced
// rather than dropped so the call graph still matches and the day a backend needs it, it is there.
//
// ⚠ THE PERFMON BRACKET IS NOT REPRODUCED (this file's banner at :250).
// =================================================================================================
void BrnRendererModule::EndRenderAntiAliased()
{
    CgsRenderTarget* const lpSceneTarget    = mAllocatedRenderTargets.GetDownSampleBuffer();
    CgsRenderTarget* const lpParticleBuffer = mAllocatedRenderTargets.GetParticleBuffer();
    if (lpSceneTarget == nullptr || lpSceneTarget->GetRenderTarget() == nullptr
        || lpParticleBuffer == nullptr)
    {
        return;
    }

    lpSceneTarget->SetRenderTargetState(0);

    renderengine::DepthStencilState* const lpDepthStencil =
        CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF);
    renderengine::RasterizerState* const lpRasterizer =
        CgsRasterizerStateFactory::GetState(E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_NONE);
    renderengine::BlendMaterialState* const lpBlend =
        CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_OPAQUE_MODULATE_NO_ALPHA_TEST_DEST_RGBA);
    if (lpDepthStencil == nullptr || lpRasterizer == nullptr || lpBlend == nullptr)
    {
        static bool sbLoggedStates = false;
        if (!sbLoggedStates)
        {
            sbLoggedStates = true;
            CgsDev::Log::WriteToLog("[qres] EndRenderAntiAliased DEFERRED: the blend /"
                                    " rasteriser / depth-stencil state factories are not"
                                    " Constructed yet\n");
        }
        return;
    }

    shadow::Device::SetState(lpDepthStencil);
    shadow::Device::SetState(lpRasterizer);
    shadow::Device::SetState(lpBlend);

    mAllocatedRenderTargets.BlitComposite(mIm2dRenderer, lpSceneTarget, lpParticleBuffer,
                                          false, false);

    lpSceneTarget->GetRenderTarget()->Resolve(false, true);
}

// The three per-frame values Render reads off the LAYER-0 INTERNAL BrnEffectsFrame early
// (asm 0x8240C290-0x8240C314) and carries in stack slots for the rest of the function.
struct BrnRendererPostFxFrameBytes
{
    // var_CD0 -- mMotionBlurData.mbIsActive (frame +0x1E0, `lbz r9, 0x1E0(r9)` @0x8240C2DC).
    // Consumed by BeginRenderAntiAliased's lbClearStencil (r5 @0x8240CDB0) and by
    // BrnPostFx::Render's lbMotionBlurEnabled (r7 @0x8240DE18).
    bool mbMotionBlurActive;

    // var_CCE -- (u8)(mMotionBlurData.mfWorldBlurAmount * 255.0f) (frame +0x1DC, `lfs f13,
    // 0x1DC(r8)` * flt_82010C20, `fctiwz` then the low byte @0x8240C2C0-0x8240C300).
    // Consumed by BeginRenderAntiAliased (r6 @0x8240CDAC) and ResolveMSAA (r5 @0x8240D5B4).
    u8 mu8WorldBlurStencil;

    // var_CCF -- the same quantisation of mfCarsBlurAmount (frame +0x1D8, @0x8240C2F0-0x8240C314).
    // Consumed by the CAR passes' stencil reference (r3 @0x8240CED0 / @0x8240D344), which this
    // build does not reconstruct yet; carried so the read is complete and greppable.
    u8 mu8CarsBlurStencil;
};

// ==================================================================================================
// BrnRendererModule::Render @0x8240BFA8 -- THE EFFECTS-FRAME -> BrnPostFx APPLY BLOCK.
//
// This is Render's OWN block (pseudocode lines 964..1260, asm 0x8240C290-0x8240C314 and
// 0x8240D700-0x8240DD4C), verbatim in behaviour and in Render's own translation unit, where the
// console had it. Render calls each function below from the point the console executes it.
//
// WHAT THE BLOCK IS. The console's post-fx effects are driven entirely by data: the effects module,
// the world's environment manager and the GUI's fx-event arbitrator each write a BrnEffectsFrame per
// layer; BrnGraphics::EffectsArbitrator blends the layers into one evaluated block per effect; and
// this block turns each evaluated block into (a) an m_enabledFx bit and (b) a state publish into the
// render-engine effect object. Nothing here decides anything -- every branch is a frame byte and
// every constant is read off the image.
//
// THE FRAME IS READ AT THE LAYER-0 *INTERNAL* SLOT, ALWAYS. Every read below is
//   `496 * mu8EffectsFrameInternal + mapaEffectsFrames[0]`
// (`lbz r5, 0xC(r30)` / `lwz r11, 0(r30)` / `mulli r11, r11, 0x1F0` with r30 = this+0x480, repeated
// once per effect at 0x8240D704 / 0x8240D7B0 / 0x8240D8D0 / 0x8240D99C / 0x8240DC98). 0x1F0 == 496 is
// the GUEST stride and is NOT reproduced here: the frame is reached through the arbitrator's
// GetInternalEffectsFrame(layer, slot) accessor, which indexes the host array with the HOST stride.
//
// THE Eval* RETURN VALUES ARE DISCARDED, and that is the asm, not a simplification. At all four call
// sites the sequence is `bl sub_823F9xxx` immediately followed by `li r11, 1` (0x8240D728/D72C,
// 0x8240D7D8/D7E0, 0x8240D8F8/D900, 0x8240D9C4/D9CC): the "did this effect evaluate" flag is set
// UNCONDITIONALLY after the call, so what actually gates the effect is the frame's own bool, tested
// before the call. Consuming the return value here would be a behaviour the binary does not have.
//
// (Hex-Rays renders those calls as `sub_823F9AA8(&out, this + 1152)` -- out in r3, the arbitrator in
// r4 -- which is the large-struct-return convention, not an argument order to copy. The seam this
// file codes against is BrnEffectsArbitrator.h's `bool EvalBloom(BrnEffects::BloomData&) const`, per
// the DWARF; whichever of the two the arbitrator group lands, the CALL SITE is the same and the
// result is unused.)
// ==================================================================================================

// The original bloom scale globals are defined and debug-registered in
// EnvironmentManager, their DWARF home. Render reads them through BrnRendererModule.h.

namespace
{
    // Degrees -> radians. flt_8203B6D4, addressed as `(flt_8203B6D4 - 0x8203B710)(r21)` at
    // 0x8240D87C (the vignette angle) and 0x8240DC44 (the blur angle); Hex-Rays prints both uses as
    // the literal 0.017453292 (pseudocode lines 1050 and 1230).
    const f32 KF_DEGREES_TO_RADIANS = 0.017453292f;

    // The depth-of-field PROJECTION planes, which Render supplies rather than the effects frame:
    // flt_82004014 -> DepthOfField::State::m_projNearPlane (`stfs f0, 0x3E0(r20)` @0x8240D988) and
    // flt_82009E10 -> m_projFarPlane (`stfs f0, 0x3E4(r20)` @0x8240D990). Hex-Rays prints the pair
    // as `_R20[248] = 0.1; _R20[249] = 1000.0;` (pseudocode lines 1092-1093).
    const f32 KF_DOF_PROJECTION_NEAR_PLANE = 0.1f;
    const f32 KF_DOF_PROJECTION_FAR_PLANE  = 1000.0f;

    // The B4-blur speed normaliser: mfSpeedMPH * flt_82048914, clamped at 1.0f
    // (`lfs f0, flt_82048914@l(r11)` / `fmuls f12, f13, f0` / `fcmpu`+`fmr f12, f31` @0x8240DA50-
    // 0x8240DA60; Hex-Rays prints it as `v182 = (*(...+464) * 0.0060606059)` with the `> 1.0` clamp,
    // pseudocode lines 1122-1124). 464 == 0x1D0 == BrnEffectsFrame::mfSpeedMPH.
    const f32 KF_BLUR_SPEED_MPH_SCALE = 0.0060606059f;   // flt_82048914  (== 1/165)

    // The two yaw-rate scalars the blur centres and amounts are biased by. Both are splatted from a
    // one-float stack block and multiplied by splat(mAngularVelocity.y):
    //   flt_82009B70 == -0.125f  -> the CENTRE bias   (stored to two stack blocks @0x8240DA88 /
    //                               @0x8240DA90, Hex-Rays `v328[0] = -0.125; v329[0] = -0.125;`)
    //   flt_82003F40 ==  0.25f   -> the AMOUNT bias   (@0x8240DAB8, Hex-Rays `v302 = 0.25;`)
    const f32 KF_BLUR_CENTRE_YAW_SCALE = -0.125f;   // flt_82009B70
    const f32 KF_BLUR_AMOUNT_YAW_SCALE =  0.25f;    // flt_82003F40

    // The motion-blur STENCIL quantiser: both blur amounts are multiplied by flt_82010C20 and
    // truncated to a byte (`fmuls f13, f13, f0` / `fctiwz` / `stfiwx` / `stb` @0x8240C2C0-0x8240C314).
    // ATTESTED at 255.0f -- conductor idat dump, DATA_NOTE.md section 2 -- and corroborated inside
    // the pseudocode, which prints the same multiply as `(*(... + 476) * 255.0)` (line 400).
    const f32 KF_MOTION_BLUR_STENCIL_SCALE = 255.0f;   // flt_82010C20

    const f32 KF_ZERO = 0.0f;
    const f32 KF_ONE  = 1.0f;

    // fsel-pair clamp to [0,1] (`fsel f0, -x, f27(0.0), x` then `fsubs f11, f31(1.0), f0` /
    // `fsel f0, f11, f0, f31` @0x8240DB88-0x8240DBA8). De-optimised to the arithmetic it expresses,
    // per AGENTS.md's strength-reduction rule.
    f32 Clamp01(f32 lfValue)
    {
        if (lfValue < KF_ZERO)
            return KF_ZERO;
        if (lfValue > KF_ONE)
            return KF_ONE;
        return lfValue;
    }

    // `vandc vD, vS, v13` with v13 == splat(0x80000000) (`vspltisw v13, -1` + `vslw v13, v13, v13`
    // @0x8240DA70 / @0x8240DA8C) -- a sign-bit clear, i.e. fabs (asm 0x8240DB48).
    f32 AbsValue(f32 lfValue)
    {
        return (lfValue < KF_ZERO) ? -lfValue : lfValue;
    }

    // The [FLAG PC bring-up] gate every function in this file shares: on the console the arbitrator
    // is Constructed by BrnRendererModule::Construct @0x8240A778 line 126 and can never be absent,
    // so the console body has no such test. On PC it is Constructed lazily (see
    // EnsureEffectsArbitratorBringUp in BrnRendererModule.cpp) and is null until then. Keeping the
    // test HERE, at the seam, is the same choice EnsurePostFxSceneTargets makes for the render-target
    // pool: the reconstructed body stays untestful and the PC failure degrades instead of crashing.
    // DELETE with the bring-up.
    const BrnEffectsFrame* GetBaseInternalFrame(const BrnGraphics::EffectsArbitrator* lpArbitrator)
    {
        if (lpArbitrator == 0)
            return 0;
        return &lpArbitrator->GetInternalEffectsFrame(
            BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_BASE, 0u);
    }

    // The last frame's evaluated state, for the sampled diagnostic below only.
    struct DiagState
    {
        bool mbBloom;
        bool mbVignette;
        bool mbDepthOfField;
        bool mbBlur;
        bool mbTint2d;
        bool mbTint3d;   // the COLOUR-CUBE tint (m_enabledFx bit 0x20), set by the tint block below
        f32  mfBloomLuminance;
        f32  mfBloomThreshold;
        f32  mafBloomColour[3];
    };
    DiagState gDiag = { false, false, false, false, false, false,
                        0.0f, 0.0f, { 0.0f, 0.0f, 0.0f } };

    // The motion-blur half of the same sampled diagnostic. mfWvpDelta is the largest absolute
    // element difference between MotionBlurState's current and previous world-view-projection
    // matrices: it is ZERO exactly when the reprojection has nothing to say (the pair is still the
    // identity pair MotionBlurState::Construct left, or the camera has not moved), and non-zero the
    // instant a real Update lands a moving camera. That single number is the cheapest proof the
    // consumer chain is alive, because BlurMatrixX/Y/W are built from precisely that difference.
    struct MotionBlurDiagState
    {
        bool mbActive;
        bool mbUpdateCalled;
        s32  miQuality;
        f32  mfCarsBlurAmount;
        f32  mfWorldBlurAmount;
        f32  mfWvpDelta;
    };
    MotionBlurDiagState gMotionBlurDiag = { false, false, 0, 0.0f, 0.0f, 0.0f };

    // Largest absolute element difference between the two WVP matrices, by named lane.
    f32 MaxAbsoluteDifference(const rw::math::vpu::Matrix44& lLhs, const rw::math::vpu::Matrix44& lRhs)
    {
        const rw::math::vpu::Vector4* const lapLhs[4] =
            { &lLhs.xAxis, &lLhs.yAxis, &lLhs.zAxis, &lLhs.wAxis };
        const rw::math::vpu::Vector4* const lapRhs[4] =
            { &lRhs.xAxis, &lRhs.yAxis, &lRhs.zAxis, &lRhs.wAxis };

        f32 lfWorst = KF_ZERO;
        for (u32 luRow = 0; luRow < 4u; ++luRow)
        {
            const f32 lafDelta[4] = { lapLhs[luRow]->x - lapRhs[luRow]->x,
                                      lapLhs[luRow]->y - lapRhs[luRow]->y,
                                      lapLhs[luRow]->z - lapRhs[luRow]->z,
                                      lapLhs[luRow]->w - lapRhs[luRow]->w };
            for (u32 luLane = 0; luLane < 4u; ++luLane)
            {
                const f32 lfAbs = AbsValue(lafDelta[luLane]);
                if (lfAbs > lfWorst)
                    lfWorst = lfAbs;
            }
        }
        return lfWorst;
    }
}

// ==================================================================================================
// Render @0x8240C290-0x8240C314 -- the three bytes read off the layer-0 internal frame once per frame.
//
// The console reloads mapaEffectsFrames[0] and mu8EffectsFrameInternal THREE times here (0x8240C2A0,
// 0x8240C2AC, 0x8240C2C8) and recomputes 496*internal three times, which is register-allocation
// noise: all three address the same frame. One reference is the reconstruction of it.
// ==================================================================================================
void BrnRendererReadPostFxFrameBytes(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                     BrnRendererPostFxFrameBytes* lpOut)
{
    if (lpOut == 0)
        return;

    lpOut->mbMotionBlurActive   = false;
    lpOut->mu8WorldBlurStencil  = 0u;
    lpOut->mu8CarsBlurStencil   = 0u;

    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
    {
        // [FLAG PC bring-up] no arbitrator yet. The zeros above are NOT a chosen floor: they are what
        // a Constructed frame yields -- BrnEffectsFrame::Construct calls mMotionBlurData.Construct(),
        // and MotionBlurData::Construct @0x821F84E8 sets mfCarsBlurAmount = mfWorldBlurAmount = 0.0f
        // and mbIsActive = false -- which is what the console reads on any frame with no motion-blur
        // event posted.
        return;
    }

    const BrnDirector::Camera::MotionBlurData& lrMotionBlur = lpFrame->GetMotionBlurData();

    lpOut->mbMotionBlurActive = lrMotionBlur.mbIsActive;
    lpOut->mu8WorldBlurStencil = static_cast<u8>(
        static_cast<s32>(lrMotionBlur.mfWorldBlurAmount * KF_MOTION_BLUR_STENCIL_SCALE));
    lpOut->mu8CarsBlurStencil = static_cast<u8>(
        static_cast<s32>(lrMotionBlur.mfCarsBlurAmount * KF_MOTION_BLUR_STENCIL_SCALE));
}

// ==================================================================================================
// Render @0x8240C69C-0x8240C798 -- THE COLOUR-CUBE (3D LUT) TINT BLOCK.
//
// It is NOT part of the apply block below, and its position matters: the console runs it EARLY --
// inside PerfMonCpu monitor `*(this + 51508)` (StartMonitor @0x8240C698 / StopMonitor @0x8240C79C),
// immediately BEFORE shadow::Device::ResetShadowing() and the three global texture binds -- because
// BeginTintBlend SCHEDULES AN EA::Jobs JOB that blends the source cubes into the tint volume
// texture while the shadow map and the world passes draw. BrnPostFx::Render then drains it
// (`if (m_processTint) { m_blendJob.WaitOn(); Tint::EndBlendJob(); }`, BrnPostFx.cpp step 1) before
// the composite samples s3. Moving this to the apply block would serialise the job against nothing
// and shorten the window to zero.
//
// THE SHAPE, off the asm (Hex-Rays pseudocode lines 505-533 renders the same block):
//   0x8240C6A8  lbz    r11, 0(this + 0xC41C)         -> mbRenderPostFX, the block's whole gate
//   0x8240C6C0  addi   r5, r1, var_C70               -> the FIVE-entry WEIGHT array
//   0x8240C6C4  addi   r4, r1, var_CA0               -> the FIVE-entry CUBE array
//   0x8240C6C8  mr     r3, r21 (this + 0x480)        -> &mEffectsArbitrator
//   0x8240C6CC  bl     EffectsArbitrator::EvalTint
//   0x8240C6D0..EC                                   -> active = EvalTint() && lbEffectsAllowed
//   0x8240C6F4..0C  ori/rlwinm 0x20 on m_enabledFx   -> BrnPostFx::SetTint(active), INLINED
//   0x8240C710  beq                                  -> if (!active) skip the publish
//   0x8240C718..90  five times:                         if (cube[i]) m_colourCubes[i] = cube[i];
//                                                       m_tintFactors[i] = weight[i];
//   0x8240C794  bl     BrnPostFx::BeginTintBlend
//
// THE NULL GUARD ON THE CUBE STORE IS REAL AND LOAD-BEARING (`cmplwi cr6, r11, 0` / `beq` before
// each of the five `stw`s, and there is no such test before the five weight `stfs`). It exists
// because the slots are PRE-SEEDED with a default cube at prepare time -- see
// PCBringUpSeedTintBlendSources below -- so a layer that contributes no cube this frame leaves the
// default in place rather than nulling a pointer the blend kernels dereference.
//
// IT IS SPELLED ONCE, IN BrnPostFx::SetColourCube (2026-08-16 fix round). The console has exactly
// one test per slot and the five it has are inlined around the five `stw`s INTO m_colourCubes --
// i.e. inside the setter that got inlined, not around the call to it. The first cut wrote the same
// test in both places: behaviour-identical, but it reads as two different rules and invites the next
// reader to "fix" one of them. The guard therefore lives with the store it guards, and the loop
// below calls the setter for every slot, unconditionally, exactly as the console's source did.
//
// THE TWO STACK ARRAYS ARE RAW, exactly as the bloom/vignette out-blocks are: `addi r4, r1,
// var_CA0` with no prior store. EvalTint writes all five entries of both on its true path (4 WORLD
// + 1 FXEVENTS colour cubes, byte_8203E114 = {0,4,1}) and the arrays are read only inside the
// `if (active)` arm, which cannot be entered on its false path.
//
// ---- BRN_POSTFX_TINT3D_AVAILABLE -- THE ONE-LINE KILL SWITCH, AND WHY IT EXISTS ------------------
// E_FX_TINT (0x20) is the ONLY one of the five effect bits that moves the composite's PERMUTATION
// INDEX (BrnPostFxShader.cpp `leShader = 4*blur | 2*dof | tint3d`), so switching it on switches the
// composite from an even permutation to an ODD one -- and every odd permutation SAMPLES the tint
// volume at s3 (`tex3D(Sampler3dTint, composedColour)`). All twelve permutation program pairs are
// adopted on this build (BrnPostFxShader.cpp's "ALL TWELVE PAIRS NOW EXIST" banner), so the draw
// itself is safe; what is NOT this group's to guarantee is what s3 CONTAINS -- the tint volume
// texture, its Lock/Unlock and the seven CPU blend kernels are group `tintrender`'s half
// (rw::graphics::postfx::Tint::Initialize @0x82403B48 / BeginBlendJob @0x823F8310 / the
// TintBlend variant table @0x82F7238C). This half and that half must land TOGETHER: with the bit on
// and no real volume texture behind s3, the grade is whatever that sampler returns. Set this to 0 to
// revert the tint layer to the step-9 behaviour (bit permanently clear, permutation index unmoved)
// without touching anything else. DELETE the switch once tintrender's half is committed and booted.
#define BRN_POSTFX_TINT3D_AVAILABLE 1
// ==================================================================================================
#if BRN_POSTFX_TINT3D_AVAILABLE
namespace
{
    // ---- [FLAG PC bring-up] ------------------------------------------------------------------
    // STANDS IN FOR: BrnEffects::EffectsModule::PrepareResources @0x8229D8A8, case 2 -- the ONE
    // place in the image that seeds BrnPostFx's tint sources. Once the colour-cube dictionary
    // ("PostFx/colourcubedictionary.bin", pool 10) is resident it writes, and writes nothing else
    // into BrnPostFx (asm @0x8229DAAC and the stores around it; pseudocode lines 76-87):
    //     m_numCubesToBlend = 5                              -> SetTintBlendNumber(5)
    //     m_tintFactors[0..4] = 0.2f                         -> SetTintBlendFactor(i, 0.2f)
    //     m_colourCubes[0..4] = <the dictionary's first cube> -> SetColourCube(i, cube)
    //
    // WHY IT HAS TO BE STOOD IN FOR. That TU is not on the build list:
    //     $ grep -n "EffectsModule" tools/build/build_game_exe.bat
    //     294:  rem   driver (EffectsModule::Update / ::GenerateDispatchLists) are BOTH still off this list. ...
    //     300:  rem   DELETE this line when ParticleModule.cpp + EffectsModule.cpp land.
    //     (two COMMENTS, no `echo` line -- the file is not compiled)
    // so on this build nothing ever publishes the blend COUNT, and BrnPostFx::BeginTintBlend
    // @0x823F8380 copies `m_numCubesToBlend` source pointers into the job parameters -- a count of
    // zero makes rw::graphics::postfx::TintBlend @0x82AD4860 index dword_82F7238C[0], which is the
    // table's NULL entry (DATA_DUMP.md "[ 0] ... 0x00000000 (NO FUNC)"). Nothing seeds the five cube
    // slots either, so the FXEVENTS slot -- whose producer (BrnGui::EffectsArbitrator) is also not
    // live -- would hand the kernels a null source pointer to read.
    //
    // WHAT IT WRITES, AND WHERE EACH VALUE COMES FROM. The count is the console's own 5, and it is
    // the same 5 three independent ways (EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT from
    // byte_8203E114, BrnPostFx::m_colourCubes[5], and BeginTintBlend's own
    // `CGS_ASSERT(m_numCubesToBlend <= 5)` at BrnPostFx.cpp:266). The CUBE is NOT invented: it is
    // the first non-null cube THIS FRAME'S EvalTint produced -- a real, loaded
    // rw::graphics::postfx::ColourCube -- copied into the slots the console would have found
    // holding the dictionary default. The weights are NOT seeded: the console's 0.2f x 5 is a
    // prepare-time value the very next Render overwrites with EvalTint's, so it would be dead code.
    //
    // ⚠ A SUBSTITUTED SLOT IS UNOBSERVABLE **ONLY WHEN ITS WEIGHT IS ZERO** -- and since the
    // 2026-08-16 fix round that is the rule the code enforces, not merely the argument for it.
    // (The first cut claimed the substitution was never observable at all. It is not true.)
    //
    // The blend kernels splat each source's weight and multiply (DATA_DUMP.md Blend2Cubes:
    // TintBlendParameters +0x2C / +0x30 -> v127 / v126 -> VMXBlend), so a ZERO-weight source
    // contributes nothing to the output -- but its pixel POINTER is still dereferenced, which is the
    // whole reason such a slot may not be left null. A slot with a NON-ZERO weight is a different
    // animal, and this build can produce one: EnvironmentManager::GenerateEffects @0x827BE698's
    // DEFAULT arm publishes (mpDefTintData, KF_DEF_EFFECTS_LAYER_TINT_WEIGHT = 0.25f) and
    // mpDefTintData is null until Prepare's E_PREPARE_WF_ACQUIRE_DEPENDENCIES acquire lands, so the
    // WORLD layer can arrive here NULL AND WEIGHTED. Substituting there would blend THIS FRAME'S
    // first cube -- the graded 0x8b7e999a "ENV_CC_Paradise_ingame_junk / TINT_Art_Style.psd" every
    // keyframe imports -- at 0.25 into a slot the console would have filled with its own
    // prepare-time default, the NEUTRAL identity cube (Prepare @0x827D49A8 loads
    // "PostFx/colourcubedictionary.bin" into pool 10 and acquires
    // "gamedb://burnout5/Playground/PostFx/ColourCubeDictionary/rgb_colourcube.tga.ImageFile?ID=217407"
    // out of it -- the same bundle PrepareResources reads its default from). That is a visibly
    // over-graded frame. So: substitute into ZERO-WEIGHT slots only, and if any slot is null with a
    // non-zero weight, log it once and return false -- the tint bit stays CLEAR for that frame,
    // which is the one outcome that cannot be mistaken for art direction.
    //
    // Returns false when there is no cube at all this frame; the caller then leaves the tint bit
    // CLEAR rather than scheduling a blend over null sources. That single extra term is the ONLY
    // deviation from the console in this whole block.
    //
    // DELETE-WHEN GameSource/Effects/EffectsModule.cpp is on the build list -- at that point
    // PrepareResources seeds the slots and the count, this function goes, and the caller's
    // `&& lbSeeded` term goes with it.
    bool PCBringUpSeedTintBlendSources(rw::graphics::postfx::ColourCube** lppColourCubes,
                                       const f32* lpafWeights,
                                       u32 luCount)
    {
        rw::graphics::postfx::ColourCube* lpDefault = 0;
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            if (lppColourCubes[luIndex] != 0)
            {
                lpDefault = lppColourCubes[luIndex];
                break;
            }
        }

        if (lpDefault == 0)
        {
            static bool sbReported = false;
            if (!sbReported)
            {
                sbReported = true;
                CgsDev::Log::WriteToLog(
                    "[postfx-tint] no colour cube this frame -- every EvalTint slot is null, so the"
                    " tint bit stays CLEAR. The world layer's cube comes from the environment"
                    " keyframe's +0x80 import (or the manager's default when Prepare has acquired"
                    " one); the fx-events layer's producer (BrnGui::EffectsArbitrator) is not live."
                    " [FLAG PC bring-up: BrnEffects::EffectsModule::PrepareResources @0x8229D8A8"
                    " not on the build list]\n");
            }
            return false;
        }

        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            if (lppColourCubes[luIndex] != 0)
                continue;

            // A null cube with a REAL weight is observable -- refuse the frame instead of grading
            // it with a cube the console would not have used. See the banner above.
            if (lpafWeights[luIndex] != 0.0f)
            {
                static bool sbWeightedNullReported = false;
                if (!sbWeightedNullReported)
                {
                    sbWeightedNullReported = true;
                    char lacMsg[288];
                    std::snprintf(lacMsg, sizeof(lacMsg),
                        "[postfx-tint] slot %u has NO colour cube but weight %.3f -- the console"
                        " would blend its prepare-time NEUTRAL default here, so the tint bit stays"
                        " CLEAR this frame rather than substituting a graded cube."
                        " [FLAG PC bring-up: BrnEffects::EffectsModule::PrepareResources"
                        " @0x8229D8A8 not on the build list]\n",
                        static_cast<unsigned>(luIndex),
                        static_cast<double>(lpafWeights[luIndex]));
                    CgsDev::Log::WriteToLog(lacMsg);
                }
                return false;
            }

            lppColourCubes[luIndex] = lpDefault;
        }

        // SetTintBlendNumber takes `const int&` (DWARF BrnPostFx.h:82), so the value needs a name.
        const int liSourceCount = static_cast<int>(luCount);
        msPostFx.SetTintBlendNumber(liSourceCount);
        return true;
    }

    // The one-shot proof line the step-10 boot check greps for. Printed the first time the tint
    // layer actually schedules a blend.
    //
    // BUILT IN A LOOP OVER luCount (2026-08-16 fix round). The first cut took a count and then
    // hard-indexed [0..4] in the format string and the argument list: correct only while
    // EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT is 5, and that constant is BUILT from the console's
    // byte_8203E114 {0,4,1} rather than written as a literal, so a hard five would over-read the
    // caller's stack arrays the day it moves. The cube's EDGE is in the line now that the tree's
    // ColourCube carries the DWARF's GetSize() -- it is the "edge=N" half of this wave's boot proof.
    void LogTintBlendOnce(rw::graphics::postfx::ColourCube* const* lppColourCubes,
                          const f32* lpfWeights, u32 luCount)
    {
        static bool sbReported = false;
        if (sbReported)
            return;
        sbReported = true;

        char lacMsg[384];
        int liWritten = std::snprintf(lacMsg, sizeof(lacMsg),
                                      "[postfx-tint] tint3d ON: %u sources",
                                      static_cast<unsigned>(luCount));
        size_t luUsed = (liWritten > 0) ? static_cast<size_t>(liWritten) : 0u;
        if (luUsed > sizeof(lacMsg) - 1u)
            luUsed = sizeof(lacMsg) - 1u;

        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
        {
            const rw::graphics::postfx::ColourCube* const lpCube = lppColourCubes[luIndex];
            liWritten = std::snprintf(lacMsg + luUsed, sizeof(lacMsg) - luUsed,
                                      " [%u]=%p edge=%u w=%.3f",
                                      static_cast<unsigned>(luIndex),
                                      static_cast<const void*>(lpCube),
                                      (lpCube != 0) ? static_cast<unsigned>(lpCube->GetSize()) : 0u,
                                      static_cast<double>(lpfWeights[luIndex]));
            if (liWritten <= 0)
                break;
            luUsed += static_cast<size_t>(liWritten);
            if (luUsed > sizeof(lacMsg) - 1u)
            {
                luUsed = sizeof(lacMsg) - 1u;   // snprintf truncated; stop appending
                break;
            }
        }

        // The newline goes on through snprintf as well, so this function performs NO index
        // arithmetic of its own into lacMsg -- every write is bounded by the runtime, and the
        // compiler needs no /GS range check (i.e. no fresh CRT external) to see it.
        std::snprintf(lacMsg + luUsed, sizeof(lacMsg) - luUsed, "\n");
        CgsDev::Log::WriteToLog(lacMsg);
    }
}
#endif  // BRN_POSTFX_TINT3D_AVAILABLE

// ==================================================================================================
// Render @0x8240C69C-0x8240C798 (pseudocode 505..533) -- THE COLOUR-CUBE (3D LUT) TINT BLOCK.
//
// ⚠ CALL IT AT THE CONSOLE'S POSITION, WHICH IS NOT THE APPLY BLOCK'S. The console runs this EARLY
// in Render -- inside the PerfMonCpu bracket at 0x8240C698/0x8240C79C, immediately before
// shadow::Device::ResetShadowing() and the three global texture binds -- so that the blend job runs
// concurrently with the shadow map and the world passes. BrnPostFx::Render drains it (m_processTint
// -> Job::WaitOn -> Tint::EndBlendJob) before the composite samples the tint volume at s3.
//   lbEffectsAllowed == the same v296 the apply block takes.
// ==================================================================================================
void BrnRendererBeginPostFxTintBlend(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                     bool lbEffectsAllowed)
{
#if !BRN_POSTFX_TINT3D_AVAILABLE
    // Kill switch OFF: the step-9 behaviour, spelled out rather than left implicit. m_enabledFx's
    // tint bit is never set, so the composite keeps selecting an EVEN permutation and nothing
    // samples s3. Nothing else in the frame changes.
    (void)lpArbitrator;
    (void)lbEffectsAllowed;
    static bool sbReported = false;
    if (!sbReported)
    {
        sbReported = true;
        CgsDev::Log::WriteToLog(
            "[postfx-tint] tint3d compiled OUT."
            " [FLAG PC bring-up: BRN_POSTFX_TINT3D_AVAILABLE]\n");
    }
#else
    // [FLAG PC bring-up] the arbitrator is Constructed lazily on this build; the console's is a
    // by-value member that always exists. Same seam gate as every other function in this file.
    if (lpArbitrator == 0)
        return;

    // The two raw out-arrays, sized by the arbitrator's own contract (4 WORLD + 1 FXEVENTS).
    rw::graphics::postfx::ColourCube*
        lapColourCubes[BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT];
    f32 lafWeights[BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT];

    // 0x8240C6CC. The return value IS consumed here (unlike the five Eval* in the apply block):
    // `clrlwi r11, r3, 24` / `cmplwi` / `beq` is the first half of the active flag.
    const bool lbEvaluated = lpArbitrator->EvalTint(lapColourCubes, lafWeights);

    bool lbTintActive = lbEvaluated && lbEffectsAllowed;

    // [FLAG PC bring-up] the ONE extra term -- see PCBringUpSeedTintBlendSources' banner. It also
    // publishes the blend COUNT the console publishes at effects-module prepare time.
    if (lbTintActive
        && !PCBringUpSeedTintBlendSources(lapColourCubes, lafWeights,
                                          BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT))
    {
        lbTintActive = false;
    }

    // 0x8240C6F4-0x8240C70C: `ori r11, r11, 0x20` / `rlwinm r11, r11, 0,27,25` on m_enabledFx --
    // BrnPostFx::SetTint(const bool&) inlined. AGENTS.md's inlining-reversal rule puts the call
    // back; the bit is never poked directly. This is the bool that reaches
    // BrnPostFxShader::Render as the composite's TINT3D permutation lane (BrnPostFx::Render
    // step 10, `lbTint = lbEffectsAllowed && (m_enabledFx & E_FX_TINT)`).
    msPostFx.SetTint(lbTintActive);
    gDiag.mbTint3d = lbTintActive;

    if (!lbTintActive)
        return;

    // 0x8240C718-0x8240C790, unrolled five times on the console (the count is a compile-time 5),
    // re-rolled here. The console's cube store is null-GUARDED and its weight store is not -- and
    // that guard lives inside BrnPostFx::SetColourCube, the setter the console inlined around that
    // very `stw` (see the banner). Both calls are therefore unconditional here.
    for (u32 luIndex = 0;
         luIndex < BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT;
         ++luIndex)
    {
        msPostFx.SetColourCube(static_cast<int>(luIndex), lapColourCubes[luIndex]);
        msPostFx.SetTintBlendFactor(static_cast<int>(luIndex), lafWeights[luIndex]);
    }

    LogTintBlendOnce(lapColourCubes, lafWeights,
                     BrnGraphics::EffectsArbitrator::KU_TINT_COLOUR_CUBE_CNT);

    // 0x8240C794. Schedules the EA::Jobs blend of the (cube, weight) sources into the current
    // tint volume texture and sets m_processTint; BrnPostFx::Render drains it.
    msPostFx.BeginTintBlend();
#endif  // BRN_POSTFX_TINT3D_AVAILABLE
}

// ==================================================================================================
// [FLAG PC witness][postfx] the applied-vignette line. ON CHANGE ONLY, opt-in, capped.
//
// WHY IT EXISTS: the composite's last colour op is `rgb = composite * lerp(inner, outer,
// smoothstep(saturate(radius + gradientAdd))) + tint2d` (tools/assets/shaders/brn_postfx_composite.fx
// :366-390), so this pair of colours IS the multiplier over the whole picture. The
// `[postfx-fx] apply-call` sampler above only reports vig=1/0; it cannot tell a neutral vignette
// from one that multiplies the frame by a dark blue. Bug-test lane `postfx` (b5-decomp#4).
// DELETE-WHEN BrnRendererModule::PCBringUpProduceBaseEffectsFrame retires (the real
// BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10 becomes the base-frame producer).
// ==================================================================================================
namespace
{
    bool PostFxDiagEnabled()
    {
        static int siOn = -1;
        if (siOn < 0)
        {
            char lacValue[8] = { 0 };
            siOn = (GetEnvironmentVariableA("BRN_POSTFX_DIAG", lacValue, sizeof(lacValue)) > 0
                    && lacValue[0] != '0') ? 1 : 0;
        }
        return siOn == 1;
    }

    void BrnRendererLogPostFxVignette(bool lbActive, const BrnEffects::VignetteData& lrVignette)
    {
        static u32  suCalls  = 0u;
        static u32  suPrints = 0u;
        const u32   luCall   = suCalls++;
        if (!PostFxDiagEnabled() || suPrints >= 24u)
            return;

        // Change detection on the six values the state carries, quantised to 1/1000 so a float
        // that only jitters in its low bits does not print every frame.
        static bool sbHave   = false;
        static bool sbActive = false;
        static s32  saiLast[10] = { 0 };
        const f32 lafNow[10] = {
            lrVignette.mv4InnerColour.x, lrVignette.mv4InnerColour.y, lrVignette.mv4InnerColour.z,
            lrVignette.mv4OuterColour.x, lrVignette.mv4OuterColour.y, lrVignette.mv4OuterColour.z,
            lrVignette.mv2Amount.x, lrVignette.mv2Amount.y,
            lrVignette.mv2Centre.x, lrVignette.mv2Centre.y
        };
        bool lbChanged = (!sbHave) || (lbActive != sbActive);
        for (int li = 0; li < 10; ++li)
        {
            const s32 liQ = static_cast<s32>(lafNow[li] * 1000.0f);
            if (liQ != saiLast[li])
                lbChanged = true;
            saiLast[li] = liQ;
        }
        sbHave   = true;
        sbActive = lbActive;
        if (!lbChanged)
            return;
        ++suPrints;

        char lacMsg[288];
        std::snprintf(lacMsg, sizeof(lacMsg),
                      "[postfx] vignette apply %u: active=%d inner=(%.4f,%.4f,%.4f)"
                      " outer=(%.4f,%.4f,%.4f) amount=(%.4f,%.4f) centre=(%.4f,%.4f)"
                      " sharp=%.4f angle=%.4f\n",
                      static_cast<unsigned>(luCall), lbActive ? 1 : 0,
                      static_cast<double>(lrVignette.mv4InnerColour.x),
                      static_cast<double>(lrVignette.mv4InnerColour.y),
                      static_cast<double>(lrVignette.mv4InnerColour.z),
                      static_cast<double>(lrVignette.mv4OuterColour.x),
                      static_cast<double>(lrVignette.mv4OuterColour.y),
                      static_cast<double>(lrVignette.mv4OuterColour.z),
                      static_cast<double>(lrVignette.mv2Amount.x),
                      static_cast<double>(lrVignette.mv2Amount.y),
                      static_cast<double>(lrVignette.mv2Centre.x),
                      static_cast<double>(lrVignette.mv2Centre.y),
                      static_cast<double>(lrVignette.mfSharpness),
                      static_cast<double>(lrVignette.mfAngle));
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

// ==================================================================================================
// Render @0x8240D700-0x8240DC50 -- THE APPLY BLOCK.
// ==================================================================================================
void BrnRendererApplyEffectsFrameToPostFx(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                          bool lbEffectsAllowed)
{
    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
        return;   // [FLAG PC bring-up] see GetBaseInternalFrame.

    const BrnEffectsFrame& lrFrame = *lpFrame;

    // ---- BLOOM (asm 0x8240D704-0x8240D7AC) --------------------------------------------------------
    {
        bool lbEvaluated = false;
        // The out-block is a RAW STACK SLOT on the console (`addi r3, r1, var_C70` with no prior
        // store), and it is left raw here too: it is only READ inside the `if (lbBloomActive)` arm,
        // which cannot be entered unless the Eval above ran and filled it. Seeding it with
        // BloomData::Construct() would be behaviour the binary does not have -- and would drag
        // BloomData::kv4DefScale (SharedClasses/Graphics/BrnEffectsData.cpp) into this TU's link.
        BrnEffects::BloomData lBloom;
        if (lrFrame.GetUseBloom())
        {
            (void)lpArbitrator->EvalBloom(lBloom);   // return DISCARDED -- see the banner
            lbEvaluated = true;
        }

        // `ori r9, r11, 2` / `rlwinm r9, r11, 0,31,29` on m_enabledFx (this+0x384) @0x8240D758-
        // 0x8240D76C -- i.e. the console INLINED BrnPostFx::SetBloom(const bool&) here. AGENTS.md's
        // inlining-reversal rule puts the call back; the bit is never poked directly.
        const bool lbBloomActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetBloom(lbBloomActive);

        if (lbBloomActive)
        {
            // The evaluated block's three members land in BrnPostFxBloomData (BrnPostFx's mBloomData,
            // guest this+0x950):
            //   mv4Scale    -> mColour      (`lvx128 v0, r0, &out+0x10` / `stvx128 v0, r20, 0x950`)
            //   mfThreshold -> mfThreshold * gfBloomThresholdScale  (`stfs f0, 0x960(r20)`)
            //   mfLuminance -> mfLuminance * gfBloomLuminanceScale  (`stfs f0, 0x964(r20)`)
            // Note the ORDER the two scalars are read in: threshold (out+4) first, luminance (out+0)
            // second -- the asm loads var_C6C before var_C70.
            BrnPostFxBloomData* const lpBloomData = msPostFx.GetBloom();
            lpBloomData->mColour     = lBloom.mv4Scale;
            lpBloomData->mfThreshold = lBloom.mfThreshold * gfBloomThresholdScale;
            lpBloomData->mfLuminance = lBloom.mfLuminance * gfBloomLuminanceScale;

            gDiag.mfBloomThreshold  = lpBloomData->mfThreshold;
            gDiag.mfBloomLuminance  = lpBloomData->mfLuminance;
            gDiag.mafBloomColour[0] = lBloom.mv4Scale.x;
            gDiag.mafBloomColour[1] = lBloom.mv4Scale.y;
            gDiag.mafBloomColour[2] = lBloom.mv4Scale.z;
        }
        gDiag.mbBloom = lbBloomActive;
    }

    // ---- VIGNETTE (asm 0x8240D7B0-0x8240D8CC) -----------------------------------------------------
    {
        bool lbEvaluated = false;
        BrnEffects::VignetteData lVignette;   // raw stack slot -- see the bloom arm's note
        if (lrFrame.GetUseVignette())
        {
            (void)lpArbitrator->EvalVignette(lVignette);
            lbEvaluated = true;
        }

        const bool lbVignetteActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetVignette(lbVignetteActive);   // `ori r11, r9, 0x10` / `rlwinm r11, r9, 0,28,26`

        if (lbVignetteActive)
        {
            // BrnPostFx's Vignette::State (guest this+0x390, four Vector4s):
            //   m_gradientScalars (+0x390) -- READ-MODIFY-WRITE: the console loads the CURRENT
            //     state vector (`li r11, 0x390` / `lvx128 v11, r20, r11` @0x8240D854-0x8240D85C),
            //     copies it to a stack block, overwrites LANE 0 with mfSharpness (`stfs f0,
            //     var_CC0` @0x8240D870) and LANE 1 with mfAngle * DEG2RAD (`stfs f0, var_CBC`
            //     @0x8240D890), then stores the block back (@0x8240D8BC). Lanes 2 and 3 are
            //     DELIBERATELY PRESERVED -- rw::graphics::postfx::Vignette::SetState reads them, and
            //     zeroing them here would be behaviour the binary does not have.
            //   m_innerColour (+0x3A0) <- mv4InnerColour   (`lvx128 v11, var_BC0` / `stvx128 .. 0x3A0`)
            //   m_outerColour (+0x3B0) <- mv4OuterColour   (`lvx128 v11, var_BB0` / `stvx128 .. 0x3B0`)
            //   m_centerScale (+0x3C0) <- (centre.x, centre.y, amount.x, amount.y), built with four
            //     vrlimi128 inserts: masks 8 and 4 at shift 0 take mv2Centre's lanes 0/1, masks 2 and
            //     1 at shift 2 take mv2Amount's lanes 0/1 (@0x8240D84C-0x8240D86C). All four lanes
            //     are written, so the `lvx128 v0, r20, 0x3C0` that seeds v0 is dead.
            rw::graphics::postfx::Vignette::State* const lpState = msPostFx.GetVignetteState();

            lpState->m_gradientScalars.x = lVignette.mfSharpness;
            lpState->m_gradientScalars.y = lVignette.mfAngle * KF_DEGREES_TO_RADIANS;
            // .z / .w untouched -- see above.

            lpState->m_innerColour = lVignette.mv4InnerColour;
            lpState->m_outerColour = lVignette.mv4OuterColour;

            lpState->m_centerScale.x = lVignette.mv2Centre.x;
            lpState->m_centerScale.y = lVignette.mv2Centre.y;
            lpState->m_centerScale.z = lVignette.mv2Amount.x;
            lpState->m_centerScale.w = lVignette.mv2Amount.y;

            // `lwz r3, 0x5BC(r20)` (m_pfxVignette) / `addi r4, r20, 0x390` (&m_vignetteState) /
            // `bl Vignette__SetState` @0x8240D830-0x8240D8C8. TWO arguments: r5/r6/r7 at the call are
            // Hex-Rays noise -- r5 was last written at 0x8240D7B0 and r6/r7 at 0x8240D6EC/0x8240D678,
            // all BEFORE the intervening `bl sub_823F9DE0`, which clobbers every volatile GPR. The
            // committed declaration agrees: rwgpfxvignette.h `void SetState(const State&)`.
            msPostFx.GetVignette()->SetState(*lpState);

            // [FLAG PC witness][postfx] the vignette the composite actually multiplies the
            // picture by, printed ON CHANGE only (never per frame -- an unbounded per-frame line
            // floods the log). Opt-in behind BRN_POSTFX_DIAG=1 and capped at 24 lines. It sits
            // INSIDE the active arm because lVignette is a raw stack slot outside it (see the
            // bloom arm's note) -- reading it when the Eval did not run would print garbage.
            // DELETE-WHEN the post-fx bring-up producers retire
            // (BrnRendererModule::PCBringUpProduceBaseEffectsFrame).
            BrnRendererLogPostFxVignette(true, lVignette);
        }
        gDiag.mbVignette = lbVignetteActive;
    }

    // ---- DEPTH OF FIELD (asm 0x8240D8D0-0x8240D998) -----------------------------------------------
    {
        bool lbEvaluated = false;
        BrnEffects::DepthOfFieldData lDepthOfField;   // raw stack slot -- see the bloom arm's note
        if (lrFrame.GetUseDepthOfField())
        {
            (void)lpArbitrator->EvalDepthOfField(lDepthOfField);
            lbEvaluated = true;
        }

        const bool lbDofActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetDepthOfField(lbDofActive);   // `ori r11, r11, 1` / `clrrwi r11, r11, 1`

        if (lbDofActive)
        {
            // DepthOfField::State (guest this+0x3D0): the four focal planes come straight off the
            // evaluated block (`stfs` x4 into 0x3D0/0x3D4/0x3D8/0x3DC), the two PROJECTION planes are
            // Render's own constants, and m_dofAmount is the block's fifth float (out+0x10 ->
            // `stfs f0, 0x3E8(r20)`). m_blurRadius (+0x3EC) IS NOT WRITTEN by the console here --
            // reproduced as untouched.
            rw::graphics::postfx::DepthOfField::State* const lpState = msPostFx.GetDofState();
            lpState->m_focalPlanes[0] = lDepthOfField.mfNearPlane;
            lpState->m_focalPlanes[1] = lDepthOfField.mfFocalPlane;
            lpState->m_focalPlanes[2] = lDepthOfField.mfFocalPlane2;
            lpState->m_focalPlanes[3] = lDepthOfField.mfFarPlane;
            lpState->m_projNearPlane  = KF_DOF_PROJECTION_NEAR_PLANE;
            lpState->m_projFarPlane   = KF_DOF_PROJECTION_FAR_PLANE;
            lpState->m_dofAmount      = lDepthOfField.mfDofAmount;

            // `lwz r3, 0x5B8(r20)` (m_pfxDof) / `addi r4, r20, 0x3D0` / `bl DepthOfField__SetState`.
            // [FLAG PC bring-up] null test: m_pfxDof can be null on this build (the effect carves are
            // PC bring-up); the console never tests it. Unreached today (mbUseDepthOfField is false).
            if (msPostFx.GetDepthOfField() != 0)
            {
                msPostFx.GetDepthOfField()->SetState(*lpState);
            }
        }
        gDiag.mbDepthOfField = lbDofActive;
    }

    // ---- B4 BLUR (asm 0x8240D99C-0x8240DC50) ------------------------------------------------------
    //
    // The one VMX-heavy arm. Every vector op here is a splat-multiply on ONE scalar --
    // mAngularVelocity.y, the camera's yaw rate (`lvx128 v0, r9, 0x1C0` then `vspltw v0, v0, 1`
    // @0x8240DA38 / @0x8240DA68; frame +0x1C0 is mAngularVelocity and word element 1 is its .y lane)
    // -- so the reconstruction is scalar arithmetic, not a fake vector library.
    {
        bool lbEvaluated = false;
        BrnEffects::BlurData lBlur;   // raw stack slot -- see the bloom arm's note
        if (lrFrame.GetUseBlur())
        {
            (void)lpArbitrator->EvalBlur(lBlur);
            lbEvaluated = true;
        }

        const bool lbBlurActive = lbEvaluated && lbEffectsAllowed;
        msPostFx.SetB4Blur(lbBlurActive);   // `ori r11, r11, 0x40` / `rlwinm r11, r11, 0,26,24`

        if (lbBlurActive)
        {
            const f32 lfYawRate = lrFrame.GetAngularVelocity().y;

            // The normalised speed factor, clamped at 1.0 (see KF_BLUR_SPEED_MPH_SCALE).
            f32 lfSpeedFactor = lrFrame.GetSpeedMPH() * KF_BLUR_SPEED_MPH_SCALE;
            if (lfSpeedFactor > KF_ONE)
                lfSpeedFactor = KF_ONE;

            // The yaw biases. The centres are pushed by yaw * -0.125 and clamped to [0,1]; the blend
            // AMOUNT's x lane is reduced by |yaw * 0.25| (`vandc` then `vsubfp v0, v12, v0`
            // @0x8240DB48-0x8240DB4C, v12 == splat(mv2BlendAmount.x)).
            const f32 lfCentreBias  = lfYawRate * KF_BLUR_CENTRE_YAW_SCALE;
            const f32 lfAmountBias  = AbsValue(lfYawRate * KF_BLUR_AMOUNT_YAW_SCALE);

            rw::graphics::postfx::B4Blur::State* const lpState = msPostFx.GetB4BlurState();

            // +0x3F0 m_blendAmount = { blendAmount.x - |yaw*0.25|, blendAmount.y, 0, 0 }.
            // Lanes 2/3 are EXPLICITLY zeroed (`std r18, 0(&var_CA8)` @0x8240DBEC, r18 == 0).
            lpState->m_blendAmount.x = lBlur.mv2BlendAmount.x - lfAmountBias;
            lpState->m_blendAmount.y = lBlur.mv2BlendAmount.y;
            lpState->m_blendAmount.z = KF_ZERO;
            lpState->m_blendAmount.w = KF_ZERO;

            // +0x400 m_blurAmount = the evaluated mv2BlurAmount, copied as ONE 16-byte vector
            // (`lvx128 v0, r0, &out+0x30` / `stvx128 v0, r20, 0x400` @0x8240DBC0/@0x8240DBE0) -- i.e.
            // all four lanes, including whatever BlurData::SetToBlend left in lanes 2/3. Reproduced
            // as a whole-vector copy rather than two lanes, because that is what the store does.
            lpState->m_blurAmount = lBlur.mv2BlurAmount;

            // +0x410 m_blendCenter = { clamp01(blendCentre.x + yaw*-0.125), blendCentre.y, 0, 0 }
            // (lanes 2/3 zeroed by `std r18, 0(&var_C38)` @0x8240DBAC).
            lpState->m_blendCenter.x = Clamp01(lBlur.mv2BlendCentre.x + lfCentreBias);
            lpState->m_blendCenter.y = lBlur.mv2BlendCentre.y;
            lpState->m_blendCenter.z = KF_ZERO;
            lpState->m_blendCenter.w = KF_ZERO;

            // +0x420 m_blurCenter = { clamp01(blurCentre.x + yaw*-0.125), blurCentre.y, 0, 0 }
            // (lanes 2/3 zeroed by `std r18, 0(&var_CB8)` @0x8240DBB4).
            lpState->m_blurCenter.x = Clamp01(lBlur.mv2BlurCentre.x + lfCentreBias);
            lpState->m_blurCenter.y = lBlur.mv2BlurCentre.y;
            lpState->m_blurCenter.z = KF_ZERO;
            lpState->m_blurCenter.w = KF_ZERO;

            // +0x430 m_blurOpacity  <- mfOpacity                       (`stfs f0, 0x430(r20)`)
            // +0x434 m_blurVelocity <- mfVelocity * speedFactor^2      (`fmuls f13, f0, f12` then
            //                          `fmuls f0, f13, f12` @0x8240DBB8 / @0x8240DBF8 -- the factor
            //                          is applied TWICE, which is the asm and not a typo)
            lpState->m_blurOpacity  = lBlur.mfOpacity;
            lpState->m_blurVelocity = lBlur.mfVelocity * lfSpeedFactor * lfSpeedFactor;

            // +0x438/+0x43C m_blendSharpMUL / m_blendSharpADD, derived from the [-1,1] sharpness.
            // Called HERE, between the velocity store and the noise store, exactly as the asm orders
            // it (`bl B4Blur__State__SetBlendSharpness` @0x8240DC28, with r3 = this+0x3F0 and
            // f1 = mfSharpness loaded @0x8240DBF4).
            lpState->SetBlendSharpness(lBlur.mfSharpness);

            // +0x440 m_blendNoise <- mfNoise                            (`stfs f0, 0x440(r20)`)
            // +0x444 m_blendAngle <- mfAngle * DEG2RAD                   (`stfs f0, 0x444(r20)`)
            lpState->m_blendNoise = lBlur.mfNoise;
            lpState->m_blendAngle = lBlur.mfAngle * KF_DEGREES_TO_RADIANS;

            // The console finishes the arm with
            //     memcpy(*(this + 0x5C8), this + 0x3F0, 0x60)      (asm 0x8240DC30-0x8240DC50)
            // i.e. it copies the 0x60-byte State it just built straight over B4Blur::m_state, that
            // class's FIRST member (rwgpfxb4blur.h, `State m_state; // +0x00`) -- an assignment, spelled
            // as one through B4Blur::SetState (rwgpfxb4blur.{h,cpp}, landed with this wave) so nothing
            // pokes a private member. [FLAG PC bring-up] the null test: m_pfxB4Blur can be null on this
            // build (BrnPostFx::Construct's effect carves are PC bring-up); the console never tests it.
            if (msPostFx.GetB4Blur() != 0)
            {
                msPostFx.GetB4Blur()->SetState(*lpState);
            }
            //
            // INERT ON THIS BUILD, and that is measured rather than hoped: the B4-blur arm only runs
            // when the layer-0 internal frame's mbUseBlur is set, and the PC bring-up producer of that
            // frame (BrnRendererModule::PCBringUpProduceBaseEffectsFrame) writes mbUseBlur = false,
            // because the console's own producer derives it from a camera flag and two debug-component
            // bytes that are all clear on a no-event frame (DATA_NOTE.md section 3). So this whole
            // block is reconstructed-and-unreached today; it must not be reached until the publish
            // above exists.
        }
        gDiag.mbBlur = lbBlurActive;
    }
}

// ==================================================================================================
// Render @0x8240DC80-0x8240DCBC -- the 2D tint colour.
// ==================================================================================================
void BrnRendererEvalPostFxTint2dColour(const BrnGraphics::EffectsArbitrator* lpArbitrator,
                                       bool lbEffectsAllowed,
                                       f32* lpafColourXYZW)
{
    if (lpafColourXYZW == 0)
        return;

    // The unconditional zero first (`vspltisw v0, 0` / `stvx128 v0, r0, &var_CA0`). This is why the
    // composite's tint is a pass-through on a no-event frame -- it is the console's own value, not a
    // fallback.
    lpafColourXYZW[0] = KF_ZERO;
    lpafColourXYZW[1] = KF_ZERO;
    lpafColourXYZW[2] = KF_ZERO;
    lpafColourXYZW[3] = KF_ZERO;

    gDiag.mbTint2d = false;

    // NOTE THE ORDER OF THE TWO CONDITIONS: the console tests `v296` (effects allowed) FIRST
    // (`cmplwi cr6, r25, 0` / `beq` @0x8240DC88-0x8240DC90) and only then loads the frame byte. Every
    // OTHER effect above tests the frame byte first and ANDs the allowed flag afterwards. Preserved.
    if (!lbEffectsAllowed)
        return;

    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
        return;   // [FLAG PC bring-up] see GetBaseInternalFrame.

    if (!lpFrame->GetUseTint2d())
        return;

    // The console evaluates INTO the slot it just zeroed (r3 == &var_CA0 at 0x8240DCB8 is the same
    // address the `stvx128 v0` at 0x8240DC8C wrote), so the zero is the seed, not a separate value.
    BrnEffects::TintData2d lTint2d;
    lTint2d.mv4Colour.SetZero();
    (void)lpArbitrator->EvalTint2d(lTint2d);   // return DISCARDED, as at the four sites above

    lpafColourXYZW[0] = lTint2d.mv4Colour.x;
    lpafColourXYZW[1] = lTint2d.mv4Colour.y;
    lpafColourXYZW[2] = lTint2d.mv4Colour.z;
    lpafColourXYZW[3] = lTint2d.mv4Colour.w;

    gDiag.mbTint2d = true;
}

// ==================================================================================================
// Render @0x8240DD04-0x8240DD4C -- MotionBlurState::Update.
//
// THE CALL, decoded off the asm rather than off Hex-Rays (which drops the fifth argument):
//     r3 = this + 0x450                     -> BrnPostFx::mMotionBlurState
//     r4 = particleRenderData + 0x60        (`addi r4, r24, 0x60`  @0x8240DD0C)
//     r5 = particleRenderData + 0xA0        (`addi r5, r24, 0xA0`  @0x8240DD08)
//     f1 = *(f32*)(particleRenderData + 0xC)(`lfs f1, 0xC(r24)`    @0x8240DD04)
//     r7 = (frame.mMotionBlurData.mbIsExpensiveMotionBlur != 0)
//          (`lbz r11, 0x1E1(r11)` / `cntlzw` / `extrwi r11,r11,1,26` / `xori r7, r11, 1`
//           @0x8240DD38-0x8240DD48 -- a byte-to-bool with the sense INVERTED TWICE, i.e. plain != 0)
// The float rides f1 and CONSUMES its GPR slot, which is why the fifth argument lands in r7 and not
// r6 -- the PPC float-argument rule AGENTS.md rule 4 names. Hex-Rays prints only four arguments and
// is wrong; the committed declaration (BrnPostFxShader.h:108) has five, ending in
// `MotionBlurState::EQuality leQuality`, and 0/1 are E_QUALITY_CHEAP / E_QUALITY_EXPENSIVE.
//
// r24 == the value BrnGame::DispatchThreadInputBuffer's read-locked ParticleRenderData accessor
// returns (`bl sub_8227F640` @0x8240C45C; that function asserts "Not locked for reading" and returns
// `a1 + 14496` == this + 0x38A0 == &mParticleRenderData, BrnDispatchThreadInputBuffer.h:182).
//
// ⭐ BOTH BLOCKERS CLOSED 2026-08-15 (post-fx step-6 producers wave), and the gate is now 1:
//   1. MotionBlurState::Update IS BODIED, in BrnPostFxShader.cpp, where the DWARF puts it. (The
//      old note here said it needed the rw::math::fpu double-precision matrix family. That was
//      wrong: Update is pure single-precision VMX with no call in it at all. The fpu double family
//      is what BrnPostFxShader::Render's REPROJECTION block needs, and that now exists too.)
//   2. DispatchThreadInputBuffer::ParticleRenderData HAS ITS REAL LAYOUT (DWARF ParticleModule.h:
//      589-606), so the three arguments are reached BY NAME -- mfCurrentTimeStep, mCgsCamera's view
//      and mCgsCamera's projection -- and the console's +0x0C / +0x60 / +0xA0 are three independent
//      confirmations of that layout rather than three offsets anybody has to cast to.
//
// ⭐ THE THIRD BLOCKER -- THE PRODUCER -- CLOSED 2026-08-16 (post-fx step 9). On the console the
// render data is filled every frame by BrnParticle::ParticleModule::GenerateRenderRequests
// @0x82281BD8 -- the ONLY caller of the write-locked accessor:
//   $ python -c "import json;print(json.load(open('.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8227F6E8.json'))['xrefs_to'])"
//   [{'address': '0x82281BD8', 'name': 'BrnParticle::ParticleModule::GenerateRenderRequests'}]
// which in turn is only reached from BrnEffects::EffectsModule::GenerateDispatchLists @0x82296668,
// after BrnParticle::ParticleModule::Update @0x822817D8 (the vtable+68 virtual EffectsModule::Update
// @0x8229EC28 drives) has refreshed the module's own record at ParticleModule+0x8E00. NEITHER module
// is on this build's list -- `grep -n "Particle" tools/build/build_game_exe.bat` finds only the LION
// vendor waveform TU -- so the record was never written and this function was handed a NULL.
//
// It is now written by the named PC bring-up stand-in for that pair,
//     BrnParticle::PCBringUpProduceParticleRenderData  (ParticleModuleBringUp.cpp)
// called from BrnGameModule::DoDispatch with the DIRECTOR's published camera and the SIM timer's
// step / time / multiplier -- the same three floats the console's EffectsModule::Update reads off
// the published TimerStatusInterface. BrnRendererModule.cpp now passes the READ-LOCKED
// `GetPublishedParticleRenderDataPC()`, gated on that producer having run.
//
// ⚠ THE OLD WARNING IS KEPT AS HISTORY, BECAUSE THE HAZARD IT NAMES IS STILL REAL: DO NOT WIRE THE
// ACCESSOR UP WITHOUT A PRODUCER. Passing `GetPublishedParticleRenderDataPC()`
// unconditionally would hand this function UNINITIALISED memory before the first producer run:
// neither DispatchThreadInputBuffer::Construct (faithfully -- the console does not clear that
// payload either) nor CreateIOBuffer<T> (since the 2026-08-15 perf wave) zeroes it. A garbage view
// matrix is almost surely singular or non-finite, and MotionBlurState::Update's zero-time-step arm
// INVERTS it four times -- the resulting NaN would propagate straight into BlurMatrixX/Y/W and, from
// there, into a tex2Dgrad gradient. That is exactly why the call site tests
// BrnParticle::PCBringUpParticleRenderDataProduced() and not just the buffer pointer. The null arm
// below stays for the frames before the first DoDispatch, and for any build where the producer is
// unmounted.
//
// ⚠ STENCIL-MASK DEVIATION, LIVE FROM THIS CHANGE ON. The console's composite blurs CARS and WORLD
// separately behind the stencil mask BrnPostFx writes; this tree's composite has no stencil mask, so
// a real (non-zero) velocity now blurs the WHOLE frame uniformly instead of only what the mask
// selects. That is a known, deliberate deviation -- the fix is the stencil mask, not a weakened
// velocity. Until it lands, expect directional streaking of the world (and of the car with it) in
// the intro / menu cameras, which is precisely the boot proof this rung is checked against.
//
// THE PARAMETER IS TYPED: BrnParticle::ParticleModule::ParticleRenderData. Passing a null pointer
// -- which Render still does when nothing in this tree PRODUCES the render data -- is the honest
// "no producer" signal, handled by the body with one reported line and not a crash.
// ==================================================================================================
#define BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE 1

bool BrnRendererUpdatePostFxMotionBlur(
    const BrnGraphics::EffectsArbitrator* lpArbitrator,
    const BrnParticle::ParticleModule::ParticleRenderData* lpParticleRenderData)
{
    gMotionBlurDiag.mbUpdateCalled = false;

    const BrnEffectsFrame* const lpFrame = GetBaseInternalFrame(lpArbitrator);
    if (lpFrame == 0)
        return false;   // [FLAG PC bring-up] see GetBaseInternalFrame.

    // r7 -- `lbz r11, 0x1E1(r11)` / cntlzw / extrwi / xori @0x8240DD38-0x8240DD48, i.e. plain != 0.
    const BrnDirector::Camera::MotionBlurData& lrMotionBlur = lpFrame->GetMotionBlurData();
    const MotionBlurState::EQuality leQuality =
        lrMotionBlur.mbIsExpensiveMotionBlur ? MotionBlurState::E_QUALITY_EXPENSIVE
                                             : MotionBlurState::E_QUALITY_CHEAP;

    gMotionBlurDiag.mbActive          = lrMotionBlur.mbIsActive;
    gMotionBlurDiag.miQuality         = static_cast<s32>(leQuality);
    gMotionBlurDiag.mfCarsBlurAmount  = lrMotionBlur.mfCarsBlurAmount;
    gMotionBlurDiag.mfWorldBlurAmount = lrMotionBlur.mfWorldBlurAmount;

#if BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE
    if (lpParticleRenderData == 0)
    {
        // [FLAG PC bring-up] the record has not been stamped YET -- see the banner. Since post-fx
        // step 9 this is no longer "there is no producer at all": the producer is
        // BrnParticle::PCBringUpProduceParticleRenderData (ParticleModuleBringUp.cpp), driven from
        // BrnGameModule::DoDispatch, and the call site here gates on
        // BrnParticle::PCBringUpParticleRenderDataProduced(). So this arm now means one of exactly
        // three things: (a) the frames before the first DoDispatch (boot / loading, where DoDispatch
        // is only reached in the IN_GAME flow state), (b) the director output buffer was null so the
        // producer returned early, or (c) ParticleModuleBringUp.cpp is not on the build list.
        // DELETE WHEN the real BrnParticle::ParticleModule + BrnEffects::EffectsModule fill
        // DispatchThreadInputBuffer::mParticleRenderData (then the record is always written before
        // any reader, as on the console, and no latch is needed).
        static bool sbReportedNoRenderData = false;
        if (!sbReportedNoRenderData)
        {
            sbReportedNoRenderData = true;
            CgsDev::Log::WriteToLog(
                "[postfx-mb] MotionBlurState::Update NOT called: no ParticleRenderData has been"
                " stamped yet (the console's producer is BrnParticle::ParticleModule::"
                "GenerateRenderRequests @0x82281BD8, not reconstructed; the PC stand-in is"
                " BrnParticle::PCBringUpProduceParticleRenderData, driven from"
                " BrnGameModule::DoDispatch). The WVP pair stays at Construct's identity/identity,"
                " so the composite's reprojection rows are exactly zero -- correct and finite, just"
                " motionless. [FLAG PC bring-up: ParticleRenderData not produced yet]\n");
        }
        return false;
    }

    const CgsGraphics::Camera& lrCamera = lpParticleRenderData->mCgsCamera;

    // [FLAG PC type bridge] The DWARF types the camera's view member `Matrix44Affine`
    // (dwarfdump/GameShared/GameClasses/Graphics/CgsCamera.h:202) and MotionBlurState::Update's
    // first parameter is a Matrix44Affine; the committed CgsGraphics::Camera models the identical
    // four 16-byte rows as a Matrix44 (mView @+0x00, pinned by that header's _AssertLayout). Same
    // bytes, different SDK spelling. Copied lane by lane rather than reinterpret_cast so nothing
    // silently depends on the two aggregates keeping the same host layout. DELETE-WHEN
    // CgsGraphics::Camera adopts the DWARF's Matrix44Affine for its view member.
    rw::math::vpu::Matrix44Affine lCameraView;
    lCameraView.xAxis = rw::math::vpu::Vector3{ lrCamera.mView.xAxis.x, lrCamera.mView.xAxis.y,
                                                lrCamera.mView.xAxis.z, lrCamera.mView.xAxis.w };
    lCameraView.yAxis = rw::math::vpu::Vector3{ lrCamera.mView.yAxis.x, lrCamera.mView.yAxis.y,
                                                lrCamera.mView.yAxis.z, lrCamera.mView.yAxis.w };
    lCameraView.zAxis = rw::math::vpu::Vector3{ lrCamera.mView.zAxis.x, lrCamera.mView.zAxis.y,
                                                lrCamera.mView.zAxis.z, lrCamera.mView.zAxis.w };
    lCameraView.wAxis = rw::math::vpu::Vector3{ lrCamera.mView.wAxis.x, lrCamera.mView.wAxis.y,
                                                lrCamera.mView.wAxis.z, lrCamera.mView.wAxis.w };

    // The console's five-argument call, argument for argument (asm 0x8240DD04-0x8240DD4C):
    //   r3 = &BrnPostFx::mMotionBlurState        r4 = particleRenderData + 0x60  (the view)
    //   r5 = particleRenderData + 0xA0 (proj)    f1 = *(f32*)(particleRenderData + 0x0C)
    //   r7 = the quality (the float in f1 CONSUMES its GPR slot -- AGENTS.md's PPC rule 4 -- which
    //        is why the fifth argument lands in r7 and Hex-Rays prints only four).
    msPostFx.GetMotionBlurState().Update(lCameraView,
                                         lrCamera.mProjection,
                                         lpParticleRenderData->mfCurrentTimeStep,
                                         leQuality);
    gMotionBlurDiag.mbUpdateCalled = true;
    gMotionBlurDiag.mfWvpDelta = MaxAbsoluteDifference(msPostFx.GetMotionBlurState().mCurrentWVP,
                                                       msPostFx.GetMotionBlurState().mPreviousWVP);
    return true;
#else
    (void)lpParticleRenderData;
    static bool sbReported = false;
    if (!sbReported)
    {
        sbReported = true;
        CgsDev::Log::WriteToLog(
            "[postfx-mb] MotionBlurState::Update compiled OUT."
            " [FLAG PC bring-up: BRN_POSTFX_MOTION_BLUR_UPDATE_AVAILABLE]\n");
    }
    return false;
#endif
}

// ==================================================================================================
// [FLAG PC bring-up diagnostic] the one line that proves the chain.
//
// LATCHED ON A COUNTER, capped at six lines: base frame -> Eval* -> BrnPostFx is a per-frame steady
// state, so a value latch would print once and a per-frame line would flood the log. Six samples 500
// frames apart cover the loading screen, the menu and the first in-world frames.
// DELETE with the bring-up.
// ==================================================================================================
void BrnRendererLogPostFxEffectState()
{
    static u32 suCalls  = 0u;
    static u32 suPrints = 0u;
    const u32 luFrame = suCalls++;
    // FLAG PC-platform leaf: report actual consumer flag changes while testing the
    // debug UI, even after the normal six startup samples have been exhausted.
    const u32 luFlags = (gDiag.mbBloom ? 1u : 0u) | (gDiag.mbVignette ? 2u : 0u)
        | (gDiag.mbDepthOfField ? 4u : 0u) | (gDiag.mbBlur ? 8u : 0u)
        | (gDiag.mbTint2d ? 16u : 0u) | (gDiag.mbTint3d ? 32u : 0u);
    static u32 suLastFlags = ~0u;
    char lacTrace[2];
    const bool lbDebugChange = GetEnvironmentVariableA("BRN_DEBUG_UI_TRACE", lacTrace, sizeof(lacTrace)) != 0
        && luFlags != suLastFlags;
    suLastFlags = luFlags;
    if (!lbDebugChange && ((luFrame % 500u) != 0u || suPrints >= 6u))
        return;
    ++suPrints;

    char lacMsg[224];
    std::snprintf(lacMsg, sizeof(lacMsg),
                  "[postfx-fx] apply-call %u: bloom=%d(lum %.3f thr %.3f col %.3f %.3f %.3f)"
                  " vig=%d dof=%d blur=%d tint2d=%d tint3d=%d\n",
                  static_cast<unsigned>(luFrame),
                  gDiag.mbBloom ? 1 : 0,
                  static_cast<double>(gDiag.mfBloomLuminance),
                  static_cast<double>(gDiag.mfBloomThreshold),
                  static_cast<double>(gDiag.mafBloomColour[0]),
                  static_cast<double>(gDiag.mafBloomColour[1]),
                  static_cast<double>(gDiag.mafBloomColour[2]),
                  gDiag.mbVignette ? 1 : 0,
                  gDiag.mbDepthOfField ? 1 : 0,
                  gDiag.mbBlur ? 1 : 0,
                  gDiag.mbTint2d ? 1 : 0,
                  gDiag.mbTint3d ? 1 : 0);
    CgsDev::Log::WriteToLog(lacMsg);

    // The motion-blur consumer line. `updated` is 1 only when the console's five-argument
    // MotionBlurState::Update actually ran; `wvpDelta` is the largest element difference between
    // the current and previous world-view-projection matrices, which is what the composite's
    // reprojection differences into BlurMatrixX/Y/W. With no producer wired, the honest reading is
    // active=0 updated=0 wvpDelta=0.000 -- and any NON-zero wvpDelta with updated=0 would be a bug.
    char lacMotionBlurMsg[192];
    std::snprintf(lacMotionBlurMsg, sizeof(lacMotionBlurMsg),
                  "[postfx-mb] apply-call %u: active=%d updated=%d quality=%d cars=%.2f world=%.2f"
                  " wvpDelta=%.4f\n",
                  static_cast<unsigned>(luFrame),
                  gMotionBlurDiag.mbActive ? 1 : 0,
                  gMotionBlurDiag.mbUpdateCalled ? 1 : 0,
                  static_cast<int>(gMotionBlurDiag.miQuality),
                  static_cast<double>(gMotionBlurDiag.mfCarsBlurAmount),
                  static_cast<double>(gMotionBlurDiag.mfWorldBlurAmount),
                  static_cast<double>(gMotionBlurDiag.mfWvpDelta));
    CgsDev::Log::WriteToLog(lacMotionBlurMsg);
}

// The world/car/sky pass block of Render (@0x8240BFA8 :725+). Pass order and list ids are the
// X360's (see the renderer wave log for the full map):
//   shadow cascades (lists 0,2,1,3,4)  -> LIVE, and no longer here: they run in
//     RenderShadowMapPasses above, called from Render BEFORE this block, which is the
//     console's own order (Render:545-640 vs. the world passes at :725+)
//   env-map faces (lists 5..10)        -> LIVE (Render's six-face loop, reflections step 1;
//                                          runs BEFORE this block, after the shadow pass)
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
    const u32 luPreZ             = GetMeshFrameForReadPC().GetList(21)->GetCount();
    const u32 luCarOpaque        = GetMeshFrameForReadPC().GetList(19)->GetCount();
    const u32 luWorldOpaque      = GetMeshFrameForReadPC().GetList(11)->GetCount();
    const u32 luWorldTransparent = GetMeshFrameForReadPC().GetList(15)->GetCount();
    const u32 luCarTransparent   = GetMeshFrameForReadPC().GetList(20)->GetCount();
    renderengine::FrameProfile::SceneLists(luPreZ, luWorldOpaque, luCarOpaque);
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
                luTotal += GetMeshFrameForReadPC().GetList(luList)->GetCount();
            if (luTotal != 0)
            {
                sbLoggedLists = true;
                suLastCarOpaque = luCarOpaque;
                *CgsDev::Log::gpDebugPrint << "[FLAG PC bring-up] MESH lists:";
                for (u32 luList = 0; luList < 25u; ++luList)
                {
                    const u32 luCount = GetMeshFrameForReadPC().GetList(luList)->GetCount();
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
        WaitForMeshSortPC(21u);
        GetMeshFrameForReadPC().GetList(21)->DispatchAllMeshesZOnly(mpInterpreter, &lContext);
    }

    // ==============================================================================================
    // THE CARS-vs-WORLD MOTION-BLUR MASK -- the CAR half (X360 Render @0x8240CEC4-0x8240CF90 and
    // @0x8240D338-0x8240D3FC).
    //
    // WHAT THE CONSOLE'S MASK IS, end to end, decoded off Render's own asm:
    //   * the layer-0 internal BrnEffectsFrame's mMotionBlurData is quantised to three stack bytes
    //     at @0x8240C2C0-0x8240C314 -- mbIsActive, (u8)(mfWorldBlurAmount*255) and
    //     (u8)(mfCarsBlurAmount*255) (BrnRendererPostFxFrameBytes, already read in Render);
    //   * the WORLD amount is the frame's STENCIL CLEAR value: BeginRenderAntiAliased passes it to
    //     D3DDevice_BeginTiling's ClearStencil (`clrlwi r9, r27, 24` @0x823FFB44) and ResolveMSAA
    //     passes the same byte to D3DDevice_EndTiling's ClearStencil (`mr r9, r26` @0x823FFD44).
    //     Both are already reconstructed and already carry the byte;
    //   * the CARS amount is stamped into the stencil BY THE CAR PASSES THEMSELVES, which is this
    //     block: `shadow::Device::BeginForceStencilWrite(carsByte)` before the car mesh list and
    //     EndForceStencilWrite after it. The force window makes
    //     shadow::Device::Xbox2SetDepthStencilStateLowLevelShadowed (X360 sub_827E8150) override
    //     every material's depth/stencil third with StencilEnable 1 / TwoSided 0 / Func ALWAYS(7) /
    //     WriteMask -1 / Ref = the forced value / Pass REPLACE(2) / ZFail KEEP(0). That override is
    //     ALREADY IMPLEMENTED on PC (shadowingdevice.cpp:747-754), and the car meshes reach it
    //     through Device::SetMaterialState -- so the window works here exactly as it does there;
    //   * the composite then reads the stencil out of the depth fetch's fourth lane and multiplies
    //     the screen velocity by it, giving cars and world DIFFERENT blur amounts per pixel.
    // ⚠ THE LAST STEP IS THE ONE THIS BUILD STILL CANNOT DO: D3D9 cannot SAMPLE a depth-stencil
    // surface's stencil, so PostFxProgramsPC.cpp's composite uses 1.0 for the mask (its own
    // "TWO DISCLOSED PC DEVIATIONS" banner says so). This block therefore makes the mask CORRECT IN
    // THE BUFFER while the consumer is still flat -- see the drive-fx wave report for the two
    // candidate PC consumers. It is written now because it is a literal reconstruction of two
    // console windows, because it is free (the whole thing is gated OFF unless a camera actually
    // requested motion blur), and because the alternative -- a consumer with nothing to consume --
    // cannot be brought up in the other order.
    //
    // THE GATE IS mMotionBlurData.mbIsActive, NOT the render-cars flag: `lbz r25, var_CD0` /
    // `beq` @0x8240CEC8 skips only the Begin, and the symmetric `cmplwi r25` / `beq` @0x8240CF84
    // skips only the End -- so the pair is balanced on ONE bool, which is what
    // BeginForceStencilWrite's `!mbForceStencilWrite` assert and EndForceStencilWrite's
    // `mbForceStencilWrite` assert require.
    //
    // POSITION OF THE READ: Render reads the three bytes once, right after SortDispatchLists, and
    // carries them in stack slots to four consumers. They are read again here, for the same reason
    // Render's own copy documents -- nothing writes the layer-0 INTERNAL frame between those two
    // points (its only writers are StartOfFrame / DoDispatch in the UPDATE frame and
    // EffectsArbitrator::EndOfFrame in SwapBuffers, all outside Render).
    // ==============================================================================================
    BrnRendererPostFxFrameBytes lCarMaskBytes;
    BrnRendererReadPostFxFrameBytes(
        sbEffectsArbitratorConstructed ? &mEffectsArbitrator : 0, &lCarMaskBytes);
    const bool lbForceCarStencil = lCarMaskBytes.mbMotionBlurActive;

    if (lbOpaqueWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(false);

        if (mbRenderCarsOpaque)
        {
            // X360 @0x8240CEC4-0x8240CED4 / @0x8240CF84-0x8240CF8C: the Begin/End pair sits INSIDE
            // the render-cars-opaque gate (the `beq cr6, loc_8240CF94` at 0x8240CEC0 jumps past all
            // three), and the dispatch of mesh list 0x13 sits between them.
            if (lbForceCarStencil)
                shadow::Device::BeginForceStencilWrite(lCarMaskBytes.mu8CarsBlurStencil);

            WaitForMeshSortPC(19u);
            GetMeshFrameForReadPC().GetList(19)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);

            if (lbForceCarStencil)
                shadow::Device::EndForceStencilWrite();
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
            WaitForMeshSortPC(11u);
            GetMeshFrameForReadPC().GetList(11)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
            renderengine::ShadowProbe_End(3u);
        }
    }

    CgsDev::Assert::PollFrameProbePC(1);

    // ---- THE SKY (X360 Render @0x8240BFA8: BrnSkyDomeManager::Render right here, between
    // the opaque and the transparent passes, gated on mbRenderSky). --------------------
    // The dome is camera-centred and 9500 units across, so it is drawn AFTER the opaque
    // geometry and depth-tests against it (its depth/stencil state writes no depth) --
    // it fills only the pixels the city left empty.
    //
    // The joined publication copied the world frame into the internal slot.
    // Sky, reflections, coronas and post-FX consume that same immutable frame.
    if (mbRenderSky && (lbPreZWork || lbOpaqueWork || lbTransparentWork)
        && maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameInternal]
        && EnsureSkyDomeBringUp())

    {
        const BrnShaderConstantsFrame& lrFrame =
            maShaderConstantsFrames[mu8ShaderConstantsFrameInternal];
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
            WaitForMeshSortPC(15u);
            GetMeshFrameForReadPC().GetList(15)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
        // X360 @0x8240D338-0x8240D348 / @0x8240D3F4-0x8240D3FC: the car-TRANSPARENT window. The
        // whole window sits INSIDE the mbRenderCarsTransparent gate: `lbzx r11, r31, 0xC417` /
        // `cmplwi cr6, r11, 0` / `beq cr6, loc_8240D400` @0x8240D2F4-0x8240D304, and loc_8240D400 is
        // PAST both the Begin (0x8240D338) and the End (0x8240D3FC). 0xC417 is
        // mbRenderCarsTransparent (the five sibling flags 0xC413..0xC417 pin the committed member
        // order in BrnRendererModule.h). What sits between Begin and End is the occlusion-query arm
        // (0x8240D36C) or the plain arm (0x8240D3D0), both of which dispatch mesh list 0x14 -- the
        // same shape as the opaque half above. (Step-10 verify finding: the first reading had the
        // pair outside the gate.)
        if (mbRenderCarsTransparent)
        {
            if (lbForceCarStencil)
                shadow::Device::BeginForceStencilWrite(lCarMaskBytes.mu8CarsBlurStencil);

            WaitForMeshSortPC(20u);
            GetMeshFrameForReadPC().GetList(20)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);

            if (lbForceCarStencil)
                shadow::Device::EndForceStencilWrite();
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
    // The corona atlas' slot is the CoronaManager's, not one of this module's members -- the X360
    // hands it straight on (`mCoronaManager.SetTextureAtlas(allocator, lpCoronaAtlas)`). On this
    // build the manager is not Constructed yet when PrepareAgain runs (there is no
    // BrnRendererModule::Prepare and no device this early), so the texture is LATCHED and
    // EnsureCoronaManagerBringUp hands it on at the first frame that can accept it. See that
    // function's banner. (Until coronas step 1 this argument was accepted and dropped.)
    gpCoronaAtlasTexture = lpCoronaAtlas;
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

// The per-frame sky/cloud constants -- SINCE THIS WAVE, A COPY OF THE LIVE WORLD FRAME.
//
// ---- WHY THIS IS A COPY AND NOT A PRODUCER (X360, proven from the asm) ----------------
// FLAG PC-platform leaf: the world still fills its native bridge record rather
// than the RendererIO pointer. Copy it only from SwapBuffers, while update and
// dispatch are joined. ARTIST Update 0x824060C8 publishes the external slot to
// WorldModule; SwapBuffers makes it internal, and Render 0x8240D0FC reads that
// internal slot. The native consumer now follows the same immutable-frame rule.
// Keep the renderer framing and all six reflection matrices in the same copy.


// @ 0x8240BFA8 - BrnRendererModule::Render. Reconstructed from the X360 ARTIST build.
//
// The full Render walks the whole frame (shadow maps, env map, world/car opaque +
// transparent, sky, coronas, particles, post-fx, MSAA resolve) and finishes with the
// loading-screen overlay and the present. During boot none of the world systems have
// data, so those passes are data-gated off; Option B reconstructs the part that actually
// runs - frame begin, the loading-screen foreground overlay, and the present. The gameplay
// passes are reconstructed incrementally as their subsystems come online.
void BrnRendererModule::PrepareDisplayPC()
{
    if (EnsureShadowMapTarget(mAllocatedRenderTargets))
        mAllocatedRenderTargets.PCResizeDisplay();
}



void BrnRendererModule::Render(BrnEffects::EffectsModule* lpEffectsModule,
                               const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer)
{
    // Finish unused/disabled-pass sorts as well, before this frame returns to
    // resource publication or shutdown. Individual passes join before reading.
    struct SortCompletionPC { renderengine::DispatchSortJobsPC& mJobs;
        ~SortCompletionPC() { mJobs.WaitAll(); } };
    SortCompletionPC lSortCompletion{
        maPreparedMeshFramesPC[mpMeshProducerInterpreterPC ? muMeshReadFramePC : 0].mSortJobs};

    using namespace renderengine::FrameProfile;
    Stage lRenderStage(RENDER_SETUP);
    // FLAG PC-platform leaf: missing host storage cannot produce a valid frame.
    // This also covers failure to create the allocator before Prepare is called.
    if (mbDispatchStorageFailedPC || !mIm2dRenderBuffer.IsPreparedPC() || !mIm2dDebugRenderBuffer.IsPreparedPC())
    {
        CgsDev::Log::WriteToLog("[renderer] 2D command storage unavailable; shutting down.\n");
        CgsSystem::HardwareInit::RequestShutdown();
        return;
    }
    // ARTIST 8240C340..398 draws only in the original stall stages 0 and 1.
    // The native engine surface survives presentation. Suppressed frames must
    // begin without the host FrameBegin clear, then still run the original
    // loading-command, GDL and effects callback work before the draw gate.
    const bool lbDrawFrame = meFrameStallStage == E_FRAMESTALL_NOT_STALLED
        || meFrameStallStage == E_FRAMESTALL_SYNCING_BUFFERS;
    if (!(lbDrawFrame ? renderengine::Device::FrameBegin()
                      : renderengine::Device::FrameBeginNoClear()))
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

    // ---- X360 Render:389-396 -- the object->mesh expansion and the pass sorts. ------------
    // Hoisted up here (it used to live at the top of RenderWorldPasses) because the SHADOW
    // pass below consumes mesh lists 0..4 and, on the console, runs before the world passes.
    // The context object is on Render's stack exactly as the X360 keeps it.
    CgsGraphics::DispatchObjectContext lDispatchContext;
    lRenderStage.Next(RENDER_BUILD_LISTS);
    const bool lbDispatchReady = BuildDispatchLists(&lDispatchContext);
    lRenderStage.Next(RENDER_SETUP);

    // ARTIST 8240C31C..330: supplied owner, fresh control IO, after conversion
    // and sorting, including stage 2. The control pair rotates independently
    // of retained draw banks, so its events are consumed exactly once.
    {
        Scope lEffectsProfile(DISPATCH_EFFECTS);
        lpEffectsModule->DispatchThreadUpdate(lpDispatchThreadInputBuffer);
    }

    // ARTIST 8240C388..38C begins the frame-long read window only after
    // the effects callback has released its own read lock.
    if (lpDispatchThreadInputBuffer != 0)
        lpDispatchThreadInputBuffer->LockForRead();

    // Original 8240C398 -> 8240E25C skips all draws, including the HUD tail.
    if (!lbDrawFrame)
    {
        if (lpDispatchThreadInputBuffer != 0)
            lpDispatchThreadInputBuffer->UnlockForRead();
        lRenderStage.Next(RENDER_PRESENT);
        renderengine::Device::ShowPixelBuffer();
        return;
    }

    // ARTIST 8240C4B0..C4C4: conversion uses time zero, then ALL mesh draw
    // passes use the completed shader frame's game time (+0x314). Publish it
    // after the draw gate so retained/stalled command banks keep their clock.
    if (mpInterpreter != nullptr)
        mpInterpreter->SetTime(maShaderConstantsFrames[mu8ShaderConstantsFrameInternal].GetGameTime());

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
    EnsurePostFxSceneTargets(mAllocatedRenderTargets, mbMultisampledBackbuffer);

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
    // ARTIST Construct8240A9F0 constructs the textured3D renderer. Native
    // D3D9 program adoption needs the device, so defer only that construction.
    if (!mbIm3dRendererConstructedPC && EnsureWorldDispatchAllocator())
    {
        mIm3dRenderer.Construct(&sWorldDispatchAllocator);
        mbIm3dRendererConstructedPC = mIm3dRenderer.HasProgramsPC();
        CGS_ASSERT(mbIm3dRendererConstructedPC, "mIm3dRenderer.Construct");
        CGS_ASSERT(CgsGraphics::GetImWhiteTexturePC() != nullptr, "Immediate-mode white texture unavailable");
    }

    // ---- X360 Render:505-533 -- THE COLOUR-CUBE (3D LUT) TINT BLOCK. ---------------------
    //
    // POSITION IS THE POINT. The console runs this HERE -- inside the PerfMonCpu bracket
    // `*(this + 51508)` (StartMonitor @0x8240C698, StopMonitor @0x8240C79C), after the dispatch
    // sorts and BEFORE shadow::Device::ResetShadowing(), the three texture binds below and the
    // shadow-map pass -- because BrnPostFx::BeginTintBlend SCHEDULES an EA::Jobs job that blends
    // the source colour cubes into the tint volume texture while the shadow map and the world
    // passes draw. BrnPostFx::Render drains it at the top of the composite (m_processTint ->
    // Job::WaitOn -> Tint::EndBlendJob) before anything samples s3. Putting it down with the apply
    // block would leave the job no window at all.
    //
    // THE GATE IS mbRenderPostFX (`lbz r11, 0(this + 0xC41C)` / `beq loc_8240C798` @0x8240C6A8-
    // 0x8240C6BC) -- the same "Render PostFX" debug toggle that gates the apply block and the
    // composite further down, tested a THIRD time here, exactly as the console tests it.
    //
    // ⚠ THE ONE PC PRECONDITION THIS BLOCK CARRIES ON TOP OF THE CONSOLE'S GATE, and it is not
    // optional. BeginTintBlend LOCKS the tint volume texture (Tint::BeginBlendJob ->
    // renderengine::Texture::Lock) and the ONLY thing that unlocks it is BrnPostFx::Render's step-1
    // drain, `if (m_processTint) { m_blendJob.WaitOn(); EndTintBlend(); }`. On the console those two
    // sites sit under ONE gate -- the same mbRenderPostFX byte, `lbz r11, 0(this + 0xC41C)` at
    // 0x8240C6A8 here and at 0x8240DC6C for the composite -- so a lock can never outlive its frame.
    // On PC the composite sits behind three further preconditions this block does not share:
    // BRN_ANTIALIAS_BRACKET_AVAILABLE / BRN_POSTFX_COMPOSITE_AVAILABLE, the scene bracket
    // (lbDispatchReady && EnsurePostFxSceneTargets, :3127), and the pool + Construct test inside
    // PCBringUpRenderPostFxComposite. With any of them false the tint would Lock every frame and
    // never Unlock -- re-locking the same alternating buffer while it is still held. So the block
    // inherits the composite's whole precondition, evaluated here from the same inputs the bracket
    // below uses: lbDispatchReady is fixed for the frame, EnsurePostFxSceneTargets is value-latched
    // and has already run unconditionally above, and PCBringUpConstructPostFx runs from inside it --
    // so asking here and asking at :3127 cannot disagree.
#if BRN_ANTIALIAS_BRACKET_AVAILABLE && BRN_POSTFX_COMPOSITE_AVAILABLE
    const bool lbTintBlendWillDrain =
        lbDispatchReady
        && maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameInternal]
        && EnsurePostFxSceneTargets(mAllocatedRenderTargets, mbMultisampledBackbuffer)
        && PCBringUpPostFxCompositeWillRun(mAllocatedRenderTargets);
#else
    // No composite in this configuration => nothing would ever drain the blend, so it is not started.
    const bool lbTintBlendWillDrain = false;
#endif

    if (mbRenderPostFX && lbTintBlendWillDrain)
    {
        lRenderStage.Next(RENDER_TINT);
        // v296 -- DispatchThreadInputBuffer::GetCalibrationUnfriendlyEnablePostFx. The console
        // reads it ONCE near the top of Render (pseudocode lines 440-441) and ANDs it into this
        // block AND into all six effects in the apply block; this is the same read, inside the
        // same frame-long read lock, so it is the same value. The no-buffer fallback is the
        // buffer's OWN Construct default (BrnDispatchThreadInputBuffer.cpp:232 seeds it true),
        // not a pick -- identical to the apply block's fallback below.
        const bool lbTintEffectsAllowed = (lpDispatchThreadInputBuffer != 0)
            ? lpDispatchThreadInputBuffer->GetCalibrationUnfriendlyEnablePostFx()
            : true;

        BrnRendererBeginPostFxTintBlend(
            sbEffectsArbitratorConstructed ? &mEffectsArbitrator : 0, lbTintEffectsAllowed);
        lRenderStage.Next(RENDER_SETUP);
    }

#if BRN_ENVMAP_PASS_AVAILABLE
    // [PC bring-up] SEED THE CONSOLE'S OWN ENV-MAP SWITCH FROM config.ini, once.
    //
    // ConstructRenderSwitches (BrnRendererModule.h) sets mRenderSwitches.mbRenderEnvmap true, which
    // is the console's value -- and it runs from the module's constructor, i.e. at STATIC-INIT time
    // (the module is reached from `static BrnGame::BrnGameModule gGameModule;`, BrnMain.cpp:45).
    // renderengine::Device::Initialize and BrnMain's LoadConfig both run later, so seeding the switch
    // in the constructor would read the value the file has not been consulted for yet. It is
    // therefore seeded here, on the first Render, from the knob -- and NOT re-applied per frame, so a
    // debug menu that toggles the switch at run time still wins, exactly as it does on the console.
    // DELETE-WHEN the debug component owns the switch and the knob can go.
    {
        static bool sbEnvMapSwitchSeeded = false;
        if (!sbEnvMapSwitchSeeded)
        {
            sbEnvMapSwitchSeeded = true;
            mRenderSwitches.mbRenderEnvmap = (renderengine::gEnvironmentMap != 0);
            if (!mRenderSwitches.mbRenderEnvmap && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[envmap] config.ini [Settings] EnvironmentMap=0 -- the six-face pass and the"
                       " sampler-13 bind are OFF\n";
            }
        }
    }
#endif  // BRN_ENVMAP_PASS_AVAILABLE

    // ---- THE CORONA SWITCH, seeded from config.ini exactly like the env-map one above ---------
    // ConstructRenderSwitches (BrnRendererModule.h:832) sets mbRenderCoronas true, which IS the
    // console's value, and it runs at static-init time -- before Device::Initialize and before
    // LoadConfig. So the knob seeds it here, on the first Render, once, and never again: a debug
    // toggle at run time still wins, exactly as on the console. NO SECOND SWITCH IS MINTED
    // (AGENTS.md rule 3) -- renderengine::gCoronas only seeds the module's own bool.
    // DELETE-WHEN the debug component owns the switch and the knob can go.
    {
        static bool sbCoronaSwitchSeeded = false;
        if (!sbCoronaSwitchSeeded)
        {
            sbCoronaSwitchSeeded = true;
            mbRenderCoronas = (renderengine::gCoronas != 0);
            if (!mbRenderCoronas && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[corona] config.ini [Settings] Coronas=0 -- the corona pass is OFF\n";
            }
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

        // ⭐⭐ s14 -- THE GLASS-FRACTURE MAP. CORRECTED 2026-09-06 (cracked-glass wave): this
        // bind used to hand unit 14 mpBlobbyShadowTexture, and that was a MISIDENTIFIED MEMBER,
        // not a stand-in. The console's line is
        //     v79 = *(this + 5804); if (v79) sub_8227D158(v79, 14);   Render @0x8240BFA8:538-540
        // and this+5804 is mpGlassFractureTextureState, which PrepareAgain @0x823FF8F8 builds
        // out of its SIXTH argument -- the glass-fracture texture -- three instructions after it
        // stores that same argument into the params block:
        //     stw r30, 0x16A8(r31)   ; 5800  mpGlassFractureTexture      = lpGlassFracture
        //     stw r30, 0x16F8(r31)   ; 5880  mGlassFractureTextureStateParams.mpTexture (+72)
        //     ...                    ; 5884  the TextureState resource block (5 words)
        //     stw r3,  0x16AC(r31)   ; 5804  mpGlassFractureTextureState = Initialize(5884, 5808)
        // The blobby-shadow texture is PrepareAgain's SECOND argument and lands at this+14320
        // (`stw r11, 0x37F0(r31)`); its only reader in the whole XEX is
        // BrnBlobbyShadowManager::Render (`*(v12 + 14320)`, Render:859). It never goes near a
        // sampler unit.
        //
        // WHAT THE WRONG TEXTURE COST. All 22 shipped vehicle pixel shaders declare
        // `GlassFractureSampler` at s14; the vehicle GLASS program
        // (Glass_Specular_Transparent_Doublesided, PS in build/game/SHADERS.BNDL) samples it
        // TWICE per pixel and its whole crack term is those two fetches:
        //     texld r1, v6,      s14
        //     texld r3, v6.zwzw, s14
        //     max   r4.xyz, r1.xyww, r3.xyww
        //     mad_sat r1.xy, r4, c5.y, -c5.x        ; c5 == g_glassFractureStrength
        // so unit 14 was feeding the crack the blobby-shadow blob instead of the crack map.
        //
        // The bind stays GATED on the pointer exactly as the console gates it, and it goes
        // through shadow::Device::SetResource for the same reason s15 above does: this build
        // has no TextureState objects yet (Construct's Initialize pair needs the resource
        // allocator), so the resource-only entry point plus an explicit sampler apply is the
        // honest stand-in. The sampler half is NOT optional here -- the vertex program tiles
        // the crack UVs (`mad o7, v3.xyxy, c160, c159` with per-pane offsets in [-0.9, +0.9]),
        // so a CLAMPed unit would smear one texel across a whole pane.
        if (mpGlassFractureTexture != 0)
        {
            shadow::Device::SetResource(mpGlassFractureTexture, 14u);
            renderengine::GlassFractureSampler_ApplyState(14u);

            static bool sbLoggedGlassFractureBind = false;
            if (!sbLoggedGlassFractureBind && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedGlassFractureBind = true;
                *CgsDev::Log::gpDebugPrint
                    << "[glassfx] s14 bound: glass-fracture texture " << mpGlassFractureTexture
                    << " (WRAP/LINEAR)\n";
            }
        }
        else if (CgsDev::Log::gpDebugPrint != 0)
        {
            static bool sbLoggedGlassFractureMissing = false;
            if (!sbLoggedGlassFractureMissing)
            {
                sbLoggedGlassFractureMissing = true;
                *CgsDev::Log::gpDebugPrint
                    << "[glassfx] s14 UNBOUND -- mpGlassFractureTexture is null; every cracked"
                       " pane will sample whatever unit 14 last held\n";
            }
        }

#if BRN_ENVMAP_PASS_AVAILABLE
        // s13 -- UNPARKED (reflections step 1, 2026-08-17). The console's bind is
        //     sub_8227D158(*(this + 5700), 13)          Render @0x8240BFA8 pseudocode line 542
        // i.e. shadow::Device::SetState(mpEnvMapTextureState, 13) -- the WHOLE-UNIT TextureState
        // overload, not the bare-resource one the two lines above use. this+5700 is
        // mpEnvMapTextureState: its sibling at this+5924 is what line 538 binds to s15, the two
        // differ by 224, and 224 is exactly the distance from mpEnvMapTextureState to
        // mpShadowMapTextureState[0] in the committed member order (see EnsureEnvMapTextureState).
        //
        // THREE DIFFERENCES FROM THE CONSOLE, all of them PC preconditions rather than choices:
        //   * the console binds UNCONDITIONALLY (no null test at line 542, unlike s14 at 540-541),
        //     because its pool cannot fail. Here the target is built lazily and can legitimately not
        //     exist yet -- or be refused outright by EnsureEnvMapTarget -- so the bind is gated on
        //     the same value-latched gate the pass uses. An unbound unit is what this build already
        //     ships; binding a non-cube texture to a samplerCUBE is not.
        //   * it is gated on mRenderSwitches.mbRenderEnvmap as well, so config.ini
        //     `[Settings] EnvironmentMap=0` really does leave sampler 13 exactly as it is today.
        //     The console has no such gate because it has no such knob.
        //   * the TextureState is built HERE, once, rather than in Construct @0x8240A778, for the
        //     same reason every other Construct step in this file is deferred: the render-target
        //     pool it samples does not exist until the D3D9 device does.
        //
        // POSITION IS THE CONSOLE'S -- before the shadow-map pass, in the same three-bind block --
        // and it has to be, because the CAR passes that sample s13 (mesh lists 19/20) run much
        // later, inside RenderWorldPasses. NOTHING BETWEEN THE TWO REBINDS UNIT 13; the check was
        // made rather than assumed, against the COMMITTED tree (i.e. before this hunk lands):
        //     $ grep -rn "SetResource(.*13\|SetState(.*, *13\|SetTexture(.*13\|, *13u)"         //           b5-decomp/src --include=*.cpp --include=*.h | grep -v "0x13\|\[13\]"
        //     (no output)
        // and the one PC bracket that does park a unit behind the engine's back
        // (PCSurfaceBracket_Save) parks unit 15 only, by the named constant KU_SHADOW_SAMPLER_UNIT:
        //     $ grep -rn "KU_SHADOW_SAMPLER_UNIT" b5-decomp/src
        //     pc/gcm/renderengine/XenonD3D9Shims.cpp:4073: const u32 KU_SHADOW_SAMPLER_UNIT = 15u;
        //     ...:4107 / :4110 / :4150 / :4151  (GetTexture / SetTexture(null) / restore / ApplyState)
        // The env-map pass itself renders INTO this texture between the bind and the car passes,
        // which on this backend means it is bound as a sampler source while it is written -- the
        // same read-while-written hazard the shadow bracket documents. It is handled where it
        // belongs, in the loop below, not here.
        if (mRenderSwitches.mbRenderEnvmap && EnsureEnvMapTarget(mAllocatedRenderTargets)
            && EnsureEnvMapTextureState(&mpEnvMapTextureState, gpEnvMapTarget))
        {
            shadow::Device::SetState(mpEnvMapTextureState, 13u);

            static bool sbLoggedEnvMapBind = false;
            if (!sbLoggedEnvMapBind && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedEnvMapBind = true;
                *CgsDev::Log::gpDebugPrint
                    << "[envmap] s13 bound: cube texture " << mpEnvMapTextureState->mpRaster
                    << " (TextureState " << mpEnvMapTextureState << ")\n";
            }
        }
#else
        // s13 -- PARKED. See the BRN_ENVMAP_PASS_AVAILABLE banner at the top of this file.
#endif  // BRN_ENVMAP_PASS_AVAILABLE
    }

    // ---- X360 Render:545-640 -- THE SHADOW-MAP PASS. -------------------------------------
    // Before RenderWorldPasses and before anything binds the scene target, exactly as the
    // console orders it. Gated on mRenderSwitches.mbRenderShadows.
    if (lbDispatchReady)
    {
        lRenderStage.Next(RENDER_SHADOWS);
        RenderShadowMapPasses(&lDispatchContext);
        lRenderStage.Next(RENDER_SETUP);
    }

#if BRN_ANTIALIAS_BRACKET_AVAILABLE
    // ---- X360 Render:725 -- OPEN THE ANTI-ALIASED SCENE PASS (call @0x8240CDB8). ----------
    //
    // POSITION. The console calls BeginRenderAntiAliased after the shadow-map pass and after the
    // env-map face loop, and before every world-pass dispatch: in Render's listing the last
    // BrnGraphics__ShadowMapRenderManager__EndRenderShadowMap is @0x8240CB1C, the env-map loop's
    // BrnRendererModule__EndRenderEnvironmentMapFace is @0x8240CD74, and the call is @0x8240CDB8.
    // The env-map faces ARE reconstructed as of reflections step 1 and run immediately above this
    // bracket (the six-face loop, whose own banner carries the address evidence), so this point
    // between the env-map loop and the world pass IS the console's position.
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
        // FLAG PC-platform leaf: native presentation can precede the first
        // published world camera/constants frame. An initialized empty mesh
        // bank is not a scene; keep the host frame's black clear until then.
        lbDispatchReady && maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameInternal]
        && EnsurePostFxSceneTargets(mAllocatedRenderTargets,
                                                    mbMultisampledBackbuffer);

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
    //
    // ⭐ CORRECTED 2026-08-18 (coronas step 2, group `coronacalib`). This paragraph used to end
    // "On this build the value is 1.0f ... nothing writes the INTERNAL frame's white level". That
    // has not been true since post-fx step 10: PublishSkyConstantsBringUp writes the live
    // EnvironmentManager white level into maShaderConstantsFrames[EXTERNAL]
    // (SetWhiteLevel, this file's PublishSkyConstantsBringUp), and SwapBuffers makes that slot the
    // INTERNAL one (`mu8ShaderConstantsFrameInternal = mu8ShaderConstantsFrameExternal`). The
    // shipped boot log reads `[sky] ... whiteLevel 0.500000` on every sky publish, and
    // `[postfx-composite] call 1000: ... white=1` only because those calls precede the first
    // publish. It IS read from the member, never hard-coded, which is what matters -- and the
    // SAME local feeds BeginRenderAntiAliased, the corona pass and the composite below, so the
    // corona vertex program's `colour.rgb * cameraPositionPlusBrightness.w` and the composite's
    // `GlobalParams.x = 1 / whiteLevel` (BrnPostFxShader.cpp:1458) are guaranteed to be the same
    // number's multiply and divide -- the console's own round trip, cancelling exactly. A corona
    // pass fed a DIFFERENT white level from the composite would be brighter or darker by that
    // ratio, which is why the value is printed in the `[corona-calib]` line.
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
    // lbClearStencil is not observable: it is not read by the X360 BeginRenderAntiAliased body
    // (see its own note). luSceneStencilClearValue NOW IS, and that changed with rung 8 -- the
    // sentence here used to end "the PC scene target's depth surface is picked by a format ladder
    // whose first candidate is D3DFMT_D24X8 -- no stencil bits -- so the PC D3DDevice_Resolve strips
    // D3DCLEAR_STENCIL from its clear", which was true only while the target was single-sampled.
    // A MULTISAMPLED scene target takes the MSAA depth ladder instead, whose first candidate is
    // D3DFMT_D24S8 (PostFxRenderTargetPCLeaf.cpp KAE_MSAA_DEPTH_FORMATS[0], chosen because the
    // build's own auto depth-stencil and the RESZ destination are both D24S8 layouts), so the
    // bound depth DOES carry stencil and TilingClearFlagsForBoundSurfaces keeps D3DCLEAR_STENCIL.
    // The value is still 0 on this build (no motion-blur event posts one), and clearing stencil to
    // it is what the console does -- so this is a faithful widening, not a behaviour to undo.
    BrnRendererPostFxFrameBytes lPostFxFrameBytes;
    BrnRendererReadPostFxFrameBytes(
        sbEffectsArbitratorConstructed ? &mEffectsArbitrator : 0, &lPostFxFrameBytes);

    const bool lbSceneClearStencil      = lPostFxFrameBytes.mbMotionBlurActive;
    const u8   luSceneStencilClearValue = lPostFxFrameBytes.mu8WorldBlurStencil;

#if BRN_ENVMAP_PASS_AVAILABLE
    // ============================================================================================
    // ---- X360 Render pseudocode 645-722 (asm 0x8240CB64-0x8240CD8C) -- THE SIX-FACE ENV-MAP PASS.
    //
    // POSITION. The console runs this AFTER the shadow-map pass and BEFORE BeginRenderAntiAliased:
    // the last EndRenderShadowMap is @0x8240CB1C, this loop's EndRenderEnvironmentMapFace is
    // @0x8240CD74, and the BeginRenderAntiAliased call is @0x8240CDB8. So this is the console's own
    // position, and the banner that used to stand at the bracket below -- "the env-map faces are not
    // reconstructed on this build, so this point between the shadow pass and the world pass IS the
    // console's position" -- is retired by this wave (it is corrected in place, above).
    //
    // ⚠ WHY IT SITS INSIDE `#if BRN_ANTIALIAS_BRACKET_AVAILABLE` AND ON lbSceneBracketOpen, which is
    // a PC precondition with no console counterpart and the single most important line in this block.
    // Each face BINDS THE 128x128 CUBE TARGET and leaves it bound. On the console the very next thing
    // Render does is BeginRenderAntiAliased, which rebinds the scene target THROUGH
    // renderengine::Device::SetState -- so the console needs no bracket and this build must not
    // invent one. But on PC that call is conditional: it is compiled out at
    // BRN_ANTIALIAS_BRACKET_AVAILABLE 0 and skipped at run time when lbSceneBracketOpen is false. If
    // the faces ran and the rebind did not, RenderWorldPasses, the GUI and the 2D tail would all draw
    // into a 128x128 cube face at a 128x128 viewport -- a black screen with no error anywhere. So the
    // pass is gated on exactly the condition that guarantees its own rebind.
    //   * NOT PCSurfaceBracket_Save/Restore, deliberately, even though it is the obvious tool: that
    //     bracket also raises `sbShadowPassActive` (XenonD3D9Shims.cpp), which changes the cull and
    //     depth-bias behaviour of WorldDraw_IndexedUP, and it parks sampler 15. Both are shadow-pass
    //     semantics; borrowing them for the env map would be a fabricated convention.
    //   * ShadowPassPCLeaf.h's four-part deletion condition for that bracket ends "(4) NOTHING
    //     BETWEEN THIS PASS AND THAT CALL MAY DRAW WITHOUT BINDING ITS OWN TARGET. On the console the
    //     env-map pass sits in that gap and binds its own; on PC, verify it." This IS that
    //     verification: the pass binds its own target, and it only runs when the rebind will follow.
    // VIEWPORT/gpLastRenderTargetState across the two, stated because a wrong answer here is
    // invisible: SetRenderTargetStateInvertDepth binds through renderengine::Device::SetState and
    // WRITES renderengine::gpLastRenderTargetState (CgsRenderTarget.cpp:348-352), so the engine's
    // "last state installed" shadow stays truthful; BeginRenderAntiAliased then binds the anti-alias
    // buffer's section-0 state through the same path, the pointers differ, and the bind therefore
    // happens rather than being skipped. The face leaves a 128x128 viewport (MinZ=1/MaxZ=0) and a
    // 128x128 scissor rect installed. The VIEWPORT is restored by BeginRenderAntiAliased's
    // ShadowedSetRenderTargetState -> renderengine::Device::SetState -> IDirect3DDevice9::
    // SetRenderTarget, which D3D9 documents as resetting the viewport to the full extent of the new
    // render target (MinZ=0/MaxZ=1) -- XenonD3D9Shims.cpp's PCSurfaceBracket banner settled exactly
    // this for rung 8; D3DDevice_BeginTiling sets no viewport and only logs a mismatch. The SCISSOR
    // RECT is NOT restored by anything: it is harmless only because renderengine::Device::
    // SetWorldPassDefaultStates drives D3DRS_SCISSORTESTENABLE off for the world passes and the
    // post-fx tail sets its own; a future scissor-enabled pass between here and there would inherit
    // 128x128 (verify F4, envface -- corrected from the first cut's "tile rects" claim).
    //
    // ⚠ READ-WHILE-WRITTEN, and it is REAL on this backend. Sampler 13 was bound to this cube's
    // colour texture a few hundred lines above, and these faces render INTO it. On the console those
    // are two different objects (the pass writes an EDRAM tile; the sampler reads the resolved copy),
    // which is why the console leaves the bind standing. On D3D9 the texture IS the resolve
    // destination, so unit 13 is parked for the length of the loop and restored after it -- the same
    // treatment, for the same reason, that PCSurfaceBracket_Save gives unit 15, and the pointer put
    // back is the one shadow::Device's cache still believes is bound, so the cache stays truthful.
    //
    // WHAT IS DROPPED FROM THE CONSOLE BODY, each with its reason (nothing is dropped silently):
    //   * `D3DDevice_SetShaderGPRAllocation(dev, 0, 0x30, 0x50)` @0x8240CB38 -- a Xenos GPR-partition
    //     hint with no D3D9 counterpart. This file already drops the other three occurrences in
    //     Render (0x8240C7F0, 0x8240D6FC, 0x8240DE68); named here so it stays greppable.
    //   * the PerfMonGpu / PerfMonCpu Start/Stop pairs (miEnvironmentMap gpu +51664, cpu +51528,
    //     per-face wait +51532) -- the whole of Render carries none of them on this build; see the
    //     BRN_GPU_PERFMON_AVAILABLE banner.
    // The per-face sort rendezvous at ARTIST 8240CBF8 is handled below by
    // WaitForMeshSortPC, using the descriptors belonging to this frame bank.
    // ============================================================================================
    lRenderStage.Next(RENDER_ENVMAP);
    if (mRenderSwitches.mbRenderEnvmap && lbSceneBracketOpen
        && lpDispatchThreadInputBuffer != 0
        && EnsureEnvMapTarget(mAllocatedRenderTargets))
    {
        // The sky half's own preconditions, evaluated ONCE for the whole loop. On the console the
        // sky dome's geometry and constants are built in Construct/Prepare and filled by the world;
        // on PC both are bring-up gates that RenderWorldPasses' main sky pass already carries
        // (BrnRendererModule.cpp, the mbRenderSky block), and this pass carries the same set so that
        // the two can never disagree about whether a dome exists.
        //
        // ARTIST 0x8240CD34..0x8240CD44 copies the INTERNAL shading frame.
        // The native producer bridge now publishes it at SwapBuffers, after join.
        const bool lbDebugFaceColours = EnvMapDebugFaceColours();
        (void)lbDebugFaceColours;
        const bool lbSkyReady = mbRenderSky
                             && maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameInternal]
                             && EnsureSkyDomeBringUp();

        // FLAG PC-platform leaf: park the cube off sampler 13 for the length of the pass (see the
        // READ-WHILE-WRITTEN note above). shadow::Device::SetResource caches on the pointer, so
        // handing it null and then the texture again is two real D3D9 SetTexture calls and leaves
        // the TEXTURE shadow holding the texture -- the truth on both sides of the loop.
        // ⚠ It also clears the WHOLE-UNIT key (mapTextureState[13] = 0, which SetResource does by
        // design -- a bare-texture bind cannot claim a TextureState is installed), so next frame's
        // SetState(mpEnvMapTextureState, 13) misses its cache and re-applies. That is CORRECT, not
        // a leak: the unit really did lose its whole-unit binding, and a cache that claimed
        // otherwise is exactly the lie that put the shadow cascades in the back buffer.
        const bool lbUnparkSampler13 = (mpEnvMapTextureState != 0);
        if (lbUnparkSampler13)
        {
            shadow::Device::SetResource(0, 13u);
        }

        // [DIAG envmap-perf, reflections step 2] CPU stage timers for the six-face pass. MEASURED
        // 2026-08-17 (VSync off, all six faces): loop=1614..1999 us/frame, begin+clear=9,
        // dispatch=1558..1941, sky=35, end+resolve=11, meshes/frame=883..1130 -- i.e. the pass is
        // DRAW-BOUND at ~1.7 us/mesh (the D3D9 runtime floor), not a per-face waste; the producer's
        // own share is ~40 us (gShadowPerf). The registered world refresh control can select
        // the original half schedule; its default remains all six faces. The line prints only
        // with BRN_ENVMAP_PERF=1 (the two QueryPerformanceCounter calls per face stay -- they are
        // cheaper than the log gate they feed). DELETE-WHEN a batching draw path lands.
        struct EnvMapPerf { double mfBegin, mfDispatch, mfSky, mfEnd, mfLoop; u32 muFaces, muMeshes, muFrames; };
        static EnvMapPerf sEnvMapPerf = {};
        static s32 siEnvMapPerfLog = -1;
        if (siEnvMapPerfLog < 0)
        {
            char lacPerf[8] = { 0 };
            siEnvMapPerfLog = (GetEnvironmentVariableA("BRN_ENVMAP_PERF", lacPerf, sizeof(lacPerf)) > 0
                               && lacPerf[0] != '0') ? 1 : 0;
        }
        LARGE_INTEGER lPerfFreq; QueryPerformanceFrequency(&lPerfFreq);
        const double lfUsPerTick = 1.0e6 / static_cast<double>(lPerfFreq.QuadPart);
        LARGE_INTEGER lLoopT0; QueryPerformanceCounter(&lLoopT0);

        // [FLAG PC bring-up probe] this frame's per-face schedule + mesh counts, for the
        // BRN_ENVMAP_STATS witness below. Always filled (two stores per face); only read when
        // the witness is armed. DELETE-WHEN b5-decomp#5 is closed.
        bool labStatFaceRendered[BrnGraphics::E_FACE_NUM] = {};
        u32  lauStatFaceMeshes[BrnGraphics::E_FACE_NUM]   = {};
        static u32 sxEnvMapEverRendered = 0u;   // bit f = face f rendered at least once, ever
        static u32 suEnvMapBeginCount   = 0u;   // total BeginRenderEnvironmentMapFace calls

        for (u32 luFace = 0; luFace < BrnGraphics::E_FACE_NUM; ++luFace)
        {
            // X360 @0x8240CC24: `lbzx r11, r29, r17` where r17 == the READ buffer + 0x99B4, i.e.
            // mabEnvMapFaceRender[luFace] -- the per-face "the producer filled list 5+face this
            // dispatch" byte, read through the accessor whose bounds assert the console fires at
            // 0x8240CC0C ("luIndex<6"). A face the producer did not fill is skipped whole: no
            // Begin, no dispatch, no sky, no resolve -- which is what makes the console's
            // three-faces-per-frame alternation (WorldModule::GenerateFrustumQueries :3486-3499)
            // cost three faces and not six.
            if (!GetPublishedEnvMapFaceRenderPC(luFace))
                continue;

            labStatFaceRendered[luFace] = true;
            sxEnvMapEverRendered |= (1u << luFace);   // [probe] see the BRN_ENVMAP_STATS block
            ++suEnvMapBeginCount;

            LARGE_INTEGER lT0; QueryPerformanceCounter(&lT0);
            BeginRenderEnvironmentMapFace(luFace, lfFrameWhiteLevel);
            LARGE_INTEGER lT1; QueryPerformanceCounter(&lT1);

            // The lock window. The console asserts on entry and exit of BOTH thirds
            // (shadowingdevice.h:1215/1220/1235/1240 -- the four assert strings are inline in
            // Render at 0x8240CC54/0x8240CC88/0x8240CCDC/0x8240CD10), which is exactly
            // Lock/UnlockRasteriserState + Lock/UnlockDepthStencilState. It is what stops the
            // material walk's per-technique binds from replacing the inverted-depth GREATER-EQUAL
            // state and the cull-front raster state BeginRenderEnvironmentMapFace just applied.
            // ⚠ ORDER: rasteriser locks FIRST and unlocks FIRST (0x8240CC40 before 0x8240CC6C;
            // 0x8240CCCC before 0x8240CCF4) -- it is NOT a nested pair, and reversing it would trip
            // the asserts rather than merely reorder two writes.
            shadow::Device::LockRasteriserState();
            shadow::Device::LockDepthStencilState();

            WaitForMeshSortPC(KU_ENV_MAP_FIRST_MESH_LIST + luFace);
            CgsGraphics::DispatchList* const lpList =
                GetMeshFrameForReadPC().GetList(KU_ENV_MAP_FIRST_MESH_LIST + luFace);
            const u32 luMeshCount = lpList->GetCount();
            lauStatFaceMeshes[luFace] = luMeshCount;
            lpList->DispatchAllMeshes(mpInterpreter, &lDispatchContext, 0, -1);

            shadow::Device::UnlockRasteriserState();
            shadow::Device::UnlockDepthStencilState();
            LARGE_INTEGER lT2; QueryPerformanceCounter(&lT2);

            if (lbSkyReady)
            {
                // The console copies the frame PER FACE (the copy constructor is inside the loop,
                // @0x8240CD44) and passes the copy's address; RenderToEnvironmentMap takes a
                // `const BrnShaderConstantsFrame*` (DecFIGS BrnSkyDomeManager.h:71), so the copy is
                // the compiler materialising a by-value temporary. It is reproduced rather than
                // hoisted: it is 800 bytes three times a frame, and the literal shape is worth more
                // than the copy is worth saving.
                const BrnShaderConstantsFrame lFrame =
                    maShaderConstantsFrames[mu8ShaderConstantsFrameInternal];
                mSkyDome.RenderToEnvironmentMap(
                    static_cast<BrnGraphics::EEnvironmentMapFace>(luFace),
                    &mIm3dRendererSkyDome,
                    mpCloudDensity0Texture, mpCloudLighting0Texture,
                    &lFrame);
            }

            LARGE_INTEGER lT3; QueryPerformanceCounter(&lT3);
            EndRenderEnvironmentMapFace(luFace);
            LARGE_INTEGER lT4; QueryPerformanceCounter(&lT4);
            sEnvMapPerf.mfBegin    += static_cast<double>(lT1.QuadPart - lT0.QuadPart) * lfUsPerTick;
            sEnvMapPerf.mfDispatch += static_cast<double>(lT2.QuadPart - lT1.QuadPart) * lfUsPerTick;
            sEnvMapPerf.mfSky      += static_cast<double>(lT3.QuadPart - lT2.QuadPart) * lfUsPerTick;
            sEnvMapPerf.mfEnd      += static_cast<double>(lT4.QuadPart - lT3.QuadPart) * lfUsPerTick;
            sEnvMapPerf.muFaces    += 1u;
            sEnvMapPerf.muMeshes   += luMeshCount;

            // [FLAG PC bring-up probe] the first face that actually ran. LATCHED ON A ONE-SHOT here
            // and NOT on a value, deliberately and against this file's usual rule: the question it
            // answers is "did the pass ever run at all", the loop cannot fire on the loading screen
            // (it needs a dispatch buffer, a world list and a built cube), and the per-frame answer
            // is already covered by the target line above. DELETE with the bring-up.
            static bool sbLoggedFirstFace = false;
            if (!sbLoggedFirstFace && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedFirstFace = true;
                *CgsDev::Log::gpDebugPrint
                    << "[envmap] first face pass: face=" << static_cast<s32>(luFace)
                    << " list=" << static_cast<s32>(KU_ENV_MAP_FIRST_MESH_LIST + luFace)
                    << " meshes=" << static_cast<s32>(luMeshCount)
                    << " sky=" << (lbSkyReady ? 1 : 0) << "\n";
            }
        }

        // ========================================================================================
        // [FLAG PC bring-up probe] BRN_ENVMAP_STATS=1 -- WHAT THE CUBE ACTUALLY HOLDS.
        //
        // Written for the reflections regression (b5-decomp#5, "reflections are weird ... they
        // were working before"). A screenshot of a car cannot separate the two explanations --
        // six good faces sampled wrongly, or some faces good and some frozen/black -- so this
        // reads the six RESOLVED faces back off the GPU and prints one line per face:
        //     [envmap] update <n> face <i> mean=(r,g,b) std=<s> lum=<l> rendered=<0|1> meshes=<m>
        // `rendered` is this frame's mabEnvMapFaceRender byte (under the 30 Hz schedule only
        // three faces are refreshed per frame), `meshes` the world mesh count dispatched into
        // that face this frame.
        //
        // RATE: one sample every KU_ENVMAP_STATS_PERIOD passes, for the first
        // KU_ENVMAP_STATS_SAMPLES samples -- 20 samples x 6 lines = 120 lines a run, and 120 GPU
        // stalls spread over ~40 s. It is OFF unless the environment variable is set, so a normal
        // run pays one getenv.
        // DELETE-WHEN b5-decomp#5 is closed.
        // ========================================================================================
        {
            // PRIME, deliberately: the env-map schedule alternates halves frame by frame and the
            // sim sub-step count alternates with it, so an EVEN period samples one parity only
            // and would report "no face ever renders" on a build where half the frames render
            // three. 61 is coprime with 2 and 3.
            //
            // ⚠ AND THE CLOCK STARTS AT THE FIRST FACE, NOT AT THE FIRST PASS. This block runs
            // from the loading screen onwards -- the target exists long before any world list
            // does -- so a counter started at pass 0 spends its whole sample budget on the boot
            // frames, where an empty cube and `rendered=0` are CORRECT and say nothing about the
            // bug. Measured 2026-09-06: 20 samples at period 30 all landed inside the first ten
            // seconds of a 150-second run. Gating on suEnvMapBeginCount puts every sample in the
            // live phase, which is the only phase the question is about.
            const u32 KU_ENVMAP_STATS_PERIOD  = 61u;
            const u32 KU_ENVMAP_STATS_SAMPLES = 20u;

            static s32 siEnvMapStats = -1;
            if (siEnvMapStats < 0)
            {
                char lacStats[8] = { 0 };
                siEnvMapStats = (GetEnvironmentVariableA("BRN_ENVMAP_STATS", lacStats,
                                                         sizeof(lacStats)) > 0
                                 && lacStats[0] != '0') ? 1 : 0;
            }

            // The raw pass counter runs from the first frame this block executes; the SAMPLE
            // clock starts at the first face actually rendered, or -- if no face ever renders,
            // which is itself the answer -- after KU_ENVMAP_STATS_FALLBACK raw passes, so the
            // witness still reports the cube it found.
            const u32 KU_ENVMAP_STATS_FALLBACK = 1800u;   // ~30 s at 60 Hz
            static u32 suEnvMapRawPass      = 0u;
            static u32 suEnvMapStatsPass    = 0u;
            static u32 suEnvMapStatsSamples = 0u;
            const u32 luRawPass = suEnvMapRawPass++;
            const bool lbStatsClockRunning =
                (suEnvMapBeginCount != 0u) || (luRawPass >= KU_ENVMAP_STATS_FALLBACK);
            const u32 luPass = lbStatsClockRunning ? suEnvMapStatsPass++ : 1u;

            if (siEnvMapStats != 0
                && suEnvMapStatsSamples < KU_ENVMAP_STATS_SAMPLES
                && (luPass % KU_ENVMAP_STATS_PERIOD) == 0u
                && CgsDev::Log::gpDebugPrint != 0
                && gpEnvMapTarget != 0
                && gpEnvMapTarget->GetRenderTarget() != 0)
            {
                const u32 luSample = suEnvMapStatsSamples++;
                *CgsDev::Log::gpDebugPrint
                    << "[envmap] sample " << static_cast<s32>(luSample)
                    << " pass " << static_cast<s32>(luPass)
                    << " readbuf=" << const_cast<void*>(
                           static_cast<const void*>(lpDispatchThreadInputBuffer))
                    << " everRendered=" << static_cast<s32>(sxEnvMapEverRendered)
                    << " begins=" << static_cast<s32>(suEnvMapBeginCount)
                    << "\n";
                for (u32 luStatFace = 0; luStatFace < BrnGraphics::E_FACE_NUM; ++luStatFace)
                {
                    f32 lafMean[3] = { 0.0f, 0.0f, 0.0f };
                    f32 lfStd      = 0.0f;
                    const bool lbOk =
                        gpEnvMapTarget->GetRenderTarget()->maColourTargets[0]
                            .PCReadBackFaceStats(luStatFace, lafMean, &lfStd);
                    const f32 lfLum = 0.299f * lafMean[0] + 0.587f * lafMean[1]
                                    + 0.114f * lafMean[2];
                    *CgsDev::Log::gpDebugPrint
                        << "[envmap] update " << static_cast<s32>(luSample)
                        << " face " << static_cast<s32>(luStatFace)
                        << " mean=(" << lafMean[0] << "," << lafMean[1] << "," << lafMean[2]
                        << ") std=" << lfStd
                        << " lum=" << lfLum
                        << " rendered=" << (labStatFaceRendered[luStatFace] ? 1 : 0)
                        << " meshes=" << static_cast<s32>(lauStatFaceMeshes[luStatFace])
                        << " read=" << (lbOk ? 1 : 0) << "\n";
                }
            }
        }

        if (lbUnparkSampler13)
        {
            shadow::Device::SetResource(mpEnvMapTextureState->mpRaster, 13u);
        }

        {
            LARGE_INTEGER lLoopT1; QueryPerformanceCounter(&lLoopT1);
            sEnvMapPerf.mfLoop += static_cast<double>(lLoopT1.QuadPart - lLoopT0.QuadPart) * lfUsPerTick;
            if (++sEnvMapPerf.muFrames >= 120u && siEnvMapPerfLog != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                const double lfN = static_cast<double>(sEnvMapPerf.muFrames);
                *CgsDev::Log::gpDebugPrint
                    << "[envmap-perf] us/frame over " << static_cast<s32>(sEnvMapPerf.muFrames)
                    << " frames: loop=" << static_cast<f32>(sEnvMapPerf.mfLoop / lfN)
                    << " begin+clear=" << static_cast<f32>(sEnvMapPerf.mfBegin / lfN)
                    << " dispatch=" << static_cast<f32>(sEnvMapPerf.mfDispatch / lfN)
                    << " sky=" << static_cast<f32>(sEnvMapPerf.mfSky / lfN)
                    << " end+resolve=" << static_cast<f32>(sEnvMapPerf.mfEnd / lfN)
                    << " | faces/frame=" << static_cast<f32>(sEnvMapPerf.muFaces / lfN)
                    << " meshes/frame=" << static_cast<f32>(sEnvMapPerf.muMeshes / lfN)
                    << "\n";
                sEnvMapPerf = EnvMapPerf();
            }
            else if (sEnvMapPerf.muFrames >= 120u)
            {
                sEnvMapPerf = EnvMapPerf();
            }
        }
    }
#endif  // BRN_ENVMAP_PASS_AVAILABLE

    lRenderStage.Next(RENDER_SETUP);
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
        // ==========================================================================================
        // ParticleModule::BuildLionVertexBuffers @0x8228AC20 -- and, inlined at its head,
        // TrailSystem::Update. CONSOLE POSITION: Render @0x8240BFA8 :453-454, under the same gate
        // as the full-res pass below and BEFORE any scene geometry. It publishes this frame's time
        // and VIEW-PROJECTION into the trail renderer -- the matrix TrailSystem::Render transforms
        // its strips by. Without it the tyre marks are transformed by a matrix nobody wrote.
        // Same null test, same DELETE-WHEN, as the full-res pass -- see its banner.
        lRenderStage.Next(RENDER_PARTICLE_BUILD);
        if (mbRenderParticles && lpDispatchThreadInputBuffer != 0)
        {
            const BrnParticle::ParticleModule::ParticleRenderData* lpPreRenderData =
                GetPublishedParticleRenderDataPC();
            if (lpPreRenderData != 0 && lpPreRenderData->mpParticleModule != 0)
            {
                // ParticleModule::BeginParticleRenderJob @0x8228A7C0 -- the console calls it
                // FIRST of the pair, at Render :453. It advances the spark motion-blur ring,
                // retires dead bucket, flips + locks the two effects vertex buffers and (here,
                // inline: no job scheduler on this build) builds the spark vertex buffer for
                // the batches RenderFullResParticles replays. Landed 2026-09-06 with the spark
                // family; before that this call had nothing to drive.
                lpPreRenderData->mpParticleModule->BeginParticleRenderJob(lpPreRenderData);
                lpPreRenderData->mpParticleModule->BuildLionVertexBuffers(lpPreRenderData);
            }
        }

        lRenderStage.Next(RENDER_WORLD);
        RenderWorldPasses(lpDispatchThreadInputBuffer, &lDispatchContext);
        lRenderStage.Next(RENDER_PARTICLES);

        // ==========================================================================================
        // THE CORONA PASS (coronas step 1, 2026-08-17).
        //
        // CONSOLE POSITION: BrnRendererModule::Render @0x8240BFA8 calls
        // BrnCoronaManager::Render(&mCoronaManager, whiteLevel) gated on the module's own
        // mbRenderCoronas switch, bracketed by the CgsDev::PerfMonCpu/PerfMonGpu pair whose monitor
        // is registered as "DT: Render coronas" (mCpuMonitors.miRenderCoronas /
        // mGpuMonitors.miCoronas) -- AFTER the transparent car pass and BEFORE the particles /
        // post-fx composite. On this build that position is HERE: RenderWorldPasses ends with the
        // transparent passes, and ResolveMSAA + the composite follow immediately below. The coronas
        // are therefore drawn INTO the scene target, additively, and the post-fx grade sees them --
        // which is what makes a flare bloom.
        //
        // (The PerfMon bracket is not reproduced: every id in mGpuMonitors is 0 on this build
        // because nothing calls PerfMonGpu::AddMonitor -- this file's own banner at :250 says so --
        // so a bracket here would time one monitor id shared with every other pass.)
        //
        // THE SWITCH IS THE CONSOLE'S OWN. mbRenderCoronas already exists (BrnRendererModule.h:733,
        // set true by ConstructRenderSwitches at :832) -- no new global is minted (AGENTS.md rule
        // 3). The user-facing knob is config.ini [Settings] Coronas, which BrnMain seeds into it.
        // ==========================================================================================
        // (lbSceneBracketOpen: the console's only gate is mbRenderCoronas because its scene target
        // always exists; here the additive pass must not land in whatever target is bound when the
        // anti-alias bracket did not open.)
        if (lbSceneBracketOpen && mbRenderCoronas && EnsureCoronaManagerBringUp(mCoronaManager))
        {
            mCoronaManager.Render(lfFrameWhiteLevel);
        }

        // ==========================================================================================
        // THE FULL-RES PARTICLE PASS -- ParticleModule::RenderFullResParticles @0x8229AFD0.
        // This is the TYRE MARKS (and, when they land, the debris / sparks / Lion).
        //
        // CONSOLE POSITION, exactly: Render @0x8240BFA8 pseudocode :887 draws the coronas, :894
        // waits on the particle render job, :899 calls RenderFullResParticles -- then the 2D
        // immediate-mode dispatches, then :920 ResolveMSAA. So it belongs HERE: after the corona
        // pass, still inside the scene bracket, before the resolve. Drawn into the scene target,
        // so the post-fx grade sees it -- which is what the console does too.
        //
        // HOW THE CONSOLE REACHES THE MODULE (asm/pseudocode :444-448):
        //     v52 = sub_8227F640(dispatchThreadInputBuffer);   // GetParticleRenderData()
        //     v53 = *(_DWORD *)v52;                            // renderData->mpParticleModule
        //     ... RenderFullResParticles(v53, v52);
        // i.e. the module pointer is the record's FIRST WORD -- ParticleModule::Construct writes
        // `mRenderData.mpParticleModule = this` (+0x8E00) and GenerateRenderRequests publishes the
        // whole record. That is why no renderer-side module handle is needed, and why the pointer
        // is trustworthy exactly when a real producer has run.
        //
        // THE GATE IS THE CONSOLE'S OWN v295 (:442): mbRenderParticles && the dispatch buffer's
        // CalibrationUnfriendlyEnablePostFx && !GetIsStalled(). The same three reads already drive
        // lbEffectsAllowed / the sun corona in this function.
        //
        // ⚠ THE NULL TEST ON mpParticleModule IS NOT INVENTED DEFENSIVE DRESSING, AND IT IS
        // TEMPORARY. Two producers can stamp this record on this build: the real
        // ParticleModule::GenerateRenderRequests (mpParticleModule = the module) and the older PC
        // bring-up stand-in BrnParticle::PCBringUpProduceParticleRenderData, which deliberately
        // leaves that word NULL (its own banner says so). Until the bring-up producer is deleted,
        // a frame stamped by it would have the renderer dereference NULL. DELETE-WHEN
        // PCBringUpProduceParticleRenderData is gone: the console has no such test.
        // ==========================================================================================
        // ⭐ WHICH ARM THE LION PASS TAKES, published ONCE for this frame before either arm runs.
        // The console has no such publish: both its arms always exist and mbIsInJunkyard alone
        // selects. On PC the quarter-res half is a lazily built chain (pool slot 9 + the four blit
        // programs) that can legitimately be absent, and with the console's gate restored an absent
        // buffer would mean the Lion dispatch draws NOWHERE. Reading it once, here, is what stops
        // the two arms from disagreeing inside a frame. See
        // BrnParticle::ParticleModule::PCBringUpSetQuarterResRouting.
        const bool lbQuarterResParticles =
            lbSceneBracketOpen && EnsureQuarterResParticleChain(mAllocatedRenderTargets);
        BrnParticle::ParticleModule::PCBringUpSetQuarterResRouting(lbQuarterResParticles);

        if (lbSceneBracketOpen && mbRenderParticles && lpDispatchThreadInputBuffer != 0)
        {
            const BrnParticle::ParticleModule::ParticleRenderData* lpFullResRenderData =
                GetPublishedParticleRenderDataPC();
            if (lpFullResRenderData != 0 && lpFullResRenderData->mpParticleModule != 0
                && lpDispatchThreadInputBuffer->GetCalibrationUnfriendlyEnablePostFx()
                && !lpDispatchThreadInputBuffer->GetIsStalled())
            {
                // [trailpass] FLAG PC bring-up diagnostic -- the tyre-mark wave's render-side
                // witness. Printed on the FIRST frame this pass runs and then whenever the trail
                // bit flips, so the log says on which frames a mark could be drawn at all.
                // DELETE with the tyre-mark bring-up.
                {
                    static s32 siLastTrailBit = -1;
                    const s32 liTrailBit = ((lpFullResRenderData->muFlags
                        & BrnParticle::ParticleModule::ParticleRenderData::eRenderDataFlagRenderTrails) != 0)
                        ? 1 : 0;
                    if (liTrailBit != siLastTrailBit)
                    {
                        siLastTrailBit = liTrailBit;
                        char lacMsg[192];
                        std::snprintf(lacMsg, sizeof(lacMsg),
                            "[trailpass] RenderFullResParticles live: trailBit=%d flags=0x%04X "
                            "white=%.3f frame=%u\n",
                            liTrailBit, static_cast<unsigned>(lpFullResRenderData->muFlags),
                            lpFullResRenderData->mfWhiteLevel, lpFullResRenderData->muCurrentFrame);
                        CgsDev::Log::WriteToLog(lacMsg);
                    }
                }

                lpFullResRenderData->mpParticleModule->RenderFullResParticles(lpFullResRenderData);
            }
        }
    }

#if BRN_ANTIALIAS_BRACKET_AVAILABLE
    lRenderStage.Next(RENDER_COMPOSITE);
    if (lbSceneBracketOpen)
    {
        // ---- THE CARS-vs-WORLD MOTION-BLUR MASK, CARRIED OUT OF THE STENCIL (step 11) -------
        // The console's mask IS the stencil buffer, and both halves of the producer are already
        // live on PC: BeginRenderAntiAliased clears stencil to mu8WorldBlurStencil and
        // RenderWorldPasses' two force-stencil windows REPLACE it with mu8CarsBlurStencil over
        // the car mesh lists (see the block in RenderWorldPasses above). Its CONSUMER cannot be:
        // D3D9 has no way to sample a depth-stencil surface's stencil plane, which is why the
        // composite used a hard-coded 1.0 until this wave.
        //
        // So the mask is COPIED into the scene target's ALPHA LANE here -- with the scene target
        // and its own D24S8 depth-stencil still bound, using the stencil operation D3D9 DOES
        // have (the TEST) -- and the composite reads `tex2D(SamplerSource, uv).a`. The resolve
        // below carries all four lanes out, so this must run BEFORE it, and after every world
        // pass, which is exactly where the console's own stencil content is final.
        //
        // GATED ON mMotionBlurData.mbIsActive, the same bool the console's force windows are
        // gated on: on a frame with no blur request the composite picks a non-blur permutation,
        // never samples the lane, and this costs nothing at all.
        if (lPostFxFrameBytes.mbMotionBlurActive)
        {
            renderengine::PCStampMotionBlurMask(lPostFxFrameBytes.mu8CarsBlurStencil,
                                                lPostFxFrameBytes.mu8WorldBlurStencil);
        }

        // ---- X360 Render:725+ -- CLOSE THE SCENE PASS (call @0x8240D5BC). --------------------
        // Resolve the scene out of the anti-alias buffer into the down-sample buffer AND clear the
        // anti-alias buffer behind the copy. That clear is the whole reason this call cannot be
        // skipped: on the untiled path it is the ONLY clear in the bracket, because
        // BeginRenderAntiAliased's untiled branch (0x823FFB90-0x823FFBD8) deliberately clears
        // nothing -- the previous frame's resolve is what left the surface clean. Drop it and the
        // scene's DEPTH is never cleared again.
        // ARTIST 8240D520..D538: world immediate buffers and RacePosition
        // share the textured3D renderer before the scene resolve/post-fx.
        if (mbRenderWorldImmediateMode && mbIm3dRendererConstructedPC)
        {
            mIm3dRenderBuffer.Dispatch(&mIm3dRenderer);
            mIm3dBufferRacePosition.Dispatch(&mIm3dRenderer);
        }
        // ARTIST 8240D57C..D59C: debug3D follows world immediate mode,
        // independently of its switch, before the scene resolve.
        if (mbIm3dRendererConstructedPC
            && (!mbDispatchThreadTakeScreenshot || mbCaptureOverlaysInScreenshot))
            mIm3dDebugRenderBuffer.Dispatch(&mIm3dRenderer);
        ResolveMSAA(lfFrameWhiteLevel, luSceneStencilClearValue);

        // ==========================================================================================
        // THE SUN CORONA (coronas step 2, 2026-08-18).
        //
        // CONSOLE POSITION, exactly: Render @0x8240BFA8 calls ResolveMSAA @0x8240D5BC, then
        // ComputeSunCoronaVisibility @0x8240D5D0, then (after BeginQuarterResBuffer and the
        // quarter-res particles) BrnSunCorona::RenderOccludedFlare @0x8240D688 -- both arms gated on
        // the SAME byte, `lbz r25, var_CC8` @0x8240D5C0, which was loaded at 0x8240C41C from
        // DispatchThreadInputBuffer::GetCalibrationUnfriendlyEnablePostFx(). That is the console's
        // own gate and it is the reason the flare disappears on the calibration screen; the same
        // read already drives lbTintEffectsAllowed / lbEffectsAllowed elsewhere in this function.
        //
        // AFTER ResolveMSAA IS LOAD-BEARING: GenerateOcclusionBuffer samples the DOWN-SAMPLE
        // buffer's depth TEXTURE, and ResolveMSAA is what puts this frame's depth there.
        //
        // ⚠ THE FLARE'S TARGET IS A DECLARED PC DEVIATION. The console draws it into the QUARTER-RES
        // particle buffer that BrnRendererModule::BeginQuarterResBuffer @0x82408C38 opens, and the
        // post-fx composite adds that buffer to the frame. On PC neither exists: BeginQuarterResBuffer
        // is declaration-only and pool slot 9 is never created (only ANTI_ALIAS + DOWN_SAMPLE are).
        // So the flare is drawn into the DOWN-SAMPLE buffer -- the surface ResolveMSAA has just
        // filled and the surface the composite reads -- i.e. the same colour bus one stage earlier.
        // The blend is the console's own additive RGB either way. DELETE-WHEN the quarter-res
        // buffer and BeginQuarterResBuffer land.
        //
        // ⚠ AND THE SCENE TARGET IS RE-BOUND UNCONDITIONALLY inside this gate, not only when the
        // flare draws. CgsRenderTarget::Begin on the 1x1 occlusion buffer leaves that 1x1 surface
        // (and a 1x1 viewport) bound -- End is a documented no-op on this backend -- so the
        // condition that authorised the occlusion pass has to be the condition that hands the scene
        // target back, or the composite below would run with a one-pixel target bound.
        // ==========================================================================================
        const bool lbSunCoronaAllowed = (lpDispatchThreadInputBuffer != 0)
            ? lpDispatchThreadInputBuffer->GetCalibrationUnfriendlyEnablePostFx()
            : false;
        if (lbSunCoronaAllowed && EnsureSunCoronaBringUp(mAllocatedRenderTargets, mSunCorona))
        {
            ComputeSunCoronaVisibility();

            CgsRenderTarget* const lpSceneTarget = mAllocatedRenderTargets.GetDownSampleBuffer();
            if (lpSceneTarget != 0 && lpSceneTarget->GetRenderTarget() != 0)
            {
                lpSceneTarget->Begin();

                // The colour and the white level are the console's own arguments, read off the
                // SAME internal frame ComputeSunCoronaVisibility used (asm 0x8240D634-0x8240D664:
                // `lbz r11, 0xAD0` -> the internal index, `lfs f30, 0x7A8(r11)` == frame+0x318 ==
                // mfWhiteLevel, and BrnShaderConstantsFrame::GetKeyLightColour on the same frame).
                //
                // The last two arguments are the console's NO-OVERRIDE values: it reads them from
                // BrnGraphics::DebugComponent (this+0xCA50 mfSunFlareOverrideBrightness and
                // this+0xCA54 mbSunFlareOverrideBrightness -- DWARF
                // BrnRendererModuleDebugComponent.h:86/87, exposed as GetOverriddenSunBrightness() /
                // GetOverrideSunBrightness()), which is a DEBUG-MENU toggle and is an EMPTY
                // PLACEHOLDER struct in this tree (BrnRendererModule.h:200). Passing (0.0f, false)
                // is the console's own default, not an invention: with the flag false the float is
                // never read and BrnSunCorona::RenderOccludedFlare uses mfSunBrightness.
                // DELETE-WHEN BrnGraphics::DebugComponent is reconstructed.
                const BrnShaderConstantsFrame& lrSunFrame =
                    maShaderConstantsFrames[mu8ShaderConstantsFrameInternal];
                mSunCorona.RenderOccludedFlare(mAllocatedRenderTargets.GetSunCoronaBuffer(),
                                               lrSunFrame.GetKeyLightColour(),
                                               lrSunFrame.GetWhiteLevel(),
                                               0.0f, false);
            }
        }

        // ==========================================================================================
        // ⭐⭐ THE QUARTER-RES PARTICLE BRACKET -- BeginQuarterResBuffer @0x82408C38,
        // ParticleModule::RenderQuarterResParticles @0x82294A20, EndRenderAntiAliased @0x82408B00.
        //
        // CONSOLE POSITION, exactly: Render @0x8240BFA8 calls ResolveMSAA, then
        // ComputeSunCoronaVisibility, then BeginQuarterResBuffer, then the quarter-res particles,
        // then BrnSunCorona::RenderOccludedFlare @0x8240D688, then EndRenderAntiAliased -- so the
        // bracket sits HERE, after the resolve and the corona visibility test and before the
        // post-fx composite reads the down-sample buffer.
        //
        // WHAT IT IS FOR, in one line: the console accumulates particles in a CLEARED, SEPARATE
        // buffer and composites them OVER the scene with a (1 - accumulatedAlpha) term, so a
        // saturating plume REPLACES its background instead of ADDING to it. This backend added
        // them straight onto the scene, which is what left the boost flame with a white core after
        // the white-level fix in a4c7670b.
        //
        // ⚠ THE FLARE STAYS WHERE IT IS, and that is a MEASURED equivalence rather than an
        // omission. The console draws RenderOccludedFlare INTO the particle buffer, between the
        // particles and EndRenderAntiAliased; the flare's blend is
        // eFactoryBlendState_Transparent_AdditiveRGB_NoAlphaTest_DestRGB -- additive RGB that
        // writes NO alpha -- so in the particle buffer it contributes flareRGB with the alpha lane
        // untouched, and the composite then evaluates scene*(1 - a) + particleRGB + flareRGB. Drawn
        // one stage earlier into the down-sample buffer, as it is above, the composite evaluates
        // (scene + flareRGB)*(1 - a) + particleRGB. The two differ only where the plume and the sun
        // flare OVERLAP -- and the only cost of moving it would be redrawing it at half resolution.
        // Stated rather than silently deviated from; the earlier "DELETE-WHEN the quarter-res
        // buffer lands" note on that block is answered by this paragraph.
        //
        // ⚠ THE PARTICLE GATE IS THE FULL-RES ARM'S, verbatim: same three console reads, same
        // temporary null test on mpParticleModule, same DELETE-WHEN. It is repeated rather than
        // hoisted because the console repeats it -- the two arms are two independent call sites in
        // Render, each with its own guard.
        // ==========================================================================================
        // The SAME latch published before the full-res arm above -- re-read here rather than
        // carried in a local, because that arm sits in the pre-bracket scope. It is a pure
        // query over what actually exists, so the two reads cannot disagree inside a frame.
        if (mAllocatedRenderTargets.PCBringUpParticleCompositeChainReady())
        {
            BeginQuarterResBuffer();

            if (mbRenderParticles && lpDispatchThreadInputBuffer != 0)
            {
                const BrnParticle::ParticleModule::ParticleRenderData* lpQuarterResRenderData =
                    GetPublishedParticleRenderDataPC();
                if (lpQuarterResRenderData != 0 && lpQuarterResRenderData->mpParticleModule != 0
                    && lpDispatchThreadInputBuffer->GetCalibrationUnfriendlyEnablePostFx()
                    && !lpDispatchThreadInputBuffer->GetIsStalled())
                {
                    lpQuarterResRenderData->mpParticleModule
                        ->RenderQuarterResParticles(lpQuarterResRenderData);
                }
            }

            EndRenderAntiAliased();
        }

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
        // m_enabledFx bits and its four effect state blocks. Its statements sit above in this file,
        // in BrnRendererApplyEffectsFrameToPostFx; it is called from here, at the console's
        // position, with the console's gate.
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
        // mbCalibrationUnfriendlyEnablePostFx = true (BrnDispatchThreadInputBuffer.cpp:232).
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

            // MotionBlurState::Update @0x823F8490 -- LIVE (post-fx step 9). Its body, its two matrix
            // arguments' layout and now its PRODUCER all exist:
            //   * the body landed in BrnPostFxShader.cpp with the rung-7 producers wave;
            //   * DispatchThreadInputBuffer::ParticleRenderData has its real DWARF layout
            //     (ParticleModule.h:589-606), so the arguments are reached BY NAME;
            //   * the record is now stamped every frame by the named PC bring-up producer
            //     BrnParticle::PCBringUpProduceParticleRenderData, called from
            //     BrnGameModule::DoDispatch on the buffer this Render read-locks after the swap.
            // (The comment that stood here -- "BLOCKED ... its body does not exist" -- had been
            // stale since rung 7.)
            //
            // THE READ LOCK IS ALREADY HELD FOR THE WHOLE FRAME: Render takes it at the top
            // (`lpDispatchThreadInputBuffer->LockForRead()`, the console's own frame-long lock) and
            // releases it at both exits, so the CONST GetParticleRenderData overload -- which
            // asserts "Not locked for reading" -- is legal here. This is the same accessor the
            // console's Render calls (`bl sub_8227F640` @0x8240C45C).
            //
            // THE LATCH IS NOT DEFENSIVE DRESSING. DispatchThreadInputBuffer::Construct
            // deliberately does NOT clear mParticleRenderData (faithfully -- the console does not
            // either) and CreateIOBuffer<T> has not zero-filled since the 2026-08-15 perf wave, so
            // before the first producer run the payload is UNINITIALISED bytes. A garbage view
            // matrix goes through four inverses in MotionBlurState::Update's zero-time-step arm and
            // the resulting NaN would reach a tex2Dgrad gradient. So the pointer is handed over only
            // once the producer has actually stamped a record -- THIS buffer instance's record: the
            // buffer is double-buffered and the producer runs only on DoDispatch frames, so the
            // latch is per instance (step-9 verify).
            // DELETE the latch (not the call) when the real ParticleModule/EffectsModule producer
            // runs -- from then on the record is written before any reader, as on the console.
            const BrnParticle::ParticleModule::ParticleRenderData* lpParticleRenderData = 0;
            if (lpDispatchThreadInputBuffer != 0
                && maShaderConstantsFrameValidPC[mu8ShaderConstantsFrameInternal])
            {
                lpParticleRenderData = GetPublishedParticleRenderDataPC();
            }
            (void)BrnRendererUpdatePostFxMotionBlur(lpEffectsArbitrator, lpParticleRenderData);
        }

        // [FLAG PC bring-up diagnostic] the sampled [postfx-fx] line -- six lines, 500 frames apart,
        // proving base frame -> Eval* -> BrnPostFx. Sits beside the [postfx-composite] diagnostics in
        // BrnPostFx.cpp. DELETE with the bring-up.
        // FLAG PC-platform leaf: bounded observation of the actual internal
        // camera constants and tint at the final-composite boundary. The first
        // real slow-motion episode arms the same window as the frame capture.
        // No camera, effect, binding or render-state value is changed.
        {
            static const bool sbRepairFrameDiag = std::getenv("BRN_REPAIR_FRAME_DIAG") != nullptr;
            static u32 suRepairFrameReports = 0;
            static u32 suRepairLastPresent = ~0u;
            if (sbRepairFrameDiag && BrnDiag::gFilmLatch.muSlomoLatched != 0u &&
                suRepairFrameReports < 900u)
            {
                const u32 luPresent = renderengine::GetDispatchPresentCountPC();
                if (luPresent != suRepairLastPresent)
                {
                    suRepairLastPresent = luPresent;
                    ++suRepairFrameReports;
                    const BrnShaderConstantsFrame& lrFrame = maShaderConstantsFrames[mu8ShaderConstantsFrameInternal];
                    const Vector3 lvEye = lrFrame.GetViewPosition();
                    const Matrix44 lmVP = lrFrame.GetViewProjectionMatrix();
                    char lacRepairFrame[768];
                    std::snprintf(lacRepairFrame, sizeof(lacRepairFrame),
                        "[repair-frame] present=%u postfx=%d allowed=%d tint=%.9g,%.9g,%.9g,%.9g eye=%.9g,%.9g,%.9g vp=%.9g,%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g;%.9g,%.9g,%.9g,%.9g\n",
                        luPresent, static_cast<int>(mbRenderPostFX), static_cast<int>(lbEffectsAllowed),
                        lafTint2dColour[0], lafTint2dColour[1], lafTint2dColour[2], lafTint2dColour[3],
                        lvEye.x, lvEye.y, lvEye.z,
                        lmVP.xAxis.x, lmVP.xAxis.y, lmVP.xAxis.z, lmVP.xAxis.w,
                        lmVP.yAxis.x, lmVP.yAxis.y, lmVP.yAxis.z, lmVP.yAxis.w,
                        lmVP.zAxis.x, lmVP.zAxis.y, lmVP.zAxis.z, lmVP.zAxis.w,
                        lmVP.wAxis.x, lmVP.wAxis.y, lmVP.wAxis.z, lmVP.wAxis.w);
                    CgsDev::Log::WriteToLog(lacRepairFrame);
                }
            }
        }
        BrnRendererLogPostFxEffectState();

        const s32 liBrightnessSetting = (lpDispatchThreadInputBuffer != 0)
            ? lpDispatchThreadInputBuffer->GetBrightness()
            : KI_DEFAULT_CALIBRATION_SETTING;
        const s32 liContrastSetting = (lpDispatchThreadInputBuffer != 0)
            ? lpDispatchThreadInputBuffer->GetContrast()
            : KI_DEFAULT_CALIBRATION_SETTING;

        // The tint / motion-blur arguments are the console's own effects-frame pair: v1 == the tint
        // vector evaluated just above, r7 == the layer-0 internal frame's mMotionBlurData.mbIsActive
        // read at the top of the bracket (lPostFxFrameBytes). (All twelve composite permutations
        // adopt since rung 6, so a true motion-blur byte selects a REAL program.)
        //
        // ---- X360 Render @0x8240DD50-0x8240DE08 -- the CALIBRATION RAMP as the composite's override
        // source. `mr r30, r18` (r18 == 0 since @0x8240CEE8) seeds the Texture* null; then, ONLY when
        // GetCalibrationUnfriendlyEnablePostFx() is FALSE (`lbz r25, var_CC8` == the byte read at
        // 0x8240C41C; `bne` skips everything when the post-fx are allowed -- i.e. the ramp is on
        // screen exactly when the calibration screen has switched the effects off):
        //   * `bl GetCalibrationTextureHandle` into var_CB0; if it != NULLResourceHandle
        //     (qword_82FAFF20, both words) -> `stdx r9, r31, 0xC920`: LATCH it into
        //     mCalibrationTextureHandle (DWARF BrnRendererModule.h:692, this+0xC920);
        //   * if mCalibrationTextureHandle != NULLResourceHandle -> `lwz r11, 0(r10)` /
        //     `lwz r30, 0(r11)`: the SmallResource double-deref (handle.mpResourceMemory's first word
        //     is the main-memory Texture*, the same idiom ColourCalibrationScreen::Update uses).
        // The latch is why the card survives a frame the screen did not re-post on: the member is
        // never cleared while the effects stay disabled, and it is simply not consulted once they
        // are re-enabled. `stw r30, var_CD4` @0x8240DE28 is the stack argument BrnPostFx::Render
        // reads as its override source (BrnPostFx.cpp step 5).
        // (`mCalibrationTextureHandle` is seeded null in Construct, as @0x8240BF74-BF88.)
        renderengine::Texture* lpCalibrationTexture = 0;
        if (!lbEffectsAllowed && lpDispatchThreadInputBuffer != 0)
        {
            const CgsResource::ResourceHandle lhCalibrationTexture =
                lpDispatchThreadInputBuffer->GetCalibrationTextureHandle();
            if (lhCalibrationTexture != CgsResource::NULLResourceHandle)
            {
                mCalibrationTextureHandle = lhCalibrationTexture;
            }
            if (mCalibrationTextureHandle != CgsResource::NULLResourceHandle)
            {
                lpCalibrationTexture = *reinterpret_cast<renderengine::Texture* const*>(
                    mCalibrationTextureHandle.mpResourceMemory);
            }
        }

        // [FLAG PC bring-up diagnostic] the override source is observable: one line per transition.
        {
            static bool sbCalibrationOverrideOn = false;
            const bool lbOn = (lpCalibrationTexture != 0);
            if (lbOn != sbCalibrationOverrideOn)
            {
                sbCalibrationOverrideOn = lbOn;
                CgsDev::Log::WriteToLog(lbOn
                    ? "[calib] composite override source ON (calibration ramp texture)\n"
                    : "[calib] composite override source OFF\n");
            }
        }

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
                lPostFxFrameBytes.mbMotionBlurActive,
                lpCalibrationTexture);

        // [DIAG] NOT IN THE X360 BINARY -- issue #30: on a black present, every input of the composite.
        {
            static u32 suBlackCompositePrinted = 0u;
            if (renderengine::gbDiagLastPresentBlack && suBlackCompositePrinted < 64u && CgsDev::Log::gpDebugPrint != 0)
            {
                ++suBlackCompositePrinted;
                *CgsDev::Log::gpDebugPrint
                    << "[composite-diag] BLACK-PRESENT present=" << renderengine::guPresentCount
                    << " composited=" << (lbComposited ? 1 : 0) << " renderPostFX=" << (mbRenderPostFX ? 1 : 0)
                    << " effectsAllowed=" << (lbEffectsAllowed ? 1 : 0)
                    << " tint2d=(" << lafTint2dColour[0] << "," << lafTint2dColour[1] << "," << lafTint2dColour[2] << "," << lafTint2dColour[3] << ")"
                    << " whiteLevel=" << lfFrameWhiteLevel << " brightness=" << liBrightnessSetting << " contrast=" << liContrastSetting
                    << " motionBlur=" << (lPostFxFrameBytes.mbMotionBlurActive ? 1 : 0)
                    << " calibTex=" << (lpCalibrationTexture != 0 ? 1 : 0) << "\n";
            }
        }
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
    lRenderStage.Next(RENDER_GUI);
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
        lRenderStage.Next(RENDER_PRESENT);
        renderengine::Device::ShowPixelBuffer();
        return;
    }

    // ARTIST 8240E000..E054: one HUD gate and the original command order.
    // GUI and movie commands share the renderer-owned Im2d buffer.
    if (mbRenderHudImmediateMode)
    {
        mLoadingScreenRenderer.RenderBackground(&mIm2dRenderer);
        mIm2dRenderBuffer.Dispatch(&mIm2dRenderer);
        if (mbIm3dRendererConstructedPC)
            mIm3dBufferMenusAndHud.Dispatch(&mIm3dRenderer);
        mLoadingScreenRenderer.RenderForeground(&mIm2dRenderer);
    }

    // ARTIST 8240E158..E190 consumes the already published debug bank.
    if (mbRenderHudImmediateMode
        && (!mbDispatchThreadTakeScreenshot || mbCaptureOverlaysInScreenshot))
        mIm2dDebugRenderBuffer.Dispatch(&mIm2dRenderer);

    // The three per-thread monitor squares (X360 RenderThreeThreadMonitors). The real per-thread
    // "running in real time" flags need the threading system (deferred), so they are derived here from
    // the present-to-present frame time - matching the observed behaviour (green at framerate, reddening
    // as the game/CPU slows). The X360 gates this on a debug-display flag.
    {
        const u64 lu64Now  = CgsSystem::GetSystemTimerBaseTime64();
        const u64 lu64Freq = CgsSystem::GetSystemTimerFrequency64();
        f32 lfFrameMs = 0.0f;
        if (gbMonitorTickValid && lu64Freq != 0u)
            lfFrameMs = static_cast<f32>(static_cast<double>(lu64Now - gu64LastMonitorTick) * 1000.0 / static_cast<double>(lu64Freq));
        gu64LastMonitorTick = lu64Now;
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

    lRenderStage.Next(RENDER_PRESENT);
    renderengine::Device::ShowPixelBuffer();
}

// FLAG PC-platform leaf: the modal loop consumes its own bank on the fenced
// render owner. The game and debug producers retain their interrupted commands.
void BrnRendererModule::RenderAssert(const CgsDev::Assert::AssertData* lpAssertData)
{
    if (!mIm2dAssertRenderBufferPC.IsPreparedPC())
        return;
    if (CgsDev::DebugManager* lpDebug = CgsDev::DebugManager::GetInstance())
    {
        mIm2dAssertRenderBufferPC.Clear();
        lpDebug->RenderAssertToBufferPC(lpAssertData, &mIm2dAssertRenderBufferPC);
        mIm2dAssertRenderBufferPC.Swap();
        mIm2dAssertRenderBufferPC.Clear();
        mIm2dAssertRenderBufferPC.Dispatch(&mAssertIm2dRendererPC);
    }
}

// The corona manager's published (swap-current) submission interface. Bodied 2026-08-17
// (boot audit F-P2-4).
//
// The header called this "Not X360-attested for this TU" and I left it unpublished when
// BrnRendererModule::Update first landed rather than guess. That caution expired with the
// extraction itself: Update @0x824060F0-108 forms the argument as
//     C = corona; slot = C[0x130]; interface = C + slot*0x70 + 0x50
// which names every part of it -- an array based at manager+0x50 with a 0x70 stride, indexed
// by a byte at manager+0x130. This class has exactly one array of that shape,
// mSubmissionInterface[2], and exactly one index byte beside it, mu8SubmissionSwapIndex. So
// the expression IS attested -- by its caller, which is where an un-bodied accessor's shape
// usually lives.
//
// MOVED HOME 2026-08-17 (coronas step 1). The paragraph that stood here said "defined from THIS TU
// rather than BrnCoronaManager.cpp because that file is not on the build list ... move it home when
// the corona TU is mounted for its own sake". That is exactly what this wave does:
// GameSource/Graphics/BrnCoronaManager.cpp is now on tools/build/build_game_exe.bat and defines
// GetSubmissionInterface (together with Construct / Clear / Swap / Render / SetCameraInfo, all of
// which the same caller needs). Keeping this copy would be LNK2005, not a duplicate comment.
// The reading above is unchanged and still correct -- see the body in BrnCoronaManager.cpp.

// ============================================================================================
// @ 0x82405E28 -- BrnRendererModule::Update.  RECONSTRUCTED 2026-08-17 (boot audit F-P2-4).
//
// The renderer's per-pass publication into the RendererIO buffer pair.  BrnGameModule::
// GamePrepare's not-done tail creates the pair, calls this, and reads the loading-screen
// allocator back out; the console runs it on EVERY not-done GamePrepare pass and again from
// the loading-screen spine (F-P5-3) and RenderGUI (F-P6-18).
//
// EVERY SOURCE IN THE CONSOLE BODY IS A RAW OFFSET INTO THE RENDERER, and this build's layout
// is an x64 reconstruction, so each one is resolved to its committed member BY TYPE AND ROLE
// (the full offset->name derivation is in progress/boot_audit/phases/P2b_renderermodule_update.md):
//
//   this+0xAE8/0x1090/0x1270/0x1550/0x1598/0x172C/0x1774 -> the seven Im2d/Im3d buffers, which
//     are the only members of those types in the class and appear in the console's order;
//   the three effects families at E+0/+4/+8 with the shared slot byte at E+0xD -> the
//     arbitrator's own GetExternalEffectsFrame(layer, slot).  The console forms
//     `496 * (2*slot + internal)`, and EffectsArbitrator.h:141 documents that same expression
//     as `mapaEffectsFrames[layer][slot][external]` -- so the console's loop index IS the slot
//     and its `s` byte IS the external index.  Layer slot counts 1/4/2 match the loop bounds;
//   this[0xAD1] + slot*0x320 + 0x490 -> maShaderConstantsFrames[mu8ShaderConstantsFrameExternal];
//   the blobby/corona slot walks -> the managers' own GetExternalBuffer() / GetSubmissionInterface();
//   this+0xC40C -> mRenderSwitches;  this+0xC8FC -> mReusableLoadingScreenAllocator.
// ============================================================================================
void BrnRendererModule::Update(CgsModule::IOBufferStack* /*lpUpdateInputStack*/,
                               CgsModule::IOBufferStack* /*lpUpdateOutputStack*/,
                               RendererIO::InputBuffer*  lpInput,
                               RendererIO::OutputBuffer* lpOutput)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput");   // X360 :0x11A7
    CGS_ASSERT(lpInput  != 0, "lpInput");    // X360 :0x11A8

    lpInput->LockForRead();
    lpOutput->LockForWrite();

    // @0x82405EA4-B0 -- the camera crosses first.
    {
        const BrnDirector::Camera::Camera& lrCamera = lpInput->GetBrnCamera();
        lpOutput->SetBrnCamera(lrCamera);
        // [cam-flags] BRN_CAM_INPUT_DIAG: the state flags as handed to the world dispatch.
        static const bool sbCamDiag = (getenv("BRN_CAM_INPUT_DIAG") != 0);
        static u32 suCamDiagCalls = 0;
        if (sbCamDiag && (suCamDiagCalls++ % 60u) == 0 && CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "[cam-flags] renderer output flags " << lrCamera.mState_uFlags << "\n";

        // ARTIST 82405EC4..5FA0: only corona setup has the FOV > 0.1 gate.
        // The renderer/world whole-camera copy above always occurs.
        if (lrCamera.GetFOV() > 0.1f)
        {
            CgsGraphics::Camera lCamera;
            lrCamera.CopyToCgsCamera(&lCamera);
            const f32 lfOotHalfFovH = lCamera.maProjectionScalars[1];
            const f32 lfOotHalfFovV = lCamera.maProjectionScalars[4];
            const f32 lfClamped = lfOotHalfFovH >= 1.0f ? lfOotHalfFovH : 1.0f;
            const Vector4 lViewXyScale = {
                lfClamped, (lfOotHalfFovV / lfOotHalfFovH) * lfClamped, 0.0f, 0.0f };
            mCoronaManager.GetSubmissionInterface()->SetCameraInfo(
                lCamera.GetViewProjectionMatrix(), lrCamera.GetPosition(), lViewXyScale);
        }
        if (lrCamera.GetEffects().mbRequestingScreenshot)
            mbUpdateThreadTakeScreenshot = true;
    }

    // @0x82405EBC-C0 -- THE LATCH SOURCE.  The console lends the ADDRESS of its embedded
    // allocator; the game module stores that pointer at gm+0x9A0630 and FreeAll's it once per
    // world drive.  This one call is why the PC had to invent a static 512 KiB world frame
    // allocator (F-P6-12).
    lpOutput->SetReusableLoadingScreenAllocator(&mReusableLoadingScreenAllocator);

    lpInput->UnlockForRead();

    // ---- the publication block (@0x82405FD0-0x82406130) -------------------------------
    lpOutput->SetDispatchFrame(GetDispatchFrameForWrite());
    lpOutput->SetIm2dRenderBuffer(&mIm2dRenderBuffer);
    lpOutput->SetIm3dRenderBuffer(&mIm3dRenderBuffer);
    lpOutput->SetIm3dRenderBufferUntex(&mIm3dRenderBufferUntex);
    lpOutput->SetIm3dDebugRenderBuffer(&mIm3dDebugRenderBuffer);
    lpOutput->SetIm2dDebugRenderBuffer(&mIm2dDebugRenderBuffer);

    // The three effects families, in the console's order and with its loop bounds
    // (kau8SlotsPerEffectsLayer == {1, 4, 2}).
    lpOutput->SetBaseEffectsFrame(
        mEffectsArbitrator.GetExternalEffectsFrame(
            BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_BASE, 0));
    for (u8 lu8Slot = 0; lu8Slot < 4; ++lu8Slot)
    {
        lpOutput->SetWorldEffectsFrame(lu8Slot,
            mEffectsArbitrator.GetExternalEffectsFrame(
                BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_WORLD, lu8Slot));
    }
    for (u8 lu8Slot = 0; lu8Slot < 2; ++lu8Slot)
    {
        lpOutput->SetFXEventsEffectsFrame(lu8Slot,
            mEffectsArbitrator.GetExternalEffectsFrame(
                BrnGraphics::EffectsArbitrator::KU_EFFECTS_LAYER_FX_EVENTS, lu8Slot));
    }

    lpOutput->SetShaderConstantsFrame(&maShaderConstantsFrames[mu8ShaderConstantsFrameExternal]);
    lpOutput->SetBlobbyShadowBuffer(mBlobbyShadowManager.GetExternalBuffer());
    // @0x824060F0-108 -- the corona submission interface. (I left this unpublished on the
    // first pass because GetSubmissionInterface was declared-only; re-testing the premise
    // showed the accessor's shape is fully named by THIS caller -- `C + C[0x130]*0x70 + 0x50`
    // is mSubmissionInterface[mu8SubmissionSwapIndex], the class's only 0x70-stride array and
    // its only index byte. Bodied in BrnCoronaManager.cpp.)
    lpOutput->SetCoronaSubmissionInterface(mCoronaManager.GetSubmissionInterface());
    lpOutput->SetIm3dRenderBufferRacePosition(&mIm3dBufferRacePosition);
    lpOutput->SetIm3dRenderBufferMenusAndHud(&mIm3dBufferMenusAndHud);
    lpOutput->SetRenderSwitches(mRenderSwitches);

    // [FLAG] @0x82406134-F0 -- the console then gathers ELEVEN scattered words
    // (renderer +0xC938/+0xC940/+0xC948/+0xC9C0/+0xC954/+0xC958/+0xC9C8/+0xC9D0/+0xCA10/
    // +0xC9D4/+0xC9D8) into a 0x2C-byte stack record and memcpy's it to lpOutput+0x1B8.
    // Those eleven are NOT resolved to committed members yet -- they sit past mRenderSwitches
    // in a region this layout has not itemised, and guessing eleven member identities is
    // exactly the misattribution class this audit exists to remove (cf. F-P7-12). The record
    // is left unwritten rather than filled with plausible-looking values; the output buffer's
    // corresponding field keeps its Construct-time zeros. Tracked as the remainder of F-P2-4.

    lpOutput->UnlockForWrite();
}
