#pragma once

namespace CgsResource { class Pool; struct Entry; }

namespace CgsResource::PCAptOwnership
{
    // FLAG PC-platform leaf: couple AptFile lifetime to the synchronous bundle
    // transport's resident entry list. The Apt engine and its import records are unchanged.
    enum class RetainResult { Acquired, AlreadyHeld, MissingOwner, Failed };
    RetainResult Retain(const void* lpHeader, const void* lpFile);
    bool Release(const void* lpFile);
    bool IsBundleLoaded(Pool* lpPool, const char* lpacPath);
    Entry* FindBundleLead(Pool* lpPool, const char* lpacPath);
    void VisitBundleEntries(Pool* lpPool, const char* lpacPath,
                            void (*lpVisitor)(Entry*, void*), void* lpContext);
}
