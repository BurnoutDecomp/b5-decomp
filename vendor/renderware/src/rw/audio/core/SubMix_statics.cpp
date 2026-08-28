// =====================================================================================
// SubMix_statics.cpp -- the two SubMix class statics the Send connect-by-name path
// walks (sSubMixList / spSubMixNextNode). The SubMix plug-in itself has NO vendor TU
// in the Feb-2007 tree (its console body is @0x82B9C370-family, phase-D-adjacent);
// these zero-initialised statics are its honest minimal home: an EMPTY submix list,
// so ConnectByNameHandler finds no submix and fails through its guarded path.
// FLAG PC link-closure home (AEMS-cascade wave 2026-08-28); grow into the real
// SubMix TU when its slice lands.
// =====================================================================================
#include "rw/audio/core/SubMixConnector.h"

namespace rw { namespace audio { namespace core {
ListDStack SubMix::sSubMixList = {};
ListDNode *SubMix::spSubMixNextNode = 0;
} } }