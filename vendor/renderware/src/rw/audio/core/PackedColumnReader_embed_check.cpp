// Tiny embed/compile check for the rw::audio::core::PackedColumnReader home: includes the
// header and touches each reconstructed entry point so the gate exercises the TU.
#include "rw/audio/core/PackedColumnReader.h"

namespace
{
void PackedColumnReaderEmbedCheck()
{
    using namespace rw::audio::core;

    sizeof(PackedColumnReader);

    static u8 sBuffer[8] = { 0 };
    u8* pCursor = sBuffer;

    PackedColumnReader reader;
    reader.mppCursor = &pCursor;
    reader.mValue = 0;
    reader.mCount = 0;
    reader.mbLiteral = 0;

    reader.ReadRunInfo();
    reader.ReadSymbol();
    (void)reader.GetNextValue();
}
} // namespace
