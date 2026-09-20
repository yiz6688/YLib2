#pragma once
#include"base_config.hpp"
#include"fftw3.h"
#include<complex>

using cpx = std::complex<double>;

class FFT
{

public:
    FFT(int fftSize)
    {

        double* td = fftw_alloc_real(fftSize);
        this->tdSpan = span_ns::span<double>(td, fftSize);

        int size = fftSize / 2;
        if(fftSize % 2 == 0)
        {
            size++;
        }

        fftw_complex *fd = fftw_alloc_complex(size);
        this->fdSpan = span_ns::span<cpx>(reinterpret_cast<cpx*>(fd), size);

        this->pfft = fftw_plan_dft_r2c_1d(fftSize, td, fd, FFTW_ESTIMATE);
        this->pifft = fftw_plan_dft_c2r_1d(fftSize, fd, td, FFTW_ESTIMATE);
    }


    ~FFT()
    {
        double* td = this->tdSpan.data();
        cpx* fd = this->fdSpan.data();

        fftw_free(td);
        fftw_free(fd);

        fftw_destroy_plan(this->pfft);
        fftw_destroy_plan(this->pifft);
    }


    void fft(span_ns::span<double> input)
    {
        if(input.data() != this->tdSpan.data())
        {
            std::copy(input.begin(), input.end(),  this->tdSpan.begin());
        }
        int len = input.size();
        std::fill(this->tdSpan.begin() + len, this->tdSpan.end(), 0.0);

        fftw_execute(this->pfft);
    }

    void ifft(span_ns::span<cpx> input)
    {
        if(input.data() != this->fdSpan.data())
        {
            std::copy(input.begin(), input.end(),  this->fdSpan.begin());
        }
        int len = input.size();
        std::fill(this->fdSpan.begin() + len, this->fdSpan.end(), cpx());

        fftw_execute(this->pifft);
    }

    int getFFTSize()
    {
        return this->tdSpan.size();
    }

    span_ns::span<double> getTD()
    {
        return this->tdSpan;
    }

    span_ns::span<cpx> getFD()
    {
        return this->fdSpan;
    }

private:
    span_ns::span<double> tdSpan;  //time_domain ,时域信号
    span_ns::span<cpx>  fdSpan;    //freq_domain ,频域信号    

    fftw_plan pfft;
    fftw_plan pifft;

};


class FFT2
{

public:
    FFT2(int fftSize)
    {

        fftw_complex* td = fftw_alloc_complex(fftSize);
        this->tdSpan = span_ns::span<cpx>(reinterpret_cast<cpx*>(td), fftSize);

        fftw_complex *fd = fftw_alloc_complex(fftSize);
        this->fdSpan = span_ns::span<cpx>(reinterpret_cast<cpx*>(fd), fftSize);

        this->pfft = fftw_plan_dft_1d(fftSize, td, fd, FFTW_FORWARD, FFTW_ESTIMATE);
        this->pifft = fftw_plan_dft_1d(fftSize, fd, td, FFTW_BACKWARD, FFTW_ESTIMATE);
    }


    ~FFT2()
    {
        cpx* td = this->tdSpan.data();
        cpx* fd = this->fdSpan.data();

        fftw_free(td);
        fftw_free(fd);

        fftw_destroy_plan(this->pfft);
        fftw_destroy_plan(this->pifft);
    }


    void fft(span_ns::span<double> input)
    {
        
        for(int i=0;i<input.size(); i++)
        {
            this->tdSpan[i].real(input[i]);
            this->tdSpan[i].imag(0);
        }
        
        int len = input.size();
        std::fill(this->tdSpan.begin() + len, this->tdSpan.end(), cpx());

        fftw_execute(this->pfft);
    }

    void ifft(span_ns::span<cpx> input)
    {
        if(input.data() != this->fdSpan.data())
        {
            std::copy(input.begin(), input.end(),  this->fdSpan.begin());
        }
        int len = input.size();
        std::fill(this->fdSpan.begin() + len, this->fdSpan.end(), cpx());

        fftw_execute(this->pifft);
    }

    int getFFTSize()
    {
        return this->tdSpan.size();
    }

    span_ns::span<cpx> getTD()
    {
        return this->tdSpan;
    }

    span_ns::span<cpx> getFD()
    {
        return this->fdSpan;
    }

private:
    span_ns::span<cpx> tdSpan;      //time_domain ,时域信号
    span_ns::span<cpx>  fdSpan;    //freq_domain ,频域信号    

    fftw_plan pfft;
    fftw_plan pifft;

};
