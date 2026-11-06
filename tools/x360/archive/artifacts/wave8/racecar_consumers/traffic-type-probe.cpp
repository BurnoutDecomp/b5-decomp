#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
VecFloat cap(const BrnTraffic::VehicleTypeRuntime& v) { return v.GetCabPivotDistance(); }
VecFloat trailer(const BrnTraffic::VehicleTypeRuntime& v) { return v.GetTrailerPivotDistance(); }
VecFloat back(const BrnTraffic::VehicleTypeRuntime& v) { return v.GetBackAxleOffset(); }
VecFloat forward(const BrnTraffic::VehicleTypeRuntime& v) { return v.GetForwardAxleOffset(); }
VecFloat mass(const BrnTraffic::VehicleTypeRuntime& v) { return v.GetMass(); }
VecFloat radius(const BrnTraffic::VehicleTypeRuntime& v) { return v.GetWheelRadius(); }
void axle(BrnTraffic::Axle& a) { a.Initialise(); }
