#include "PluginProcessor.h"
#include "PluginEditor.h"

PodProcessor::PodProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    modeParam   = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode"));
    mixParam    = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter ("mix"));
    bypassParam = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter ("bypass"));
}

juce::AudioProcessorValueTreeState::ParameterLayout PodProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray names;
    for (auto& m : podModes)
        names.add (m.name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", names, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat>  (juce::ParameterID { "mix", 1 }, "Mix",
                                                              juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

bool PodProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void PodProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };

    hp.prepare (spec);  lpA.prepare (spec);  lpB.prepare (spec);
    lowShelf.prepare (spec);  peak.prepare (spec);  highShelf.prepare (spec);
    reverb.prepare (spec);

    hp.setType  (juce::dsp::StateVariableTPTFilterType::highpass);
    lpA.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lpB.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    hp.reset();  lpA.reset();  lpB.reset();
    lowShelf.reset();  peak.reset();  highShelf.reset();
    reverb.reset();

    dry.setSize (2, (int) spec.maximumBlockSize);
    lastMode = -1;
}

void PodProcessor::updateFilters (int idx)
{
    const auto& m = podModes[(size_t) idx];
    const float nyq = 0.45f * (float) sr;

    hp.setCutoffFrequency (juce::jmin (m.hp, nyq));
    lpA.setCutoffFrequency (juce::jmin (m.lp, nyq));
    lpB.setCutoffFrequency (m.steep ? juce::jmin (m.lp, nyq) : nyq);

    *lowShelf.state  = *Coeffs::makeLowShelf  (sr, m.lowF,  0.707f, juce::Decibels::decibelsToGain (m.lowDb));
    *peak.state      = *Coeffs::makePeakFilter (sr, m.peakF, m.peakQ, juce::Decibels::decibelsToGain (m.peakDb));
    *highShelf.state = *Coeffs::makeHighShelf (sr, juce::jmin (m.highF, nyq), 0.707f, juce::Decibels::decibelsToGain (m.highDb));
}

void PodProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n == 0 || bypassParam->get())
        return;

    const int modeIdx = juce::jlimit (0, (int) podModes.size() - 1, modeParam->getIndex());
    if (modeIdx != lastMode)
    {
        updateFilters (modeIdx);
        lastMode = modeIdx;
    }
    const auto& m = podModes[(size_t) modeIdx];
    const float mix = mixParam->get();

    if (dry.getNumSamples() < n)
        dry.setSize (2, n, false, false, true);
    for (int ch = 0; ch < 2; ++ch)
        dry.copyFrom (ch, 0, buffer, ch, 0, n);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    hp.process (ctx);
    lpA.process (ctx);
    lpB.process (ctx);
    lowShelf.process (ctx);
    peak.process (ctx);
    highShelf.process (ctx);

    if (m.reverb > 0.0f)
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = 0.5f;
        p.damping = 0.7f;
        p.wetLevel = m.reverb;
        p.dryLevel = 1.0f;
        p.width = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
        reverb.process (ctx);
    }

    // szélesség + telítés + szint
    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);
    const float gain = juce::Decibels::decibelsToGain (m.gainDb);
    const bool sat = m.drive > 1.001f;

    for (int i = 0; i < n; ++i)
    {
        const float mid  = 0.5f * (l[i] + r[i]);
        const float side = 0.5f * (l[i] - r[i]) * m.width;
        float L = mid + side;
        float R = mid - side;

        if (sat)
        {
            L = std::tanh (m.drive * L) / m.drive;
            R = std::tanh (m.drive * R) / m.drive;
        }
        l[i] = L * gain;
        r[i] = R * gain;
    }

    // dry/wet
    for (int ch = 0; ch < 2; ++ch)
    {
        buffer.applyGain (ch, 0, n, mix);
        buffer.addFrom (ch, 0, dry, ch, 0, n, 1.0f - mix);
    }
}

juce::AudioProcessorEditor* PodProcessor::createEditor()
{
    return new PodEditor (*this);
}

void PodProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PodProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PodProcessor();
}
