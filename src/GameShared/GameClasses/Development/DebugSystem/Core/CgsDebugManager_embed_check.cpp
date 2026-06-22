// Compile-only embedder check for CgsDev::DebugManager. Not part of the shipping build: it embeds
// the manager by value and exercises the thread-safe accessors + the font handoff by name -- the
// same SetDebugFont / ThreadSafeAquire / ThreadSafeRelease entry points GamePrepare and the
// DebugComponent registration path drive (X360 0x823B14E8 / 0x821F1E50 / 0x821F1EB8).
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"

namespace CgsDev
{

struct DebugManagerEmbedderCheck
{
    DebugManager mDebugManager;
};

void DebugManagerEmbedderCheck_Exercise(
        DebugManagerEmbedderCheck& lrEmbedder,
        const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont)
{
    DebugManager& lrManager = lrEmbedder.mDebugManager;

    // Font handoff to the debug renderers (X360 0x823B14E8).
    lrManager.SetDebugFont(lrFont);

    // Singleton + critical-section bracket (X360 0x821F1E50 / 0x821F1EB8).
    DebugManager* lpAcquired = DebugManager::ThreadSafeAquire();
    DebugManager::ThreadSafeRelease(lpAcquired);
}

}
