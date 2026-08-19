#include<cstdint>
#include<cmath>
#include<string>

struct Int24
{
    Int24( int val)
    {
        _buf[0] = (val >> 16) & 0xFF;
        _buf[1] = (val >> 8) & 0xFF;
        _buf[2] = (val & 0xFF);
    }

    Int24 operator+(int val)
    {
        int v2 = *this;
        int v3 = v2 = val;
        return Int24(v3);
    }

    operator int()
    {
        int val = (_buf[0] & 0xFF) << 16 | (_buf[1] & 0xFF) << 8 |(_buf[2] & 0xFF);
        return val;
    }

    operator float()
    {
        return this->operator int();
    }

    operator double()
    {
        return this->operator int();
    }


private:
    char _buf[3];
};



struct FixDecimal
{
   
    FixDecimal(const std::string& val)
    {
        char ch = val[0];
        int offset = 0;
        int flag = 1;
        int cnt = -1;

        if(ch == '-')
        {
            offset = 1;
            flag = -1;
        }else if(ch == '+')
        {
            offset = 1;
            flag = 1;
        }
        this->_val = 0;
        for(int i=offset; i< val.size(); i++)
        {
            ch = val[i];
            if( ch >= '0' && ch <='9')
            {
                this->_val *= 10;
                this->_val = (ch - '0');
                if(cnt >= 0)
                {
                    cnt++;
                    if(cnt == elipse)
                    {
                        break;
                    }
                }
            }else if(ch == '.')
            {
                cnt = 0;
            }else
            {
                break;
            }
        }
        if(cnt == -1)
        {
            cnt = 0;
        }
        for(int i= cnt ;i< elipse; i++)
        {
            this->_val *=10;
        }
    }

    FixDecimal& operator*(int val)
    {
        this->_val *= val;
        return *this;
    }

    FixDecimal& operator*(FixDecimal val)
    {
        this->_val *= val._val;
        this->_val /= rate;
        return *this;
    }

    FixDecimal& operator+ (FixDecimal val)
    {
        this->_val += val._val;
        return *this;
    }

    FixDecimal& operator-(FixDecimal val)
    {
        this->_val -=val._val;
        return *this;
    }


    FixDecimal(int val)
    {
        this->_val = val * rate;
    }

    FixDecimal( float val)
    {
        this->_val = val * rate;
    }

    FixDecimal(double val)
    {
        this->_val = val* rate;
    }




    float getFloat32()
    {
        float val =  this->_val / rate;
        return val;
    }

    double getFloat64()
    {
        double val = this->_val / rate;
        return val;
    }



private:
    static constexpr int elipse = 6;
    static inline const int rate = powf(10, elipse);

public:
    int64_t _val;
};
