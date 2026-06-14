#pragma once

namespace rw::math::fpu
{
template <typename Type>
class Vector2Template
{
public:
    Vector2Template(Type lX, Type lY);

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
