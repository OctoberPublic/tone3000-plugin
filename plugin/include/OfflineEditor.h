#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Processor.h"

// Local-only editor. No HTTP client, account/session, updater or audio-device manager.
class OfflineEditor final : public juce::AudioProcessorEditor,
                            public juce::FileDragAndDropTarget,
                            private juce::ListBoxModel, private juce::Timer {
public:
  explicit OfflineEditor(TONE3000Processor&);
  ~OfflineEditor() override;
  void paint(juce::Graphics&) override;
  void resized() override;
  bool isInterestedInFileDrag(const juce::StringArray&) override;
  void filesDropped(const juce::StringArray&, int, int) override;

private:
  class RackPage;
  int getNumRows() override;
  void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
  void selectedRowsChanged(int) override;
  void timerCallback() override;
  void refreshChain();
  void refreshPresets();
  void chooseFile(bool folder, bool replace);
  void loadFile(const juce::File&, const std::string& target = {});
  void moveSelected(int direction);
  juce::var selectedBlock() const;
  std::string selectedId() const;

  TONE3000Processor& processor;
  juce::LookAndFeel_V4 look;
  juce::Label title, status, route;
  juce::TextButton add{"Load NAM / IR"}, folder{"Load folder"}, replace{"Replace"}, remove{"Remove"};
  juce::TextButton up{"Up"}, down{"Down"}, undo{"Undo"}, redo{"Redo"}, reset{"New"}, save{"Save preset"};
  juce::ToggleButton stereo{"Two chains"}, power{"Block on"}, normalize{"Normalize NAM"};
  juce::ComboBox lane, presets, models;
  juce::TextEditor presetName;
  juce::ListBox chain{"Local models", this};
  juce::Slider blockInput, blockOutput, blockMix;
  juce::Label inputLabel, outputLabel, mixLabel;
  juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
  std::unique_ptr<juce::FileChooser> chooser;
  std::vector<juce::var> rows;
  juce::StringArray presetIds;
  bool updating = false;
  juce::uint32 revision = ~juce::uint32{};
};
