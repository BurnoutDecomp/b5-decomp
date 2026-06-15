#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"

// CgsModule::EventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent, 20>::Construct  @ 0x8228DDC0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (20) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnPhysics::Deformation::GlassSmashOrCrackEvent, 20>::Construct();
