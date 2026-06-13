#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnReplays::GameModuleSerialiser)
//
//   Construct @ 0x8264C6A0:
//       return BrnReplays::BaseSerialiser::Construct(
//                  this, 5, 0, 1024, 1024, "GameModule", 0);
//
// Leaf forwarder, mirror of the other replay serialisers: parameterises the
// shared BaseSerialiser::Construct with stream id 5, 1 KiB buffers and the
// "GameModule" channel name. BaseSerialiser lives in its own TU; declared here
// for the compile-only gate.

namespace BrnReplays
{
    struct BaseSerialiser
    {
        int Construct(int nId, int nMode, int nBufferSize, int nMaxSize,
                      const char* pName, int nFlag);
    };

    struct GameModuleSerialiser : BaseSerialiser
    {
        int Construct();
    };

    int GameModuleSerialiser::Construct()
    {
        return BaseSerialiser::Construct(5, 0, 1024, 1024, "GameModule", 0);
    }
}
