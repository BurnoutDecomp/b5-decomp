#ifndef CGS_SOUND_LOGIC_CGSDYNAMICMIXER_H
#define CGS_SOUND_LOGIC_CGSDYNAMICMIXER_H

#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"

// =============================================================================
// GameShared/GameClasses/Sound/Logic/CgsDynamicMixer.h
//
// CgsSound::Logic::DynamicMixer -- the game's concrete dynamic-mixer, the CgsSound
// seam onto the NFS mix system. Derives Nicotine::IDynamicMixer (which owns the
// NFSMixMaster / SnapshotMixer sub-objects, freed through the mixer allocator). This
// is the same IDynamicMixer-derived mixer the SoundLogic Module embeds @+0x2C30
// (CgsSoundLogicModule.h). Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827DC970
// (the scalar deleting destructor), the ONLY function of this class in this batch.
//
// FLAG (minimal slice): that destructor proves DynamicMixer touches no members of
// its own at teardown beyond the IDynamicMixer base sub-object, so no additional
// fields are modelled here. The rest of the class surface (any Init/Process
// overrides) is left to its full owning TU.
// =============================================================================
#endif // CGS_SOUND_LOGIC_CGSDYNAMICMIXER_H
