#pragma once

#include "../dependencies/choc/platform/choc_DisableAllWarnings.h"
#include "../dependencies/signalsmith/plot.h"
#include "../dependencies/choc/platform/choc_ReenableAllWarnings.h"

#include "hart_audio_buffer.hpp"

namespace hart {

/// @brief Plots audio buffers as an svg file
/// @private
template <typename SampleType>
void plotData (const AudioBuffer<SampleType>& input, const AudioBuffer<SampleType>& output, double sampleRateHz, const std::string& plotFilePath)
{
    const double bufferSizeSeconds = static_cast <double> (output.getNumFrames()) / sampleRateHz;
    const double timeIncrementSeconds = 1.0 / sampleRateHz;

    signalsmith::plot::Figure figure;
    auto& inputSignalPlot = figure (0, 0).plot (1200, 200);
    auto& outputSignalPlot = figure (0, 1).plot (1200, 200);

    inputSignalPlot.y.major (0).label ("Value");
    outputSignalPlot.y.major (0).label ("Value");
    inputSignalPlot.x.major (0);
    outputSignalPlot.x.major (0).label("Time (s)");
    inputSignalPlot.title ("Input audio");
    outputSignalPlot.title ("Output audio");

    for (double t = 0.1; t < bufferSizeSeconds; t += 0.1)
    {
        inputSignalPlot.x.minor (t);
        outputSignalPlot.x.minor (t);
    }

    for (double t = 1.0; t < bufferSizeSeconds; t += 1.0)
    {
        inputSignalPlot.x.major (t);
        outputSignalPlot.x.major (t);
    }

    double timeSeconds = 0.0;

    for (size_t channel = 0; channel < input.getNumChannels(); ++channel)
    {
        auto &inputSignalLine = inputSignalPlot.line();

        for (size_t frame = 0; frame < input.getNumFrames(); ++frame)
        {
            inputSignalLine.add (timeSeconds, input[channel][frame]);
            timeSeconds += timeIncrementSeconds;
        }
    }

    timeSeconds = 0.0;

    for (size_t channel = 0; channel < output.getNumChannels(); ++channel)
    {
        auto &outputSignalLine = outputSignalPlot.line();

        for (size_t frame = 0; frame < output.getNumFrames(); ++frame)
        {
            outputSignalLine.add (timeSeconds, output[channel][frame]);
            timeSeconds += timeIncrementSeconds;
        }
    }

    figure.write (plotFilePath);
}

}  // namespace hart
