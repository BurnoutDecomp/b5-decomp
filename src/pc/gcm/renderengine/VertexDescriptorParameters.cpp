#include "renderstates.h"

namespace renderengine
{
// ---------------------------------------------------------------------------
// VertexDescriptor::Parameters::Parameters() -- X360 @0x82276870.
//
// Seeds all 16 element slots to "empty". Written against the GROUND-TRUTH byte
// lanes of the record (see the lane table on VertexDescriptor::Parameters::Element
// in VertexDescriptor.h): the committed member NAMES are shifted one lane from
// what they hold, so the assignments below are commented with the lane each one
// really is.
//
// Three of the eight lanes were previously wrong or unwritten, and each is a live
// defect the moment a Parameters user mounts:
//   * the STREAM lane was seeded 0xFFFF (the asm seeds 0) -- 0xFFFF is then used
//     as a stream index by Initialize;
//   * the METHOD lane was seeded 255 (the asm seeds 0) -- D3D9 rejects a vertex
//     declaration whose D3DDECLMETHOD is not one of 0..5;
//   * the OFFSET (0xFFFF) and USAGE (0xFF) lanes were never written at all, so a
//     slot the caller leaves alone kept whatever the stack held.
// ---------------------------------------------------------------------------
VertexDescriptor::Parameters::Parameters()
{
    for (u32 luIndex = 0; luIndex < 16; ++luIndex)
    {
        Element& lElement = maElements[luIndex];
        lElement.mu16Stream    = 0;        // lane +0x0 stream       (was 0xFFFF)
        lElement.mu16Pad0      = 0xFFFF;   // lane +0x2 offset       (was unwritten)
        lElement.miOffset      = -1;       // lane +0x4 format       (-1 == empty slot)
        lElement.mu8Type       = 0;        // lane +0x8 method       (was 255)
        lElement.mu8Pad1       = 0xFF;     // lane +0x9 usage        (was unwritten; 0xFF == default)
        lElement.mu8Usage      = 0xFF;     // lane +0xA usageIndex
        lElement.mu8UsageIndex = 0;        // lane +0xB elementType
        lElement.mu8Enabled    = 1;        // lane +0xC elementClass (u32; low byte)
        lElement.maPad2[0]     = 0;        // lane +0xD..+0xF        (the rest of elementClass)
        lElement.maPad2[1]     = 0;
        lElement.maPad2[2]     = 0;
        maElementFlags[luIndex] = 0;
    }
}
}
