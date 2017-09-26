#pragma once

#include <vector>

template<typename T>
class Fook
{
public:
    Fook();
    virtual ~Fook();

    bool Add(T obj);
    virtual void Print(const T& obj) = 0;
private:
    std::vector<T>  m_objects;
};