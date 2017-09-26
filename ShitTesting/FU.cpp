#include "FU.h"

#include <iostream>
#include "Fook.cpp"

template class Fook<Params>;

FU::FU()
{
}

FU::~FU()
{
}

void FU::Print(const Params& p)
{
    std::cout << "A: " << p.a << "\t\t B: " << p.b;
}