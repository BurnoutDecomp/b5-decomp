#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::DirectorBridgeSerialiser)
//
//   Construct @ 0x8264C498:
//       return BrnReplays::BaseSerialiser::Construct(
//                  this, 3, 0, 0x8000, 0x8000, "DirectorBridge", 0);
//
// A leaf forwarder: the derived serialiser's Construct simply parameterises the
// shared BaseSerialiser::Construct with this stream's id (3), buffer sizes
// (0x8000 each) and channel name. BaseSerialiser is reconstructed by its own TU;
// here it is declared for the compile-only gate.

namespace BrnReplays
{
    struct BaseSerialiser
    {
        int Construct(int nId, int nMode, int nBufferSize, int nMaxSize,
                      const char* pName, int nFlag);
    };

    struct DirectorBridgeSerialiser : BaseSerialiser
    {
        int Construct();
    };

    int DirectorBridgeSerialiser::Construct()
    {
        return BaseSerialiser::Construct(3, 0, 0x8000, 0x8000, "DirectorBridge", 0);
    }
}
