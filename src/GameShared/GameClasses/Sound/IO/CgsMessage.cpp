#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"

namespace CgsSound
{
namespace Io
{

// CgsMessage.cpp:119 (DWARF virtual void Destruct(), inlining SetEventId and
// SetStateManagerId). ARTIST 0x82689868, the only slot of the vtable 0x820AEC04:
// sth 0 at +0x08, sth 0xFFFF at +0x0A.
void MessageHeader::Destruct()
{
    SetEventId(0);
    SetStateManagerId(KU16_NO_DESTINATION);
}

}
}
