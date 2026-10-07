#include "OfflineEditor.h"
#include "OfflineEffects.h"

namespace {
constexpr juce::uint32 background = 0xff11151c, surface = 0xff1c2330, accent = 0xff69ddbd;
struct Control : juce::Component {
  juce::Label label;
  juce::Slider slider;
  juce::ToggleButton toggle;
  juce::ComboBox choice;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> choiceAttachment;
  Control(juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name) {
    setName(id);
    auto* parameter = state.getParameter(id);
    if (parameter == nullptr) return;
    if (dynamic_cast<juce::AudioParameterBool*>(parameter)) {
      toggle.setButtonText(name); toggle.setName(name); toggle.setWantsKeyboardFocus(false);
      addAndMakeVisible(toggle);
      buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state, id, toggle);
    } else if (auto* options = dynamic_cast<juce::AudioParameterChoice*>(parameter)) {
      label.setText(name, juce::dontSendNotification); label.setJustificationType(juce::Justification::centred);
      choice.addItemList(options->choices, 1); choice.setName(name);
      addAndMakeVisible(label); addAndMakeVisible(choice);
      choiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state, id, choice);
    } else {
      label.setText(name, juce::dontSendNotification); label.setJustificationType(juce::Justification::centred);
      slider.setName(name); slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 22);
      slider.setTextValueSuffix(parameter->getLabel().isEmpty() ? "" : " " + parameter->getLabel());
      slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
      addAndMakeVisible(label); addAndMakeVisible(slider);
      sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, id, slider);
      const bool percent = id.startsWith("fx") && parameter->getNormalisableRange().start == 0
          && parameter->getNormalisableRange().end == 1;
      const auto unit = parameter->getLabel();
      slider.textFromValueFunction = [percent, unit](double v) {
        if (percent) return juce::String(v * 100, 1) + " %";
        return juce::String(v, std::abs(v) >= 100 ? 0 : 1) + (unit.isEmpty() ? "" : " " + unit);
      };
      slider.valueFromTextFunction = [percent](const juce::String& text) { return text.getDoubleValue() / (percent ? 100.0 : 1.0); };
      if (id == "inputLevel" || id == "outputLevel") {
        slider.textFromValueFunction = [](double v) { return juce::String((v - 0.5) * 48.0, 1) + " dB"; };
        slider.valueFromTextFunction = [](const juce::String& text) { return text.getDoubleValue() / 48.0 + 0.5; };
      }
      slider.updateText();
    }
  }
  void resized() override {
    auto r = getLocalBounds(); label.setBounds(r.removeFromTop(24)); slider.setBounds(r);
    toggle.setBounds(getLocalBounds().reduced(2, 28));
    choice.setBounds(r.reduced(8, 18).withHeight(28));
  }
};
}

class OfflineEditor::RackPage : public juce::Component {
public:
  explicit RackPage(juce::AudioProcessorValueTreeState& s) : state(s) {
    addAndMakeVisible(viewport); viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
  }
  void group(const juce::String& name, std::initializer_list<std::pair<const char*, const char*>> entries) {
    auto* label = headings.add(new juce::Label());
    label->setText(name, juce::dontSendNotification); label->setColour(juce::Label::textColourId, juce::Colour(accent));
    content.addAndMakeVisible(label);
    const int start = controls.size();
    for (auto [id, text] : entries) { auto* c = controls.add(new Control(state, id, text)); content.addAndMakeVisible(c); }
    groups.emplace_back(start, controls.size());
  }
  void effect(int first, int end) {
    auto* label = headings.add(new juce::Label());
    label->setText(OfflineEffects::specs[static_cast<size_t>(first)].name, juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, juce::Colour(accent)); content.addAndMakeVisible(label);
    const int start = controls.size();
    for (int i = first; i < end; ++i) {
      const auto& p = OfflineEffects::specs[static_cast<size_t>(i)];
      auto* c = controls.add(new Control(state, p.id, i == first ? "Enabled" : p.name)); content.addAndMakeVisible(c);
    }
    groups.emplace_back(start, controls.size());
  }
  void resized() override {
    viewport.setBounds(getLocalBounds());
    const int w = std::max(300, getWidth() - 22);
    int y = 12;
    for (size_t n = 0; n < groups.size(); ++n) {
      headings[static_cast<int>(n)]->setBounds(12, y, w - 24, 26); y += 28;
      auto [begin, end] = groups[n];
      const int columns = std::min(4, end - begin), width = (w - 24) / columns;
      for (int i = begin; i < end; ++i)
        controls[i]->setBounds(12 + ((i - begin) % columns) * width, y + ((i - begin) / columns) * 106, width, 100);
      y += ((end - begin + columns - 1) / columns) * 106 + 16;
    }
    content.setSize(w, std::max(getHeight(), y));
  }
private:
  juce::AudioProcessorValueTreeState& state;
  juce::Component content;
  juce::Viewport viewport;
  juce::OwnedArray<Control> controls;
  juce::OwnedArray<juce::Label> headings;
  std::vector<std::pair<int, int>> groups;
};

OfflineEditor::OfflineEditor(TONE3000Processor& p) : AudioProcessorEditor(p), processor(p) {
  look.setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(background));
  look.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(accent));
  look.setColour(juce::Slider::thumbColourId, juce::Colour(accent));
  look.setColour(juce::TextButton::buttonColourId, juce::Colour(surface));
  look.setColour(juce::ListBox::backgroundColourId, juce::Colour(background));
  setLookAndFeel(&look);
  setWantsKeyboardFocus(false);
  title.setText("LOCAL RIG  /  VST3", juce::dontSendNotification);
  title.setFont(juce::FontOptions(22.0f, juce::Font::bold));
  route.setText("INPUT > COMP > DRIVE > NAM / IR > CHORUS > PHASER > TREMOLO > DELAY > REVERB > OUTPUT", juce::dontSendNotification);
  route.setFont(juce::FontOptions(12.0f));
  status.setText("Drop an A2 .nam model or an IR .wav. All effects work without a model.", juce::dontSendNotification);
  presetName.setTextToShowWhenEmpty("Preset name", juce::Colours::grey);
  presets.setTextWhenNothingSelected("Presets"); lane.addItem("Left / mono", 1); lane.addItem("Right", 2); lane.setSelectedId(1);
  models.setTextWhenNothingSelected("Select a model");
  juce::Component* components[]{&title, &route, &status, &presets, &presetName, &save, &undo, &redo, &reset,
    &add, &folder, &replace, &remove, &up, &down, &stereo, &power, &normalize, &lane, &models,
    &chain, &blockInput, &blockOutput, &blockMix, &inputLabel, &outputLabel, &mixLabel, &tabs};
  for (auto* c : components) addAndMakeVisible(c);
  for (auto* b : {&add, &folder, &replace, &remove, &up, &down, &undo, &redo, &reset, &save}) b->setWantsKeyboardFocus(false);
  chain.setRowHeight(48);
  inputLabel.setText("Block input", juce::dontSendNotification); outputLabel.setText("Block output", juce::dontSendNotification);
  mixLabel.setText("Block mix", juce::dontSendNotification);
  for (auto* s : {&blockInput, &blockOutput, &blockMix}) {
    s->setSliderStyle(juce::Slider::LinearHorizontal); s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    s->setRange(0, 1, 0.001);
  }
  blockInput.onValueChange = [this] { if (!updating) processor.setBlockParam(selectedId(), "inputGain", blockInput.getValue()); };
  blockOutput.onValueChange = [this] { if (!updating) processor.setBlockParam(selectedId(), "outputGain", blockOutput.getValue()); };
  blockMix.onValueChange = [this] { if (!updating) processor.setBlockParam(selectedId(), "mix", blockMix.getValue()); };
  power.onClick = [this] { processor.setBlockParam(selectedId(), "enabled", power.getToggleState()); };
  normalize.onClick = [this] { processor.setBlockParam(selectedId(), "normalize", normalize.getToggleState()); };
  add.onClick = [this] { chooseFile(false, false); }; folder.onClick = [this] { chooseFile(true, false); };
  replace.onClick = [this] { chooseFile(false, true); };
  remove.onClick = [this] { processor.removeChainBlock(selectedId()); refreshChain(); };
  up.onClick = [this] { moveSelected(-1); }; down.onClick = [this] { moveSelected(1); };
  undo.onClick = [this] { processor.undoChain(); refreshChain(); }; redo.onClick = [this] { processor.redoChain(); refreshChain(); };
  reset.onClick = [this] { processor.resetToDefault(); refreshChain(); };
  stereo.onClick = [this] { processor.setStereoMode(stereo.getToggleState()); refreshChain(); };
  lane.onChange = [this] { if (!updating) { processor.setActiveEditChain(lane.getSelectedId() == 2 ? "right" : "left"); revision = ~juce::uint32{}; refreshChain(); } };
  presets.onChange = [this] {
    const int i = presets.getSelectedId() - 1;
    if (!updating && i >= 0 && i < presetIds.size()) {
      if (processor.loadPreset(presetIds[i])) { presetName.setText(presets.getText()); refreshChain(); }
      else status.setText("Could not load preset.", juce::dontSendNotification);
    }
  };
  save.onClick = [this] {
    const auto result = processor.savePreset(presetName.getText());
    status.setText(result.isObject() ? "Preset saved with embedded models and effects." : "Enter a preset name; check that the data folder is writable.", juce::dontSendNotification);
    refreshPresets();
  };
  models.onChange = [this] {
    if (updating) return;
    const auto block = selectedBlock();
    if (auto* array = block["tone"]["models"].getArray()) {
      const int i = models.getSelectedId() - 1;
      if (i >= 0 && i < array->size()) {
        auto m = array->getReference(i);
        processor.switchModel(selectedId(), static_cast<int>(m["id"]), m);
      }
    }
  };
  auto* input = new RackPage(processor.parameters);
  input->group("Input / output", {{"inputLevel", "Input"}, {"outputLevel", "Output"}, {"toneEqEnabled", "Tone EQ"}});
  input->group("Tone stack", {{"toneBass", "Bass"}, {"toneMid", "Mid"}, {"toneTreble", "Treble"}});
  input->group("Noise gate", {{"gateEnabled", "Enabled"}, {"gateThreshold", "Threshold"}, {"gateRelease", "Release"}, {"gateHold", "Hold"}});
  input->group("Pitch shift", {{"pitchEnabled", "Enabled"}, {"pitchSemitones", "Semitones"}, {"pitchStep", "Whole steps"}});
  input->group("NAM quality", {{"osEnabled", "Oversampling"}, {"osFactor", "Factor (2x/4x/8x)"}});
  tabs.addTab("Input / tone", juce::Colour(surface), input, true);
  auto* dynamics = new RackPage(processor.parameters);
  dynamics->effect(OfflineEffects::compOn, OfflineEffects::driveOn);
  dynamics->effect(OfflineEffects::driveOn, OfflineEffects::chorusOn);
  tabs.addTab("Compressor / drive", juce::Colour(surface), dynamics, true);
  auto* modulation = new RackPage(processor.parameters);
  modulation->effect(OfflineEffects::chorusOn, OfflineEffects::phaserOn);
  modulation->effect(OfflineEffects::phaserOn, OfflineEffects::tremOn);
  modulation->effect(OfflineEffects::tremOn, OfflineEffects::delayOn);
  tabs.addTab("Modulation", juce::Colour(surface), modulation, true);
  auto* space = new RackPage(processor.parameters);
  space->effect(OfflineEffects::delayOn, OfflineEffects::reverbOn);
  space->effect(OfflineEffects::reverbOn, OfflineEffects::count);
  tabs.addTab("Delay / reverb", juce::Colour(surface), space, true);
  refreshPresets(); refreshChain();
  setResizable(true, false); setResizeLimits(1040, 720, 1600, 1100); setSize(1120, 760);
  startTimerHz(5);
}

OfflineEditor::~OfflineEditor() { stopTimer(); setLookAndFeel(nullptr); }
void OfflineEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(background)); }
void OfflineEditor::resized() {
  auto r = getLocalBounds().reduced(16);
  auto top = r.removeFromTop(36); title.setBounds(top.removeFromLeft(270));
  presets.setBounds(top.removeFromLeft(180).reduced(3)); presetName.setBounds(top.removeFromLeft(170).reduced(3));
  save.setBounds(top.removeFromLeft(106).reduced(3)); undo.setBounds(top.removeFromLeft(66).reduced(3));
  redo.setBounds(top.removeFromLeft(66).reduced(3)); reset.setBounds(top.removeFromLeft(66).reduced(3));
  route.setBounds(r.removeFromTop(30)); status.setBounds(r.removeFromBottom(30));
  auto left = r.removeFromLeft(310); r.removeFromLeft(14); tabs.setBounds(r);
  auto row = left.removeFromTop(34); lane.setBounds(row.removeFromLeft(150).reduced(2)); stereo.setBounds(row);
  row = left.removeFromTop(38); add.setBounds(row.removeFromLeft(155).reduced(2)); folder.setBounds(row.reduced(2));
  chain.setBounds(left.removeFromTop(std::max(130, left.getHeight() - 240)).reduced(2));
  row = left.removeFromTop(34); replace.setBounds(row.removeFromLeft(88).reduced(2)); remove.setBounds(row.removeFromLeft(82).reduced(2));
  up.setBounds(row.removeFromLeft(65).reduced(2)); down.setBounds(row.reduced(2));
  models.setBounds(left.removeFromTop(32).reduced(2));
  row = left.removeFromTop(32); power.setBounds(row.removeFromLeft(120)); normalize.setBounds(row);
  for (auto pair : {std::pair{&inputLabel, &blockInput}, std::pair{&outputLabel, &blockOutput}, std::pair{&mixLabel, &blockMix}}) {
    row = left.removeFromTop(36); pair.first->setBounds(row.removeFromLeft(90)); pair.second->setBounds(row);
  }
}
int OfflineEditor::getNumRows() { return static_cast<int>(rows.size()); }
void OfflineEditor::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
  if (row < 0 || row >= getNumRows()) return;
  if (selected) g.fillAll(juce::Colour(surface));
  const auto& b = rows[static_cast<size_t>(row)];
  g.setColour(juce::Colours::white); g.setFont(15.0f);
  g.drawText(b["tone"]["title"].toString(), 10, 2, w - 20, 24, juce::Justification::centredLeft);
  g.setColour(juce::Colour(accent)); g.setFont(12.0f);
  g.drawText(static_cast<bool>(b["loadFailed"]) ? "Unavailable - replace with a local file" : static_cast<bool>(b["loaded"]) ? "Local model ready" : "Loading...",
      10, 25, w - 20, h - 25, juce::Justification::centredLeft);
}
juce::var OfflineEditor::selectedBlock() const {
  const int i = chain.getSelectedRow(); return i >= 0 && i < static_cast<int>(rows.size()) ? rows[static_cast<size_t>(i)] : juce::var();
}
std::string OfflineEditor::selectedId() const { return selectedBlock()["blockId"].toString().toStdString(); }
void OfflineEditor::selectedRowsChanged(int) {
  const juce::ScopedValueSetter<bool> guard(updating, true);
  const auto block = selectedBlock(); const bool valid = block.isObject();
  for (juce::Component* c : std::initializer_list<juce::Component*>{&replace, &remove, &up, &down, &power, &normalize, &models, &blockInput, &blockOutput, &blockMix}) c->setEnabled(valid);
  power.setToggleState(static_cast<bool>(block["params"]["enabled"]), juce::dontSendNotification);
  normalize.setToggleState(static_cast<bool>(block["params"]["normalize"]), juce::dontSendNotification);
  blockInput.setValue(block["params"]["inputGain"], juce::dontSendNotification);
  blockOutput.setValue(block["params"]["outputGain"], juce::dontSendNotification);
  blockMix.setValue(block["params"]["mix"], juce::dontSendNotification);
  models.clear(juce::dontSendNotification);
  if (auto* array = block["tone"]["models"].getArray()) {
    for (int i = 0; i < array->size(); ++i) {
      const auto& m = array->getReference(i);
      auto name = m["name"].toString(); if (name.isEmpty()) name = "Model " + juce::String(i + 1);
      models.addItem(name, i + 1);
      if (m["id"] == block["activeModelId"]) models.setSelectedId(i + 1, juce::dontSendNotification);
    }
  }
}
void OfflineEditor::refreshChain() {
  const auto id = selectedId();
  const juce::ScopedValueSetter<bool> guard(updating, true);
  const auto state = processor.getChainState(-1);
  revision = static_cast<juce::uint32>(static_cast<int>(state["revision"]));
  const bool isStereo = static_cast<bool>(state["stereoEnabled"]);
  stereo.setToggleState(isStereo, juce::dontSendNotification); lane.setItemEnabled(2, isStereo);
  lane.setSelectedId(isStereo && state["activeSide"].toString() == "right" ? 2 : 1, juce::dontSendNotification);
  rows.clear();
  if (auto* array = state[lane.getSelectedId() == 2 ? "chainRight" : "chain"].getArray())
    for (auto& b : *array) if (b["kind"].toString() == "tone") rows.push_back(b);
  chain.updateContent();
  int selected = -1;
  for (int i = 0; i < getNumRows(); ++i) if (rows[static_cast<size_t>(i)]["blockId"].toString().toStdString() == id) selected = i;
  if (selected < 0 && !rows.empty()) selected = 0;
  chain.selectRow(selected); selectedRowsChanged(selected); chain.repaint();
  undo.setEnabled(static_cast<bool>(state["canUndo"])); redo.setEnabled(static_cast<bool>(state["canRedo"]));
}
void OfflineEditor::refreshPresets() {
  const juce::ScopedValueSetter<bool> guard(updating, true);
  const int old = presets.getSelectedId(); presets.clear(juce::dontSendNotification); presetIds.clear();
  const auto state = processor.getPresetList();
  if (auto* array = state["presets"].getArray()) for (auto& p : *array) {
    presetIds.add(p["id"].toString()); presets.addItem(p["name"].toString(), presetIds.size());
  }
  if (old <= presetIds.size()) presets.setSelectedId(old, juce::dontSendNotification);
}
void OfflineEditor::timerCallback() { if (revision != processor.getCurrentChainRevision()) refreshChain(); }
void OfflineEditor::chooseFile(bool isFolder, bool replaceBlock) {
  const auto target = replaceBlock ? selectedId() : std::string();
  chooser = std::make_unique<juce::FileChooser>(isFolder ? "Load a folder of NAM or IR files" : "Load an A2 NAM model or IR WAV", juce::File(), "*.nam;*.wav");
  chooser->launchAsync(juce::FileBrowserComponent::openMode | (isFolder ? juce::FileBrowserComponent::canSelectDirectories : juce::FileBrowserComponent::canSelectFiles),
      [self = juce::Component::SafePointer<OfflineEditor>(this), target](const juce::FileChooser& f) {
        if (self && f.getResult().exists()) self->loadFile(f.getResult(), target);
      });
}
void OfflineEditor::loadFile(const juce::File& f, const std::string& target) {
  const auto result = processor.loadLocalTonePath(f, target);
  status.setText(result["error"].isVoid() ? "Loaded " + f.getFileName() : result["error"].toString(), juce::dontSendNotification);
  refreshChain();
}
bool OfflineEditor::isInterestedInFileDrag(const juce::StringArray& files) {
  for (const auto& name : files) if (juce::File(name).isDirectory() || juce::File(name).hasFileExtension("nam;wav")) return true;
  return false;
}
void OfflineEditor::filesDropped(const juce::StringArray& files, int, int) {
  for (const auto& f : files) if (juce::File(f).isDirectory() || juce::File(f).hasFileExtension("nam;wav")) loadFile(juce::File(f));
}
void OfflineEditor::moveSelected(int direction) {
  const int i = chain.getSelectedRow(), next = i + direction;
  if (i < 0 || next < 0 || next >= getNumRows()) return;
  // Preserve the lane's insert placeholders; reorder only the two selected tone slots.
  const auto state = processor.getChainState(-1);
  std::vector<std::string> order;
  if (auto* array = state[lane.getSelectedId() == 2 ? "chainRight" : "chain"].getArray())
    for (auto& b : *array) order.push_back(b["blockId"].toString().toStdString());
  auto a = std::find(order.begin(), order.end(), selectedId());
  auto b = std::find(order.begin(), order.end(), rows[static_cast<size_t>(next)]["blockId"].toString().toStdString());
  if (a != order.end() && b != order.end()) { std::iter_swap(a, b); processor.reorderChainBlocks(order); refreshChain(); }
}
