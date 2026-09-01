#ifndef ATTRIBSYS_ENUMS_E_ACTION_H
#define ATTRIBSYS_ENUMS_E_ACTION_H

namespace AttribSys
{
namespace Enums
{
namespace eAction
{

// DecFIGS eAction.h:12. These are authored bit values, not a zero-based index.
enum eAction
{
    Collision = 1,
    Detach    = 2,
    Hinging   = 4,
    HingeOpen = 8,
    HingeClose = 16,
    Cracking  = 32,
};

const int KI_NUM_ENUMS = 6;
const int KI_MAX_VALUE = 32;

} // namespace eAction
} // namespace Enums
} // namespace AttribSys

#endif // ATTRIBSYS_ENUMS_E_ACTION_H
