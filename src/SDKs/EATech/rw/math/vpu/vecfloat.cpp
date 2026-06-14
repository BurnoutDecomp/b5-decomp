#include "types.hpp"

namespace rw::math::vpu
{
class VecFloat
{
public:
    explicit VecFloat(const float* pfValue);

private:
    float mafLane[4];
};

VecFloat::VecFloat(const float* pfValue)
{
    const float lfValue = *pfValue;
    mafLane[0] = lfValue;
    mafLane[1] = lfValue;
    mafLane[2] = lfValue;
    mafLane[3] = lfValue;
}
}
