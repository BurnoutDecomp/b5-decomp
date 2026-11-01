#include "SDKs/EATech/rwcollision/volume_debug_access.h"
#include <cstring>

namespace rw { namespace collision {

extern const Volume::VTable gVolumeHandler_82F91740;
extern const Volume::VTable gVolumeHandler_82F918C0;
extern const Volume::VTable gVolumeHandler_82F919A4;
extern const Volume::VTable gVolumeHandler_82F9176C;
extern const Volume::VTable gVolumeHandler_82F91894;
extern const Volume::VTable gVolumeHandler_82F919D0;

Volume::VTable* gVolumeVTable[VOLUMETYPENUMINTERNALTYPES];

// ARTIST 82BB03A8 stores registry entries in this order.
RwBool Volume::InitializeVTable()
{
    gVolumeVTable[0] = 0;
    gVolumeVTable[VOLUMETYPEAGGREGATE] = const_cast<Volume::VTable*>(&gVolumeHandler_82F919D0);
    gVolumeVTable[VOLUMETYPESPHERE] = const_cast<Volume::VTable*>(&gVolumeHandler_82F91740);
    gVolumeVTable[VOLUMETYPECAPSULE] = const_cast<Volume::VTable*>(&gVolumeHandler_82F918C0);
    gVolumeVTable[VOLUMETYPETRIANGLE] = const_cast<Volume::VTable*>(&gVolumeHandler_82F919A4);
    gVolumeVTable[VOLUMETYPEBOX] = const_cast<Volume::VTable*>(&gVolumeHandler_82F9176C);
    gVolumeVTable[VOLUMETYPECYLINDER] = const_cast<Volume::VTable*>(&gVolumeHandler_82F91894);
    return 1;
}

// ARTIST 82BB12D0 copies the complete 0x60-byte record.
Volume& Volume::operator=(const Volume& source)
{
    memcpy(this, &source, sizeof(*this));
    return *this;
}

} }
