// AURA Synth Bass (plugin) - Copyright (c) 2026 Thiago R. Galvão. Todos os direitos reservados.
#pragma once
#include "PluginProcessor.h"

class AuraEditor : public juce::AudioProcessorEditor {
public:
    explicit AuraEditor(AuraProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    AuraProcessor& proc;
    juce::ComboBox presets;
    juce::Label title;
    juce::GenericAudioProcessorEditor generic;
};
