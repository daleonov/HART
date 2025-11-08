#pragma once

#include <cmath>  // pow()

namespace hart
{

template <typename SampleType>
inline static SampleType decibelsToRatio (SampleType valueDb)
{
	return std::pow ((SampleType) 10, valueDb / ((SampleType) 20));
}

}  // namespace hart
