#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_SERIALISATION_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_SERIALISATION_H

#include "types.hpp"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"   // DebugMenuSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"     // TextFileReadSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"    // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"                // Behaviour::Parameters (SerialiseBehaviourParameters)

// ============================================================================
// GameSource/Director/Camera/Behaviours/Serialisation.h
//
// The camera serialiser family's own header: the three file/menu serialisers (their own headers,
// included above), the two bank-only serialisers -- TestbedSetupSerialiser (registers every bank
// block as a "Testbed" debug-menu action) and BehaviourParameterNamingSerialiser (names every
// bank block) -- and SerialiseBehaviourParameters, the type-tag dispatch every bank walk uses.
//
// Templates: the bodies of TestbedSetupSerialiser::Serialise<T>,
// BehaviourParameterNamingSerialiser::Serialise<T> and SerialiseBehaviourParameters<TSerialiser>
// live in Camera/BrnBehaviourParameterBank.cpp, the only TU that walks the bank; it carries one
// explicit instantiation of SerialiseBehaviourParameters per serialiser.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    class DebugComponent;   // DirectorModule/BrnDirectorModuleDebugCompononent.h

namespace Camera
{

// Registers each bank block it visits as a debug-menu action that activates the block on the
// arbitrator's testbed state. Layout: one word, the target debug component.
class TestbedSetupSerialiser
{
public:
    void Construct(BrnDirector::DebugComponent* lpDebugComponent) { mpDebugComponent = lpDebugComponent; }

    // The bank's version tag registers nothing.
    void Serialise(const char* /*lpcName*/, Utils::VersionNumber& /*lrValue*/) {}

    // One "Testbed" action per block. Body: Camera/BrnBehaviourParameterBank.cpp.
    template<class T> void Serialise(const char* lpcName, T& lrParams);

private:
    BrnDirector::DebugComponent* mpDebugComponent;   // +0x00
};

// Hands each bank block the name the bank walk gives it (its debug name). Stateless.
class BehaviourParameterNamingSerialiser
{
public:
    // The bank's version tag is not named.
    void Serialise(const char* /*lpcName*/, Utils::VersionNumber& /*lrValue*/) {}

    // Body: Camera/BrnBehaviourParameterBank.cpp.
    template<class T> void Serialise(const char* lpcName, T& lrParams);
};

// Dispatch one bank block on its type tag (Behaviour::Parameters::GetType) to
// lrSerialiser.Serialise(lpcName, <the block as its concrete Parameters type>); an unknown tag
// asserts "Params type needs to be added to Serialise: " << the block's debug name. Body + one
// explicit instantiation per
// serialiser (DebugMenu / TextFileRead / TextFileWrite / TestbedSetup / BehaviourParameterNaming):
// Camera/BrnBehaviourParameterBank.cpp.
template<class TSerialiser>
void SerialiseBehaviourParameters(const char* lpcName, Behaviour::Parameters& lrParameters,
                                  TSerialiser& lrSerialiser);

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_SERIALISATION_H
