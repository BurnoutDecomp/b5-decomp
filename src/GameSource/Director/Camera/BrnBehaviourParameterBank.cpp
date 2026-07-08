// ============================================================================
// GameSource/Director/Camera/BrnBehaviourParameterBank.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourParameterBank slice this TU owns:
//   - BehaviourParameterBank::SaveParameters() @0x822732D0
//
// SaveParameters writes the entire camera parameter bank to the human-readable debug text
// file "d:\\camera.txt". On the X360 the TextFileWriteSerialiser's Construct/Destruct helpers
// were folded inline, so the raw asm shows fopen / an inline muRecursionDepth==0 assert /
// fclose directly. Reconstructed here as the calls the original source made (reverse-inlining
// per the project's de-optimisation rule): the fopen("d:\\camera.txt","w") + depth reset live
// in TextFileWriteSerialiser::Construct(const char*), and the CGS_ASSERT(muRecursionDepth==0)
// (Serialisation.h:643) + fclose live in TextFileWriteSerialiser::Destruct().
//
// X360 0x822732D0:
//     v4 = fopen("d:\\camera.txt", "w");            ; -> Construct
//     v3 = 0;                                       ; muRecursionDepth = 0 (Construct)
//     BehaviourParameterBank::Serialise<TextFileWriteSerialiser>(this, &serialiser);
//     if (v3) { CGS_ASSERT("muRecursionDepth == 0", Serialisation.h:643); }  ; -> Destruct
//     if (v4) fclose(v4);                           ; -> Destruct
//
// The stack serialiser is {muRecursionDepth@+0x00 = 0, mpFile@+0x04 = fopen result}, i.e. a
// TextFileWriteSerialiser built by Construct.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser

namespace BrnDirector
{
namespace Camera
{

void BehaviourParameterBank::SaveParameters()
{
    TextFileWriteSerialiser lSerialiser;
    lSerialiser.Construct("d:\\camera.txt");   // fopen "w" + muRecursionDepth = 0 (inlined)
    Serialise(lSerialiser);                    // this->Serialise<TextFileWriteSerialiser>(&lSerialiser)
    lSerialiser.Destruct();                    // CGS_ASSERT(muRecursionDepth == 0); fclose (inlined)
}

} // namespace Camera
} // namespace BrnDirector
