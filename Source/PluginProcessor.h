#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "SynthVoice.h"

class NebulonProcessor : public juce::AudioProcessor
{
public:
    NebulonProcessor();
    ~NebulonProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nebulon"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    // oscilloscope feed (written on audio thread, read by the UI)
    static constexpr int scopeSize = 2048;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    void processDelay (juce::AudioBuffer<float>&);

    neb::SynthParams params;
    juce::Synthesiser synth;
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;

    juce::AudioBuffer<float> delayBuf;
    int delayWrite = 0;
    float delayLpState[2] {};
    juce::SmoothedValue<float> delaySamples;

    int currentProgram = 0;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulonProcessor)
};
