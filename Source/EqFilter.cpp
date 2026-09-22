#include "EqFilter.h"

EqFilter::EqFilter(Type type, float freqHz, float gainDb, float Q)
    : m_type(type), m_freqHz(freqHz), m_gainDb(gainDb), m_Q(Q)     
{

}

void EqFilter::updateCoefficients()
{
    using C = juce::dsp::IIR::Coefficients<double>;

    switch (m_type)
    {
        case Type::Peak:
            m_coefficients = C::makePeakFilter(m_sampleRate, m_freqHz, m_Q,
                                               juce::Decibels::decibelsToGain(m_gainDb));
            break;
        case Type::LowShelf:
            m_coefficients = C::makeLowShelf(m_sampleRate, m_freqHz, m_Q,
                                             juce::Decibels::decibelsToGain(m_gainDb));
            break;
        case Type::HighShelf:
            m_coefficients = C::makeHighShelf(m_sampleRate, m_freqHz, m_Q,
                                              juce::Decibels::decibelsToGain(m_gainDb));
            break;
        case Type::LowPass:
            m_coefficients = C::makeLowPass(m_sampleRate, m_freqHz, m_Q);
            break;
        case Type::HighPass:
            m_coefficients = C::makeHighPass(m_sampleRate, m_freqHz, m_Q);
            break;
        case Type::BandPass:
            m_coefficients = C::makeBandPass(m_sampleRate, m_freqHz, m_Q);
            break;
        case Type::Notch:
            m_coefficients = C::makeNotch(m_sampleRate, m_freqHz, m_Q);
            break;
        case Type::AllPass:
            m_coefficients = C::makeAllPass(m_sampleRate, m_freqHz, m_Q);
            break;
    }
  
    for (auto& filter : m_filters)
    {
        filter.coefficients = m_coefficients;
        filter.reset();
    }
}

void EqFilter::prepare(double sampleRate, int numOfCh)
{
    m_sampleRate = sampleRate;
    m_numOfCh = numOfCh;

    if (m_filters.size() != m_numOfCh)
    {
        m_filters.clear();
        m_filters.resize(m_numOfCh);
    }

    updateCoefficients();   
}

void EqFilter::setSampleRate(double sampleRate)
{
    if (juce::approximatelyEqual(m_sampleRate, sampleRate))
        return;                       

    m_sampleRate = sampleRate;

    updateCoefficients();
}

void EqFilter::setFreq(float freqHz)
{
    if (juce::approximatelyEqual(m_freqHz, freqHz))
        return;    

    m_freqHz = freqHz;
    updateCoefficients();
}

void EqFilter::setQ(float Q)
{
    if (juce::approximatelyEqual(m_Q, Q))
        return;    

    m_Q = Q;
    updateCoefficients();
}