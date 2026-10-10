#pragma once

namespace CgsPC::Debug
{
    // FLAG PC-platform leaf: unavailable native menu pages may explain an
    // explicit UI request. Engine-driven HUD activation must stay silent.
    inline thread_local bool sbComponentMenuRequest = false;

    class ComponentMenuRequest
    {
    public:
        ComponentMenuRequest() : mbPrevious(sbComponentMenuRequest)
        { sbComponentMenuRequest = true; }
        ~ComponentMenuRequest() { sbComponentMenuRequest = mbPrevious; }
        ComponentMenuRequest(const ComponentMenuRequest&) = delete;
        ComponentMenuRequest& operator=(const ComponentMenuRequest&) = delete;
    private:
        bool mbPrevious;
    };
}
