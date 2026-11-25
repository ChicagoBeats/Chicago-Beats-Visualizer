#pragma once
#include "PluginProcessor.h"
#include <array>

class CBEditor : public AudioProcessorEditor, private Timer
{
public:
    explicit CBEditor (CBProcessor&);
    ~CBEditor() override { stopTimer(); }

    void paint (Graphics&) override;
    void resized() override;
    void mouseDown (const MouseEvent&) override;

private:
    enum { numBands = 64, numCols = 56 };

    struct Col  { float y = 0, speed = 5; int len = 10; };
    struct Star { Point<float> p, v; float life = 1; };

    void timerCallback() override;
    void analyse();
    void resetCol (int i, bool scatter);

    void loadSkinList();
    void stepSkin (int dir);
    void randomize();
    void stepSize (float factor);

    // drawing (all in a fixed 1280x720 design space, scaled at paint time)
    void drawCanvas (Graphics&, bool vfx);
    void drawFrame (Graphics&);
    void drawSpeaker (Graphics&, Point<float> c, float r, float exc, float shake);
    void drawPanels (Graphics&);
    void drawTransport (Graphics&);
    void drawKeyboard (Graphics&);
    void drawButton (Graphics&, Rectangle<float>, const String&, bool on);

    CBProcessor& proc;
    ComponentBoundsConstrainer constrainer;
    float scale = 1.0f;

    dsp::FFT fft { CBProcessor::fftOrder };
    dsp::WindowingFunction<float> window { (size_t) CBProcessor::fftSize, dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * CBProcessor::fftSize> fftData {};

    float bands[numBands] = {};
    float dispL = 0, dispR = 0, bass = 0, bassEnv = 0, treble = 0;
    float coneX = 0, coneV = 0;          // spring-driven subwoofer cone
    float keyDisp[128] = {};

    Col cols[numCols];
    Array<Star> stars;
    Random rng;

    File skinFolder;
    Array<File> skins;
    int skinIndex = -1;
    Image skin;

    Colour accent { 0xffff1a1a };
    bool showMatrix = true, showStars = true;

    // clickable areas (design space)
    Rectangle<float> btnOn   { 54, 306, 74, 28 },   btnOff  { 132, 306, 74, 28 };
    Rectangle<float> btnPrev { 1090, 354, 62, 26 }, btnNext { 1160, 354, 62, 26 };
    Rectangle<float> btnDice { 1130, 400, 44, 44 };
    Rectangle<float> btnMinus { 1090, 466, 62, 26 }, btnPlus { 1160, 466, 62, 26 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CBEditor)
};
