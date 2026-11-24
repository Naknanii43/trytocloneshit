#pragma once
#include "Rack.h"
#include "Params.h"
#include "Presets.h"
#include <unordered_map>
#include <string>
#include <juce_audio_processors/juce_audio_processors.h>

class ReedstormProcessor : public juce::AudioProcessor
{
public:
    ReedstormProcessor();
    ~ReedstormProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Reedstorm"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return (int) rs::factoryPresets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadPreset (int index);
    int getCurrentPresetIndex() const { return currentPreset; }

    juce::AudioProcessorValueTreeState apvts;
    rs::RackMeters& meters() { return rack.meters; }

private:
    rs::RackParams snapshot() const;
    float get (const char* id) const;

    rs::Rack rack;
    std::unordered_map<std::string, std::atomic<float>*> raw;
    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReedstormProcessor)
};
