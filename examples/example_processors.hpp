#pragma once

#include <cmath>

namespace hart
{

namespace examples
{

class LinearStereoBooster
{
public:
    void process (const float* const* inputs, float* const* outputs, size_t numFrames)
    {
        for (size_t frame = 0; frame < numFrames; ++frame)
        {
            outputs[0][frame] = inputs[0][frame] * gainLinear;
            outputs[1][frame] = inputs[1][frame] * gainLinear;
        }
    }

    void setGainDb (float newGainDb)
    {
        gainLinear = std::pow (10.0f, newGainDb / 20.0f);
    }

    float getGainDb()
    {
        return 20.0f * std::log10 (gainLinear);
    }

private:
    float gainLinear = 1.0f;
};

}  // namespace examples

}  // namespace hart
