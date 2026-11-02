#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MasterPanels.h"
#include "BandLabImport.h"

// Overlay for finding presets: search, tag chips, a scrolling list, and Import / Export of single preset files.
class PresetBrowser : public juce::Component, private juce::TextEditor::Listener, private juce::ListBoxModel
{
public:
    std::function<void()> onClose;

    explicit PresetBrowser (ModularFXAudioProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty ("Search presets or tags", juce::Colour (0xFF7C828C));
        search.setFont (juce::Font (juce::FontOptions (16.0f)));
        search.setIndents (14, 0);
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF22252B));
        search.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        search.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFF3B82F6));
        search.addListener (this);
        addAndMakeVisible (search);

        list.setModel (this);
        list.setRowHeight (40);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xFF4A4F58));
        addAndMakeVisible (list);

        auto style = [] (masterui::PaintedButton& b, const juce::String& text)
        {
            b.painter = [text] (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down, bool)
            {
                g.setColour (down ? juce::Colour (0xFF383C45) : over ? juce::Colour (0xFF30333B) : juce::Colour (0xFF2A2D34));
                g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
                g.setColour (juce::Colours::white.withAlpha (0.92f));
                g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
                g.drawText (text, r, juce::Justification::centred);
            };
        };
        style (importButton, "Import...");  style (exportButton, "Export...");  style (closeButton, "Close");
        importButton.onClick = [this] { askImport(); };
        exportButton.onClick = [this] { askExport(); };
        closeButton.onClick  = [this] { close(); };
        for (auto* b : { &importButton, &exportButton, &closeButton }) addAndMakeVisible (*b);
        setVisible (false);
    }

    // ---- state used by the editor and by the tests
    void open()
    {
        search.setText ({}, false);
        tag = "All";
        refresh();
        setVisible (true);
        toFront (true);
        search.grabKeyboardFocus();
    }
    void close() { setVisible (false); if (onClose) onClose(); }
    void setSearchText (const juce::String& t) { search.setText (t, false); refresh(); }
    void setTag (const juce::String& t) { tag = t; refresh(); }
    juce::StringArray getVisibleNames() const { juce::StringArray n; for (int i : shown) n.add (all[(size_t) i].name); return n; }
    void pickRow (int row) { if (row >= 0 && row < (int) shown.size()) { proc.loadPreset (shown[(size_t) row]); close(); } }

    static const juce::StringArray& tagNames()
    {
        static const juce::StringArray t { "All", "Vocal", "Guitar", "Drums", "Mix", "Creative", "Utility", "Mine" };
        return t;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.6f));
        auto p = panel().toFloat();
        g.setColour (juce::Colour (0xFF181A1F));
        g.fillRoundedRectangle (p, 14.0f);
        g.setColour (juce::Colour (0xFF2A2D34));
        g.drawRoundedRectangle (p, 14.0f, 1.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)));
        g.drawText ("Presets", (int) p.getX() + 18, (int) p.getY() + 12, 120, 28, juce::Justification::centredLeft);

        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        chipRects.clear();
        int x = (int) p.getX() + 18;
        const int y = (int) p.getY() + 52;
        for (const auto& t : tagNames())
        {
            const int w = juce::Font (juce::FontOptions (13.0f)).getStringWidth (t) + 26;
            juce::Rectangle<int> r (x, y, w, 26);
            const bool on = t == tag;
            g.setColour (on ? juce::Colour (0xFF22C7DB) : juce::Colour (0xFF23262C));
            g.fillRoundedRectangle (r.toFloat(), 13.0f);
            g.setColour (on ? juce::Colour (0xFF0B1114) : juce::Colours::white.withAlpha (0.8f));
            g.drawText (t, r, juce::Justification::centred);
            chipRects.push_back ({ t, r });
            x += w + 6;
        }
        if (shown.empty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.setFont (juce::Font (juce::FontOptions (15.0f)));
            g.drawText ("No presets match", list.getBounds(), juce::Justification::centred);
        }
    }

    void resized() override
    {
        const auto p = panel();
        search.setBounds (p.getRight() - 18 - 250, p.getY() + 12, 250, 30);
        list.setBounds (p.getX() + 12, p.getY() + 90, p.getWidth() - 24, p.getHeight() - 90 - 56);
        closeButton.setBounds (p.getRight() - 18 - 80, p.getBottom() - 46, 80, 32);
        exportButton.setBounds (p.getRight() - 18 - 80 - 8 - 96, p.getBottom() - 46, 96, 32);
        importButton.setBounds (p.getRight() - 18 - 80 - 8 - 96 - 8 - 96, p.getBottom() - 46, 96, 32);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (const auto& c : chipRects)
            if (c.second.contains (e.getPosition())) { setTag (c.first); return; }
        if (! panel().contains (e.getPosition())) close();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        return false;
    }

private:
    juce::Rectangle<int> panel() const { return getLocalBounds().withSizeKeepingCentre (juce::jmin (680, getWidth() - 40), juce::jmin (380, getHeight() - 20)); }

    void refresh()
    {
        all = proc.getPresetList();
        shown.clear();
        const auto q = search.getText().trim();
        for (int i = 0; i < (int) all.size(); ++i)
        {
            const auto& info = all[(size_t) i];
            if (tag == "Mine" && info.factory) continue;
            if (tag != "All" && tag != "Mine" && ! tagsOf (info).contains (tag)) continue;
            if (q.isNotEmpty() && ! info.name.containsIgnoreCase (q) && ! info.tags.containsIgnoreCase (q)) continue;
            shown.push_back (i);
        }
        list.updateContent();
        list.repaint();
        repaint();
    }

    static juce::StringArray tagsOf (const ModularFXAudioProcessor::PresetInfo& i)
    {
        juce::StringArray t; t.addTokens (i.tags, ",", ""); t.trim(); t.removeEmptyStrings(); return t;
    }

    // ---- ListBoxModel
    int getNumRows() override { return (int) shown.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) shown.size()) return;
        const auto& info = all[(size_t) shown[(size_t) row]];
        const bool current = info.name == proc.getPresetName();
        auto r = juce::Rectangle<float> (0.0f, 2.0f, (float) w, (float) h - 4.0f).reduced (4.0f, 0.0f);
        g.setColour (selected ? juce::Colour (0xFF2A2D34) : juce::Colour (0xFF1D2025));
        g.fillRoundedRectangle (r, 9.0f);
        if (current) { g.setColour (juce::Colour (0xFF22C7DB)); g.fillEllipse (r.getX() + 12.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f); }

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.drawText (info.name, (int) r.getX() + 30, 0, w - 330, h, juce::Justification::centredLeft, true);

        float x = r.getRight() - 12.0f;
        if (! info.factory)
        {
            g.setColour (juce::Colours::white.withAlpha (0.4f));
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText ("mine", (int) x - 40, 0, 40, h, juce::Justification::centredRight);
            x -= 48.0f;
        }
        auto tags = tagsOf (info);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        for (int i = tags.size() - 1; i >= 0; --i)
        {
            const float tw = juce::Font (juce::FontOptions (12.0f)).getStringWidth (tags[i]) + 18.0f;
            juce::Rectangle<float> c (x - tw, (float) h * 0.5f - 11.0f, tw, 22.0f);
            g.setColour (juce::Colour (0xFF2E323A));
            g.fillRoundedRectangle (c, 11.0f);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (tags[i], c, juce::Justification::centred);
            x -= tw + 6.0f;
        }
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        pickRow (row);
    }

    void textEditorTextChanged (juce::TextEditor&) override { refresh(); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { close(); }

    int selectedPresetIndex() const
    {
        const int row = list.getSelectedRow();
        if (row >= 0 && row < (int) shown.size()) return shown[(size_t) row];
        for (int i = 0; i < (int) all.size(); ++i) if (all[(size_t) i].name == proc.getPresetName()) return i;
        return -1;
    }

    void askExport()
    {
        const int idx = selectedPresetIndex();
        if (idx < 0) return;
        const auto name = all[(size_t) idx].name;
        chooser = std::make_unique<juce::FileChooser> ("Export preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (juce::File::createLegalFileName (name) + ".mfxpreset"), "*.mfxpreset");
        juce::Component::SafePointer<PresetBrowser> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe, idx] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr || fc.getResult() == juce::File()) return;
                                  safe->proc.exportPreset (idx, fc.getResult().withFileExtension ("mfxpreset"));
                              });
    }

    void askImport()
    {
        juce::PopupMenu m;
        m.addItem (1, "Preset file (.mfxpreset)...");
        m.addItem (2, "BandLab preset file (.json)...");
        m.addItem (3, "BandLab preset from clipboard (link or JSON)");
        juce::Component::SafePointer<PresetBrowser> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (importButton),
                         [safe] (int r) { if (safe == nullptr) return; if (r == 1) safe->askImportFile(); else if (r == 2) safe->askImportBandLabFile(); else if (r == 3) safe->importBandLabText (juce::SystemClipboard::getTextFromClipboard()); });
    }

    // Shows what happened; lists anything that could not be converted.
    void reportBandLab (int idx, const juce::StringArray& notes)
    {
        if (idx < 0)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed", "That does not look like a BandLab effect preset.");
            return;
        }
        if (notes.isEmpty()) { close(); return; }
        juce::Component::SafePointer<PresetBrowser> safe (this);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Preset imported", "Some effects could not be converted:\n\n" + notes.joinIntoString ("\n"),
                                                "OK", nullptr, juce::ModalCallbackFunction::create ([safe] (int) { if (safe != nullptr) safe->close(); }));
    }

    void importBandLabText (const juce::String& textIn)
    {
        const auto text = textIn.trim();
        if (bandlab::looksLikeLink (text))
        {
            juce::Component::SafePointer<PresetBrowser> safe (this);
            juce::Thread::launch ([safe, text]
            {
                const auto json = bandlab::fetchPresetJson (text);
                juce::MessageManager::callAsync ([safe, json]
                {
                    if (safe == nullptr) return;
                    if (json.isEmpty())
                    {
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not download the preset",
                            "BandLab did not let the plug-in download that link (the preset may be private, or BandLab blocks this kind of request).\n\n"
                            "Open the preset link in your browser, save or copy the preset's JSON, then use Import > BandLab preset file, or copy the JSON and use Import > BandLab preset from clipboard.");
                        return;
                    }
                    juce::StringArray notes;
                    safe->reportBandLab (safe->proc.importBandLabJson (json, notes), notes);
                });
            });
            return;
        }
        juce::StringArray notes;
        reportBandLab (proc.importBandLabJson (text, notes), notes);
    }

    void askImportBandLabFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import BandLab preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json;*.txt");
        juce::Component::SafePointer<PresetBrowser> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr || fc.getResult() == juce::File()) return;
                                  safe->importBandLabText (fc.getResult().loadFileAsString());
                              });
    }

    void askImportFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Import preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mfxpreset;*.xml");
        juce::Component::SafePointer<PresetBrowser> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr || fc.getResult() == juce::File()) return;
                                  if (safe->proc.importPreset (fc.getResult()) >= 0) safe->close();
                                  else juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import failed", "That file is not a valid preset.");
                              });
    }

    ModularFXAudioProcessor& proc;
    juce::TextEditor search;
    juce::ListBox list;
    masterui::PaintedButton importButton, exportButton, closeButton;
    std::vector<ModularFXAudioProcessor::PresetInfo> all;
    std::vector<int> shown;                         // indices into 'all'
    juce::String tag { "All" };
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> chipRects;
    std::unique_ptr<juce::FileChooser> chooser;
};
