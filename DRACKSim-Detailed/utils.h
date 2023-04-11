#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>

#define KILO 1024
#define MEGA (KILO * KILO)
#define GIGA (KILO * MEGA)

extern std::string StringInt(uint64_t val, uint32_t width = 0, char padding = ' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed, std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << val;
    return ostr.str();
}

extern std::string StringFlt(long double val, uint32_t width = 0, char padding = ' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed, std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << val;
    return ostr.str();
}

extern std::string StringHex(uint64_t val, uint32_t width = 0, char padding = ' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed, std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << std::hex << "0x" << val;
    return ostr.str();
}

extern std::string StringString(std::string val, uint32_t width = 0, char padding = ' ')
{
    std::ostringstream ostr;
    ostr.setf(std::ios::fixed, std::ios::floatfield);
    ostr.fill(padding);
    ostr << std::setw(width) << val;
    return ostr.str();
}
