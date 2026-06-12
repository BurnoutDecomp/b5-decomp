#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A5F38
//   (CgsPhysics::PhysicsSimulationIO::OutputBuffer::Destruct)
//
// Behaviour-faithful to the X360 pseudocode: clears a handful of fields scattered
// through the (large) output buffer, then chains to the base IOBuffer destructor.
//
//   *(this + 4)      = 0.0f;   // a float field
//   *(this + 131144) = 0;
//   *(this + 128056) = 0;
//   *(this + 38440)  = 0;
//   *(this + 24)     = 0;
//   *(this + 8)      = 0;
//   return CgsModule::IOBuffer::Destruct(this);
//
// The buffer is ~128 KB; rather than declare the full layout we clear the exact
// recovered byte offsets directly (offset 4 is a 32-bit float, the rest 32-bit
// integers/pointers).

namespace CgsModule
{
    struct IOBuffer
    {
        int Destruct();
    };
}

namespace CgsPhysics
{
    namespace PhysicsSimulationIO
    {
        struct OutputBuffer : CgsModule::IOBuffer
        {
            int Destruct();
        };

        int OutputBuffer::Destruct()
        {
            char* lpBase = reinterpret_cast<char*>(this);
            *reinterpret_cast<f32*>(lpBase + 4)      = 0.0f;
            *reinterpret_cast<u32*>(lpBase + 131144) = 0;
            *reinterpret_cast<u32*>(lpBase + 128056) = 0;
            *reinterpret_cast<u32*>(lpBase + 38440)  = 0;
            *reinterpret_cast<u32*>(lpBase + 24)     = 0;
            *reinterpret_cast<u32*>(lpBase + 8)      = 0;
            return CgsModule::IOBuffer::Destruct();
        }
    }
}
