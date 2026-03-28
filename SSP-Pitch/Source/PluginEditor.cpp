#include "PluginEditor.h"

#include "MusicNoteUtils.h"
#include "PitchScale.h"
#include "SSPVectorUI.h"

namespace
{
using namespace ssp::pitch;
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

constexpr int editorWidth = 1440;
constexpr int editorHeight = 920;
constexpr float keyboardWidth = 76.0f;

juce::Colour accentTeal() { return juce::Colour(0xff35d4c9); }
juce::Colour accentCyan() { return juce::Colour(0xff34c6ff); }
juce::Colour accentAmber() { return juce::Colour(0xffffc15a); }
juce::Colour accentCoral() { return juce::Colour(0xffff6d6b); }
juce::Colour softText() { return juce::Colour(0xff90a8bb); }
juce::Colour strongText() { return juce::Colour(0xffedf6ff); }
juce::Colour noteGray() { return juce::Colour(0xff556270); }

juce::String formatPercent(double value)
{
    return juce::String((int) std::round(value)) + "%";
}

juce::String formatFormant(double value)
{
    const auto rounded = juce::roundToInt((float) value);
    return juce::String(rounded > 0 ? "+" : "") + juce::String(rounded) + " st";
}

juce::String formatDb(double value)
{
    return juce::String(value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}

juce::String formatPitchReadout(float midiNote)
{
    return ssp::pitch::formatMidiNote(midiNote, true);
}

class KnobField final : public juce::Component
{
public:
    KnobField(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& titleText)
        : attachment(state, paramId, knob)
    {
        addAndMakeVisible(title);
        addAndMakeVisible(knob);
        title.setText(titleText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, softText());
        title.setFont(juce::Font(11.0f, juce::Font::bold));
        knob.setColour(juce::Slider::rotarySliderFillColourId, accentCyan());
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    }

    void setFormatting(std::function<juce::String(double)> formatter)
    {
        knob.textFromValueFunction = std::move(formatter);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        title.setBounds(area.removeFromTop(16));
        knob.setBounds(area);
    }

private:
    juce::Label title;
    ssp::ui::SSPKnob knob;
    SliderAttachment attachment;
};

class PitchRollComponent final : public juce::Component
{
public:
    enum class Tool
    {
        pointer = 0,
        pitch,
        time,
        duration,
        split,
        merge,
        draw,
        pencil
    };

    explicit PitchRollComponent(PluginProcessor& processorToUse)
        : processor(processorToUse)
    {
        setWantsKeyboardFocus(true);
    }

    void setSession(AnalysisSession newSession)
    {
        session = std::move(newSession);
        repaint();
    }

    void setTool(Tool newTool)
    {
        tool = newTool;
        repaint();
    }

    Tool getTool() const noexcept { return tool; }
    juce::StringArray getSelectedNoteIds() const { return selectedNoteIds; }
    void clearSelection()
    {
        selectedNoteIds.clear();
        repaint();
    }

    void zoomTime(float factor)
    {
        pixelsPerSecond = juce::jlimit(30.0f, 420.0f, pixelsPerSecond * factor);
        repaint();
    }

    void zoomPitch(float factor)
    {
        pixelsPerSemitone = juce::jlimit(8.0f, 46.0f, pixelsPerSemitone * factor);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        ssp::ui::drawPanelBackground(g, bounds, accentCyan(), 14.0f);

        auto inner = bounds.reduced(10.0f);
        auto cursorMeter = inner.removeFromRight(30.0f);
        auto rollArea = inner;
        auto keyboardArea = rollArea.removeFromLeft(keyboardWidth);

        drawGrid(g, rollArea);
        drawBackgroundOverlays(g, rollArea);
        drawKeyboard(g, keyboardArea);
        drawNotes(g, rollArea);
        drawPlaybackCursor(g, rollArea);
        drawDeviationMeter(g, cursorMeter);

        if (session.analyzing)
            drawAnalysisOverlay(g, inner);

        if (session.polyphonicWarning)
        {
            auto warning = inner.removeFromTop(22.0f).removeFromRight(370.0f);
            g.setColour(accentCoral().withAlpha(0.22f));
            g.fillRoundedRectangle(warning, 8.0f);
            g.setColour(accentCoral().brighter(0.1f));
            g.drawRoundedRectangle(warning, 8.0f, 1.0f);
            g.setColour(strongText());
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("Polyphonic audio detected - SSP Pitch works best on monophonic material.", warning.toNearestInt().reduced(10, 0),
                       juce::Justification::centredLeft, false);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto noteId = hitTestNote(event.position);
        dragOrigin = event.position;
        dragPitchDelta = 0.0f;
        dragTimeDelta = 0.0;
        durationStartDelta = 0.0;
        durationEndDelta = 0.0;
        panning = false;
        dragging = false;
        drawing = false;
        resizing = false;

        if (tool == Tool::draw)
        {
            drawing = true;
            drawStartTime = xToTime(event.position.x);
            drawEndTime = drawStartTime;
            drawPitch = yToMidi(event.position.y);
            return;
        }

        if (noteId.isNotEmpty())
        {
            if (! event.mods.isCommandDown() && ! event.mods.isCtrlDown())
                selectedNoteIds.clear();

            if (! selectedNoteIds.contains(noteId))
                selectedNoteIds.add(noteId);

            draggingNoteId = noteId;

            if (tool == Tool::split)
            {
                processor.splitNote(noteId, xToTime(event.position.x));
                return;
            }

            if (tool == Tool::merge)
            {
                if (selectedNoteIds.size() >= 2)
                    processor.mergeNotes(selectedNoteIds);
                return;
            }

            if (tool == Tool::duration)
            {
                resizing = true;
                resizeAffectsStart = event.position.x < timeToX(getNoteById(noteId)->startTime) + 12.0f;
                return;
            }

            dragging = tool != Tool::pencil;
            if (tool == Tool::pencil)
                processor.nudgeNotePitchTrace(noteId, xToTime(event.position.x), yToMidi(event.position.y), 0.05f);

            repaint();
            return;
        }

        selectedNoteIds.clear();
        panning = true;
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (drawing)
        {
            drawEndTime = xToTime(event.position.x);
            drawPitch = yToMidi(event.position.y);
            repaint();
            return;
        }

        if (panning)
        {
            timeOffset -= (event.position.x - dragOrigin.x) / pixelsPerSecond;
            topMidi += (event.position.y - dragOrigin.y) / pixelsPerSemitone;
            dragOrigin = event.position;
            repaint();
            return;
        }

        if (tool == Tool::pencil && draggingNoteId.isNotEmpty())
        {
            processor.nudgeNotePitchTrace(draggingNoteId, xToTime(event.position.x), yToMidi(event.position.y), 0.08f);
            return;
        }

        if (resizing)
        {
            const double delta = (event.position.x - dragOrigin.x) / pixelsPerSecond;
            if (resizeAffectsStart)
                durationStartDelta = delta;
            else
                durationEndDelta = delta;
            repaint();
            return;
        }

        if (dragging && draggingNoteId.isNotEmpty())
        {
            if (tool == Tool::pointer || tool == Tool::pitch)
                dragPitchDelta = (dragOrigin.y - event.position.y) / pixelsPerSemitone;

            if (tool == Tool::pointer || tool == Tool::time)
                dragTimeDelta = (event.position.x - dragOrigin.x) / pixelsPerSecond;

            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (drawing)
        {
            const double startTime = juce::jmin(drawStartTime, drawEndTime);
            const double endTime = juce::jmax(drawStartTime, drawEndTime);
            if (endTime - startTime > 0.04)
                processor.addDrawnNote(startTime, endTime, std::round(drawPitch));
        }
        else if (resizing && draggingNoteId.isNotEmpty())
        {
            processor.resizeNote(draggingNoteId, durationStartDelta, durationEndDelta);
        }
        else if (dragging && draggingNoteId.isNotEmpty())
        {
            processor.moveNote(draggingNoteId, dragPitchDelta, dragTimeDelta);
        }

        dragging = false;
        drawing = false;
        resizing = false;
        panning = false;
        draggingNoteId.clear();
        dragPitchDelta = 0.0f;
        dragTimeDelta = 0.0;
        durationStartDelta = 0.0;
        durationEndDelta = 0.0;
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (juce::ModifierKeys::currentModifiers.isCtrlDown() || juce::ModifierKeys::currentModifiers.isCommandDown())
            zoomPitch(wheel.deltaY > 0 ? 1.08f : 0.93f);
        else
            zoomTime(wheel.deltaY > 0 ? 1.08f : 0.93f);
    }

private:
    const DetectedNote* getNoteById(const juce::String& id) const
    {
        for (const auto& note : session.notes)
            if (note.id == id)
                return &note;
        return nullptr;
    }

    juce::String hitTestNote(juce::Point<float> position) const
    {
        auto roll = getLocalBounds().toFloat().reduced(20.0f);
        roll.removeFromRight(30.0f);
        roll.removeFromLeft((int) keyboardWidth);
        for (const auto& note : session.notes)
        {
            auto noteBounds = getBlobBounds(note);
            if (noteBounds.intersects(roll) && noteBounds.expanded(2.0f).contains(position))
                return note.id;
        }
        return {};
    }

    juce::Rectangle<float> getBlobBounds(const DetectedNote& note) const
    {
        const float x = timeToX(note.startTime + dragTimeAdjustmentForNote(note.id));
        const float width = juce::jmax(8.0f, (float) ((note.endTime - note.startTime) * pixelsPerSecond + durationAdjustmentForNote(note.id, false) - durationAdjustmentForNote(note.id, true)));
        const float y = midiToY(note.correctedPitch + pitchAdjustmentForNote(note.id)) - pixelsPerSemitone * 0.42f;
        return { x, y, width, juce::jmax(14.0f, pixelsPerSemitone * 0.84f) };
    }

    float pitchAdjustmentForNote(const juce::String& noteId) const
    {
        return draggingNoteId == noteId ? dragPitchDelta : 0.0f;
    }

    double dragTimeAdjustmentForNote(const juce::String& noteId) const
    {
        return draggingNoteId == noteId ? dragTimeDelta : 0.0;
    }

    double durationAdjustmentForNote(const juce::String& noteId, bool start) const
    {
        if (draggingNoteId != noteId)
            return 0.0;
        return start ? durationStartDelta * pixelsPerSecond : durationEndDelta * pixelsPerSecond;
    }

    float timeToX(double timeSeconds) const
    {
        return 20.0f + keyboardWidth + (float) ((timeSeconds - timeOffset) * pixelsPerSecond);
    }

    float midiToY(float midiNote) const
    {
        return 20.0f + (topMidi - midiNote) * pixelsPerSemitone;
    }

    double xToTime(float x) const
    {
        return timeOffset + (x - (20.0f + keyboardWidth)) / pixelsPerSecond;
    }

    float yToMidi(float y) const
    {
        return topMidi - (y - 20.0f) / pixelsPerSemitone;
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> rollArea) const
    {
        g.setColour(juce::Colour(0xff0d141d));
        g.fillRoundedRectangle(rollArea, 10.0f);

        const auto keyRoot = (int) processor.apvts.getRawParameterValue("scaleKey")->load();
        const auto modeName = getScaleModeNames()[juce::jlimit(0, getScaleModeNames().size() - 1, (int) processor.apvts.getRawParameterValue("scaleMode")->load())];

        const int visibleTop = juce::roundToInt(topMidi + 2.0f);
        const int visibleBottom = juce::roundToInt(topMidi - rollArea.getHeight() / pixelsPerSemitone - 2.0f);

        for (int midi = visibleTop; midi >= visibleBottom; --midi)
        {
            auto row = juce::Rectangle<float>(rollArea.getX(), midiToY((float) midi), rollArea.getWidth(), pixelsPerSemitone);
            const bool inScale = isMidiNoteInScale(midi, keyRoot, modeName);
            const bool isC = ((midi % 12) + 12) % 12 == 0;
            auto fill = juce::Colour(0xff101923);
            if (inScale)
                fill = juce::Colour(0xff13222d);
            else
                fill = juce::Colour(0xff1a1619);

            if (processor.apvts.getRawParameterValue("showHeatmap")->load() >= 0.5f)
                fill = fill.interpolatedWith(accentCyan(), session.pitchHeatmap[(size_t) juce::jlimit(0, 127, midi)] * 0.18f);

            g.setColour(fill);
            g.fillRect(row);
            g.setColour(isC ? juce::Colour(0x33e6f5ff) : juce::Colour(0x1ffffff));
            g.drawHorizontalLine((int) row.getY(), row.getX(), row.getRight());
        }

        const double visibleStart = juce::jmax(0.0, timeOffset);
        const double visibleEnd = visibleStart + rollArea.getWidth() / pixelsPerSecond;
        const double grid = 0.25;
        for (double time = std::floor(visibleStart / grid) * grid; time <= visibleEnd; time += grid)
        {
            const auto x = timeToX(time);
            const bool major = std::abs(std::fmod(time, 1.0)) < 0.001;
            g.setColour(major ? juce::Colour(0x30ffffff) : juce::Colour(0x16ffffff));
            g.drawVerticalLine((int) x, rollArea.getY(), rollArea.getBottom());
        }
    }

    void drawKeyboard(juce::Graphics& g, juce::Rectangle<float> keyboardArea) const
    {
        g.setColour(juce::Colour(0xff0b1017));
        g.fillRoundedRectangle(keyboardArea, 10.0f);

        const int visibleTop = juce::roundToInt(topMidi + 2.0f);
        const int visibleBottom = juce::roundToInt(topMidi - keyboardArea.getHeight() / pixelsPerSemitone - 2.0f);
        const juce::StringArray noteNames{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        for (int midi = visibleTop; midi >= visibleBottom; --midi)
        {
            const int noteClass = ((midi % 12) + 12) % 12;
            const bool blackKey = noteClass == 1 || noteClass == 3 || noteClass == 6 || noteClass == 8 || noteClass == 10;
            auto row = juce::Rectangle<float>(keyboardArea.getX(), midiToY((float) midi), keyboardArea.getWidth(), pixelsPerSemitone);
            g.setColour(blackKey ? juce::Colour(0xff111821) : juce::Colour(0xffd5e0ea).withAlpha(0.07f));
            g.fillRect(row);
            g.setColour(juce::Colour(0x25ffffff));
            g.drawHorizontalLine((int) row.getY(), row.getX(), row.getRight());
            if (noteClass == 0)
            {
                const int octave = (midi / 12) - 1;
                g.setColour(strongText());
                g.setFont(juce::Font(11.0f, juce::Font::bold));
                g.drawText(noteNames[noteClass] + juce::String(octave), row.toNearestInt().reduced(8, 0), juce::Justification::centredLeft, false);
            }
        }
    }

    void drawBackgroundOverlays(juce::Graphics& g, juce::Rectangle<float> rollArea) const
    {
        if (processor.apvts.getRawParameterValue("showSpectrogram")->load() >= 0.5f)
        {
            for (const auto& frame : session.spectrogram)
            {
                const float x = timeToX(frame.time);
                const float columnWidth = juce::jmax(2.0f, pixelsPerSecond * 0.035f);
                if (x > rollArea.getRight() || x + columnWidth < rollArea.getX())
                    continue;

                for (size_t bin = 0; bin < frame.bins.size(); ++bin)
                {
                    const float alpha = frame.bins[bin] * 0.35f;
                    if (alpha <= 0.01f)
                        continue;

                    const float y = juce::jmap((float) bin, 0.0f, (float) frame.bins.size() - 1.0f, rollArea.getBottom(), rollArea.getY());
                    g.setColour(accentCyan().withAlpha(alpha));
                    g.fillRect(x, y, columnWidth, rollArea.getHeight() / (float) frame.bins.size() + 1.0f);
                }
            }
        }

        if (processor.apvts.getRawParameterValue("showWaveform")->load() >= 0.5f)
        {
            juce::Path waveform;
            bool started = false;
            for (const auto& point : session.waveform)
            {
                const float x = timeToX(point.time);
                const float yTop = juce::jmap(point.maxValue, -1.0f, 1.0f, rollArea.getBottom(), rollArea.getY());
                const float yBottom = juce::jmap(point.minValue, -1.0f, 1.0f, rollArea.getBottom(), rollArea.getY());
                if (! started)
                {
                    waveform.startNewSubPath(x, yTop);
                    started = true;
                }
                waveform.lineTo(x, yTop);
                waveform.lineTo(x, yBottom);
            }
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.strokePath(waveform, juce::PathStrokeType(1.0f));
        }
    }

    void drawNotes(juce::Graphics& g, juce::Rectangle<float> rollArea) const
    {
        for (const auto& note : session.notes)
        {
            auto bounds = getBlobBounds(note);
            if (! bounds.intersects(rollArea))
                continue;

            const float centsOff = std::abs((note.correctedPitch - std::round(note.correctedPitch)) * 100.0f);
            const bool inScale = isMidiNoteInScale(juce::roundToInt(note.correctedPitch),
                                                   (int) processor.apvts.getRawParameterValue("scaleKey")->load(),
                                                   getScaleModeNames()[juce::jlimit(0, getScaleModeNames().size() - 1,
                                                   (int) processor.apvts.getRawParameterValue("scaleMode")->load())]);
            auto fill = note.muted ? noteGray() : accentTeal();
            if (! inScale || centsOff > 30.0f)
                fill = accentCoral();
            else if (centsOff > 10.0f)
                fill = accentAmber();

            fill = fill.withMultipliedAlpha(juce::jlimit(0.38f, 1.0f, note.amplitude * 1.3f));
            g.setColour(fill);
            g.fillRoundedRectangle(bounds, 10.0f);
            g.setColour(selectedNoteIds.contains(note.id) ? strongText() : fill.brighter(0.3f));
            g.drawRoundedRectangle(bounds, 10.0f, selectedNoteIds.contains(note.id) ? 2.0f : 1.0f);

            juce::Path originalPath;
            juce::Path correctedPath;
            bool haveOriginal = false;
            bool haveCorrected = false;
            for (size_t i = 0; i < note.pitchTrace.size(); ++i)
            {
                const auto& point = note.pitchTrace[i];
                const float x = timeToX(point.time + dragTimeAdjustmentForNote(note.id));
                const float y = midiToY(point.pitch + pitchAdjustmentForNote(note.id));
                if (! haveOriginal)
                {
                    originalPath.startNewSubPath(x, y);
                    haveOriginal = true;
                }
                else
                {
                    originalPath.lineTo(x, y);
                }
            }

            for (const auto& point : note.correctedPitchTrace)
            {
                const float x = timeToX(point.time + dragTimeAdjustmentForNote(note.id));
                const float y = midiToY(point.pitch + pitchAdjustmentForNote(note.id));
                if (! haveCorrected)
                {
                    correctedPath.startNewSubPath(x, y);
                    haveCorrected = true;
                }
                else
                {
                    correctedPath.lineTo(x, y);
                }
            }

            if (processor.apvts.getRawParameterValue("showGhostTrace")->load() >= 0.5f)
            {
                g.setColour(juce::Colours::white.withAlpha(0.18f));
                g.strokePath(originalPath, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.setColour(strongText().withAlpha(0.9f));
            g.strokePath(correctedPath, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (drawing)
        {
            auto preview = juce::Rectangle<float>(timeToX(juce::jmin(drawStartTime, drawEndTime)),
                                                  midiToY(std::round(drawPitch)) - pixelsPerSemitone * 0.42f,
                                                  juce::jmax(8.0f, (float) std::abs(drawEndTime - drawStartTime) * pixelsPerSecond),
                                                  juce::jmax(14.0f, pixelsPerSemitone * 0.84f));
            g.setColour(accentCyan().withAlpha(0.45f));
            g.fillRoundedRectangle(preview, 10.0f);
            g.setColour(strongText());
            g.drawRoundedRectangle(preview, 10.0f, 1.4f);
        }
    }

    void drawPlaybackCursor(juce::Graphics& g, juce::Rectangle<float> rollArea) const
    {
        const float x = timeToX(processor.getPlayheadSeconds());
        if (x < rollArea.getX() || x > rollArea.getRight())
            return;

        g.setColour(accentAmber().withAlpha(0.85f));
        g.drawVerticalLine((int) x, rollArea.getY(), rollArea.getBottom());
    }

    void drawDeviationMeter(juce::Graphics& g, juce::Rectangle<float> meterArea) const
    {
        g.setColour(juce::Colour(0xff0d141c));
        g.fillRoundedRectangle(meterArea, 10.0f);
        g.setColour(juce::Colour(0x20ffffff));
        g.drawRoundedRectangle(meterArea, 10.0f, 1.0f);

        float cents = 0.0f;
        for (const auto& note : session.notes)
        {
            if (processor.getPlayheadSeconds() >= note.startTime && processor.getPlayheadSeconds() <= note.endTime)
            {
                cents = (note.originalPitch - note.correctedPitch) * 100.0f;
                break;
            }
        }

        const float centreY = meterArea.getCentreY();
        const float needleY = juce::jmap(juce::jlimit(-50.0f, 50.0f, cents), -50.0f, 50.0f, meterArea.getBottom() - 14.0f, meterArea.getY() + 14.0f);
        g.setColour(accentCyan().withAlpha(0.2f));
        g.fillRect(meterArea.reduced(10.0f, 12.0f));
        g.setColour(strongText());
        g.drawLine(meterArea.getX() + 6.0f, centreY, meterArea.getRight() - 6.0f, centreY, 1.0f);
        g.setColour(std::abs(cents) > 30.0f ? accentCoral() : accentTeal());
        g.fillRoundedRectangle(meterArea.getX() + 5.0f, needleY - 2.0f, meterArea.getWidth() - 10.0f, 4.0f, 2.0f);
    }

    void drawAnalysisOverlay(juce::Graphics& g, juce::Rectangle<float> bounds) const
    {
        auto overlay = bounds.reduced(bounds.getWidth() * 0.24f, bounds.getHeight() * 0.38f);
        g.setColour(juce::Colours::black.withAlpha(0.72f));
        g.fillRoundedRectangle(overlay, 14.0f);
        g.setColour(accentCyan().withAlpha(0.85f));
        g.drawRoundedRectangle(overlay, 14.0f, 1.2f);

        g.setColour(strongText());
        g.setFont(juce::Font(18.0f, juce::Font::bold));
        g.drawText(session.progressLabel, overlay.removeFromTop(34.0f).toNearestInt(), juce::Justification::centred, false);

        auto bar = overlay.removeFromTop(20.0f).reduced(28.0f, 0.0f);
        g.setColour(juce::Colour(0xff132330));
        g.fillRoundedRectangle(bar, 7.0f);
        g.setColour(accentCyan().withAlpha(0.85f));
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * session.progress), 7.0f);

        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(juce::String((int) std::round(session.progress * 100.0f)) + "%", overlay.removeFromTop(28.0f).toNearestInt(), juce::Justification::centred, false);
    }

    PluginProcessor& processor;
    AnalysisSession session;
    Tool tool = Tool::pointer;
    juce::StringArray selectedNoteIds;
    juce::String draggingNoteId;
    float pixelsPerSecond = 140.0f;
    float pixelsPerSemitone = 18.0f;
    float topMidi = 84.0f;
    double timeOffset = 0.0;
    juce::Point<float> dragOrigin;
    float dragPitchDelta = 0.0f;
    double dragTimeDelta = 0.0;
    double durationStartDelta = 0.0;
    double durationEndDelta = 0.0;
    bool resizeAffectsStart = false;
    bool dragging = false;
    bool panning = false;
    bool drawing = false;
    bool resizing = false;
    double drawStartTime = 0.0;
    double drawEndTime = 0.0;
    float drawPitch = 60.0f;
};

} // namespace

class PitchRootComponent final : public juce::Component
{
public:
    explicit PitchRootComponent(PluginProcessor& processorToUse)
        : processor(processorToUse),
          roll(processorToUse),
          correctPitch(processor.apvts, "correctPitch", "Correct Pitch"),
          correctDrift(processor.apvts, "correctDrift", "Correct Drift"),
          correctTiming(processor.apvts, "correctTiming", "Correct Timing"),
          vibratoDepth(processor.apvts, "vibratoDepth", "Vibrato Depth"),
          vibratoSpeed(processor.apvts, "vibratoSpeed", "Vibrato Speed"),
          formantShift(processor.apvts, "formantShift", "Formant Shift"),
          formantAttachment(processor.apvts, "formantPreserve", formantPreserveToggle),
          heatmapAttachment(processor.apvts, "showHeatmap", heatmapToggle),
          waveformAttachment(processor.apvts, "showWaveform", waveformToggle),
          spectrogramAttachment(processor.apvts, "showSpectrogram", spectrogramToggle),
          ghostTraceAttachment(processor.apvts, "showGhostTrace", ghostTraceToggle),
          statsAttachment(processor.apvts, "showStats", statsToggle),
          snapAttachment(processor.apvts, "snapMode", snapBox),
          keyAttachment(processor.apvts, "scaleKey", keyBox),
          modeAttachment(processor.apvts, "scaleMode", modeBox)
    {
        addAndMakeVisible(titleLabel);
        titleLabel.setText("SSP Pitch", juce::dontSendNotification);
        titleLabel.setColour(juce::Label::textColourId, strongText());
        titleLabel.setFont(juce::Font(28.0f, juce::Font::bold));

        addAndMakeVisible(subtitleLabel);
        subtitleLabel.setText("Monophonic pitch editing, note blobs, and correction tracing in SSP's vector language.", juce::dontSendNotification);
        subtitleLabel.setColour(juce::Label::textColourId, softText());
        subtitleLabel.setFont(juce::Font(12.0f));

        for (auto* button : { &playButton, &stopButton, &loopButton, &captureButton, &undoButton, &redoButton,
                              &compareAButton, &compareCopyButton, &compareBButton, &zoomTimeMinusButton, &zoomTimePlusButton,
                              &zoomPitchMinusButton, &zoomPitchPlusButton, &settingsButton,
                              &pointerToolButton, &pitchToolButton, &timeToolButton, &durationToolButton, &splitToolButton,
                              &mergeToolButton, &drawToolButton, &pencilToolButton, &muteNoteButton })
            addAndMakeVisible(*button);

        for (auto* box : { &presetBox, &snapBox, &keyBox, &modeBox })
            addAndMakeVisible(*box);

        for (auto* toggle : { &formantPreserveToggle, &heatmapToggle, &waveformToggle, &spectrogramToggle, &ghostTraceToggle, &statsToggle })
            addAndMakeVisible(*toggle);

        addAndMakeVisible(correctPitch);
        addAndMakeVisible(correctDrift);
        addAndMakeVisible(correctTiming);
        addAndMakeVisible(vibratoDepth);
        addAndMakeVisible(vibratoSpeed);
        addAndMakeVisible(formantShift);

        addAndMakeVisible(roll);

        correctPitch.setFormatting(formatPercent);
        correctDrift.setFormatting(formatPercent);
        correctTiming.setFormatting(formatPercent);
        vibratoDepth.setFormatting(formatPercent);
        vibratoSpeed.setFormatting(formatPercent);
        formantShift.setFormatting(formatFormant);

        for (const auto& presetName : PluginProcessor::getFactoryPresetNames())
            presetBox.addItem(presetName, presetBox.getNumItems() + 1);

        for (int i = 0; i < getSnapModeNames().size(); ++i)
            snapBox.addItem(getSnapModeNames()[i], i + 1);
        for (int i = 0; i < getScaleKeyNames().size(); ++i)
            keyBox.addItem(getScaleKeyNames()[i], i + 1);
        for (int i = 0; i < getScaleModeNames().size(); ++i)
            modeBox.addItem(getScaleModeNames()[i], i + 1);

        presetBox.onChange = [this]
        {
            if (presetBox.getSelectedId() > 0)
                processor.loadFactoryCorrectionPreset(presetBox.getText());
        };

        playButton.onClick = [this] { processor.setTransportPlaying(! processor.isTransportPlaying()); };
        stopButton.onClick = [this] { processor.stopTransport(); };
        loopButton.onClick = [this] { processor.setLoopEnabled(! processor.isLoopEnabled()); };
        captureButton.onClick = [this]
        {
            if (processor.isCapturing())
                processor.stopCaptureAndAnalyze();
            else
                processor.startCapture();
        };
        undoButton.onClick = [this] { processor.undoLastEdit(); };
        redoButton.onClick = [this] { processor.redoLastEdit(); };
        compareAButton.onClick = [this] { processor.setActiveCompareSlot(0); };
        compareCopyButton.onClick = [this] { processor.copyActiveCompareSlotToOther(); };
        compareBButton.onClick = [this] { processor.setActiveCompareSlot(1); };
        zoomTimeMinusButton.onClick = [this] { roll.zoomTime(0.9f); };
        zoomTimePlusButton.onClick = [this] { roll.zoomTime(1.12f); };
        zoomPitchMinusButton.onClick = [this] { roll.zoomPitch(0.9f); };
        zoomPitchPlusButton.onClick = [this] { roll.zoomPitch(1.12f); };
        settingsButton.onClick = []
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                   "SSP Pitch Settings",
                                                   "Pitch analysis runs on a background thread and progressively updates the piano roll.\n\n"
                                                   "This first build keeps all transport, capture, scale, and per-note editing controls inside the SSP Pitch shell.");
        };

        auto setTool = [this](PitchRollComponent::Tool newTool)
        {
            roll.setTool(newTool);
            syncToolButtons();
        };

        pointerToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::pointer); };
        pitchToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::pitch); };
        timeToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::time); };
        durationToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::duration); };
        splitToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::split); };
        mergeToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::merge); };
        drawToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::draw); };
        pencilToolButton.onClick = [setTool] { setTool(PitchRollComponent::Tool::pencil); };

        heatmapToggle.setButtonText("Heatmap");
        waveformToggle.setButtonText("Waveform");
        spectrogramToggle.setButtonText("Spectrogram");
        ghostTraceToggle.setButtonText("Before/After");
        statsToggle.setButtonText("Stats");
        formantPreserveToggle.setButtonText("Formant Preserve");

        originalPitchLabel.setColour(juce::Label::textColourId, strongText());
        correctedPitchLabel.setColour(juce::Label::textColourId, strongText());
        selectedNoteLabel.setColour(juce::Label::textColourId, softText());
        for (auto* label : { &selectedNoteLabel, &originalPitchLabel, &correctedPitchLabel })
        {
            addAndMakeVisible(*label);
            label->setFont(juce::Font(13.0f, juce::Font::bold));
        }

        configureNoteSlider(notePitchOverrideSlider, "Per-note Pitch", 0.0, 100.0);
        configureNoteSlider(noteDriftOverrideSlider, "Per-note Drift", 0.0, 100.0);
        configureNoteSlider(noteFormantSlider, "Per-note Formant", -12.0, 12.0);
        configureNoteSlider(noteGainSlider, "Note Gain", -24.0, 12.0);
        configureNoteSlider(noteTransitionSlider, "Transition", 0.0, 100.0);

        notePitchOverrideSlider.onValueChange = [this] { applySelectedNoteControlChanges(); };
        noteDriftOverrideSlider.onValueChange = [this] { applySelectedNoteControlChanges(); };
        noteFormantSlider.onValueChange = [this] { applySelectedNoteControlChanges(); };
        noteGainSlider.onValueChange = [this] { applySelectedNoteControlChanges(); };
        noteTransitionSlider.onValueChange = [this] { applySelectedNoteControlChanges(); };
        muteNoteButton.onClick = [this]
        {
            const auto selected = roll.getSelectedNoteIds();
            if (selected.isEmpty())
                return;
            processor.muteNotes(selected, muteNoteButton.getToggleState());
        };

        syncToolButtons();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(18, 16);
        area.removeFromTop(88);
        auto bottomPanel = area.removeFromBottom(212);
        area.removeFromBottom(10);

        ssp::ui::drawPanelBackground(g, bottomPanel.removeFromLeft(360).toFloat(), accentTeal(), 14.0f);
        ssp::ui::drawPanelBackground(g, bottomPanel.removeFromLeft(640).toFloat(), accentCyan(), 14.0f);
        ssp::ui::drawPanelBackground(g, bottomPanel.toFloat(), accentAmber(), 14.0f);
    }

    void syncFromSession(const AnalysisSession& session)
    {
        roll.setSession(session);
        updateNoteInspector(session);

        captureButton.setButtonText(processor.isCapturing() ? "Stop / Analyze" : "Capture / Analyze");
        loopButton.setToggleState(processor.isLoopEnabled(), juce::dontSendNotification);
        playButton.setToggleState(processor.isTransportPlaying(), juce::dontSendNotification);
        compareAButton.setToggleState(processor.getActiveCompareSlot() == 0, juce::dontSendNotification);
        compareBButton.setToggleState(processor.getActiveCompareSlot() == 1, juce::dontSendNotification);
        undoButton.setEnabled(processor.canUndo());
        redoButton.setEnabled(processor.canRedo());
    }

    PitchRollComponent& getRoll() noexcept { return roll; }

private:
    void configureNoteSlider(juce::Slider& slider, const juce::String& labelText, double min, double max)
    {
        addAndMakeVisible(slider);
        slider.setLookAndFeel(&ssp::ui::getVectorLookAndFeel());
        slider.setRange(min, max, 0.1);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 18);
        slider.setColour(juce::Slider::trackColourId, accentCyan());
        slider.setName(labelText);
    }

    void applySelectedNoteControlChanges()
    {
        if (updatingNoteInspector)
            return;

        const auto selected = roll.getSelectedNoteIds();
        if (selected.size() != 1)
            return;

        const auto& id = selected[0];
        processor.setNotePitchCorrection(id, (float) notePitchOverrideSlider.getValue());
        processor.setNoteDriftCorrection(id, (float) noteDriftOverrideSlider.getValue());
        processor.setNoteFormantShift(id, (float) noteFormantSlider.getValue());
        processor.setNoteGainDb(id, (float) noteGainSlider.getValue());
        processor.setNoteTransition(id, (float) noteTransitionSlider.getValue());
    }

    void updateNoteInspector(const AnalysisSession& session)
    {
        const auto selected = roll.getSelectedNoteIds();
        if (selected.size() != 1)
        {
            selectedNoteLabel.setText("No note selected", juce::dontSendNotification);
            originalPitchLabel.setText("Original: --", juce::dontSendNotification);
            correctedPitchLabel.setText("Corrected: --", juce::dontSendNotification);
            return;
        }

        auto noteIt = std::find_if(session.notes.begin(), session.notes.end(),
                                   [&](const auto& note) { return note.id == selected[0]; });
        if (noteIt == session.notes.end())
            return;

        updatingNoteInspector = true;
        selectedNoteLabel.setText("Selected note", juce::dontSendNotification);
        originalPitchLabel.setText("Original: " + formatPitchReadout(noteIt->originalPitch), juce::dontSendNotification);
        correctedPitchLabel.setText("Corrected: " + formatPitchReadout(noteIt->correctedPitch), juce::dontSendNotification);
        notePitchOverrideSlider.setValue(noteIt->pitchCorrection >= 0.0f ? noteIt->pitchCorrection : processor.apvts.getRawParameterValue("correctPitch")->load(),
                                         juce::dontSendNotification);
        noteDriftOverrideSlider.setValue(noteIt->driftCorrection >= 0.0f ? noteIt->driftCorrection : processor.apvts.getRawParameterValue("correctDrift")->load(),
                                         juce::dontSendNotification);
        noteFormantSlider.setValue(noteIt->formantShift, juce::dontSendNotification);
        noteGainSlider.setValue(noteIt->gainDb, juce::dontSendNotification);
        noteTransitionSlider.setValue(noteIt->transitionToNext, juce::dontSendNotification);
        muteNoteButton.setToggleState(noteIt->muted, juce::dontSendNotification);
        updatingNoteInspector = false;
    }

    void syncToolButtons()
    {
        pointerToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::pointer, juce::dontSendNotification);
        pitchToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::pitch, juce::dontSendNotification);
        timeToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::time, juce::dontSendNotification);
        durationToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::duration, juce::dontSendNotification);
        splitToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::split, juce::dontSendNotification);
        mergeToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::merge, juce::dontSendNotification);
        drawToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::draw, juce::dontSendNotification);
        pencilToolButton.setToggleState(roll.getTool() == PitchRollComponent::Tool::pencil, juce::dontSendNotification);
    }

public:
    void resized() override
    {
        auto area = getLocalBounds().reduced(18, 16);
        auto header = area.removeFromTop(88);
        auto topLine = header.removeFromTop(34);
        titleLabel.setBounds(topLine.removeFromLeft(170));
        playButton.setBounds(topLine.removeFromLeft(40));
        topLine.removeFromLeft(6);
        stopButton.setBounds(topLine.removeFromLeft(40));
        topLine.removeFromLeft(6);
        loopButton.setBounds(topLine.removeFromLeft(56));
        topLine.removeFromLeft(10);
        captureButton.setBounds(topLine.removeFromLeft(136));
        topLine.removeFromLeft(10);
        undoButton.setBounds(topLine.removeFromLeft(52));
        topLine.removeFromLeft(6);
        redoButton.setBounds(topLine.removeFromLeft(52));
        topLine.removeFromLeft(10);
        compareAButton.setBounds(topLine.removeFromLeft(32));
        topLine.removeFromLeft(4);
        compareCopyButton.setBounds(topLine.removeFromLeft(42));
        topLine.removeFromLeft(4);
        compareBButton.setBounds(topLine.removeFromLeft(32));
        topLine.removeFromLeft(10);
        presetBox.setBounds(topLine.removeFromLeft(174));
        topLine.removeFromLeft(10);
        settingsButton.setBounds(topLine.removeFromLeft(48));
        topLine.removeFromLeft(8);
        zoomTimeMinusButton.setBounds(topLine.removeFromLeft(28));
        zoomTimePlusButton.setBounds(topLine.removeFromLeft(28));
        topLine.removeFromLeft(4);
        zoomPitchMinusButton.setBounds(topLine.removeFromLeft(28));
        zoomPitchPlusButton.setBounds(topLine.removeFromLeft(28));

        auto secondLine = header.removeFromTop(30);
        subtitleLabel.setBounds(secondLine.removeFromLeft(450));
        snapBox.setBounds(secondLine.removeFromLeft(120));
        secondLine.removeFromLeft(8);
        keyBox.setBounds(secondLine.removeFromLeft(74));
        secondLine.removeFromLeft(8);
        modeBox.setBounds(secondLine.removeFromLeft(148));
        secondLine.removeFromLeft(12);
        heatmapToggle.setBounds(secondLine.removeFromLeft(108));
        waveformToggle.setBounds(secondLine.removeFromLeft(108));
        spectrogramToggle.setBounds(secondLine.removeFromLeft(126));
        ghostTraceToggle.setBounds(secondLine.removeFromLeft(126));
        statsToggle.setBounds(secondLine.removeFromLeft(82));

        auto bottomPanel = area.removeFromBottom(212);
        area.removeFromBottom(10);
        roll.setBounds(area);

        auto leftTools = bottomPanel.removeFromLeft(360);
        leftTools.reduce(12, 14);
        leftTools.removeFromTop(8);
        auto toolRow1 = leftTools.removeFromTop(34);
        pointerToolButton.setBounds(toolRow1.removeFromLeft(86));
        toolRow1.removeFromLeft(6);
        pitchToolButton.setBounds(toolRow1.removeFromLeft(86));
        toolRow1.removeFromLeft(6);
        timeToolButton.setBounds(toolRow1.removeFromLeft(86));
        toolRow1.removeFromLeft(6);
        durationToolButton.setBounds(toolRow1.removeFromLeft(86));

        leftTools.removeFromTop(8);
        auto toolRow2 = leftTools.removeFromTop(34);
        splitToolButton.setBounds(toolRow2.removeFromLeft(86));
        toolRow2.removeFromLeft(6);
        mergeToolButton.setBounds(toolRow2.removeFromLeft(86));
        toolRow2.removeFromLeft(6);
        drawToolButton.setBounds(toolRow2.removeFromLeft(86));
        toolRow2.removeFromLeft(6);
        pencilToolButton.setBounds(toolRow2.removeFromLeft(86));

        leftTools.removeFromTop(16);
        formantPreserveToggle.setBounds(leftTools.removeFromTop(28));

        auto centreKnobs = bottomPanel.removeFromLeft(640);
        auto knobRow = centreKnobs.reduced(10, 14);
        auto knobWidth = knobRow.getWidth() / 6;
        correctPitch.setBounds(knobRow.removeFromLeft(knobWidth));
        correctDrift.setBounds(knobRow.removeFromLeft(knobWidth));
        correctTiming.setBounds(knobRow.removeFromLeft(knobWidth));
        vibratoDepth.setBounds(knobRow.removeFromLeft(knobWidth));
        vibratoSpeed.setBounds(knobRow.removeFromLeft(knobWidth));
        formantShift.setBounds(knobRow.removeFromLeft(knobWidth));

        auto notePanel = bottomPanel.reduced(10, 8);
        selectedNoteLabel.setBounds(notePanel.removeFromTop(20));
        originalPitchLabel.setBounds(notePanel.removeFromTop(20));
        correctedPitchLabel.setBounds(notePanel.removeFromTop(20));
        notePanel.removeFromTop(6);
        notePitchOverrideSlider.setBounds(notePanel.removeFromTop(28));
        noteDriftOverrideSlider.setBounds(notePanel.removeFromTop(28));
        noteFormantSlider.setBounds(notePanel.removeFromTop(28));
        noteGainSlider.setBounds(notePanel.removeFromTop(28));
        noteTransitionSlider.setBounds(notePanel.removeFromTop(28));
        notePanel.removeFromTop(6);
        muteNoteButton.setBounds(notePanel.removeFromTop(28).removeFromLeft(120));
    }

private:
    PluginProcessor& processor;
    PitchRollComponent roll;
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    ssp::ui::SSPButton playButton{"Play"};
    ssp::ui::SSPButton stopButton{"Stop"};
    ssp::ui::SSPButton loopButton{"Loop"};
    ssp::ui::SSPButton captureButton{"Capture / Analyze"};
    ssp::ui::SSPButton undoButton{"Undo"};
    ssp::ui::SSPButton redoButton{"Redo"};
    ssp::ui::SSPButton compareAButton{"A"};
    ssp::ui::SSPButton compareCopyButton{"<>"}; 
    ssp::ui::SSPButton compareBButton{"B"};
    ssp::ui::SSPButton zoomTimeMinusButton{"-"};
    ssp::ui::SSPButton zoomTimePlusButton{"+"};
    ssp::ui::SSPButton zoomPitchMinusButton{"v-"};
    ssp::ui::SSPButton zoomPitchPlusButton{"v+"};
    ssp::ui::SSPButton settingsButton{"Gear"};

    ssp::ui::SSPButton pointerToolButton{"Pointer"};
    ssp::ui::SSPButton pitchToolButton{"Pitch"};
    ssp::ui::SSPButton timeToolButton{"Time"};
    ssp::ui::SSPButton durationToolButton{"Duration"};
    ssp::ui::SSPButton splitToolButton{"Split"};
    ssp::ui::SSPButton mergeToolButton{"Merge"};
    ssp::ui::SSPButton drawToolButton{"Draw"};
    ssp::ui::SSPButton pencilToolButton{"Pencil"};

    ssp::ui::SSPDropdown presetBox;
    ssp::ui::SSPDropdown snapBox;
    ssp::ui::SSPDropdown keyBox;
    ssp::ui::SSPDropdown modeBox;

    ssp::ui::SSPToggle formantPreserveToggle{"Formant Preserve"};
    ssp::ui::SSPToggle heatmapToggle{"Heatmap"};
    ssp::ui::SSPToggle waveformToggle{"Waveform"};
    ssp::ui::SSPToggle spectrogramToggle{"Spectrogram"};
    ssp::ui::SSPToggle ghostTraceToggle{"Before/After"};
    ssp::ui::SSPToggle statsToggle{"Stats"};

    KnobField correctPitch;
    KnobField correctDrift;
    KnobField correctTiming;
    KnobField vibratoDepth;
    KnobField vibratoSpeed;
    KnobField formantShift;

    ButtonAttachment formantAttachment;
    ButtonAttachment heatmapAttachment;
    ButtonAttachment waveformAttachment;
    ButtonAttachment spectrogramAttachment;
    ButtonAttachment ghostTraceAttachment;
    ButtonAttachment statsAttachment;
    ComboBoxAttachment snapAttachment;
    ComboBoxAttachment keyAttachment;
    ComboBoxAttachment modeAttachment;

    juce::Label selectedNoteLabel;
    juce::Label originalPitchLabel;
    juce::Label correctedPitchLabel;
    juce::Slider notePitchOverrideSlider;
    juce::Slider noteDriftOverrideSlider;
    juce::Slider noteFormantSlider;
    juce::Slider noteGainSlider;
    juce::Slider noteTransitionSlider;
    ssp::ui::SSPButton muteNoteButton{"Mute Note"};
    bool updatingNoteInspector = false;
};

PluginEditor::PluginEditor(PluginProcessor& processorToUse)
    : juce::AudioProcessorEditor(&processorToUse),
      pluginProcessor(processorToUse),
      root(std::make_unique<PitchRootComponent>(processorToUse))
{
    setSize(editorWidth, editorHeight);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(*root);
    startTimerHz(30);
    lastTimerSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

PluginEditor::~PluginEditor() = default;

void PluginEditor::paint(juce::Graphics& g)
{
    ssp::ui::drawEditorBackdrop(g, getLocalBounds().toFloat().reduced(6.0f));
}

void PluginEditor::resized()
{
    root->setBounds(getLocalBounds().reduced(10));
}

bool PluginEditor::keyPressed(const juce::KeyPress& key)
{
    auto& roll = root->getRoll();
    const auto selected = roll.getSelectedNoteIds();

    if (key == juce::KeyPress::spaceKey)
    {
        pluginProcessor.setTransportPlaying(! pluginProcessor.isTransportPlaying());
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        roll.clearSelection();
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        pluginProcessor.muteNotes(selected, true);
        return true;
    }

    if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) && (key.getTextCharacter() == 'z' || key.getTextCharacter() == 'Z'))
        return pluginProcessor.undoLastEdit();

    if ((key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown()) && (key.getTextCharacter() == 'y' || key.getTextCharacter() == 'Y'))
        return pluginProcessor.redoLastEdit();

    if (key == juce::KeyPress::upKey)
    {
        for (const auto& id : selected)
            pluginProcessor.moveNote(id, key.getModifiers().isShiftDown() ? 0.01f : (key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown() ? 12.0f : 1.0f), 0.0);
        return true;
    }

    if (key == juce::KeyPress::downKey)
    {
        for (const auto& id : selected)
            pluginProcessor.moveNote(id, key.getModifiers().isShiftDown() ? -0.01f : (key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown() ? -12.0f : -1.0f), 0.0);
        return true;
    }

    if (key == juce::KeyPress::leftKey)
    {
        for (const auto& id : selected)
            pluginProcessor.moveNote(id, 0.0f, -0.01);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        for (const auto& id : selected)
            pluginProcessor.moveNote(id, 0.0f, 0.01);
        return true;
    }

    switch (key.getTextCharacter())
    {
        case 's':
        case 'S': root->getRoll().setTool(PitchRollComponent::Tool::split); return true;
        case 'm':
        case 'M': pluginProcessor.mergeNotes(selected); return true;
        case 'p':
        case 'P': root->getRoll().setTool(PitchRollComponent::Tool::pencil); return true;
        case '1': root->getRoll().setTool(PitchRollComponent::Tool::pointer); return true;
        case '2': root->getRoll().setTool(PitchRollComponent::Tool::pitch); return true;
        case '3': root->getRoll().setTool(PitchRollComponent::Tool::time); return true;
        case '4': root->getRoll().setTool(PitchRollComponent::Tool::duration); return true;
        case '5': root->getRoll().setTool(PitchRollComponent::Tool::split); return true;
        case '6': root->getRoll().setTool(PitchRollComponent::Tool::merge); return true;
        case '7': root->getRoll().setTool(PitchRollComponent::Tool::draw); return true;
        default: break;
    }

    return juce::AudioProcessorEditor::keyPressed(key);
}

bool PluginEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return ! files.isEmpty();
}

void PluginEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (! files.isEmpty())
        pluginProcessor.analyzeAudioFile(juce::File(files[0]));
}

void PluginEditor::timerCallback()
{
    const double nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    pluginProcessor.advanceTransport(nowSeconds - lastTimerSeconds);
    lastTimerSeconds = nowSeconds;

    const auto session = pluginProcessor.getSessionCopy();
    if (session.revision != lastRevision || pluginProcessor.isTransportPlaying())
    {
        root->syncFromSession(session);
        lastRevision = session.revision;
    }
}
