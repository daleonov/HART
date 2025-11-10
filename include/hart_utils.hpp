#pragma once

#include <algorithm>  // min(), max()
#include <cctype>  // isalpha()
#include <cmath>  // pow()
#include <string>

namespace hart
{

template <typename NumericType>
NumericType clamp (const NumericType& value, const NumericType& low, const NumericType& high)
{
    return std::min<NumericType> (std::max<NumericType> (value, low), high);
}

template <typename SampleType>
inline static SampleType decibelsToRatio (SampleType valueDb)
{
	return std::pow ((SampleType) 10, valueDb / ((SampleType) 20));
}

bool isAbsolutePath (const std::string& path)
{
    if (path.empty())
    	return false;

    if (path[0] == '/' || path[0] == '\\')
    	return true;

	#ifdef _WIN32
    if (path.size() > 1 && std::isalpha (path[0]) && path[1] == ':')
    	return true;
	#endif

    return false;
}

std::string toAbsolutePath (const std::string& path)
{
    if (isAbsolutePath(path))
    	return path;

    return CLIConfig::get().getDataRootPath() + '/' + path;
}

}  // namespace hart
