#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (CgsSound::Playback::Plugins::GainArray)
//
//   CreateInstance @ 0x826C3A10 — placement init of a gain-array plugin instance:
//       if (instance) instance[0] = vtable;          // off_820AE188
//       instance[3] = &instance[10];                 // gain block self-pointer
//       for 6 iterations: set two gain arrays to 1.0
//           interleaved: instance[10],[12],[14],[16],[18],[20]
//           contiguous : instance[22],[23],[24],[25],[26],[27]
//       return 1;
//   GetSize @ 0x82689E08 — return 112; (byte size of the instance)
//
// Both functions of this plugin are reconstructed together. Offsets are in 32-bit
// words from the instance base, matching the Hex-Rays `_DWORD*` view.

namespace CgsSound { namespace Playback { namespace Plugins {

    // Plugin vtable (off_820AE188), defined elsewhere; data-stub reference.
    extern void* const gpGainArrayVTable;

    struct GainArray
    {
        void* mpVtable;        // [0]  word 0
        u32   mau1[2];         // [1..2]
        f32*  mpGainBlock;     // [3]  word 3 -> &maBlock[0] (== &word[10])
        u32   mau2[6];         // [4..9]
        f32   maBlock[18];     // [10..27]  word 10 .. word 27

        static int CreateInstance(GainArray* lpInstance);
        static int GetSize();
    };

    static const int KI_GAIN_ARRAY_INSTANCE_SIZE = 112;
    static const int KI_GAIN_ARRAY_ENTRIES       = 6;

    int GainArray::CreateInstance(GainArray* lpInstance)
    {
        if (lpInstance)
            lpInstance->mpVtable = gpGainArrayVTable;

        lpInstance->mpGainBlock = lpInstance->maBlock;

        for (int liEntry = 0; liEntry < KI_GAIN_ARRAY_ENTRIES; ++liEntry)
        {
            lpInstance->maBlock[liEntry * 2]   = 1.0f;   // interleaved (words 10,12,..,20)
            lpInstance->maBlock[12 + liEntry]  = 1.0f;   // contiguous  (words 22..27)
        }
        return 1;
    }

    int GainArray::GetSize()
    {
        return KI_GAIN_ARRAY_INSTANCE_SIZE;
    }

}}}
