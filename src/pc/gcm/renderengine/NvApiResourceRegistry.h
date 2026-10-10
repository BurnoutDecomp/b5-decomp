#pragma once

#include <d3d9.h>
#include <algorithm>
#include <vector>

// FLAG PC-platform leaf: NVAPI registration belongs to a resource lifetime,
// not its address. Hold a COM reference until unregistration succeeds. Track
// every successful registration, including resolve variants that were not used.
namespace renderengine
{
    template<class Resource> class NvApiResourceRegistryPC
    {
    public:
        using Operation = int (__cdecl*)(Resource*);
    private:
        Operation mpRegister = nullptr;
        Operation mpUnregister = nullptr;
        std::vector<Resource*> maResources;
    public:
        NvApiResourceRegistryPC() = default;
        NvApiResourceRegistryPC(const NvApiResourceRegistryPC&) = delete;
        NvApiResourceRegistryPC& operator=(const NvApiResourceRegistryPC&) = delete;
        ~NvApiResourceRegistryPC() { Clear(); }

        bool Configure(Operation lpRegister, Operation lpUnregister)
        {
            if (!Clear()) return false;
            mpRegister = lpRegister;
            mpUnregister = lpUnregister;
            return mpRegister && mpUnregister;
        }

        bool Register(Resource* lpResource)
        {
            if (!lpResource || !mpRegister || !mpUnregister) return false;
            if (std::find(maResources.begin(), maResources.end(), lpResource) != maResources.end())
                return true;
            // Allocate bookkeeping before the driver call, so a successful
            // registration can never be lost to a later allocation failure.
            maResources.push_back(lpResource);
            if (mpRegister(lpResource) != 0)
            {
                maResources.pop_back();
                return false;
            }
            lpResource->AddRef();
            return true;
        }

        bool Clear()
        {
            size_t luRetained = 0;
            for (Resource* lpResource : maResources)
            {
                if (mpUnregister(lpResource) == 0)
                    lpResource->Release();
                else
                    // Preserve both the registration and object identity on
                    // failure. The next joined retirement can retry safely.
                    maResources[luRetained++] = lpResource;
            }
            maResources.resize(luRetained);
            return maResources.empty();
        }
    };

    inline NvApiResourceRegistryPC<IDirect3DResource9> gNvApiDepthResourcesPC;
}
