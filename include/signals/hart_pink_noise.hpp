#pragma once

#include <algorithm>  // fill()
#include <array>
#include <random>
#include <vector>

#include "hart_cliconfig.hpp"
#include "signals/hart_signal.hpp"
#include "hart_utils.hpp"  // makeRandomSeeds()

namespace hart
{

/// @brief Produces deterministic pink noise
/// @details Calibrated to -12 dB RMS, with sample peaks being around unity gain.
///
/// This Signal is constructed in time domain, with high CPU efficiency, and
/// quite accurate, but not perfect, frequency response. You may also use
/// `Spectrum::colouredNoise()`, combined with `Spectrum::toSignal()`, to get a
/// finite signal source with ideal frequency response, if you want the highest
/// frequency responce accuracy, and don't mind its finite nature, and substantially
/// higher CPU cost.
///
/// This Signal is implemented around Paul Kellet's pinking filter, described in a
/// [DSP generation of Pink (1/f) Noise](https://www.firstpr.com.au/dsp/pink-noise/)
/// article. The general idea is a cascade of one-pole filters, hand-tuned to
/// have a frequency response within a claimed +/- 0.05 dB accuracy, and feeding
/// white noise through it, to achieve a surprisingly accurate pink noise signal.
///
/// This class is a pretty faithful implementation of Paul Kellet's "instrumentation
/// grade" version of the filter, with some extra calibration and measurements to make
/// it fit HART standards. Below listed are some measurements of this Signal's output.
/// You can see more details about this specific Signal implementation in my design
/// notes in the `Scripts/PinkNoise Filter.ipynb` notebook, shipped with this repo.
///
/// Contains energy down to DC frequency, and has a pretty accurate frequency response
/// above 9.2 Hz at 44.1 kHz, according to the author of the original filter. However,
/// it was designed for 44.1 kHz sample rate, and while it's still very accurate at
/// other sample rates, it's best to use it only up to 96 kHz included, as the lower
/// end frequency response at very high sample rates (such as 192 kHz) is no longer
/// as accurate in the audible frequency range (see the chart below).
///
/// **Maximum spectral-shape error above 20 Hz after optimal
/// constant gain alignment at common sample rates:**
///
/// Sample rate, Hz | Max error, dB
/// --------------- | --------------
///           44100 |         0.029
///           48000 |         0.029
///           88200 |         0.030
///           96000 |         0.035
///          176400 |         0.460
///          192000 |         0.562
///
/// **Output levels**
///
/// Output RMS calibrated to -12 dB. Measured output sample peaks
/// (see `PinkNoise Filter.ipynb` for details): median: -2.37 dB, p95: -0.23 dB.
///
/// For multi-channel set-ups, each channel will produce a unique noise instance.
///
/// @warning It is not recommended to use it with very high sample rates, such
/// as 192 kHz or higher - see the descritpion for details. However, it's still
/// usable at those very high rates, with a caveat of less accurate response at
/// the lower end of audible frequency range.
/// @tparam SampleType 
/// @ingroup Signals
template <typename SampleType>
class PinkNoise :
    public Signal<SampleType, PinkNoise<SampleType>>
{
public:

    /// @brief Constructs a PinkNoise Signal instance
    /// @param randomSeed RNG seed value for generating noise. Per-channel RNG seeds will
    /// be derived from this value. Defaults to global seed value, controlled via CLI.
    PinkNoise (uint_fast32_t randomSeed = CLIConfig::getInstance().getRandomSeed()) :
        m_randomSeed (randomSeed)
    {
    }

    PinkNoise (const PinkNoise& other) :
        m_randomSeed (other.m_randomSeed)
    {
        m_channelRngs.resize (other.m_channelRngs.size());
        m_channelStates.resize (other.m_channelStates.size());

        for (size_t channel = 0; channel < m_channelRngs.size(); ++channel)
            m_channelRngs[channel] = other.m_channelRngs[channel];

        for (size_t channel = 0; channel < m_channelStates.size(); ++channel)
            m_channelStates[channel] = other.m_channelStates[channel];
    }

    PinkNoise (PinkNoise&& other) :
        m_randomSeed (other.m_randomSeed),
        m_channelRngs (std::move (other.m_channelRngs)),
        m_channelStates (std::move (other.m_channelStates))
    {
    }

    PinkNoise& operator= (const PinkNoise& other) noexcept
    {
        if (this == &other)
            return *this;

        m_randomSeed = other.m_randomSeed;
        m_channelRngs.resize (other.m_channelRngs.size());
        m_channelStates.resize (other.m_channelStates.size());

        for (size_t channel = 0; channel < m_channelRngs.size(); ++channel)
            m_channelRngs[channel] = other.m_channelRngs[channel];

        for (size_t channel = 0; channel < m_channelStates.size(); ++channel)
            m_channelStates[channel] = other.m_channelStates[channel];

        return *this;
    }

    PinkNoise& operator= (PinkNoise&& other) noexcept
    {
        if (this == &other)
            return *this;

        m_randomSeed = other.m_randomSeed;
        m_channelRngs = std::move (other.m_channelRngs);
        m_channelStates = std::move (other.m_channelStates);

        return *this;
    }

    ~PinkNoise() = default;

    void prepare (double /* sampleRateHz */, size_t numOutputChannels, size_t /* maxBlockSizeFrames */) override
    {
        hassert (m_channelRngs.size() == m_channelStates.size());
        hassert (m_channelRngs.size() == m_channelRandomSeeds.size());
        const size_t previousNumOutputChannels = m_channelRngs.size();

        if (numOutputChannels == previousNumOutputChannels)
            return;

        m_channelRandomSeeds = makeRandomSeeds<uint_fast32_t> (numOutputChannels, m_randomSeed);
        hassert (m_channelRandomSeeds.size() == numOutputChannels);

        m_channelRngs.resize (numOutputChannels);
        resetRngs();

        m_channelStates.resize (numOutputChannels);
        resetChannelStates();

        warmUp();
    }

    void renderNextBlock (AudioBuffer<SampleType>& output) override
    {
        const size_t numChannels = output.getNumChannels();
        const size_t numFrames = output.getNumFrames();

        if (m_channelStates.size() != numChannels || m_channelStates.size() != m_channelRngs.size())
        {
            // prepare() has not been called before calling process()?
            hassertfalse;
            return;
        }

        constexpr SampleType whiteNoisePeakLinear = (SampleType) 0.142528277179462;  // To have output RMS at about -12 dB
        std::uniform_real_distribution<SampleType> distribution {-whiteNoisePeakLinear, whiteNoisePeakLinear};

        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            SampleType* outputChannel = output[channel];
            ChannelState& bs = m_channelStates[channel];
            std::mt19937& channelRng = m_channelRngs[channel];

            for (size_t frame = 0; frame < numFrames; ++frame)
            {
                const SampleType white = distribution (channelRng);
                bs[0] = SampleType (0.99886) * bs[0] + white * SampleType (0.0555179);
                bs[1] = SampleType (0.99332) * bs[1] + white * SampleType (0.0750759);
                bs[2] = SampleType (0.96900) * bs[2] + white * SampleType (0.1538520);
                bs[3] = SampleType (0.86650) * bs[3] + white * SampleType (0.3104856);
                bs[4] = SampleType (0.55000) * bs[4] + white * SampleType (0.5329522);
                bs[5] = SampleType (-0.7616) * bs[5] - white * SampleType (0.0168980);
                outputChannel[frame] = bs[0] + bs[1] + bs[2] + bs[3] + bs[4] + bs[5] + bs[6] + white * SampleType (0.5362);
                bs[6] = white * SampleType (0.115926);
            }
        }
    }

    void reset() override
    {
        resetRngs();
        resetChannelStates();
        warmUp();
    }

    void represent (std::ostream& stream) const override
    {
        stream << "PinkNoise (" << m_randomSeed << ')';
    }

private:
    uint_fast32_t m_randomSeed;
    std::vector<uint_fast32_t> m_channelRandomSeeds;
    std::vector<std::mt19937> m_channelRngs;

    using ChannelState = std::array<SampleType, 7>;
    std::vector<ChannelState> m_channelStates;

    void resetChannelStates()
    {
        for (ChannelState& bs : m_channelStates)
            std::fill (bs.begin(), bs.end(), (SampleType) 0);
    }

    void resetRngs()
    {
        hassert (m_channelRngs.size() == m_channelRandomSeeds.size());

        for (size_t channel = 0; channel < m_channelRngs.size(); ++channel)
            m_channelRngs[channel].seed (m_channelRandomSeeds[channel]);
    }

    void warmUp()
    {
        // Ideally, we don't want to start generating test signal
        // with filters in their zeroed states, so we'll fast forward
        // them a bit, specifically ~10 tau's of a slowest pole.
        // See "scripts\PinkNoise Signal.ipynb" for details.

        constexpr size_t warmUpDurationFrames = 8683;
        const size_t numChannels = m_channelStates.size();
        AudioBuffer<SampleType> dummyBuffer (numChannels, warmUpDurationFrames);

        renderNextBlock (dummyBuffer);
    }
};

HART_SIGNAL_DECLARE_ALIASES_FOR (PinkNoise)

}  // namespace hart
 