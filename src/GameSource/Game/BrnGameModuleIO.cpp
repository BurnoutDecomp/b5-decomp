#include "GameSource/Game/BrnGameModuleIO.h"

// ARTIST's six real template hooks are supplied by ModuleSingleBufferedTemplate.
// Their Construct/Destruct calls fold to 0x8284CB38 (blr), and Prepare/Release calls
// fold to 0x82C296C8 (li r3,1;blr). DecFIGS separately retains these named methods.
// The request-interface member's birth flag belongs to BaseVariableEventQueue's
// C++ constructor. These original legacy methods do not initialize/clear that queue.
namespace BrnGame
{
namespace BrnGameModuleIO
{
    void InputBuffer::Construct() {}
    bool InputBuffer::Prepare() { return true; }
    bool InputBuffer::Release() { return true; }
    void InputBuffer::Destruct() {}

    void OutputBuffer::Construct() {}
    bool OutputBuffer::Prepare() { return true; }
    bool OutputBuffer::Release() { return true; }
    void OutputBuffer::Destruct() {}
}
}
