#include "PluginEditor.h"

PodEditor::PodEditor (PodProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      mixAttachment (p.apvts, "mix", mixSlider)
{
    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    mixSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xff2f7fe0));
    mixSlider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xffb8bcc4));
    mixSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible (mixSlider);

    setSize (320, 540);
    startTimerHz (30);
}

int PodEditor::currentMode() const
{
    return juce::jlimit (0, (int) podModes.size() - 1,
                         (int) std::lround (proc.apvts.getRawParameterValue ("mode")->load()));
}

bool PodEditor::isBypassed() const
{
    return proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
}

void PodEditor::stepMode (int dir)
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("mode"));
    if (p == nullptr) return;

    const int cur = p->getIndex();
    const int n = juce::jlimit (0, (int) podModes.size() - 1, cur + dir);
    if (n == cur) return;

    p->beginChangeGesture();
    *p = n;
    p->endChangeGesture();
    repaint();
}

void PodEditor::toggleBypass()
{
    if (auto* p = proc.apvts.getParameter ("bypass"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
    repaint();
}

float PodEditor::angleOf (juce::Point<float> p) const
{
    return std::atan2 (p.y - wheelC.y, p.x - wheelC.x);
}

void PodEditor::resized()
{
    screenR = { 32.0f, 34.0f, (float) getWidth() - 64.0f, 200.0f };
    wheelC = { (float) getWidth() * 0.5f, 395.0f };

    auto area = screenR.toNearestInt();
    auto bar = area.removeFromBottom (22);
    mixSlider.setBounds (bar.withTrimmedLeft (36).withTrimmedRight (10));
}

void PodEditor::paint (juce::Graphics& g)
{
    const bool byp = isBypassed();
    const int cur = currentMode();

    // iPod test: kitölti az egész ablakot, nincs sötét sarok
    auto body = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff6f6f7), 0.0f, body.getY(),
                                             juce::Colour (0xffc9cacd), 0.0f, body.getBottom(), false));
    g.fillRect (body);

    // kijelző keret
    g.setColour (juce::Colour (0xff111111));
    g.fillRoundedRectangle (screenR.expanded (6.0f), 10.0f);

    // kijelző
    g.saveState();
    g.reduceClipRegion (screenR.toNearestInt());
    g.setColour (juce::Colours::white);
    g.fillRect (screenR);

    // címsor
    auto title = screenR.withHeight (24.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0f2), 0.0f, title.getY(),
                                             juce::Colour (0xffb5b9c0), 0.0f, title.getBottom(), false));
    g.fillRect (title);
    g.setColour (juce::Colour (0xff6b6f76));
    g.drawLine (title.getX(), title.getBottom(), title.getRight(), title.getBottom());

    g.setColour (juce::Colour (0xff202225));
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText ("Sound Mode", title.toNearestInt(), juce::Justification::centred);

    if (byp)
    {
        g.setColour (juce::Colour (0xffd9342b));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText ("BYPASS", title.toNearestInt().withTrimmedLeft (8), juce::Justification::centredLeft);
    }

    // akku ikon
    auto batt = juce::Rectangle<float> (title.getRight() - 32.0f, title.getY() + 8.0f, 20.0f, 9.0f);
    g.setColour (juce::Colour (0xff3a3d42));
    g.drawRoundedRectangle (batt, 2.0f, 1.0f);
    g.setColour (juce::Colour (0xff3cc455));
    g.fillRoundedRectangle (batt.reduced (2.0f), 1.0f);
    g.setColour (juce::Colour (0xff3a3d42));
    g.fillRect (batt.getRight(), batt.getY() + 2.5f, 2.0f, 4.0f);

    // lista
    const float rowH = 22.0f;
    const float top = screenR.getY() + 24.0f;
    for (int i = 0; i < (int) podModes.size(); ++i)
    {
        auto row = juce::Rectangle<float> (screenR.getX(), top + rowH * (float) i, screenR.getWidth(), rowH);
        const bool sel = (i == cur);

        if (sel)
        {
            auto c1 = byp ? juce::Colour (0xffb0b2b8) : juce::Colour (0xff4a9bf0);
            auto c2 = byp ? juce::Colour (0xff8a8c92) : juce::Colour (0xff1f62c9);
            g.setGradientFill (juce::ColourGradient (c1, 0.0f, row.getY(), c2, 0.0f, row.getBottom(), false));
            g.fillRect (row);
            g.setColour (juce::Colours::white);
        }
        else
        {
            g.setColour (byp ? juce::Colour (0xff9a9ca2) : juce::Colours::black);
        }

        g.setFont (juce::Font (juce::FontOptions (14.0f, sel ? juce::Font::bold : juce::Font::plain)));
        g.drawText (podModes[(size_t) i].name, row.toNearestInt().withTrimmedLeft (10), juce::Justification::centredLeft);
        g.drawText (">", row.toNearestInt().withTrimmedRight (10), juce::Justification::centredRight);
    }

    // alsó sáv (mix)
    auto barR = screenR.withTop (screenR.getBottom() - 22.0f);
    g.setColour (juce::Colour (0xffeceef2));
    g.fillRect (barR);
    g.setColour (juce::Colour (0xffb0b4bb));
    g.drawLine (barR.getX(), barR.getY(), barR.getRight(), barR.getY());
    g.setColour (juce::Colour (0xff4a4d53));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("MIX", barR.toNearestInt().withTrimmedLeft (6).withWidth (30), juce::Justification::centredLeft);

    g.restoreState();

    // click wheel
    juce::Rectangle<float> outer (wheelC.x - outerR, wheelC.y - outerR, outerR * 2.0f, outerR * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeaeaec), 0.0f, outer.getY(),
                                             juce::Colour (0xffc2c3c8), 0.0f, outer.getBottom(), false));
    g.fillEllipse (outer);
    g.setColour (juce::Colour (0xff9d9da2));
    g.drawEllipse (outer, 1.5f);

    juce::Rectangle<float> inner (wheelC.x - innerR, wheelC.y - innerR, innerR * 2.0f, innerR * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff9f9fa), 0.0f, inner.getY(),
                                             juce::Colour (0xffd9dadd), 0.0f, inner.getBottom(), false));
    g.fillEllipse (inner);
    g.setColour (juce::Colour (0xffa5a5aa));
    g.drawEllipse (inner, 1.5f);

    // státusz LED a közepén
    g.setColour (byp ? juce::Colour (0xff9a9ca2) : juce::Colour (0xff3cc455));
    g.fillEllipse (wheelC.x - 4.0f, wheelC.y - 4.0f, 8.0f, 8.0f);

    g.setColour (juce::Colour (0xff6e6e73));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("MENU", juce::Rectangle<float> (wheelC.x - 30.0f, wheelC.y - outerR + 14.0f, 60.0f, 20.0f).toNearestInt(), juce::Justification::centred);
    g.drawText ("<<", juce::Rectangle<float> (wheelC.x - outerR + 8.0f, wheelC.y - 10.0f, 36.0f, 20.0f).toNearestInt(), juce::Justification::centred);
    g.drawText (">>", juce::Rectangle<float> (wheelC.x + outerR - 44.0f, wheelC.y - 10.0f, 36.0f, 20.0f).toNearestInt(), juce::Justification::centred);
    g.drawText (">||", juce::Rectangle<float> (wheelC.x - 30.0f, wheelC.y + outerR - 34.0f, 60.0f, 20.0f).toNearestInt(), juce::Justification::centred);
}

void PodEditor::mouseDown (const juce::MouseEvent& e)
{
    const float d = e.position.getDistanceFrom (wheelC);
    drag = Drag::none;

    if (d <= innerR)
    {
        drag = Drag::centre;
    }
    else if (d <= outerR)
    {
        drag = Drag::ring;
        lastAngle = angleOf (e.position);
        accum = 0.0f;
        moved = false;
    }
}

void PodEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (drag != Drag::ring) return;

    const float pi = juce::MathConstants<float>::pi;
    const float a = angleOf (e.position);
    float delta = a - lastAngle;
    if (delta > pi)       delta -= 2.0f * pi;
    else if (delta < -pi) delta += 2.0f * pi;
    lastAngle = a;
    accum += delta;

    if (e.getDistanceFromDragStart() > 4)
        moved = true;

    const float step = 0.45f; // ~26 fok = egy lépés a listában
    while (accum >  step) { stepMode (+1); accum -= step; }
    while (accum < -step) { stepMode (-1); accum += step; }
}

void PodEditor::mouseUp (const juce::MouseEvent& e)
{
    if (drag == Drag::centre && e.position.getDistanceFrom (wheelC) <= innerR)
    {
        toggleBypass();
    }
    else if (drag == Drag::ring && ! moved)
    {
        auto d = e.position - wheelC;
        if (std::abs (d.x) > std::abs (d.y))
            stepMode (d.x < 0 ? -1 : +1);
    }
    drag = Drag::none;
}

void PodEditor::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheelAccum += w.deltaY;
    while (wheelAccum >=  0.15f) { stepMode (-1); wheelAccum -= 0.15f; }
    while (wheelAccum <= -0.15f) { stepMode (+1); wheelAccum += 0.15f; }
}
