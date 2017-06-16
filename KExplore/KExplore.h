#pragma once

typedef PEJOB (*FPspGetNextJob)(PEJOB Job);

struct KernelFunctions {
    FPspGetNextJob PspGetNextJob;
};

