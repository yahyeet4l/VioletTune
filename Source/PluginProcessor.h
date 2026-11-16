#pragma once
#include <JuceHeader.h>

namespace Scales
{
    inline const char* const names[8] = { "Chromatic", "Major", "Minor", "Harmonic Minor",
                                          "Pentatonic Maj", "Pentatonic Min", "Dorian", "Mixolydian" };
    inline constexpr bool masks[8][12] = {
        { 1,1,1,1,1,1,1,1,1,1,1,1 },
        { 1,0,1,0,1,1,0,1,0,1,0,1 },
        { 1,0,1,1,0,1,0,1,1,0,1,0 },
        { 1,0,1,1,0,1,0,1,1,0,0,1 },
        { 1,0,1,0,1,0,0,1,0,1,0,0 },
        { 1,0,0,1,0,1,0,1,0,0,1,0 },
        { 1,0,1,1,0,1,0,1,0,1,1,0 },
        { 1,0,1,0,1,1,0,1,0,1,1,0 } };
    inline juce::StringArray list() { juce::StringArray s; for (auto* n : names) s.add (n); return s; }
}

// Pitch-synchronous two-head delay-line shifter. Window length follows the detected
// period (a whole number of periods), which keeps the two heads in phase -> far less
// chorus/comb artefacts than a fixed window, and gives the tight "hard tune" sound.
struct PitchShifter
{
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        buf.assign (1 << 15, 0.0f);
        mask = (int) buf.size() - 1;
        w = 0;
        targetWin = (float) (0.04 * sr);
        for (int h = 0; h < 2; ++h) { win[h] = targetWin; p[h] = 0.5f * (float) h; }
    }

    void setTargetWin (float samples)
    {
        targetWin = juce::jlimit ((float) (0.012 * sr), (float) (0.06 * sr), samples);
    }

    float process (float in, float ratio)
    {
        buf[(size_t) w] = in;
        float num = 0.0f, den = 1e-6f;
        for (int h = 0; h < 2; ++h)
        {
            float rp = (float) w - (p[h] * win[h] + 2.0f);
            if (rp < 0.0f) rp += (float) buf.size();
            int i0 = (int) rp;
            float fr = rp - (float) i0;
            float s = buf[(size_t) (i0 & mask)] * (1.0f - fr) + buf[(size_t) ((i0 + 1) & mask)] * fr;
            float g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p[h]);
            num += s * g; den += g;
        }
        for (int h = 0; h < 2; ++h)
        {
            p[h] += (1.0f - ratio) / win[h];
            if (p[h] >= 1.0f)     { p[h] -= 1.0f; win[h] = targetWin; }   // new window length only
            else if (p[h] < 0.0f) { p[h] += 1.0f; win[h] = targetWin; }   // when the head is silent
        }
        w = (w + 1) & mask;
        return num / den;
    }

    std::vector<float> buf;
    double sr = 44100.0;
    int mask = 0, w = 0;
    float win[2] { 1764.0f, 1764.0f }, p[2] { 0.0f, 0.5f }, targetWin = 1764.0f;
};

struct SimpleDelay
{
    void prepare (int maxSamples)
    {
        int s = 1; while (s < maxSamples + 4) s <<= 1;
        b.assign ((size_t) s, 0.0f); mask = s - 1; w = 0;
    }
    float process (float in, int d)
    {
        b[(size_t) w] = in;
        float o = b[(size_t) ((w - d) & mask)];
        w = (w + 1) & mask;
        return o;
    }
    std::vector<float> b; int mask = 0, w = 0;
};

class VioletTuneProcessor : public juce::AudioProcessor
{
public:
    VioletTuneProcessor();
    ~VioletTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VioletTune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // note heat maps (per pitch class, 0..1) read by the editor
    std::atomic<float> inHeat[12], outHeat[12];

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float detectPitch();
    void analyse();

    static constexpr int ringSize = 4096, decSize = 2048, hop = 256, yinW = 640;
    std::array<float, ringSize> ring {}, linear {};
    std::array<float, decSize> dec {};
    std::array<float, 1500> dBuf {};
    int ringW = 0, hopCount = 0;
    double sr = 44100.0;

    PitchShifter shifters[2], dblShift[2];
    SimpleDelay dblDelay[2];

    float correction = 0.0f, desired = 0.0f, coef = 1.0f;   // semitones
    int lastTarget = -1, pendingTarget = -1, pendingCount = 0, unvoicedHops = 0;
    float noteTime = 0.0f;

    std::atomic<float> *pSpeed = nullptr, *pAmount = nullptr, *pSustain = nullptr,
                       *pStab = nullptr, *pDouble = nullptr, *pKey = nullptr, *pScale = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VioletTuneProcessor)
};
