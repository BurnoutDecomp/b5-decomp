#pragma once

// =================================================================================================
// CgsSceneManager::CgsCollision::LineWithPolySoupStreamJobDesc -- the descriptor of the STREAMED
// line-vs-static-world test (descriptor type 4, E_COLLISION_TYPE_LINE_POLYSOUP_STREAM).
//
// Shape from the class declaration: five data members and the public accessors. Offsets from
// the console:
//   * BaseCollisionGenerator::RunCollideLineAgainstPolySoupStream inlines Prepare; relative to
//     the batch's descriptor slot it stores the map at +0x00, the producer at +0x04, the per-line
//     result capacity at +0x08, mbTestDoubleSided (a byte) at +0x0D and mbTestNearest (a byte) at
//     +0x0C, then the CollisionJobDescription bookkeeping: +0xF0 = null, +0xF4 = 0.0f,
//     +0xF8 = null, +0xFF = 4.
//   * PolygonSoupTesterJob::ExecuteLineTest reads them back through the accessors below; each
//     accessor is the private GetData() "return this" shim followed by one load (+0x00, +0x04,
//     `lbz +0x0C`, `lbz +0x0D`).
// The declaration also names GetMaxResultsPerLine; no console function reads it, so it is left out.
// =================================================================================================

#include "types.hpp"

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h" // CollisionJobDescription (base)
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"                 // Line (StreamCommand, by value)
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"   // PolySoupLineNearestResult (StreamResult, by value)

// Pointer use only. Full homes:
//   PolygonSoupListSpatialMap -- Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h
//   SimpleDataStreamProducer  -- Memory/DataStream/CgsSimpleDataStreamProducer.h
namespace CgsGeometric { struct PolygonSoupListSpatialMap; }
namespace CgsMemory { struct SimpleDataStreamProducer; }

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct LineWithPolySoupStreamJobDesc : public CollisionJobDescription
    {
        const CgsGeometric::PolygonSoupListSpatialMap* mpSpatialMap;         // +0x00
        CgsMemory::SimpleDataStreamProducer*           mpStream;             // +0x04
        s32                                            miMaxResultsPerLine;  // +0x08
        bool                                           mbTestNearest;        // +0x0C
        bool                                           mbTestDoubleSided;    // +0x0D

        // The stream's records (declared with the class): one line per command; per line a 16-byte
        // header (the hit count) and the 112-byte hit records PolygonSoupTesterJob writes.
        struct StreamCommand { CgsGeometric::Line mLine; };
        struct StreamResult { u32 muNumResults; u32 muPad[3]; CgsGeometric::PolySoupLineNearestResult maResults[1]; };

        const CgsGeometric::PolygonSoupListSpatialMap* GetSpatialMapPointer() const { return mpSpatialMap; }
        CgsMemory::SimpleDataStreamProducer* GetStream() const { return mpStream; }
        bool GetTestNearest() const { return mbTestNearest; }
        bool GetTestDoubleSided() const { return mbTestDoubleSided; }

        // Declared with the class. The console inlines it into
        // RunCollideLineAgainstPolySoupStream (the stores listed in the banner), so the
        // dispatcher writes these MEMBERS rather than the console's byte offsets.
        void Prepare(const CgsGeometric::PolygonSoupListSpatialMap* lpSpatialMap,
                     CgsMemory::SimpleDataStreamProducer*           lpStream,
                     s32                                            liMaxResultsPerLine,
                     bool                                           lbTestNearest,
                     bool                                           lbTestDoubleSided)
        {
            mpSpatialMap        = lpSpatialMap;
            mpStream            = lpStream;
            miMaxResultsPerLine = liMaxResultsPerLine;
            mbTestDoubleSided   = lbTestDoubleSided;
            mbTestNearest       = lbTestNearest;

            mpResultsList = NULL;
            mfRadius      = 0.0f;
            mpDebugStream = 0;
            muJobType     = static_cast<u8>(E_COLLISIONJOB_LINE_WITH_POLYSOUP_STREAM);
        }
    };
}
}
