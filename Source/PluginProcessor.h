/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "EqFilter.h"

//==============================================================================
/**
*/
class TanhCompAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    TanhCompAudioProcessor();
    ~TanhCompAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::AudioProcessorValueTreeState apvts;

    std::vector<std::unique_ptr<juce::dsp::Oversampling<float>>> oversamplers;

    size_t prevOsIndex;

  
    EqFilter hpf1  { EqFilter::Type::HighPass, 1.0f, 0.0f, 1.0f / sqrtf(2.0f)};
    EqFilter hpf2  { EqFilter::Type::HighPass, 1.0f, 0.0f, 1.0f / sqrtf(2.0f)};
    EqFilter lpf1  { EqFilter::Type::LowPass, 1.0f, 0.0f, 1.0f / sqrtf(2.0f)};
    EqFilter lpf2  { EqFilter::Type::LowPass, 1.0f, 0.0f, 1.0f / sqrtf(2.0f)};

    std::vector<EqFilter*> allFilters{&hpf1, &hpf2, &lpf1, &lpf2};

    std::vector<float> envelopesT;
    std::vector<float> envelopesB;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TanhCompAudioProcessor)
};
