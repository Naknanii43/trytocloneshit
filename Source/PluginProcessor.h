#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "Engine.h"
#include "Arpeggiator.h"
#include "FxChain.h"
#include "PhrasePlayer.h"

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // --- presets
    juce::String getPresetCategory (int index) const;
    juce::File getUserPresetFolder() const;
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& file);
    juce::String currentPresetName, currentCategory { "Pads" };
    juce::String getPresetDescription() const;

    // Generates a new, playable random patch and gives it a name.
    void inspire();

    // --- samples
    bool loadUserSample (int layer, const juce::File& file);
    const neb::SampleData* getLayerSource (int layer) const;
    juce::String getUserSampleName (int layer) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    neb::Arpeggiator arp;
    neb::PhrasePlayer preview;

    // oscilloscope feed (written on audio thread, read by the UI)
    static constexpr int scopeSize = 2048;
    std::array<float, scopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    void resetToDefaults();
    void setParam (const juce::String& id, float value);

    neb::Shared shared;
    neb::NebSynth synth;
    neb::FxChain fx;
    juce::AudioFormatManager formatManager;
    std::vector<std::unique_ptr<neb::SampleData>> loadedSamples; // kept alive while the plugin lives
    juce::CriticalSection sampleLock;

    int currentProgram = 0;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulonProcessor)
};
