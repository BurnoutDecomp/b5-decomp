#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"

// CgsDev::DebugRender - buffered 2D debug-render bodies. Draw2DX queues a byte-image event under its
// type ID; Dispatch2D walks the queue and replays each into the immediate-mode renderer, then clears.
// Reconstructed from the X360 ARTIST build (see CgsDebugRender.h for addresses). The events are POD and
// passed to the queue as CgsModule::Event* by reinterpret (the queue stores them by byte image).

namespace CgsDev
{
    void DebugRender::Construct()
    {
        m2DQueue.Construct();
    }

    void DebugRender::Clear()
    {
        m2DQueue.Clear();
    }

    // X360 Draw2DText 0x8282B1D0: queue the string (STRING event) then the text record (TEXT event).
    void DebugRender::Draw2DText(const char* lpcText, f32 lfX, f32 lfY, f32 lfScale, RGBA lColour)
    {
        if (!lpcText)
            return;

        m2DQueue.AddStringEventSafe(lpcText, Internal::E_INEVENT_2D_STRING);

        Internal::CInEventDrawText2D lEvent;
        lEvent.mfX     = lfX;
        lEvent.mfY     = lfY;
        lEvent.mfSize  = lfScale;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_TEXT, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DTextJustified 0x8282BAC0 (DWARF CgsDebugRender.cpp:672). Measure the string at
    // lfSize, shift the position LEFT by 0 / half / all of that width for LEFT / CENTRE / RIGHT,
    // then queue it exactly like Draw2DText (STRING event id 0, then the TEXT record id 1).
    //
    // The width model is the console's own and is a flat monospace estimate, not a font metric:
    //   width = strlen(text) * lfSize * 0.65        (flt_82097F40 == 0.65f, read off the image)
    //   CENTRE: x -= width * 0.5                    (flt_82001DA0 == 0.5f)
    //   RIGHT:  x -= width
    // The X360 inlines the strlen (a `lbz`/`addi`/`cmplwi` loop, no call), so it is spelled inline
    // here too rather than pulling rw::core::stdc::StringLength into this TU. Only the X lane is
    // written back (`vrlimi128 v0, v13, 8, 0` merges lane 0 alone), and the LEFT arm skips the
    // merge entirely -- so Y/Z/W ride through untouched in every case.
    //
    // ⚠️ NO NULL GUARD, deliberately: the console's first act is to dereference lpcText in the
    // length loop. Draw2DText above does guard, because its own X360 body does.
    //
    // This is the entry point BrnDirector::DebugPrinter::ActualPrint @0x821F71D8 draws every
    // Director debug line through, and the one Camera::Utils::Tweaker's readout uses.
    void DebugRender::Draw2DTextJustified(const char* lpcText, Vector2 lv2Position,
                                          Justification leJustification, f32 lfSize, RGBA lColour)
    {
        // Inlined string length (the console's own loop).
        u32 luLength = 0;
        while (lpcText[luLength] != '\0')
        {
            ++luLength;
        }

        const f32 lfWidth = static_cast<f32>(luLength) * lfSize * 0.65f;

        if (leJustification == E_JUSTIFY_CENTRE)
        {
            lv2Position.x -= lfWidth * 0.5f;
        }
        else if (leJustification == E_JUSTIFY_RIGHT)
        {
            lv2Position.x -= lfWidth;
        }

        m2DQueue.AddStringEventSafe(lpcText, Internal::E_INEVENT_2D_STRING);

        Internal::CInEventDrawText2D lEvent;
        lEvent.mfX     = lv2Position.x;
        lEvent.mfY     = lv2Position.y;
        lEvent.mfSize  = lfSize;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_TEXT, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DLine (CInEventDrawLine2D, ID 2).
    void DebugRender::Draw2DLine(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour)
    {
        Internal::CInEventDrawLine2D lEvent;
        lEvent.mfX1    = lfX0;
        lEvent.mfY1    = lfY0;
        lEvent.mfX2    = lfX1;
        lEvent.mfY2    = lfY1;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_LINE, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DBox 0x8282B2D8 (CInEventDrawBox2D, ID 3): screen rect as origin + extent + colour.
    // The record stores {mfX, mfY, mfWidth, mfHeight} (DWARF); the min/max interface converts here.
    void DebugRender::Draw2DBox(f32 lfMinX, f32 lfMinY, f32 lfMaxX, f32 lfMaxY, RGBA lColour)
    {
        Internal::CInEventDrawBox2D lEvent;
        lEvent.mfX      = lfMinX;
        lEvent.mfY      = lfMinY;
        lEvent.mfWidth  = lfMaxX - lfMinX;
        lEvent.mfHeight = lfMaxY - lfMinY;
        lEvent.mColour  = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_BOX, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Dispatch2D 0x8282A4B8: walk the queue, replaying each event into lpRenderer. A STRING event
    // is held and consumed by the next TEXT event (the X360 pairs them). Clears the queue when done.
    void DebugRender::Dispatch2D(Debug2DImmediateRender* lpRenderer, bool lbClear)
    {
        if (!lpRenderer)
            return;

        const char*             lpcPendingString = "";
        const CgsModule::Event* lpEvent          = nullptr;
        s32                     liSize           = 0;
        s32                     liType           = m2DQueue.GetFirstEvent(&lpEvent, &liSize);

        while (liType >= 0)
        {
            switch (liType)
            {
                case Internal::E_INEVENT_2D_STRING:
                    lpcPendingString = reinterpret_cast<const char*>(lpEvent);
                    break;

                case Internal::E_INEVENT_2D_TEXT:
                {
                    const Internal::CInEventDrawText2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawText2D*>(lpEvent);
                    lpRenderer->DrawText(lpcPendingString, lpEv->mfX, lpEv->mfY, lpEv->mfSize, lpEv->mColour);
                    break;
                }

                case Internal::E_INEVENT_2D_LINE:
                {
                    const Internal::CInEventDrawLine2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawLine2D*>(lpEvent);
                    Vector2 lv2Start = { lpEv->mfX1, lpEv->mfY1, 0.0f, 0.0f };
                    Vector2 lv2End   = { lpEv->mfX2, lpEv->mfY2, 0.0f, 0.0f };
                    lpRenderer->DrawLine(lv2Start, lv2End, lpEv->mColour);
                    break;
                }

                case Internal::E_INEVENT_2D_BOX:
                {
                    const Internal::CInEventDrawBox2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawBox2D*>(lpEvent);
                    lpRenderer->DrawBox(lpEv->mfX, lpEv->mfY, lpEv->mfWidth, lpEv->mfHeight, lpEv->mColour);
                    break;
                }

                default:
                    break;   // richer 2D prims (frame/circle/poly/...) are the follow-on
            }

            liType = m2DQueue.GetNextEvent(lpEvent, &lpEvent, &liSize);
        }

        if (lbClear)
            m2DQueue.Clear();
    }
}
