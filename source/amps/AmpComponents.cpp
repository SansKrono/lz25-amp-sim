#include "AmpComponents.h"

//================ ToneStack ==================
void ToneStack::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;

    trebleFilter.reset();
    midFilter.reset();
    bassFilter.reset();
    presenceFilter.reset();
    bottomEndFilter.reset();
    resonanceFilter.reset();

    trebleFilter.prepare (spec);
    midFilter.prepare (spec);
    bassFilter.prepare (spec);
    presenceFilter.prepare (spec);
    bottomEndFilter.prepare (spec);
    resonanceFilter.prepare (spec);

    // Defaults matching current processor chain
    setTrebleFilter (trebleCF, trebleQ);
    setMidFilter (midCF, midQ);
    setBassFilter (bassCF, bassQ);
    setPresenceFilter (presenceSF, presenceQ);
    setResonance (3000.0f);

    setTreble (trebleGain);
    setMid (midGain);
    setBass (bassGain);
    setPresence (presenceMult);
}

void ToneStack::processPre (juce::dsp::AudioBlock<float>& block)
{
    // Order: Resonance (ladder), BottomEnd EQ (low shelf)
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    resonanceFilter.process (ctx);
    bottomEndFilter.process (ctx);
}

void ToneStack::process (juce::dsp::AudioBlock<float>& block)
{
    // Post-distortion EQ order matching previous chain: Presence -> Bass -> Mid -> Treble
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    presenceFilter.process (ctx);
    bassFilter.process (ctx);
    midFilter.process (ctx);
    trebleFilter.process (ctx);
}

void ToneStack::reset ()
{
    trebleFilter.reset();
    midFilter.reset();
    bassFilter.reset();
    presenceFilter.reset();
    bottomEndFilter.reset();
    resonanceFilter.reset();
}

void ToneStack::setTreble (float gain)
{
    trebleGain = gain;
    trebleFilter.state = juce::dsp::IIR::Coefficients<float>::makePeakFilter ((double) spec.sampleRate, trebleCF, trebleQ, juce::jlimit (0.0f, 4.0f, gain));
}

void ToneStack::setMid (float gain)
{
    midGain = gain;
    midFilter.state = juce::dsp::IIR::Coefficients<float>::makePeakFilter ((double) spec.sampleRate, midCF, midQ, juce::jlimit (0.0f, 4.0f, gain));
}

void ToneStack::setBass (float gain)
{
    bassGain = gain;
    bassFilter.state = juce::dsp::IIR::Coefficients<float>::makePeakFilter ((double) spec.sampleRate, bassCF, bassQ, juce::jlimit (0.0f, 4.0f, gain));
    bottomEndFilter.state = juce::dsp::IIR::Coefficients<float>::makeLowShelf ((double) spec.sampleRate, bottomEndHz, bottomEndQ, juce::jlimit (0.0f, 4.0f, gain));
}

void ToneStack::setPresence (float mult)
{
    presenceMult = mult;
    const float db = juce::jlimit (-24.0f, 24.0f, presenceBoostDB * mult);
    presenceFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf ((double) spec.sampleRate, presenceSF, presenceQ, juce::Decibels::decibelsToGain (db));
}

void ToneStack::setResonance (float cutoff)
{
    resonanceFilter.setMode (juce::dsp::LadderFilterMode::LPF12);
    resonanceFilter.setResonance (0.2f);
    resonanceFilter.setCutoffFrequencyHz (cutoff);
}

void ToneStack::setTrebleFilter (float centerFreq, float q)
{
    trebleCF = centerFreq;
    trebleQ = q;
    setTreble (trebleGain);
}

void ToneStack::setMidFilter (float centerFreq, float q)
{
    midCF = centerFreq;
    midQ = q;
    setMid (midGain);
}

void ToneStack::setBassFilter (float centerFreq, float q)
{
    bassCF = centerFreq;
    bassQ = q;
    setBass (bassGain);
}

void ToneStack::setPresenceFilter (float shelfFreq, float q)
{
    presenceSF = shelfFreq;
    presenceQ = q;
    setPresence (presenceMult);
}

//================ PreDistortionFilter ==================
void PreDistortionFilter::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    hpf.reset();
    hpf.prepare (spec);
    setHighPassCutoff (cutoffHz);
}

void PreDistortionFilter::process (juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    hpf.process (ctx);
}

void PreDistortionFilter::reset ()
{
    hpf.reset();
}

void PreDistortionFilter::setHighPassCutoff (float hz)
{
    cutoffHz = hz;
    hpf.state = juce::dsp::IIR::Coefficients<float>::makeHighPass ((double) spec.sampleRate, juce::jmax (10.0f, hz));
}

//================ Waveshaper ==================
void Waveshaper::prepare (const juce::dsp::ProcessSpec& s)
{
    spec = s;
    preGain.reset();
    preGain.prepare (spec);
    shaper.reset();
    shaper.prepare (spec);
}

void Waveshaper::process (juce::dsp::AudioBlock<float>& block)
{
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    preGain.process (ctx);
    shaper.process (ctx);
}

void Waveshaper::reset ()
{
    preGain.reset();
    shaper.reset();
}

void Waveshaper::setFunction (WaveshaperFunction func)
{
    shaper.functionToUse = func;
}

void Waveshaper::setPreGain (float gainDb)
{
    preGain.setGainDecibels (gainDb);
}
