#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::ResourceIO::FileSystemStatusInterface::Construct @ 0x82676600
//   CgsResource::ResourceIO::OutputBuffer::Construct              @ 0x828EC978
//
// Two trivial placement constructors that just clear/initialise the leading
// status bytes of their objects and return the object pointer.

namespace CgsResource
{
    namespace ResourceIO
    {
        struct FileSystemStatusInterface
        {
            u8 mbInUse;   // [+0]

            FileSystemStatusInterface* Construct()
            {
                mbInUse = 0;
                return this;
            }
        };

        struct OutputBuffer
        {
            u8 mbOpen;    // [+0]
            u8 mbFull;    // [+1]

            OutputBuffer* Construct()
            {
                mbOpen = 1;
                mbFull = 0;
                return this;
            }
        };
    }
}
