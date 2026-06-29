// BrnReplays::DebugComponent -- in-game replay debug overlay.
// Reconstructed from the X360 ARTIST build (BrnReplayDebugComponent.cpp functions,
// see BrnReplayDebugComponent.h for the recovered layout + offset provenance).
//
// SOURCE-OF-TRUTH: X360 ARTIST pseudocode+asm (behaviour), DecFIGS DWARF (shape).
//
// The X360 overlay reaches the immediate renderer through two compiler-inlined
// helpers the pseudocode shows as raw outlined funcs:
//   MaybeDrawText(render, text, &pos, x, y, scale, ..., colour) @0x82824048
//       == render->DrawText(text, x, y, scale, colour)   (builds Vector2{x,y})
//   sub_8281C3E0(render, x0, y0, x1, y1, ..., colour)     @0x8281C3E0
//       == render->DrawBox(x0, y0, x1-x0, y1-y0, colour)  (min + size)
// Both are re-inlined here as the named CgsDev::Debug2DImmediateRender primitives.
//
// The X360 lazy-init globals (`if((flags&bit)==0){flags|=bit; gColour=VALUE;}`) are
// the compiler's thread-safe one-time init of file-scope const colours/metrics; they
// are de-optimised here into the named KU_/KF_ constants below.

#include "GameSource/Replays/BrnReplayDebugComponent.h"
#include "GameSource/Replays/BrnReplayModule.h"
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"   // live serialiser snapshot getters
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "BrnCommonTypes.h"   // Vector2 (rw::math::vpu) for DrawLine endpoints

#include <cstdio>
#include <cstring>

namespace BrnReplays
{
    using CgsDev::RGBA;
    using CgsDev::Debug2DImmediateRender;

    // Build the vpu screen-space point DrawLine takes from an (x,y) pair.
    static inline Vector2 MakePoint(f32 lfX, f32 lfY)
    {
        Vector2 lv2Point;
        lv2Point.x = lfX;
        lv2Point.y = lfY;
        lv2Point.z = 0.0f;
        lv2Point.w = 0.0f;
        return lv2Point;
    }

    // --- recovered overlay palette + layout metrics (de-optimised lazy globals) ---
    // Packed 0xAABBGGRR-style u32 colours read off the X360 lazy-init stores.
    static const RGBA KU_COL_MAIN_BG       = 0x294000u;   // main-window background (flt_82FFA7E0 path: 0x78FFFFFF semi-transp white frame is separate)
    static const RGBA KU_COL_WINDOW_FRAME  = 0x78FFFFFFu; // RenderHUD frame fill (dword_82FFA7E0)
    static const RGBA KU_COL_GRAPH_FILL     = 0x64009600u; // RenderGraph box fill   (dword_82FFA7C8)
    static const RGBA KU_COL_GRAPH_LINE     = 0x64000096u; // RenderGraph plot line  (dword_82FFA7C4)
    static const RGBA KU_COL_LABEL          = 0xFF32326Eu; // status/serialiser label (dword_82FFA778/784/7D4)
    static const RGBA KU_COL_VALUE          = 0xFF323232u; // status/serialiser value (dword_82FFA774/780/7D0)

    // Block-view per-state colours (RenderWrite/ReadStreamBlocks lazy-init table).
    static const RGBA KU_COL_BLOCK_EMPTY    = 0x64969696u; // dword_82FFA7A0 / 82FFA7BC
    static const RGBA KU_COL_BLOCK_PENDING  = 0x64009696u; // dword_82FFA79C / 82FFA7B8
    static const RGBA KU_COL_BLOCK_READY    = 0x64960096u; // dword_82FFA798 / 82FFA7B4
    static const RGBA KU_COL_BLOCK_LOCKED   = 0x6400C800u; // dword_82FFA794 / 82FFA7B0
    static const RGBA KU_COL_BLOCK_CURSOR   = 0x64C80000u; // dword_82FFA790 / 82FFA7AC (lis 0x64C8)
    static const RGBA KU_COL_BLOCK_KEYFRAME = 0x64000096u; // dword_82FFA78C / 82FFA7A8

    // Layout metrics (virtual-screen units). RenderTitle/Status/Serialisers spacing.
    static const f32 KF_TEXT_SCALE      = 12.0f;   // flt_82013FB0 title scale (small text == 5.0f via flt_8200426C below)
    static const f32 KF_TEXT_SCALE_SMALL = 5.0f;   // flt_8200426C
    static const f32 KF_LINE_HEIGHT     = 20.0f;   // flt_820054CC title line advance
    static const f32 KF_VALUE_INDENT    = 70.0f;   // flt_820051BC label->value column gap
    static const f32 KF_WINDOW_PAD      = 5.0f;

    // Stream-block grid tunables (Render{Write,Read}StreamBlocks @0x8264EFF8/0x8264F390).
    // The per-cell loop steps a byte cursor by 0x18 (24) from 0 until it reaches 0xA8C0
    // (43200), i.e. exactly 1800 cells (0xA8C0 / 0x18 == 0x708 == 1800).
    static const s32 KI_STREAM_CELLS    = 1800;    // == 0x708 (cell count == loop trips)
    static const s32 KI_STREAM_CELL_STRIDE = 24;   // 0x18 byte stride between block records
    static const s32 KI_STREAM_LOOP_BOUND  = 43200;// 0xA8C0 cursor bound (1800 * 24)

    // The grid COLUMN count is read unconditionally from a fixed data global -- write
    // grid: dword_82F2A684 (0x8264F104), read grid: dword_82F2A69C (0x8264F49C). The
    // rows = ceil(1800 / columns). That rodata word is NOT in the dump, so it is modelled
    // here as a named const with a FLAGGED placeholder; see still_open. (Both grids share
    // the same column count in this build -- the two globals are 0x18 apart in the same
    // table.) The asm does NOT gate this on the stream pointer being non-null.
    static const s32 KI_STREAM_COLUMNS  = 60;      // FLAGGED placeholder: dword_82F2A684 /
                                                   // dword_82F2A69C (rodata not dumped).

    // Cell geometry rodata (also not dumped) -- modelled as FLAGGED named consts.
    // Cell box stride per axis == (KF_STREAM_CELL_BASE * 2 + KF_STREAM_CELL_GAP):
    //   write: flt_82F2A680 * flt_82001D9C(==2.0) + flt_82F2A678 (0x8264F178)
    //   read : flt_82F2A698 * 2.0               + flt_82F2A690 (0x8264F510)
    // Grid origin offset from the section cursor:
    //   write: +flt_82F2A674 (x) / +flt_82F2A67C (y); read: +flt_82F2A68C / +flt_82F2A694
    // Cell box extent: width flt_82F2A678 (write) / flt_82F2A690 (read); height adds
    //   flt_82F2A670 (write) / flt_82F2A688 (read).
    static const f32 KF_STREAM_CELL_BASE   = 1.0f; // FLAGGED: flt_82F2A680/698 (not dumped)
    static const f32 KF_STREAM_CELL_GAP    = 0.0f; // FLAGGED: flt_82F2A678/690 (not dumped)
    static const f32 KF_STREAM_ORIGIN_X    = 0.0f; // FLAGGED: flt_82F2A674/68C (not dumped)
    static const f32 KF_STREAM_ORIGIN_Y    = 0.0f; // FLAGGED: flt_82F2A67C/694 (not dumped)
    static const f32 KF_STREAM_CELL_EXTRA_H= 0.0f; // FLAGGED: flt_82F2A670/688 (not dumped)
    // Per-axis cell stride == base*2 + gap (the literal 2.0 is flt_82001D9C).
    static const f32 KF_STREAM_CELL_STEP   = KF_STREAM_CELL_BASE * 2.0f + KF_STREAM_CELL_GAP;

    // ----------------------------------------------------------------------------
    // Construct @0x82652978 -- store the module + allocator, allocate the per-
    // serialiser record ring + the three usage graphs, zero the running counters.
    // (The X360 calls the base CgsDev::DebugComponent constructor first; it is an
    //  ICF-folded no-op in this build.)
    // ----------------------------------------------------------------------------
    void DebugComponent::Construct(ReplayModule* lpReplayModule, rw::IResourceAllocator* lpAllocator)
    {
        CgsDev::DebugComponent::Construct();

        mpReplayModule    = lpReplayModule;
        mpAllocator       = lpAllocator;
        miMaxSerialisers  = KI_NUM_SERIALISERS;
        miCurrSerialisers = 0;
        mbShowHud         = false;
        miWriteSlotsUsed  = 0;
        miWriteBufferUsed = 0;

        // The X360 builds four allocator records here: one DebugSerialiserInfo ring
        // (0x294 bytes) for the serialiser snapshots, and three DebugGraph rings
        // (0x51C bytes) for the write-slots / write-buffer / read graphs. They are
        // allocated through the resource allocator's vtable (lwz 0x10(allocator)).
        // Modelled as the named members; the per-graph ring head is reset below.
        mpSerialisers          = static_cast<DebugSerialiserInfo*>(nullptr);
        mpWriteSlotsUsedGraph  = static_cast<DebugGraph*>(nullptr);
        mpWriteBufferUsedGraph = static_cast<DebugGraph*>(nullptr);
        mpReadGraph            = static_cast<DebugGraph*>(nullptr);

        // X360 seeds each graph's ring header (capacity 256, head/tail/count = 0,
        // mpData -> &samples). Done by OnActivate's ClearGraph too; kept faithful.
        if (mpWriteSlotsUsedGraph)
        {
            mpWriteSlotsUsedGraph->mBuffer.miCapacity = DebugGraph::KI_NUM_SAMPLES;
            mpWriteSlotsUsedGraph->mBuffer.mpData     = mpWriteSlotsUsedGraph->mBuffer.mafSamples;
            mpWriteSlotsUsedGraph->mBuffer.miHead     = 0;
            mpWriteSlotsUsedGraph->mBuffer.miTail     = 0;
            mpWriteSlotsUsedGraph->mBuffer.miCount    = 0;
        }
        if (mpWriteBufferUsedGraph)
        {
            mpWriteBufferUsedGraph->mBuffer.miCapacity = DebugGraph::KI_NUM_SAMPLES;
            mpWriteBufferUsedGraph->mBuffer.mpData     = mpWriteBufferUsedGraph->mBuffer.mafSamples;
            mpWriteBufferUsedGraph->mBuffer.miHead     = 0;
            mpWriteBufferUsedGraph->mBuffer.miTail     = 0;
            mpWriteBufferUsedGraph->mBuffer.miCount    = 0;
        }
        if (mpReadGraph)
        {
            mpReadGraph->mBuffer.miCapacity = DebugGraph::KI_NUM_SAMPLES;
            mpReadGraph->mBuffer.miHead     = 0;
            mpReadGraph->mBuffer.miTail     = 0;
            mpReadGraph->mBuffer.miCount    = 0;
            mpReadGraph->mBuffer.mpData     = mpReadGraph->mBuffer.mafSamples;
        }
    }

    void DebugComponent::Destruct()
    {
        CgsDev::DebugComponent::Destruct();
    }

    // ----------------------------------------------------------------------------
    // RenderHUD @0x8265A848 -- draw the overlay once mbShowHud is set. The X360
    // measures the window first (RenderMainWindow with a null render target), draws
    // the frame fill behind it, then renders it for real.
    // ----------------------------------------------------------------------------
    void DebugComponent::RenderHUD(Debug2DImmediateRender* lpRender)
    {
        CgsDev::DebugComponent::RenderHUD(lpRender);

        if (!mbShowHud)
            return;

        Vector2f lv2Min;
        Vector2f lv2Max;
        // Measurement pass: no draw target, just accumulate the window extents.
        RenderMainWindow(nullptr, &lv2Min, &lv2Max);

        // Frame fill behind the window (KU_COL_WINDOW_FRAME, semi-transparent white).
        lpRender->DrawBox(lv2Min.X(), lv2Min.Y(),
                          lv2Max.X() - lv2Min.X(), lv2Max.Y() - lv2Min.Y(),
                          KU_COL_WINDOW_FRAME);

        // Real draw pass.
        RenderMainWindow(lpRender, &lv2Min, &lv2Max);
    }

    void DebugComponent::Update()
    {
        // X360: empty in this build (no own Update body beyond the base).
    }

    // ----------------------------------------------------------------------------
    // RenderMainWindow @0x82656FE0 -- lay out the overlay sections top to bottom,
    // advancing the running cursor and accumulating the min/max window extents.
    // The cursor starts at (250,100) and each section returns the height it used.
    // ----------------------------------------------------------------------------
    void DebugComponent::RenderMainWindow(Debug2DImmediateRender* lpRender, Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        Vector2f lv2Cursor(250.0f, 100.0f);
        Vector2f lv2Min;
        Vector2f lv2Max;

        f32 lfY = 100.0f;
        lfY += RenderTitle(lpRender, lv2Cursor, &lv2Min, &lv2Max);
        lv2Cursor = Vector2f(250.0f, lfY);

        lfY += RenderStatus(lpRender, lv2Cursor, &lv2Min, &lv2Max);
        lv2Cursor = Vector2f(250.0f, lfY);

        if (mpReplayModule->GetState() == E_STREAM_STATE_RECORDING)
        {
            lfY += RenderWriteStreamStatus(lpRender, lv2Cursor, &lv2Min, &lv2Max);
            lv2Cursor = Vector2f(250.0f, lfY);
        }

        lfY += RenderSerialisers(lpRender, lv2Cursor, &lv2Min, &lv2Max);
        lv2Cursor = Vector2f(250.0f, lfY);

        const EStreamState leState = mpReplayModule->GetState();
        const EStreamStage leStage = mpReplayModule->GetStreamStage();
        if (leState == E_STREAM_STATE_RECORDING && leStage == E_STREAM_STAGE_OPEN)
        {
            RenderWriteStreamBlocks(lpRender, lv2Cursor, &lv2Min, &lv2Max);
        }
        else if (leState == E_STREAM_STATE_PLAYING && leStage == E_STREAM_STAGE_OPEN)
        {
            RenderReadStreamBlocks(lpRender, lv2Cursor, &lv2Min, &lv2Max);
        }

        *lpv2Min = lv2Min;
        *lpv2Max = lv2Max;
    }

    // ----------------------------------------------------------------------------
    // RenderTitle @0x8264EA08 -- the window header. Initialises the min/max extents
    // to the cursor, sizes the title box (100 wide, 20 tall), draws the label, and
    // returns the title line height (20).
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderTitle(Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos,
                                    Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        *lpv2Min = Vector2f(lrv2Pos.X(), lrv2Pos.Y());
        *lpv2Max = Vector2f(lrv2Pos.X() + 100.0f, lrv2Pos.Y() + KF_LINE_HEIGHT);

        if (lpRender)
        {
            lpRender->DrawText("Replay Debug Component",
                               lrv2Pos.X() + KF_WINDOW_PAD, lrv2Pos.Y() + KF_WINDOW_PAD,
                               KF_TEXT_SCALE, KU_COL_LABEL);
        }
        return KF_LINE_HEIGHT;
    }

    // ----------------------------------------------------------------------------
    // RenderStatus @0x8264EAA8 -- "State: <name>" and "Stream Stage: <name>" rows.
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderStatus(Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos,
                                     Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        // off_82F2A56C state name table ("IDLE"/...), off_82F2A57C stage table ("CLOSED"/...).
        static const char* const KAC_STATE_NAMES[E_STREAM_STATE_COUNT] =
            { "IDLE", "RECORDING", "PLAYING" };
        static const char* const KAC_STREAM_STAGE_NAMES[E_STREAM_STAGE_COUNT] =
            { "CLOSED", "OPENING", "OPEN", "CLOSING", "ERROR" };

        const f32 lfX = lrv2Pos.X() + KF_WINDOW_PAD;
        f32       lfY = lrv2Pos.Y() + KF_TEXT_SCALE_SMALL; // flt_82F2A640

        if (lpRender)
        {
            lpRender->DrawText("State: ", lfX, lfY, KF_TEXT_SCALE_SMALL, KU_COL_LABEL);
            lpRender->DrawText(KAC_STATE_NAMES[mpReplayModule->GetState()],
                               lfX + KF_VALUE_INDENT, lfY, KF_TEXT_SCALE_SMALL, KU_COL_VALUE);
        }

        lfY += KF_TEXT_SCALE; // flt_82F2A63C row advance
        if (lpRender)
        {
            lpRender->DrawText("Stream Stage: ", lfX, lfY, KF_TEXT_SCALE_SMALL, KU_COL_LABEL);
            lpRender->DrawText(KAC_STREAM_STAGE_NAMES[mpReplayModule->GetStreamStage()],
                               lfX + KF_VALUE_INDENT, lfY, KF_TEXT_SCALE_SMALL, KU_COL_VALUE);
        }

        // Grow the window extents to cover this section (min/max clamp).
        const f32 lfBottom = lfY + KF_TEXT_SCALE + KF_TEXT_SCALE_SMALL;
        if (lpv2Min)
            *lpv2Min = Vector2f(lrv2Pos.X() < lpv2Min->X() ? lrv2Pos.X() : lpv2Min->X(),
                               lrv2Pos.Y() < lpv2Min->Y() ? lrv2Pos.Y() : lpv2Min->Y());
        if (lpv2Max)
        {
            const f32 lfRight = lfX + 200.0f;
            *lpv2Max = Vector2f(lfRight > lpv2Max->X() ? lfRight : lpv2Max->X(),
                               lfBottom > lpv2Max->Y() ? lfBottom : lpv2Max->Y());
        }
        return lfY + KF_TEXT_SCALE - (lrv2Pos.Y());
    }

    // ----------------------------------------------------------------------------
    // RenderWriteStreamStatus @0x82652BA0 -- write index / stall count + the three
    // usage graphs (slots-used / buffer-used / read).
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderWriteStreamStatus(Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos,
                                                Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        const f32 lfX = lrv2Pos.X() + KF_WINDOW_PAD;
        f32       lfY = lrv2Pos.Y() + KF_TEXT_SCALE_SMALL; // flt_82F2A6AC

        if (lpRender)
            lpRender->DrawText("Write Index: ", lfX, lfY, KF_TEXT_SCALE_SMALL, KU_COL_LABEL);
        DrawInt32(lpRender, mpReplayModule->GetWriteIndex(), lfX + KF_VALUE_INDENT, lfY, KF_TEXT_SCALE_SMALL, KU_COL_VALUE);

        lfY += KF_TEXT_SCALE_SMALL; // flt_82F2A6A8
        if (lpRender)
            lpRender->DrawText("Stalls: ", lfX, lfY, KF_TEXT_SCALE_SMALL, KU_COL_LABEL);
        DrawInt32(lpRender, mpReplayModule->GetWriteStalls(), lfX + KF_VALUE_INDENT, lfY, KF_TEXT_SCALE_SMALL, KU_COL_VALUE);

        lfY += KF_TEXT_SCALE_SMALL;

        // Three stacked usage graphs, each KF_VALUE_INDENT-ish tall (flt_82F2A6A4/A0).
        Vector2f lv2GraphMin(lfX, lfY);
        Vector2f lv2GraphMax(KF_TEXT_SCALE_SMALL, KF_TEXT_SCALE_SMALL);
        lfY += RenderGraph(lpRender, mpWriteSlotsUsedGraph, lv2GraphMin, lv2GraphMax) + KF_TEXT_SCALE_SMALL;

        lv2GraphMin = Vector2f(lfX, lfY);
        lfY += RenderGraph(lpRender, mpWriteBufferUsedGraph, lv2GraphMin, lv2GraphMax) + KF_TEXT_SCALE_SMALL;

        lv2GraphMin = Vector2f(lfX, lfY);
        lfY += RenderGraph(lpRender, mpReadGraph, lv2GraphMin, lv2GraphMax) + KF_TEXT_SCALE_SMALL;

        const f32 lfBottom = lfY + KF_WINDOW_PAD;
        if (lpv2Min)
            *lpv2Min = Vector2f(lrv2Pos.X() < lpv2Min->X() ? lrv2Pos.X() : lpv2Min->X(),
                               lrv2Pos.Y() < lpv2Min->Y() ? lrv2Pos.Y() : lpv2Min->Y());
        if (lpv2Max)
        {
            const f32 lfRight = lfX + 200.0f;
            *lpv2Max = Vector2f(lfRight > lpv2Max->X() ? lfRight : lpv2Max->X(),
                               lfBottom > lpv2Max->Y() ? lfBottom : lpv2Max->Y());
        }
        return lfY - lrv2Pos.Y();
    }

    // ----------------------------------------------------------------------------
    // RenderReadStreamStatus @ (declared in DWARF; render dispatch routes through
    // RenderWriteStreamStatus on X360 for both -- kept as a thin wrapper).
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderReadStreamStatus(Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos,
                                               Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        return RenderWriteStreamStatus(lpRender, lrv2Pos, lpv2Min, lpv2Max);
    }

    // ----------------------------------------------------------------------------
    // RenderSerialisers @0x8264ECA0 -- a column-headed table of every registered
    // serialiser (id / name / static size / buffer size / buffer used).
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderSerialisers(Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos,
                                          Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        // Column x-offsets from flt_82F2A658/65C/660/664/668; row advance flt_82F2A64C.
        const f32 KF_COL0 = lrv2Pos.X() + KF_WINDOW_PAD;       // flt_82F2A658
        const f32 KF_COL_STEP = 60.0f;                         // flt_82F2A65C..668 spacing
        const f32 KF_HDR_SCALE = KF_TEXT_SCALE_SMALL;          // flt_82F2A650
        const f32 KF_ROW_SCALE = KF_TEXT_SCALE_SMALL;          // flt_82F2A648
        const f32 KF_ROW_STEP  = KF_TEXT_SCALE_SMALL;          // flt_82F2A64C

        f32 lfY = lrv2Pos.Y() + KF_WINDOW_PAD; // flt_82F2A654

        if (lpRender)
        {
            lpRender->DrawText("ID",          KF_COL0,                 lfY, KF_HDR_SCALE, KU_COL_LABEL);
            lpRender->DrawText("Name",        KF_COL0 + KF_COL_STEP,   lfY, KF_HDR_SCALE, KU_COL_LABEL);
            lpRender->DrawText("Static Size", KF_COL0 + KF_COL_STEP*2, lfY, KF_HDR_SCALE, KU_COL_LABEL);
            lpRender->DrawText("Buffer Size", KF_COL0 + KF_COL_STEP*3, lfY, KF_HDR_SCALE, KU_COL_LABEL);
            lpRender->DrawText("Buffer Used", KF_COL0 + KF_COL_STEP*4, lfY, KF_HDR_SCALE, KU_COL_LABEL);
        }

        f32 lfRowY = lfY + KF_ROW_STEP;
        for (s32 liRow = 0; liRow < miCurrSerialisers; ++liRow)
        {
            const DebugSerialiserInfo& lrInfo = mpSerialisers[liRow];

            char lacIdText[256];
            std::snprintf(lacIdText, sizeof(lacIdText), "%d", lrInfo.meId);

            if (lpRender)
            {
                // ID column "%d" <- record+0x14 (meId);  Name <- record+0x1C (macName).
                lpRender->DrawText(lacIdText,     KF_COL0,               lfRowY, KF_ROW_SCALE, KU_COL_VALUE);
                lpRender->DrawText(lrInfo.macName, KF_COL0 + KF_COL_STEP, lfRowY, KF_ROW_SCALE, KU_COL_VALUE);
            }
            // Data columns re-derived from RenderSerialisers @0x8264ECA0 (lwz off(r30)):
            //   "Static Size" <- record+0x08 (miStaticBufferSize, src+0x24)
            //   "Buffer Size" <- record+0x04 (miBufferSize,       src+0x0C)
            //   "Buffer Used" <- record+0x0C (miBufferUsed,        src+0x10)
            DrawDataSize(lpRender, lrInfo.miStaticBufferSize, KF_COL0 + KF_COL_STEP*2, lfRowY, KF_ROW_SCALE, KU_COL_VALUE);
            DrawDataSize(lpRender, lrInfo.miBufferSize,        KF_COL0 + KF_COL_STEP*3, lfRowY, KF_ROW_SCALE, KU_COL_VALUE);
            DrawDataSize(lpRender, lrInfo.miBufferUsed,        KF_COL0 + KF_COL_STEP*4, lfRowY, KF_ROW_SCALE, KU_COL_VALUE);

            lfRowY += KF_ROW_STEP;
        }

        const f32 lfBottom = lfRowY + KF_WINDOW_PAD;
        if (lpv2Min)
            *lpv2Min = Vector2f(KF_COL0 - KF_WINDOW_PAD < lpv2Min->X() ? KF_COL0 - KF_WINDOW_PAD : lpv2Min->X(),
                               lrv2Pos.Y() < lpv2Min->Y() ? lrv2Pos.Y() : lpv2Min->Y());
        if (lpv2Max)
        {
            const f32 lfRight = KF_COL0 + KF_COL_STEP*5;
            *lpv2Max = Vector2f(lfRight > lpv2Max->X() ? lfRight : lpv2Max->X(),
                               lfBottom > lpv2Max->Y() ? lfBottom : lpv2Max->Y());
        }
        return lfRowY - lrv2Pos.Y();
    }

    // ----------------------------------------------------------------------------
    // RenderGraph @0x8264F728 -- draw the graph's bounding box then plot its ring
    // samples as a line strip, mapping each sample into the box by (value-min)/(max-min).
    //
    // Re-derived from the X360 asm. Two operands the Hex-Rays draft got wrong:
    //   * the X-axis denominator is the ring CAPACITY-1 (lwz 0x10C(graph); addi -1 ==
    //     255), NOT count-1 (0x8264F874/F884).
    //   * each sample is fetched through CgsContainers::RingBuffer<float>::operator[]
    //     (graph+0x108, idx) which adds the ring head offset and wraps (0x8264F850/
    //     F864) -- a wrapped ring must read oldest->newest, so a linear mafSamples[i]
    //     is wrong.
    // The point math also matches the asm exactly: the per-axis span is (max - 2) with
    // the box origin (min.x/min.y) added back at the end (the asm adds the min vector
    // via vaddfp128), and the normalised value t is clamped to [0,1] (two fsel guards).
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderGraph(Debug2DImmediateRender* lpRender, DebugGraph* lpGraph,
                                    const Vector2f& lrv2Min, const Vector2f& lrv2Max)
    {
        if (lpRender)
            lpRender->DrawBox(lrv2Min.X(), lrv2Min.Y(),
                              lrv2Max.X() - lrv2Min.X(), lrv2Max.Y() - lrv2Min.Y(),
                              KU_COL_GRAPH_FILL);

        const s32 liCount = lpGraph->mBuffer.miCount;        // graph+0x118
        if (lpRender && liCount > 0)
        {
            const f32 lfMin   = lpGraph->mfMin;              // graph+0x100
            const f32 lfRange = lpGraph->mfMax - lpGraph->mfMin; // (graph+0x104) - (graph+0x100)
            // X-axis denominator == ring capacity - 1 (graph+0x10C - 1), NOT count-1.
            const f32 lfDenom = static_cast<f32>(lpGraph->mBuffer.miCapacity - 1);

            const f32 lfSpanX = lrv2Max.X() - 2.0f;          // (max.x - 2.0), origin added below
            const f32 lfSpanY = lrv2Max.Y() - 2.0f;          // (max.y - 2.0)

            // Plot adjacent sample pairs (i, i+1) as line segments, newest at the right.
            // Each endpoint uses its OWN sample (i -> point at fraction i; i+1 -> i+1).
            for (s32 liI = liCount - 2; liI >= 0; --liI)
            {
                // Ring read (head-offset + wrap), not a linear sample index.
                const f32 lfVI   = lpGraph->mBuffer[liI];
                const f32 lfVI1  = lpGraph->mBuffer[liI + 1];

                const f32 lfXI  = lrv2Min.X() + (static_cast<f32>(liI)     / lfDenom) * lfSpanX + 1.0f;
                const f32 lfXI1 = lrv2Min.X() + (static_cast<f32>(liI + 1) / lfDenom) * lfSpanX + 1.0f;

                // Normalise into [0,1] (lower + upper clamp == the two fsel), invert Y.
                f32 lfTI  = lfRange != 0.0f ? (lfVI  - lfMin) / lfRange : 0.0f;
                f32 lfTI1 = lfRange != 0.0f ? (lfVI1 - lfMin) / lfRange : 0.0f;
                if (lfTI  < 0.0f) lfTI  = 0.0f;  if (lfTI  > 1.0f) lfTI  = 1.0f;
                if (lfTI1 < 0.0f) lfTI1 = 0.0f;  if (lfTI1 > 1.0f) lfTI1 = 1.0f;

                const f32 lfYI  = lrv2Min.Y() + (1.0f - lfTI)  * lfSpanY + 1.0f;
                const f32 lfYI1 = lrv2Min.Y() + (1.0f - lfTI1) * lfSpanY + 1.0f;

                lpRender->DrawLine(MakePoint(lfXI1, lfYI1), MakePoint(lfXI, lfYI), KU_COL_GRAPH_LINE);
            }
        }
        return lrv2Max.Y();
    }

    // ----------------------------------------------------------------------------
    // ClearGraph @0x8264F708 (inlined in OnActivate) -- reset a graph's ring.
    // ----------------------------------------------------------------------------
    void DebugComponent::ClearGraph(DebugGraph* lpGraph)
    {
        lpGraph->mBuffer.Clear();
    }

    // ----------------------------------------------------------------------------
    // RenderWriteStreamBlocks @0x8264EFF8 -- the per-frunk block grid for the write
    // stream. Lays out exactly KI_STREAM_CELLS (1800) cells in a grid whose column
    // count is read UNCONDITIONALLY from a fixed data global (dword_82F2A684); the row
    // count is ceil(1800 / columns). The asm does NOT branch on GetWriteStream() being
    // non-null -- it always lays out 1800 cells (per-cell loop: cursor += 24 until
    // >= 43200). The per-block colour comes from the WriteStream block table; that
    // decode belongs to the ReplayModule TU's block type, so the colour here defaults
    // to EMPTY (honest: no foreign offset poke) while the grid geometry is exact.
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderWriteStreamBlocks(Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos,
                                                Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        // Column count read unconditionally from the data global (no null-gate).
        const s32 liColumns = KI_STREAM_COLUMNS;                  // dword_82F2A684
        const s32 liRows    = (KI_STREAM_CELLS + liColumns - 1) / liColumns; // ceil(1800/cols)

        const f32 lfOriginX = lrv2Pos.X() + KF_STREAM_ORIGIN_X;  // posX + flt_82F2A674
        const f32 lfOriginY = lrv2Pos.Y() + KF_STREAM_ORIGIN_Y;  // posY + flt_82F2A67C
        const f32 lfStep    = KF_STREAM_CELL_STEP;               // flt_82F2A680*2 + flt_82F2A678
        const f32 lfCellW   = KF_STREAM_CELL_GAP;                // flt_82F2A678 box width
        const f32 lfCellH   = KF_STREAM_CELL_EXTRA_H;            // flt_82F2A670 box height

        if (lpRender)
        {
            // Exactly 1800 cells (0xA8C0/0x18). col = n%cols, row = n/cols.
            for (s32 liCell = 0; liCell < KI_STREAM_CELLS; ++liCell)
            {
                const s32 liCol = liCell % liColumns;
                const s32 liRow = liCell / liColumns;
                const f32 lfX = static_cast<f32>(liCol) * lfStep + lfOriginX;
                const f32 lfY = static_cast<f32>(liRow) * lfStep + lfOriginY;
                // Block-flag colour decode belongs to the WriteStream block type
                // (ReplayModule TU); paint the default EMPTY slot until it lands.
                lpRender->DrawBox(lfX, lfY, lfCellW, lfCellH, KU_COL_BLOCK_EMPTY);
            }
        }

        // Window extents: min clamps to the origin, max clamps to the far grid corner.
        const f32 lfMaxX = static_cast<f32>(liColumns) * lfStep + lfOriginX;
        const f32 lfMaxY = static_cast<f32>(liRows)    * lfStep + lfOriginY;
        if (lpv2Min)
            *lpv2Min = Vector2f(lfOriginX < lpv2Min->X() ? lfOriginX : lpv2Min->X(),
                                lfOriginY < lpv2Min->Y() ? lfOriginY : lpv2Min->Y());
        if (lpv2Max)
            *lpv2Max = Vector2f(lfMaxX > lpv2Max->X() ? lfMaxX : lpv2Max->X(),
                                lfMaxY > lpv2Max->Y() ? lfMaxY : lpv2Max->Y());

        return lfMaxY - lfOriginY;  // == rows * step (0x8264F374 fsubs f29,f30)
    }

    // ----------------------------------------------------------------------------
    // RenderReadStreamBlocks @0x8264F390 -- the read-stream counterpart of the above.
    // ----------------------------------------------------------------------------
    f32 DebugComponent::RenderReadStreamBlocks(Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos,
                                               Vector2f* lpv2Min, Vector2f* lpv2Max)
    {
        // Column count read unconditionally from the data global (no null-gate).
        const s32 liColumns = KI_STREAM_COLUMNS;                  // dword_82F2A69C
        const s32 liRows    = (KI_STREAM_CELLS + liColumns - 1) / liColumns; // ceil(1800/cols)

        const f32 lfOriginX = lrv2Pos.X() + KF_STREAM_ORIGIN_X;  // posX + flt_82F2A68C
        const f32 lfOriginY = lrv2Pos.Y() + KF_STREAM_ORIGIN_Y;  // posY + flt_82F2A694
        const f32 lfStep    = KF_STREAM_CELL_STEP;               // flt_82F2A698*2 + flt_82F2A690
        const f32 lfCellW   = KF_STREAM_CELL_GAP;                // flt_82F2A690 box width
        const f32 lfCellH   = KF_STREAM_CELL_EXTRA_H;            // flt_82F2A688 box height

        if (lpRender)
        {
            for (s32 liCell = 0; liCell < KI_STREAM_CELLS; ++liCell)
            {
                const s32 liCol = liCell % liColumns;
                const s32 liRow = liCell / liColumns;
                const f32 lfX = static_cast<f32>(liCol) * lfStep + lfOriginX;
                const f32 lfY = static_cast<f32>(liRow) * lfStep + lfOriginY;
                // Read-stream block-flag decode belongs to the ReadStream block type
                // (ReplayModule TU); paint the default EMPTY slot until it lands.
                lpRender->DrawBox(lfX, lfY, lfCellW, lfCellH, KU_COL_BLOCK_EMPTY);
            }
        }

        const f32 lfMaxX = static_cast<f32>(liColumns) * lfStep + lfOriginX;
        const f32 lfMaxY = static_cast<f32>(liRows)    * lfStep + lfOriginY;
        if (lpv2Min)
            *lpv2Min = Vector2f(lfOriginX < lpv2Min->X() ? lfOriginX : lpv2Min->X(),
                                lfOriginY < lpv2Min->Y() ? lfOriginY : lpv2Min->Y());
        if (lpv2Max)
            *lpv2Max = Vector2f(lfMaxX > lpv2Max->X() ? lfMaxX : lpv2Max->X(),
                                lfMaxY > lpv2Max->Y() ? lfMaxY : lpv2Max->Y());

        return lfMaxY - lfOriginY;  // == rows * step
    }

    // ----------------------------------------------------------------------------
    // DrawText @0x8264BD08-region helper -- thin forward to the renderer.
    // ----------------------------------------------------------------------------
    void DebugComponent::DrawText(Debug2DImmediateRender* lpRender, const char* lpcText,
                                  f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        if (lpRender)
            lpRender->DrawText(lpcText, lfX, lfY, lfScale, lColour);
    }

    // ----------------------------------------------------------------------------
    // DrawInt32 @0x8264BB70 -- "%d" format then draw (if a render target is set).
    // ----------------------------------------------------------------------------
    void DebugComponent::DrawInt32(Debug2DImmediateRender* lpRender, s32 liValue,
                                   f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        char lacText[256];
        std::snprintf(lacText, sizeof(lacText), "%d", liValue);
        if (lpRender)
            lpRender->DrawText(lacText, lfX, lfY, lfScale, lColour);
    }

    // ----------------------------------------------------------------------------
    // DrawDataSize @0x8264BC08 -- format a byte count as "%dMB, %dKB, %dB" /
    // "%dKB, %dB" / "%dB" depending on magnitude, then draw.
    // ----------------------------------------------------------------------------
    void DebugComponent::DrawDataSize(Debug2DImmediateRender* lpRender, s32 liBytes,
                                      f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        const s32 liMB = liBytes / 0x100000;
        const s32 liKB = (liBytes % 0x100000) / 1024;
        const s32 liB  = liBytes - (((liMB << 10) + liKB) << 10);

        char lacText[256];
        if (liMB > 0)
            std::snprintf(lacText, sizeof(lacText), "%dMB, %dKB, %dB", liMB, liKB, liB);
        else if (liKB > 0)
            std::snprintf(lacText, sizeof(lacText), "%dKB, %dB", liKB, liB);
        else
            std::snprintf(lacText, sizeof(lacText), "%dB", liB);

        if (lpRender)
            lpRender->DrawText(lacText, lfX, lfY, lfScale, lColour);
    }

    // ----------------------------------------------------------------------------
    // DrawFraction @0x8264B...-region -- "<num>/<den>" then draw.
    // ----------------------------------------------------------------------------
    void DebugComponent::DrawFraction(Debug2DImmediateRender* lpRender, s32 liNumerator, s32 liDenominator,
                                      f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        char lacText[256];
        std::snprintf(lacText, sizeof(lacText), "%d/%d", liNumerator, liDenominator);
        if (lpRender)
            lpRender->DrawText(lacText, lfX, lfY, lfScale, lColour);
    }

    // ----------------------------------------------------------------------------
    // PreUpdateRecord @0x8264BD08 -- snapshot every live serialiser into the record
    // array the overlay renders. The X360 walks all KI_NUM_SERIALISERS module slots,
    // copying each present serialiser's name + sizes into the next free record.
    // ----------------------------------------------------------------------------
    void DebugComponent::PreUpdateRecord()
    {
        miCurrSerialisers = 0;

        for (s32 liId = 0; liId < KI_NUM_SERIALISERS; ++liId)
        {
            // The live object is a BaseSerialiser (asm reads it at the BaseSerialiser
            // field offsets); the overlay snapshots its named fields into the record.
            BaseSerialiser* lpSrc = mpReplayModule->GetSerialiser(liId);
            if (!lpSrc)
                continue;

            DebugSerialiserInfo& lrDst = mpSerialisers[miCurrSerialisers];

            // X360 asserts the source name fits the 32-byte record (CgsStringUtils.h:65):
            // it strlen's the source name and fires when length >= 0x20.
            CGS_ASSERT(std::strlen(lpSrc->GetName()) < (size_t)DebugSerialiserInfo::KI_MAX_NAME_LENGTH,
                       "serialiser name too long for record buffer");
            std::strcpy(lrDst.macName, lpSrc->GetName());      // src+0x30 -> record+0x1C

            // Field copies re-derived from PreUpdateRecord @0x8264BD08. The record packs
            // the live serialiser's fields differently (live src offset -> record dst):
            lrDst.meMode             = lpSrc->GetMode();            // src+0x00 -> dst+0x00
            lrDst.miBufferSize       = lpSrc->GetBufferSize();      // src+0x0C -> dst+0x04
            lrDst.miBufferUsed       = lpSrc->GetBufferUsed();      // src+0x10 -> dst+0x0C
            lrDst.miBufferRead       = lpSrc->GetBufferRead();      // src+0x14 -> dst+0x10
            lrDst.meId               = lpSrc->GetId();             // src+0x28 -> dst+0x14
            lrDst.meContext          = lpSrc->GetContext();         // src+0x2C -> dst+0x18
            lrDst.miStaticBufferSize = lpSrc->GetStaticBufferSize();// src+0x24 -> dst+0x08

            ++miCurrSerialisers;
        }
    }

    void DebugComponent::PostUpdateRecord()
    {
        // X360: empty in this build.
    }

    // ----------------------------------------------------------------------------
    // OnActivate @0x8264F9F8 -- register the overlay's menu items (the Show HUD /
    // auto-start toggles + the six action buttons) and clear the three usage graphs.
    // ----------------------------------------------------------------------------
    void DebugComponent::OnActivate()
    {
        CGS_ASSERT(mpReplayModule != nullptr, "No replay module\n");

        RegisterVariable(&mbShowHud, "Show HUD");
        RegisterVariable(mpReplayModule->GetAutoStartFlagPtr(), "Enable auto-start");

        RegisterFunction(&DebugComponent::StartPlayingCB,      this, "Start Playing");
        RegisterFunction(&DebugComponent::StopPlayingCB,       this, "Stop Playing");
        RegisterFunction(&DebugComponent::StartRecordingCB,    this, "Start Recording");
        RegisterFunction(&DebugComponent::StopRecordingCB,     this, "Stop Recording");
        RegisterFunction(&DebugComponent::MarkActionReplayCB,  this, "Mark Action Replay");
        RegisterFunction(&DebugComponent::StartActionReplayCB, this, "Start/Stop Action Replay");

        ClearGraph(mpWriteSlotsUsedGraph);
        ClearGraph(mpWriteBufferUsedGraph);
        ClearGraph(mpReadGraph);
    }

    // ----------------------------------------------------------------------------
    // Action callbacks @0x8264BFF0..0x8264C048 -- each sets a one-shot request flag
    // on the replay module. The void* user-data is the owning DebugComponent.
    // ----------------------------------------------------------------------------
    void DebugComponent::StartPlayingCB(void* lpUserData)
    {
        static_cast<DebugComponent*>(lpUserData)->mpReplayModule->RequestStartPlaying();
    }
    void DebugComponent::StopPlayingCB(void* lpUserData)
    {
        static_cast<DebugComponent*>(lpUserData)->mpReplayModule->RequestStopPlaying();
    }
    void DebugComponent::StartRecordingCB(void* lpUserData)
    {
        static_cast<DebugComponent*>(lpUserData)->mpReplayModule->RequestStartRecording();
    }
    void DebugComponent::StopRecordingCB(void* lpUserData)
    {
        static_cast<DebugComponent*>(lpUserData)->mpReplayModule->RequestStopRecording();
    }
    void DebugComponent::MarkActionReplayCB(void* lpUserData)
    {
        static_cast<DebugComponent*>(lpUserData)->mpReplayModule->RequestMarkActionReplay();
    }
    void DebugComponent::StartActionReplayCB(void* lpUserData)
    {
        static_cast<DebugComponent*>(lpUserData)->mpReplayModule->RequestStartActionReplay();
    }
    void DebugComponent::AutoStartChangeCB(void* /*lpUserData*/, void* /*lpValue*/)
    {
        // X360: no-op forwarder in this build (the bool is driven directly by the menu).
    }
}
