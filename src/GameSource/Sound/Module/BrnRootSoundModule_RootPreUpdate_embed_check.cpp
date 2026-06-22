// Translation-unit embed check for the Sound group's two homes:
//   * GameSource/Sound/Module/BrnRootSoundModuleIo.h (RootPreUpdateOutputBuffer +
//     the PreUpdateOutput minimal slice) and its .cpp accessors.
//   * GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h
//     (SoundLogicModule::GetBrnInputStructure) and its .cpp accessor.
// Pulls both headers so the layout static_asserts (_AssertLayout) and the new
// declarations participate in a standalone compile.
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
