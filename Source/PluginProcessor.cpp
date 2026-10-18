#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout FuckAnTuneProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
                    StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
                    StringArray { "Chromatique", "Majeur", "Mineur", "Pentatonique mineur" }, 0));

    // 0 = tres lent, 1 = correction instantanee (equivalent du "0" d'Antares)
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Speed",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.85f));

    // l'EXTRA : robotisation au-dela de la correction instantanee
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "robot", 1 }, "Robot",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    return layout;
}

FuckAnTuneProcessor::FuckAnTuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    keyParam   = apvts.getRawParameterValue ("key");
    scaleParam = apvts.getRawParameterValue ("scale");
    speedParam = apvts.getRawParameterValue ("speed");
    robotParam = apvts.getRawParameterValue ("robot");
}

//==============================================================================
bool FuckAnTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void FuckAnTuneProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    decim  = jmax (1, (int) std::round (sr / 12000.0));
    srD    = sr / (double) decim;
    tauMin = jmax (4, (int) (srD / 1100.0));          // voix jusqu'a ~1100 Hz
    tauMax = (int) std::ceil (srD / 70.0) + 2;        // voix a partir de ~70 Hz

    decRing.assign ((size_t) decSize, 0.0f);
    frame.assign   ((size_t) (frameW + tauMax + 2), 0.0f);
    diffBuf.assign ((size_t) (tauMax + 2), 0.0f);
    cmnd.assign    ((size_t) (tauMax + 2), 1.0f);

    for (int c = 0; c < 2; ++c)
    {
        dl[c].assign   ((size_t) dlSize, 0.0f);
        comb[c].assign ((size_t) combSize, 0.0f);
    }

    decPos = decCnt = sinceAnalysis = 0;
    decAcc = 0.0f;
    voiced = false;
    corrCur = 0.0f;
    shiftPhase = 0.0f;
    dlPos = combPos = 0;
    combD = 200.0f;
    outMidiHeld = 60.0f;
}

//==============================================================================
// Note autorisee la plus proche (en numero MIDI)
float FuckAnTuneProcessor::findTargetNote (float midi) const
{
    const int base = (int) std::floor (midi);
    float best = std::round (midi);
    float bestDist = 1.0e9f;

    for (int k = base - 7; k <= base + 8; ++k)
    {
        const int pc = ((k % 12) + 12) % 12;

        if (! noteMask[pc])
            continue;

        const float dist = std::abs ((float) k - midi);

        if (dist < bestDist)
        {
            bestDist = dist;
            best = (float) k;
        }
    }

    return best;
}

//==============================================================================
// Detection de hauteur (algorithme YIN) sur le signal reduit a ~12 kHz
void FuckAnTuneProcessor::analyse()
{
    const int need = frameW + tauMax + 1;

    for (int j = 0; j < need; ++j)
        frame[(size_t) j] = decRing[(size_t) ((decPos - need + j) & decMask)];

    float energy = 0.0f;
    for (int j = 0; j < frameW; ++j)
        energy += frame[(size_t) j] * frame[(size_t) j];
    energy /= (float) frameW;

    if (energy < 4.0e-6f)               // trop faible : pas de voix
    {
        voiced = false;
        return;
    }

    // 1) fonction de difference
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float s = 0.0f;
        for (int j = 0; j < frameW; ++j)
        {
            const float d = frame[(size_t) j] - frame[(size_t) (j + tau)];
            s += d * d;
        }
        diffBuf[(size_t) tau] = s;
    }

    // 2) difference moyenne normalisee
    cmnd[0] = 1.0f;
    float run = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        run += diffBuf[(size_t) tau];
        cmnd[(size_t) tau] = run > 0.0f ? diffBuf[(size_t) tau] * (float) tau / run : 1.0f;
    }

    // 3) premier creux sous le seuil
    int tauEst = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (cmnd[(size_t) tau] < 0.15f)
        {
            while (tau + 1 < tauMax && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                ++tau;

            tauEst = tau;
            break;
        }
    }

    if (tauEst < 0)                     // sinon : plus petit creux, s'il est assez net
    {
        int minTau = tauMin;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cmnd[(size_t) tau] < cmnd[(size_t) minTau])
                minTau = tau;

        if (cmnd[(size_t) minTau] < 0.35f)
            tauEst = minTau;
    }

    if (tauEst < 0)
    {
        voiced = false;
        return;
    }

    // 4) interpolation parabolique
    float better = (float) tauEst;
    if (tauEst > 1 && tauEst < tauMax)
    {
        const float s0 = cmnd[(size_t) (tauEst - 1)];
        const float s1 = cmnd[(size_t) tauEst];
        const float s2 = cmnd[(size_t) (tauEst + 1)];
        const float denom = 2.0f * (2.0f * s1 - s2 - s0);

        if (std::abs (denom) > 1.0e-9f)
            better += (s2 - s0) / denom;
    }

    const float f0 = (float) srD / better;

    if (f0 < 70.0f || f0 > 1100.0f)
    {
        voiced = false;
        return;
    }

    const float midi = 69.0f + 12.0f * std::log2 (f0 / 440.0f);

    if (! voiced)
        midiHist[0] = midiHist[1] = midiHist[2] = midi;

    midiHist[2] = midiHist[1];
    midiHist[1] = midiHist[0];
    midiHist[0] = midi;

    // mediane sur 3 mesures : evite les sauts d'octave
    detMidi = jmax (jmin (midiHist[0], midiHist[1]),
                    jmin (jmax (midiHist[0], midiHist[1]), midiHist[2]));

    targetNote = findTargetNote (detMidi);
    corrTarget = targetNote - detMidi;
    voiced = true;
}

//==============================================================================
void FuckAnTuneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh = jmin (2, buffer.getNumChannels());
    const int n     = buffer.getNumSamples();

    if (numCh == 0 || decRing.empty())
        return;

    // --- parametres
    const int   keyIdx   = jlimit (0, 11, (int) keyParam->load());
    const int   scaleIdx = jlimit (0, 3,  (int) scaleParam->load());
    const float speed    = speedParam->load();
    const float robot    = robotParam->load();

    static const bool scales[4][12] = {
        { 1,1,1,1,1,1,1,1,1,1,1,1 },   // chromatique
        { 1,0,1,0,1,1,0,1,0,1,0,1 },   // majeur
        { 1,0,1,1,0,1,0,1,1,0,1,0 },   // mineur
        { 1,0,0,1,0,1,0,1,0,0,1,0 }    // pentatonique mineur
    };

    for (int i = 0; i < 12; ++i)
        noteMask[(i + keyIdx) % 12] = scales[scaleIdx][i];

    // speed 1.0 = instantane ; plus bas = glissement plus lent (jusqu'a 400 ms)
    const float tauSec = std::pow (1.0f - speed, 2.0f) * 0.4f;
    const float tauEff = jmax (0.0004f, tauSec);
    const float sr = (float) sampleRate;

    const float kCorr = 1.0f - std::exp (-1.0f / (tauEff * sr));
    const float kUnv  = 1.0f - std::exp (-1.0f / (0.03f * sr));
    const float kComb = 1.0f - std::exp (-1.0f / (0.015f * sr));

    // ROBOT : fenetre de decalage plus courte + resonance a la hauteur de la note
    const float W        = (0.030f - 0.020f * robot) * sr;
    const float combG    = 0.55f + 0.35f * robot;
    const float robotMix = robot * 0.85f;

    float* chan[2] = { buffer.getWritePointer (0),
                       numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        const float in0 = chan[0][i];
        const float in1 = chan[1] != nullptr ? chan[1][i] : in0;

        // --- detection (sur le signal mono, reduit en frequence)
        decAcc += 0.5f * (in0 + in1);

        if (++decCnt >= decim)
        {
            decRing[(size_t) decPos] = decAcc / (float) decim;
            decPos = (decPos + 1) & decMask;
            decAcc = 0.0f;
            decCnt = 0;

            if (++sinceAnalysis >= hopD)
            {
                sinceAnalysis = 0;
                analyse();
            }
        }

        // --- correction lissee (en demi-tons)
        const float tgt = voiced ? corrTarget : 0.0f;
        corrCur += (voiced ? kCorr : kUnv) * (tgt - corrCur);

        if (voiced)
            outMidiHeld = detMidi + corrCur;

        const float ratio = jlimit (0.5f, 2.0f, std::exp2 (corrCur / 12.0f));

        // --- decalage de hauteur : deux lignes a retard qui se fondent l'une dans l'autre
        shiftPhase += (1.0f - ratio) / W;
        if (shiftPhase >= 1.0f)      shiftPhase -= 1.0f;
        else if (shiftPhase < 0.0f)  shiftPhase += 1.0f;

        float p2 = shiftPhase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;

        const float d1 = shiftPhase * W;
        const float d2 = p2 * W;
        const float g1 = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * shiftPhase);
        const float g2 = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * p2);

        // --- resonance robot accordee sur la note de sortie
        const float noteHz = jlimit (60.0f, 2000.0f, 440.0f * std::exp2 ((outMidiHeld - 69.0f) / 12.0f));
        const float dTarget = jlimit (24.0f, (float) combSize - 4.0f, sr / noteHz);
        combD += kComb * (dTarget - combD);

        for (int c = 0; c < numCh; ++c)
        {
            auto& d = dl[c];
            d[(size_t) dlPos] = (c == 0 ? in0 : in1);

            const float s = g1 * readLinear (d, dlMask, dlPos, d1)
                          + g2 * readLinear (d, dlMask, dlPos, d2);

            auto& cb = comb[c];
            const float yD = readLinear (cb, combMask, combPos, combD);
            const float y  = s + combG * yD;
            cb[(size_t) combPos] = y;

            const float cOut = y * (1.0f - combG);
            chan[c][i] = s + robotMix * (cOut - s);
        }

        dlPos   = (dlPos + 1) & dlMask;
        combPos = (combPos + 1) & combMask;
    }

    targetMidiMeter.store (targetNote);
    voicedMeter.store (voiced ? 1.0f : 0.0f);
}

//==============================================================================
AudioProcessorEditor* FuckAnTuneProcessor::createEditor()
{
    return new FuckAnTuneEditor (*this);
}

void FuckAnTuneProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FuckAnTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FuckAnTuneProcessor();
}
