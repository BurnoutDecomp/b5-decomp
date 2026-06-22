// Tiny embed/ODR + linkage check for the AptValue / AptValueVector /
// AptValueGC_PoolManager home group. Exercises each owned entry point from a
// TU other than its defining .cpp so the headers' layout + signatures compile.
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"

// Concrete AptValue (AptValue is abstract) to exercise the owned virtuals.
namespace
{
    class EmbedAptValue : public AptValue
    {
    public:
        EmbedAptValue() : AptValue(AptVFT_None) {}
        void AddRef() override {}
        void Release() override {}
        bool IsGarbageCollected() const override { return false; }
        void RegisterReferences() override {}
    };

    class EmbedProducer : public AptValueProducer
    {
    public:
        AptValue* Produce() override { return nullptr; }
    };
}

void AptValueGroup_EmbedCheckEntry()
{
    // AptValueVector owned methods.
    AptValueVector vec;
    vec.mnTop = 0;
    vec.mnCapacity = 0;
    vec.mppItems = nullptr;
    EmbedProducer prod;
    (void)vec.PopAndPush(0, &prod);
    vec.SafePop(0);
    (void)vec.pop();
    vec.Shutdown();
    vec.shutdown();

    // AptValueGC_PoolManager statics + (touch the accessor signatures).
    AptValueGC_PoolManager::StaticInitialize();

    // AptValue owned virtuals reachable through the vtable.
    AptValue* pv = new EmbedAptValue();
    pv->ForceDelete();   // -> PreDestroy / DestroyGCPointers / delete this
}
