#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout VioletTuneProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;

    // Speed (ms). Knob travel: far left = slow (400 ms), 95% = 0 ms, last 5% = negative speed down to -3 ms.
    NormalisableRange<float> speedRange (-3.0f, 400.0f,
        [] (float, float, float n) {
            n = jlimit (0.0f, 1.0f, n);
            if (n <= 0.95f) { float x = 1.0f - n / 0.95f; return 400.0f * x * x; }
            return -3.0f * (n - 0.95f) / 0.05f; },
        [] (float, float, float v) {
            v = jlimit (-3.0f, 400.0f, v);
            if (v >= 0.0f) return 0.95f * (1.0f - std::sqrt (v / 400.0f));
            return 0.95f + 0.05f * (-v / 3.0f); },
        [] (float, float, float v) { return jlimit (-3.0f, 400.0f, v); });

    auto msAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return (std::abs (v) < 10.0f ? String (v, 1) : String (roundToInt (v))) + " ms"; });
    auto pctAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        { return String (roundToInt (v * 100.0f)) + " %"; });
    NormalisableRange<float> unit (0.0f, 1.0f, 0.001f);

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed",   1 }, "Speed",    speedRange, 25.0f, msAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount",  1 }, "Amount",   unit, 1.0f, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain",  unit, 0.5f, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "stab",    1 }, "Stabilizer", unit, 0.4f, pctAttr));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "double",  1 }, "Double",   unit, 0.0f, pctAttr));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key",    1 }, "Key",
            StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale",  1 }, "Scale", Scales::list(), 0));
    return l;
}

VioletTuneProcessor::VioletTuneProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pSpeed   = apvts.getRawParameterValue ("speed");
    pAmount  = apvts.getRawParameterValue ("amount");
    pSustain = apvts.getRawParameterValue ("sustain");
    pStab    = apvts.getRawParameterValue ("stab");
    pDouble  = apvts.getRawParameterValue ("double");
    pKey     = apvts.getRawParameterValue ("key");
    pScale   = apvts.getRawParameterValue ("scale");
    for (int i = 0; i < 12; ++i) { inHeat[i].store (0.0f); outHeat[i].store (0.0f); }
}

void VioletTuneProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& s : shifters) s.prepare (sr);
    for (auto& s : dblShift) s.prepare (sr);
    for (auto& d : dblDelay) d.prepare ((int) (0.05 * sr));
    ring.fill (0.0f);
    ringW = hopCount = 0;
    correction = desired = 0.0f; coef = 1.0f;
    lastTarget = pendingTarget = -1; pendingCount = unvoicedHops = 0; noteTime = 0.0f;
    setLatencySamples ((int) (0.02 * sr));
}

bool VioletTuneProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto in = l.getMainInputChannelSet();
    return in == l.getMainOutputChannelSet() && (in == AudioChannelSet::mono() || in == AudioChannelSet::stereo());
}

// YIN on a 2x-decimated copy of the newest audio (cheap enough to run every 256 samples)
float VioletTuneProcessor::detectPitch()
{
    const double srD = sr * 0.5;
    const int tauMax = jmin ((int) (srD / 70.0), decSize - yinW - 2, (int) dBuf.size() - 2);
    const int tauMin = jmax (2, (int) (srD / 1000.0));
    const int base = decSize - (yinW + tauMax + 1);       // newest samples

    float energy = 0.0f;
    for (int j = 0; j < yinW; ++j) { float x = dec[(size_t) (decSize - 1 - j)]; energy += x * x; }
    if (energy / (float) yinW < 1e-6f) return -1.0f;       // silence gate

    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < yinW; ++j)
        {
            float diff = dec[(size_t) (base + j)] - dec[(size_t) (base + j + tau)];
            sum += diff * diff;
        }
        dBuf[(size_t) tau] = sum;
    }
    float running = 0.0f;
    dBuf[0] = 1.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += dBuf[(size_t) tau];
        dBuf[(size_t) tau] = running > 0.0f ? dBuf[(size_t) tau] * (float) tau / running : 1.0f;
    }
    for (int tau = tauMin; tau < tauMax - 1; ++tau)
    {
        if (dBuf[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax - 1 && dBuf[(size_t) tau + 1] < dBuf[(size_t) tau]) ++tau;
            float a = dBuf[(size_t) tau - 1], b = dBuf[(size_t) tau], c = dBuf[(size_t) tau + 1];
            float denom = a - 2.0f * b + c;
            float shift = std::abs (denom) > 1e-9f ? 0.5f * (a - c) / denom : 0.0f;
            return (float) srD / ((float) tau + shift);
        }
    }
    return -1.0f;
}

void VioletTuneProcessor::analyse()
{
    for (int i = 0; i < ringSize; ++i) linear[(size_t) i] = ring[(size_t) ((ringW + i) % ringSize)];
    for (int i = 0; i < decSize; ++i)  dec[(size_t) i] = 0.5f * (linear[(size_t) (2 * i)] + linear[(size_t) (2 * i + 1)]);

    const float speed = pSpeed->load(), amount = pAmount->load(), sustain = pSustain->load(), stab = pStab->load();
    const int key = (int) pKey->load(), scale = jlimit (0, 7, (int) pScale->load());
    const float hopSec = (float) hop / (float) sr;
    auto allowed = [&] (int note) { return Scales::masks[scale][((note - key) % 12 + 12) % 12]; };
    auto pc = [] (float m) { return (((int) std::round (m)) % 12 + 12) % 12; };

    for (int i = 0; i < 12; ++i) { inHeat[i].store (inHeat[i].load() * 0.993f); outHeat[i].store (outHeat[i].load() * 0.993f); }

    const float f0 = detectPitch();
    float tauMs = jmax (0.3f, speed);                      // negative speed = fastest possible snap

    if (f0 > 0.0f)
    {
        unvoicedHops = 0;
        const float midi = 69.0f + 12.0f * std::log2 (f0 / 440.0f);

        int best = -1; float bestDist = 1e9f;
        for (int c = (int) std::round (midi) - 3; c <= (int) std::round (midi) + 3; ++c)
        {
            if (!allowed (c)) continue;
            float dist = std::abs ((float) c - midi);
            if (dist < bestDist) { bestDist = dist; best = c; }
        }

        // ---- Note Stabilizer: hysteresis + the new note must persist for a few frames ----
        if (lastTarget < 0 || !allowed (lastTarget)) { lastTarget = best; noteTime = 0.0f; pendingTarget = -1; pendingCount = 0; }
        else if (best != lastTarget)
        {
            const float need = 0.5f + stab * 0.45f;
            if (std::abs (midi - (float) lastTarget) > need)
            {
                if (best == pendingTarget) ++pendingCount; else { pendingTarget = best; pendingCount = 1; }
                if (pendingCount >= 1 + (int) (stab * 6.0f)) { lastTarget = best; noteTime = 0.0f; pendingTarget = -1; pendingCount = 0; }
            }
            else { pendingTarget = -1; pendingCount = 0; }
            noteTime += hopSec;
        }
        else { noteTime += hopSec; pendingTarget = -1; pendingCount = 0; }

        // ---- Sustain: how firmly small drifts/vibrato on a held note are flattened ----
        const float dev = midi - (float) lastTarget;
        float x = jlimit (0.0f, 1.0f, (std::abs (dev) - 0.05f) / 0.30f);
        float smooth = x * x * (3.0f - 2.0f * x);
        float w = sustain + (1.0f - sustain) * smooth;
        desired = -dev * w * amount;

        inHeat[pc (midi)].store (jmin (1.0f, inHeat[pc (midi)].load() + 0.10f));
        const int po = pc (midi + correction);
        outHeat[po].store (jmin (1.0f, outHeat[po].load() + 0.10f));
    }
    else
    {
        lastTarget = -1; noteTime = 0.0f;
        if (++unvoicedHops > 14) { desired = 0.0f; tauMs = 80.0f; }   // hold briefly, then relax
    }

    coef = 1.0f - std::exp (-1.0f / (float) (sr * tauMs * 0.001));
}

void VioletTuneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chs = jmin (buffer.getNumChannels(), 2);
    const float dbl = pDouble->load() * 0.7f;
    const float up = std::exp2 (9.0f / 1200.0f), dn = std::exp2 (-9.0f / 1200.0f);
    const int d1 = (int) (0.014 * sr), d2 = (int) (0.023 * sr);

    for (int s = 0; s < n; ++s)
    {
        float mono = 0.0f;
        for (int c = 0; c < chs; ++c) mono += buffer.getReadPointer (c)[s];
        mono /= (float) jmax (1, chs);

        ring[(size_t) ringW] = mono;
        ringW = (ringW + 1) % ringSize;
        if (++hopCount >= hop)
        {
            hopCount = 0;
            analyse();
        }

        correction += coef * (desired - correction);
        const float ratio = std::exp2 (correction * (1.0f / 12.0f));

        float a = 0.0f, b = 0.0f;
        if (dbl > 0.001f)
        {
            a = dblDelay[0].process (dblShift[0].process (mono, ratio * up), d1);
            b = dblDelay[1].process (dblShift[1].process (mono, ratio * dn), d2);
        }
        for (int c = 0; c < chs; ++c)
        {
            float* d = buffer.getWritePointer (c);
            float dbo = chs == 2 ? (c == 0 ? a : b) : 0.5f * (a + b);
            d[s] = shifters[c].process (d[s], ratio) + dbo * dbl;
        }
    }
}

AudioProcessorEditor* VioletTuneProcessor::createEditor() { return new VioletTuneEditor (*this); }

void VioletTuneProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void VioletTuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VioletTuneProcessor(); }
