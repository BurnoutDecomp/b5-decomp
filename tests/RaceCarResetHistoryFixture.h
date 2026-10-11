#pragma once
#include "GameShared/GameClasses/Containers/CgsArray.h"

// Test-owned state for the production producers. Real Array semantics preserve
// the original capacity/assert behavior; no writer is implemented in this fixture.
struct RaceCarResetHistoryFixture
{
    Matrix44Affine mLastResetTransform;
    Vector3 mLastResetLineTestStart = {}, mLastResetLineTestEnd = {};
    Array<Vector3, 10> mLastResetLineTestIntersections;
    RaceCarResetHistoryFixture()
    {
        mLastResetTransform.SetIdentity();
        mLastResetLineTestIntersections.Clear();
    }
};
