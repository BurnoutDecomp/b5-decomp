#include "types.hpp"

namespace rw::math::vpu
{
class Vector3
{
public:
    Vector3(float lfX, float lfY, float lfZ);

private:
    float mafLane[4];
};

Vector3::Vector3(float lfX, float lfY, float lfZ)
    : mafLane{lfX, lfY, lfZ, 0.0f}
{
}
}
