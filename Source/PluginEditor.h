#pragma once

#include "PluginProcessor.h"
#include <array>
#include <memory>

//==============================================================================
// Potards RGB : l'arc est un arc-en-ciel qui change de couleurs quand on tourne
class RgbKnobLookAndFeel : public LookAndFeel_V4
{
public:
    float animT = 0.0f;     // avance en continu (mis a jour par l'editeur)

    RgbKnobLookAndFeel()
    {
        setColour (ComboBox::backgroundColourId, Colour (0xff2a0f4a));
        setColour (ComboBox::textColourId, Colours::white);
        setColour (ComboBox::outlineColourId, Colour (0xffff4fd8));
        setColour (ComboBox::arrowColourId, Colour (0xffffe14d));
        setColour (PopupMenu::backgroundColourId, Colour (0xff1b0a33));
        setColour (PopupMenu::textColourId, Colours::white);
        setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xffff4fd8));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, Slider& slider) override
    {
        const bool active = slider.isMouseButtonDown();
        const bool hover  = slider.isMouseOver();

        auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (14.0f);
        const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  c = bounds.getCentre();
        const float angEnd = startAngle + pos * (endAngle - startAngle);

        // Rail
        Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
        g.setColour (Colour (0xff1a0a30));
        g.strokePath (track, PathStrokeType (11.0f, PathStrokeType::curved, PathStrokeType::rounded));

        // Arc arc-en-ciel : la couleur depend de la valeur ET du temps
        const int segs = 48;
        for (int i = 0; i < segs; ++i)
        {
            const float f0 = (float) i / (float) segs;
            if (f0 >= pos)
                break;

            const float f1 = jmin ((float) (i + 1) / (float) segs, pos);
            const float a0 = startAngle + f0 * (endAngle - startAngle);
            const float a1 = startAngle + f1 * (endAngle - startAngle);
            const float hue = std::fmod (f0 * 0.9f + animT + pos * 0.6f, 1.0f);

            Path seg;
            seg.addCentredArc (c.x, c.y, r, r, 0.0f, a0, a1, true);

            if (active || hover)
            {
                g.setColour (Colour::fromHSV (hue, 0.8f, 1.0f, active ? 0.28f : 0.14f));
                g.strokePath (seg, PathStrokeType (22.0f, PathStrokeType::curved, PathStrokeType::rounded));
            }

            g.setColour (Colour::fromHSV (hue, 0.85f, active ? 1.0f : 0.92f, 1.0f));
            g.strokePath (seg, PathStrokeType (11.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // Corps du potard
        const float rb = r - 22.0f;
        const float rimHue = std::fmod (animT + pos * 0.6f, 1.0f);

        g.setColour (Colour::fromHSV (rimHue, 0.9f, 1.0f, active ? 0.55f : 0.30f));
        g.fillEllipse (c.x - rb - 5.0f, c.y - rb - 5.0f, (rb + 5.0f) * 2.0f, (rb + 5.0f) * 2.0f);

        g.setGradientFill (ColourGradient (Colour (0xff4a3270), c.x - rb * 0.5f, c.y - rb * 0.7f,
                                           Colour (0xff10061f), c.x + rb * 0.6f, c.y + rb * 0.9f, false));
        g.fillEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f);

        g.setColour (Colour::fromHSV (rimHue, 0.9f, 1.0f, 0.9f));
        g.drawEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f, 3.0f);

        // Aiguille blanche
        Path ptr;
        ptr.addRoundedRectangle (-3.5f, -rb + 6.0f, 7.0f, rb * 0.38f, 3.0f);
        g.setColour (Colours::white);
        g.fillPath (ptr, AffineTransform::rotation (angEnd).translated (c.x, c.y));
    }
};

//==============================================================================
class FuckAnTuneEditor : public AudioProcessorEditor,
                         private Timer
{
public:
    explicit FuckAnTuneEditor (FuckAnTuneProcessor&);
    ~FuckAnTuneEditor() override;

    void paint (Graphics&) override;
    void resized() override;
    void mouseDown (const MouseEvent&) override;

private:
    void timerCallback() override;
    void respawn (int i, bool anywhere);

    struct Balloon
    {
        float x, y, radius, speed, phase, hue;
    };

    FuckAnTuneProcessor& proc;
    RgbKnobLookAndFeel laf;

    ComboBox keyBox, scaleBox;
    Slider speedKnob, robotKnob;

    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>   speedAtt, robotAtt;

    std::array<Balloon, 16> balloons;
    Random rng;
    float activity = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FuckAnTuneEditor)
};
