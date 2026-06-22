#pragma once

namespace rw::math::fpu
{
template <typename Type>
class Vector2Template
{
public:
    Vector2Template(Type lX, Type lY);

    // Additive grow (World-AI group): the canonical rwmath component readers.
    // BrnAStar's distance heuristics read x/y from a Vector2Template<float> &;
    // on X360 these are direct loads of the two leading lanes (lfs 0(rN)/4(rN)).
    // Inline accessors keep member access by-name and do not change the layout
    // (mX at +0, mY at +4 preserved).
    Type X() const { return mX; }
    Type Y() const { return mY; }

private:
    Type mX;
    Type mY;
};

template <typename Type>
class Vector3Template
{
public:
    Vector3Template(Type lX, Type lY, Type lZ);

private:
    Type mX;
    Type mY;
    Type mZ;
};

template <typename Type>
class Vector4Template
{
public:
    Vector4Template(Type lX, Type lY, Type lZ, Type lW);

private:
    Type mX;
    Type mY;
    Type mZ;
    Type mW;
};
}
