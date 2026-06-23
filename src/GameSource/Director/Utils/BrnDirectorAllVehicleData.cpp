// ============================================================================
// GameSource/Director/Utils/BrnDirectorAllVehicleData.cpp
//
// Compilation home for the BrnDirector::AllVehicleData::Array<NearestCarInfo, 8> container
// slice this TU owns. The TU is the X360 Array<NearestCarInfo, 8>::Append instantiation
// @0x821FBA48 (IDA spells it BrnDirector::AllVehicleData::NearestCarInfo,8>::Append). Its body
// is the generic Array<T,N>::Append in CgsArray.h; the out-of-line anchor below forces exactly
// that instantiation (Append only) to be emitted -- mirroring the X360, which instantiated
// only the Append member for this element type. (A whole-class explicit instantiation would
// drag in Contains/FindFirstInstanceOf, which require NearestCarInfo::operator== -- not part
// of this TU and never instantiated by the console.)
//
// Append is called by AllVehicleData::Update @0x8221D938 to push each computed nearest-car
// record into the snapshot's fixed 8-slot array.
// ============================================================================

#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"

namespace BrnDirector
{

// Out-of-line anchor: pushes one nearest-car record onto the snapshot's array (the operation
// AllVehicleData::Update performs once per nearby vehicle).
void AllVehicleData_AppendNearestCarAnchor(
    AllVehicleData::NearestCarInfoArray& lrArray,
    const AllVehicleData::NearestCarInfo& lrInfo)
{
    lrArray.Append(lrInfo);
}

} // namespace BrnDirector
