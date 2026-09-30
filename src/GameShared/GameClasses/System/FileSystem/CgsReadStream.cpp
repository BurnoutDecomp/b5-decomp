#include "GameShared/GameClasses/System/FileSystem/CgsReadStream.h"

#include "GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.h"

namespace CgsFileSystem
{

void ReadStream::Construct(StreamDeviceDiskRead* apStreamDevice)
{
    mpStreamDevice = apStreamDevice;
}

u32 ReadStream::Read(u32 auAmount, void* apDestination)
{
    return mpStreamDevice->Read(auAmount, apDestination);
}

void ReadStream::Seek(u64 auPosition)
{
    mpStreamDevice->Seek(auPosition);
}

bool ReadStream::IsValid() const
{
    return mpStreamDevice != 0;
}

u64 ReadStream::Tell() const
{
    return mpStreamDevice->Tell();
}

u32 ReadStream::GetAmountOfDataInBuffer() const
{
    return mpStreamDevice->GetAmountOfDataInBuffer();
}

ReadStream& ReadStream::operator=(StreamDeviceDiskRead* apStreamDevice)
{
    mpStreamDevice = apStreamDevice;
    return *this;
}

} // namespace CgsFileSystem
