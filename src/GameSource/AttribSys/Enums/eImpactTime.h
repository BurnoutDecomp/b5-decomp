#ifndef ATTRIBSYS_ENUMS_E_IMPACT_TIME_H
#define ATTRIBSYS_ENUMS_E_IMPACT_TIME_H

namespace AttribSys
{
namespace Enums
{
namespace eImpactTime
{

// DecFIGS eImpactTime.h:12. The authored values are flags rather than a
// zero-based sequence; zero is not a valid state.
enum eImpactTime
{
    False = 1,
    True  = 2,
    VSlow = 4,
};

const int KI_NUM_ENUMS = 3;
const int KI_MAX_VALUE = 4;

} // namespace eImpactTime
} // namespace Enums
} // namespace AttribSys

#endif // ATTRIBSYS_ENUMS_E_IMPACT_TIME_H
