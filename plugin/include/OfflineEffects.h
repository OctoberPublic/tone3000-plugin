#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

// Fixed host-rate rack. All buffers are allocated in prepare(), never in audio callbacks.
class OfflineEffects {
public:
  struct Parameter { const char* id; const char* name; float min, max, initial; const char* unit; };
  enum Index {
    compOn, threshold, ratio, attack, release, makeup,
    driveOn, driveGain, driveTone, driveLevel,
    chorusOn, chorusRate, chorusDepth, chorusMix,
    phaserOn, phaserRate, phaserDepth, phaserMix,
    tremOn, tremRate, tremDepth,
    delayOn, delayTime, delayFeedback, delayMix, delayTone,
    reverbOn, reverbSize, reverbDamping, reverbMix, count
  };
  static const std::array<Parameter, count> specs;
  static void addParameters(juce::AudioProcessorValueTreeState::ParameterLayout&);
  void bind(juce::AudioProcessorValueTreeState&);
  void prepare(double sampleRate, int maximumBlockSize);
  void reset();
  void processPre(juce::AudioBuffer<float>&);
  void processPost(juce::AudioBuffer<float>&);
  double tailSeconds() const;

private:
  void updateTargets();
  float next(Index i) { return smooth[static_cast<size_t>(i)].getNextValue(); }
  float raw(Index i) const;
  double rate = 48000.0;
  std::array<std::atomic<float>*, count> params{};
  std::array<juce::SmoothedValue<float>, count> smooth;
  juce::AudioBuffer<float> chorusBuffer, delayBuffer;
  int chorusWrite = 0, delayWrite = 0;
  float envelope = 0.0f;
  double chorusPhase = 0, phaserPhase = 0, tremPhase = 0;
  std::array<float, 2> drivePrevious{}, driveLow{}, delayLow{}, phaserFeedback{};
  std::array<std::array<float, 4>, 2> phaserState{};
  juce::Reverb reverb;
  bool prepared = false;
  bool postWasActive = false, delayWasActive = false, reverbWasActive = false;
};
