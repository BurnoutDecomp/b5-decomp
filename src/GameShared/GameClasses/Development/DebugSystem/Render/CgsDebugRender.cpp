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
        lEvent.mfScale = lfScale;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_TEXT, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DLine (CInEventDrawLine2D, ID 2).
    void DebugRender::Draw2DLine(f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, RGBA lColour)
    {
        Internal::CInEventDrawLine2D lEvent;
        lEvent.mfX0    = lfX0;
        lEvent.mfY0    = lfY0;
        lEvent.mfX1    = lfX1;
        lEvent.mfY1    = lfY1;
        lEvent.mColour = lColour;
        m2DQueue.AddEventSafe(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                              Internal::E_INEVENT_2D_LINE, static_cast<s32>(sizeof(lEvent)));
    }

    // X360 Draw2DBox 0x8282B2D8 (CInEventDrawBox2D, ID 3): min/max corners + colour.
    void DebugRender::Draw2DBox(f32 lfMinX, f32 lfMinY, f32 lfMaxX, f32 lfMaxY, RGBA lColour)
    {
        Internal::CInEventDrawBox2D lEvent;
        lEvent.mfMinX  = lfMinX;
        lEvent.mfMinY  = lfMinY;
        lEvent.mfMaxX  = lfMaxX;
        lEvent.mfMaxY  = lfMaxY;
        lEvent.mColour = lColour;
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
                    lpRenderer->DrawText(lpcPendingString, lpEv->mfX, lpEv->mfY, lpEv->mfScale, lpEv->mColour);
                    break;
                }

                case Internal::E_INEVENT_2D_LINE:
                {
                    const Internal::CInEventDrawLine2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawLine2D*>(lpEvent);
                    Vector2 lv2Start = { lpEv->mfX0, lpEv->mfY0, 0.0f, 0.0f };
                    Vector2 lv2End   = { lpEv->mfX1, lpEv->mfY1, 0.0f, 0.0f };
                    lpRenderer->DrawLine(lv2Start, lv2End, lpEv->mColour);
                    break;
                }

                case Internal::E_INEVENT_2D_BOX:
                {
                    const Internal::CInEventDrawBox2D* lpEv =
                        reinterpret_cast<const Internal::CInEventDrawBox2D*>(lpEvent);
                    lpRenderer->DrawBox(lpEv->mfMinX, lpEv->mfMinY,
                                        lpEv->mfMaxX - lpEv->mfMinX, lpEv->mfMaxY - lpEv->mfMinY, lpEv->mColour);
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
