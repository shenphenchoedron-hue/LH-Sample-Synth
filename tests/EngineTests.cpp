#include <juce_core/juce_core.h>

#include "TestHelpers.h"
#include "engine/Envelope.h"
#include "engine/SampleRegion.h"

using namespace lhss;
using namespace lhss::test;

namespace
{
/** Plays one note (no note-off) and returns the number of output samples until the engine is silent. */
int measureLifetime (InstrumentEngine& engine, int note, std::vector<float>* capture = nullptr, int maxSeconds = 6)
{
    const int block = 256;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    noteOn (engine, note);
    const int limit = static_cast<int> (engine.getSampleRate() * maxSeconds);
    int total = 0;
    while (engine.getActiveVoiceCount() > 0 && total < limit)
    {
        engine.process (buf, midi);
        if (capture != nullptr)
            for (int i = 0; i < block; ++i) capture->push_back (buf.getSample (0, i));
        total += block;
    }
    return total;
}

double goertzelPower (const std::vector<float>& x, size_t from, size_t to, double freq, double sr)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = from; i < to && i < x.size(); ++i)
    {
        const double s = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

struct EngineFixture
{
    SampleStore store;
    InstrumentEngine engine;
    EngineParams params;

    explicit EngineFixture (double hostRate = 48000.0, std::shared_ptr<const SampleData> sample = makeSineSample (220.0, 1.2, 48000.0))
    {
        engine.prepare (hostRate, 256);
        params.attackMs = 1.0f;
        params.sustain = 1.0f;
        params.releaseMs = 20.0f;
        params.grainPosRandom = 0.0f;
        params.stereoSpread = 0.0f;
        params.pitchRandom = 0.0f;
        engine.setParameters (params);
        store.publish (sample, engine);
        runBlocks (engine, 1);
    }

    void apply() { engine.setParameters (params); }
};
} // namespace

//==============================================================================
class PitchMathTests final : public juce::UnitTest
{
public:
    PitchMathTests() : juce::UnitTest ("Pitch ratio & sample-rate conversion", "LHSampleSynth") {}

    void runTest() override
    {
        beginTest ("Pitch ratio calculation");
        expectWithinAbsoluteError (pitchRatio (60, 60), 1.0, 1e-12);
        expectWithinAbsoluteError (pitchRatio (67, 60), std::pow (2.0, 7.0 / 12.0), 1e-12);
        expectWithinAbsoluteError (pitchRatio (61, 60), 1.0594630943592953, 1e-12);

        beginTest ("Octave up");
        expectWithinAbsoluteError (pitchRatio (72, 60), 2.0, 1e-12);
        expectWithinAbsoluteError (pitchRatio (84, 60), 4.0, 1e-12);

        beginTest ("Octave down");
        expectWithinAbsoluteError (pitchRatio (48, 60), 0.5, 1e-12);
        expectWithinAbsoluteError (pitchRatio (36, 60), 0.25, 1e-12);

        beginTest ("Sample-rate conversion factor");
        expectWithinAbsoluteError (sourceIncrement (48000.0, 48000.0), 1.0, 1e-12);
        expectWithinAbsoluteError (sourceIncrement (44100.0, 88200.0), 0.5, 1e-12);
        expectWithinAbsoluteError (sourceIncrement (96000.0, 48000.0), 2.0, 1e-12);
        expectWithinAbsoluteError (sourceIncrement (48000.0, 44100.0), 48000.0 / 44100.0, 1e-12);

        beginTest ("Natural mode at root keeps duration for every host rate");
        for (double host : { 44100.0, 48000.0, 88200.0, 96000.0 })
        {
            auto fp = std::make_unique<EngineFixture> (host); auto& f = *fp;
            f.params.playbackMode = PlaybackMode::Natural;
            f.apply();
            const double seconds = measureLifetime (f.engine, 60) / host;
            expectWithinAbsoluteError (seconds, 1.2, 0.02, "host rate " + juce::String (host));
        }

        beginTest ("Natural mode resamples (octave up = half duration, octave down = double)");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.playbackMode = PlaybackMode::Natural;
            f.apply();
            expectWithinAbsoluteError (measureLifetime (f.engine, 72) / 48000.0, 0.6, 0.02);
            expectWithinAbsoluteError (measureLifetime (f.engine, 48) / 48000.0, 2.4, 0.02);
        }

        beginTest ("Velocity curve");
        expectWithinAbsoluteError (velocityToGain (1.0f, 1.0f), 1.0f, 1e-6f);
        expect (velocityToGain (0.5f, 1.0f) < 0.4f && velocityToGain (0.5f, 1.0f) > 0.25f);
        expectWithinAbsoluteError (velocityToGain (0.1f, 0.0f), 1.0f, 1e-6f);
    }
};

//==============================================================================
class GranularPitchTests final : public juce::UnitTest
{
public:
    GranularPitchTests() : juce::UnitTest ("Granular duration-preserving pitch", "LHSampleSynth") {}

    void runTest() override
    {
        beginTest ("Pitch mode keeps duration at C2, C3 and C4");
        for (int note : { 48, 60, 72 })
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.playbackMode = PlaybackMode::Pitch;
            f.apply();
            const double seconds = measureLifetime (f.engine, note) / 48000.0;
            expectWithinAbsoluteError (seconds, 1.2, 0.12, "note " + juce::String (note) + " lasted " + juce::String (seconds));
        }

        beginTest ("Pitch mode actually transposes (octave up / down)");
        for (auto [note, expected, other] : { std::tuple { 72, 440.0, 220.0 }, std::tuple { 48, 110.0, 220.0 } })
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.playbackMode = PlaybackMode::Pitch;
            f.apply();
            std::vector<float> out;
            measureLifetime (f.engine, note, &out);
            const double pTarget = goertzelPower (out, 9600, 43200, expected, 48000.0);
            {
                double best = 0, bestF = 0, rms = 0;
                for (size_t i = 9600; i < 43200 && i < out.size(); ++i) rms += out[i] * out[i];
                for (double fr = 50; fr < 1000; fr += 5) { auto pw = goertzelPower (out, 9600, 43200, fr, 48000.0); if (pw > best) { best = pw; bestF = fr; } }
                logMessage ("size " + juce::String ((int) out.size()) + " rms " + juce::String (rms) + " peakF " + juce::String (bestF));
            }
            const double pSource = goertzelPower (out, 9600, 43200, other, 48000.0);
            expect (pTarget > pSource * 20.0, "note " + juce::String (note) + " target/source power ratio "
                                                  + juce::String (pTarget / juce::jmax (1e-12, pSource)));
        }

        beginTest ("Freeze keeps sounding beyond the sample length and still releases");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.freeze = true;
            f.params.triggerMode = TriggerMode::Gate;
            f.apply();
            noteOn (f.engine, 64);
            runBlocks (f.engine, static_cast<int> (2.5 * 48000 / 256));
            expectEquals (f.engine.getActiveVoiceCount(), 1);
            noteOff (f.engine, 64);
            runBlocks (f.engine, 40);
            expectEquals (f.engine.getActiveVoiceCount(), 0);
        }

        beginTest ("Realtime budget: 16 voices, Pitch mode + formant");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.formant = 0.8f;
            f.params.triggerMode = TriggerMode::Gate;
            f.params.loopOn = true;
            f.apply();
            for (int n = 0; n < 16; ++n) noteOn (f.engine, 48 + n);
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            const int blocks = 48000 * 2 / 256; // 2 s of audio
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            for (int b = 0; b < blocks; ++b) f.engine.process (buf, midi);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            logMessage ("16 voices, 2 s audio rendered in " + juce::String (ms, 1) + " ms (realtime factor "
                        + juce::String (2000.0 / ms, 1) + "x)");
            expect (ms < 2000.0, "must render faster than realtime");

        }

        beginTest ("Formant processing stays finite and bounded");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.formant = 1.0f;
            f.apply();
            std::vector<float> out;
            measureLifetime (f.engine, 79, &out);
            float peak = 0.0f;
            bool finite = true;
            for (float v : out) { peak = juce::jmax (peak, std::abs (v)); finite = finite && std::isfinite (v); }
            expect (finite);
            expect (peak > 0.01f && peak <= 1.0f, "peak " + juce::String (peak));
        }
    }
};

//==============================================================================
class RegionTests final : public juce::UnitTest
{
public:
    RegionTests() : juce::UnitTest ("Sample region & loop bounds", "LHSampleSynth") {}

    void runTest() override
    {
        beginTest ("Sample region validation");
        {
            auto r = resolveRegion (48000, 48000.0, 0.25f, 0.75f, false, 0, 1, 0);
            expectEquals (r.start, 12000.0);
            expectEquals (r.end, 36000.0);

            r = resolveRegion (48000, 48000.0, 0.8f, 0.2f, false, 0, 1, 0); // inverted
            expect (r.start < r.end);

            r = resolveRegion (48000, 48000.0, 0.5f, 0.5f, false, 0, 1, 0); // equal
            expect (r.end - r.start >= kMinRegionFrames);

            r = resolveRegion (48000, 48000.0, 1.0f, 1.0f, false, 0, 1, 0); // both at end
            expect (r.start < r.end && r.end <= 48000.0 && r.start >= 0.0);

            r = resolveRegion (48000, 48000.0, -3.0f, 7.0f, false, 0, 1, 0); // out of range
            expectEquals (r.start, 0.0);
            expectEquals (r.end, 48000.0);

            r = resolveRegion (48000, 48000.0, std::numeric_limits<float>::quiet_NaN(), 1.0f, false, 0, 1, 0);
            expect (r.start >= 0.0 && r.start < r.end);
        }

        beginTest ("Loop bounds stay inside the active region");
        {
            auto r = resolveRegion (48000, 48000.0, 0.2f, 0.6f, true, 0.0f, 1.0f, 10.0f);
            expect (r.loopOn);
            expectEquals (r.loopStart, r.start);
            expectEquals (r.loopEnd, r.end);

            r = resolveRegion (48000, 48000.0, 0.0f, 1.0f, true, 0.7f, 0.3f, 0.0f); // inverted loop
            expect (r.loopStart < r.loopEnd);

            r = resolveRegion (48000, 48000.0, 0.0f, 1.0f, true, 0.5f, 0.5f, 0.0f); // zero-length loop
            expect (r.loopEnd - r.loopStart >= kMinLoopFrames);

            r = resolveRegion (48000, 48000.0, 0.0f, 1.0f, true, 0.4f, 0.5f, 5000.0f); // huge crossfade
            expect (r.crossfade <= (r.loopEnd - r.loopStart) * 0.5 + 1e-9);
            expect (r.crossfade <= r.loopStart);
        }

        beginTest ("Gate loop sustains past the sample and releases on note-off (Natural & Pitch, fwd & rev)");
        for (int variant = 0; variant < 4; ++variant)
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.loopOn = true;
            f.params.loopStart = 0.3f;
            f.params.loopEnd = 0.6f;
            f.params.triggerMode = TriggerMode::Gate;
            f.params.playbackMode = (variant & 1) ? PlaybackMode::Pitch : PlaybackMode::Natural;
            f.params.reverse = (variant & 2) != 0;
            f.apply();
            noteOn (f.engine, 60);
            runBlocks (f.engine, static_cast<int> (3.0 * 48000 / 256));
            expectEquals (f.engine.getActiveVoiceCount(), 1, "variant " + juce::String (variant));
            const auto& ph = f.engine.getVoiceManager().getVoices()[0].getPlayhead();
            expect (ph.position >= 0.3 * 57600 - 2 && ph.position <= 0.6 * 57600 + 2, "variant " + juce::String (variant));
            noteOff (f.engine, 60);
            runBlocks (f.engine, 60);
            expectEquals (f.engine.getActiveVoiceCount(), 0);
        }

        beginTest ("One Shot loop holds while key is down, plays out after note-off");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.loopOn = true;
            f.params.loopStart = 0.2f;
            f.params.loopEnd = 0.4f;
            f.params.playbackMode = PlaybackMode::Natural;
            f.apply();
            noteOn (f.engine, 60);
            runBlocks (f.engine, static_cast<int> (2.0 * 48000 / 256));
            expectEquals (f.engine.getActiveVoiceCount(), 1);
            noteOff (f.engine, 60);
            runBlocks (f.engine, static_cast<int> (1.0 * 48000 / 256));
            expectEquals (f.engine.getActiveVoiceCount(), 0);
        }

        beginTest ("Reverse bounds");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.reverse = true;
            f.params.playbackMode = PlaybackMode::Natural;
            f.params.sampleStart = 0.25f;
            f.params.sampleEnd = 0.75f;
            f.apply();
            noteOn (f.engine, 60);
            const auto& v = f.engine.getVoiceManager().getVoices()[0];
            const double start = 0.25 * 57600, end = 0.75 * 57600;
            expectWithinAbsoluteError (v.getPlayhead().position, end - 1.0, 1e-9);
            expectEquals (v.getPlayhead().direction, -1);
            bool inside = true;
            double last = v.getPlayhead().position;
            bool monotonic = true;
            int blocks = 0;
            while (f.engine.getActiveVoiceCount() > 0 && blocks++ < 1000)
            {
                runBlocks (f.engine, 1);
                if (! v.isActive()) break;
                const double p = v.getPlayhead().position;
                inside = inside && p >= start && p <= end;
                monotonic = monotonic && p <= last;
                last = p;
            }
            expect (inside, "reverse playhead left the region");
            expect (monotonic, "reverse playhead moved forward");
            expectWithinAbsoluteError (blocks * 256 / 48000.0, 0.6, 0.02);
        }
    }
};

//==============================================================================
class VoiceTests final : public juce::UnitTest
{
public:
    VoiceTests() : juce::UnitTest ("Voice allocation & stealing", "LHSampleSynth") {}

    void runTest() override
    {
        beginTest ("Voice allocation: 16 independent voices");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.triggerMode = TriggerMode::Gate;
            f.apply();
            for (int n = 0; n < 16; ++n) noteOn (f.engine, 40 + n);
            expectEquals (f.engine.getVoiceManager().getSoundingVoiceCount(), 16);
            juce::Array<int> notes;
            for (const auto& v : f.engine.getVoiceManager().getVoices())
                if (v.isActive()) notes.addIfNotAlreadyThere (v.getNote());
            expectEquals (notes.size(), 16);
            runBlocks (f.engine, 4);
            expectEquals (f.engine.getVoiceManager().getSoundingVoiceCount(), 16);
        }

        beginTest ("Voice stealing prefers oldest voice and fades it");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.triggerMode = TriggerMode::Gate;
            f.apply();
            for (int n = 0; n < 16; ++n) noteOn (f.engine, 40 + n);
            noteOn (f.engine, 90);
            const auto& vm = f.engine.getVoiceManager();
            expectEquals (vm.getSoundingVoiceCount(), 16);
            bool oldestKilled = false, newPlaying = false;
            for (const auto& v : vm.getVoices())
            {
                if (v.isActive() && v.getNote() == 40) oldestKilled = v.isKilling();
                if (v.isActive() && v.getNote() == 90) newPlaying = ! v.isKilling();
            }
            expect (oldestKilled, "oldest voice should fade out");
            expect (newPlaying);
            runBlocks (f.engine, 4); // 4 ms fade done
            expectEquals (vm.getActiveVoiceCount(), 16);
        }

        beginTest ("Voice stealing prefers released voices");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.triggerMode = TriggerMode::Gate;
            f.params.releaseMs = 5000.0f;
            f.apply();
            for (int n = 0; n < 16; ++n) noteOn (f.engine, 40 + n);
            noteOff (f.engine, 50);
            noteOn (f.engine, 91);
            for (const auto& v : f.engine.getVoiceManager().getVoices())
            {
                if (v.isActive() && v.getNote() == 50) expect (v.isKilling(), "released voice should be stolen");
                if (v.isActive() && v.getNote() == 40) expect (! v.isKilling(), "oldest held voice must survive");
            }
        }

        beginTest ("Parameter clamping: polyphony and root note limits");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.triggerMode = TriggerMode::Gate;
            f.params.polyphony = 400;
            f.params.rootNote = -20;
            f.params.outputGainDb = 300.0f;
            f.params.cutoffHz = 1.0e9f;
            f.apply();
            for (int n = 0; n < 30; ++n) noteOn (f.engine, 30 + n);
            expect (f.engine.getVoiceManager().getSoundingVoiceCount() <= kMaxPolyphony);

            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            float peak = 0.0f;
            for (int b = 0; b < 20; ++b)
            {
                f.engine.process (buf, midi);
                peak = juce::jmax (peak, buf.getMagnitude (0, 256));
            }
            expect (peak <= 1.0f, "limiter must keep output <= 0 dBFS");

            f.params.polyphony = 2;
            f.apply();
            noteOn (f.engine, 100);
            expectEquals (f.engine.getVoiceManager().getSoundingVoiceCount(), 2);
        }

        beginTest ("MIDI events are rendered at their sample offset; variable block sizes");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.attackMs = 0.5f;
            f.apply();
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 300);
            f.engine.process (buf, midi);
            expectEquals (buf.getMagnitude (0, 0, 300), 0.0f);
            expect (buf.getMagnitude (0, 300, 212) > 0.0f);

            for (int size : { 1, 7, 64, 333, 1000, 4096 }) // larger than the prepared 256 too
            {
                juce::AudioBuffer<float> b (2, size);
                juce::MidiBuffer m;
                m.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), size - 1);
                f.engine.process (b, m);
                bool finite = true;
                for (int i = 0; i < size; ++i) finite = finite && std::isfinite (b.getSample (0, i));
                expect (finite);
            }
        }

        beginTest ("Sample replacement during playback is safe");
        {
            auto fp = std::make_unique<EngineFixture>(); auto& f = *fp;
            f.params.triggerMode = TriggerMode::Gate;
            f.apply();
            noteOn (f.engine, 60);
            const auto* first = f.engine.getAcknowledgedSample();
            auto second = makeSineSample (330.0, 0.5, 44100.0, 2);
            f.store.publish (second, f.engine);
            runBlocks (f.engine, 2);
            expect (f.engine.getAcknowledgedSample() == second.get());
            noteOn (f.engine, 64);

            const SampleData* s60 = nullptr; const SampleData* s64 = nullptr;
            for (const auto& v : f.engine.getVoiceManager().getVoices())
            {
                if (v.isActive() && v.getNote() == 60) s60 = v.getSample();
                if (v.isActive() && v.getNote() == 64) s64 = v.getSample();
            }
            expect (s60 == first, "old voice keeps the old sample");
            expect (s64 == second.get(), "new voice uses the new sample");

            f.store.collectGarbage (f.engine);
            expectEquals ((int) f.store.retainedCount(), 2, "old sample still in use must be retained");
            noteOff (f.engine, 60);
            noteOff (f.engine, 64);
            runBlocks (f.engine, 20);
            f.store.collectGarbage (f.engine);
            expectEquals ((int) f.store.retainedCount(), 1);
        }
    }
};

//==============================================================================
class EnvelopeTests final : public juce::UnitTest
{
public:
    EnvelopeTests() : juce::UnitTest ("ADSR", "LHSampleSynth") {}

    void runTest() override
    {
        beginTest ("ADSR transitions");
        Envelope env;
        env.setSampleRate (1000.0);
        env.setParameters (10.0f, 50.0f, 0.5f, 100.0f); // 10 / 50 / 0.5 / 100 samples
        expect (env.getStage() == Envelope::Stage::Idle);
        env.noteOn();
        expect (env.getStage() == Envelope::Stage::Attack);
        for (int i = 0; i < 10; ++i) env.next();
        expect (env.getStage() == Envelope::Stage::Decay);
        expectWithinAbsoluteError (env.getLevel(), 1.0f, 1e-5f);
        for (int i = 0; i < 200; ++i) env.next();
        expect (env.getStage() == Envelope::Stage::Sustain);
        expectWithinAbsoluteError (env.getLevel(), 0.5f, 1e-3f);
        env.noteOff();
        expect (env.getStage() == Envelope::Stage::Release);
        int n = 0;
        while (env.isActive() && n < 1000) { env.next(); ++n; }
        expect (! env.isActive());
        expect (n > 50 && n < 300, "release length " + juce::String (n));

        beginTest ("Retrigger starts from current level (no jump) and kill fades");
        env.noteOn();
        for (int i = 0; i < 5; ++i) env.next();
        const float mid = env.getLevel();
        env.noteOn();
        expect (std::abs (env.next() - mid) < 0.2f);
        env.kill (5.0f);
        expect (env.getStage() == Envelope::Stage::Kill);
        for (int i = 0; i < 6; ++i) env.next();
        expect (! env.isActive());

        beginTest ("Zero sustain ends the voice");
        env.setParameters (1.0f, 10.0f, 0.0f, 10.0f);
        env.noteOn();
        for (int i = 0; i < 500; ++i) env.next();
        expect (! env.isActive());
    }
};

static PitchMathTests pitchMathTests;
static GranularPitchTests granularPitchTests;
static RegionTests regionTests;
static VoiceTests voiceTests;
static EnvelopeTests envelopeTests;
