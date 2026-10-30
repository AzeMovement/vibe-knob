#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace style
{
    inline const juce::Colour background { 0xff0f1014 };
    inline const juce::Colour panel      { 0xff17191f };
    inline const juce::Colour track      { 0xff2a2e37 };
    inline const juce::Colour text       { 0xffe6e8ee };
    inline const juce::Colour quietText  { 0xff7f8794 };
    inline const juce::Colour accent     { 0xffffa646 };

    inline juce::Font font (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Flat, dark controls. Sliders and toggles take their colour from the mood that's playing.
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, background);
            setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::Slider::trackColourId, accent);
            setColour (juce::TooltipWindow::backgroundColourId, panel);
            setColour (juce::TooltipWindow::textColourId, text);
            setColour (juce::TooltipWindow::outlineColourId, track);
            setColour (juce::ComboBox::backgroundColourId, panel);
            setColour (juce::ComboBox::outlineColourId, track);
            setColour (juce::ComboBox::textColourId, text);
            setColour (juce::ComboBox::arrowColourId, quietText);
            setColour (juce::PopupMenu::backgroundColourId, panel);
            setColour (juce::PopupMenu::textColourId, text);
            setColour (juce::PopupMenu::headerTextColourId, quietText);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, track);
            setColour (juce::PopupMenu::highlightedTextColourId, text);
            setColour (juce::AlertWindow::backgroundColourId, panel);
            setColour (juce::AlertWindow::textColourId, text);
            setColour (juce::AlertWindow::outlineColourId, track);
            setColour (juce::TextEditor::backgroundColourId, background);
            setColour (juce::TextEditor::textColourId, text);
            setColour (juce::TextEditor::outlineColourId, track);
            setColour (juce::TextEditor::focusedOutlineColourId, accent);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                   bool isHighlighted, bool isDown) override
        {
            const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
            const auto corner = bounds.getHeight() * 0.5f;

            if (button.getToggleState())
            {
                g.setColour (button.findColour (juce::TextButton::buttonOnColourId));
                g.fillRoundedRectangle (bounds, corner);
                return;
            }

            if (! button.isEnabled())
            {
                g.setColour (track.withAlpha (0.6f));
                g.drawRoundedRectangle (bounds, corner, 1.0f);
                return;
            }

            g.setColour (isDown ? track.brighter (0.2f) : (isHighlighted ? track : panel));
            g.fillRoundedRectangle (bounds, corner);
            g.setColour (isHighlighted ? quietText : track);
            g.drawRoundedRectangle (bounds, corner, 1.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
        {
            g.setFont (font (12.0f, true));
            g.setColour (button.getToggleState() ? background
                                                 : (button.isEnabled() ? text : quietText.withAlpha (0.6f)));
            g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
        }

        // Small knobs: a thin arc that fills from the left (or from the middle for +/- controls).
        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            const auto area = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
            const auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
            const auto centre = area.getCentre();
            const auto colour = slider.findColour (juce::Slider::trackColourId);
            const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
            const auto angle = startAngle + pos * (endAngle - startAngle);
            const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

            juce::Path back, value;
            back.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, startAngle, endAngle, true);
            value.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);

            const juce::PathStrokeType stroke (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (track);
            g.strokePath (back, stroke);
            g.setColour (slider.isEnabled() ? colour : quietText);
            g.strokePath (value, stroke);

            g.setColour (panel.brighter (0.15f));
            g.fillEllipse (juce::Rectangle<float> (radius * 1.2f, radius * 1.2f).withCentre (centre));
            const auto tip = centre.getPointOnCircumference (radius * 0.5f, angle);
            g.setColour (text);
            g.drawLine ({ centre.getPointOnCircumference (radius * 0.18f, angle), tip }, 2.0f);
        }

        // Thin bars with the value drawn from the setting's resting point.
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                               float, float, juce::Slider::SliderStyle, juce::Slider& slider) override
        {
            const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
            const auto bar = area.withSizeKeepingCentre (area.getWidth(), 4.0f);
            const auto rest = (float) slider.getProperties().getWithDefault ("rest", slider.getMinimum());
            const auto restX = (float) slider.getPositionOfValue (rest);
            const auto colour = slider.findColour (juce::Slider::trackColourId);

            g.setColour (track);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (slider.isEnabled() ? colour : colour.withMultipliedSaturation (0.6f).withAlpha (0.75f));
            g.fillRoundedRectangle (bar.withLeft (juce::jmin (restX, sliderPos)).withRight (juce::jmax (restX, sliderPos) + 0.5f), 2.0f);

            if (slider.isEnabled())
            {
                g.setColour (text);
                g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ sliderPos, area.getCentreY() }));
            }
        }

        void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
        {
            const auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
            g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (box.findColour (juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

            juce::Path arrow;
            const auto cx = (float) width - 12.0f, cy = (float) height * 0.5f;
            arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
            g.setColour (box.findColour (juce::ComboBox::arrowColourId));
            g.fillPath (arrow);
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
        juce::Font getPopupMenuFont() override { return font (13.0f); }
    };
}
