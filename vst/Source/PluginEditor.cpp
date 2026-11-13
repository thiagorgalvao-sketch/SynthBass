#include "PluginEditor.h"

AuraEditor::AuraEditor(AuraProcessor& p) : AudioProcessorEditor(&p), proc(p), generic(p) {
    title.setText("AURA SYNTH BASS  -  (c) 2026 Thiago R. Galvao", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    addAndMakeVisible(title);
    for (int i = 0; i < aura::kNumFactory; i++) presets.addItem(juce::String(i + 1) + "  " + aura::kFactory[i].name, i + 1);
    presets.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presets.onChange = [this] { proc.setCurrentProgram(presets.getSelectedId() - 1); };
    addAndMakeVisible(presets);
    addAndMakeVisible(generic);
    setResizable(true, true);
    setResizeLimits(480, 400, 1200, 1400);
    setSize(640, 760);
}
void AuraEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff10161c)); }
void AuraEditor::resized() {
    auto r = getLocalBounds().reduced(6);
    title.setBounds(r.removeFromTop(24));
    presets.setBounds(r.removeFromTop(28).reduced(0, 2));
    generic.setBounds(r);
}
