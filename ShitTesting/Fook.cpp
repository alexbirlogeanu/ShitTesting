#include "Fook.h"
#include <iostream>

template<typename T>
Fook<T>::Fook()
{
    m_objects.reserve(10);//whatever
}

template<typename T>
Fook<T>::~Fook()
{
    m_objects.clear();
}

template<typename T>
bool Fook<T>::Add(T obj)
{
    std::cout << "Adding : ";
    Print(obj);
    std::cout << std::endl;
    m_objects.push_back(obj);
    return true;
}