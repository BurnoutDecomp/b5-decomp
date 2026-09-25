// ============================================================================
// GameSource/Director/Camera/BrnCameraCollisionPolicy.cpp -- NOT MOUNTED.
//
// RETIRED 2026-09-25 (FX-DIRECTOR2). This TU held BrnDirector::Camera::CollisionPolicy::Fail
// @0x82206450 with a BehaviourSharedInfo* first argument; the console passes the CAMERA (DWARF
// BrnCollisionPolicy.h:523 `Fail(Camera&, ValidityAccount::EFailedFlag)`, and both callers --
// VisibilityCollisionPolicy::ProcessSceneQueryResults and CollisionPolicyAttachedToVehicle::
// ResolveCollisions -- hand it the camera they were given). The corrected body lives in the
// MOUNTED BrnVisibilityCollisionPolicy.cpp, next to its first caller, rather than mounting this
// file.
// ============================================================================
