#include "types.hpp"

namespace rw::math::vpu
{
class Vector2
{
public:
    Vector2(float lfX, float lfY);

private:
    float mafLane[4];
};

Vector2::Vector2(float lfX, float lfY)
    : mafLane{lfX, lfY, 0.0f, 0.0f}
{
}
}
