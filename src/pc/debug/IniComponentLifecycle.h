#pragma once

// FLAG PC-platform leaf: remember which lazy pages the startup INI activated.
// This sidecar adds no fields to recovered engine objects and owns no values.
#include <algorithm>
#include <mutex>
#include <vector>

namespace CgsPC::Debug
{
    struct IniComponentLifecycle
    {
        std::mutex mutex;
        std::vector<const void*> components;
    };

    inline IniComponentLifecycle& GetIniComponentLifecycle()
    {
        static IniComponentLifecycle state;
        return state;
    }

    inline void ResetIniComponents()
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.components.clear();
    }

    inline void SetIniComponent(const void* component, bool enabled)
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto entry = std::find(state.components.begin(), state.components.end(), component);
        if (enabled && entry == state.components.end()) state.components.push_back(component);
        else if (!enabled && entry != state.components.end()) state.components.erase(entry);
    }

    inline bool IsIniComponent(const void* component)
    {
        auto& state = GetIniComponentLifecycle();
        std::lock_guard<std::mutex> lock(state.mutex);
        return std::find(state.components.begin(), state.components.end(), component) != state.components.end();
    }
}
