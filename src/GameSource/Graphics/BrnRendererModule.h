#pragma once

#include "types.hpp"

// Real loading-screen-path member types (Option B: these are reconstructed for real; the
// off-path gameplay-render subsystems below remain opaque storage until reached).
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderBuffer.h"  // CgsGraphics::Im2dRenderBuffer (canonical)
#include "GameSource/Game/BrnLoadingScreenRenderer.h"                // BrnGame::LoadingScreenRenderer
#include "GameSource/Game/BrnDispatchThreadInputBuffer.h"            // BrnGame::DispatchThreadInputBuffer (Render's input)
#include "GameSource/Graphics/BrnShaderConstantsFrame.h"             // BrnShaderConstantsFrame
#include "GameSource/Graphics/BrnEffectsArbitrator.h"                // BrnGraphics::EffectsArbitrator
#include "GameSource/Graphics/BrnSunCorona.h"                        // BrnSunCorona (mSunCorona, embedded by value)
#include "GameSource/Graphics/BrnCoronaManager.h"                    // BrnCoronaManager (mCoronaManager, embedded by value)
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"   // CgsModule::ModuleSingleBuffered (real base)

// EA::Jobs::Job is still an off-path placeholder (the renderer's sort/dispatch jobs do not run
// during the loading screen); reconstructed with the job system. The real EA::Thread::RWMutex
// now comes in via CgsDataBuffer.h (pulled by the real ModuleSingleBuffered base) - the former
// stub RWMutex was removed so the renderer/game modules share one real module base + mutex type.
namespace EA
{
namespace Jobs
{
class Job
{
public:
    Job(s32 liPriority = 0);
};
}
}

// The REAL dispatch-frame family (BufferedDispatchFrame / DispatchFrame /
// DispatchList / DispatchBin / DispatchObjectContext / the interpreter) -- the
// world render pass drives them, so the former local placeholders are gone.
#include "GameShared/GameClasses/Graphics/CgsBufferedDispatchFrame.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"

namespace CgsGraphics
{
// CgsGraphics::Im2dRenderBuffer is the real type (CgsImRenderBuffer.h, ImRenderBuffer<Basic2dColouredTexturedVertex>).
// CgsGraphics::Im2d is the real type (CgsIm2d.h) - the loading screen renders through it.

class Im2dUntex
{
};

class Im3dRenderBuffer
{
};

class Im3d
{
};

class Im3dRenderBufferUntex
{
};

class Im3dUntex
{
};

class Im3dZOnly
{
};

class OcclusionCullManager
{
};
}

// EffectsArbitrator is the real type (BrnEffectsArbitrator.h); Im3dSkyDome is the real
// BrnGraphics::Im3dSkyDome (BrnIm3d.h) now that the sky-dome draw path is mounted -- the
// ODR placeholder that used to stand here is gone.
#include "GameSource/Graphics/ImmediateMode/BrnIm3d.h"               // BrnGraphics::Im3dSkyDome

namespace BrnResource
{
class LinearResourceAllocator;
}

// RendererIO::RenderSwitches now comes from its DWARF home (BrnRendererModuleIO.h:68) -- the
// forward-slice copy this header carried was deleted per the consolidation FLAG there, when the
// world-module mount first co-included both spellings in one TU (BrnGameModule). The include also
// supplies the REAL BrnBlobbyShadowManager (see the deleted stub below).
#include "GameSource/Graphics/BrnRendererModuleIO.h"

namespace renderengine
{
class Texture;
class TextureState;
}

// BrnRendererModule::EndRenderPostFx @0x823F65B0 takes one of these. Pointer-only use in this
// header, so a forward declaration is the documented cascade-avoidance exception rather than an
// include of the post-fx SDK header. Class KEY checked against the definition, not guessed:
// `class RenderTarget` at
// SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h:126, inside
// `namespace rw` (:31) / `namespace graphics` (:33) / `namespace postfx` (:35).
namespace rw { namespace graphics { namespace postfx { class RenderTarget; } } }

// BrnRendererModule::PCBringUpSetCameraInput below takes the director's published camera record
// by pointer only, so this is the same documented cascade-avoidance forward declaration as the
// RenderTarget one above rather than an include of Camera.h (which would drag CameraEffects /
// DepthOfField / CameraState / CgsCamera into every TU that includes this header). Class KEY
// checked against the definition, not guessed: `struct alignas(16) Camera` at
// GameSource/Director/Camera/Camera.h:59, inside `namespace BrnDirector` (:45) / `namespace
// Camera` (:51). The alignas belongs to the definition, so it is correctly absent here.
namespace BrnDirector { namespace Camera { struct Camera; } }

// BrnRendererMemory is the real type now (GameSource/Graphics/BrnRendererMemory.h): it owns the
// renderer's render-target pool, and mAllocatedRenderTargets below is embedded BY VALUE, so the
// shadow pass -- which asks it for GetShadowMapBuffer(0) -- needs the complete layout, not the
// empty placeholder that used to stand here.
//
// ODR TRAP (the same one the BrnBlobbyShadowManager note below records): the real class is also a
// GLOBAL-namespace `struct BrnRendererMemory`, so the placeholder was not a distinct type that
// could coexist with it -- any TU that co-included both headers got a redefinition error. It is
// gone WITH the include added in the same edit, per the project rule.
//
// LAYOUT: this grows BrnRendererModule by sizeof(BrnRendererMemory) - 1 (and the sibling
// ShadowMapRenderManager change below by another 11), which in turn grows BrnGameModule (it embeds
// the renderer by value at BrnGameModule.hpp:614). Nothing pins either size -- there is no
// _AssertLayout / static_assert on sizeof for either class, and no member of either is reached by
// raw byte offset -- so the growth is inert. It is also the CORRECTION: the console object carries
// both sub-objects at full size, and the placeholders were understating it.
#include "GameSource/Graphics/BrnRendererMemory.h"

// BrnShaderConstantsFrame is the real type (BrnShaderConstantsFrame.h).

struct TextureStateParameters
{
};

struct Resource
{
};

// BrnBlobbyShadowManager is the real type now (GameSource/Graphics/BrnBlobbyShadowManager.h,
// included via BrnRendererModuleIO.h above). The empty stub that lived here ODR-clashed with the
// real class once the world-module mount co-included the world dispatch buffer in the game TU
// (same trap as the BrnGameModule.hpp module stubs; mBlobbyShadowManager below is by value, so
// the complete real type is required).

// BrnCoronaManager is the real type (GameSource/Graphics/BrnCoronaManager.h, included above) --
// mCoronaManager is embedded by value below, which needs the complete type, not a stub.

// The three render-state factories are the REAL classes now (gate-flip wave, 2026-08-15) -- the
// empty placeholder structs that stood here were an ODR fault against the real headers the post-fx
// TUs include, and they left every state table null: the console's BrnRendererModule::Construct
// @0x8240A778 constructs the three by-value members through vtbl[0] at 0x8240A950-0x8240A994
// (`this+0x3940 / +0x3944 / +0x3948`, r4 = mpGraphicsAllocator), and the post-fx composite pushes
// slots of those tables (saDepthStencilStates[1], saRasterizerStates[2]) -- with the tables null the
// push was a compare-then-skip and the composite quad drew under the world's back-face cull, i.e. not
// at all. Their Construct/Destruct/Prepare vtables link: Construct in each factory .cpp,
// Destruct/Prepare in CgsStateFactoryLinkStubs.cpp. On PC the three Constructs run from the bring-up
// (BrnRendererModule::Render, beside the post-fx pool) because the module's Construct has no allocator.
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"

struct SortInfo
{
};

struct OcclusionJobData
{
};

// DispatchObjectContext / DispatchList are the real CgsGraphics types now
// (CgsDispatcherCommands.h / CgsDispatcher.h, included above).

// BrnSkyDomeManager is the real type (BrnSkyDomeManager.h) -- the placeholder that used to
// stand here is gone with the sky-dome mount.
#include "GameSource/Graphics/BrnSkyDomeManager.h"

// BrnSunCorona is the real type (BrnSunCorona.h, included below) - it owns the sun-flare vertex
// descriptor + shader programs and is embedded by value as mSunCorona.

// LoadingScreenRenderer is the real type (BrnGame::LoadingScreenRenderer).

// mCalibrationTextureHandle is the real CgsResource::ResourceHandle (DWARF BrnRendererModule.h:692;
// the console's Render latches DispatchThreadInputBuffer::GetCalibrationTextureHandle() into it at
// 0x8240DDB8 and Construct seeds it from NULLResourceHandle at 0x8240BF74). The empty global-namespace
// `struct ResourceHandle {}` that used to stand here was the ShadowMapRenderManager class of
// placeholder: a DIFFERENT type that silently shadowed the real one and occupied one byte.
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"

// BrnGraphics::ShadowMapRenderManager is the real type now
// (GameSource/Graphics/BrnShadowMapRenderManager.h) -- mShadowMapRenderManager below is embedded
// by value and Render calls its Begin/EndRenderShadowMap bracket, so the complete type is
// required. Unlike BrnRendererMemory the placeholder here was a DIFFERENT type (global-namespace
// `ShadowMapRenderManager` vs. the real `BrnGraphics::ShadowMapRenderManager`), so it did not
// ODR-clash -- it silently shadowed the real class instead, which is worse: the member compiled
// and occupied one byte while the real manager's three fields (the two shadow-cache buffer
// indices and mbForceFrontFaceCull) did not exist at all.
#include "GameSource/Graphics/BrnShadowMapRenderManager.h"

struct DebugComponent
{
};

// Vector3 / Vector4 are the real rw::math types (BrnCommonTypes.h, pulled in above).

class BrnRendererModule : public CgsModule::ModuleSingleBuffered
{
public:
    enum ERendererPrepareStage
    {
        eRendererPrepareStart,
        eRendererPrepareManager,
        eRendererPrepareBlobbyShadows,
        eRendererPrepareCoronas,
        eRendererPrepareDone
    };

    enum ERendererReleaseStage
    {
        eRendererReleaseStart,
        eRendererReleaseCoronas,
        eRendererReleaseBlobbyShadows,
        eRendererReleaseManager,
        eRendererReleaseDone
    };

    enum EFrameStallStage
    {
        E_FRAMESTALL_NOT_STALLED,
        E_FRAMESTALL_SYNCING_BUFFERS,
        E_FRAMESTALL_STALLED
    };

    struct BrnCpuMonitors
    {
        s32 miWholeDispatchThread;
        s32 miObjectToMeshListConversion;
        s32 miStartSortJobs;
        s32 miStartTintBlendJob;
        s32 miDispatchShadowmapNearCSMList;
        s32 miWaitOnShadowNearSortJob;
        s32 miDispatchShadowmapFarCSMList;
        s32 miWaitOnShadowFarSortJob;
        s32 miDispatchEnvmapLists;
        s32 miWaitOnEnvmapSortJobs;
        s32 miDispatchWorldLists;
        s32 miDispatchWorldOpaqueList;
        s32 miDispatchWorldTransparentList;
        s32 miWaitOnWorldOpaqueSortJob;
        s32 miWaitOnWorldTransparentSortJob;
        s32 miGenerateOcclusionQueryList;
        s32 miDispatchOcclusionQueries;
        s32 miDispatchCarLists;
        s32 miDispatchCarOpaqueList;
        s32 miDispatchCarTransparentList;
        s32 miWaitOnCarOpaqueSortJob;
        s32 miWaitOnCarTransparentSortJob;
        s32 miRenderSky;
        s32 miRenderCoronas;
        s32 miEffectsUpdate;
        s32 miBuildParticleVertexBuffers;
        s32 miWaitOnParticleVertexBuffersJob;
        s32 miRenderFullResParticles;
        s32 miRenderQuarterResParticles;
        s32 miPPUShaderPatching;
        s32 miRenderIm3d;
        s32 miRenderDebugData;
        s32 miRenderPostFX;
        s32 miRenderHUD;
        s32 miRenderApt;
        s32 miShowPixelBuffer;
        s32 miClearGraphicsContext;
        s32 miWaitOnPreZSortJob;
        s32 miDispatchPreZ;

        void Construct()
        {
            s32* lpMonitor = &miWholeDispatchThread;
            for (u32 luIndex = 0; luIndex < sizeof(BrnCpuMonitors) / sizeof(s32); ++luIndex)
                lpMonitor[luIndex] = 0;
        }
    };

    struct BrnGpuMonitors
    {
        s32 miScreenClear;
        s32 miShadowmap;
        s32 miSky;
        s32 miEnvironmentMap;
        s32 miWorldOpaque;
        s32 miWorldTransparent;
        s32 miCarOpaque;
        s32 miCarTransparent;
        // X360-ONLY, and absent from the DecFIGS DWARF because the PS3 has no EDRAM: console index 8
        // of the twenty monitors BrnRendererModule::Construct @0x8240A778 registers. Its own name is
        // the console's -- `addi r3, r11, aResolvemsaafro@l # "ResolveMSAAFromEDRAM"` @0x8240B5DC,
        // whose returned id is stored to this+0xC9E4 by `ori r9, r11, 0xC9E4` @0x8240B5F4 /
        // `stwx r10, r31, r9` @0x8240B604. Its one reader is BrnRendererModule::ResolveMSAA
        // @0x823FFBE0 (this+0xC9E4 at 0x823FFC58 and 0x823FFD78). Without it that function and
        // EndRenderAntiAliased @0x82408B00 (this+0xC9E8, index 9) would share one host member.
        // The DWARF's own source-line comments leave the gap this fills: miCarTransparent is
        // BrnRendererModule.h:759 and miDownsampleMSAAAndCompParticles is :764.
        s32 miResolveMSAAFromEDRAM;
        s32 miDownsampleMSAAAndCompParticles;
        s32 miSunCoronaVisibilityTest;
        s32 miFullResParticles;
        s32 miQuarterResParticles;
        s32 miCoronas;
        s32 miPostFX;
        s32 miIm3dAndRacePositions;
        s32 miMenusAndHud;
        s32 miDebug3d;
        s32 miDebug2d;
        s32 miPreZ;

        void Construct()
        {
            s32* lpMonitor = &miScreenClear;
            for (u32 luIndex = 0; luIndex < sizeof(BrnGpuMonitors) / sizeof(s32); ++luIndex)
                lpMonitor[luIndex] = 0;
        }
    };

    struct BrnGpuHwCounters
    {
        s32 miEnvMap;
        s32 miShadowMap;
        s32 miWorldOpaque;
        s32 miCarOpaque;
        s32 miWorldTransparent;
        s32 miCarTransparent;
        s32 miPostFX;

        void Construct()
        {
            s32* lpMonitor = &miEnvMap;
            for (u32 luIndex = 0; luIndex < sizeof(BrnGpuHwCounters) / sizeof(s32); ++luIndex)
                lpMonitor[luIndex] = 0;
        }
    };

    BrnRendererModule();

    // @ 0x8240A778 - one-time construction of the renderer's subsystems.
    void Construct();

    // @ 0x8240BFA8 - render one frame from the dispatch-thread input buffer the game side
    // published (the X360 a2/lpDispatchThreadInputBuffer). The loading-screen overlay path
    // is reconstructed; the gameplay-render path (shadows/world/cars/particles/post-fx) is
    // data-gated off during boot and reconstructed incrementally.
    void Render(const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer);

    // Renders the on-screen assert overlay (forwarded from BrnGameModule::RenderAssert).
    void RenderAssert(const struct AssertData* lpAssertData);

    // ---- the per-frame GDL (game-side dispatch list) ring contract ----------
    // X360 drives the this+680 BufferedDispatchFrame from three entry points; the
    // three below are the slices of them that exist on PC (their other work --
    // the seven immediate-mode buffers, the effects arbitrator, the corona /
    // blobby-shadow index flips -- lands with those subsystems).

    // @ 0x823FC160 - start-of-update-frame: rewind the GDL frame the game side is
    // about to fill and open the shader-constant table's frame on its bin.
    void StartOfFrame();

    // @ 0x823FFE28 - end-of-update-frame (X360 calls SwapBuffers @0x823FC678).
    void EndOfFrame();

    // @ 0x82405E28 (BrnRendererModule::Update) publishes exactly this expression
    // into RendererIO::OutputBuffer::SetDispatchFrame; the world side reads it
    // back through WorldModuleIO::DispatchInputBuffer::GetDispatchFrame(). Exposed
    // as a named accessor so the renderer->world bridge has one seam to bind to.
    CgsGraphics::DispatchFrame* GetDispatchFrameForWrite();

    // ⭐ @0x82405E28 -- BrnRendererModule::Update, RECONSTRUCTED 2026-08-17 (boot audit
    // F-P2-4). The renderer's per-pass publication into the RendererIO buffer pair: take the
    // input's camera, lend the output the reusable loading-screen allocator (which is what
    // BrnGameModule::GamePrepare's tail latches into gm+0x9A0630), then publish every
    // per-frame buffer the rest of the engine reads back through the output buffer.
    // Store-for-store map in progress/boot_audit/phases/P2b_renderermodule_update.md.
    void Update(CgsModule::IOBufferStack* lpUpdateInputStack,
                CgsModule::IOBufferStack* lpUpdateOutputStack,
                RendererIO::InputBuffer*  lpInput,
                RendererIO::OutputBuffer* lpOutput);

    // The reusable loading-screen allocator, X360 renderer+51452 (0xC8FC). Update lends its
    // ADDRESS to the output buffer (`addis r4,r31,1; addi r4,r4,-0x3704` @0x82405EBC), the
    // game module latches it, and LoadingScriptedState::Update FreeAll's it before each world
    // drive. It is an embedded member on the console, not a pointer.
    CgsMemory::LinearMalloc* GetReusableLoadingScreenAllocator() { return &mReusableLoadingScreenAllocator; }

    // [FLAG PC bring-up] Hand a WORLD-layer effects frame to the world module.
    //
    // STANDS IN FOR RendererIO::OutputBuffer::GetWorldEffectsFrame(luSlot) @0x823B3C38, which
    // BrnRendererModule::Update @0x82405E28 (pseudocode line 110) fills, per slot, with
    // mEffectsArbitrator.GetExternalEffectsFrame(KU_EFFECTS_LAYER_WORLD, luSlot); the console then
    // moves the pointer across in BridgeRendererToWorld (GameBridgeRendererToX.cpp:50) so
    // WorldModule::GenerateDispatchLists @0x827D1CE8 can hand the four frames to
    // EnvironmentManager::GenerateEffects @0x827BE698. Neither Update nor the RendererIO buffers
    // exist on this build (BrnGameModule.cpp:1339-1360), so the world side calls this instead.
    //
    // Returns nullptr until the arbitrator has been Constructed (it is built lazily on PC -- see
    // EnsureEffectsArbitratorBringUp in BrnRendererModule.cpp), and the world side must treat a null
    // as "no effects frame this frame" rather than dereferencing it.
    // DELETE-WHEN the RendererIO buffers are created on PC and Update publishes for real.
    BrnEffectsFrame* GetWorldEffectsFrameBringUp(u8 luSlot);

    // [FLAG PC bring-up] Stage the DIRECTOR'S PUBLISHED CAMERA for the base-frame producer.
    //
    // STANDS IN FOR BrnEffects::EffectsIO::DispatchInputBuffer::SetCameraInput @0x823C9988
    // (DWARF EffectsModuleIO.h:242, `void SetCameraInput(const Camera*)`), whose body is a
    // "locked for writing" assert followed by one `BrnDirector::Camera::Camera::operator=` into
    // the buffer's by-value `Camera mCameraInput` member (EffectsModuleIO.h:261, X360 this+0x50).
    // Its ONE caller in the image is BrnGameModule::DoDispatch @0x823DC458 line 103:
    //     v20 = BrnDirector::DirectorIO::OutputBuffer::GetCameraOutput(*v11);
    //     BrnEffects::EffectsIO::DispatchInputBuffer::SetCameraInput(v18, v20);
    // The record is then read by BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10
    // lines 116-221 to decide, per frame, whether depth-of-field / B4 blur / motion blur are on
    // and with what parameters. None of the EffectsIO buffers is created on this build, so the
    // renderer's bring-up producer (PCBringUpProduceBaseEffectsFrame) reads a copy staged here
    // instead. A null pointer is ignored (the record then keeps its last staged value, or the
    // director's Camera::Construct defaults if nothing has ever been staged).
    // DELETE-WHEN the EffectsIO dispatch buffer set is real on PC (this goes with the producer).
    void PCBringUpSetCameraInput(const BrnDirector::Camera::Camera* lpCamera);

    // [FLAG PC bring-up] PCBringUpSetRaceCarStateCache -- NOT an X360 function.
    //
    // STANDS IN FOR the player-car arm of BrnEffects::EffectsModule::Update @0x8229EC28, which
    // is the ONLY writer of the effects module's TempRaceCarStateCache (DWARF EffectsModule.h:577,
    // module +180864). Its four DYNAMIC fields are copied straight off the player's
    // BrnPhysics::Vehicle::RaceCarState, reached through the world's
    // RCEntityActiveRaceCarOutputInterface:
    //     v101 = RCEntityActiveRaceCarOutputInterface::GetPlayerActiveRaceCarIndex(iface);
    //     _R3  = RCEntityActiveRaceCarOutputInterface::GetActiveRaceCarState(iface, v101);
    //     _R11 = 816;  _R10 = 180992;  lvx v0,r3,r11 / stvx v0,r31,r10   -> mvLinearVelocity
    //     _R9  = 832;  _R8  = 181008;  lvx v0,r3,r9  / stvx v0,r31,r8    -> mvAngularVelocity
    //     this->field_2C320 = *(_R3 + 972);                              -> mfSpeedMPH
    //     this->field_2C324 = *(_R3 + 1044);                             -> mfSteering
    // (RaceCarState's committed members sit at exactly those four offsets --
    // BrnVehicleEvents.h mLinearVelocity @816 / mAngularVelocity @832 / mfSpeedMPH @972 /
    // mfSteering @1044 -- so the caller reads them BY NAME, never by displacement.)
    // BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10 then copies the cache into
    // the layer-0 BrnEffectsFrame (frame +0x1B0/+0x1C0/+0x1D0/+0x1D4), which is what
    // PCBringUpProduceBaseEffectsFrame does with the values staged here.
    // Neither the effects module nor its IO buffers exist on this build; the caller is
    // BrnGameModule::DoDispatch, beside PCBringUpSetCameraInput, off the SAME world output
    // interface the console's producer reads. Nothing is staged while the player car is not
    // active (the console's whole block is inside `if (IsPlayerCarActive(...))`), so the last
    // staged values stand -- exactly as the console's cache does.
    // ⚠ The cache's two TRANSFORM fields are NOT staged and cannot be: nothing in the X360 image
    // writes module +180864 / +180928 at all (see the BLOCKED banner in the producer).
    // DELETE-WHEN BrnEffects::EffectsModule is on the build list and fills its own cache.
    void PCBringUpSetRaceCarStateCache(Vector3::InParam lvLinearVelocity,
                                       Vector3::InParam lvAngularVelocity,
                                       f32 lfSpeedMPH,
                                       f32 lfSteering);

private:
    enum
    {
        KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS = 16,
        KU_NUM_SHADOWMAP_DISPATCH_JOBS = 5,
        KU_NUM_ENVMAP_SORT_JOBS = 6,
        KU_NUM_INTERPRET_FUNCTIONS = 4,
        KU_SCREENSHOT_TEXT_LENGTH = 32
    };

    // @ 0x823FC678 - the buffer-flip half of EndOfFrame (X360 calls it from there).
    void SwapBuffers();

    // ---- the frame bracket's END (the off-screen scene target's exit path) -------------------
    // DECLARATIONS ONLY: the reconstructed bodies for all three are delivered in this pass's
    // bodies/ directory and are deliberately NOT mounted into BrnRendererModule.cpp yet -- they
    // call the three state factories (which ARE the real classes in this header since the
    // 2026-08-15 gate-flip wave; that precondition is met). The full, ordered precondition list for mounting them is in the pass
    // REPORT (scratch/postfx_round3_out/G4_bracket_end/REPORT.md section 2.4). Nothing calls any
    // of the three today, so declaring them adds no link requirement.
    //
    // @ 0x82408B00 - close the anti-aliased pass: retarget to the down-sample buffer, restore the
    // pass-default state triple, composite the quarter-res particles, resolve the colour surface.
    void EndRenderAntiAliased();

    // @ 0x82408C38 - open the quarter-resolution soft-particle buffer: bind it, clear all colour
    // targets, set the depth-write/no-colour-write triple, blit the scene depth down into it.
    void BeginQuarterResBuffer();

    // @ 0x823F65B0 - the pass-default state-triple restore that follows the post-fx chain. The
    // RenderTarget parameter is UNUSED on this build and that is an asm fact, not an omission:
    // the body reads only the four globals and never touches r3 or r4. Its type comes from the
    // caller -- Render @0x8240BFA8 loads `lwz r4, 0x108(r29)`, a CgsRenderTarget::mpRenderTarget
    // (CgsRenderTarget.h:199), i.e. rw::graphics::postfx::RenderTarget*.
    void EndRenderPostFx(rw::graphics::postfx::RenderTarget* lpRenderTarget);

    void ClearDispatchCounters();
    void ClearScreenshotState();
    void ConstructRenderSwitches();

    // @ 0x82405A30 - the three per-thread monitor squares (bottom-centre): each is green when its
    // thread is keeping up (running in real time) and red when it has fallen behind. The X360 draws
    // them via the untextured Basic2dColouredVertex renderer at normalised coords; reconstructed
    // through mIm2dRenderer (untextured -> solid colour) at the same screen positions.
    void RenderThreeThreadMonitors(bool lbThread0, bool lbThread1, bool lbThread2);

    // @ 0x82406410 - draw the two solid-black bars that frame a widescreen (letterboxed) view: one
    // across the top, one across the bottom. lfDestAspectRatio is the visible/kept vertical fraction
    // of the screen; the cropped-away remainder (1 - lfDestAspectRatio) is split evenly, so each bar
    // is (1 - lfDestAspectRatio) * 0.5 of the height and spans the full width. Drawn through the
    // immediate-mode 2D renderer (DWARF signature CgsGraphics::Im2d& + float).
    void RenderLetterBoxBars(CgsGraphics::Im2d& lIm2d, f32 lfDestAspectRatio);

    // @ 0x823F5898 - expand every GDL object list (0..12) of the read-side buffered
    // frame into the mesh lists (0..24) of the render frame. The PC runs the X360's
    // own single-threaded fallback path (the 16-job path needs the job scheduler).
    void ConvertObjectsToMeshes(CgsGraphics::BufferedDispatchFrame* lpGdlFrames,
                                CgsGraphics::DispatchFrame* lpMeshFrame,
                                CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
                                const CgsGraphics::DispatchObjectContext* lpContext);

    // @ 0x823F5F70 - sort every pass list of the render frame (X360: RadixSort jobs;
    // PC: the synchronous DispatchList::SortForDispatch stand-in).
    void SortDispatchLists(CgsGraphics::DispatchFrame* lpMeshFrame);

    // @ 0x823FFA18 - open the frame's ANTI-ALIASED scene pass: publish this frame's background
    // colour into mvBackgroundColour, bind the anti-alias buffer's section-0 surface state, and
    // (multisampled path only) open the Xenos predicated-tiling pass that clears the two EDRAM tiles
    // to it. THIS is the call that makes the world pass render OFF-SCREEN instead of into the swap
    // chain; everything Render submits between it and ResolveMSAA / EndRenderAntiAliased lands in the
    // anti-alias buffer. Called from Render @0x8240BFA8 (Render:725), after the shadow-map and
    // env-map passes.
    //
    // Signature and PARAMETER NAMES from the DecFIGS DWARF (BrnRendererModule.h:775 declares
    // `void BeginRenderAntiAliased(float32_t, bool8_t, uint8_t)` in the PRIVATE section;
    // _compile/BrnGraphicsUnity.cpp:4597 spells the names
    // `const float32_t lfWhiteLevel, const bool8_t lbClearStencil, const uint8_t luStencilClearValue`).
    // The X360 prologue attests all three positions: lfWhiteLevel in f1 (a float SKIPS its GPR slot,
    // so r4 is dead), lbClearStencil in r5, luStencilClearValue in r6 (`mr r27, r6` @0x823FFA30) --
    // the bool8 in the middle is what pushes the stencil byte out to r6, so the asm attests the
    // middle parameter independently of the DWARF. Non-static: Render passes the module in r3 and the
    // body uses it as `this` (`mr r31, r3` @0x823FFA28, then `lbzx r11, r31, r11` with r11 = 0xC434).
    //
    // DEFINITION IS GATED behind BRN_ANTIALIAS_BRACKET_AVAILABLE in BrnRendererModule.cpp; read that
    // banner before calling this.
    void BeginRenderAntiAliased(f32 lfWhiteLevel, bool lbClearStencil, u8 luStencilClearValue);

    // @ 0x823FFBE0 - close the anti-aliased scene pass: RESOLVE the EDRAM depth and colour surfaces
    // into the DOWN-SAMPLE buffer's sampleable textures (one resolve pair per EDRAM tile, each
    // predicated to its own tile's replay of the command stream), then close the tiling pass. The
    // colour resolve also CLEARS both EDRAM surfaces behind itself, which is what leaves the next
    // frame's untiled path with nothing to clear. Called from Render @0x8240BFA8.
    //
    // Signature and PARAMETER NAMES from the DecFIGS DWARF (BrnRendererModule.h:781 declares
    // `void ResolveMSAA(float32_t, uint8_t)` in the PRIVATE section; _compile/BrnGraphicsUnity.cpp:487
    // spells the names `const float32_t lfWhiteLevel, const uint8_t luStencilValue`). The X360
    // prologue agrees: the float rides f1 (skipping r4) and the stencil byte is r5
    // (`mr r27, r5` @0x823FFC28, narrowed by `clrlwi r26, r27, 24` @0x823FFC70). Non-static: the body
    // parks r3 as `this` and reads members off it (`lwz r3, 0x248(r30)` @0x823FFC98).
    //
    // DEFINITION IS GATED behind BRN_ANTIALIAS_BRACKET_AVAILABLE in BrnRendererModule.cpp.
    void ResolveMSAA(f32 lfWhiteLevel, u8 luStencilValue);

    // @ 0x823F63E0 -- open ONE FACE of the environment-map (car-reflection) cube pass: bind the
    // env-map target's section-0 surface state with an INVERTED viewport depth range, clear its
    // colour to whiteLevel * 0.3 and its depth to 0.0 (the far value under inverted depth), then
    // apply the three cached render states the face's geometry walk runs under. Called from
    // Render @0x8240BFA8 (`bl` @0x8240CC3C), once per rendered face, between the shadow-map pass
    // and BeginRenderAntiAliased.
    //
    // Signature and PARAMETER NAMES from the DecFIGS DWARF: BrnRendererModule.h (dwarfdump file
    // line 799) declares `void BeginRenderEnvironmentMapFace(uint32_t, float32_t)` and
    // _compile/BrnGraphicsUnity.cpp:4633 spells the names
    // `uint32_t luFace, const float32_t lfWhiteLevel`. The X360 prologue attests both positions:
    // the face rides r4 (`mr r30, r4` @0x823F6400) and the float rides f1 (`fmr f31, f1`
    // @0x823F63F4) -- a float argument SKIPS its GPR slot, which is why Hex-Rays prints the
    // parameter list as (int, int, double). Non-static: r3 is `this` (`mr r31, r3` @0x823F63F8,
    // then `lwz r10, 0x244(r31)` == mAllocatedRenderTargets.GetEnvMapBuffer()).
    //
    // DEFINITION IS GATED behind BRN_ENVMAP_PASS_AVAILABLE in BrnRendererModule.cpp; read that
    // banner before calling this.
    void BeginRenderEnvironmentMapFace(u32 luFace, f32 lfWhiteLevel);

    // @ 0x823FC5E8 -- close one env-map face: resolve colour target 0 of the env-map render target
    // into that FACE of its cube texture. The whole body is the two asserts plus
    //     mAllocatedRenderTargets.GetEnvMapBuffer()->GetRenderTarget()->maColourTargets[0].Resolve(luFace)
    // (`addi r3, r11, 0x20` @0x823FC664 -- rt+0x20 IS maColourTargets[0] on the 4-byte-pointer image
    // -- then `bl sub_823F9170`). DWARF BrnRendererModule.h (dwarfdump file line 802) declares
    // `void EndRenderEnvironmentMapFace(uint32_t)` and BrnGraphicsUnity.cpp:1445 names the
    // parameter `luFace`.
    void EndRenderEnvironmentMapFace(u32 luFace);

    // The world/car/sky pass block of Render (@0x8240BFA8 mid-section), split out
    // for readability; runs between the frame begin and the 2D overlay tail.
public:
    // @ 0x823FF8F8 - BrnRendererModule::PrepareAgain. The second half of the renderer's
    // prepare: BrnGameModule::GamePrepare stage 3 hands it the five global textures it just
    // resolved and it stores them for the passes that sample them (the blobby-shadow
    // manager, the sky dome's two cloud layers, the corona atlas, the damage-FX glass
    // fracture). The X360 writes them at this+0xC4E0 / +0xC4E4 (the two cloud slots
    // BrnSkyDomeManager::Render is handed) and their three siblings.
    void PrepareAgain(renderengine::Texture* lpBlobbyShadow,
                      renderengine::Texture* lpCloudDensity,
                      renderengine::Texture* lpCloudLighting,
                      renderengine::Texture* lpCoronaAtlas,
                      renderengine::Texture* lpGlassFracture);

private:
    // @ 0x8240BFA8 (Render:389-396) - reset the render frame, point the interpreter at it, build
    // the per-frame DispatchObjectContext the X360 keeps on Render's stack, expand the GDL object
    // lists into mesh lists and sort every pass list. Returns false when the GDL ring never came
    // up (Construct's allocator gate did not open), in which case no pass may run.
    //
    // This is hoisted OUT of RenderWorldPasses and up into Render because the console's frame order
    // is convert/sort -> SHADOW MAPS -> env map -> BeginRenderAntiAliased -> the world passes: the
    // shadow cascades consume mesh lists 0..4, so the lists have to exist before them.
    bool BuildDispatchLists(CgsGraphics::DispatchObjectContext* lpContext);

    // @ 0x8240BFA8 (Render:545-640) - the three shadow-map cascades, gated on
    // mRenderSwitches.mbRenderShadows. Each cascade brackets its mesh-list walks with
    // ShadowMapRenderManager::Begin/EndRenderShadowMap; the lists are {0,2} / {1,3} / {4}.
    void RenderShadowMapPasses(CgsGraphics::DispatchObjectContext* lpContext);

    void RenderWorldPasses(const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer,
                           CgsGraphics::DispatchObjectContext* lpContext);

    // [FLAG PC bring-up] Sky-dome bring-up (NOT X360 functions -- see the bodies).
    // EnsureSkyDomeBringUp does the Construct/Prepare pair the console runs from
    // BrnRendererModule::Construct/Prepare, deferred to the first world frame because
    // both need a live D3D device.
    //
    // PublishSkyConstantsBringUp is NOT a producer: on the console this frame is filled by
    // the WORLD (WorldModule::SetupShaderConstantsBeforeRendering @0x827D1410 writes it in
    // place, through the pointer BrnRendererModule::Update @0x82405E28 lends it via
    // RendererIO::OutputBuffer::SetShaderConstantsFrame @0x823FB608 ->
    // BridgeRendererToWorld @0x823CDD20 -> DispatchInputBuffer::GetShaderConstantsFrame
    // @0x827BBEF0), and the renderer only reads it. So this function COPIES the live frame
    // the real producer fills on PC -- gBrnWorldShaderConstantsFrameBringUp
    // (BrnShaderConstantsFrame.h) -- into the renderer's own frame, and keeps only the
    // camera half, which the dispatch IO buffer set would otherwise carry across.
    // DELETE-WHEN that IO buffer set is real; see the banner over the definition.
    bool mbSkyDomeReady;
    bool mbSkyDomeTried;
    bool EnsureSkyDomeBringUp();
    void PublishSkyConstantsBringUp(BrnShaderConstantsFrame* lpFrame);

    // [FLAG PC bring-up] Write the LAYER-0 (base) effects frame the console's effects module writes.
    // Stands in for BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10 (lines 40-120);
    // see the banner over the definition in BrnRendererModule.cpp for what it writes and why.
    void PCBringUpProduceBaseEffectsFrame();

    ERendererPrepareStage mePrepareStage;
    ERendererReleaseStage meReleaseStage;
    s32                   mDisplayType;
    u16                   mu16FrontBufferHeight;
    bool                  mbIsHD;
    bool                  mbIsInterlaced;
    BrnRendererMemory     mAllocatedRenderTargets;
    CgsGraphics::BufferedDispatchFrame mDoubleBufferedDispatchFrame;
    CgsGraphics::DispatchFrame         mSingleBufferedDispatchFrame;
    BrnGraphics::EffectsArbitrator     mEffectsArbitrator;
    BrnShaderConstantsFrame            maShaderConstantsFrames[2];
    u8                    mu8ShaderConstantsFrameInternal;
    u8                    mu8ShaderConstantsFrameExternal;
    CgsGraphics::DispatchPacketInterpreter* mpInterpreter;
    void (*maInterpretFunctions[KU_NUM_INTERPRET_FUNCTIONS])(CgsGraphics::DispatchCommand*, CgsGraphics::DispatchFrame*, void*, f32);
    CgsGraphics::Im2dRenderBuffer       mIm2dRenderBuffer;
    CgsGraphics::Im2d                   mIm2dRenderer;
    CgsGraphics::Im2dUntex              mIm2dRendererUntex;
    CgsGraphics::Im3dRenderBuffer       mIm3dRenderBuffer;
    CgsGraphics::Im3d                   mIm3dRenderer;
    CgsGraphics::Im3dRenderBufferUntex  mIm3dRenderBufferUntex;
    CgsGraphics::Im3dUntex              mIm3dRendererUntex;
    BrnGraphics::Im3dSkyDome            mIm3dRendererSkyDome;
    CgsGraphics::Im3dZOnly              mIm3dRendererZOnly;
    CgsGraphics::Im3dRenderBuffer       mIm3dDebugRenderBuffer;
    CgsGraphics::Im2dRenderBuffer       mIm2dDebugRenderBuffer;
    renderengine::TextureState*         mpTextureState;
    TextureStateParameters              mTextureStateParams;
    Resource                            mTextureStateResource;
    renderengine::TextureState*         mpEnvMapTextureState;
    TextureStateParameters              mEnvMapTextureStateParams;
    Resource                            mEnvMapTextureStateResource;
    renderengine::Texture*              mpGlassFractureTexture;
    renderengine::TextureState*         mpGlassFractureTextureState;
    TextureStateParameters              mGlassFractureTextureStateParams;
    Resource                            mGlassFractureTextureStateResource;
    Resource                            mBackBufferTextureResource;
    renderengine::TextureState*         mpShadowMapTextureState[2];
    CgsGraphics::Im3dRenderBuffer       mIm3dBufferRacePosition;
    CgsGraphics::Im3dRenderBuffer       mIm3dBufferMenusAndHud;
    BrnBlobbyShadowManager              mBlobbyShadowManager;
    renderengine::Texture*              mpBlobbyShadowTexture;
    f32                                 mfBlobbyShadowAlpha;
    BrnCoronaManager                    mCoronaManager;
    CgsBlendStateFactory                mBlendStateFactory;
    CgsRasterizerStateFactory           mRasterizerStateFactory;
    CgsDepthStencilStateFactory         mDepthStencilStateFactory;
    BrnResource::LinearResourceAllocator* mpGraphicsAllocator;
    EA::Jobs::Job                       maObjectToMeshJob[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS];
    CgsGraphics::DispatchObjectContext  maObjectToMeshJobContext[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS];
    CgsGraphics::DispatchList*          mapaObjectToMeshJobOutputDispatchLists[KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS];
    EA::Jobs::Job                       maShadowMapSortJob[KU_NUM_SHADOWMAP_DISPATCH_JOBS];
    SortInfo                            maShadowMapSortJobData[KU_NUM_SHADOWMAP_DISPATCH_JOBS];
    EA::Jobs::Job                       maEnvmapSortJobs[KU_NUM_ENVMAP_SORT_JOBS];
    SortInfo                            maEnvmapSortJobData[KU_NUM_ENVMAP_SORT_JOBS];
    EA::Jobs::Job                       mPreZSortJob;
    SortInfo                            mPreZSortJobData;
    EA::Jobs::Job                       mWorldOpaqueSortJob;
    SortInfo                            mWorldOpaqueSortJobData;
    EA::Jobs::Job                       mCarOpaqueSortJob;
    SortInfo                            mCarOpaqueSortJobData;
    EA::Jobs::Job                       mWorldTransparentSortJob;
    SortInfo                            mWorldTransparentSortJobData;
    EA::Jobs::Job                       mCarTransparentSortJob;
    SortInfo                            mCarTransparentSortJobData;
    EA::Jobs::Job                       mOcclusionWorldOpaqueJob;
    OcclusionJobData                    mOcclusionJobWorldOpaqueInfo;
    bool                                mbMultisampledBackbuffer;
    bool                                mbShowEnvironmentMap;
    bool                                mbShowShadowMap;
    bool                                mbSortDisplayListsWideNotLong;
    s32                                 miShowShadowMapIndex;
    f32                                 mfAspectCorrection;
    RendererIO::RenderSwitches          mRenderSwitches;
    // X360 renderer+0xC8FC -- the allocator Update publishes (see GetReusableLoadingScreenAllocator).
    CgsMemory::LinearMalloc             mReusableLoadingScreenAllocator;
    bool                                mbRenderPreZ;
    bool                                mbRenderWorldOpaque;
    bool                                mbRenderCarsOpaque;
    bool                                mbRenderSky;
    bool                                mbRenderWorldTransparent;
    bool                                mbRenderCarsTransparent;
    bool                                mbRenderBlobbyShadows;
    bool                                mbRenderParticles;
    bool                                mbRenderCoronas;
    bool                                mbRenderWorldImmediateMode;
    bool                                mbRenderPostFX;
    bool                                mbRenderHudImmediateMode;
    bool                                mbOcclusionCullCarOpaque;
    bool                                mbOcclusionCullWorldOpaque;
    bool                                mbOcclusionCullCarTransparent;
    bool                                mbOcclusionCullWorldTransparent;
    bool                                mbOcclusionCullShadowMap;
    bool                                mbPreZNearOnly;
    bool                                mbRenderPreZAlpha;
    f32                                 mfPreZDistanceThreshold;
    f32                                 mfOccludeeNearClipOffset;
    u32                                 muOcclusionCullIndexCountThreshold;
    bool                                mbGreyBackgroundColour;
    bool                                mbClearDispatchCounts;
    u32                                 mu32NumWorldOpaqueObjectTotals;
    u32                                 mu32NumCarOpaqueObjectTotals;
    u32                                 mu32NumWorldTransparentObjectTotals;
    u32                                 mu32NumCarTransparentObjectTotals;
    u32                                 mu32NumShadowObjectTotals;
    u32                                 mu32DispatchFrameCounter;
    u32                                 mu32NumWorldOpaqueObjects;
    u32                                 mu32NumCarOpaqueObjects;
    u32                                 mu32NumWorldTransparentObjects;
    u32                                 mu32NumCarTransparentObjects;
    u32                                 mu32NumShadowObjects;
    bool                                mbUpdateThreadTakeScreenshot;
    bool                                mbDispatchThreadTakeScreenshot;
    bool                                mbCaptureOverlaysInScreenshot;
    u32                                 muScreenshotCounter;
    char                                macScreenShotText[KU_SCREENSHOT_TEXT_LENGTH];
    Vector3                             mKeyLightDirection;
    Vector3                             mKeyLightColor;
    Vector3                             mKeyLightSpecularColour;
    Vector3                             mAmbientColour;
    Vector4                             mvBackgroundColour;
    const renderengine::Texture*        mpCloudDensity0Texture;
    const renderengine::Texture*        mpCloudLighting0Texture;
    BrnSkyDomeManager                   mSkyDome;
    BrnSunCorona                        mSunCorona;
    EFrameStallStage                    meFrameStallStage;
    s32                                 miFrameStallCountdown;
    CgsGraphics::OcclusionCullManager   mOcclusionCullManager;
    BrnGame::LoadingScreenRenderer      mLoadingScreenRenderer;
    CgsResource::ResourceHandle         mCalibrationTextureHandle;   // X360 +0xC920
    BrnCpuMonitors                      mCpuMonitors;
    BrnGpuMonitors                      mGpuMonitors;
    BrnGpuHwCounters                    mGpuHwMonitors;
    s32                                 miCpuPerfMonDispatchThread;
    BrnGraphics::ShadowMapRenderManager mShadowMapRenderManager;
    bool                                mbDiskErrorLastFrame;
    s32                                 miFramesSinceDiskErrorReported;
    DebugComponent                      mDebugComponent;
};

inline void BrnRendererModule::ClearDispatchCounters()
{
    mu32NumWorldOpaqueObjectTotals = 0;
    mu32NumCarOpaqueObjectTotals = 0;
    mu32NumWorldTransparentObjectTotals = 0;
    mu32NumCarTransparentObjectTotals = 0;
    mu32NumShadowObjectTotals = 0;
    mu32DispatchFrameCounter = 0;
    mu32NumWorldOpaqueObjects = 0;
    mu32NumCarOpaqueObjects = 0;
    mu32NumWorldTransparentObjects = 0;
    mu32NumCarTransparentObjects = 0;
    mu32NumShadowObjects = 0;
}

inline void BrnRendererModule::ClearScreenshotState()
{
    mbUpdateThreadTakeScreenshot = false;
    mbDispatchThreadTakeScreenshot = false;
    mbCaptureOverlaysInScreenshot = false;
    muScreenshotCounter = 0;

    for (u32 luIndex = 0; luIndex < KU_SCREENSHOT_TEXT_LENGTH; ++luIndex)
        macScreenShotText[luIndex] = 0;
}

inline void BrnRendererModule::ConstructRenderSwitches()
{
    mRenderSwitches.mbRenderShadows = true;
    mRenderSwitches.mbRenderEnvmap = true;
    mRenderSwitches.mbRenderWorld = true;
    mRenderSwitches.mbRenderProps = true;
    mRenderSwitches.mbRenderRaceCars = true;
    mRenderSwitches.mbRenderTraffic = true;

    mbRenderPreZ = true;
    mbRenderWorldOpaque = true;
    mbRenderCarsOpaque = true;
    mbRenderSky = true;
    mbRenderWorldTransparent = true;
    mbRenderCarsTransparent = true;
    mbRenderBlobbyShadows = true;
    mbRenderParticles = true;
    mbRenderCoronas = true;
    mbRenderWorldImmediateMode = true;
    mbRenderPostFX = true;
    mbRenderHudImmediateMode = true;
}

inline BrnRendererModule::BrnRendererModule()
{
    mePrepareStage = eRendererPrepareStart;
    meReleaseStage = eRendererReleaseStart;
    mDisplayType = 0;
    mu16FrontBufferHeight = 0;
    mbIsHD = false;
    mbIsInterlaced = false;

    mu8ShaderConstantsFrameInternal = 0;
    mu8ShaderConstantsFrameExternal = 0;
    mpInterpreter = 0;
    for (u32 luIndex = 0; luIndex < KU_NUM_INTERPRET_FUNCTIONS; ++luIndex)
        maInterpretFunctions[luIndex] = 0;

    mpTextureState = 0;
    mpEnvMapTextureState = 0;
    mpGlassFractureTexture = 0;
    mpGlassFractureTextureState = 0;
    mpShadowMapTextureState[0] = 0;
    mpShadowMapTextureState[1] = 0;
    mpBlobbyShadowTexture = 0;
    // X360 Construct @0x8240BC8C stores flt_8203B710 = 0.7f into this slot; the PS3 DWARF
    // carries the same value independently. It was 0.0f here, i.e. fully transparent blobs.
    mfBlobbyShadowAlpha = 0.7f;
    mpGraphicsAllocator = 0;
    for (u32 luIndex = 0; luIndex < KU_NUM_OBJECT_TO_MESH_DISPATCH_JOBS; ++luIndex)
        mapaObjectToMeshJobOutputDispatchLists[luIndex] = 0;

    // ⚠ CORRECTED 2026-08-16 (anti-aliasing wave): this was `false`; THE X360 STORES 1, and it is
    // the single byte the console's whole anti-aliasing configuration hangs off.
    //
    // (1) IT IS SET UNCONDITIONALLY, in Construct's opening block -- there is no display-mode read,
    //     no video-settings query and no branch anywhere near it:
    //         0x8240A7A8  addis r29, r31, 1
    //         0x8240A7B0  addi  r29, r29, -0x3C00      ; r29 = this + 0xC400
    //         0x8240A7B4  li    r28, 1
    //         0x8240A7C4  stb   r28, 0(r29)            ; mbMultisampledBackbuffer = 1
    //
    // (2) IT IS ALSO THE POOL'S lbEnableMSAA -- the same byte, read straight back and handed to
    //     BrnRendererMemory::Construct:
    //         0x8240A8BC  lbz   r11, 0(r29)
    //         0x8240A8C8  stb   r11, 0x2A0+var_209(r1) ; sp+0x97 == the callee's `arg_97`
    //         0x8240A8FC  bl    BrnRendererMemory__Construct
    //     and BrnRendererMemory::Construct @0x823FCA38 reads `arg_97` as lbEnableMSAA and forwards
    //     it untouched to CreateAntiAliasBuffer @0x823F6B40, which is what selects
    //     BrnGraphics::KMSAA_TILING_PLAN (multisample format 1 == 2 samples, two predicated tiles)
    //     over KNO_MSAA_TILING_PLAN. See BrnAntiAliasTiling.h.
    //
    // So the target's multisample format and the frame bracket's tiled branch CANNOT be set
    // independently without inventing a console configuration that does not exist. The PC bring-up
    // creator takes this same flag as its argument for exactly that reason
    // (BrnRendererMemory::PCBringUpCreatePostFxSceneTargets).
    //
    // WHAT THE PC SAMPLE COUNT IS is a separate question and is NOT decided here: the leaf maps the
    // plan's multisample format to a D3DMULTISAMPLE_TYPE and applies the renderengine::gAntiAliasing
    // override (Xbox2SurfaceShims.h). At the default (0) that is the console's own 2x.
    mbMultisampledBackbuffer = true;
    mbShowEnvironmentMap = false;
    mbShowShadowMap = false;
    mbSortDisplayListsWideNotLong = false;
    miShowShadowMapIndex = 0;
    // X360 BrnRendererModule::Construct @0x8240A828 stores flt_82001C98 == 1.0f into this slot
    // (`addi r28, r28, -0x3BF8` = this+0xC408, `lfs f29, flt_82001C98@l(r11)`, `stfs f29, 0(r28)`;
    // the constant is dumped at DATA_DUMP.md:1516 as 0x3F800000). It was 0.0f here -- a placeholder
    // zero, and this member is a RATIO whose neutral is 1.0: Render's letterbox branch is
    // `if (mfAspectCorrection < 1.0f) RenderLetterBoxBars(mfAspectCorrection, ...)`, so 0.0f means
    // "letterbox every frame", and BrnPostFx::Render takes the same value as its f4 aspect
    // correction, where 0.0f would stretch the composite by an invented number.
    mfAspectCorrection = 1.0f;
    ConstructRenderSwitches();

    mbOcclusionCullCarOpaque = false;
    mbOcclusionCullWorldOpaque = false;
    mbOcclusionCullCarTransparent = false;
    mbOcclusionCullWorldTransparent = false;
    mbOcclusionCullShadowMap = false;
    mbPreZNearOnly = false;
    mbRenderPreZAlpha = false;
    mfPreZDistanceThreshold = 0.0f;
    mfOccludeeNearClipOffset = 0.0f;
    muOcclusionCullIndexCountThreshold = 0;
    mbGreyBackgroundColour = false;

    mbClearDispatchCounts = false;
    ClearDispatchCounters();
    ClearScreenshotState();

    mKeyLightDirection.SetZero();
    mKeyLightColor.SetZero();
    mKeyLightSpecularColour.SetZero();
    mAmbientColour.SetZero();
    mvBackgroundColour.SetZero();
    mpCloudDensity0Texture = 0;
    mpCloudLighting0Texture = 0;
    mbSkyDomeReady = false;
    mbSkyDomeTried = false;

    meFrameStallStage = E_FRAMESTALL_NOT_STALLED;
    miFrameStallCountdown = 0;
    mCpuMonitors.Construct();
    mGpuMonitors.Construct();
    mGpuHwMonitors.Construct();
    miCpuPerfMonDispatchThread = 0;
    mbDiskErrorLastFrame = false;
    miFramesSinceDiskErrorReported = 0;
}
