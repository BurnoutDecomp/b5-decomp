#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"   // CgsDev::RGBA (packed u32 colour)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"               // CgsModule::Event (empty queued-event base)

// CgsDev::Internal debug-draw records - the byte-image event structs the CgsDev debug renderer
// queues into its VariableEventQueue<16384,16> and replays each frame. Each Draw* publisher fills
// one of these, then hands it to VariableEventQueue<16384,16>::AddEventSafe<CInEventDrawX>(&rec, id)
// which forwards sizeof(CInEventDrawX) as the record size; the dispatcher walks the queue and casts
// each event image back to its record type. All derive from the empty CgsModule::Event base (the
// queue's common event type), so the empty-base optimisation makes sizeof(record) == the packed
// field image the X360 stores - which is exactly the byte count each AddEventSafe instance bakes in.
//
// Layout (field names/order/types) is DWARF-authoritative from the DecFIGS header
// (references/DecFIGS/dwarfdump/.../Render/CgsDebugRenderCommon.h). sizeof of each record is
// cross-checked below against the size immediate (li r6) baked into that record's AddEventSafe
// instance in BURNOUT_X360_ARTIST.XEX (addresses noted per struct). f32 and RGBA are both 4 bytes,
// so the records are dense (no interior padding).

namespace CgsDev
{
    namespace Internal
    {
        // --- 2D (screen-space) records ---

        // AddEventSafe @ 0x82828418, size 0x10 (16). Text with the character bytes carried by a
        // preceding STRING event.
        struct CInEventDrawText2D : public CgsModule::Event
        {
            f32  mfX;
            f32  mfY;
            f32  mfSize;
            RGBA mColour;
        };

        // (No dedicated AddEventSafe instance in this <16384,16> queue's todo set; kept here as the
        // canonical home for the 2D line record used by DebugRender::Draw2DLine / Dispatch2D.)
        struct CInEventDrawLine2D : public CgsModule::Event
        {
            f32  mfX1;
            f32  mfY1;
            f32  mfX2;
            f32  mfY2;
            RGBA mColour;
        };

        // AddEventSafe @ 0x82828588, size 0x14 (20). Screen rect stored as origin + extent.
        struct CInEventDrawBox2D : public CgsModule::Event
        {
            f32  mfX;
            f32  mfY;
            f32  mfWidth;
            f32  mfHeight;
            RGBA mColour;
        };

        // --- 3D (world-space) records ---

        // AddEventSafe @ 0x82828640, size 0x14 (20).
        struct CInEventDrawText : public CgsModule::Event
        {
            f32  mfX;
            f32  mfY;
            f32  mfZ;
            f32  mfSize;
            RGBA mColour;
        };

        // AddEventSafe @ 0x828286F8, size 0x1C (28).
        struct CInEventDrawLine : public CgsModule::Event
        {
            f32  mfX1;
            f32  mfY1;
            f32  mfZ1;
            f32  mfX2;
            f32  mfY2;
            f32  mfZ2;
            RGBA mColour;
        };

        // AddEventSafe @ 0x828287B0, size 0x34 (52). Four world-space corners.
        struct CInEventDrawQuad : public CgsModule::Event
        {
            f32  mfX1;
            f32  mfY1;
            f32  mfZ1;
            f32  mfX2;
            f32  mfY2;
            f32  mfZ2;
            f32  mfX3;
            f32  mfY3;
            f32  mfZ3;
            f32  mfX4;
            f32  mfY4;
            f32  mfZ4;
            RGBA mColour;
        };

        // AddEventSafe @ 0x82828868, size 0x30 (48). Full basis (at/up/right) + position; no colour.
        struct CInEventDrawAxis : public CgsModule::Event
        {
            f32 mfAtX;
            f32 mfAtY;
            f32 mfAtZ;
            f32 mfUpX;
            f32 mfUpY;
            f32 mfUpZ;
            f32 mfRtX;
            f32 mfRtY;
            f32 mfRtZ;
            f32 mfPosX;
            f32 mfPosY;
            f32 mfPosZ;
        };

        // AddEventSafe @ 0x82828920, size 0x14 (20).
        struct CInEventDrawSphere : public CgsModule::Event
        {
            f32  mfX;
            f32  mfY;
            f32  mfZ;
            f32  mfRadius;
            RGBA mColour;
        };

        // AddEventSafe @ 0x828289D8, size 0x14 (20).
        struct CInEventDrawSolidSphere : public CgsModule::Event
        {
            f32  mfX;
            f32  mfY;
            f32  mfZ;
            f32  mfRadius;
            RGBA mColour;
        };

        // AddEventSafe @ 0x82828A90, size 0x20 (32). Position + facing direction + radius.
        struct CInEventDrawCircle : public CgsModule::Event
        {
            f32  mfPosX;
            f32  mfPosY;
            f32  mfPosZ;
            f32  mfDirX;
            f32  mfDirY;
            f32  mfDirZ;
            f32  mfRadius;
            RGBA mColour;
        };

        // AddEventSafe @ 0x82828B48, size 0x4C (76). Oriented box: position + at/up/right basis +
        // inf/sup corner offsets.
        struct CInEventDrawBox : public CgsModule::Event
        {
            f32  mfPosX;
            f32  mfPosY;
            f32  mfPosZ;
            f32  mfAtX;
            f32  mfAtY;
            f32  mfAtZ;
            f32  mfUpX;
            f32  mfUpY;
            f32  mfUpZ;
            f32  mfRtX;
            f32  mfRtY;
            f32  mfRtZ;
            f32  mfInfX;
            f32  mfInfY;
            f32  mfInfZ;
            f32  mfSupX;
            f32  mfSupY;
            f32  mfSupZ;
            RGBA mColour;
        };

        // AddEventSafe @ 0x82828C00, size 0x4C (76). Same layout as CInEventDrawBox (filled solid).
        struct CInEventDrawSolidBox : public CgsModule::Event
        {
            f32  mfPosX;
            f32  mfPosY;
            f32  mfPosZ;
            f32  mfAtX;
            f32  mfAtY;
            f32  mfAtZ;
            f32  mfUpX;
            f32  mfUpY;
            f32  mfUpZ;
            f32  mfRtX;
            f32  mfRtY;
            f32  mfRtZ;
            f32  mfInfX;
            f32  mfInfY;
            f32  mfInfZ;
            f32  mfSupX;
            f32  mfSupY;
            f32  mfSupZ;
            RGBA mColour;
        };

        // AddEventSafe @ 0x82828CB8, size 0x1C (28). Two world-space endpoints.
        struct CInEventDrawArrow : public CgsModule::Event
        {
            f32  mfX1;
            f32  mfY1;
            f32  mfZ1;
            f32  mfX2;
            f32  mfY2;
            f32  mfZ2;
            RGBA mColour;
        };

        // sizeof cross-check: each == the size immediate baked into its AddEventSafe instance.
        static_assert(sizeof(CInEventDrawText2D)     == 0x10, "CInEventDrawText2D must be 16 bytes (AddEventSafe li r6,0x10)");
        static_assert(sizeof(CInEventDrawLine2D)     == 0x14, "CInEventDrawLine2D must be 20 bytes");
        static_assert(sizeof(CInEventDrawBox2D)      == 0x14, "CInEventDrawBox2D must be 20 bytes (AddEventSafe li r6,0x14)");
        static_assert(sizeof(CInEventDrawText)       == 0x14, "CInEventDrawText must be 20 bytes (AddEventSafe li r6,0x14)");
        static_assert(sizeof(CInEventDrawLine)       == 0x1C, "CInEventDrawLine must be 28 bytes (AddEventSafe li r6,0x1C)");
        static_assert(sizeof(CInEventDrawQuad)       == 0x34, "CInEventDrawQuad must be 52 bytes (AddEventSafe li r6,0x34)");
        static_assert(sizeof(CInEventDrawAxis)       == 0x30, "CInEventDrawAxis must be 48 bytes (AddEventSafe li r6,0x30)");
        static_assert(sizeof(CInEventDrawSphere)     == 0x14, "CInEventDrawSphere must be 20 bytes (AddEventSafe li r6,0x14)");
        static_assert(sizeof(CInEventDrawSolidSphere) == 0x14, "CInEventDrawSolidSphere must be 20 bytes (AddEventSafe li r6,0x14)");
        static_assert(sizeof(CInEventDrawCircle)     == 0x20, "CInEventDrawCircle must be 32 bytes (AddEventSafe li r6,0x20)");
        static_assert(sizeof(CInEventDrawBox)        == 0x4C, "CInEventDrawBox must be 76 bytes (AddEventSafe li r6,0x4C)");
        static_assert(sizeof(CInEventDrawSolidBox)   == 0x4C, "CInEventDrawSolidBox must be 76 bytes (AddEventSafe li r6,0x4C)");
        static_assert(sizeof(CInEventDrawArrow)      == 0x1C, "CInEventDrawArrow must be 28 bytes (AddEventSafe li r6,0x1C)");
    }
}
