#include "rw/physics/rigidbody.h"
#pragma float_control(precise, on, push)
namespace rw { namespace physics {
void RigidBody::InertiaUpdate(Inertia* lpInertia)
{
    const __vector4 lRi = __lvx(&mRi, 0);
    const __vector4 lUp = __lvx(&mUp, 0);
    const __vector4 lAt = __lvx(&mAt, 0);
    const __vector4 lDiagonal = lpInertia->GetInverseInertia().mV;
    const __vector4 lRiScaled = __vmulfp(lRi, __vspltw(lDiagonal, 0));
    const __vector4 lUpScaled = __vmulfp(lUp, __vspltw(lDiagonal, 1));
    const __vector4 lAtScaled = __vmulfp(lAt, __vspltw(lDiagonal, 2));

    __vector4 lFull = __vmulfp(lUpScaled, __vspltw(lUp, 0));
    lFull = __vmaddfp(lRiScaled, __vspltw(lRi, 0), lFull);
    lFull = __vmaddfp(lAtScaled, __vspltw(lAt, 0), lFull);
    lFull = __vrlimi(lFull, __lvx(&mIfull, 0), 1, 0);
    __stvx(lFull, &mIfull, 0);

    __vector4 lSplit = __vmulfp(__vpermwi(lUp, 0x97), __vpermwi(lUpScaled, 0x9B));
    lSplit = __vmaddfp(__vpermwi(lRi, 0x97), __vpermwi(lRiScaled, 0x9B), lSplit);
    lSplit = __vmaddfp(__vpermwi(lAt, 0x97), __vpermwi(lAtScaled, 0x9B), lSplit);
    lSplit = __vrlimi(lSplit, __lvx(&mIsplt, 0), 1, 0);
    __stvx(lSplit, &mIsplt, 0);
    const __vector4 lMass = __vspltw(__lvlx(&lpInertia->GetInverseMass(), 0), 0);
    __stvx(__vrlimi(__lvx(&mIfull, 0), lMass, 1, 0), &mIfull, 0);
}


}}
#pragma float_control(pop)
