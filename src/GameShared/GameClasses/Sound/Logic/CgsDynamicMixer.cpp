// ============================================================================
// CgsDynamicMixer.cpp -- CgsSound::Logic::DynamicMixer out-of-line destructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DC970
//   (CgsSound::Logic::DynamicMixer::`scalar deleting destructor')
//     bl  Nicotine::IDynamicMixer::~IDynamicMixer(this);  // base sub-object dtor
//     if (a2 & 1) operator delete(this);                  // deleting tail (host delete)
//     return this;
//
// CgsSound::Logic::DynamicMixer is the game's Nicotine::IDynamicMixer subclass (the
// CgsSound seam onto the NFS mix system; the same IDynamicMixer-derived mixer the
// SoundLogic Module embeds @+0x2C30). The scalar-deleting-destructor's only source-
// level effect is running the base ~IDynamicMixer() (which frees the NFSMixMaster /
// SnapshotMixer sub-objects through the mixer allocator); the conditional operator
// delete is the MSVC deleting-destructor tail, re-synthesised from this virtual
// destructor + the class's operator delete.
//
// FLAG (confidence medium): the destructor proves DynamicMixer touches no members of
// its own beyond the IDynamicMixer base at teardown, so no additional fields are
// modelled. The base ~IDynamicMixer() runs implicitly via inheritance -- no explicit
// call is written (that would double-destroy on the host). Mirrors the committed
// sibling destructor-home pattern (CgsEffectObjectDtor.cpp).
// ============================================================================

#include "GameShared/GameClasses/Sound/Logic/CgsDynamicMixer.h"

namespace CgsSound
{
namespace Logic
{

DynamicMixer::~DynamicMixer()
{
    // No DynamicMixer-specific member teardown: the X360 destructor only runs the
    // Nicotine::IDynamicMixer base sub-object destructor (implicit here) and, in the
    // deleting flavour, frees storage -- both compiler/operator-delete concerns.
}

} // namespace Logic
} // namespace CgsSound
