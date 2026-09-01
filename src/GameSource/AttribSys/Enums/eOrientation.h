#ifndef ATTRIBSYS_ENUMS_E_ORIENTATION_H
#define ATTRIBSYS_ENUMS_E_ORIENTATION_H

namespace AttribSys
{
namespace Enums
{
namespace eOrientation
{

// DecFIGS eOrientation.h:12. These values are authored flags.
enum eOrientation
{
    Front  = 1,
    Side   = 2,
    Rear   = 4,
    Roof   = 8,
    Bottom = 16,
};

const int KI_NUM_ENUMS = 5;
const int KI_MAX_VALUE = 16;

} // namespace eOrientation
} // namespace Enums
} // namespace AttribSys

#endif // ATTRIBSYS_ENUMS_E_ORIENTATION_H
