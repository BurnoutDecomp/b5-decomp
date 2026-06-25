#pragma once

#include "types.hpp"

namespace BrnDirector
{
    class DirectorResourceManager;
    class MainDirector;

    namespace Camera { class Camera; }
    namespace DirectorIO { struct InputBuffer; }

    class DirectorDevTools
    {
    public:
        void Construct(MainDirector* lpDirectorModule,
                       DirectorResourceManager* lpDirectorResourceManager);
        void Update(const DirectorIO::InputBuffer* lpInput, const Camera::Camera& lrCamera);

    private:
        void LiveCamUpdate(const Camera::Camera& lrCamera);

        DirectorResourceManager* mpDirectorResourceManager;
        MainDirector* mpDirectorModule;

        static DirectorDevTools* mpInstance;
    };
}
