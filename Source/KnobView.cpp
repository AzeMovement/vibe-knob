#include "KnobView.h"

namespace
{
    constexpr float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    constexpr float endAngle = juce::MathConstants<float>::pi * 0.75f;

    void setParameterNotifying (juce::ParameterAttachment& a, float v, bool& inGesture)
    {
        if (inGesture)
            a.setValueAsPartOfGesture (v);
        else
            a.setValueAsCompleteGesture (v);
    }
}

KnobView::KnobView (VibeKnobProcessor& p)
    : processor (p),
      parameter (*p.params.getParameter ("vibe")),
      attachment (parameter, [this] (float v) { value = v; repaint(); })
{
    attachment.sendInitialUpdate();
    setRepaintsOnMouseActivity (false);
}

juce::Point<float> KnobView::centre() const
{
    return getLocalBounds().toFloat().getCentre().translated (0.0f, 14.0f);
}

float KnobView::radius() const
{
    return juce::jmin ((float) getWidth() * 0.3f, (float) getHeight() * 0.37f);
}

float KnobView::angleFor (float position) const
{
    return startAngle + juce::jlimit (0.0f, 1.0f, position) * (endAngle - startAngle);
}

juce::Rectangle<float> KnobView::labelArea (int index, int count) const
{
    const auto a = angleFor (ring::positionOf (index, count));
    const auto p = centre().getPointOnCircumference (radius() * 1.27f, a);

    // Labels sit outside the ring: pushed left on the left side, right on the right.
    const auto w = 86.0f, h = 20.0f;
    const auto side = std::sin (a);
    return juce::Rectangle<float> (w, h).withCentre (p.translated (side * w * 0.42f, -std::cos (a) * 4.0f));
}

void KnobView::paint (juce::Graphics& g)
{
    const auto& moods = processor.getRing();
    const auto n = (int) moods.size();
    const auto c = centre();
    const auto r = radius();
    const auto now = processor.getKnobNow();
    const auto colour = processor.isTeaching() ? style::text : ring::colourAt (moods, value);

    // Glow in the colour of the mood.
    {
        juce::ColourGradient glow (colour.withAlpha (0.28f), c, colour.withAlpha (0.0f), c.translated (r * 1.45f, 0.0f), true);
        g.setGradientFill (glow);
        g.fillEllipse (juce::Rectangle<float> (r * 2.9f, r * 2.9f).withCentre (c));
    }

    // The ring: every mood's colour along the way, bright up to where the knob is.
    const auto ringRadius = r * 1.06f;
    const juce::PathStrokeType stroke (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt);
    constexpr int steps = 120;
    for (int i = 0; i < steps; ++i)
    {
        const auto t0 = (float) i / steps, t1 = (float) (i + 1) / steps;
        juce::Path seg;
        seg.addCentredArc (c.x, c.y, ringRadius, ringRadius, 0.0f, angleFor (t0), angleFor (t1) + 0.004f, true);
        const auto segColour = ring::colourAt (moods, (t0 + t1) * 0.5f);
        g.setColour (t1 <= value + 0.001f ? segColour : segColour.withAlpha (0.18f));
        g.strokePath (seg, stroke);
    }

    // Where each mood sits.
    for (int i = 0; i < n; ++i)
    {
        const auto p = c.getPointOnCircumference (ringRadius, angleFor (ring::positionOf (i, n)));
        g.setColour (style::background);
        g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (p));
        g.setColour (moods[(size_t) i].colour);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (p));
    }

    // Where motion has taken it, if that's somewhere else.
    if (std::abs (now - value) > 0.004f && ! processor.isTeaching())
    {
        const auto p = c.getPointOnCircumference (ringRadius, angleFor (now));
        g.setColour (style::text);
        g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (p), 2.0f);
    }

    // The knob itself.
    const auto body = juce::Rectangle<float> (r * 1.7f, r * 1.7f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 6.0f).expanded (2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2f39), body.getCentreX(), body.getY(),
                                             juce::Colour (0xff131519), body.getCentreX(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (colour.withAlpha (0.35f));
    g.drawEllipse (body, 1.5f);

    const auto a = angleFor (value);
    g.setColour (colour);
    juce::Path pointer;
    pointer.startNewSubPath (c.getPointOnCircumference (r * 0.6f, a));
    pointer.lineTo (c.getPointOnCircumference (r * 0.8f, a));
    g.strokePath (pointer, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // The mood's name in the middle.
    juce::String name, detail;
    if (processor.isTeaching())
    {
        name = "Teaching";
        detail = "knob paused";
    }
    else if (n > 0)
    {
        const auto b = ring::locate (value, n);
        const auto nearest = b.t < 0.5f ? b.from : b.to;
        name = moods[(size_t) nearest].name;
        if (b.t > 0.12f && b.t < 0.88f)
            detail = juce::String (juce::roundToInt ((1.0f - b.t) * 100.0f)) + "% " + moods[(size_t) b.from].name
                     + "  /  " + juce::String (juce::roundToInt (b.t * 100.0f)) + "% " + moods[(size_t) b.to].name;
    }

    g.setColour (colour);
    g.setFont (style::font (juce::jlimit (16.0f, 30.0f, r * 0.2f), true));
    g.drawFittedText (name, body.reduced (r * 0.2f, 0.0f).withSizeKeepingCentre (r * 1.3f, r * 0.3f).toNearestInt(), juce::Justification::centred, 1);
    g.setColour (style::quietText);
    g.setFont (style::font (11.0f));
    g.drawFittedText (detail, body.withSizeKeepingCentre (r * 1.4f, 16.0f).translated (0.0f, r * 0.24f).toNearestInt(), juce::Justification::centred, 1);

    // Mood names around the edge.
    for (int i = 0; i < n; ++i)
    {
        const auto weight = 1.0f - juce::jmin (1.0f, std::abs (value - ring::positionOf (i, n)) * (float) (n - 1));
        const auto area = labelArea (i, n);
        g.setColour (weight > 0.5f ? moods[(size_t) i].colour : style::quietText.interpolatedWith (style::text, weight));
        g.setFont (style::font (13.0f, weight > 0.5f));
        const auto side = std::sin (angleFor (ring::positionOf (i, n)));
        g.drawText (moods[(size_t) i].name, area,
                    side < -0.2f ? juce::Justification::centredRight : (side > 0.2f ? juce::Justification::centredLeft : juce::Justification::centred));
    }
}

void KnobView::mouseDown (const juce::MouseEvent& e)
{
    const auto n = (int) processor.getRing().size();
    for (int i = 0; i < n; ++i)
        if (labelArea (i, n).contains (e.position))
        {
            attachment.setValueAsCompleteGesture (ring::positionOf (i, n));
            return;
        }

    dragging = true;
    dragStart = value;
    attachment.beginGesture();
}

void KnobView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    // Up or right turns it up. Hold Shift for fine moves.
    const auto distance = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    const auto scale = e.mods.isShiftDown() ? 1500.0f : 300.0f;
    attachment.setValueAsPartOfGesture (juce::jlimit (0.0f, 1.0f, dragStart + distance / scale));
}

void KnobView::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        attachment.endGesture();
    dragging = false;
}

void KnobView::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (parameter.getDefaultValue()));
}

void KnobView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto step = (wheel.deltaY + wheel.deltaX) * (wheel.isReversed ? -0.1f : 0.1f);
    attachment.setValueAsCompleteGesture (juce::jlimit (0.0f, 1.0f, value + step));
}

//==============================================================================
PadView::PadView (VibeKnobProcessor& p, MoodLibrary& l)
    : processor (p), library (l),
      xAttachment (*p.params.getParameter ("padx"), [this] (float v) { x = v; repaint(); }),
      yAttachment (*p.params.getParameter ("pady"), [this] (float v) { y = v; repaint(); })
{
    xAttachment.sendInitialUpdate();
    yAttachment.sendInitialUpdate();
}

juce::Rectangle<float> PadView::padArea() const
{
    const auto side = (float) juce::jmin (getWidth() - 20, getHeight() - 20);
    return getLocalBounds().toFloat().withSizeKeepingCentre (side, side);
}

juce::Rectangle<float> PadView::cornerLabel (int corner) const
{
    const auto area = padArea().reduced (8.0f);
    const auto w = 110.0f, h = 22.0f;
    const auto left = corner % 2 == 0;
    const auto top = corner < 2;
    return { left ? area.getX() : area.getRight() - w, top ? area.getY() : area.getBottom() - h, w, h };
}

void PadView::paint (juce::Graphics& g)
{
    const auto area = padArea();
    const auto& corners = processor.getCorners();
    const std::array<juce::Colour, 4> colours { corners[0].colour, corners[1].colour, corners[2].colour, corners[3].colour };

    // Every blend of the four moods, as colour.
    juce::Image wash (juce::Image::RGB, 24, 24, false);
    for (int j = 0; j < 24; ++j)
        for (int i = 0; i < 24; ++i)
            wash.setPixelAt (i, j, pad::colourAt (colours, (float) i / 23.0f, 1.0f - (float) j / 23.0f)
                                       .interpolatedWith (style::background, 0.62f));

    juce::Path clip;
    clip.addRoundedRectangle (area, 14.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (clip);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (wash, area);

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (int i = 1; i < 4; ++i)
        {
            g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * (float) i / 4.0f), area.getY(), area.getBottom());
            g.drawHorizontalLine (juce::roundToInt (area.getY() + area.getHeight() * (float) i / 4.0f), area.getX(), area.getRight());
        }
    }
    g.setColour (style::track);
    g.drawRoundedRectangle (area, 14.0f, 1.0f);

    for (int i = 0; i < 4; ++i)
    {
        g.setColour (colours[(size_t) i]);
        g.setFont (style::font (13.0f, true));
        g.drawText (corners[(size_t) i].name + (edition::isPro ? juce::String (" ") + juce::String::charToString (0x25be) : juce::String()),
                    cornerLabel (i), i % 2 == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight);
    }

    const auto now = processor.getPadNow();
    auto point = [&] (float px, float py) { return juce::Point<float> (area.getX() + px * area.getWidth(), area.getBottom() - py * area.getHeight()); };
    const auto colour = pad::colourAt (colours, x, y);

    if (now.getDistanceFrom ({ x, y }) > 0.004f)
    {
        g.setColour (style::text.withAlpha (0.8f));
        g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (point (now.x, now.y)), 2.0f);
    }

    const auto puck = juce::Rectangle<float> (24.0f, 24.0f).withCentre (point (x, y));
    g.setColour (colour.withAlpha (0.3f));
    g.fillEllipse (puck.expanded (10.0f));
    g.setColour (colour);
    g.fillEllipse (puck);
    g.setColour (style::text);
    g.drawEllipse (puck, 2.0f);
}

void PadView::moveTo (juce::Point<float> p)
{
    const auto area = padArea();
    const auto nx = juce::jlimit (0.0f, 1.0f, (p.x - area.getX()) / area.getWidth());
    const auto ny = juce::jlimit (0.0f, 1.0f, (area.getBottom() - p.y) / area.getHeight());
    setParameterNotifying (xAttachment, nx, dragging);
    setParameterNotifying (yAttachment, ny, dragging);
}

void PadView::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 4; ++i)
        if (cornerLabel (i).contains (e.position))
        {
            if (! edition::isPro)
                return;

            juce::PopupMenu menu;
            menu.addSectionHeader ("Mood in this corner");
            const auto moods = library.available();
            for (size_t m = 0; m < moods.size(); ++m)
                menu.addItem ((int) m + 1, moods[m].name, true, moods[m].name == processor.getCorners()[(size_t) i].name);

            juce::Component::SafePointer<PadView> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (cornerLabel (i).toNearestInt())),
                                [safe, moods, i] (int chosen)
                                {
                                    if (safe != nullptr && chosen > 0)
                                    {
                                        safe->processor.setCorner (i, moods[(size_t) chosen - 1]);
                                        safe->repaint();
                                    }
                                });
            return;
        }

    dragging = true;
    xAttachment.beginGesture();
    yAttachment.beginGesture();
    moveTo (e.position);
}

void PadView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        moveTo (e.position);
}

void PadView::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        xAttachment.endGesture();
        yAttachment.endGesture();
    }
    dragging = false;
}
