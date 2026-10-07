#include "OfflineEffects.h"
#include <cmath>

const std::array<OfflineEffects::Parameter, OfflineEffects::count> OfflineEffects::specs{{
  {"fxCompOn", "Compressor", 0, 1, 0, ""},
  {"fxCompThreshold", "Threshold", -60, 0, -18, "dB"},
  {"fxCompRatio", "Ratio", 1, 12, 4, ":1"},
  {"fxCompAttack", "Attack", 1, 100, 10, "ms"},
  {"fxCompRelease", "Release", 20, 1000, 150, "ms"},
  {"fxCompMakeup", "Makeup", 0, 18, 0, "dB"},
  {"fxDriveOn", "Drive", 0, 1, 0, ""},
  {"fxDriveGain", "Drive", 0, 30, 12, "dB"},
  {"fxDriveTone", "Tone", 500, 10000, 4000, "Hz"},
  {"fxDriveLevel", "Level", -24, 6, -6, "dB"},
  {"fxChorusOn", "Chorus", 0, 1, 0, ""},
  {"fxChorusRate", "Rate", 0.05f, 5, 0.8f, "Hz"},
  {"fxChorusDepth", "Depth", 0, 1, 0.4f, ""},
  {"fxChorusMix", "Mix", 0, 1, 0.3f, ""},
  {"fxPhaserOn", "Phaser", 0, 1, 0, ""},
  {"fxPhaserRate", "Rate", 0.05f, 5, 0.4f, "Hz"},
  {"fxPhaserDepth", "Depth", 0, 1, 0.7f, ""},
  {"fxPhaserMix", "Mix", 0, 1, 0.5f, ""},
  {"fxTremOn", "Tremolo", 0, 1, 0, ""},
  {"fxTremRate", "Rate", 0.1f, 15, 4, "Hz"},
  {"fxTremDepth", "Depth", 0, 1, 0.5f, ""},
  {"fxDelayOn", "Delay", 0, 1, 0, ""},
  {"fxDelayTime", "Time", 10, 2000, 350, "ms"},
  {"fxDelayFeedback", "Feedback", 0, 0.85f, 0.35f, ""},
  {"fxDelayMix", "Mix", 0, 1, 0.25f, ""},
  {"fxDelayTone", "Tone", 500, 12000, 6000, "Hz"},
  {"fxReverbOn", "Reverb", 0, 1, 0, ""},
  {"fxReverbSize", "Size", 0, 1, 0.5f, ""},
  {"fxReverbDamping", "Damping", 0, 1, 0.5f, ""},
  {"fxReverbMix", "Mix", 0, 1, 0.2f, ""}
}};

namespace {
constexpr double tau = juce::MathConstants<double>::twoPi;
bool isSwitch(int i) {
  return i == OfflineEffects::compOn || i == OfflineEffects::driveOn || i == OfflineEffects::chorusOn
      || i == OfflineEffects::phaserOn || i == OfflineEffects::tremOn || i == OfflineEffects::delayOn
      || i == OfflineEffects::reverbOn;
}
float readDelay(const juce::AudioBuffer<float>& b, int ch, int write, float delay) {
  float position = static_cast<float>(write) - delay;
  if (position < 0) position += static_cast<float>(b.getNumSamples());
  const int first = static_cast<int>(position);
  const int second = (first + 1) % b.getNumSamples();
  const float fraction = position - static_cast<float>(first);
  return b.getSample(ch, first) + fraction * (b.getSample(ch, second) - b.getSample(ch, first));
}
float logCosh(float x) {
  const float a = std::abs(x);
  return a + std::log1p(std::exp(-2.0f * a)) - 0.69314718056f;
}
float softClip(float x, float previous) {
  // First-order antiderivative antialiasing of tanh; stable at equal inputs.
  return std::abs(x - previous) > 0.001f
      ? (logCosh(x) - logCosh(previous)) / (x - previous)
      : std::tanh(0.5f * (x + previous));
}
}

void OfflineEffects::addParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout) {
  for (int i = 0; i < count; ++i) {
    const auto& s = specs[static_cast<size_t>(i)];
    const juce::ParameterID id{s.id, 44 + i};
    if (isSwitch(i)) layout.add(std::make_unique<juce::AudioParameterBool>(id, s.name, false));
    else layout.add(std::make_unique<juce::AudioParameterFloat>(id, juce::String(s.id).substring(2) + " / " + s.name,
        juce::NormalisableRange<float>(s.min, s.max, 0.0001f), s.initial,
        juce::AudioParameterFloatAttributes().withLabel(s.unit)));
  }
}

void OfflineEffects::bind(juce::AudioProcessorValueTreeState& state) {
  for (int i = 0; i < count; ++i) params[static_cast<size_t>(i)] = state.getRawParameterValue(specs[static_cast<size_t>(i)].id);
}
float OfflineEffects::raw(Index i) const {
  const auto index = static_cast<size_t>(i);
  const float value = params[index] ? params[index]->load(std::memory_order_relaxed) : specs[index].initial;
  return std::isfinite(value) ? juce::jlimit(specs[index].min, specs[index].max, value) : specs[index].initial;
}
void OfflineEffects::prepare(double sampleRate, int maximumBlockSize) {
  juce::ignoreUnused(maximumBlockSize);
  rate = std::max(8000.0, sampleRate);
  chorusBuffer.setSize(2, static_cast<int>(rate * 0.06) + 4);
  delayBuffer.setSize(2, static_cast<int>(rate * 2.0) + 4);
  reverb.setSampleRate(rate);
  for (int i = 0; i < count; ++i) smooth[static_cast<size_t>(i)].reset(rate, 0.025);
  prepared = true;
  reset();
}
void OfflineEffects::reset() {
  chorusBuffer.clear(); delayBuffer.clear(); reverb.reset();
  chorusWrite = delayWrite = 0;
  envelope = 0; chorusPhase = phaserPhase = tremPhase = 0;
  drivePrevious.fill(0); driveLow.fill(0); delayLow.fill(0); phaserFeedback.fill(0);
  for (auto& s : phaserState) s.fill(0);
  postWasActive = delayWasActive = reverbWasActive = false;
  for (int i = 0; i < count; ++i) smooth[static_cast<size_t>(i)].setCurrentAndTargetValue(raw(static_cast<Index>(i)));
}
void OfflineEffects::updateTargets() {
  for (int i = 0; i < count; ++i) smooth[static_cast<size_t>(i)].setTargetValue(raw(static_cast<Index>(i)));
}

void OfflineEffects::processPre(juce::AudioBuffer<float>& buffer) {
  if (!prepared) return;
  updateTargets();
  if (raw(compOn) == 0 && raw(driveOn) == 0 && smooth[compOn].getCurrentValue() == 0 && smooth[driveOn].getCurrentValue() == 0) {
    for (int i = compOn; i < chorusOn; ++i) smooth[static_cast<size_t>(i)].skip(buffer.getNumSamples());
    envelope = 0; drivePrevious.fill(0); driveLow.fill(0);
    return;
  }
  const int channels = std::min(2, buffer.getNumChannels());
  // Attack/release are detector coefficients, read once per block.
  const float a = std::exp(-1.0f / (0.001f * raw(attack) * static_cast<float>(rate)));
  const float r = std::exp(-1.0f / (0.001f * raw(release) * static_cast<float>(rate)));
  const float low = 1.0f - std::exp(-static_cast<float>(tau) * std::min(raw(driveTone), static_cast<float>(rate * 0.45)) / static_cast<float>(rate));
  for (int n = 0; n < buffer.getNumSamples(); ++n) {
    const float comp = next(compOn), drive = next(driveOn);
    const float th = next(threshold), ra = next(ratio), mu = next(makeup);
    const float gain = juce::Decibels::decibelsToGain(next(driveGain));
    const float level = juce::Decibels::decibelsToGain(next(driveLevel));
    if (comp == 0 && drive == 0) { envelope = 0; drivePrevious.fill(0); driveLow.fill(0); continue; }
    float peak = 0;
    for (int ch = 0; ch < channels; ++ch) peak = std::max(peak, std::abs(buffer.getSample(ch, n)));
    const float coefficient = peak > envelope ? a : r;
    envelope = coefficient * envelope + (1.0f - coefficient) * peak;
    const float over = std::max(0.0f, juce::Decibels::gainToDecibels(envelope, -120.0f) - th);
    const float compGain = juce::Decibels::decibelsToGain(mu - over * (1.0f - 1.0f / ra));
    for (int ch = 0; ch < channels; ++ch) {
      float x = buffer.getSample(ch, n);
      x *= 1.0f + comp * (compGain - 1.0f);
      if (drive > 0) {
        const float input = x * gain;
        const auto c = static_cast<size_t>(ch);
        const float wet = softClip(input, drivePrevious[c]);
        drivePrevious[c] = input;
        driveLow[c] += low * (wet - driveLow[c]);
        x += drive * (driveLow[c] * level - x);
      }
      buffer.setSample(ch, n, x);
    }
  }
}

void OfflineEffects::processPost(juce::AudioBuffer<float>& buffer) {
  if (!prepared) return;
  updateTargets();
  bool active = false;
  for (auto i : {chorusOn, phaserOn, tremOn, delayOn, reverbOn})
    active = active || raw(i) > 0 || smooth[static_cast<size_t>(i)].getCurrentValue() > 0;
  if (!active) {
    if (postWasActive) {
      chorusBuffer.clear(); delayBuffer.clear(); reverb.reset(); delayLow.fill(0);
      for (auto& s : phaserState) s.fill(0);
      phaserFeedback.fill(0);
    }
    postWasActive = delayWasActive = reverbWasActive = false;
    for (int i = chorusOn; i < count; ++i) smooth[static_cast<size_t>(i)].skip(buffer.getNumSamples());
    return;
  }
  postWasActive = true;
  const int channels = std::min(2, buffer.getNumChannels());
  juce::Reverb::Parameters p;
  p.roomSize = raw(reverbSize); p.damping = raw(reverbDamping);
  p.wetLevel = 1; p.dryLevel = 0; p.width = 1; p.freezeMode = 0;
  reverb.setParameters(p); // JUCE internally smooths its coefficients.
  const float low = 1.0f - std::exp(-static_cast<float>(tau) * std::min(raw(delayTone), static_cast<float>(rate * 0.45)) / static_cast<float>(rate));
  for (int n = 0; n < buffer.getNumSamples(); ++n) {
    const float chorus = next(chorusOn), phaser = next(phaserOn), trem = next(tremOn);
    const float delay = next(delayOn), verb = next(reverbOn);
    if (delay == 0 && delayWasActive) { delayBuffer.clear(); delayLow.fill(0); }
    if (verb == 0 && reverbWasActive) reverb.reset();
    delayWasActive = delay > 0; reverbWasActive = verb > 0;
    const float cr = next(chorusRate), cd = next(chorusDepth), cm = next(chorusMix);
    const float pr = next(phaserRate), pd = next(phaserDepth), pm = next(phaserMix);
    const float tr = next(tremRate), td = next(tremDepth);
    const float dt = next(delayTime) * 0.001f * static_cast<float>(rate);
    const float feedback = next(delayFeedback), mix = next(delayMix), vm = next(reverbMix);
    std::array<float, 2> x{}, wet{};
    for (int ch = 0; ch < channels; ++ch) {
      const auto c = static_cast<size_t>(ch);
      x[c] = buffer.getSample(ch, n);
      chorusBuffer.setSample(ch, chorusWrite, x[c] * chorus);
      if (chorus > 0) {
        const float ms = 15.0f + 7.0f * cd * static_cast<float>(std::sin(chorusPhase + ch * 1.57));
        x[c] += chorus * cm * (readDelay(chorusBuffer, ch, chorusWrite, ms * 0.001f * static_cast<float>(rate)) - x[c]);
      }
      if (phaser > 0) {
        const float hz = 300.0f * std::pow(10.0f, pd * (0.5f + 0.5f * static_cast<float>(std::sin(phaserPhase))));
        const float t = std::tan(static_cast<float>(juce::MathConstants<double>::pi) * std::min(hz, static_cast<float>(rate * 0.4)) / static_cast<float>(rate));
        const float coefficient = (1.0f - t) / (1.0f + t);
        float y = x[c] + 0.25f * phaserFeedback[c];
        for (auto& z : phaserState[c]) { const float out = z - coefficient * y; z = y + coefficient * out; y = out; }
        phaserFeedback[c] = y;
        x[c] += phaser * pm * (y - x[c]);
      } else { phaserState[c].fill(0); phaserFeedback[c] = 0; }
      x[c] *= 1.0f - trem * td * (0.5f + 0.5f * static_cast<float>(std::sin(tremPhase)));
      const float echo = readDelay(delayBuffer, ch, delayWrite, dt);
      delayLow[c] += low * (echo - delayLow[c]);
      delayBuffer.setSample(ch, delayWrite, delay * (x[c] + feedback * delayLow[c]));
      // Bypass fades the whole effect out. Re-enabling never resurrects an old tail.
      x[c] += delay * mix * (delayLow[c] - x[c]);
      wet[c] = x[c] * verb;
    }
    if (verb > 0) {
      if (channels == 2) reverb.processStereo(&wet[0], &wet[1], 1);
      else if (channels == 1) reverb.processMono(&wet[0], 1);
    }
    for (int ch = 0; ch < channels; ++ch) {
      const auto c = static_cast<size_t>(ch);
      buffer.setSample(ch, n, x[c] + verb * vm * (wet[c] - x[c]));
    }
    chorusWrite = (chorusWrite + 1) % chorusBuffer.getNumSamples();
    delayWrite = (delayWrite + 1) % delayBuffer.getNumSamples();
    chorusPhase = std::fmod(chorusPhase + tau * cr / rate, tau);
    phaserPhase = std::fmod(phaserPhase + tau * pr / rate, tau);
    tremPhase = std::fmod(tremPhase + tau * tr / rate, tau);
  }
}

double OfflineEffects::tailSeconds() const {
  // Conservative bounds while bypass ramps finish; serial reverb follows delay.
  const double feedback = raw(delayFeedback);
  const double repeats = feedback > 0 ? std::max(1.0, std::ceil(std::log(0.0001) / std::log(feedback))) : 1.0;
  return (raw(delayOn) > 0.5f ? raw(delayTime) * 0.001 * repeats : 0.0)
      + (raw(reverbOn) > 0.5f ? 30.0 : 0.0);
}
