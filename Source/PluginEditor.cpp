#include "PluginEditor.h"

using namespace neb;

namespace
{
constexpr int userIdBase = 10000;
const juce::Rectangle<int> headerBounds { 12, 10, NebulonEditor::designW - 24, 92 };
}

NebulonEditor::NebulonEditor (NebulonProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    // pages
    pages.push_back (std::make_unique<ui::MainPage> (proc, &content, [this] (int l) { showPage (1 + l); }));
    for (int l = 0; l < numLayers; ++l)
        pages.push_back (std::make_unique<ui::LayerPage> (proc, &content, l));
    pages.push_back (std::make_unique<ui::ModPage> (proc, &content));
    pages.push_back (std::make_unique<ui::ArpPage> (proc, &content));
    pages.push_back (std::make_unique<ui::FxPage> (proc, &content));
    for (auto& pg : pages) content.addChildComponent (*pg);

    const juce::StringArray tabNames { "MAIN", "LAYER A", "LAYER B", "LAYER C", "LAYER D", "MODULATION", "ARP", "EFFECTS" };
    for (int i = 0; i < tabNames.size(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (tabNames[i]);
        b->setClickingTogglesState (false);
        juce::Colour c = colours::perf;
        if (i >= 1 && i <= 4) c = colours::layer (i - 1);
        if (i == 5) c = colours::mod;
        if (i == 6) c = colours::layerA;
        if (i == 7) c = colours::fx;
        b->setColour (juce::TextButton::buttonOnColourId, c);
        b->onClick = [this, i] { showPage (i); };
        content.addAndMakeVisible (*b);
        tabs.push_back (std::move (b));
    }

    // presets
    content.addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("-");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id >= userIdBase)
        {
            const int idx = id - userIdBase;
            if (juce::isPositiveAndBelow (idx, userPresetFiles.size())) proc.loadUserPreset (userPresetFiles[idx]);
        }
        else if (id - 1 != proc.getCurrentProgram() || proc.currentPresetName != proc.getProgramName (id - 1))
        {
            proc.setCurrentProgram (id - 1);
        }
    };
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn })
    {
        b->setColour (juce::TextButton::buttonOnColourId, colours::perf);
        content.addAndMakeVisible (*b);
    }
    auto step = [this] (int d)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
        refreshPresetBox();
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
    saveBtn.onClick = [this] { savePresetDialog(); };
    refreshPresetBox();

    content.addAndMakeVisible (scope);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffcfd6e6));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, colours::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::layerA.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::layerB.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, colours::outline);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setLowestVisibleKey (36);
    content.addAndMakeVisible (keyboard);

    showPage (0);

    setResizable (true, true);
    setResizeLimits (designW * 3 / 4, designH * 3 / 4, designW * 2, designH * 2);
    getConstrainer()->setFixedAspectRatio ((double) designW / designH);
    setSize (designW, designH);
    startTimerHz (30);
}

NebulonEditor::~NebulonEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NebulonEditor::showPage (int index)
{
    currentPage = juce::jlimit (0, (int) pages.size() - 1, index);
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        pages[(size_t) i]->setVisible (i == currentPage);
        tabs[(size_t) i]->setToggleState (i == currentPage, juce::dontSendNotification);
    }
    pages[(size_t) currentPage]->refresh();
}

void NebulonEditor::resized()
{
    content.setBounds (0, 0, designW, designH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designW));
}

void NebulonEditor::Content::resized()
{
    auto& e = owner;
    auto h = headerBounds.reduced (16, 8);
    auto row1 = h.removeFromTop (44);
    h.removeFromTop (6);
    auto row2 = h;

    // title area is painted; scope in the middle; presets on the right
    row1.removeFromLeft (230);
    auto presetArea = row1.removeFromRight (420);
    e.scope.setBounds (row1.reduced (12, 2));
    auto pr = presetArea.withSizeKeepingCentre (presetArea.getWidth(), 30);
    e.saveBtn.setBounds (pr.removeFromRight (64));
    pr.removeFromRight (6);
    e.nextBtn.setBounds (pr.removeFromRight (30));
    e.prevBtn.setBounds (pr.removeFromLeft (30));
    e.presetBox.setBounds (pr.reduced (4, 0));

    const int tw = row2.getWidth() / (int) e.tabs.size();
    for (auto& t : e.tabs) t->setBounds (row2.removeFromLeft (tw).reduced (3, 0));

    auto body = getLocalBounds().reduced (12, 0);
    body.removeFromTop (headerBounds.getBottom() + 8);
    e.keyboard.setBounds (body.removeFromBottom (72).withTrimmedBottom (10));
    e.keyboard.setKeyWidth ((float) e.keyboard.getWidth() / 43.0f);
    body.removeFromBottom (6);
    for (auto& pg : e.pages) pg->setBounds (body);
}

void NebulonEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    const auto w = (float) getWidth(), h = (float) getHeight();
    g.setGradientFill (juce::ColourGradient (colours::layerA.withAlpha (0.10f), w * 0.15f, h * 0.1f,
                                             juce::Colours::transparentBlack, w * 0.55f, h * 0.6f, true));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (colours::layerB.withAlpha (0.10f), w * 0.85f, h * 0.2f,
                                             juce::Colours::transparentBlack, w * 0.4f, h * 0.7f, true));
    g.fillAll();

    auto hb = headerBounds.toFloat();
    g.setColour (colours::panel.withAlpha (0.9f));
    g.fillRoundedRectangle (hb, 10.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (hb.reduced (0.5f), 10.0f, 1.0f);

    auto t = headerBounds.reduced (20, 8).removeFromTop (46).removeFromLeft (230);
    juce::ColourGradient tg (colours::layerA, (float) t.getX(), 0, colours::layerB, (float) t.getRight() - 30, 0, false);
    g.setGradientFill (tg);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)).withExtraKerningFactor (0.18f));
    g.drawText ("NEBULON", t.removeFromTop (30), juce::Justification::bottomLeft);
    g.setColour (colours::textDim);
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.12f));
    g.drawText ("HYBRID ATMOSPHERE SYNTHESIZER", t, juce::Justification::topLeft);
}

//==============================================================================
void NebulonEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    juce::String lastCat;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        const auto cat = proc.getPresetCategory (i);
        if (cat != lastCat) { presetBox.addSectionHeading (cat); lastCat = cat; }
        presetBox.addItem (proc.getProgramName (i), i + 1);
    }
    userPresetFiles = proc.getUserPresets();
    if (! userPresetFiles.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userPresetFiles.size(); ++i)
            presetBox.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userIdBase + i);
    }

    int sel = 0;
    for (int i = 0; i < userPresetFiles.size(); ++i)
        if (userPresetFiles[i].getFileNameWithoutExtension() == proc.currentPresetName) sel = userIdBase + i;
    if (sel == 0 && proc.getProgramName (proc.getCurrentProgram()) == proc.currentPresetName)
        sel = proc.getCurrentProgram() + 1;
    if (sel != 0) presetBox.setSelectedId (sel, juce::dontSendNotification);
    else presetBox.setText (proc.currentPresetName, juce::dontSendNotification);
}

void NebulonEditor::savePresetDialog()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name your preset (saved to Documents/Nebulon/Presets)",
                                     juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName.isEmpty() ? juce::String ("My Patch") : proc.currentPresetName + " (edit)");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<NebulonEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result != 1 || safe == nullptr) return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty() && safe->proc.saveUserPreset (name))
            safe->refreshPresetBox();
    }), true);
}

void NebulonEditor::timerCallback()
{
    scope.repaint();
    pages[(size_t) currentPage]->refresh();
    if (presetBox.getText() != proc.currentPresetName && ! presetBox.isPopupActive())
        refreshPresetBox();
}

//==============================================================================
void NebulonEditor::Scope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::bg.withAlpha (0.8f));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    constexpr int N = NebulonProcessor::scopeSize;
    std::array<float, N> buf;
    const int w = proc.scopeWrite.load();
    for (int i = 0; i < N; ++i) buf[(size_t) i] = proc.scope[(size_t) ((w + i) % N)];

    const int view = 512;
    int startIdx = N - view - 1;
    for (int i = N - view - 2; i > N / 4; --i)
        if (buf[(size_t) i] <= 0.0f && buf[(size_t) i + 1] > 0.0f) { startIdx = i; break; }

    float peak = 0.0f;
    for (int i = 0; i < view; ++i) peak = juce::jmax (peak, std::abs (buf[(size_t) (startIdx + i)]));
    const float gain = peak > 0.0001f ? juce::jmin (4.0f, 0.9f / peak) : 1.0f;

    juce::Path path;
    const float midY = b.getCentreY(), amp = b.getHeight() * 0.42f;
    for (int i = 0; i < view; ++i)
    {
        const float x = b.getX() + 6 + (b.getWidth() - 12) * (float) i / (float) (view - 1);
        const float y = midY - buf[(size_t) (startIdx + i)] * gain * amp;
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }

    juce::ColourGradient grad (colours::layerA, b.getX(), 0, colours::layerB, b.getRight(), 0, false);
    g.setGradientFill (grad);
    g.setOpacity (0.25f);
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
    g.setOpacity (1.0f);
    g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
}
