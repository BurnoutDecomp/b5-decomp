#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DA0E0
//   (CgsSound::Playback::GenericRwacWaveContent::DoUnload)
//
// Behaviour-faithful to the X360 pseudocode:
//     return CgsResource::BinaryFileResource_::Unload(this + 32, this, *(this + 12));
//
// Delegates to the embedded BinaryFileResource_ base (at byte offset 32), passing
// the owning object and a parameter read from offset 12. BinaryFileResource_ is
// not yet reconstructed; forward-declared here (its real body lands with its TU).

namespace CgsResource
{
    struct BinaryFileResource_
    {
        int Unload(void* lpOwner, int liParam);
    };
}

namespace CgsSound { namespace Playback {

    struct GenericRwacWaveContent
    {
        int DoUnload();
    };

    int GenericRwacWaveContent::DoUnload()
    {
        char* lpThis = reinterpret_cast<char*>(this);
        CgsResource::BinaryFileResource_* lpBase =
            reinterpret_cast<CgsResource::BinaryFileResource_*>(lpThis + 32);
        const int liParam = *reinterpret_cast<int*>(lpThis + 12);
        return lpBase->Unload(this, liParam);
    }

}}
