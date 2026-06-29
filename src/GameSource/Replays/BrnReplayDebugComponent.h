#pragma once

// BrnReplays::DebugComponent -- the in-game debug overlay for the replay system
// (record/playback HUD, per-serialiser buffer usage graphs, stream-block views).
// DWARF home: GameSource/Replays/BrnReplayDebugComponent.h:93.
//
// This wave GROWS the wave-1 minimal slice (which homed only GetName) into the full
// DebugComponent: the real DWARF-attested member layout, the DebugSerialiserInfo /
// DebugGraph helper structs (BrnReplayDebugComponent.h:52/76), and the full render /
// record / action-callback surface. The bodies live in BrnReplayDebugComponent.cpp.
//
// LAYOUT (X360 ARTIST asm, BrnReplays::DebugComponent::Construct @0x82652978 +
//   OnActivate @0x8264F9F8 + PreUpdateRecord @0x8264BD08):
//   the base CgsDev::DebugComponent occupies the head; this class adds:
//   +0x0C mpReplayModule        ReplayModule*   (Construct: stw r29,0xC; a1[3])
//   +0x10 mpAllocator           IResourceAllocator* (Construct: stw r28,0x10)
//   +0x14 mpSerialisers         DebugSerialiserInfo* (Construct: stw r11,0x14)
//   +0x18 miMaxSerialisers      s32 = 11        (Construct: li 0xB; stw 0x18)
//   +0x1C miCurrSerialisers     s32 = 0         (Construct: stw r31,0x1C; PreUpdate 0x1C)
//   +0x20 mbShowHud             bool            (Construct: stb r31,0x20; OnActivate reg)
//   +0x24 miWriteSlotsUsed      s32 = 0         (Construct: stw r31,0x24)
//   +0x28 miWriteBufferUsed     s32 = 0         (Construct: stw r31,0x28)
//   +0x2C mpWriteSlotsUsedGraph DebugGraph*     (Construct: stw r9,0x2C; OnActivate a1[11])
//   +0x30 mpWriteBufferUsedGraph DebugGraph*    (Construct: stw r9,0x30; OnActivate a1[12])
//   +0x34 mpReadGraph           DebugGraph*     (Construct: stw r9,0x34; OnActivate a1[13])
// (The three DebugGraph* live at +0x2C/+0x30/+0x34 -- a1[11]/[12]/[13] in OnActivate.)

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"          // Vector2Template<float>

namespace rw { class IResourceAllocator; }

namespace BrnReplays
{
    class ReplayModule;

    // The overlay's screen-space points are 2-float pairs. The DWARF labels the
    // render-helper params Vector2Template<float> (mX@+0/mY@+4) -- the scalar fpu
    // rwmath 2-vector, which is what the X360 stack pairs really are.
    typedef rw::math::fpu::Vector2Template<float> Vector2f;

    // DWARF: BrnReplayDebugComponent.h:52. The per-serialiser snapshot the overlay
    // records each frame in PreUpdateRecord and renders in RenderSerialisers.
    // Field offsets re-derived from the X360 ASSEMBLY of PreUpdateRecord @0x8264BD08
    // (60-byte record == 0x3C stride; r27 = record base) and the columns read back in
    // RenderSerialisers @0x8264ECA0. The two are the ground truth for the layout:
    //
    //   PreUpdateRecord stores (record <- live serialiser src):
    //     +0x00 <- src+0x00   (0x8264BF6C  stw r11,0(r27))    meMode
    //     +0x04 <- src+0x0C   (0x8264BF7C  stw r11,4(r27))    miBufferSize
    //     +0x0C <- src+0x10   (0x8264BF8C  stw r11,0xC(r27))  miBufferUsed
    //     +0x10 <- src+0x14   (0x8264BF9C  stw r11,0x10(r27)) miBufferRead
    //     +0x14 <- src+0x28   (0x8264BFAC  stw r11,0x14(r27)) meId
    //     +0x18 <- src+0x2C   (0x8264BFBC  stw r11,0x18(r27)) meContext
    //     +0x08 <- src+0x24   (0x8264BFCC  stw r11,8(r27))    miStaticBufferSize
    //     +0x1C macName[32]   (0x8264BF40-BF5C  byte copy, dst = record+0x1C)
    //
    //   RenderSerialisers reads (r30 = record base):
    //     ID column   "%d"          <- lwz r6,0x14(r30)   == record+0x14 (meId)
    //     Name column               <- addi r4,r30,0x1C   == record+0x1C (macName)
    //     "Static Size" DrawDataSize<- lwz r5,8(r30)      == record+0x08 (miStaticBufferSize)
    //     "Buffer Size" DrawDataSize<- lwz r5,4(r30)      == record+0x04 (miBufferSize)
    //     "Buffer Used" DrawDataSize<- lwz r5,0xC(r30)    == record+0x0C (miBufferUsed)
    //
    // So by offset: meMode@0x00, miBufferSize@0x04, miStaticBufferSize@0x08,
    // miBufferUsed@0x0C, miBufferRead@0x10, meId@0x14, meContext@0x18, macName@0x1C.
    struct DebugSerialiserInfo
    {
        static const s32 KI_MAX_NAME_LENGTH = 32;

        s32  meMode;             // @0x00 BaseSerialiser::EMode snapshot (src+0x00)
        s32  miBufferSize;       // @0x04 "Buffer Size" column          (src+0x0C)
        s32  miStaticBufferSize; // @0x08 "Static Size" column          (src+0x24)
        s32  miBufferUsed;       // @0x0C "Buffer Used" column          (src+0x10)
        s32  miBufferRead;       // @0x10                               (src+0x14)
        s32  meId;               // @0x14 ESerialiserId, ID column "%d" (src+0x28)
        s32  meContext;          // @0x18 ESerialiserContext snapshot   (src+0x2C)
        char macName[KI_MAX_NAME_LENGTH]; // @0x1C
    };

    // DWARF: BrnReplayDebugComponent.h:76. A named float ring-buffer graph the
    // overlay plots (write-slots-used / write-buffer-used / read). The X360 layout
    // (OnActivate @0x8264F9F8 ClearGraph path + RenderGraph @0x8264F728):
    //   +0x000 mafSamples[256]   ring storage (float)
    //   +0x100 (256) mfMin       f32
    //   +0x104 (260) mfMax       f32
    //   +0x108 (264) mBuffer     FixedRingBuffer<float,256> head (mpData/muCapacity/...)
    //   +0x110 (272) miHead      s32  (ring write cursor)
    //   +0x114 (276) miTail      s32
    //   +0x118 (280) miCount     s32  (RenderGraph reads +0x118 == sample count)
    // OnActivate clears: mfMin=0, samples[0]=0, mfMax=0, miHead/miTail/miCount=0.
    struct DebugGraph
    {
        static const s32 KI_NUM_SAMPLES = 256;

        // A fixed-capacity float ring buffer (DWARF FixedRingBuffer<float32_t,256>).
        // RenderGraph reads it through CgsContainers::RingBuffer<float>::operator[];
        // it is modelled here with the named members the overlay touches.
        struct GraphRingBuffer
        {
            f32  mafSamples[KI_NUM_SAMPLES]; // @+0x00 ring storage
            f32  mfMin;                      // @+0x100 running min
            f32  mfMax;                      // @+0x104 running max
            void* mpData;                    // @+0x108 ring base ptr
            s32  miCapacity;                 // @+0x10C
            s32  miHead;                     // @+0x110 write cursor
            s32  miTail;                     // @+0x114
            s32  miCount;                    // @+0x118 live sample count

            void Clear()
            {
                mfMin  = 0.0f;
                mfMax  = 0.0f;
                mafSamples[0] = 0.0f;
                miHead  = 0;
                miTail  = 0;
                miCount = 0;
            }

            // CgsContainers::RingBuffer<float>::operator[] @0x8264E0A0 -- the ring
            // read RenderGraph uses (NOT a linear index): it adds the ring head
            // offset and wraps modulo capacity, so a wrapped ring reads oldest->
            // newest in order. Returns mpData[(miHead + liIndex) % miCapacity].
            f32 operator[](s32 liIndex) const
            {
                const f32* lpData = static_cast<const f32*>(mpData);
                return lpData[(miHead + liIndex) % miCapacity];
            }
        };

        char            macName[KI_NUM_SAMPLES]; // DWARF: char[256] name
        f32             mfMin;                   // DWARF mfMin
        f32             mfMax;                   // DWARF mfMax
        GraphRingBuffer mBuffer;                 // DWARF DebugGraph::GraphRingBuffer
    };

    // DWARF: BrnReplayDebugComponent.h:93 -- struct DebugComponent : public CgsDev::DebugComponent.
    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        // Construct @0x82652978 -- store the module/allocator, allocate the four
        // serialiser-info ring buffers + the three usage graphs, zero the counters.
        void Construct(ReplayModule* lpReplayModule, rw::IResourceAllocator* lpAllocator);
        void Destruct();

        // RenderHUD @0x8265A848 -- the per-frame overlay draw (gated on mbShowHud).
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender);
        virtual void Update();

        // PreUpdateRecord @0x8264BD08 -- snapshot every live serialiser into the
        // mpSerialisers record array for the overlay to render.
        void PreUpdateRecord();
        void PostUpdateRecord();

    protected:
        // The render helpers all return the height (in virtual-screen units) the
        // section consumed, growing the running window extents via lpv2Min/lpv2Max.
        void RenderMainWindow(CgsDev::Debug2DImmediateRender* lpRender, Vector2f* lpv2Min, Vector2f* lpv2Max);
        f32  RenderTitle(CgsDev::Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos, Vector2f* lpv2Min, Vector2f* lpv2Max);
        f32  RenderStatus(CgsDev::Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos, Vector2f* lpv2Min, Vector2f* lpv2Max);
        f32  RenderWriteStreamStatus(CgsDev::Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos, Vector2f* lpv2Min, Vector2f* lpv2Max);
        f32  RenderReadStreamStatus(CgsDev::Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos, Vector2f* lpv2Min, Vector2f* lpv2Max);
        f32  RenderSerialisers(CgsDev::Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos, Vector2f* lpv2Min, Vector2f* lpv2Max);
        f32  RenderWriteStreamBlocks(CgsDev::Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos, Vector2f* lpv2Min, Vector2f* lpv2Max);
        f32  RenderReadStreamBlocks(CgsDev::Debug2DImmediateRender* lpRender, const Vector2f& lrv2Pos, Vector2f* lpv2Min, Vector2f* lpv2Max);

        // Text/value draw helpers (thin wrappers over the immediate renderer's
        // DrawText that format an int / byte-size / fraction first).
        void DrawText(CgsDev::Debug2DImmediateRender* lpRender, const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour);
        void DrawInt32(CgsDev::Debug2DImmediateRender* lpRender, s32 liValue, f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour);
        void DrawDataSize(CgsDev::Debug2DImmediateRender* lpRender, s32 liBytes, f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour);
        void DrawFraction(CgsDev::Debug2DImmediateRender* lpRender, s32 liNumerator, s32 liDenominator, f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour);

        f32  RenderGraph(CgsDev::Debug2DImmediateRender* lpRender, DebugGraph* lpGraph, const Vector2f& lrv2Min, const Vector2f& lrv2Max);
        void ClearGraph(DebugGraph* lpGraph);

        virtual void        OnActivate();
        virtual const char* GetName() const { return "Replays"; }
        virtual const char* GetPath() const { return "Replays"; }
        virtual bool        IsSimple() const { return false; }

        // Debug-menu action callbacks. Registered with the menu as DebugCallbackFunction
        // (void(*)(void*)); the void* user-data is the owning DebugComponent. Each sets
        // a one-shot request flag the ReplayModule consumes next sim.
        static void StartPlayingCB(void* lpUserData);
        static void StopPlayingCB(void* lpUserData);
        static void StartRecordingCB(void* lpUserData);
        static void StopRecordingCB(void* lpUserData);
        static void MarkActionReplayCB(void* lpUserData);
        static void StartActionReplayCB(void* lpUserData);
        static void AutoStartChangeCB(void* lpUserData, void* lpValue);

    protected:
        static const s32 KI_NUM_SERIALISERS = 11; // X360 Construct: li r11,0xB

        ReplayModule*          mpReplayModule;       // @0x0C
        rw::IResourceAllocator* mpAllocator;         // @0x10
        DebugSerialiserInfo*   mpSerialisers;        // @0x14
        s32                    miMaxSerialisers;     // @0x18 == 11
        s32                    miCurrSerialisers;    // @0x1C
        bool                   mbShowHud;            // @0x20
        s32                    miWriteSlotsUsed;     // @0x24
        s32                    miWriteBufferUsed;    // @0x28
        DebugGraph*            mpWriteSlotsUsedGraph; // @0x2C
        DebugGraph*            mpWriteBufferUsedGraph;// @0x30
        DebugGraph*            mpReadGraph;          // @0x34
    };
}
