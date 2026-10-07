#include "OfflineEditor.h"
#include <gtest/gtest.h>

namespace {
juce::Component* named(juce::Component& root, const juce::String& name) {
  if (root.getName() == name) return &root;
  for (auto* c : root.getChildren()) if (auto* result = named(*c, name)) return result;
  return nullptr;
}
juce::Slider* slider(juce::Component& root) {
  if (auto* s = dynamic_cast<juce::Slider*>(&root)) return s;
  for (auto* c : root.getChildren()) if (auto* result = slider(*c)) return result;
  return nullptr;
}
}

TEST(OfflineEditorTest, EffectControlEditsAutomatableParameterAndFollowsHostChanges) {
  TONE3000Processor p;
  OfflineEditor editor(p);
  for (auto* c : editor.getChildren())
    if (auto* tabs = dynamic_cast<juce::TabbedComponent*>(c)) tabs->setCurrentTabIndex(3);
  auto* control = named(editor, "fxDelayTime");
  ASSERT_NE(control, nullptr);
  auto* s = slider(*control); ASSERT_NE(s, nullptr);
  s->setValue(625, juce::sendNotificationSync);
  EXPECT_NEAR(p.parameters.getRawParameterValue("fxDelayTime")->load(), 625, 0.01);
  auto* parameter = p.parameters.getParameter("fxDelayTime");
  parameter->setValueNotifyingHost(parameter->convertTo0to1(250));
  juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
  EXPECT_NEAR(s->getValue(), 250, 0.01);
}

TEST(OfflineEditorTest, LocalFileDropCreatesAChainBlockWithoutAnAccount) {
  TONE3000Processor p;
  OfflineEditor editor(p);
  const auto files = juce::File(T3K_TEST_FILES_DIR).findChildFiles(juce::File::findFiles, true, "*.wav");
  ASSERT_FALSE(files.isEmpty());
  const juce::StringArray paths{files[0].getFullPathName()};
  ASSERT_TRUE(editor.isInterestedInFileDrag(paths));
  editor.filesDropped(paths, 30, 150);
  const auto state = p.getChainState(-1);
  int tones = 0;
  for (const auto& b : *state["chain"].getArray()) if (b["kind"].toString() == "tone") ++tones;
  EXPECT_EQ(tones, 1);
}

TEST(OfflineEditorTest, EditorRendersEveryEffectPageAtMinimumAndDefaultSize) {
  TONE3000Processor p;
  OfflineEditor editor(p);
  juce::TabbedComponent* tabs = nullptr;
  for (auto* c : editor.getChildren()) if (auto* t = dynamic_cast<juce::TabbedComponent*>(c)) tabs = t;
  ASSERT_NE(tabs, nullptr); ASSERT_EQ(tabs->getNumTabs(), 4);
  for (const int width : {1040, 1120}) {
    editor.setSize(width, 760);
    for (int i = 0; i < tabs->getNumTabs(); ++i) {
      tabs->setCurrentTabIndex(i);
      const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
      EXPECT_TRUE(image.isValid()); EXPECT_EQ(image.getWidth(), width);
      if (width == 1120) {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("local-rig-page-" + juce::String(i) + ".png");
        auto out = file.createOutputStream(); ASSERT_NE(out, nullptr); out->setPosition(0); out->truncate();
        EXPECT_TRUE(juce::PNGImageFormat().writeImageToStream(image, *out));
      }
    }
  }
}
