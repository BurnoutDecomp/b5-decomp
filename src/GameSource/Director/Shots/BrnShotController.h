#ifndef GAMESOURCE_DIRECTOR_SHOTS_BRN_SHOT_CONTROLLER_H
#define GAMESOURCE_DIRECTOR_SHOTS_BRN_SHOT_CONTROLLER_H

#include "types.hpp"

// ============================================================================
// GameSource/Director/Shots/BrnShotController.h
//
// BrnDirector::ShotContext + BrnDirector::ShotController -- the per-frame input bundle a
// director SHOT hands each of its controllers, and the abstract base every shot controller
// derives from. A Shot owns up to KI_MAX_CONTROLLERS (5) ShotController*, and
// Shot::ExecuteShot(const ShotContext&, Camera*) walks them, calling each one's virtual
// Update with the same context and the camera being produced.
//
// Layout/declaration authority: the DecFIGS DWARF for this exact source path
// (references/DecFIGS/dwarfdump/GameSource/Director/Shots/BrnShotController.h):
//
//     struct BrnDirector::ShotContext {                     // BrnShotController.h:48
//         const AllVehicleData*          mpAllVehicleData;      // :49
//         const ICE::CameraSpaceHandler* mpCameraSpaceHandler;  // :50
//         const Timestep*                mpTimestep;            // :51
//     }
//     struct BrnDirector::ShotController {                  // BrnShotController.h:62
//         vptr;
//         virtual void Update(const ShotContext&, Camera*);    // :76
//     }
//
// X360-corroborated by KeyAnimController::Update @0x8223D020, which is the only
// reconstructed Update in the family: it reads the context's +0x08 (mpTimestep) to advance
// its playback timer and its +0x04 (mpCameraSpaceHandler) to project the take's authored
// eye/look points into world space, and it writes into the Camera* second argument.
//
// This header carries ONLY the two declarations (no bodies): both types are pure plumbing
// and every substantial body belongs to a derived controller's own TU.
// ============================================================================

namespace ICE { class CameraSpaceHandler; }

namespace BrnDirector
{

// The per-frame world vehicle table (the shot controllers that anchor to a car resolve it
// through this). Its reconstructed home is the vehicle-data TU; ShotContext only stores a
// pointer, so an incomplete type is sufficient here.
struct AllVehicleData;

// The director frame-delta bundle (GameSource/Director/Utils/BrnDirectorTimestep.h). Only
// pointed at here -- forward-declared so this header stays dependency-free.
class Timestep;

namespace Camera { struct Camera; }

// ----------------------------------------------------------------------------
// ShotContext (DWARF BrnShotController.h:48). The read-only per-frame inputs a shot
// hands every one of its controllers.
// ----------------------------------------------------------------------------
struct ShotContext
{
    const AllVehicleData*          mpAllVehicleData;       // :49
    const ICE::CameraSpaceHandler* mpCameraSpaceHandler;   // :50
    const Timestep*                mpTimestep;             // :51
};

// ----------------------------------------------------------------------------
// ShotController (DWARF BrnShotController.h:62). The abstract per-frame camera-shot
// controller. One virtual; no data members beyond the vptr.
// ----------------------------------------------------------------------------
struct ShotController
{
    ShotController() {}

    // :76 -- advance this controller and write its contribution into the camera.
    //
    // Declared PURE here. The DWARF dump does not distinguish pure from plain virtual, but
    // the type is abstract in practice: it has no data members, no reconstructed body for
    // Update anywhere in the image, and every user is a derived controller that overrides
    // it (KeyAnimController @0x8223D020 is the reconstructed one). Making it pure keeps the
    // base from silently linking as an empty no-op if a derived override is ever missed.
    // (No virtual destructor: the console class has none -- every controller is embedded by
    // value in its owner and is never deleted through this base.)
    virtual void Update(const ShotContext& lrContext, Camera::Camera* lpCamera) = 0;
};

} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_SHOTS_BRN_SHOT_CONTROLLER_H
