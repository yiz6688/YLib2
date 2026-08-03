#include<fstream>
#include<string>
#include<iomanip>
#include<map>
#include<cmath>


class FileUtils
{


public:


    static void writeNumbers(std::pair<double, double>* data, int dataLen, const std::string& filePath)
    {
        std::ofstream ofs(filePath);
        for(int i=0;i<dataLen; i++)
        {
            std::pair<double, double> p = data[i];
            if(std::isnan(p.first))
            {
                ofs<< std::fixed<< std::setprecision(13)<<"nan"<< ", "<<p.second<< "\n";
            }else
            {
                ofs<< std::fixed<< std::setprecision(13)<<p.first<< ", "<<p.second<< "\n";
            }
        }
        ofs.close();
    }

    static void writeNumbers(double* data, int dataLen, const std::string& filePath)
    {
        std::ofstream ofs(filePath);
        for(int i=0;i<dataLen; i++)
        {
            ofs<< std::fixed<< std::setprecision(13)<< data[i]<< "\n";
        }
        ofs.close();
    }

    static void writeNumbers(float* data, int dataLen, const std::string& filePath)
    {
        std::ofstream ofs(filePath);
        for(int i=0;i<dataLen; i++)
        {
            ofs<< std::fixed<< std::setprecision(10)<< data[i]<< "\n";
        }
        ofs.close();
    }


    static void writeBytes(double* data, int dataLen, const std::string& filePath)
    {
        std::ofstream ofs(filePath);
        int size = dataLen * sizeof(double);
        char* ptr = reinterpret_cast<char*>(data);
        ofs.write(ptr, size);
        ofs.close();
    }

    static void writeBytes(float* data, int dataLen, const std::string& filePath)
    {
        std::ofstream ofs(filePath);
        int size = dataLen * sizeof(float);
        char* ptr = reinterpret_cast<char*>(data);
        ofs.write(ptr, size);
        ofs.close();
    }


    static std::string createPath(const std::string& rootPath, int index, const std::string& suffix = "")
    {
        std::string filePath = rootPath + std::to_string(index) + suffix+ ".txt";
        return filePath;
    }

    


};