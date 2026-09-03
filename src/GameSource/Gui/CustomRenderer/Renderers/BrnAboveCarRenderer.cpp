// BrnAboveCarRenderer.cpp -- BrnGui::AboveCarRenderer, the GUI custom-render component that
// draws the marker / score / gamer-tag / banking-score overlay above each car.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Jan-2008). This TU's ledger has 7 functions;
// 5 are bodied here (Construct, GetID, Prepare, Release, Update). RenderComponent and
// RenderBankingScores are declaration-only -- see the per-function note below for why (both
// depend on BrnGui::GuiCache methods that are still [todo]/[blocked] on class:BrnGui::GuiCache,
// and RenderComponent additionally reaches the un-homed AboveCarRenderer::RenderReplayAboveCar /
// SetTransformMatrixForCar helpers and several unattributed sub_* helper addresses the X360
// decompiler could not name). Per AGENTS.md this project does not fabricate bodies to force
// 100% coverage; leaving them declared with an honest note is the established convention (see
// e.g. BrnSatNavRenderer.cpp's declaration-only members for the equivalent situation).
//
// SOURCE-OF-TRUTH: X360 ARTIST pseudocode + asm is authoritative for behaviour; the DWARF
// (references/DecFIGS/.../BrnAboveCarRenderer.h) gives the declaration shapes/names. No
// Feb-2007 source exists for this TU.
//
// ASSERTS: the X360 build constructs dynamic assert text through CgsDev::Assert::gpcMessageBuffer
// + StrStream for the "unknown prepare/release stage" cases; per the project convention (see
// BrnSatNavRenderer.cpp's FireAboveCarAssert helper) these are lowered to the static-message
// Begin/Fire/End sequence with the recovered literal expression + the original file/line.

#include "GameSource/Gui/CustomRenderer/Renderers/BrnAboveCarRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // AddMonitor
#include "GameSource/Replays/BrnGuiModuleAboveCarObjectLayout.h"          // GuiModuleAboveCarObjectLayout::Clear
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h"            // GuiModuleStaticLayout (Update transfer target)

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

namespace BrnGui
{
namespace
{
const char* const KPC_FILE =
    "..\\..\\..\\GameSource\\Gui/CustomRenderer/Renderers/BrnAboveCarRenderer.cpp";

// The X360 builds these asserts through StrStream/gpcMessageBuffer; lowered to the static
// Begin/Fire/End sequence at the recovered file/line (mirrors BrnSatNavRenderer.cpp's
// FireSatNavAssert helper).
void FireAboveCarAssert(const char* lpcExpression, s32 liLine)
{
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(lpcExpression, KPC_FILE, liLine);
    CgsDev::Assert::EndAssert();
}
} // anonymous namespace

// 0x82454388 -- one-time zero-initialise every cached sub-state (gamer-tag cache, replay object
// layout cache, blend-state resource head, text object, perf-monitor handle) then register the
// CPU perf-monitor. X360 asserts `leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT` inside the
// zeroing loop bound check (a defensive over-run guard on the fixed 8-iteration loop; the C++
// for-loop below can never actually trip it, so the assert is reproduced as an unreachable guard
// for behavioural honesty rather than a live branch).
void AboveCarRenderer::Construct()
{
    CustomRenderComponentInterface::Construct();

    mePrepareStage  = E_PREPARESTAGE_START;   // +8
    meReleaseStage  = E_RELEASESTAGE_START;   // +12
    mpHeapAllocator = 0;                      // +16
    mpBlendState    = 0;                      // X360 +1300 (0x514) -- cross-attested by Release(),
                                               // which reads mpBlendState from this same offset.

    // X360 @0x824543B8 zeroes a single word at +1184 (0x4A0), immediately before the 80-byte
    // mRecentCrashSet sweep below and 16 bytes ahead of it in memory. Its DWARF identity is not
    // attested in the truncated header dump for this TU (the dump stops naming members at
    // miAboveCarRendererPM/h:217, and 1184 does not land on any member this TU's other bodies
    // cross-reference the way 1300/1280 do for mpBlendState/mBlendStateResource). Left
    // unattributed here rather than guessed -- FLAG: one X360 store not yet mapped to a named
    // member of this class.

    // Zero the 10-qword (80-byte) recent-crash bit set in one sweep (X360 @0x824543A0-CC: a
    // 10-iteration `*v2++ = 0` over BitArray<601>::maxBits, which is exactly 10 u64 fields --
    // UnSetAll() is that same all-fields-zero sweep). Size-matched to mRecentCrashSet (DWARF h:201)
    // rather than mBlendStateResource (DWARF h:204, a 20-byte Resource descriptor per the sibling
    // BrnSatNavRenderer.h Resource convention -- too small for this 80-byte sweep); mBlendStateResource
    // itself is NOT zeroed anywhere in this body (matches the X360, which never stores to +1280..1300
    // in Construct).
    mRecentCrashSet.UnSetAll();

    mTextObject.Construct(0, 0);

    // Per-active-race-car caches: clear the gamer-tag info and the replay object layout in
    // lockstep (X360 @0x82454424-4470, 8 iterations, E_ACTIVE_RACE_CAR_INDEX_COUNT == 8).
    for (s32 liIndex = 0; liIndex < 8; ++liIndex)
    {
        PlayerGamerTagAboveCarInfo& lrInfo = maCachedPlayerGameTagInfos[liIndex];
        lrInfo.mbUsed          = false;
        lrInfo.macRivalName[0] = '\0';
        lrInfo.mpRivalName     = 0;
        lrInfo.macPositionText[0] = '\0';
        lrInfo.mpPositionText  = 0;

        maAboveCarObjectLayouts[liIndex].Clear();

        // X360 defensive bound check on the loop counter (never trips for this fixed 8-iteration
        // for-loop; reproduced for behavioural honesty).
        const s32 liNextIndex = liIndex + 1;
        if (liNextIndex > 8)
        {
            FireAboveCarAssert("leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT", 39);
        }
    }

    // [verify H 2026-09-03] CORRECTED: the store at +1714 is `stb r30(=0, li r30,0 @0x8245439C), 0x6B2(this)`
    // @0x82454478 -- a BYTE into mbTimeExtensionPending (DWARF h:214, +0x6B2). The s16 miTimeExtension
    // sits at +0x6B0 (1712) and is NOT written by Construct (RecvEvent id 427 @0x8245499C writes it).
    mbTimeExtensionPending = false;   // stb 0, 0x6B2 @0x82454478
    miAboveCarRendererPM = -1; // +1716 (X360 pre-seeds -1 before AddMonitor overwrites it below)

    miAboveCarRendererPM = CgsDev::PerfMonCpu::AddMonitor("AboveCarRenderer", 3, 0, 2.0, 0, 0);
    if (miAboveCarRendererPM < 0)
    {
        FireAboveCarAssert("miAboveCarRendererPM >= 0", 184);
    }
}

// 0x824469A8 -- two-stage prepare (mirrors Release's two-stage machine). On the first call
// (START) latch the resource allocator and advance to DONE; subsequent calls short-circuit true.
// DWARF signature: Prepare(GuiEventQueueSmall*, rw::IResourceAllocator*, rw::IResourceAllocator*)
// -- register mapping this=r3, GuiEventQueueSmall*=r4, 1st IResourceAllocator*=r5,
// 2nd IResourceAllocator*=r6. The X360 body only ever reads r5 (`stw r5, 0x10(r11)` @0x82446A2C,
// r11==this at that point) and never touches r4/r6, so the FIRST resource-allocator argument
// (lpA, the base interface's 2nd formal) is what gets stored into mpHeapAllocator -- matching
// the sibling SatNavRenderer::Prepare's "stores its first allocator arg" convention (not the
// inverse, as an earlier reading of the raw pseudocode's renumbered a1/a2/a3 locals suggested).
bool AboveCarRenderer::Prepare(void* lpResourceAllocator, void* lpA, void* lpB)
{
    (void)lpResourceAllocator;  // DWARF: GuiEventQueueSmall*, unused by this stage machine
    (void)lpB;                  // DWARF: 2nd rw::IResourceAllocator*, unused

    const s32 liStage = static_cast<s32>(mePrepareStage);
    if (liStage != E_PREPARESTAGE_START)
    {
        if (liStage != E_PREPARESTAGE_DONE)
        {
            FireAboveCarAssert(" unknown prepare stage in AboveCarRenderer ", 224);
            return false;
        }
    }
    else
    {
        mpHeapAllocator = static_cast<rw::IResourceAllocator*>(lpA);
    }

    mePrepareStage = E_PREPARESTAGE_DONE;
    return true;
}

// 0x82446A58 -- two-stage release. On the first call (START) release the owned blend state
// through the resource allocator's destroy slot (vtable +0x14) if one was created, then advance
// to DONE; subsequent calls short-circuit true.
bool AboveCarRenderer::Release()
{
    const s32 liStage = static_cast<s32>(meReleaseStage);
    if (liStage != E_RELEASESTAGE_START)
    {
        if (liStage != E_RELEASESTAGE_DONE)
        {
            FireAboveCarAssert(" unknown release stage in AboveCarRenderer ", 270);
            return false;
        }
    }
    else
    {
        // X360 @0x82446ADC-B00: if mpBlendState (+0x514) was ever created, destroy it through the
        // heap allocator's destroy slot: (*(*mpHeapAllocator + 0x14))(mpHeapAllocator,
        // &mBlendStateResource). rw::IResourceAllocator is a forward-declared opaque interface
        // everywhere in this codebase (no committed vtable/Destroy member to dispatch through
        // yet), so the dispatch itself is DECLINED here (not fabricated) -- the guard condition
        // is kept faithful even though its body is a no-op. Store-for-store the X360 also does
        // NOT null out mpBlendState afterward (unlike SatNavRenderer::Release's texture-state
        // members), so nothing is cleared here either.
        (void)(mpBlendState != 0); // guard reproduced; dispatch through mpHeapAllocator omitted
    }

    meReleaseStage = E_RELEASESTAGE_DONE;
    return true;
}

// 0x82446B28 -- mirror this renderer's live gamer-tag/score state to or from the replay
// serialiser's static layout buffer, depending on the serialiser's current mode. mpGuiModuleSerialiser
// (X360 +1440) is read through its mode dword (BaseSerialiser::meMode via GetMode()); modes
// {RECORDING_PREPARING, RECORDING, RECORDING_STALLED} push our state OUT to the static layout,
// modes {PLAYING_PREPARING, PLAYING, PLAYING_STALLED} pull it back IN. Both directions copy the
// same 256-byte block (X360 offset +1456..+1712, i.e. maAboveCarObjectLayouts, matching
// GuiModuleStaticLayout::maCarRecordsA's 8*32-byte X360 extent) to/from the static layout's
// +0x190 (400) field -- BaseSerialiser::GetStaticBuffer() + 400 == &GuiModuleStaticLayout::
// maCarRecordsA. The transfer size is sizeof(maAboveCarObjectLayouts) rather than the X360's
// literal 256: on this 64-bit host GuiModuleAboveCarObjectLayout is NOT byte-exact to the X360's
// 32-byte stride (its own TU models it as 28 bytes of named/opaque storage, no padding), so a
// literal 256-byte transfer would over-read/-write past the host array; copying the host
// object's own size (clamped to the destination's own extent) is the semantic-parity
// equivalent per the project's x64-gate policy.
//
// The concrete leaf type (BrnReplays::GuiModuleSerialiser) is a `.cpp`-local compile-only slice
// in BrnReplayGuiModuleSerialiser.cpp with no shared header; mpGuiModuleSerialiser is typed as
// the real, fully-shared BrnReplays::BaseSerialiser ancestor instead (see the header comment),
// and GetStaticLayout()'s effect (assert the buffer is big enough, reinterpret it as the leaf's
// static-layout type) is reproduced directly against GuiModuleStaticLayout via GetStaticBuffer().
void AboveCarRenderer::Update()
{
    using BrnReplays::BaseSerialiser;

    if (mpGuiModuleSerialiser == 0)
    {
        return;
    }

    const BaseSerialiser::EMode leMode = mpGuiModuleSerialiser->GetMode();
    const size_t kTransferSize = sizeof(maAboveCarObjectLayouts) < sizeof(BrnReplays::GuiModuleStaticLayout::maCarRecordsA)
                                      ? sizeof(maAboveCarObjectLayouts)
                                      : sizeof(BrnReplays::GuiModuleStaticLayout::maCarRecordsA);

    // RECORDING family -> push our cached layout OUT to the serialiser's static buffer.
    if (leMode == BaseSerialiser::E_MODE_RECORDING_PREPARING ||
        leMode == BaseSerialiser::E_MODE_RECORDING ||
        leMode == BaseSerialiser::E_MODE_RECORDING_STALLED)
    {
        BrnReplays::GuiModuleStaticLayout* const lpStaticLayout =
            reinterpret_cast<BrnReplays::GuiModuleStaticLayout*>(mpGuiModuleSerialiser->GetStaticBuffer());
        std::memcpy(lpStaticLayout->maCarRecordsA, &maAboveCarObjectLayouts[0], kTransferSize);
        return;
    }

    // PLAYING family -> pull the serialiser's static buffer IN to our cached layout.
    if (leMode == BaseSerialiser::E_MODE_PLAYING_PREPARING ||
        leMode == BaseSerialiser::E_MODE_PLAYING ||
        leMode == BaseSerialiser::E_MODE_PLAYING_STALLED)
    {
        BrnReplays::GuiModuleStaticLayout* const lpStaticLayout =
            reinterpret_cast<BrnReplays::GuiModuleStaticLayout*>(mpGuiModuleSerialiser->GetStaticBuffer());
        std::memcpy(&maAboveCarObjectLayouts[0], lpStaticLayout->maCarRecordsA, kTransferSize);
    }
}

// 0x82446BE8 -- the component's CgsID (64-bit content hash), constant-folded in the X360 asm to
// 0x4DCEFF7CA8C29C00. The base virtual slot is declared returning a u32 component id (see
// SatNavRenderer::GetComponentID); return the distinguishing low word the manager keys on.
// FLAG: the base widens CgsID(64) -> u32 here, matching the SatNavRenderer precedent.
u32 AboveCarRenderer::GetComponentID() const
{
    return 0xA8C29C00u;
}

// ---------------------------------------------------------------------------------------------
// RenderComponent / RenderBankingScores -- DECLARATION-ONLY.
//
// Both are massive (RenderComponent ~35 callees, RenderBankingScores ~25 callees) render bodies
// that draw the above-car markers, gamer tags, scores and banked-score sign popups through the
// immediate-mode CgsGraphics::BasicColouredTexturedVertex API (CgsIm3d.cpp / the committed
// ImRenderer<BasicColouredTexturedVertex> instantiation) -- NOT the 2D
// Basic2dColouredTexturedVertex API BrnSatNavRenderer.cpp uses (this renderer draws in 3D world
// space above each car, projected to screen via BrnDirector::Camera::Utils::
// ProjectWorldSpacePointToScreen, itself still [todo]).
//
// Both bodies are gated on BrnGui::GuiCache methods that are still [todo] / class-blocked
// (class:BrnGui::GuiCache status: blocked, 39/60 methods un-homed):
//   RenderBankingScores needs: IsActiveRaceCarIndexUsed, IsRaceCarCrashing,
//     GetOnlinePlayerInCarSelect, GetRaceCarPosition, GetEventPositionOfRaceCar.
//   RenderComponent additionally needs: IsActiveRaceCarConnecting, GetOnlinePlayerInCarSelect,
//     GetRaceCarPosition, GetEventPositionOfRaceCar, PLUS the un-homed sibling helpers
//     BrnGui::AboveCarRenderer::RenderReplayAboveCar (not in this TU's ledger; declared nowhere
//     yet) and SetTransformMatrixForCar (DWARF-declared but not a ledger function of this TU),
//     and several X360 addresses the decompiler could not attribute a name to (sub_82459190,
//     sub_82459460, sub_82458898 et al -- NOT modelled as fabricated stand-ins here; a prior
//     Wave38 attempt at this TU was reverted for doing exactly that).
//
// Per AGENTS.md, functions that cannot be reconstructed faithfully without fabricating layout,
// rodata, or un-homed callee behaviour are left declaration-only with an honest note rather than
// force-fabricated for coverage. When class:BrnGui::GuiCache's remaining methods and
// RenderReplayAboveCar/SetTransformMatrixForCar land, this TU can be revisited to body these two.
// ---------------------------------------------------------------------------------------------

} // namespace BrnGui
