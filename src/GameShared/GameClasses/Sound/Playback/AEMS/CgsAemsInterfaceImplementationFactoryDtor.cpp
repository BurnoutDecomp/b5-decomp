// ============================================================================
// CgsAemsInterfaceImplementationFactoryDtor.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826C2848
//   (CgsSound::Playback::AemsRWSampleFactory::`scalar deleting destructor')
//
// Compiler-synthesised scalar-deleting destructor -- identical shape to the committed
// sibling AemsContent/GenericRwacWaveContent dtors:
//   AemsRWSampleFactory::~AemsRWSampleFactory(this); // bl ~dtor
//   if (a2 & 1) operator delete(this);               // a2&1
//
// DWARF (CgsAemsInterfaceImplementation.h:48) attests AemsRWSampleFactory : public
// Factory with only trivially-destructible members (PlugInConfig[3] + PlugInRegistry*
// + nine PlugInHandle words). The Factory base dtor does the teardown, so the class
// destructor touches no members; defining it out-of-line emits exactly the vtable
// store + base ~Factory call. Body is empty.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h"

namespace CgsSound
{
namespace Playback
{
    AemsRWSampleFactory::~AemsRWSampleFactory()
    {
        // No owned resources: PlugInConfig[3] + PlugInRegistry* + PlugInHandle words
        // are all trivially destructible; the Factory base dtor runs implicitly.
    }
}
}
