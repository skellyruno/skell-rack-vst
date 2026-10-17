#pragma once
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>
#include "FxRegistry.h"
#include "FxCatalog.h"

// The "Add Effect" browser: a category list on the left, a searchable and scrollable list of effects on the right.
namespace fxbrowser
{
    const juce::Colour kPanel  { 0xFF0F1013 };
    const juce::Colour kBorder { 0xFF24262B };
    const juce::Colour kHover  { 0xFF1B1D22 };
    const juce::Colour kGrey   { 0xFF8B93A1 };

    // Little mock-up of the effect's card, drawn from its parameter list.
    inline void drawThumb (juce::Graphics& g, const FxInfo& info, juce::Rectangle<float> r)
    {
        if (info.id == ID_OneKnob)
        {
            g.setColour (juce::Colour (0xFFFFB703));
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (juce::Colours::black.withAlpha(0.85f));
            g.fillRoundedRectangle (r.withSizeKeepingCentre (r.getWidth() * 0.34f, 4.0f).withY (r.getY() + 8.0f), 2.0f);
            const auto k = r.withSizeKeepingCentre (r.getWidth() * 0.5f, r.getWidth() * 0.5f).translated (0.0f, 4.0f);
            g.fillEllipse (k);
            g.setColour (juce::Colours::white);
            g.drawLine (k.getCentreX(), k.getCentreY(), k.getRight() - 6.0f, k.getCentreY() - 7.0f, 2.0f);
            return;
        }

        g.setColour (juce::Colour (0xFF22252B));
        g.fillRoundedRectangle (r, 9.0f);
        const auto card = r.reduced (r.getWidth() * 0.13f, r.getHeight() * 0.1f);
        g.setColour (info.bg.interpolatedWith (juce::Colour (0xFF16181C), 0.55f));
        g.fillRoundedRectangle (card, 5.0f);

        g.setColour (info.accent.withAlpha (0.8f));
        g.fillRoundedRectangle (card.getX() + 6.0f, card.getY() + 6.0f, card.getWidth() * 0.4f, 2.5f, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRoundedRectangle (card.getX() + 6.0f, card.getY() + 11.0f, card.getWidth() * 0.25f, 2.5f, 1.2f);

        auto body = card.reduced (6.0f, 0.0f).withTrimmedTop (18.0f).withTrimmedBottom (5.0f);

        if (info.display == DisplayKind::EqCurve)
        {
            auto d = body.removeFromTop (body.getHeight() * 0.38f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (d, 3.0f);
            juce::Path p;  p.startNewSubPath (d.getX() + 3.0f, d.getCentreY());
            p.cubicTo (d.getCentreX() - 8.0f, d.getCentreY() - 7.0f, d.getCentreX() + 2.0f, d.getCentreY() + 7.0f, d.getRight() - 3.0f, d.getCentreY());
            g.setColour (info.accent);  g.strokePath (p, juce::PathStrokeType (1.4f));
            body.removeFromTop (3.0f);
        }
        else if (info.display == DisplayKind::GainReduction)
        {
            auto d = body.removeFromTop (5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));  g.fillRoundedRectangle (d, 2.0f);
            g.setColour (info.accent);  g.fillRoundedRectangle (d.withWidth (d.getWidth() * 0.45f), 2.0f);
            body.removeFromTop (4.0f);
        }

        int knobs = 0, others = 0;
        for (const auto& s : info.params) (s.kind == ParamKind::Knob ? knobs : others)++;
        knobs = juce::jlimit (1, 8, knobs);
        const int cols = knobs <= 2 ? knobs : (knobs <= 4 ? 2 : 3);
        const int rows = (knobs + cols - 1) / cols;
        const float cell = juce::jmin (body.getWidth() / (float) cols, (body.getHeight() - (others > 0 ? 7.0f : 0.0f)) / (float) rows);
        const float dia = juce::jmin (cell * 0.72f, 15.0f);
        const float gridW = cell * (float) cols, gridH = cell * (float) rows;
        const float x0 = body.getCentreX() - gridW * 0.5f;
        const float y0 = body.getY() + juce::jmax (0.0f, (body.getHeight() - (others > 0 ? 7.0f : 0.0f) - gridH) * 0.5f);
        for (int i = 0; i < knobs; ++i)
        {
            const int rr = i / cols, cc = i % cols;
            const int inRow = juce::jmin (cols, knobs - rr * cols);
            const float off = (float) (cols - inRow) * cell * 0.5f;
            const float cx = x0 + off + ((float) cc + 0.5f) * cell, cy = y0 + ((float) rr + 0.5f) * cell;
            g.setColour (juce::Colour (0xFF8E949E));
            g.fillEllipse (cx - dia * 0.5f, cy - dia * 0.5f, dia, dia);
            g.setColour (juce::Colour (0xFF16181C));
            g.drawLine (cx, cy, cx + dia * 0.18f, cy - dia * 0.4f, 1.3f);
        }
        if (others > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.fillRoundedRectangle (card.getCentreX() - card.getWidth() * 0.28f, card.getBottom() - 9.0f, card.getWidth() * 0.56f, 4.0f, 2.0f);
        }
    }
}

class EffectBrowser : public juce::Component, private juce::TextEditor::Listener
{
public:
    std::function<void (int)> onPick;
    std::function<void()> onClose;

    EffectBrowser()
    {
        setInterceptsMouseClicks (true, true);

        search.setTextToShowWhenEmpty ("Search", fxbrowser::kGrey);
        search.setFont (juce::Font (juce::FontOptions (16.0f)));
        search.setIndents (38, 0);
        search.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1C1E24));
        search.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        search.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        search.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFF3B82F6));
        search.setColour (juce::TextEditor::highlightColourId, juce::Colour (0xFF3B82F6).withAlpha (0.4f));
        search.setJustification (juce::Justification::centredLeft);
        search.addListener (this);
        addAndMakeVisible (search);

        sideViewport.setViewedComponent (&side, false);
        sideViewport.setScrollBarsShown (true, false);
        sideViewport.setScrollBarThickness (8);
        addAndMakeVisible (sideViewport);

        listViewport.setViewedComponent (&list, false);
        listViewport.setScrollBarsShown (true, false);
        listViewport.setScrollBarThickness (8);
        listViewport.onScroll = [this] { syncSelectionToScroll(); };
        addAndMakeVisible (listViewport);

        for (auto* v : { &sideViewport, static_cast<juce::Viewport*> (&listViewport) })
        {
            v->getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xFF4A4F58));
            v->getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, juce::Colour (0xFF1B1D22));
        }

        side.owner = this;
        setVisible (false);
    }

    void open()
    {
        search.setText ("", false);
        buildList();
        setVisible (true);
        toFront (true);
        listViewport.setViewPosition (0, 0);
        selectedCategory = 0;
        side.repaint();
        search.grabKeyboardFocus();
    }

    void setSearchText (const juce::String& t)       // also used by the tests
    {
        search.setText (t, false);
        buildList();
        listViewport.setViewPosition (0, 0);
        selectedCategory = 0;
        side.repaint();
    }

    void close()
    {
        setVisible (false);
        if (onClose) onClose();
    }

    juce::Rectangle<int> getPanelBounds() const
    {
        const int w = juce::jmin (720, getWidth() - 24), h = juce::jmin (404, getHeight() - 12);
        return { (getWidth() - w) / 2, 6, w, h };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        const auto p = getPanelBounds().toFloat();
        g.setColour (fxbrowser::kPanel);
        g.fillRoundedRectangle (p, 12.0f);
        g.setColour (fxbrowser::kBorder);
        g.drawRoundedRectangle (p, 12.0f, 1.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        g.drawText ("Effects", (int) p.getX() + 24, (int) p.getY() + 8, 160, 44, juce::Justification::centredLeft);

        g.setColour (fxbrowser::kBorder);
        g.drawHorizontalLine ((int) p.getY() + kHeaderH, p.getX(), p.getRight());
        g.drawVerticalLine ((int) p.getX() + kSideW, p.getY() + (float) kHeaderH, p.getBottom());
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // magnifier in the search box
        const auto s = search.getBounds().toFloat();
        g.setColour (fxbrowser::kGrey);
        const float cx = s.getX() + 18.0f, cy = s.getCentreY() - 1.0f;
        g.drawEllipse (cx - 6.0f, cy - 6.0f, 11.0f, 11.0f, 1.8f);
        g.drawLine (cx + 3.0f, cy + 4.0f, cx + 8.0f, cy + 9.0f, 1.8f);
    }

    void resized() override
    {
        const auto p = getPanelBounds();
        search.setBounds (p.getX() + kSideW + 14, p.getY() + 10, p.getWidth() - kSideW - 38, 38);
        sideViewport.setBounds (p.getX() + 6, p.getY() + kHeaderH + 6, kSideW - 8, p.getHeight() - kHeaderH - 14);
        listViewport.setBounds (p.getX() + kSideW + 1, p.getY() + kHeaderH + 1, p.getWidth() - kSideW - 2, p.getHeight() - kHeaderH - 8);
        layoutSide();
        layoutList();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! getPanelBounds().contains (e.getPosition())) close();
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        return false;
    }

private:
    static constexpr int kHeaderH = 58, kSideW = 190, kRowH = 98, kThumb = 78, kSectionH = 76;

    // ------------------------------------------------ left: categories
    struct Side : juce::Component
    {
        EffectBrowser* owner = nullptr;
        std::vector<juce::String> names;

        void paint (juce::Graphics& g) override
        {
            for (size_t i = 0; i < names.size(); ++i)
            {
                const bool sel = owner != nullptr && owner->selectedCategory == (int) i;
                g.setColour (sel ? juce::Colours::white : fxbrowser::kGrey);
                g.setFont (juce::Font (juce::FontOptions (16.0f, sel ? juce::Font::bold : juce::Font::plain)));
                g.drawText (names[i], 18, (int) i * kCatH, getWidth() - 20, kCatH, juce::Justification::centredLeft);
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const int i = e.y / kCatH;
            if (owner != nullptr && i >= 0 && i < (int) names.size()) owner->categoryClicked (i);
        }
        static constexpr int kCatH = 40;
    };

    // ------------------------------------------------ right: effects
    struct Row : juce::Component
    {
        int id = 0;
        const FxInfo* info = nullptr;
        juce::String tagline;
        std::function<void (int)> onClick;
        bool hover = false;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (4.0f, 2.0f);
            if (hover) { g.setColour (fxbrowser::kHover); g.fillRoundedRectangle (r, 10.0f); }
            if (info == nullptr) return;
            fxbrowser::drawThumb (g, *info, juce::Rectangle<float> (r.getX() + 8.0f, r.getCentreY() - (float) kThumb * 0.5f, (float) kThumb, (float) kThumb));
            const float tx = r.getX() + 8.0f + (float) kThumb + 16.0f;
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
            g.drawText (info->name, juce::Rectangle<float> (tx, r.getY() + 14.0f, r.getRight() - tx - 40.0f, 24.0f), juce::Justification::centredLeft);
            g.setColour (fxbrowser::kGrey);
            g.setFont (juce::Font (juce::FontOptions (14.0f)));
            g.drawFittedText (tagline, juce::Rectangle<float> (tx, r.getY() + 40.0f, r.getRight() - tx - 40.0f, 40.0f).toNearestInt(), juce::Justification::topLeft, 2);
            if (hover)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                const float cx = r.getRight() - 22.0f, cy = r.getCentreY();
                g.drawLine (cx - 7.0f, cy, cx + 7.0f, cy, 2.0f);
                g.drawLine (cx, cy - 7.0f, cx, cy + 7.0f, 2.0f);
            }
        }
        void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick (id); }
    };

    struct Section { juce::String title, blurb; int y = 0; int category = -1; };

    struct ListContent : juce::Component
    {
        std::vector<Section> sections;
        std::vector<std::unique_ptr<Row>> rows;
        juce::String emptyText;

        void paint (juce::Graphics& g) override
        {
            for (const auto& s : sections)
            {
                g.setColour (juce::Colours::white);
                g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
                g.drawText (s.title, 18, s.y + 6, getWidth() - 36, 30, juce::Justification::centredLeft);
                g.setColour (fxbrowser::kGrey);
                g.setFont (juce::Font (juce::FontOptions (14.0f)));
                g.drawFittedText (s.blurb, 18, s.y + 38, getWidth() - 36, 40, juce::Justification::topLeft, 2);
            }
        }
    };

    struct TrackingViewport : juce::Viewport
    {
        std::function<void()> onScroll;
        void visibleAreaChanged (const juce::Rectangle<int>& a) override { juce::Viewport::visibleAreaChanged (a); if (onScroll) onScroll(); }
    };

    void textEditorTextChanged (juce::TextEditor&) override { buildList(); listViewport.setViewPosition (0, 0); selectedCategory = 0; side.repaint(); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { close(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override
    {
        if (! list.rows.empty() && onPick) { const int id = list.rows.front()->id; setVisible (false); onPick (id); if (onClose) onClose(); }
    }

    void pick (int id)
    {
        setVisible (false);
        if (onPick) onPick (id);
        if (onClose) onClose();
    }

    void categoryClicked (int i)
    {
        if (search.getText().isNotEmpty()) { search.setText ("", false); buildList(); }
        for (const auto& s : list.sections)
            if (s.category == i) { listViewport.setViewPosition (0, s.y); break; }
        selectedCategory = i;
        side.repaint();
    }

    void syncSelectionToScroll()
    {
        if (search.getText().isNotEmpty()) return;
        const int y = listViewport.getViewPositionY() + 12;
        int cat = 0;
        for (const auto& s : list.sections) if (s.y <= y) cat = s.category;
        // at the very bottom the last category wins
        if (listViewport.getViewPositionY() + listViewport.getViewHeight() >= list.getHeight() - 2 && ! list.sections.empty())
            cat = list.sections.back().category;
        if (cat != selectedCategory) { selectedCategory = cat; side.repaint(); }
    }

    void layoutSide()
    {
        side.names.clear();
        for (const auto& c : getFxCategories()) side.names.push_back (c.name);
        side.setSize (sideViewport.getMaximumVisibleWidth(), (int) side.names.size() * Side::kCatH + 8);
    }

    void buildList()
    {
        list.removeAllChildren();
        list.rows.clear();
        list.sections.clear();

        const juce::String q = search.getText().trim();
        int y = 0;

        auto addRow = [&] (const FxCatalogEntry& e)
        {
            auto row = std::make_unique<Row>();
            row->id = e.id;  row->info = getFxInfo (e.id);  row->tagline = e.tagline;
            row->onClick = [this] (int id) { pick (id); };
            row->setBounds (0, y, 10, kRowH);
            list.addAndMakeVisible (*row);
            list.rows.push_back (std::move (row));
            y += kRowH;
        };

        if (q.isEmpty())
        {
            const auto& cats = getFxCategories();
            for (int ci = 0; ci < (int) cats.size(); ++ci)
            {
                bool any = false;
                for (const auto& e : getFxCatalog())
                    if (e.category == cats[(size_t) ci].name)
                    {
                        if (! any) { list.sections.push_back ({ cats[(size_t) ci].name, cats[(size_t) ci].blurb, y, ci }); y += kSectionH; any = true; }
                        addRow (e);
                    }
                y += 14;
            }
            y += juce::jmax (0, listViewport.getViewHeight() - 260);       // let the last category scroll to the top
        }
        else
        {
            int count = 0;
            for (const auto& e : getFxCatalog())
            {
                const auto* info = getFxInfo (e.id);
                if (info == nullptr) continue;
                if (info->name.containsIgnoreCase (q) || e.tagline.containsIgnoreCase (q) || e.category.containsIgnoreCase (q))
                    ++count;
            }
            list.sections.push_back ({ count > 0 ? "Results" : "No effects found", count > 0 ? "Effects matching \"" + q + "\"." : "Try a different word, like reverb, delay or pitch.", 0, -1 });
            y = kSectionH;
            for (const auto& e : getFxCatalog())
            {
                const auto* info = getFxInfo (e.id);
                if (info == nullptr) continue;
                if (info->name.containsIgnoreCase (q) || e.tagline.containsIgnoreCase (q) || e.category.containsIgnoreCase (q))
                    addRow (e);
            }
        }
        list.setSize (juce::jmax (10, listViewport.getMaximumVisibleWidth()), y + 8);
        layoutRows();
        list.repaint();
    }

    void layoutRows()
    {
        for (auto& r : list.rows) r->setSize (list.getWidth(), kRowH);
    }

    void layoutList()
    {
        if (list.getWidth() != listViewport.getMaximumVisibleWidth())
        {
            list.setSize (juce::jmax (10, listViewport.getMaximumVisibleWidth()), list.getHeight());
            layoutRows();
        }
    }

    juce::TextEditor search;
    Side side;
    ListContent list;
    juce::Viewport sideViewport;
    TrackingViewport listViewport;
    int selectedCategory = 0;
};
