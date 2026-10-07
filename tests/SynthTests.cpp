#include <juce_core/juce_core.h>

#include "TestHelpers.h"
#include "dsp/Effects.h"
#include "dsp/Lfo.h"

using namespace lhss;
using namespace lhss::test;

namespace
{
struct Rig
{
    SampleStore store;
    InstrumentEngine engine;
    EngineParams params;

    explicit Rig (double toneHz = 220.0, double seconds = 1.2)
    {
        engine.prepare (48000.0, 256);
        params.attackMs = 1.0f;
        params.sustain = 1.0f;
        params.releaseMs = 20.0f;
        params.grainPosRandom = 0.0f;
        params.stereoSpread = 0.0f;
        params.triggerMode = TriggerMode::Gate;
        engine.setParameters (params);
        store.publish (makeSineSample (toneHz, seconds, 48000.0), engine);
        runBlocks (engine, 1);
    }
    void apply() { engine.setParameters (params); }

    /** Renders `seconds` of left-channel output. */
    std::vector<float> render (double seconds)
    {
        std::vector<float> out;
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        for (int b = 0; b < static_cast<int> (seconds * 48000 / 256); ++b)
        {
            engine.process (buf, midi);
            for (int i = 0; i < 256; ++i) out.push_back (buf.getSample (0, i));
        }
        return out;
    }

    std::vector<const Voice*> sounding() const
    {
        std::vector<const Voice*> v;
        for (const auto& voice : engine.getVoiceManager().getVoices())
            if (voice.isActive() && ! voice.isKilling()) v.push_back (&voice);
        return v;
    }
};

double rms (const std::vector<float>& x, double fromSec, double toSec)
{
    const auto a = static_cast<size_t> (fromSec * 48000), b = std::min (x.size(), static_cast<size_t> (toSec * 48000));
    double acc = 0.0;
    for (size_t i = a; i < b; ++i) acc += x[i] * x[i];
    return std::sqrt (acc / std::max<size_t> (1, b - a));
}
} // namespace

class SynthTests final : public juce::UnitTest
{
public:
    SynthTests() : juce::UnitTest ("Synth section (LFO, filter env, voice, effects)", "LHSampleSynth") {}

    void runTest() override
    {
        beginTest ("LFO shapes stay in range and have the right period");
        {
            dsp::Lfo lfo;
            lfo.prepare (1000.0);
            for (int shape = 0; shape < 5; ++shape)
            {
                lfo.reset();
                float lo = 2.0f, hi = -2.0f;
                for (int i = 0; i < 4000; ++i)
                {
                    const float v = lfo.advance (static_cast<dsp::Lfo::Shape> (shape), 2.0, 1);
                    lo = juce::jmin (lo, v);
                    hi = juce::jmax (hi, v);
                }
                expect (lo >= -1.0f && hi <= 1.0f, "shape " + juce::String (shape));
                expect (hi - lo > 0.5f, "shape " + juce::String (shape) + " must move");
            }
            lfo.reset();
            lfo.advance (dsp::Lfo::Shape::Sine, 1.0, 250);
            expectWithinAbsoluteError (lfo.advance (dsp::Lfo::Shape::Sine, 1.0, 1), 1.0f, 1e-3f);
            expectWithinAbsoluteError (syncDivisionSeconds (2, 120.0), 0.5, 1e-9);   // 1/4 at 120 bpm
            expectWithinAbsoluteError (syncDivisionSeconds (12, 120.0), 0.375, 1e-9); // 1/8. at 120 bpm
        }

        beginTest ("Coarse tune transposes (Natural: +12 st halves duration)");
        {
            auto r = std::make_unique<Rig>();
            r->params.playbackMode = PlaybackMode::Natural;
            r->params.triggerMode = TriggerMode::OneShot;
            r->params.coarseTune = 12;
            r->apply();
            noteOn (r->engine, 60);
            int blocks = 0;
            while (r->engine.getActiveVoiceCount() > 0 && blocks < 1000) { runBlocks (r->engine, 1); ++blocks; }
            expectWithinAbsoluteError (blocks * 256 / 48000.0, 0.6, 0.02);
        }

        beginTest ("Poly glide moves the new voice from the previous note");
        {
            auto r = std::make_unique<Rig>();
            r->params.glideMs = 100.0f;
            r->apply();
            noteOn (r->engine, 60);
            runBlocks (r->engine, 2);
            noteOn (r->engine, 72);
            const Voice* v72 = nullptr;
            for (auto* v : r->sounding()) if (v->getNote() == 72) v72 = v;
            expect (v72 != nullptr);
            if (v72 != nullptr)
            {
                expectWithinAbsoluteError (v72->getCurrentNote(), 60.0, 0.01);
                runBlocks (r->engine, 10); // ~53 ms
                expect (v72->getCurrentNote() > 64.0 && v72->getCurrentNote() < 70.0);
                runBlocks (r->engine, 20);
                expectWithinAbsoluteError (v72->getCurrentNote(), 72.0, 1e-6);
            }
        }

        beginTest ("Mono mode: one voice, returns to held note on release");
        {
            auto r = std::make_unique<Rig>();
            r->params.voiceMode = VoiceMode::Mono;
            r->apply();
            noteOn (r->engine, 60);
            noteOn (r->engine, 64);
            runBlocks (r->engine, 2);
            auto s = r->sounding();
            expectEquals ((int) s.size(), 1);
            if (! s.empty()) expectEquals (s[0]->getNote(), 64);
            noteOff (r->engine, 64);
            s = r->sounding();
            expectEquals ((int) s.size(), 1);
            if (! s.empty()) expectEquals (s[0]->getNote(), 60);
            noteOff (r->engine, 60);
            runBlocks (r->engine, 20);
            expectEquals (r->engine.getActiveVoiceCount(), 0);
        }

        beginTest ("Legato mode glides without retriggering");
        {
            auto r = std::make_unique<Rig>();
            r->params.voiceMode = VoiceMode::Legato;
            r->params.glideMs = 50.0f;
            r->apply();
            noteOn (r->engine, 60);
            runBlocks (r->engine, 4);
            const auto* first = r->sounding().front();
            const auto age = first->getAge();
            noteOn (r->engine, 67);
            const auto s = r->sounding();
            expectEquals ((int) s.size(), 1);
            expect (s.front() == first && s.front()->getAge() == age, "legato must reuse the voice");
            expectEquals (s.front()->getNote(), 67);
            expect (s.front()->getCurrentNote() < 61.0, "glide starts at the old pitch");
        }

        beginTest ("Unison starts several detuned voices per note");
        {
            auto r = std::make_unique<Rig>();
            r->params.unisonVoices = 4;
            r->apply();
            noteOn (r->engine, 60);
            expectEquals ((int) r->sounding().size(), 4);
            for (int n = 0; n < 12; ++n) noteOn (r->engine, 62 + n);
            expect ((int) r->sounding().size() <= VoiceManager::kMaxSounding);
            const auto out = r->render (0.2);
            float peak = 0.0f;
            for (float v : out) peak = juce::jmax (peak, std::abs (v));
            expect (peak <= 1.0f && peak > 0.01f);
        }

        beginTest ("Filter envelope opens a closed low-pass");
        {
            auto r = std::make_unique<Rig> (3000.0, 2.0);
            r->params.playbackMode = PlaybackMode::Natural;
            r->params.cutoffHz = 150.0f;
            r->params.resonance = 0.0f;
            r->params.fenvAmount = 1.0f;
            r->params.fenvDecayMs = 150.0f;
            r->apply();
            noteOn (r->engine, 60);
            const auto out = r->render (1.0);
            const double early = rms (out, 0.005, 0.03), late = rms (out, 0.7, 0.9);
            expect (early > late * 4.0, "early " + juce::String (early) + " late " + juce::String (late));
        }

        beginTest ("Velocity > Filter darkens soft notes");
        {
            double loud = 0.0, soft = 0.0;
            for (int vel : { 127, 20 })
            {
                auto r = std::make_unique<Rig> (3000.0, 1.0);
                r->params.playbackMode = PlaybackMode::Natural;
                r->params.velocitySensitivity = 0.0f;
                r->params.cutoffHz = 4000.0f;
                r->params.velToFilter = 1.0f;
                r->apply();
                noteOn (r->engine, 60, vel);
                const auto out = r->render (0.4);
                (vel == 127 ? loud : soft) = rms (out, 0.1, 0.3);
            }
            expect (loud > soft * 3.0, "loud " + juce::String (loud) + " soft " + juce::String (soft));
        }

        beginTest ("LFO > Amp produces tremolo, Drive stays bounded");
        {
            auto r = std::make_unique<Rig>();
            r->params.playbackMode = PlaybackMode::Natural;
            r->params.loopOn = true;
            r->params.lfoOn = true;
            r->params.lfoToAmp = 1.0f;
            r->params.lfoRateHz = 5.0f;
            r->params.drive = 1.0f;
            r->apply();
            noteOn (r->engine, 60);
            const auto out = r->render (1.0);
            double lo = 1e9, hi = 0.0;
            for (double t = 0.1; t < 0.9; t += 0.02) { const double v = rms (out, t, t + 0.02); lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); }
            expect (hi > lo * 3.0, "tremolo depth");
            bool ok = true;
            for (float v : out) ok = ok && std::isfinite (v) && std::abs (v) <= 1.0f;
            expect (ok);
        }

        beginTest ("LFO switched off: no modulation, whatever the amounts are");
        {
            auto renderWith = [] (bool lfoAmounts)
            {
                auto r = std::make_unique<Rig>();
                r->params.playbackMode = PlaybackMode::Natural;
                r->params.loopOn = true;
                r->params.lfoOn = false;
                r->params.lfoRateHz = 5.0f;
                if (lfoAmounts)
                {
                    r->params.lfoToAmp = 1.0f;
                    r->params.lfoToPitch = 12.0f;
                    r->params.lfoToPan = 1.0f;
                    r->params.lfoToCutoff = 1.0f;
                    r->params.lfoToGrainPos = 1.0f;
                }
                r->apply();
                noteOn (r->engine, 60);
                return r->render (1.0);
            };
            const auto withAmounts = renderWith (true), without = renderWith (false);
            double maxDiff = 0.0;
            for (size_t i = 0; i < withAmounts.size() && i < without.size(); ++i)
                maxDiff = juce::jmax (maxDiff, (double) std::abs (withAmounts[i] - without[i]));
            expect (! withAmounts.empty() && withAmounts.size() == without.size());
            expectLessOrEqual (maxDiff, 1.0e-6, "output identical to no LFO at all");
        }

        beginTest ("Delay repeats after the set time; chorus at 0 % is transparent");
        {
            dsp::StereoDelay d;
            d.prepare (1000.0, 2.5);
            std::vector<float> l (600, 0.0f), r (600, 0.0f);
            l[0] = r[0] = 1.0f;
            d.process (l.data(), r.data(), 600, 0.1, 0.5f, 1.0f, false);
            expectWithinAbsoluteError (l[100], 1.0f, 0.05f);
            expect (l[200] > 0.2f && l[200] < 0.6f, "feedback echo");
            expectEquals (l[50], 0.0f);

            dsp::Chorus c;
            c.prepare (48000.0);
            std::vector<float> a (512), b (512);
            for (int i = 0; i < 512; ++i) a[(size_t) i] = b[(size_t) i] = std::sin (i * 0.1f);
            const auto ref = a;
            c.process (a.data(), b.data(), 512, 1.0f, 1.0f, 0.0f);
            expect (a == ref);
        }

        beginTest ("Reverb adds a tail after the note ends");
        {
            auto r = std::make_unique<Rig> (440.0, 0.2);
            r->params.playbackMode = PlaybackMode::Natural;
            r->params.triggerMode = TriggerMode::OneShot;
            r->params.reverbMix = 0.6f;
            r->apply();
            noteOn (r->engine, 60);
            const auto out = r->render (1.0);
            expect (rms (out, 0.4, 0.6) > 1.0e-4, "reverb tail");
        }
    }
};

static SynthTests synthTests;
