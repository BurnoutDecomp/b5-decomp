// ============================================================================
// DECLARATION-ONLY + FLAG  (class:BrnPhysics::Vehicle ledger)
//
// Three of this class TU's 11 ledger functions reach members whose FULL layout is NOT
// homed in any committed header. They are intentionally NOT bodied here: bodying them
// would require fabricating an aggregate layout (raw-offset hacks into a foreign type or
// invented bit-array member offsets), which the reconstruction rules forbid. They remain
// declaration-only until their owning aggregates land their own ledger TUs.
//
// The other 8 functions of this TU ARE bodied (against committed named members):
//   @0x825E5010  BaseEventQueue<PhysicalTrafficState>::AddEvent()   -> BaseEventQueue_PhysicalTrafficState_AddEvent.cpp
//   @0x8254D8B8  BaseEventQueue<ImpactEvent>::GetEvent(s32)         -> BaseEventQueue_ImpactEvent_GetEvent.cpp
//   @0x827B4D48  BaseEventQueue<TrafficRemovedEvent>::GetEvent(s32) -> BaseEventQueue_TrafficRemovedEvent_GetEvent.cpp
//   @0x825B7AE8  CreatePhysicalTrafficEvent::operator=             -> CreatePhysicalTrafficEvent.cpp
//   @0x8270BF70  CreateArticulatedTrafficEvent::operator=          -> CreateArticulatedTrafficEvent.cpp
//   @0x825B3358  PhysicalTrafficVehicle::GetArticulatedVehicleType -> ../BrnPhysicalTrafficManager.cpp
//   @0x825A0FB0  VehicleManagerIO::GetVehicleManagerOutputInterface() const -> ../BrnVehicleManagerIO.cpp
//   @0x825A1058  VehicleManagerIO::GetVehicleManagerOutputInterface()       -> ../BrnVehicleManagerIO.cpp
//
// --------------------------------------------------------------------------------------
// FLAG 1 -- VehicleManagerOutputInterface::operator= (SetVehicleManagerOutputInterface)
//           @ 0x827A9B20   (called by *::InputBuffer_PostPhysics::SetVehicleManagerOutputInterface
//           and WorldModule::BridgePhysicsToOutput).
//
// The X360 body copies/clears members spread across a >2168-byte aggregate:
//   *(this+8)    = 0; TrafficCrashedEvent_::Append(this+0,   src+0)
//   *(this+0x150)= 0; TrafficSlammedEvent_::Append(this+0x150, src+0x150)
//   *(this+0x2F0)= 0; TrafficCrashedEvent_::Append(this+0x2F0, src+0x2F0)
//   *(this+0x3A0)= 0; RaceCarCrashEvent_::Append(this+0x3A0,  src+0x3A0)
//   *(this+0x5B0)= 0; RaceCarResetEvent_::Append(this+0x5B0,  src+0x5B0)
//   *(this+0x6C0)= 0; CreateVehicleResult_::Append(this+0x6C0, src+0x6C0)
//   *(this+0x750)= 0; short>::Append(this+0x750, src+0x750)
//   3x byte copy @0x79C..0x79E;  *(this+0x7A0)=0; TrafficRemovedEvent_::Append(this+0x7A0, src+0x7A0)
//   2x word copy @0x874/0x878
//
// The committed home (BrnVehicleOutputInterface.h) for VehicleManagerOutputInterface is a
// 256-byte NOMINAL blob -- the full member set (seven EventQueue<...> members + the byte/word
// scalars at +0x79C/+0x874) is that type's OWN future ledger TU. Bodying this operator= here
// would require either raw-offset stores into the committed blob (forbidden) or fabricating the
// full layout (forbidden). It ALSO depends on per-instantiation Append() siblings
// (RaceCarCrashEvent/RaceCarResetEvent/CreateVehicleResult/short queues) that are not yet
// homed. DECLARATION-ONLY until VehicleManagerOutputInterface's layout lands.
//
// --------------------------------------------------------------------------------------
// FLAG 2 -- ArticulatedJointPool removed-joint accessor    @ 0x825C2860
// FLAG 3 -- ArticulatedJointPool created-joint accessor    @ 0x825C26F0
//           (both called by ArticulatedJointPool::SendCreateRemoveJointEvents).
//
// These index into the ArticulatedJointPool's mRemovedJointBitArray / mCreatedJointBitArray
// (asserts "mRemovedJointBitArray.IsBitSet(liJointIndex)" / "mCreatedJointBitArray.IsBitSet(...)"
// at BrnPhysicalTrafficManagerIO.h:222/211) and return pointers into the joint/create arrays
// (`8*(liJointIndex+242)+pool` and `192*liJointIndex+pool+16`). The committed ArticulatedJointPool
// (BrnPhysicalTrafficManager.h) is a 112-byte OPAQUE slice ("size-correct slice ...; not laid out
// here") whose real member layout -- the two BitArrays at the +0x7E8/+0x7E0-region word offsets
// and the per-joint arrays at stride 8/192 -- is the ArticulatedJointPool's own ledger TU. The
// X360 offsets (8*((idx>>6)+253), 8*((idx>>6)+252), 8*(idx+242), 192*idx+16) cannot be reproduced
// without that layout, and reaching them via raw offsets into the opaque slice is forbidden.
// DECLARATION-ONLY until ArticulatedJointPool's full layout lands.
// ============================================================================
