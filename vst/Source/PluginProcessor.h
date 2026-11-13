// AURA Synth Bass (plugin) - Copyright (c) 2026 Thiago R. Galvão. Todos os direitos reservados. ID: AURA-SB-TRG-2026
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AuraEngine.h"
#include "Presets.h"

class AuraProcessor : public juce::AudioProcessor {
public:
    AuraProcessor();
    ~AuraProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "AURA Synth Bass"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return aura::kNumFactory; }
    int getCurrentProgram() override { return program; }
    void setCurrentProgram(int i) override;
    const juce::String getProgramName(int i) override { return (i >= 0 && i < aura::kNumFactory) ? juce::String(aura::kFactory[i].name) : juce::String(); }
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    aura::Engine engine;
    std::atomic<float>* raw[aura::NP] = {};
    std::atomic<float> *pVelSens = nullptr, *pVelDest = nullptr, *pPrio = nullptr, *pAtDest = nullptr;
    int program = 0;
    double lastSr = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AuraProcessor)
};
