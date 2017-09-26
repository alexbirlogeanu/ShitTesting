#pragma once

#include "Fook.h"
struct Params
{
    int a;
    char b;
};


class FU : public Fook<Params>
{
public:
    FU();

    virtual ~FU();

    void Print(const Params& p) override;
};