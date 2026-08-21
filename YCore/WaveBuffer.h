#pragma once
#include"WaveFormat.h"
#include"myType.h"

class WaveBuffer
{

public:
    WaveBuffer(int _sampleLen, SampleType _sampleType);

    ~WaveBuffer();




public:





private:
    union _sample_
    {
        double *pd;
        float  *pf;
        int    *pi;
        int24  *pt;
        short  *ps;
        char   *raw;
    };

    
    int sampleLen;

    SampleType sampleType;

    _sample_ ss;
};