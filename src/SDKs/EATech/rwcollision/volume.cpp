#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::collision::Volume::InitializeVTable @ 0x82BB03A8
//   rw::collision::Volume::operator=        @ 0x82BB12D0
//
// InitializeVTable lazily fills the shared Volume processing vtable (7 words at
// dword_8327EEE0) with the six handler entry points (slot 0 stays null). The handlers
// live in other TUs; they are referenced here as external code symbols.
//
// operator= copies the 96-byte volume payload from the source. The X360 body does the
// first 64 bytes via four VMX vector moves and the remaining 32 bytes word-by-word
// (with some redundant re-stores Hex-Rays surfaced); the net effect is a 96-byte copy.

namespace rw
{
    namespace collision
    {
        // The six Volume handler entry points (defined in other TUs).
        extern const u8 gVolumeHandler_82F91740;
        extern const u8 gVolumeHandler_82F918C0;
        extern const u8 gVolumeHandler_82F919A4;
        extern const u8 gVolumeHandler_82F9176C;
        extern const u8 gVolumeHandler_82F91894;
        extern const u8 gVolumeHandler_82F919D0;

        // Shared Volume vtable (dword_8327EEE0 .. dword_8327EEF8).
        void* gVolumeVTable[7];

        class Volume
        {
        public:
            int   InitializeVTable();
            void* Assign(const void* pSource);
        };

        int Volume::InitializeVTable()
        {
            gVolumeVTable[0] = 0;
            gVolumeVTable[6] = const_cast<u8*>(&gVolumeHandler_82F919D0);
            gVolumeVTable[1] = const_cast<u8*>(&gVolumeHandler_82F91740);
            gVolumeVTable[2] = const_cast<u8*>(&gVolumeHandler_82F918C0);
            gVolumeVTable[3] = const_cast<u8*>(&gVolumeHandler_82F919A4);
            gVolumeVTable[4] = const_cast<u8*>(&gVolumeHandler_82F9176C);
            gVolumeVTable[5] = const_cast<u8*>(&gVolumeHandler_82F91894);
            return 1;
        }

        void* Volume::Assign(const void* pSource)
        {
            memcpy(this, pSource, 96);
            return this;
        }
    }
}
