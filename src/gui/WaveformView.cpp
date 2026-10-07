#include "WaveformView.h"
#include "LHLookAndFeel.h"
#include "../parameters/ParameterIDs.h"

namespace lhss::gui
{
namespace
{
constexpr float kMinGap = 0.002f;

const juce::Colour markerColours[] = { colours::green, colours::blue, colours::orange, colours::pink };
const char* markerNames[] = { "Sample Start", "Sample End", "Loop Start", "Loop End" };
} // namespace

WaveformView::WaveformView (juce::AudioProcessorValueTreeState& state)
{
    params[SampleStart] = state.getParameter (ids::sampleStart);
    params[SampleEnd]   = state.getParameter (ids::sampleEnd);
    params[LoopStart]   = state.getParameter (ids::loopStart);
    params[LoopEnd]     = state.getParameter (ids::loopEnd);
    loopOnParam         = state.getParameter (ids::loopOn);
    setRepaintsOnMouseActivity (false);
}

void WaveformView::setSample (std::shared_ptr<const SampleData> s)
{
    sample = std::move (s);
    repaint();
}

void WaveformView::setMessage (const juce::String& text, bool isError)
{
    message = text;
    messageIsError = isError;
    repaint();
}

float WaveformView::value (int m) const { return params[(size_t) m]->convertFrom0to1 (params[(size_t) m]->getValue()); }

void WaveformView::refreshIfParametersChanged()
{
    bool changed = false;
    for (int m = 0; m < NumMarkers; ++m)
        if (! juce::approximatelyEqual (lastValues[(size_t) m], value (m))) { lastValues[(size_t) m] = value (m); changed = true; }
    const float lo = loopOnParam->getValue();
    if (! juce::approximatelyEqual (lastValues[NumMarkers], lo)) { lastValues[NumMarkers] = lo; changed = true; }
    if (changed) repaint();
}

juce::Rectangle<float> WaveformView::waveArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop ((float) kLabelArea).withTrimmedBottom (6.0f);
}

float WaveformView::xForValue (float v) const
{
    const auto a = waveArea().reduced (10.0f, 0.0f);
    return a.getX() + v * a.getWidth();
}

float WaveformView::valueForX (float x) const
{
    const auto a = waveArea().reduced (10.0f, 0.0f);
    return juce::jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth());
}

int WaveformView::hitTest (float x) const
{
    const bool loopOn = loopOnParam->getValue() >= 0.5f;
    int best = None;
    float bestDist = 9.0f;
    const int order[] = { LoopStart, LoopEnd, SampleStart, SampleEnd };
    for (int m : order)
    {
        if (! loopOn && (m == LoopStart || m == LoopEnd)) continue;
        const float d = std::abs (xForValue (value (m)) - x);
        if (d < bestDist) { bestDist = d; best = m; }
    }
    return best;
}

void WaveformView::paint (juce::Graphics& g)
{
    const auto area = waveArea();
    g.setColour (colours::boxBg);
    g.fillRoundedRectangle (area, 10.0f);
    g.setColour (juce::Colour (0xffdde8f1));
    g.drawRoundedRectangle (area, 10.0f, 1.0f);

    const bool loopOn = loopOnParam->getValue() >= 0.5f;
    const float xs = xForValue (value (SampleStart)), xe = xForValue (value (SampleEnd));
    const float xls = xForValue (juce::jlimit (value (SampleStart), value (SampleEnd), value (LoopStart)));
    const float xle = xForValue (juce::jlimit (value (SampleStart), value (SampleEnd), value (LoopEnd)));

    // Region shading (as in the reference design): attack part, loop, tail.
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area.toNearestInt());
        auto band = [&] (float x0, float x1, juce::Colour c, float alpha)
        {
            if (x1 > x0) { g.setColour (c.withAlpha (alpha)); g.fillRect (juce::Rectangle<float>::leftTopRightBottom (x0, area.getY(), x1, area.getBottom())); }
        };
        if (loopOn)
        {
            band (xs, xls, colours::green, 0.08f);
            band (xls, xle, colours::pink, 0.07f);
            band (xle, xe, colours::blue, 0.05f);
        }
        else band (xs, xe, colours::green, 0.08f);
    }

    if (sample != nullptr)
    {
        const auto& mn = sample->getOverviewMin();
        const auto& mx = sample->getOverviewMax();
        const auto inner = area.reduced (10.0f, 12.0f);
        const float mid = inner.getCentreY(), half = inner.getHeight() * 0.5f;
        float peak = 0.0001f;
        for (size_t i = 0; i < mx.size(); ++i) peak = juce::jmax (peak, mx[i], -mn[i]);
        const float scale = half / juce::jmax (peak, 0.25f);

        juce::Path p;
        const int cols = juce::jmax (2, (int) inner.getWidth());
        auto binFor = [&] (int c) { return juce::jlimit (0, SampleData::kOverviewBins - 1, c * SampleData::kOverviewBins / cols); };
        p.startNewSubPath (inner.getX(), mid);
        for (int c = 0; c < cols; ++c)
            p.lineTo (inner.getX() + (float) c, mid - mx[(size_t) binFor (c)] * scale);
        for (int c = cols - 1; c >= 0; --c)
            p.lineTo (inner.getX() + (float) c, mid - mn[(size_t) binFor (c)] * scale);
        p.closeSubPath();
        g.setColour (juce::Colour (0xff8295a9).withAlpha (0.84f));
        g.fillPath (p);

        // Dim everything outside the active region.
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (area.getX() + 1.0f, area.getY() + 1.0f, xs, area.getBottom() - 1.0f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xe, area.getY() + 1.0f, area.getRight() - 1.0f, area.getBottom() - 1.0f));
    }

    if (sample == nullptr || message.isNotEmpty())
    {
        g.setFont (uiFont (13.0f));
        g.setColour (messageIsError ? colours::error : colours::tiny);
        g.drawFittedText (message.isNotEmpty() ? message : juce::String ("Load a WAV / AIFF recording (or drop a file here)"),
                          area.reduced (20.0f, 10.0f).toNearestInt(), juce::Justification::centred, 3);
    }

    // Markers
    for (int m = 0; m < NumMarkers; ++m)
    {
        const bool isLoop = m == LoopStart || m == LoopEnd;
        const float alpha = (isLoop && ! loopOn) ? 0.3f : 1.0f;
        const float v = isLoop ? juce::jlimit (value (SampleStart), value (SampleEnd), value (m)) : value (m);
        const float x = xForValue (v);
        const auto c = markerColours[m].withMultipliedAlpha (alpha);
        const float w = (m == dragging || m == hover) ? 4.0f : 3.0f;

        g.setColour (c);
        g.fillRect (x - w * 0.5f, (float) kLabelArea - 6.0f, w, area.getBottom() - kLabelArea + 6.0f);
        g.fillRoundedRectangle (x - 6.0f, (float) kLabelArea - 17.0f, 12.0f, 12.0f, 3.0f);
        g.setFont (uiFont (9.5f));
        const float lx = juce::jlimit (0.0f, (float) getWidth() - 80.0f, x - 40.0f);
        g.drawText (markerNames[m], juce::Rectangle<float> (lx, 0.0f, 80.0f, 13.0f), juce::Justification::centred, false);
    }
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTest (e.position.x);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h == None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::LeftRightResizeCursor);
        repaint();
    }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTest (e.position.x);
    if (dragging != None) params[(size_t) dragging]->beginChangeGesture();
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == None) return;

    float v = valueForX (e.position.x);
    const float s = value (SampleStart), en = value (SampleEnd);
    switch (dragging)
    {
        case SampleStart: v = juce::jlimit (0.0f, en - kMinGap, v); break;
        case SampleEnd:   v = juce::jlimit (s + kMinGap, 1.0f, v); break;
        case LoopStart:   v = juce::jlimit (s, juce::jmax (s, value (LoopEnd) - kMinGap), v); break;
        case LoopEnd:     v = juce::jlimit (juce::jmin (en, value (LoopStart) + kMinGap), en, v); break;
        default: break;
    }
    auto* p = params[(size_t) dragging];
    p->setValueNotifyingHost (p->convertTo0to1 (v));
    repaint();
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (dragging != None) params[(size_t) dragging]->endChangeGesture();
    dragging = None;
    repaint();
}
} // namespace lhss::gui
