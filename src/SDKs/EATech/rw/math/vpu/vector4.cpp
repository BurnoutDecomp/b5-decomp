#include "types.hpp"

namespace rw::math::vpu
{
class Vector4
{
public:
    Vector4(float lfX, float lfY, float lfZ, float lfW);

private:
    float mafLane[4];
};

Vector4::Vector4(float lfX, float lfY, float lfZ, float lfW)
    : mafLane{lfX, lfY, lfZ, lfW}
{
}
}
