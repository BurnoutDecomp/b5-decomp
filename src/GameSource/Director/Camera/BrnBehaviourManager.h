#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_MANAGER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_MANAGER_H

// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager.h
//
// BrnDirector::Camera::BehaviourManager -- owns every live director camera behaviour and the
// per-state behaviour-allocation book-keeping. DWARF home BrnBehaviourManager.h.
//
// CANONICAL PATH ONLY (forward declaration). The BehaviourManager type does not yet have a
// reconstructed home of its own; its current minimal slice (allocate / release / pause-update
// + the UnSetBehaviourUsedByHandle / CheckNoBehavioursAreAllocatedByState release methods)
// lives in GameSource/Director/Utils/BrnICEMoviePlayer.h. Defining the class here too would
// fork it (a hard ODR redefinition once both are in one TU), so this header only forward-
// declares it -- include the ICE movie-player header (or, in future, the dedicated
// BehaviourManager TU's header) when the COMPLETE type is needed. Pointer/reference members
// only need this forward declaration.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace Camera
    {
        class BehaviourManager;   // complete definition: BrnICEMoviePlayer.h (until its own TU lands)
    }
}

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_BEHAVIOUR_MANAGER_H
