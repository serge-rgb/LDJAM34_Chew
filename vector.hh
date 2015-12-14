#pragma once

template<typename T>
struct Vec2
{
    union
    {
        struct
        {
            T x;
            T y;
        };
        struct
        {
            T w;
            T h;
        };
        T d[2];
    };
};

typedef Vec2<float> v2f;

template<typename T> bool operator== (Vec2<T> a, Vec2<T> b)
{
    bool result = a.x == b.x && a.y == b.y;
    return result;
}

template<typename T> bool operator!= (Vec2<T> a, Vec2<T> b)
{
    bool result = !(a == b);
    return result;
}

template<typename T> Vec2<T> operator- (Vec2<T> a, Vec2<T> b)
{

    Vec2<T> result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

template<typename T> Vec2<T> operator-= (Vec2<T> a, Vec2<T> b)
{
    Vec2<T> res;
    res = a - b;
    return res;
}

template<typename T> Vec2<T> operator+ (Vec2<T> a, Vec2<T> b)
{
    Vec2<T> result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

template<typename T> Vec2<T> operator* (Vec2<T> a, Vec2<T> b)
{
    Vec2<T> result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

template<typename T> Vec2<T> operator* (Vec2<T> v, T factor)
{
    Vec2<T> result;
    result.x = factor * v.x;
    result.y = factor * v.y;
    return result;
}

template<typename T> Vec2<T> operator/ (Vec2<T> v, T factor)
{
    Vec2<T> result;
    result.x = v.x / factor;
    result.y = v.y / factor;
    return result;
}

template<typename T> Vec2<T> perpendicular (Vec2<T> a)
{
    Vec2<T> result =
    {
        -a.y,
        a.x
    };
    return result;
}

