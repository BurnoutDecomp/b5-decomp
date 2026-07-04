#include "GameSource/Replays/Serialisers/BrnReplaySoundSerialiserStaticLayout.h"

// BrnReplays::SoundSerialiserStaticLayout, reconstructed from BURNOUT_X360_ARTIST.XEX.
// The sound module's replay static-layout block. Only Construct() is recovered
// (a boot-trace TU: no DWARF, no Feb-2007 source). Called by
// BrnSound::Module::SoundLogicModule::Update.
//
//   Construct @ 0x826ADDA8
//
// Construct zeroes two leading control words (+0x700, +0x810), constructs the embedded
// VariableEventQueue<512,16> (+0x814), then zeroes a standalone float (+0xA24) and two
// trailing (u32,f32) control pairs (+0xC58/+0xC5C and +0xE88/+0xE8C). The 0.0f source is
// the corpus-wide flt_82001CC0 constant. The X360 store offsets are the sole attested facts.

namespace BrnReplays
{
    // -------- Construct @ 0x826ADDA8 --------
    void SoundSerialiserStaticLayout::Construct()
    {
        miNumScrapes = 0;          // stw r30,0x810
        miNumCollisions = 0;       // stw r30,0x700
        mEventQueue.Construct();   // VariableEventQueue<512,16>::Construct(this+0x814)

        mfA24 = 0.0f;              // stfs flt(0.0f) @0xA24

        miNumTrafficEntities = 0;  // stw  r30,0xC58
        mfC5C = 0.0f;              // stfs @0xC5C

        muE88 = 0;                 // stw  r30,0xE88
        mfE8C = 0.0f;              // stfs @0xE8C
    }
}
