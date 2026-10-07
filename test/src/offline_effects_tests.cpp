#include "OfflineEffects.h"
#include "Processor.h"
#include <gtest/gtest.h>
#include <cmath>

namespace {
void set(TONE3000Processor& p, const char* id, float v) {
  auto* param = p.parameters.getParameter(id);
  ASSERT_NE(param, nullptr);
  param->setValueNotifyingHost(param->convertTo0to1(v));
}
struct Rack {
  TONE3000Processor processor;
  OfflineEffects effects;
  Rack() { effects.bind(processor.parameters); }
  void prepare(double rate = 48000) { effects.prepare(rate, 128); }
};
float energy(const juce::AudioBuffer<float>& b, int begin = 0) {
  double sum = 0;
  for (int ch = 0; ch < b.getNumChannels(); ++ch)
    for (int i = begin; i < b.getNumSamples(); ++i) sum += b.getSample(ch, i) * b.getSample(ch, i);
  return static_cast<float>(sum);
}
}

TEST(OfflineEffectsTest, DisabledRackIsBitExactAtAllHostRatesAndBlockSizes) {
  for (const double rate : {44100.0, 48000.0, 96000.0, 192000.0}) for (const int channels : {1, 2}) {
    Rack rack; rack.prepare(rate);
    for (const int size : {0, 1, 17, 128, 4097}) {
      juce::AudioBuffer<float> b(channels, size), original(channels, size);
      for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < size; ++i)
        b.setSample(ch, i, 0.7f * std::sin(static_cast<float>(i + ch)));
      original.makeCopyOf(b); rack.effects.processPre(b); rack.effects.processPost(b);
      for (int ch = 0; ch < channels; ++ch) for (int i = 0; i < size; ++i)
        EXPECT_EQ(b.getSample(ch, i), original.getSample(ch, i));
    }
  }
}
TEST(OfflineEffectsTest, CompressorReducesPeaksAndLinksStereoDetector) {
  Rack r; set(r.processor, "fxCompOn", 1); set(r.processor, "fxCompAttack", 1);
  set(r.processor, "fxCompThreshold", -20); set(r.processor, "fxCompRatio", 4); r.prepare();
  juce::AudioBuffer<float> b(2, 4800);
  for (int i = 0; i < b.getNumSamples(); ++i) { b.setSample(0, i, 1); b.setSample(1, i, 0.25f); }
  r.effects.processPre(b);
  EXPECT_NEAR(b.getSample(0, 4799), std::pow(10.0f, -15.0f / 20), 0.002f);
  EXPECT_NEAR(b.getSample(0, 4799), 4 * b.getSample(1, 4799), 0.000001f);
}
TEST(OfflineEffectsTest, DriveLimitsHotSignalAndIsSymmetric) {
  Rack positive, negative;
  for (auto* r : {&positive, &negative}) { set(r->processor, "fxDriveOn", 1); set(r->processor, "fxDriveGain", 30); r->prepare(); }
  juce::AudioBuffer<float> a(1, 4800), b(1, 4800);
  for (int i = 0; i < 4800; ++i) { a.setSample(0, i, 2); b.setSample(0, i, -2); }
  positive.effects.processPre(a); negative.effects.processPre(b);
  EXPECT_LT(a.getSample(0, 4799), 0.51f);
  EXPECT_GT(a.getSample(0, 4799), 0.45f);
  EXPECT_NEAR(a.getSample(0, 4799), -b.getSample(0, 4799), 0.0001f);
}
TEST(OfflineEffectsTest, DelayHasExpectedTimeFeedbackAndReset) {
  Rack r; set(r.processor, "fxDelayOn", 1); set(r.processor, "fxDelayTime", 100);
  set(r.processor, "fxDelayMix", 1); set(r.processor, "fxDelayFeedback", 0.5f); r.prepare();
  juce::AudioBuffer<float> b(2, 15000); b.clear(); b.setSample(0, 0, 1);
  r.effects.processPost(b);
  EXPECT_EQ(energy(b, 0) > 0, true);
  for (int i = 0; i < 4799; ++i) EXPECT_FLOAT_EQ(b.getSample(0, i), 0);
  EXPECT_GT(b.getSample(0, 4800), 0.3f); EXPECT_GT(b.getSample(0, 9600), 0.1f);
  EXPECT_EQ(b.getMagnitude(1, 0, b.getNumSamples()), 0);
  r.effects.reset(); b.clear(); r.effects.processPost(b); EXPECT_EQ(energy(b), 0);
}
TEST(OfflineEffectsTest, TremoloUsesHostRateAndHasExpectedDepth) {
  Rack r; set(r.processor, "fxTremOn", 1); set(r.processor, "fxTremRate", 2); set(r.processor, "fxTremDepth", 1); r.prepare();
  juce::AudioBuffer<float> b(1, 24000);
  for (int i = 0; i < b.getNumSamples(); ++i) b.setSample(0, i, 1);
  r.effects.processPost(b);
  EXPECT_NEAR(b.getSample(0, 6000), 0, 0.0001f);
  EXPECT_NEAR(b.getSample(0, 18000), 1, 0.0001f);
}
TEST(OfflineEffectsTest, ChorusAndPhaserProduceWetResponse) {
  for (const auto* on : {"fxChorusOn", "fxPhaserOn"}) {
    Rack r; set(r.processor, on, 1); r.prepare();
    juce::AudioBuffer<float> b(2, 4096); b.clear(); b.setSample(0, 0, 1); b.setSample(1, 0, 1);
    r.effects.processPost(b);
    EXPECT_LT(b.getSample(0, 0), 0.999f);
    EXPECT_GT(energy(b, 1), 0.0001f);
  }
}
TEST(OfflineEffectsTest, ReverbRingsOutOnSilenceAndResets) {
  Rack r; set(r.processor, "fxReverbOn", 1); set(r.processor, "fxReverbMix", 1); r.prepare();
  juce::AudioBuffer<float> b(2, 48000); b.clear(); b.setSample(0, 0, 1); r.effects.processPost(b);
  EXPECT_GT(energy(b, 1000), 0.001f); EXPECT_GT(r.effects.tailSeconds(), 2);
  r.effects.reset(); b.clear(); r.effects.processPost(b); EXPECT_EQ(energy(b), 0);
}
TEST(OfflineEffectsTest, EveryEffectParameterSurvivesHostStateAndPresetRoundTrip) {
  TONE3000Processor a, b;
  for (const auto& s : OfflineEffects::specs) set(a, s.id, s.max);
  juce::MemoryBlock state; a.getStateInformation(state); b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  for (const auto& s : OfflineEffects::specs) EXPECT_NEAR(b.parameters.getRawParameterValue(s.id)->load(), s.max, 0.001f) << s.id;
  auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("local-rig-test-" + juce::Uuid().toString());
  a.setPresetStoreForTesting(dir);
  auto saved = a.savePreset("Effects"); ASSERT_TRUE(saved.isObject());
  for (const auto& s : OfflineEffects::specs) set(a, s.id, s.initial);
  ASSERT_TRUE(a.loadPreset(saved["id"].toString()));
  for (const auto& s : OfflineEffects::specs) EXPECT_NEAR(a.parameters.getRawParameterValue(s.id)->load(), s.max, 0.001f) << s.id;
  dir.deleteRecursively();
}
TEST(OfflineEffectsTest, AllEffectsRemainFiniteDuringAutomationAndReprepare) {
  Rack r;
  for (const double rate : {44100.0, 96000.0}) {
    r.prepare(rate);
    for (int block = 0; block < 80; ++block) {
      for (const auto& s : OfflineEffects::specs) set(r.processor, s.id, block % 2 ? s.max : s.min);
      juce::AudioBuffer<float> b(2, block % 3 ? 257 : 1);
      for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < b.getNumSamples(); ++i) b.setSample(ch, i, 0.1f * std::sin(0.1f * static_cast<float>(i)));
      r.effects.processPre(b); r.effects.processPost(b);
      for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < b.getNumSamples(); ++i) {
        EXPECT_TRUE(std::isfinite(b.getSample(ch, i))); EXPECT_LT(std::abs(b.getSample(ch, i)), 32);
      }
    }
  }
}

TEST(OfflineEffectsTest, BypassClearsDelayAndReverbWhileAnotherEffectIsOn) {
  for (const auto* on : {"fxDelayOn", "fxReverbOn"}) {
    Rack r; set(r.processor, on, 1); set(r.processor, "fxTremOn", 1); set(r.processor, "fxTremDepth", 0); r.prepare();
    juce::AudioBuffer<float> impulse(2, 128); impulse.clear(); impulse.setSample(0, 0, 1); r.effects.processPost(impulse);
    set(r.processor, on, 0);
    juce::AudioBuffer<float> silence(2, 4800); silence.clear(); r.effects.processPost(silence);
    set(r.processor, on, 1);
    juce::AudioBuffer<float> after(2, 48000); after.clear(); r.effects.processPost(after);
    EXPECT_EQ(energy(after), 0) << on;
  }
}

TEST(OfflineEffectsTest, DelayIsConnectedToTheActualProcessorSignalPath) {
  TONE3000Processor p;
  // Isolate the delay response from the existing gate's opening envelope.
  set(p, "gateEnabled", 0);
  set(p, "fxDelayOn", 1); set(p, "fxDelayTime", 100); set(p, "fxDelayMix", 1); set(p, "fxDelayFeedback", 0);
  p.setPlayConfigDetails(2, 2, 48000, 128);
  p.prepareToPlay(48000, 128);
  juce::MidiBuffer midi;
  double early = 0, echo = 0;
  for (int block = 0; block < 50; ++block) {
    juce::AudioBuffer<float> buffer(2, 128); buffer.clear();
    if (block == 0) { buffer.setSample(0, 0, 1); buffer.setSample(1, 0, 1); }
    p.processBlock(buffer, midi);
    for (int i = 0; i < 128; ++i) {
      const double power = buffer.getSample(0, i) * buffer.getSample(0, i);
      if (block * 128 + i < 4700) early += power;
      else echo += power;
    }
  }
  EXPECT_LT(early, 0.000001); EXPECT_GT(echo, 0.01);
  EXPECT_GE(p.getTailLengthSeconds(), 0.1);
  p.releaseResources();
}

TEST(OfflineEffectsTest, RemoteModelUrlsAreRejectedWithoutNetworking) {
  TONE3000Processor p;
  juce::Array<juce::URL> urls{juce::URL("https://example.invalid/model.nam")};
  EXPECT_FALSE(p.loadLocalToneUrls(urls)["error"].isVoid());
}
