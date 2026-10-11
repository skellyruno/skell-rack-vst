#pragma once
#include <JuceHeader.h>

class ReverserDSP
{
public:
    ReverserDSP() { prepareToPlay(44100.0, 512); }

    void prepareToPlay(double sampleRate, int)
    {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        int bufSamples = static_cast<int>(currentSampleRate * 2.0);
        if (bufSamples <= 0) bufSamples = 88200;

        circularBuffer.setSize(2, bufSamples, false, true, true);
        circularBuffer.clear();
        writePos = 0;
    }

    void processBlock(juce::AudioBuffer<float>& buffer, float mix)
    {
        if (bypassed.load(std::memory_order_relaxed)) return;

        int numInputChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();
        int bufSize = circularBuffer.getNumSamples();

        if (numInputChannels == 0 || numSamples == 0 || bufSize <= 0) return;

        mix = juce::jlimit(0.0f, 1.0f, mix);
        int processChannels = std::min(numInputChannels, circularBuffer.getNumChannels());
        int grainSize = static_cast<int>(currentSampleRate * 0.25);
        if (grainSize <= 0) grainSize = 1000;

        for (int i = 0; i < numSamples; ++i)
        {
            int offsetWithinGrain = writePos % grainSize;
            int readPos = (writePos - offsetWithinGrain) + (grainSize - offsetWithinGrain - 1);
            readPos = ((readPos % bufSize) + bufSize) % bufSize;

            for (int ch = 0; ch < processChannels; ++ch)
            {
                float dry = buffer.getSample(ch, i);
                circularBuffer.setSample(ch, writePos, dry);
                float wet = circularBuffer.getSample(ch, readPos);
                buffer.setSample(ch, i, dry * (1.0f - mix) + wet * mix);
            }

            writePos = (writePos + 1) % bufSize;
        }
    }

    void setBypassed(bool bypass) { bypassed.store(bypass, std::memory_order_relaxed); }
    bool isBypassed() const { return bypassed.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> bypassed { false };
    juce::AudioBuffer<float> circularBuffer;
    int writePos = 0;
    double currentSampleRate = 44100.0;
};

class NoiseGateDSP
{
public:
    NoiseGateDSP() = default;
    void prepareToPlay(double, int) {}

    void processBlock(juce::AudioBuffer<float>& buffer, float threshDb)
    {
        if (bypassed.load(std::memory_order_relaxed)) return;

        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float threshLinear = juce::Decibels::decibelsToGain(threshDb);

        for (int i = 0; i < numSamples; ++i)
        {
            float level = std::abs(buffer.getSample(0, i));
            currentEnvelope = level > threshLinear ? level : currentEnvelope * 0.99f;
            float gain = currentEnvelope > threshLinear ? 1.0f : 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.setSample(ch, i, buffer.getSample(ch, i) * gain);
        }
    }

    void setBypassed(bool bypass) { bypassed.store(bypass, std::memory_order_relaxed); }
    bool isBypassed() const { return bypassed.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> bypassed { false };
    float currentEnvelope = 0.0f;
};

class ChorusDSP
{
public:
    ChorusDSP() { prepareToPlay(44100.0, 512); }

    void prepareToPlay(double sampleRate, int)
    {
        currentSampleRate = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        delayBuffer.setSize(2, static_cast<int>(currentSampleRate * 0.1), false, true, true);
        delayBuffer.clear();
        writePos = 0;
        lfoPhase = 0.0f;
    }

    void processBlock(juce::AudioBuffer<float>& buffer, float depth, float rate)
    {
        if (bypassed.load(std::memory_order_relaxed)) return;

        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();
        int bufSize = delayBuffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0 || bufSize <= 0) return;

        int processChannels = std::min(numChannels, delayBuffer.getNumChannels());
        float lfoInc = (rate * juce::MathConstants<float>::twoPi) / static_cast<float>(currentSampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            lfoPhase += lfoInc;
            if (lfoPhase >= juce::MathConstants<float>::twoPi) lfoPhase -= juce::MathConstants<float>::twoPi;

            float modDelay = (std::sin(lfoPhase) * 0.5f + 0.5f) * (depth * 0.02f) * static_cast<float>(currentSampleRate);
            float readPos = static_cast<float>(writePos) - modDelay;
            if (readPos < 0.0f) readPos += static_cast<float>(bufSize);

            int iRead = static_cast<int>(readPos);
            float frac = readPos - static_cast<float>(iRead);
            int iReadNext = (iRead + 1) % bufSize;

            for (int ch = 0; ch < processChannels; ++ch)
            {
                float dry = buffer.getSample(ch, i);
                delayBuffer.setSample(ch, writePos, dry);

                float sample1 = delayBuffer.getSample(ch, iRead);
                float sample2 = delayBuffer.getSample(ch, iReadNext);
                float wet = sample1 + frac * (sample2 - sample1);

                buffer.setSample(ch, i, dry * 0.6f + wet * 0.4f);
            }

            writePos = (writePos + 1) % bufSize;
        }
    }

    void setBypassed(bool bypass) { bypassed.store(bypass, std::memory_order_relaxed); }
    bool isBypassed() const { return bypassed.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> bypassed { false };
    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    float lfoPhase = 0.0f;
    double currentSampleRate = 44100.0;
};
