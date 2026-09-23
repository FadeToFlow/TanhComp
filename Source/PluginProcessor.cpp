/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TanhCompAudioProcessor::TanhCompAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts (*this, nullptr, "Parameters", createParameters())
#endif
{
}

TanhCompAudioProcessor::~TanhCompAudioProcessor()
{
}

//==============================================================================
const juce::String TanhCompAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TanhCompAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TanhCompAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TanhCompAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TanhCompAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TanhCompAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TanhCompAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TanhCompAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String TanhCompAudioProcessor::getProgramName (int index)
{
    return {};
}

void TanhCompAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void TanhCompAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = static_cast<size_t>(getTotalNumInputChannels());
    oversamplers.clear();
    for (size_t factor = 1; factor <= 4; ++factor)
    {
        auto os = std::make_unique<juce::dsp::Oversampling<float>>(
            numChannels,                                                          
            factor,                                                   
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, 
            true                                                        
        );
        
        os->initProcessing(static_cast<size_t>(samplesPerBlock));
        os->reset();
        
        oversamplers.push_back(std::move(os));
    }

    size_t osIndex = static_cast<size_t> (apvts.getRawParameterValue ("OS")->load());
    float osMultiplier = static_cast<float>(1 << osIndex); 
    float overSampleRate = static_cast<float>(getSampleRate()) * osMultiplier;
    prevOsIndex = osIndex;
    if (osIndex > 0)
    {
        const float latency = oversamplers[osIndex - 1]->getLatencyInSamples();
        setLatencySamples (juce::roundToInt (latency));
    }
    else
    {
        setLatencySamples (0);
    }       

    for (auto *filter : allFilters)
    {
        filter->prepare(overSampleRate, getTotalNumInputChannels());
    }

    envelopesB.resize(getTotalNumInputChannels());
    envelopesT.resize(getTotalNumInputChannels());
}

void TanhCompAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
 
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool TanhCompAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void TanhCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
   
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    size_t osIndex = static_cast<size_t> (apvts.getRawParameterValue ("OS")->load());   
    float osMultiplier = static_cast<float>(1 << osIndex); 
    float overSampleRate = static_cast<float>(getSampleRate()) * osMultiplier;

    size_t modeIndex = static_cast<size_t> (apvts.getRawParameterValue ("MODE")->load());
    bool isSymm = !modeIndex;

    float driveDb = apvts.getRawParameterValue("GAIN")->load();
    float driveGain = juce::Decibels::decibelsToGain(driveDb);

    // float lpFreq = apvts.getRawParameterValue("LPFREQ")->load();
    // eq1_f2.setFreq(lpFreq);
    // float lpQ = apvts.getRawParameterValue("LPQ")->load();
    // eq1_f2.setQ(lpQ);

    float bias = apvts.getRawParameterValue("BIAS")->load();

    float swspeed = apvts.getRawParameterValue("SWSPEED")->load();
    float speed = apvts.getRawParameterValue("SPEED")->load();
    hpf1.setFreq(swspeed);
    hpf2.setFreq(swspeed);
    lpf1.setFreq(speed);
    lpf2.setFreq(speed);

    float pressure = apvts.getRawParameterValue("PRESS")->load();

    float makeup = apvts.getRawParameterValue("MKUP")->load();

    juce::dsp::AudioBlock<float> mainBlock(buffer);
    juce::dsp::AudioBlock<float> blockToProcess = mainBlock; 

    if (prevOsIndex != osIndex)
    {
        prevOsIndex = osIndex;
        if (osIndex > 0)
        {
            const float latency = oversamplers[osIndex - 1]->getLatencyInSamples();
            setLatencySamples (juce::roundToInt (latency));
        }
        else
        {
            setLatencySamples (0);
        }  
        

        for (auto *filter : allFilters)
        {
            filter->setSampleRate(overSampleRate);
        }
    }
 
    if (osIndex > 0)
    {
        blockToProcess = oversamplers[osIndex - 1]->processSamplesUp(mainBlock);
    }

    for (size_t ch = 0; ch < blockToProcess.getNumChannels(); ++ch)
    {
        auto* data = blockToProcess.getChannelPointer(ch);
        
        for (size_t i = 0; i < blockToProcess.getNumSamples(); ++i)
        {
            float x = data[i] * driveGain;

            float xT = x > 0 ? x : 0;
            float xB = x < 0 ? x : 0;
            
            if (isSymm)
            {
                envelopesT[ch] = lpf1[ch].processSample(xT);
                envelopesB[ch] = lpf2[ch].processSample(xB);               

                xT = hpf1[ch].processSample(xT);
                xB = hpf2[ch].processSample(xB);

                xT += envelopesT[ch]*juce::Decibels::decibelsToGain(pressure-10);
                xB += envelopesB[ch]*juce::Decibels::decibelsToGain(pressure-10);

                xT += bias;
                xB -= bias;

                xT = tanh(xT);
                xB = tanh(xB);

                data[i] = (xB + xT) * juce::Decibels::decibelsToGain(makeup);
            }
            else
            {
                float rectifierX = xT - xB;
                envelopesT[ch] = lpf1[ch].processSample(rectifierX);
                envelopesB[ch] = hpf1[ch].processSample(envelopesT[ch]);

                x += envelopesT[ch]*juce::Decibels::decibelsToGain(pressure-10);
                x += envelopesB[ch];

                x += bias;

                x = tanh(x);

                x = dcBlock[ch].processSample(x);

                data[i] = x * juce::Decibels::decibelsToGain(makeup);
            }

        }       
    }

    if (osIndex > 0)
    {
        oversamplers[osIndex - 1]->processSamplesDown(mainBlock);
    }    
}

//==============================================================================
bool TanhCompAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* TanhCompAudioProcessor::createEditor()
{
    //return new TanhCompAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this); 
}

//==============================================================================
void TanhCompAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);    
}

void TanhCompAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TanhCompAudioProcessor();
}


juce::AudioProcessorValueTreeState::ParameterLayout TanhCompAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        
    juce::StringArray osChoices { "1x (Off)", "2x", "4x", "8x", "16x" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>("OS", "Oversampling", osChoices, 0));

    juce::StringArray modeChoices { "Symmetrical", "Asymmetrical"};
    params.push_back(std::make_unique<juce::AudioParameterChoice>("MODE", "Mode", modeChoices, 0));    

    params.push_back(std::make_unique<juce::AudioParameterFloat>("GAIN", "Gain (dB)", -10.0f, 30.0f, 16.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("BIAS", "Bias", 0.0f, 3.0f, 0.5f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("PRESS", "Pressure", 0.0f, 40.0f, 22.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>("SPEED", "Pressure Speed", 0.1f, 10.0f, 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("SWSPEED", "Swing Speed", 0.1f, 10.0f, 3.0f));
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>("MKUP", "Makeup (dB)", 0.0f, 40.0f, 6.0f));


    return { params.begin(), params.end() };
}
