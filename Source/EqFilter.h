#pragma once

#include <JuceHeader.h>

class EqFilter
{
public:
    enum class Type
    {
        Peak,
        LowShelf,
        HighShelf,
        LowPass,
        HighPass,
        BandPass,
        Notch,
        AllPass
    };

    static float OctToQ(float bwOct) noexcept
    {
        return 1.0f / (2.0f * std::sinh(0.5f * std::log(2.0f) * bwOct));
    }   

    EqFilter(Type type, float freqHz, float gainDb, float Q);   
    void updateCoefficients();
    void prepare(double sampleRate, int numOfCh);
    void setSampleRate(double sampleRate);
    void setFreq(float freqHz);
    void setQ(float Q);

    juce::dsp::IIR::Filter<double>&       operator[](int ch)       { return m_filters[ch]; }
    const juce::dsp::IIR::Filter<double>& operator[](int ch) const { return m_filters[ch]; }

private:
    Type m_type;
    float m_freqHz;
    float m_gainDb;
    float m_Q;
    double m_sampleRate;

    int m_numOfCh;

    juce::dsp::IIR::Coefficients<double>::Ptr m_coefficients;
    std::vector<juce::dsp::IIR::Filter<double>> m_filters;


};