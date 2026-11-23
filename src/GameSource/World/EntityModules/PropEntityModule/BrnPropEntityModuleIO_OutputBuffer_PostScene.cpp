#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

// DecFIGS BrnPropEntityModuleIO.cpp:221: the whole body is the base teardown. ARTIST ICF-folds it into
// BrnWorld::PropEntityIO::OutputBuffer_PreScene::Destruct @0x822DC3D0 (`b CgsModule::IOBuffer::Destruct`),
// the target DestroyIOBuffer<OutputBuffer_PostScene> @0x827B6E18 calls.
void BrnWorld::PropEntityIO::OutputBuffer_PostScene::Destruct()
{
    CgsModule::IOBuffer::Destruct();
}
