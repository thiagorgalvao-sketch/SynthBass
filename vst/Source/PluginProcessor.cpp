// AURA Synth Bass (plugin) - Copyright (c) 2026 Thiago R. Galvão. Todos os direitos reservados. ID: AURA-SB-TRG-2026
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

namespace {
enum Kind { F, I, B, W, LW };
struct Def { const char* id; const char* name; float mn, mx, ex; Kind k; };
// mesma ordem de aura::P
const Def kDefs[aura::NP] = {
 {"oct","Oitava",-3,1,1,I},{"wave","Osc 1 Onda",0,2,1,W},{"pw1","Osc 1 Largura",0.05f,0.5f,1,F},{"lvl","Osc 1 Nivel",0,1,1,F},
 {"w2","Osc 2 Onda",0,2,1,W},{"pw2","Osc 2 Largura",0.05f,0.5f,1,F},{"l2","Osc 2 Nivel",0,1,1,F},{"d2","Osc 2 Detune (st)",-7,7,1,F},
 {"o2","Osc 2 Oitava",-2,3,1,I},{"sync","Osc 2 Sync",0,1,1,B},{"sub","Sub Nivel",0,1,1,F},{"sw","Sub Quadrada",0,1,1,B},{"nz","Ruido Nivel",0,1,1,F},
 {"cut","Filtro Cutoff",0,1,1,F},{"res","Filtro Ressonancia",0,1,1,F},{"fenv","Filtro Env Amt (oit)",0,4,1,F},{"kt","Filtro Teclado",0,1,1,F},
 {"drv","Filtro Drive",0,1,1,F},{"pol","Filtro Polos",1,4,1,I},{"glide","Glide (s)",0,0.4f,2,F},{"pa","Pitch Env Quant (st)",0,48,2,F},
 {"pd","Pitch Env Decay (s)",0.01f,1.5f,2,F},{"lr","LFO Rate (Hz)",0.1f,30,2,F},{"lwv","LFO Onda",0,1,1,LW},{"lp","LFO Afinacao (st)",0,12,2,F},
 {"lf","LFO Filtro (oit)",0,3,1,F},{"lw","LFO Largura",0,1,1,F},{"fa","Filtro Attack (s)",0.002f,1,3,F},{"fd","Filtro Decay (s)",0.02f,2,2,F},
 {"fs","Filtro Sustain",0,1,1,F},{"fr","Filtro Release (s)",0.02f,2,2,F},{"a","Volume Attack (s)",0.002f,1,3,F},{"d","Volume Decay (s)",0.02f,2,2,F},
 {"s","Volume Sustain",0,1,1,F},{"r","Volume Release (s)",0.02f,2,2,F},{"leg","Legato",0,1,1,B},{"vol","Volume",0,3,1,F},
 {"l3","Osc 3 Nivel",0,1,1,F},{"w3","Osc 3 Onda",0,2,1,W},{"o3","Osc 3 Oitava",-3,3,1,I},{"d3","Osc 3 Detune (st)",-7,7,1,F},
 {"k3","Osc 3 Segue Teclado",0,1,1,B},{"nc","Ruido Rosa",0,1,1,B},{"ovl","Overload",0,1,1,F},{"lsr","LFO Fonte (0 LFO, 1 Osc3, 2 Rand)",0,2,1,I},
 {"ec","Envelopes Lineares",0,1,1,B},{"dt","Delay Tempo (s)",0.02f,1,1,F},{"df","Delay Repeticoes",0,0.9f,1,F},{"dm","Delay Mix",0,1,1,F}};
}

AudioProcessorValueTreeState::ParameterLayout AuraProcessor::makeLayout() {
    AudioProcessorValueTreeState::ParameterLayout L;
    for (int i = 0; i < aura::NP; i++) {
        const Def& d = kDefs[i]; ParameterID pid(d.id, 1);
        switch (d.k) {
        case F: { NormalisableRange<float> r(d.mn, d.mx); r.skew = 1.0f / d.ex;
                  AudioParameterFloatAttributes at;
                  if (std::string(d.id) == "cut") at = at.withStringFromValueFunction([](float v, int) { return String((int)std::round(30 * std::pow(600.0, v))) + " Hz"; });
                  L.add(std::make_unique<AudioParameterFloat>(pid, d.name, r, aura::kFactory[0].v[i], at)); break; }
        case I: L.add(std::make_unique<AudioParameterInt>(pid, d.name, (int)d.mn, (int)d.mx, (int)aura::kFactory[0].v[i])); break;
        case B: L.add(std::make_unique<AudioParameterBool>(pid, d.name, aura::kFactory[0].v[i] > 0.5f)); break;
        case W: L.add(std::make_unique<AudioParameterChoice>(pid, d.name, StringArray{ "Triangular", "Serra", "Quadrada" }, (int)aura::kFactory[0].v[i])); break;
        case LW: L.add(std::make_unique<AudioParameterChoice>(pid, d.name, StringArray{ "Triangular", "Quadrada" }, (int)aura::kFactory[0].v[i])); break;
        }
    }
    L.add(std::make_unique<AudioParameterFloat>(ParameterID("velsens", 1), "Sensibilidade ao toque", NormalisableRange<float>(0, 1), 0.75f));
    L.add(std::make_unique<AudioParameterChoice>(ParameterID("veldest", 1), "Toque age em", StringArray{ "Volume e brilho", "So volume", "So brilho" }, 0));
    L.add(std::make_unique<AudioParameterChoice>(ParameterID("prio", 1), "Prioridade de nota", StringArray{ "Ultima nota", "Mais grave", "Mais aguda" }, 0));
    L.add(std::make_unique<AudioParameterChoice>(ParameterID("atdest", 1), "Pressao de canal", StringArray{ "Desligada", "Vibrato", "Filtro", "Volume" }, 0));
    return L;
}

AuraProcessor::AuraProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "AURA", makeLayout()) {
    for (int i = 0; i < aura::NP; i++) raw[i] = apvts.getRawParameterValue(kDefs[i].id);
    pVelSens = apvts.getRawParameterValue("velsens"); pVelDest = apvts.getRawParameterValue("veldest");
    pPrio = apvts.getRawParameterValue("prio"); pAtDest = apvts.getRawParameterValue("atdest");
    engine.prepare(44100);
}

void AuraProcessor::prepareToPlay(double sr, int) {
    for (int i = 0; i < aura::NP; i++) engine.p[i] = raw[i]->load();
    engine.prepare(sr); lastSr = sr;
}

void AuraProcessor::setCurrentProgram(int idx) {
    if (idx < 0 || idx >= aura::kNumFactory) return;
    program = idx;
    for (int i = 0; i < aura::NP; i++) {
        if (auto* p = apvts.getParameter(kDefs[i].id)) {
            float v = aura::kFactory[idx].v[i];
            p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture();
        }
    }
}

void AuraProcessor::processBlock(AudioBuffer<float>& buf, MidiBuffer& midi) {
    ScopedNoDenormals nd;
    const int n = buf.getNumSamples(), nch = buf.getNumChannels();
    for (int i = 0; i < aura::NP; i++) engine.p[i] = raw[i]->load();
    engine.velSens = pVelSens->load(); engine.velDest = (int)pVelDest->load(); engine.prio = (int)pPrio->load(); engine.atDest = (int)pAtDest->load();
    if (buf.getNumChannels() < 1) return;
    float* out = buf.getWritePointer(0);
    int pos = 0;
    auto render = [&](int upto) { if (upto > pos) { engine.process(out + pos, upto - pos); pos = upto; } };
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        render(meta.samplePosition);
        if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
        else if (m.isPitchWheel()) engine.bend = jlimit(-1.0f, 1.0f, (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isChannelPressure()) engine.atVal = m.getChannelPressureValue() / 127.0f;
        else if (m.isController()) {
            const int c = m.getControllerNumber(), v = m.getControllerValue();
            if (c == 1) engine.mod = v / 127.0f;
            else if (c == 64) engine.setSustain(v >= 64);
            else if (c == 123 || c == 120) engine.allOff();
        } else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allOff();
    }
    render(n);
    for (int c = 1; c < nch; c++) buf.copyFrom(c, 0, buf, 0, 0, n);
}

AudioProcessorEditor* AuraProcessor::createEditor() { return new AuraEditor(*this); }

void AuraProcessor::getStateInformation(MemoryBlock& dest) {
    auto st = apvts.copyState(); st.setProperty("program", program, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}
void AuraProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size)) {
        auto st = ValueTree::fromXml(*xml);
        if (st.isValid()) { program = (int)st.getProperty("program", 0); apvts.replaceState(st); }
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
