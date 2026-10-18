#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include <vector>

using namespace juce;

class FuckAnTuneProcessor : public AudioProcessor
{
public:
    FuckAnTuneProcessor();
    ~FuckAnTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return "FUCKANTUNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    AudioProcessorValueTreeState apvts;

    // Infos pour l'interface
    std::atomic<float> targetMidiMeter { 0.0f };   // note visee (numero MIDI)
    std::atomic<float> voicedMeter     { 0.0f };   // 1 = voix detectee

private:
    static AudioProcessorValueTreeState::ParameterLayout createLayout();

    void analyse();                       // detection de hauteur (YIN)
    float findTargetNote (float midi) const;

    static float readLinear (const std::vector<float>& buf, int mask, int writePos, float delay)
    {
        const float rp = (float) writePos - delay;
        const int i0 = (int) std::floor (rp);
        const float fr = rp - (float) i0;
        return buf[(size_t) (i0 & mask)] * (1.0f - fr) + buf[(size_t) ((i0 + 1) & mask)] * fr;
    }

    // --- parametres lus au debut de chaque bloc
    std::atomic<float>* keyParam    = nullptr;
    std::atomic<float>* scaleParam  = nullptr;
    std::atomic<float>* speedParam  = nullptr;
    std::atomic<float>* robotParam  = nullptr;

    bool noteMask[12] = {};

    // --- detection de hauteur
    double sampleRate = 44100.0;
    int    decim = 4;
    double srD = 11025.0;
    int    tauMin = 10, tauMax = 160;

    static constexpr int frameW = 384;
    static constexpr int hopD   = 64;
    static constexpr int decSize = 2048;
    static constexpr int decMask = decSize - 1;

    std::vector<float> decRing, frame, diffBuf, cmnd;
    int   decPos = 0, decCnt = 0, sinceAnalysis = 0;
    float decAcc = 0.0f;

    bool  voiced = false;
    float midiHist[3] = { 0, 0, 0 };
    float detMidi = 60.0f, corrTarget = 0.0f, targetNote = 60.0f;

    // --- correction / decalage de hauteur
    float corrCur = 0.0f;                 // correction actuelle en demi-tons
    float shiftPhase = 0.0f;
    float outMidiHeld = 60.0f;
    float combD = 200.0f;

    static constexpr int dlSize = 16384;
    static constexpr int dlMask = dlSize - 1;
    static constexpr int combSize = 4096;
    static constexpr int combMask = combSize - 1;

    std::vector<float> dl[2], comb[2];
    int dlPos = 0, combPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuckAnTuneProcessor)
};
