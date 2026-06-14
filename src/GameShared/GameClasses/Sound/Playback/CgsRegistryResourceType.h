#pragma once

#include "types.hpp"

namespace CgsDev
{
class Assert
{
public:
    static void BeginAssert();
    static void FireAssert(const char* lpcExpression, const char* lpcFile, int liLine);
    static void EndAssert();
};
}

namespace CgsResource
{
class RegistryResourceType
{
public:
    struct ResourceDescriptor
    {
        struct Entry
        {
            u32 muSize;
            u32 muAlignment;
        };

        Entry maEntries[5];
    };

    struct Registry
    {
        u32 muUnknown0;
        u32 muStringCount;
        u32 muNameDataSize;
        u32 muUnknown12;
        u32 muPayloadSize;
    };

    ResourceDescriptor* GetSerialisedResourceDescriptor(ResourceDescriptor* pDescriptor, const Registry* pRegistry);
    static int GetTypeID();
};

inline RegistryResourceType::ResourceDescriptor* RegistryResourceType::GetSerialisedResourceDescriptor(
    ResourceDescriptor* pDescriptor,
    const Registry* pRegistry)
{
    if (!pRegistry)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpRegistry", "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/CgsRegistryResourceType.h", 91);
        CgsDev::Assert::EndAssert();
    }

    const u32 luSerialisedSize =
        pRegistry->muPayloadSize + pRegistry->muNameDataSize + 4 * (pRegistry->muStringCount + 7);

    for (ResourceDescriptor::Entry& lEntry : pDescriptor->maEntries)
    {
        lEntry.muSize = 0;
        lEntry.muAlignment = 1;
    }

    pDescriptor->maEntries[3].muSize = luSerialisedSize;
    pDescriptor->maEntries[4].muSize = luSerialisedSize;
    pDescriptor->maEntries[0].muSize = luSerialisedSize;
    pDescriptor->maEntries[0].muAlignment = 4;
    return pDescriptor;
}

inline int RegistryResourceType::GetTypeID()
{
    return 40960;
}
}
