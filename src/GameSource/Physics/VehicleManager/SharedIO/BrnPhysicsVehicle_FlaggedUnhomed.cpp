// ============================================================================
// class:BrnPhysics::Vehicle ledger -- RESOLUTION NOTE (was DECLARATION-ONLY + FLAG)
//
// This TU's 11 ledger functions ARE ALL NOW BODIED. The 3 that were previously
// declaration-only (their owning aggregates were un-homed opaque slices) landed real
// layouts elsewhere in this wave and are now bodied against committed named members:
//
//   @0x827A9B20  VehicleManagerOutputInterface::operator=  (dossier 'VehicleManagerOutputInt')
//       -> BrnVehicleManagerOutputInterface.cpp, against the full member set now laid out in
//          BrnVehicleOutputInterface.h (seven EventQueue<...> members + VehicleGuiOutputMessages
//          @0x79C + WheelFFSpring @0x874, replacing the prior 256-byte NOMINAL blob).
//   @0x825C2860  ArticulatedJointCreateBuffer::GetRemoveJointEvent (dossier 'ArticulatedJointCreat')
//   @0x825C26F0  ArticulatedJointCreateBuffer::GetCreateJointEvent (dossier 'ArticulatedJointCreateBu')
//       -> BrnPhysicalTrafficManagerIO.cpp, against ArticulatedJointCreateBuffer's committed
//          layout (mCreatedJointBitArray@0x7E0 / mRemovedJointBitArray@0x7E8, maCreatedJointEvents
//          @0x10 stride 192, maRemovedJointEvents@0x790 stride 8 -- byte-for-byte matching the
//          asm's 8*((idx>>6)+253/252), 8*(idx+242), 192*idx+16). NOTE: these two accessors index
//          the ArticulatedJointCreateBuffer IO buffer (BrnPhysicalTrafficManagerIO.h), NOT the
//          unrelated same-named-field ArticulatedJointPool embedded in
//          ../BrnPhysicalTrafficManager.h (that manager's own opaque slice belongs to the
//          separate, already-`done` class:BrnPhysics::Vehicle::PhysicalTrafficManager TU and is
//          untouched by this resolution).
//
// The other 8 functions of this TU were already bodied (against committed named members):
//   @0x825E5010  BaseEventQueue<PhysicalTrafficState>::AddEvent()   -> BaseEventQueue_PhysicalTrafficState_AddEvent.cpp
//   @0x8254D8B8  BaseEventQueue<ImpactEvent>::GetEvent(s32)         -> BaseEventQueue_ImpactEvent_GetEvent.cpp
//   @0x827B4D48  BaseEventQueue<TrafficRemovedEvent>::GetEvent(s32) -> BaseEventQueue_TrafficRemovedEvent_GetEvent.cpp
//   @0x825B7AE8  CreatePhysicalTrafficEvent::operator=             -> CreatePhysicalTrafficEvent.cpp
//   @0x8270BF70  CreateArticulatedTrafficEvent::operator=          -> CreateArticulatedTrafficEvent.cpp
//   @0x825B3358  PhysicalTrafficVehicle::GetArticulatedVehicleType -> ../BrnPhysicalTrafficManager.cpp
//   @0x825A0FB0  VehicleManagerIO::GetVehicleManagerOutputInterface() const -> ../BrnVehicleManagerIO.cpp
//   @0x825A1058  VehicleManagerIO::GetVehicleManagerOutputInterface()       -> ../BrnVehicleManagerIO.cpp
//
// This TU now contributes no bodies of its own; kept as a resolution record (deleting it would
// lose the cross-reference trail from the earlier FOUNDATION wave).
// ============================================================================
