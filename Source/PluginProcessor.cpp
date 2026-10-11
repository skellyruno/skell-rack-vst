#include "PluginProcessor.h"
#include "PluginEditor.h"

ModularFXAudioProcessor::ModularFXAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    revMixParam = apvts.getRawParameterValue("REV_MIX");
    gateThreshParam = apvts.getRawParameterValue("GATE_THRESH");
    choDepthParam = apvts.getRawParameterValue("CHO_DEPTH");
    choSpeedParam = apvts.getRawParameterValue("CHO_SPEED");

    moduleOrder[0].store(ID_Reverser);
    moduleOrder[1].store(ID_NoiseGate);
    moduleOrder[2].store(ID_Chorus);
    moduleCount.store(3);
}

bool ModularFXAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto mainInput = layouts.getMainInputChannelSet();
    auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainInput == mainOutput;
}

juce::AudioProcessorValueTreeState::ParameterLayout ModularFXAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("REV_MIX", "Reverser Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("REV_TIME", "Reverser Time", 0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GATE_THRESH", "Gate Threshold", -60.0f, 0.0f, -30.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GATE_ATTACK", "Gate Attack", 1.0f, 100.0f, 37.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("GATE_RELEASE", "Gate Release", 10.0f, 1000.0f, 200.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("CHO_DEPTH", "Chorus Depth", 0.0f, 1.0f, 0.6f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("CHO_AMOUNT", "Chorus Amount", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("CHO_SPEED", "Chorus Speed", 0.1f, 5.0f, 1.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("CHO_BASS", "Chorus Bass", -12.0f, 12.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("CHO_TREBLE", "Chorus Treble", -12.0f, 12.0f, 0.0f));

    return { params.begin(), params.end() };
}

void ModularFXAudioProcessor::updateModuleOrder(const std::vector<int>& newOrder)
{
    int count = std::min(static_cast<int>(newOrder.size()), 3);
    for (int i = 0; i < count; ++i)
    {
        moduleOrder[i].store(newOrder[i], std::memory_order_relaxed);
    }
    moduleCount.store(count, std::memory_order_release);
}

std::vector<int> ModularFXAudioProcessor::getModuleOrder() const
{
    std::vector<int> currentOrder;
    int count = moduleCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i)
    {
        currentOrder.push_back(moduleOrder[i].load(std::memory_order_relaxed));
    }
    return currentOrder;
}

void ModularFXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverser.prepareToPlay(sampleRate, samplesPerBlock);
    noiseGate.prepareToPlay(sampleRate, samplesPerBlock);
    chorus.prepareToPlay(sampleRate, samplesPerBlock);
}

void ModularFXAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    float mix = revMixParam ? revMixParam->load() : 0.0f;
    float thresh = gateThreshParam ? gateThreshParam->load() : -60.0f;
    float depth = choDepthParam ? choDepthParam->load() : 0.0f;
    float speed = choSpeedParam ? choSpeedParam->load() : 1.0f;

    int count = moduleCount.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i)
    {
        int modId = moduleOrder[i].load(std::memory_order_relaxed);
        if (modId == ID_Reverser) reverser.processBlock(buffer, mix);
        else if (modId == ID_NoiseGate) noiseGate.processBlock(buffer, thresh);
        else if (modId == ID_Chorus) chorus.processBlock(buffer, depth, speed);
    }
}

juce::AudioProcessorEditor* ModularFXAudioProcessor::createEditor()
{
    return new ModularFXAudioProcessorEditor(*this);
}

void ModularFXAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
        copyXmlToBinary(*xml, destData);
}

void ModularFXAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0) return;

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ModularFXAudioProcessor();
}
