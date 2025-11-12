#pragma once

#include <memory>
#include <vector>

namespace hart
{

class Envelope
{
public:
    virtual ~Envelope() = default;
    virtual void renderNextBlock (size_t blockSize, std::vector<double>& valuesOutput) = 0;
    virtual void prepare (double sampleRateHz, size_t maxBlockSizeFrames) = 0;
    virtual void reset() = 0;
    virtual std::unique_ptr<Envelope> copy() const = 0;

    std::vector<double> renderNextBlock (size_t blockSize)
    {
        std::vector<double> values (blockSize);
        renderNextBlock (blockSize, values);
        return values;
    }
};

}  // namespace hart
