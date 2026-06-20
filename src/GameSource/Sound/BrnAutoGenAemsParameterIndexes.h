#ifndef GAMESOURCE_SOUND_BRNAUTOGENAEMSPARAMETERINDEXES_H
#define GAMESOURCE_SOUND_BRNAUTOGENAEMSPARAMETERINDEXES_H

#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"

// AUTO-GENERATED. BrnAutoGenAemsParameterIndexes.h — generated declarations for the
// AEMS parameter-name index arrays. Each array is a list of interned
// CgsSound::Playback::Name parameter handles for one AEMS class. The element type
// and counts are DWARF-attested (GameSource/Sound/BrnAutoGenAemsParameterIndexes.*).
//
// Arrays are NON-const: they are written at startup by the compiler-generated
// dynamic (static) initializers, one per array, which intern each name via the
// Name(const char*) ctor (MakeHash -> Store).
//
// SCOPE: only AEMS_Class::gaAEMS_ClassParameterNames is DEFINED so far (in the .cpp).
// The other 13 sibling arrays are each their own static-init TU and will add their
// definitions to BrnAutoGenAemsParameterIndexes.cpp later. Declaring all 14 here is
// harmless and establishes the shared home once.

namespace BrnSound
{
namespace ParameterIndexes
{

namespace AEMS_Class
{
    extern CgsSound::Playback::Name gaAEMS_ClassParameterNames[12];
}

namespace AEMS_TrafficEngineClass
{
    extern CgsSound::Playback::Name gaAEMS_TrafficEngineClassParameterNames[7];
}

namespace AEMS_Skids
{
    extern CgsSound::Playback::Name gaAEMS_SkidsParameterNames[26];
}

namespace AEMS_Skids_Traffic
{
    extern CgsSound::Playback::Name gaAEMS_Skids_TrafficParameterNames[5];
}

namespace AEMS_class_horns
{
    extern CgsSound::Playback::Name gaAEMS_class_hornsParameterNames[7];
}

namespace AEMS_ScrapeGranulator
{
    extern CgsSound::Playback::Name gaAEMS_ScrapeGranulatorParameterNames[9];
}

namespace AEMS_PlayInAir
{
    extern CgsSound::Playback::Name gaAEMS_PlayInAirParameterNames[6];
}

namespace AEMS_PlayTakeOff
{
    extern CgsSound::Playback::Name gaAEMS_PlayTakeOffParameterNames[6];
}

namespace AEMS_surface_class
{
    extern CgsSound::Playback::Name gaAEMS_surface_classParameterNames[9];
}

namespace AEMS_TurboClass
{
    extern CgsSound::Playback::Name gaAEMS_TurboClassParameterNames[9];
}

namespace AEMS_GearWhineClass
{
    extern CgsSound::Playback::Name gaAEMS_GearWhineClassParameterNames[5];
}

namespace AEMS_detonation
{
    extern CgsSound::Playback::Name gaAEMS_detonationParameterNames[5];
}

namespace AEMS_tendril
{
    extern CgsSound::Playback::Name gaAEMS_tendrilParameterNames[5];
}

namespace AEMS_crumple
{
    extern CgsSound::Playback::Name gaAEMS_crumpleParameterNames[4];
}

}
}

#endif // GAMESOURCE_SOUND_BRNAUTOGENAEMSPARAMETERINDEXES_H
