#include "GameShared/GameClasses/Module/CgsEventQueue.h"

// CgsModule::EventQueue<char, 50>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (50) char-buffer event
// queue instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count. (Element type is the builtin char.)
template void CgsModule::EventQueue<char, 50>::Construct();
