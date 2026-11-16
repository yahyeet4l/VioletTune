#include "PluginEditor.h"

MeltingKnobLAF::MeltingKnobLAF()
{
    setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.7f));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    setColour (juce::ComboBox::backgroundColourId, Look::panel);
    setColour (juce::ComboBox::textColourId, juce::Colours::white.withAlpha (0.85f));
    setColour (juce::ComboBox::outlineColourId, Look::purple.withAlpha (0.35f));
    setColour (juce::ComboBox::arrowColourId, Look::purple);
    setColour (juce::PopupMenu::backgroundColourId, Look::panel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Look::purple.darker (0.4f));
}

void MeltingKnobLAF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    using namespace juce;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float outer = jmin (w, h) * 0.5f - 6.0f;
    const float R = outer * 0.72f;
    const float t = Look::time;

    // ---- indicator groove + gradient arc (black -> purple, length = amount) ----
    const float arcR = outer - 2.0f;
    Path groove;
    groove.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colour (0xff141416));
    g.strokePath (groove, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const int segs = 64;
    const float valAngle = startAngle + pos * (endAngle - startAngle);
    for (int i = 0; i < segs; ++i)
    {
        if (pos <= 0.001f) break;
        float f0 = (float) i / segs, f1 = (float) (i + 1) / segs;
        float a0 = startAngle + f0 * (valAngle - startAngle);
        float a1 = startAngle + f1 * (valAngle - startAngle);
        Path seg;
        seg.addCentredArc (cx, cy, arcR, arcR, 0.0f, a0, a1 + 0.01f, true);
        g.setColour (Look::black.interpolatedWith (Look::purple, f1));
        g.strokePath (seg, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::butt));
    }
    {
        float tx = cx + arcR * std::sin (valAngle), ty = cy - arcR * std::cos (valAngle);
        Colour tip = Look::black.interpolatedWith (Look::purple, pos);
        ColourGradient glow (tip.withAlpha (0.55f * pos), tx, ty, tip.withAlpha (0.0f), tx + 11, ty, true);
        g.setGradientFill (glow);
        g.fillEllipse (tx - 11, ty - 11, 22, 22);
        g.setColour (tip.brighter (0.2f));
        g.fillEllipse (tx - 3.0f, ty - 3.0f, 6.0f, 6.0f);
    }

    // ---- melting knob body ----
    const float wobble = 0.6f + 2.4f * pos;
    const float drip   = R * (0.03f + 0.17f * pos);
    Path body;
    const int N = 120;
    for (int i = 0; i <= N; ++i)
    {
        float th = MathConstants<float>::twoPi * (float) i / N;
        float r = R + wobble * (std::sin (3 * th + t * 1.3f) + 0.6f * std::sin (5 * th - t * 0.9f)
                                + 0.35f * std::sin (8 * th + t * 2.1f));
        float down = std::max (0.0f, std::sin (th));
        float lobes = 0.5f + 0.5f * std::sin (t * 0.8f + 4.0f * th);
        r += drip * std::pow (down, 5.0f) * (0.4f + 0.6f * lobes);
        float px = cx + r * std::cos (th), py = cy + r * std::sin (th);
        if (i == 0) body.startNewSubPath (px, py); else body.lineTo (px, py);
    }
    body.closeSubPath();
    body = body.createPathWithRoundedCorners (6.0f);

    DropShadow (Colours::black.withAlpha (0.75f), 14, { 0, 6 }).drawForPath (g, body);
    DropShadow (Colours::black.withAlpha (0.4f), 5, { 0, 2 }).drawForPath (g, body);

    ColourGradient fill (Colour (0xff36363d), cx - R * 0.6f, cy - R * 0.8f,
                         Colour (0xff141417), cx + R * 0.7f, cy + R, false);
    g.setGradientFill (fill);
    g.fillPath (body);

    Colour tint = Look::black.interpolatedWith (Look::purple, pos);
    ColourGradient tintG (tint.withAlpha (0.05f + 0.35f * pos), cx, cy + R,
                          tint.withAlpha (0.0f), cx, cy - R * 0.2f, false);
    g.setGradientFill (tintG);
    g.fillPath (body);

    g.setColour (Colours::white.withAlpha (0.12f));
    g.strokePath (body, PathStrokeType (1.2f));
    ColourGradient rim (Colours::white.withAlpha (0.16f), cx, cy - R, Colours::transparentWhite, cx, cy, false);
    g.setGradientFill (rim);
    g.strokePath (body, PathStrokeType (2.0f));

    Path ptr;
    ptr.addRoundedRectangle (-1.8f, -R * 0.78f, 3.6f, R * 0.32f, 1.8f);
    g.setColour (tint.brighter (0.3f).interpolatedWith (Colours::white, 0.15f));
    g.fillPath (ptr, AffineTransform::rotation (valAngle).translated (cx, cy));
}

VioletTuneEditor::VioletTuneEditor (VioletTuneProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    setSize (760, 500);

    auto setup = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        s.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        s.setLookAndFeel (&laf);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };
    setup (speed,   speedL,   "SPEED");
    setup (amount,  amountL,  "AMOUNT");
    setup (sustain, sustainL, "SUSTAIN");
    setup (stab,    stabL,    "STABILIZER");
    setup (dbl,     dblL,     "DOUBLE");

    key.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scale.addItemList (Scales::list(), 1);
    addAndMakeVisible (key);
    addAndMakeVisible (scale);
    for (auto* l : { &keyL, &scaleL })
    {
        l->setJustificationType (juce::Justification::centredRight);
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    }
    keyL.setText ("KEY", juce::dontSendNotification);
    scaleL.setText ("SCALE", juce::dontSendNotification);

    speedA   = std::make_unique<SA> (proc.apvts, "speed",   speed);
    amountA  = std::make_unique<SA> (proc.apvts, "amount",  amount);
    sustainA = std::make_unique<SA> (proc.apvts, "sustain", sustain);
    stabA    = std::make_unique<SA> (proc.apvts, "stab",    stab);
    dblA     = std::make_unique<SA> (proc.apvts, "double",  dbl);
    keyA     = std::make_unique<CA> (proc.apvts, "key",     key);
    scaleA   = std::make_unique<CA> (proc.apvts, "scale",   scale);

    startTimerHz (60);
}

VioletTuneEditor::~VioletTuneEditor()
{
    for (auto* s : { &speed, &amount, &sustain, &stab, &dbl }) s->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void VioletTuneEditor::paintHeat (juce::Graphics& g, juce::Rectangle<int> r, bool input)
{
    static const char* nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const int scl = juce::jlimit (0, 7, (int) proc.apvts.getRawParameterValue ("scale")->load());
    const float cw = (float) r.getWidth() / 12.0f;

    for (int pc = 0; pc < 12; ++pc)
    {
        auto cell = juce::Rectangle<float> ((float) r.getX() + pc * cw, (float) r.getY(), cw - 4.0f, (float) r.getHeight());
        const bool inScale = Scales::masks[scl][((pc - key) % 12 + 12) % 12];
        const float heat = (input ? proc.inHeat[pc] : proc.outHeat[pc]).load();

        g.setColour (juce::Colour (0xff16161a));
        g.fillRoundedRectangle (cell, 4.0f);
        g.setColour (Look::black.interpolatedWith (Look::purple, juce::jlimit (0.0f, 1.0f, heat)).withAlpha (0.15f + 0.85f * heat));
        g.fillRoundedRectangle (cell.reduced (1.0f), 3.0f);
        g.setColour (inScale ? Look::purple.withAlpha (0.45f) : juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (cell, 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (inScale ? 0.8f : 0.25f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (nm[pc], cell.toNearestInt(), juce::Justification::centred);
    }
}

void VioletTuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (Look::bg);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawRect (getLocalBounds(), 1);

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("VIOLET", 24, 14, 200, 28, juce::Justification::centredLeft);
    g.setColour (Look::purple);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("AUTOMATIC PITCH CORRECTION", 24, 40, 260, 16, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff18181b));
    g.drawLine (24.0f, 64.0f, (float) getWidth() - 24.0f, 64.0f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("IN",  24, heatIn.getY(),  30, heatIn.getHeight(),  juce::Justification::centredLeft);
    g.drawText ("OUT", 24, heatOut.getY(), 30, heatOut.getHeight(), juce::Justification::centredLeft);
    paintHeat (g, heatIn, true);
    paintHeat (g, heatOut, false);
}

void VioletTuneEditor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (74).reduced (20, 0);
    auto heat = area.removeFromTop (62).withTrimmedLeft (36);
    heatIn  = heat.removeFromTop (28);
    heat.removeFromTop (4);
    heatOut = heat.removeFromTop (28);

    auto bottom = area.removeFromBottom (54);
    auto row = area.reduced (0, 6);
    const int cw = row.getWidth() / 5;

    juce::Slider* sl[] = { &speed, &amount, &sustain, &stab, &dbl };
    juce::Label*  lb[] = { &speedL, &amountL, &sustainL, &stabL, &dblL };
    for (int i = 0; i < 5; ++i)
    {
        auto c = row.removeFromLeft (cw);
        lb[i]->setBounds (c.removeFromBottom (20));
        sl[i]->setBounds (c.reduced (6, 0));
    }

    auto b = bottom.withSizeKeepingCentre (440, 28);
    keyL.setBounds (b.removeFromLeft (40));
    key.setBounds (b.removeFromLeft (100));
    b.removeFromLeft (30);
    scaleL.setBounds (b.removeFromLeft (50));
    scale.setBounds (b.removeFromLeft (180));
}
