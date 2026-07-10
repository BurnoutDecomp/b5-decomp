// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h
//
// Canonical (DWARF) home for the GUI view module's IO buffers (the X360 cites
// gameshared\gameclasses\gui\view\CgsGuiViewModuleIO.h). This is a MINIMAL slice
// covering only the X360-emitted OutputBuffer accessor owned by this group:
//   CgsGui::ViewIO::OutputBuffer::GetGuiEventQueue() @ 0x8284F040 (read-lock handle)
//
// LAYOUT (authoritative -- pinned by the X360 OutputBuffer bodies):
//   base  CgsModule::IOBuffer                       (1-byte FlagSet status; +1..+3 pad)
//   +4    GuiEventQueue mGuiEvents (VariableEventQueue<18432,16>)
// Proven by:
//   Construct @ 0x82858E40  -- *a1 = 1 (eStatusConstructed), then
//                              CgsModule::VariableEventQueue<18432,16>::Construct(a1 + 4)
//   Destruct  @ 0x82858E58  -- VariableEventQueue<18432,16>::Destruct(a1 + 4) then
//                              IOBuffer::Destruct(a1)
//   GetGuiEventQueue @ 0x8284F040 -- asserts read-lock (status bit 4: lbz + extrwi 1,27),
//                              returns this + 4 (addi r3, r28, 4)
//
// The queue is the same 18432-byte GUI event-queue specialisation the committed
// CgsGuiModuleIO.h / CgsGuiResourceModuleIO.h homes settled on (GuiEventQueueBase<18432,16>
// == VariableEventQueue<18432,16>); the X360 Construct/Destruct targets prove the size here.
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"  // CgsModule::IOBuffer (1-byte FlagSet base)
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"      // CgsGui::GuiEventQueueBase<N,16>

namespace CgsGui
{
namespace ViewIO
{
    // The view->output GUI event buffer. Derives CgsModule::IOBuffer (the 1-byte status
    // FlagSet: bit 3 = locked-for-write, bit 4 = locked-for-read) and embeds a single GUI
    // event queue at this+4 that the view module fills (write lock) and the GUI module
    // bridges out of (read lock, via GuiModule::BridgeFromViewToOutput).
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // GuiEventQueue == GuiEventQueueBase<18432,16> (X360 Construct/Destruct @0x82858E40/E58).
        typedef CgsGui::GuiEventQueueBase<18432, 16> GuiEventQueue;

        // X360 0x82858E40 / 0x82858E58 -- declared here for the member surface; bodied in
        // their own homes (the per-TU compile gate does not link).
        void Construct();
        void Destruct();

        // X360 0x8284F040. Asserts this buffer is locked-for-reading (status bit 4), then
        // returns the read handle to the embedded event queue at this+4.
        const GuiEventQueue* GetGuiEventQueue() const;

        // X360 0x8284F0E8 (the write twin -- CgsGui::GuiModule::Update fetches it before
        // appending the AptCommunicator trigger queue). Asserts locked-for-writing (bit 3),
        // then returns the write handle to the embedded event queue at this+4.
        GuiEventQueue* GetGuiEventQueue();

        // Byte-offset pin (the embedded queue lands at this+4: 1-byte IOBuffer base + 3 pad,
        // VariableEventQueue alignof == 4).
        static void _AssertLayout();

    private:
        u8            maStatusPad[3]; // +1..+3 (force the queue to +4 like the X360)
        GuiEventQueue mGuiEvents;     // +4 (DWARF CgsGuiViewModuleIO.h)
    };

    // ---- CgsGui::ViewIO::InputBuffer (X360 accessors 0x824F7A10 / 0x824F7AB8 / 0x82856EC8) ----
    // Additive: the sibling input buffer that owns the immediate-mode renderer set + view-state
    // event queue. Mirrors the committed CgsGuiModuleIO.h shapes (only the offsets differ:
    // 0x10020/0x10040 here vs 0x8020/0x8040 there, and the ViewIO namespace).

    // ---- CgsGraphics::Camera value-image (foreign type; mirrors CgsGuiModuleIO.h) ----------
    // 368-byte alignas(16) span == the X360 CgsGraphics::Camera copy extent. Opaque storage so the
    // by-value Camera::operator= in SetImRenderers is reproduced without the un-homed Camera type.
    struct alignas(16) CgsGraphicsCameraStorage
    {
        unsigned char maBytes[368];
    };
    static_assert(sizeof(CgsGraphicsCameraStorage) % 16 == 0, "Camera storage 16-byte multiple");

    // ---- ImRendererSet (foreign type; mirrors CgsGuiModuleIO.h) ----------------------------
    // SetImRenderers @0x82856EC8 copies the leading 5 renderer-pointer slots (console 5 dwords,
    // +0x00..+0x13) then the camera (console +0x20); GetImRenderers @0x824F7A10 returns
    // &mRendererSet. The five slots carry the assert-attested names ViewModule::Render
    // @0x82858810 checks ("lpViewInput->GetImRenderers().mpIm2dRenderBuffer" / the three
    // mpIm3dRenderBuffer* fields); slot 1 is never asserted/derefed (name unrecovered). x64:
    // the slots widen to 8-byte pointers (semantic parity by named member -- the console's
    // byte offsets shift; typed void* here so this IO header does not drag the renderer
    // types in -- ViewModule::Render casts slot 0 back to CgsGui::AptIm2dRenderBuffer*).
    struct ImRendererSet
    {
        void* mpIm2dRenderBuffer;                    // console +0x00 (CgsGui::AptIm2dRenderBuffer*)
        void* mpReserved04;                          // console +0x04
        void* mpIm3dRenderBufferUntex;               // console +0x08
        void* mpIm3dRenderBufferRacePosition;        // console +0x0C
        void* mpIm3dRenderBufferMenusAndHud;         // console +0x10
        CgsGraphicsCameraStorage mCamera;            // console +0x20 (X360 camera @ this+0x10040)
    };

    // The view->input buffer. Derives CgsModule::IOBuffer and embeds a large GUI event queue at
    // this+4 (the view-state queue the AddViewState writers Append into under a write lock) plus
    // the immediate-mode renderer set at this+0x10020.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // ViewStateQueue == GuiEventQueueBase<65536,16> (proven by the this+4 .. this+0x10020 span).
        typedef CgsGui::GuiEventQueueBase<65536, 16> ViewStateQueue;

        // X360 0x824F7A10: read-lock (bit 4); returns &mRendererSet (this+0x10020).
        const ImRendererSet& GetImRenderers() const;
        // X360 0x824F7AB8: write-lock (bit 3); returns &mViewStateQueue (this+4). A Get* that
        // checks the WRITE lock -- the AddViewState writers hold it while Append'ing.
        ViewStateQueue& GetViewStateQueue();

        // X360 0x8284EF98 (baked CgsGuiViewModuleIO.h:172): READ-lock (bit 4) handle to the
        // view-state queue ("Not locked for reading\n"), returns this+4. The
        // ViewModule::Update-side reader (ProcessIncomingViewEvents iterates it).
        const ViewStateQueue& GetEvents() const;
        // X360 0x82856EC8: write-lock (bit 3); copies the source set's 5-dword head into
        // mRendererSet then assigns the source camera (src+0x20) into mRendererSet.mCamera.
        void SetImRenderers(const ImRendererSet& lrRenderers);

        // Byte-offset pins (compiled in CgsGuiViewModuleIO.cpp).
        static void _AssertInputLayout();

    private:
        u8             maStatusPad[3];  // +1..+3 (force +4 placement)
        ViewStateQueue mViewStateQueue; // +0x0004
        ImRendererSet  mRendererSet;    // +0x10020 (16-aligned; embeds the camera at +0x10040)
    };
}
}
