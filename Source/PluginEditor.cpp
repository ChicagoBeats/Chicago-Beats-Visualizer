#include "PluginEditor.h"

namespace
{
    constexpr float W = 1280.0f, H = 720.0f;
    const Rectangle<float> canvas (250.0f, 170.0f, 780.0f, 330.0f);   // the picture/visualizer window

    ColourGradient chrome (float x, float y, float h)
    {
        ColourGradient g (Colour (0xfff2f2f2), x, y, Colour (0xff2b2b2b), x, y + h, false);
        g.addColour (0.45, Colour (0xff9c9c9c));
        g.addColour (0.55, Colour (0xff555555));
        return g;
    }

    void drawCrown (Graphics& g, float x0, float y0, float w, float h, Colour gem)
    {
        const float xs[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
        const float ys[5] = { 0.25f, 0.10f, 0.0f, 0.10f, 0.25f };
        Path p;
        p.startNewSubPath (x0, y0 + h);
        for (int i = 0; i < 5; ++i)
        {
            p.lineTo (x0 + w * xs[i], y0 + h * ys[i]);
            if (i < 4) p.lineTo (x0 + w * (xs[i] + 0.125f), y0 + h * 0.55f);
        }
        p.lineTo (x0 + w, y0 + h);
        p.closeSubPath();

        g.setGradientFill (chrome (y0, y0, h));
        g.fillPath (p);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.strokePath (p, PathStrokeType (1.5f));
        g.setColour (gem);
        for (int i = 0; i < 5; ++i)
            g.fillEllipse (x0 + w * xs[i] - 4.0f, y0 + h * ys[i] - 2.0f, 8.0f, 8.0f);
    }

    bool isBlackKey (int n) { const int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
}

CBEditor::CBEditor (CBProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 0; i < numCols; ++i) resetCol (i, true);

    skinFolder = File::getSpecialLocation (File::userDocumentsDirectory)
                    .getChildFile ("ChicagoBeatsVisualizer").getChildFile ("skins");
    skinFolder.createDirectory();
    loadSkinList();
    if (skins.size() > 0) stepSkin (1);

    constrainer.setFixedAspectRatio (16.0 / 9.0);
    constrainer.setSizeLimits (640, 360, 1920, 1080);
    setConstrainer (&constrainer);
    setResizable (true, true);                       // bottom-right drag handle

    const int w = roundToInt (W * jlimit (0.5f, 1.5f, proc.uiScale.load()));
    setSize (w, roundToInt (w * 9.0f / 16.0f));
    startTimerHz (30);
}

void CBEditor::resized()
{
    scale = (float) getWidth() / W;
    proc.uiScale = scale;
}

void CBEditor::stepSize (float factor)           // the in-plugin  -  /  +  buttons
{
    const int w = jlimit (640, 1920, roundToInt (getWidth() * factor));
    setSize (w, roundToInt (w * 9.0f / 16.0f));
}

void CBEditor::resetCol (int i, bool scatter)
{
    cols[i].len   = 6 + rng.nextInt (12);
    cols[i].speed = 3.0f + rng.nextFloat() * 6.0f;
    cols[i].y     = scatter ? rng.nextFloat() * canvas.getHeight()
                            : -rng.nextFloat() * 120.0f;
}

// ---------------------------------------------------------------- skins
void CBEditor::loadSkinList()
{
    skins.clear();
    skinFolder.findChildFiles (skins, File::findFiles, false, "*.png;*.jpg;*.jpeg;*.gif");
}

void CBEditor::stepSkin (int dir)
{
    loadSkinList();
    if (skins.isEmpty()) { skin = Image(); skinIndex = -1; return; }
    skinIndex = (skinIndex + dir + skins.size()) % skins.size();
    skin = ImageFileFormat::loadFrom (skins[skinIndex]);
}

void CBEditor::randomize()
{
    static const uint32 palette[] = { 0xffff1a1a, 0xffffc233, 0xff2d7bff, 0xff19e08a, 0xffb04bff, 0xffffffff };
    accent = Colour (palette[rng.nextInt (6)]);
    showMatrix = rng.nextBool();
    showStars  = rng.nextBool();
    if (! showMatrix && ! showStars) showMatrix = true;
    loadSkinList();
    if (skins.size() > 1) stepSkin (1 + rng.nextInt (skins.size() - 1));
}

void CBEditor::mouseDown (const MouseEvent& e)
{
    const auto pt = e.position / scale;
    if      (btnOn.contains (pt))    *proc.vfxOn = true;
    else if (btnOff.contains (pt))   *proc.vfxOn = false;
    else if (btnPrev.contains (pt))  stepSkin (-1);
    else if (btnNext.contains (pt))  stepSkin (1);
    else if (btnDice.contains (pt))  randomize();
    else if (btnMinus.contains (pt)) stepSize (0.9f);
    else if (btnPlus.contains (pt))  stepSize (1.1f);
}

// ---------------------------------------------------------------- audio analysis + motion
void CBEditor::analyse()
{
    proc.copyLatestSamples (fftData.data(), CBProcessor::fftSize);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) CBProcessor::fftSize);
    std::fill (fftData.begin() + CBProcessor::fftSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double binHz = proc.sampleRate.load() / (double) CBProcessor::fftSize;
    for (int b = 0; b < numBands; ++b)
    {
        const double f0 = 30.0 * std::pow (16000.0 / 30.0, b / (double) numBands);
        const double f1 = 30.0 * std::pow (16000.0 / 30.0, (b + 1) / (double) numBands);
        const int lo = jmax (1, (int) std::floor (f0 / binHz));
        const int hi = jmin (CBProcessor::fftSize / 2 - 1, jmax (lo + 1, (int) std::ceil (f1 / binHz)));
        float m = 0.0f;
        for (int k = lo; k <= hi; ++k) m = jmax (m, fftData[(size_t) k]);
        const float db = Decibels::gainToDecibels (m / (CBProcessor::fftSize * 0.25f), -90.0f);
        const float level = jlimit (0.0f, 1.0f, jmap (db, -70.0f, 0.0f, 0.0f, 1.0f));
        bands[b] = jmax (level, bands[b] * 0.85f);
    }

    float lo = 0, hi = 0;
    for (int i = 0; i < 12; ++i)  lo += bands[i];
    for (int i = 48; i < 64; ++i) hi += bands[i];
    bass   = jlimit (0.0f, 1.0f, std::pow (lo / 12.0f, 1.5f) * 1.6f);
    treble = jlimit (0.0f, 1.0f, hi / 16.0f);
    bassEnv = jmax (bass, bassEnv * 0.90f);

    // spring: kick slams the cone out, it overshoots and rebounds
    const float accel = (bass - coneX) * 0.55f - coneV * 0.22f;
    coneV += accel;
    coneX = jlimit (-0.35f, 1.25f, coneX + coneV);

    dispL = jmax (proc.peakL.exchange (0.0f), dispL * 0.88f);
    dispR = jmax (proc.peakR.exchange (0.0f), dispR * 0.88f);

    for (int i = 0; i < 128; ++i)
        keyDisp[i] = jmax (proc.keyVel[i].load(), keyDisp[i] * 0.90f);
}

void CBEditor::timerCallback()
{
    analyse();

    if (*proc.vfxOn)
    {
        if (showMatrix)
            for (int i = 0; i < numCols; ++i)
            {
                cols[i].y += cols[i].speed * (1.0f + treble * 2.0f);
                if (cols[i].y - cols[i].len * 14.0f > canvas.getHeight()) resetCol (i, false);
            }

        if (showStars)
        {
            if (rng.nextFloat() < 0.03f + bassEnv * 0.25f)
            {
                Star s;
                s.p = { canvas.getX() + rng.nextFloat() * canvas.getWidth() * 0.7f, canvas.getY() };
                s.v = { 8.0f + rng.nextFloat() * 8.0f, 3.0f + rng.nextFloat() * 4.0f };
                stars.add (s);
            }
            for (int i = stars.size(); --i >= 0;)
            {
                auto& s = stars.getReference (i);
                s.p += s.v;
                s.life -= 0.025f;
                if (s.life <= 0.0f || ! canvas.expanded (40.0f).contains (s.p)) stars.remove (i);
            }
        }
    }
    repaint();
}

// ---------------------------------------------------------------- painting
void CBEditor::paint (Graphics& g)
{
    g.addTransform (AffineTransform::scale (scale));
    const bool vfx = *proc.vfxOn;

    g.fillAll (Colours::black);
    g.setGradientFill (ColourGradient (accent.withAlpha (0.30f), W * 0.5f, H * 0.5f,
                                       Colours::black, 0.0f, 0.0f, true));
    g.fillRect (0.0f, 0.0f, W, H);

    drawCanvas (g, vfx);
    drawFrame (g);
    drawPanels (g);
    drawTransport (g);
    drawKeyboard (g);

    // speakers last so they overlap the frame edges
    const float shake = vfx ? bassEnv * 3.0f : 0.0f;
    const float exc   = vfx ? coneX : 0.0f;
    drawSpeaker (g, { 130.0f, 176.0f },  107.0f, exc,         shake);
    drawSpeaker (g, { 1149.0f, 176.0f }, 107.0f, exc,         shake);
    drawSpeaker (g, { 92.0f, 627.0f },   46.0f,  exc * 0.8f,  shake * 0.6f);
    drawSpeaker (g, { 1188.0f, 627.0f }, 46.0f,  exc * 0.8f,  shake * 0.6f);
}

void CBEditor::drawCanvas (Graphics& g, bool vfx)
{
    Path clip;
    clip.addRoundedRectangle (canvas, 18.0f);
    g.saveState();
    g.reduceClipRegion (clip);

    // picture / skin (cover-fit). Without a skin, a dark gradient is shown.
    if (skin.isValid())
    {
        const float ir = (float) skin.getWidth() / (float) skin.getHeight();
        const float cr = canvas.getWidth() / canvas.getHeight();
        int sw = skin.getWidth(), sh = skin.getHeight();
        if (ir > cr) sw = roundToInt (sh * cr); else sh = roundToInt (sw / cr);
        const int sx = (skin.getWidth() - sw) / 2, sy = (skin.getHeight() - sh) / 2;
        g.drawImage (skin, (int) canvas.getX(), (int) canvas.getY(), (int) canvas.getWidth(), (int) canvas.getHeight(),
                     sx, sy, sw, sh);
    }
    else
    {
        g.setGradientFill (ColourGradient (accent.withAlpha (0.45f), canvas.getCentreX(), canvas.getBottom(),
                                           Colour (0xff050505), canvas.getCentreX(), canvas.getY(), false));
        g.fillRect (canvas);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.setFont (Font (18.0f, Font::bold));
        g.drawText ("Drop .png / .jpg / .gif skins into Documents\\ChicagoBeatsVisualizer\\skins",
                    canvas.toNearestInt(), Justification::centred);
    }

    if (vfx)
    {
        g.setColour (accent.withAlpha (bassEnv * 0.18f));       // bass pulse wash
        g.fillRect (canvas);

        if (showMatrix)
        {
            g.setFont (Font (Font::getDefaultMonospacedFontName(), 12.0f, Font::plain));
            static const String chars ("01ABCDEF<>/|=+*");
            for (int i = 0; i < numCols; ++i)
            {
                const float x = canvas.getX() + i * 14.0f;
                for (int k = 0; k < cols[i].len; ++k)
                {
                    const float y = canvas.getY() + cols[i].y - k * 14.0f;
                    if (y < canvas.getY() - 14.0f || y > canvas.getBottom()) continue;
                    const float a = 1.0f - (float) k / (float) cols[i].len;
                    g.setColour ((k == 0 ? Colours::white : accent).withAlpha (a * 0.8f));
                    g.drawSingleLineText (String::charToString (chars[rng.nextInt (chars.length())]),
                                          (int) x, (int) y);
                }
            }
        }

        if (showStars)
            for (auto& s : stars)
            {
                const auto tail = s.p - s.v * 8.0f;
                g.setGradientFill (ColourGradient (Colours::white.withAlpha (s.life), s.p.x, s.p.y,
                                                   Colours::transparentWhite, tail.x, tail.y, false));
                g.drawLine (s.p.x, s.p.y, tail.x, tail.y, 2.0f);
                g.setColour (Colours::white.withAlpha (s.life));
                g.fillEllipse (s.p.x - 2.0f, s.p.y - 2.0f, 4.0f, 4.0f);
            }
    }

    // spectrum bars along the bottom of the canvas (always live)
    const float bw = canvas.getWidth() / numBands;
    for (int b = 0; b < numBands; ++b)
    {
        const float h = bands[b] * canvas.getHeight() * 0.45f;
        const float x = canvas.getX() + b * bw;
        g.setColour (accent.withAlpha (0.85f));
        g.fillRect (x + 1.0f, canvas.getBottom() - h, bw - 2.0f, h);
        g.setColour (Colours::white);
        g.fillRect (x + 1.0f, canvas.getBottom() - h, bw - 2.0f, 2.0f);
    }
    g.restoreState();
}

void CBEditor::drawFrame (Graphics& g)
{
    const auto outer = canvas.expanded (14.0f);
    const float glow = 0.35f + 0.65f * bassEnv;
    for (int i = 4; i >= 1; --i)                                // soft red glow
    {
        g.setColour (accent.withAlpha (glow * 0.08f * (float) (5 - i)));
        g.drawRoundedRectangle (outer.expanded ((float) i * 2.0f), 26.0f, 3.0f);
    }
    g.setGradientFill (chrome (outer.getY(), outer.getY(), outer.getHeight()));
    g.drawRoundedRectangle (outer, 24.0f, 10.0f);
    g.setColour (accent.withAlpha (glow));
    g.drawRoundedRectangle (canvas.expanded (3.0f), 20.0f, 2.0f);

    // title plate + crown
    const Rectangle<float> plate (368.0f, 78.0f, 543.0f, 76.0f);
    g.setColour (Colour (0xff0c0c0c));
    g.fillRoundedRectangle (plate, 14.0f);
    g.setGradientFill (chrome (plate.getY(), plate.getY(), plate.getHeight()));
    g.drawRoundedRectangle (plate, 14.0f, 4.0f);

    g.setFont (Font ("Georgia", 46.0f, Font::bold | Font::italic));
    g.setColour (Colours::white);
    g.drawText ("Chicago", Rectangle<int> (384, 78, 250, 56), Justification::centredRight);
    g.setColour (accent);
    g.drawText (" Beats", Rectangle<int> (634, 78, 260, 56), Justification::centredLeft);
    g.setFont (Font (15.0f, Font::bold));
    g.setColour (Colours::white);
    g.drawText ("VISUALIZER", Rectangle<int> (368, 130, 543, 22), Justification::centred);

    drawCrown (g, 560.0f, 6.0f, 160.0f, 68.0f, accent);
}

void CBEditor::drawSpeaker (Graphics& g, Point<float> c, float r, float exc, float shake)
{
    c += { (rng.nextFloat() - 0.5f) * shake, (rng.nextFloat() - 0.5f) * shake };

    g.setGradientFill (chrome (c.y - r, c.y - r, 2.0f * r));            // chrome cabinet ring
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);

    const float led = 0.30f + 0.70f * bassEnv;                          // LED ring + halo
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (accent.withAlpha (led * 0.10f * (float) (4 - i)));
        g.drawEllipse (c.x - r * 0.86f - i * 3.0f, c.y - r * 0.86f - i * 3.0f,
                       r * 1.72f + i * 6.0f, r * 1.72f + i * 6.0f, r * 0.05f);
    }
    g.setColour (accent.withAlpha (led));
    g.drawEllipse (c.x - r * 0.86f, c.y - r * 0.86f, r * 1.72f, r * 1.72f, r * 0.06f);

    const float cr = r * 0.78f * (1.0f + 0.07f * exc);                  // cone thrusts toward the viewer
    g.setColour (Colour (0xff070707));
    g.fillEllipse (c.x - cr, c.y - cr, 2.0f * cr, 2.0f * cr);
    g.setColour (Colour (0xff2c2c2c));
    g.drawEllipse (c.x - cr, c.y - cr, 2.0f * cr, 2.0f * cr, r * 0.07f);   // surround flexes with cr

    const float hx = c.x - r * 0.18f * (1.0f + exc), hy = c.y - r * 0.18f * (1.0f + exc);
    g.setGradientFill (ColourGradient (Colour (0xff6a6a6a), hx, hy, Colour (0xff101010), c.x + cr, c.y + cr, true));
    g.fillEllipse (c.x - cr * 0.85f, c.y - cr * 0.85f, cr * 1.7f, cr * 1.7f);

    const float dc = r * 0.30f * (1.0f + 0.20f * exc);                  // dust cap pulses
    g.setGradientFill (chrome (c.y - dc, c.y - dc, 2.0f * dc));
    g.fillEllipse (c.x - dc, c.y - dc, 2.0f * dc, 2.0f * dc);
    drawCrown (g, c.x - dc * 0.6f, c.y - dc * 0.45f, dc * 1.2f, dc * 0.9f, accent);
}

void CBEditor::drawButton (Graphics& g, Rectangle<float> r, const String& t, bool on)
{
    g.setColour (on ? accent : Colour (0xff141414));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? Colours::white : accent.withAlpha (0.65f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.5f);
    g.setFont (Font (14.0f, Font::bold));
    g.drawText (t, r, Justification::centred);
}

void CBEditor::drawPanels (Graphics& g)
{
    const Rectangle<float> left (46, 279, 168, 222), right (1072, 279, 168, 222);
    for (auto p : { left, right })
    {
        g.setColour (Colour (0xdd0a0a0a));
        g.fillRoundedRectangle (p, 10.0f);
        g.setGradientFill (chrome (p.getY(), p.getY(), p.getHeight()));
        g.drawRoundedRectangle (p, 10.0f, 3.0f);
    }
    g.setColour (Colours::white);
    g.setFont (Font (13.0f, Font::bold));
    auto label = [&] (const String& t, Rectangle<float> r) { g.drawText (t, r, Justification::centred); };

    // left: Motion VFX / dB meter / spectrum
    const bool vfx = *proc.vfxOn;
    label ("MOTION VFX", { 46, 282, 168, 20 });
    drawButton (g, btnOn, "ON", vfx);
    drawButton (g, btnOff, "OFF", ! vfx);

    label ("DB METER", { 46, 338, 168, 16 });
    const float meters[4] = { dispL, dispR, bass, treble };
    for (int m = 0; m < 4; ++m)
        for (int s = 0; s < 12; ++s)
        {
            const bool lit = jlimit (0.0f, 1.0f, meters[m]) * 12.0f > (float) s;
            g.setColour (lit ? (s >= 9 ? Colours::white : accent) : Colour (0xff1c1c1c));
            g.fillRect (64.0f + m * 34.0f, 404.0f - s * 4.4f, 26.0f, 3.4f);
        }

    label ("SPECTRUM", { 46, 410, 168, 16 });
    for (int b = 0; b < 16; ++b)
    {
        float v = 0; for (int k = 0; k < 4; ++k) v = jmax (v, bands[b * 4 + k]);
        const float h = v * 56.0f;
        g.setColour (Colours::white);
        g.fillRect (60.0f + b * 8.6f, 492.0f - h, 6.0f, h);
    }

    // right: skin mode / randomizer / resize
    g.setColour (Colours::white);
    label ("SKIN MODE", { 1072, 282, 168, 20 });
    const Rectangle<float> thumb (1090, 304, 132, 44);
    g.setColour (Colour (0xff151515));
    g.fillRoundedRectangle (thumb, 4.0f);
    if (skin.isValid())
        g.drawImage (skin, (int) thumb.getX(), (int) thumb.getY(), (int) thumb.getWidth(), (int) thumb.getHeight(),
                     0, 0, skin.getWidth(), skin.getHeight());
    drawButton (g, btnPrev, "<", false);
    drawButton (g, btnNext, ">", false);

    g.setColour (Colours::white);
    label ("RANDOMIZER", { 1072, 384, 168, 16 });
    g.setColour (Colour (0xfff0f0f0));
    g.fillRoundedRectangle (btnDice, 8.0f);
    g.setColour (accent);
    for (auto d : { Point<float> (.25f, .25f), Point<float> (.75f, .25f), Point<float> (.5f, .5f),
                    Point<float> (.25f, .75f), Point<float> (.75f, .75f) })
        g.fillEllipse (btnDice.getX() + d.x * 44.0f - 4.0f, btnDice.getY() + d.y * 44.0f - 4.0f, 8.0f, 8.0f);

    g.setColour (Colours::white);
    label ("RESIZE INTERFACE", { 1072, 448, 168, 16 });
    drawButton (g, btnMinus, "-", false);
    drawButton (g, btnPlus, "+", false);
}

void CBEditor::drawTransport (Graphics& g)
{
    // decorative for now: the media transport arrives with the video/gif player milestone
    const Rectangle<float> bar (270, 528, 740, 46);
    g.setColour (Colour (0xdd0a0a0a));
    g.fillRoundedRectangle (bar, 10.0f);
    g.setGradientFill (chrome (bar.getY(), bar.getY(), bar.getHeight()));
    g.drawRoundedRectangle (bar, 10.0f, 3.0f);
    g.setColour (Colours::white);
    g.setFont (Font (17.0f, Font::bold));
    g.drawText ("Chicago Beats", Rectangle<int> (300, 531, 260, 22), Justification::centredLeft);
    g.setFont (Font (13.0f));
    g.drawText ("Now Playing...", Rectangle<int> (300, 551, 260, 18), Justification::centredLeft);

    g.setColour (accent);
    g.fillEllipse (616.0f, 527.0f, 48.0f, 48.0f);
    Path play; play.addTriangle (632.0f, 540.0f, 632.0f, 562.0f, 652.0f, 551.0f);
    g.setColour (Colours::white); g.fillPath (play);
    Path back, fwd;
    back.addTriangle (600.0f, 551.0f, 612.0f, 543.0f, 612.0f, 559.0f);
    fwd.addTriangle (680.0f, 543.0f, 680.0f, 559.0f, 692.0f, 551.0f);
    g.setColour (accent); g.fillPath (back); g.fillPath (fwd);
}

void CBEditor::drawKeyboard (Graphics& g)
{
    const float x0 = 180.0f, y0 = 590.0f, kw = 920.0f / 36.0f, kh = 86.0f;
    int wi = 0;
    for (int n = 36; n <= 96; ++n)
        if (! isBlackKey (n))
        {
            const float v = keyDisp[n];
            const Rectangle<float> k (x0 + wi * kw, y0, kw - 1.0f, kh);
            g.setColour (Colours::white.interpolatedWith (accent, jlimit (0.0f, 1.0f, v)));
            g.fillRect (k);
            if (v > 0.05f)
            {
                g.setColour (accent.withAlpha (v * 0.6f));
                g.fillRect (k.getX(), k.getY() - 10.0f * v, k.getWidth(), 10.0f * v);
            }
            ++wi;
        }

    wi = 0;
    for (int n = 36; n <= 96; ++n)
        if (! isBlackKey (n)) ++wi;
        else
        {
            const float v = keyDisp[n];
            const Rectangle<float> k (x0 + wi * kw - kw * 0.3f, y0, kw * 0.6f, kh * 0.58f);
            g.setColour (Colour (0xff111111).interpolatedWith (accent, jlimit (0.0f, 1.0f, v)));
            g.fillRect (k);
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawRect (k, 1.0f);
        }
}
