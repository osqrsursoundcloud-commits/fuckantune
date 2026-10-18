#include "PluginEditor.h"

namespace
{
    constexpr int editorW = 640;
    constexpr int editorH = 450;

    const StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

//==============================================================================
FuckAnTuneEditor::FuckAnTuneEditor (FuckAnTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // Les listes doivent etre remplies AVANT de creer les attachements
    keyBox.addItemList (noteNames, 1);
    scaleBox.addItemList (StringArray { "Chromatique", "Majeur", "Mineur", "Pentatonique mineur" }, 1);

    keyAtt   = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);

    for (auto* k : { &speedKnob, &robotKnob })
    {
        k->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        k->setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    }

    speedAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "speed", speedKnob);
    robotAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "robot", robotKnob);

    for (auto* c : std::initializer_list<Component*> { &keyBox, &scaleBox, &speedKnob, &robotKnob })
    {
        c->setLookAndFeel (&laf);
        addAndMakeVisible (c);
    }

    setSize (editorW, editorH);

    for (size_t i = 0; i < balloons.size(); ++i)
        respawn ((int) i, true);

    startTimerHz (30);
}

FuckAnTuneEditor::~FuckAnTuneEditor()
{
    stopTimer();

    for (auto* c : std::initializer_list<Component*> { &keyBox, &scaleBox, &speedKnob, &robotKnob })
        c->setLookAndFeel (nullptr);
}

//==============================================================================
void FuckAnTuneEditor::respawn (int i, bool anywhere)
{
    auto& b = balloons[(size_t) i];
    b.radius = 22.0f + rng.nextFloat() * 20.0f;
    b.x      = rng.nextFloat() * (float) editorW;
    b.y      = anywhere ? rng.nextFloat() * (float) editorH
                        : (float) editorH + b.radius + rng.nextFloat() * 120.0f;
    b.speed  = 0.5f + rng.nextFloat() * 1.0f;
    b.phase  = rng.nextFloat() * MathConstants<float>::twoPi;
    b.hue    = rng.nextFloat();
}

void FuckAnTuneEditor::timerCallback()
{
    laf.animT = std::fmod (laf.animT + 0.006f, 1.0f);
    activity += 0.15f * (proc.voicedMeter.load() - activity);

    for (size_t i = 0; i < balloons.size(); ++i)
    {
        auto& b = balloons[i];
        b.y -= b.speed * (1.0f + 2.5f * activity);   // les ballons montent plus vite quand tu chantes
        b.phase += 0.04f;

        if (b.y < -b.radius * 3.0f)
            respawn ((int) i, false);
    }

    repaint();
}

void FuckAnTuneEditor::mouseDown (const MouseEvent& e)
{
    // Clique sur un ballon pour l'eclater !
    const auto pt = e.position;

    for (size_t i = 0; i < balloons.size(); ++i)
    {
        const auto& b = balloons[i];
        const float bx = b.x + std::sin (b.phase) * 6.0f;
        const float dx = (pt.x - bx) / (b.radius * 0.85f);
        const float dy = (pt.y - b.y) / b.radius;

        if (dx * dx + dy * dy < 1.0f)
        {
            respawn ((int) i, false);
            break;
        }
    }
}

//==============================================================================
void FuckAnTuneEditor::paint (Graphics& g)
{
    const float t = laf.animT;

    // Ciel de fete foraine
    g.setGradientFill (ColourGradient (Colour (0xff1a0b3d), 0.0f, 0.0f,
                                       Colour (0xff5a1670), 0.0f, (float) editorH, false));
    g.fillAll();

    // Ballons
    for (const auto& b : balloons)
    {
        const float bx = b.x + std::sin (b.phase) * 6.0f;
        const float by = b.y;
        const Colour col = Colour::fromHSV (b.hue, 0.75f, 1.0f, 0.95f);

        Path str;
        str.startNewSubPath (bx, by + b.radius * 1.05f);
        for (int k = 1; k <= 8; ++k)
            str.lineTo (bx + std::sin (b.phase * 1.3f + (float) k * 0.7f) * 5.0f,
                        by + b.radius * 1.05f + (float) k * 9.0f);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.strokePath (str, PathStrokeType (1.2f));

        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.9f),
                                           bx - b.radius * 0.3f, by - b.radius * 0.4f,
                                           col,
                                           bx + b.radius * 0.5f, by + b.radius * 0.7f, true));
        g.fillEllipse (bx - b.radius * 0.85f, by - b.radius, b.radius * 1.7f, b.radius * 2.0f);

        Path knot;
        knot.addTriangle (bx, by + b.radius * 0.98f,
                          bx - 4.0f, by + b.radius * 1.12f,
                          bx + 4.0f, by + b.radius * 1.12f);
        g.setColour (col.darker (0.3f));
        g.fillPath (knot);
    }

    // Auvent de chapiteau rouge et creme
    const int stripes = 16;
    const float sw = (float) editorW / (float) stripes;

    for (int i = 0; i < stripes; ++i)
    {
        g.setColour (i % 2 == 0 ? Colour (0xffe8203a) : Colour (0xfffff2e0));
        g.fillRect (sw * (float) i, 0.0f, sw + 1.0f, 34.0f);
        g.fillEllipse (sw * (float) i, 34.0f - sw * 0.5f, sw, sw);
    }

    // Titre : chaque lettre change de couleur et danse
    const String title = "FUCKANTUNE";
    const float cell = 46.0f;
    const float startX = ((float) editorW - cell * (float) title.length()) * 0.5f;

    g.setFont (FontOptions (40.0f, Font::bold));

    for (int i = 0; i < title.length(); ++i)
    {
        const float dy = std::sin (t * 40.0f + (float) i * 0.7f) * 4.0f;
        const auto  r  = Rectangle<float> (startX + cell * (float) i, 58.0f + dy, cell, 48.0f);

        g.setColour (Colour (0xff000000).withAlpha (0.55f));
        g.drawText (title.substring (i, i + 1), r.translated (2.5f, 3.0f), Justification::centred);

        g.setColour (Colour::fromHSV (std::fmod ((float) i * 0.1f + t, 1.0f), 0.85f, 1.0f, 1.0f));
        g.drawText (title.substring (i, i + 1), r, Justification::centred);
    }

    g.setFont (FontOptions (12.0f, Font::bold));
    g.setColour (Colour (0xffffe14d));
    g.drawText ("*  A U T O T U N E   R O B O T   D E   F O I R E  *",
                Rectangle<int> (0, 106, editorW, 16), Justification::centred);

    // Etiquettes KEY / SCALE
    g.setFont (FontOptions (12.0f, Font::bold));
    g.setColour (Colour (0xffff9be8));
    g.drawText ("KEY",   Rectangle<int> (30, 130, 110, 14),  Justification::centredLeft);
    g.drawText ("SCALE", Rectangle<int> (150, 130, 190, 14), Justification::centredLeft);

    // Affichage de la note visee
    const bool voiced = proc.voicedMeter.load() > 0.5f;
    String noteText = "--";

    if (voiced)
    {
        const int m = (int) std::round (proc.targetMidiMeter.load());
        noteText = noteNames[((m % 12) + 12) % 12] + String (m / 12 - 1);
    }

    const auto noteBox = Rectangle<float> (450.0f, 130.0f, 160.0f, 48.0f);
    g.setColour (Colour (0xff12061f).withAlpha (0.85f));
    g.fillRoundedRectangle (noteBox, 12.0f);
    g.setColour (Colour::fromHSV (std::fmod (t * 2.0f, 1.0f), 0.9f, 1.0f, voiced ? 1.0f : 0.35f));
    g.drawRoundedRectangle (noteBox, 12.0f, 3.0f);

    g.setFont (FontOptions (30.0f, Font::bold));
    g.drawText (noteText, noteBox.toNearestInt(), Justification::centred);

    // Etiquettes des potards
    const int speedPct = (int) std::round (speedKnob.getValue() * 100.0);
    const int robotPct = (int) std::round (robotKnob.getValue() * 100.0);

    g.setFont (FontOptions (18.0f, Font::bold));
    g.setColour (Colours::white);
    g.drawText ("SPEED", Rectangle<int> (80, 408, 200, 22),  Justification::centred);
    g.drawText ("ROBOT", Rectangle<int> (360, 408, 200, 22), Justification::centred);

    g.setFont (FontOptions (13.0f));
    g.setColour (Colour (0xffffe14d));
    g.drawText (String (speedPct) + " %" + (speedPct >= 99 ? "   INSTANTANE" : ""),
                Rectangle<int> (80, 428, 200, 18), Justification::centred);
    g.drawText (String (robotPct) + " %" + (robotPct >= 70 ? "   ABUSE" : ""),
                Rectangle<int> (360, 428, 200, 18), Justification::centred);
}

void FuckAnTuneEditor::resized()
{
    keyBox.setBounds   (30, 146, 110, 30);
    scaleBox.setBounds (150, 146, 190, 30);

    speedKnob.setBounds (Rectangle<int> (0, 0, 210, 210).withCentre (Point<int> (180, 300)));
    robotKnob.setBounds (Rectangle<int> (0, 0, 210, 210).withCentre (Point<int> (460, 300)));
}
