#include "PluginEditor.h"

#include "MidiBuddyVectorUI.h"

#include <cmath>

namespace
{
constexpr int editorWidth = 1280;
constexpr int editorHeight = 820;
constexpr int chordEditorRows = 24;
constexpr int chordEditorDefaultVisibleRows = 16;
constexpr int chordEditorMinVisibleRows = 10;

void styleLabel(juce::Label& label,
                const juce::String& text,
                juce::Font font,
                juce::Colour colour,
                juce::Justification justification)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(font);
    label.setColour(juce::Label::textColourId, colour);
    label.setJustificationType(justification);
    label.setInterceptsMouseClicks(false, false);
}

void setupCaption(juce::Label& label, const juce::String& text)
{
    styleLabel(label, text, midibuddyui::smallCapsFont(10.5f), midibuddyui::textMuted(), juce::Justification::centredLeft);
}

void setupCombo(juce::ComboBox& box, const juce::StringArray& items)
{
    for (int index = 0; index < items.size(); ++index)
        box.addItem(items[index], index + 1);

    box.setColour(juce::ComboBox::backgroundColourId, midibuddyui::backgroundSoft());
    box.setColour(juce::ComboBox::outlineColourId, midibuddyui::outline());
    box.setColour(juce::ComboBox::textColourId, midibuddyui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, midibuddyui::textStrong());
}

void setupKnob(juce::Slider& slider, juce::Colour accent, const std::function<juce::String(double)>& formatter)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::thumbColourId, midibuddyui::textStrong());
    slider.textFromValueFunction = formatter;
}

void setupButton(juce::TextButton& button, const juce::String& text, juce::Colour colour, juce::Colour textColour)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, colour);
    button.setColour(juce::TextButton::textColourOffId, textColour);
}

void setComboItems(juce::ComboBox& box, const juce::StringArray& items)
{
    box.clear();
    for (int index = 0; index < items.size(); ++index)
        box.addItem(items[index], index + 1);
}

class ProgressionCardButton final : public juce::Button
{
public:
    std::function<void()> onPrimaryClick;
    std::function<void()> onContextClick;

    ProgressionCardButton()
        : juce::Button("ProgressionCard")
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setDisplay(PluginProcessor::SlotDisplayData newDisplay, bool shouldShow, bool isSelectedNow)
    {
        display = std::move(newDisplay);
        active = shouldShow;
        selected = isSelectedNow;
        setVisible(active);
        repaint();
    }

    void paintButton(juce::Graphics& g, bool isHovered, bool isPressed) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const auto border = selected ? midibuddyui::brandAmber()
                                     : isHovered ? midibuddyui::brandTeal()
                                                 : midibuddyui::outlineSoft();
        auto fillTop = juce::Colour(0xff18222c);
        auto fillBottom = juce::Colour(0xff101822);

        if (selected)
        {
            fillTop = juce::Colour(0xff20251d);
            fillBottom = juce::Colour(0xff161810);
        }
        else if (isPressed)
        {
            fillTop = fillTop.brighter(0.08f);
            fillBottom = fillBottom.brighter(0.04f);
        }

        juce::ColourGradient fill(fillTop, bounds.getTopLeft(), fillBottom, bounds.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 8.0f);

        if (selected)
        {
            g.setColour(midibuddyui::brandAmber().withAlpha(0.12f));
            g.fillRoundedRectangle(bounds.reduced(2.0f), 6.0f);
        }

        g.setColour(juce::Colour(0xff080c10));
        g.drawRoundedRectangle(bounds, 8.0f, 1.2f);
        g.setColour(border);
        g.drawRoundedRectangle(bounds.reduced(1.6f), 6.0f, selected ? 1.4f : 1.0f);

        auto content = bounds.reduced(14.0f, 10.0f);
        g.setColour(selected ? midibuddyui::brandAmber() : midibuddyui::textMuted());
        g.setFont(midibuddyui::smallCapsFont(10.0f));
        g.drawText(display.heading, content.removeFromTop(16.0f).toNearestInt(), juce::Justification::centredLeft, false);

        g.setColour(midibuddyui::textStrong());
        g.setFont(midibuddyui::titleFont(16.0f));
        g.drawFittedText(display.primary, content.removeFromTop(34.0f).toNearestInt(), juce::Justification::centred, 2);

        g.setColour(selected ? midibuddyui::brandIce() : midibuddyui::textMuted());
        g.setFont(midibuddyui::bodyFont(10.0f));
        g.drawFittedText(display.secondary, content.toNearestInt(), juce::Justification::centredBottom, 2);
    }

    void clicked(const juce::ModifierKeys& modifiers) override
    {
        if (! modifiers.isPopupMenu() && onPrimaryClick)
            onPrimaryClick();
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
        {
            if (onContextClick)
                onContextClick();
            return;
        }

        juce::Button::mouseUp(event);
    }

private:
    PluginProcessor::SlotDisplayData display;
    bool active = false;
    bool selected = false;
};

class ChordPianoRoll final : public juce::Component
{
public:
    ChordPianoRoll(PluginProcessor& processorToUse,
                   std::function<int()> slotGetterToUse,
                   std::function<void()> onUpdateToUse)
        : processor(processorToUse),
          slotGetter(std::move(slotGetterToUse)),
          onUpdate(std::move(onUpdateToUse))
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        setWantsKeyboardFocus(true);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        juce::ColourGradient fill(juce::Colour(0xff141d26), bounds.getTopLeft(),
                                  juce::Colour(0xff0d141b), bounds.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 8.0f);
        g.setColour(midibuddyui::outlineSoft());
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

        const auto layout = getLayout();
        g.setColour(midibuddyui::brandAmber());
        g.setFont(midibuddyui::smallCapsFont(10.0f));
        g.drawText("Advanced Chord Editor", layout.titleArea.toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour(midibuddyui::textMuted());
        g.setFont(midibuddyui::bodyFont(10.0f));
        g.drawText("Click to add, drag to move, wheel to scroll, Cmd/Ctrl+wheel to zoom, right-click/Delete to remove.",
                   layout.titleArea.toNearestInt(),
                   juce::Justification::centredRight,
                   false);

        if (! processor.isChordModeActive())
        {
            g.setColour(midibuddyui::textMuted());
            g.setFont(midibuddyui::bodyFont(12.0f));
            g.drawFittedText("Switch back to Chords mode to edit voicings here. Click the note lanes to build custom stacks for the selected slot.",
                             layout.bodyArea.toNearestInt(),
                             juce::Justification::centred,
                             3);
            return;
        }

        const int selectedSlot = slotGetter();
        const int rootMidi = processor.getSlotRootMidi(selectedSlot);
        const auto intervals = processor.getSlotEditorIntervals(selectedSlot);
        const int visibleRows = getVisibleRowCount();
        const int highestVisibleSemitone = getHighestVisibleSemitone();
        const float rowHeight = layout.timelineArea.getHeight() / (float) visibleRows;

        g.setColour(juce::Colour(0xff101821));
        g.fillRect(layout.topLeftArea);
        g.setColour(midibuddyui::outlineSoft());
        g.drawRect(layout.topLeftArea.toNearestInt(), 1);
        g.setColour(midibuddyui::textMuted());
        g.setFont(midibuddyui::smallCapsFont(9.5f));
        g.drawText("Pitch", layout.topLeftArea.toNearestInt(), juce::Justification::centred, false);

        g.setColour(juce::Colour(0xff101821));
        g.fillRect(layout.rulerArea);
        g.setColour(midibuddyui::outlineSoft());
        g.drawRect(layout.rulerArea.toNearestInt(), 1);

        for (int beat = 0; beat < 4; ++beat)
        {
            const auto beatRect = juce::Rectangle<float>(layout.rulerArea.getX() + (layout.rulerArea.getWidth() / 4.0f) * (float) beat,
                                                         layout.rulerArea.getY(),
                                                         layout.rulerArea.getWidth() / 4.0f,
                                                         layout.rulerArea.getHeight());
            g.setColour(midibuddyui::textMuted());
            g.setFont(midibuddyui::smallCapsFont(9.5f));
            g.drawText(juce::String(beat + 1), beatRect.toNearestInt().reduced(6, 0), juce::Justification::centredLeft, false);
        }

        if (isSemitoneVisible(hoveredSemitone))
        {
            const auto hoverRect = getLaneRectForSemitone(hoveredSemitone);
            g.setColour(midibuddyui::brandIce().withAlpha(0.08f));
            g.fillRect(hoverRect);
        }

        for (int row = 0; row < visibleRows; ++row)
        {
            const int semitone = highestVisibleSemitone - row;
            const float y = layout.timelineArea.getY() + row * rowHeight;
            auto rowRect = juce::Rectangle<float>(layout.timelineArea.getX(), y, layout.timelineArea.getWidth(), rowHeight);
            const bool sharp = ((rootMidi + semitone) % 12 == 1)
                               || ((rootMidi + semitone) % 12 == 3)
                               || ((rootMidi + semitone) % 12 == 6)
                               || ((rootMidi + semitone) % 12 == 8)
                               || ((rootMidi + semitone) % 12 == 10);

            g.setColour(sharp ? juce::Colour(0xff111821) : juce::Colour(0xff17212c));
            g.fillRect(rowRect);
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.drawHorizontalLine(juce::roundToInt(y), layout.timelineArea.getX(), layout.timelineArea.getRight());

            auto keyRect = juce::Rectangle<float>(layout.keyboardArea.getX(), y, layout.keyboardArea.getWidth(), rowHeight);
            g.setColour(sharp ? juce::Colour(0xff0e141b) : juce::Colour(0xff1a2430));
            g.fillRect(keyRect);
            g.setColour(midibuddyui::outlineSoft());
            g.drawRect(keyRect.toNearestInt(), 1);

            if (hoveredSemitone == semitone)
            {
                g.setColour(midibuddyui::brandIce().withAlpha(0.10f));
                g.fillRect(keyRect);
            }

            g.setColour(sharp ? midibuddyui::brandIce() : midibuddyui::textMuted());
            g.setFont(midibuddyui::bodyFont(9.8f));
            g.drawText(juce::MidiMessage::getMidiNoteName(rootMidi + semitone, true, false, 4),
                       keyRect.toNearestInt().reduced(6, 0),
                       juce::Justification::centredLeft,
                       false);
        }

        for (int subdivision = 0; subdivision <= 16; ++subdivision)
        {
            const float x = layout.timelineArea.getX() + ((float) subdivision / 16.0f) * layout.timelineArea.getWidth();
            const bool barLine = subdivision % 4 == 0;
            g.setColour(barLine ? juce::Colours::white.withAlpha(0.14f)
                                : juce::Colours::white.withAlpha(0.05f));
            g.drawVerticalLine(juce::roundToInt(x), layout.timelineArea.getY(), layout.timelineArea.getBottom());
        }

        for (auto interval : intervals)
        {
            if (! juce::isPositiveAndBelow(interval, chordEditorRows))
                continue;

            auto noteBounds = getNoteRectForSemitone(interval);
            const bool selected = selectedSemitone == interval;
            juce::ColourGradient noteFill((selected ? midibuddyui::brandTeal() : midibuddyui::brandAmber()).withAlpha(0.94f), noteBounds.getTopLeft(),
                                          midibuddyui::brandIce().withAlpha(selected ? 0.98f : 0.90f), noteBounds.getBottomRight(), false);
            g.setGradientFill(noteFill);
            g.fillRoundedRectangle(noteBounds, 4.0f);
            g.setColour(juce::Colour(0xff071118).withAlpha(0.72f));
            g.drawRoundedRectangle(noteBounds, 4.0f, 1.0f);

            if (selected)
            {
                g.setColour(midibuddyui::brandAmber());
                g.drawRoundedRectangle(noteBounds.reduced(1.2f), 4.0f, 1.2f);
            }

            g.setColour(juce::Colour(0xff071118).withAlpha(0.76f));
            g.setFont(midibuddyui::bodyFont(10.0f));
            g.drawText(juce::MidiMessage::getMidiNoteName(rootMidi + interval, true, false, 4),
                       noteBounds.toNearestInt().reduced(8, 0),
                       juce::Justification::centredLeft,
                       false);
        }

        if (isSemitoneVisible(hoveredSemitone) && ! isSemitoneActive(hoveredSemitone))
        {
            auto ghostNote = getNoteRectForSemitone(hoveredSemitone);
            g.setColour(midibuddyui::brandIce().withAlpha(0.22f));
            g.fillRoundedRectangle(ghostNote, 4.0f);
            g.setColour(midibuddyui::brandIce().withAlpha(0.70f));
            g.drawRoundedRectangle(ghostNote, 4.0f, 1.0f);
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (! processor.isChordModeActive())
            return false;

        if (selectedSemitone >= 0 && (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey))
        {
            removeSemitone(selectedSemitone);
            return true;
        }

        if (selectedSemitone >= 0 && key.getKeyCode() == juce::KeyPress::upKey)
            return nudgeSelectedNoteBy(1);

        if (selectedSemitone >= 0 && key.getKeyCode() == juce::KeyPress::downKey)
            return nudgeSelectedNoteBy(-1);

        if (key.getKeyCode() == juce::KeyPress::pageUpKey)
        {
            scrollViewBy(-4);
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::pageDownKey)
        {
            scrollViewBy(4);
            return true;
        }

        return false;
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (std::abs(wheel.deltaY) < 0.001f)
            return;

        if (event.mods.isCommandDown() || event.mods.isCtrlDown())
        {
            zoomViewBy(wheel.deltaY > 0.0f ? -1 : 1);
            return;
        }

        scrollViewBy(wheel.deltaY > 0.0f ? -1 : 1);
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        updateHover(event.position);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        updateHover({ -100.0f, -100.0f });
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        grabKeyboardFocus();
        draggedSemitone = -1;
        dragMode = DragMode::none;
        didMoveDuringDrag = false;

        const int semitone = getSemitoneAt(event.position);
        updateHover(event.position);
        if (semitone < 0)
            return;

        if (event.mods.isRightButtonDown())
        {
            if (isSemitoneActive(semitone))
                removeSemitone(semitone);
            return;
        }

        if (isSemitoneActive(semitone))
        {
            selectedSemitone = semitone;
            dragMode = DragMode::moveNote;
            draggedSemitone = semitone;
            repaint();
            return;
        }

        processor.toggleCustomNoteForSlot(slotGetter(), semitone);
        selectedSemitone = semitone;
        dragMode = DragMode::moveNote;
        draggedSemitone = semitone;
        repaint();
        if (onUpdate)
            onUpdate();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        updateHover(event.position);
        if (dragMode == DragMode::moveNote)
        {
            autoScrollDuringDrag(event.position);
            moveDraggedNoteTo(event.position);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        const int semitone = getSemitoneAt(event.position);
        if (semitone >= 0 && isSemitoneActive(semitone))
            removeSemitone(semitone);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragMode = DragMode::none;
        draggedSemitone = -1;
        didMoveDuringDrag = false;
    }

private:
    struct Layout
    {
        juce::Rectangle<float> titleArea;
        juce::Rectangle<float> topLeftArea;
        juce::Rectangle<float> rulerArea;
        juce::Rectangle<float> keyboardArea;
        juce::Rectangle<float> timelineArea;
        juce::Rectangle<float> bodyArea;
    };

    enum class DragMode
    {
        none,
        moveNote
    };

    Layout getLayout() const
    {
        auto bounds = getLocalBounds().toFloat().reduced(12.0f, 10.0f);
        auto titleArea = bounds.removeFromTop(16.0f);
        bounds.removeFromTop(6.0f);
        auto rulerStrip = bounds.removeFromTop(20.0f);
        auto bodyArea = bounds;
        const float keyboardWidth = 82.0f;
        auto keyboardArea = bodyArea.removeFromLeft(keyboardWidth);
        auto timelineArea = bodyArea;

        return {
            titleArea,
            { keyboardArea.getX(), rulerStrip.getY(), keyboardWidth, rulerStrip.getHeight() },
            { timelineArea.getX(), rulerStrip.getY(), timelineArea.getWidth(), rulerStrip.getHeight() },
            keyboardArea,
            timelineArea,
            { keyboardArea.getX(), keyboardArea.getY(), keyboardWidth + timelineArea.getWidth(), keyboardArea.getHeight() }
        };
    }

    int getVisibleRowCount() const
    {
        return juce::jlimit(chordEditorMinVisibleRows, chordEditorRows, visibleRowCount);
    }

    int getHighestVisibleSemitone() const
    {
        return juce::jlimit(0, chordEditorRows - 1, lowestVisibleSemitone + getVisibleRowCount() - 1);
    }

    bool isSemitoneVisible(int semitone) const
    {
        return juce::isPositiveAndBelow(semitone, chordEditorRows)
            && semitone >= lowestVisibleSemitone
            && semitone <= getHighestVisibleSemitone();
    }

    void clampViewState()
    {
        visibleRowCount = getVisibleRowCount();
        lowestVisibleSemitone = juce::jlimit(0,
                                             juce::jmax(0, chordEditorRows - visibleRowCount),
                                             lowestVisibleSemitone);
    }

    void ensureSemitoneVisible(int semitone)
    {
        if (! juce::isPositiveAndBelow(semitone, chordEditorRows))
            return;

        if (semitone < lowestVisibleSemitone)
            lowestVisibleSemitone = semitone;
        else if (semitone > getHighestVisibleSemitone())
            lowestVisibleSemitone = semitone - getVisibleRowCount() + 1;

        clampViewState();
    }

    void scrollViewBy(int deltaRows)
    {
        if (deltaRows == 0)
            return;

        lowestVisibleSemitone += deltaRows;
        clampViewState();
        repaint();
    }

    void zoomViewBy(int deltaRows)
    {
        if (deltaRows == 0)
            return;

        const int anchor = selectedSemitone >= 0 ? selectedSemitone
                         : hoveredSemitone >= 0 ? hoveredSemitone
                                                : lowestVisibleSemitone + getVisibleRowCount() / 2;
        visibleRowCount = juce::jlimit(chordEditorMinVisibleRows, chordEditorRows, visibleRowCount + deltaRows);
        ensureSemitoneVisible(anchor);
        repaint();
    }

    juce::Rectangle<float> getLaneRectForSemitone(int semitone) const
    {
        if (! isSemitoneVisible(semitone))
            return {};

        const auto layout = getLayout();
        const float rowHeight = layout.timelineArea.getHeight() / (float) getVisibleRowCount();
        const int row = getHighestVisibleSemitone() - juce::jlimit(0, chordEditorRows - 1, semitone);
        return { layout.bodyArea.getX(), layout.timelineArea.getY() + row * rowHeight, layout.bodyArea.getWidth(), rowHeight };
    }

    juce::Rectangle<float> getNoteRectForSemitone(int semitone) const
    {
        if (! isSemitoneVisible(semitone))
            return {};

        const auto layout = getLayout();
        const float rowHeight = layout.timelineArea.getHeight() / (float) getVisibleRowCount();
        const int row = getHighestVisibleSemitone() - juce::jlimit(0, chordEditorRows - 1, semitone);
        return {
            layout.timelineArea.getX() + 6.0f,
            layout.timelineArea.getY() + row * rowHeight + 2.0f,
            layout.timelineArea.getWidth() - 12.0f,
            juce::jmax(8.0f, rowHeight - 4.0f)
        };
    }

    int getSemitoneAt(juce::Point<float> position) const
    {
        const auto layout = getLayout();
        if (! layout.bodyArea.contains(position))
            return -1;

        const float rowHeight = layout.timelineArea.getHeight() / (float) getVisibleRowCount();
        const int row = juce::jlimit(0, getVisibleRowCount() - 1, (int) ((position.y - layout.timelineArea.getY()) / rowHeight));
        return getHighestVisibleSemitone() - row;
    }

    bool isSemitoneActive(int semitone) const
    {
        const auto intervals = processor.getSlotEditorIntervals(slotGetter());
        return std::find(intervals.begin(), intervals.end(), semitone) != intervals.end();
    }

    void updateHover(juce::Point<float> position)
    {
        const int semitone = getSemitoneAt(position);
        if (hoveredSemitone == semitone)
            return;

        hoveredSemitone = semitone;
        repaint();
    }

    void removeSemitone(int semitone)
    {
        if (! processor.isChordModeActive() || semitone < 0)
            return;

        processor.toggleCustomNoteForSlot(slotGetter(), semitone);
        if (selectedSemitone == semitone)
            selectedSemitone = -1;
        repaint();
        if (onUpdate)
            onUpdate();
    }

    bool nudgeSelectedNoteBy(int delta)
    {
        if (selectedSemitone < 0)
            return false;

        const int target = juce::jlimit(0, chordEditorRows - 1, selectedSemitone + delta);
        if (target == selectedSemitone)
            return false;

        if (! processor.moveCustomNoteForSlot(slotGetter(), selectedSemitone, target))
            return false;

        selectedSemitone = target;
        draggedSemitone = target;
        ensureSemitoneVisible(target);
        repaint();
        if (onUpdate)
            onUpdate();
        return true;
    }

    void autoScrollDuringDrag(juce::Point<float> position)
    {
        const auto layout = getLayout();
        if (position.y < layout.timelineArea.getY() + 4.0f)
            scrollViewBy(-1);
        else if (position.y > layout.timelineArea.getBottom() - 4.0f)
            scrollViewBy(1);
    }

    void moveDraggedNoteTo(juce::Point<float> position)
    {
        if (! processor.isChordModeActive())
            return;

        const int targetSemitone = getSemitoneAt(position);
        if (targetSemitone < 0 || targetSemitone == draggedSemitone)
            return;

        if (! processor.moveCustomNoteForSlot(slotGetter(), draggedSemitone, targetSemitone))
            return;

        draggedSemitone = targetSemitone;
        selectedSemitone = targetSemitone;
        didMoveDuringDrag = true;
        ensureSemitoneVisible(targetSemitone);
        repaint();
        if (onUpdate)
            onUpdate();
    }

    PluginProcessor& processor;
    std::function<int()> slotGetter;
    std::function<void()> onUpdate;
    DragMode dragMode = DragMode::none;
    int draggedSemitone = -1;
    int hoveredSemitone = -1;
    int selectedSemitone = -1;
    int lowestVisibleSemitone = 0;
    int visibleRowCount = chordEditorDefaultVisibleRows;
    bool didMoveDuringDrag = false;
};

class MidiDragTile final : public juce::Component
{
public:
    MidiDragTile(PluginProcessor& processorToUse, std::function<void()> onUpdateToUse)
        : processor(processorToUse),
          onUpdate(std::move(onUpdateToUse))
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const auto accent = dragging ? midibuddyui::brandAmber() : midibuddyui::brandTeal();
        juce::ColourGradient fill(juce::Colour(0xff18222c), bounds.getTopLeft(),
                                  juce::Colour(0xff0d141c), bounds.getBottomLeft(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds, 8.0f);

        g.setColour(juce::Colour(0xff080c10));
        g.drawRoundedRectangle(bounds, 8.0f, 1.8f);
        g.setColour(accent.withAlpha(0.95f));
        g.drawRoundedRectangle(bounds.reduced(2.0f), 6.0f, 1.0f);

        auto area = bounds.reduced(16.0f, 14.0f);
        auto footer = area.removeFromBottom(18.0f);
        area.removeFromBottom(6.0f);
        const float heroHeight = juce::jlimit(66.0f, 88.0f, area.getHeight() * 0.19f);
        auto hero = area.removeFromTop(heroHeight);
        g.setColour(accent.withAlpha(0.14f));
        g.fillRoundedRectangle(hero, 8.0f);
        g.setColour(accent.withAlpha(0.85f));
        g.drawRoundedRectangle(hero, 8.0f, 1.2f);

        auto heroText = hero.reduced(16.0f, 14.0f);
        g.setColour(midibuddyui::brandAmber());
        g.setFont(midibuddyui::smallCapsFont(11.0f));

        juce::String heroLabel = "MIDI CLIP READY";
        if (processor.isMelodyModeActive())
            heroLabel = "MELODY CLIP READY";
        else if (processor.isBasslineModeActive())
            heroLabel = "BASSLINE CLIP READY";
        else if (processor.isMotifModeActive())
            heroLabel = "MOTIF CLIP READY";

        g.drawText(heroLabel,
                   heroText.removeFromTop(16.0f).toNearestInt(),
                   juce::Justification::centredLeft,
                   false);

        g.setColour(midibuddyui::textStrong());
        g.setFont(midibuddyui::titleFont(heroHeight > 76.0f ? 26.0f : 22.0f));
        g.drawText("DRAG THIS OUT",
                   heroText.removeFromTop(heroHeight > 76.0f ? 38.0f : 32.0f).toNearestInt(),
                   juce::Justification::centredLeft,
                   false);

        g.setColour(midibuddyui::textMuted());
        g.setFont(midibuddyui::bodyFont(10.0f));
        g.drawFittedText("See the clip here, then drag this whole panel into Ableton.",
                         heroText.toNearestInt(),
                         juce::Justification::topLeft,
                         2);

        area.removeFromTop(6.0f);
        auto summaryBox = area.removeFromBottom(38.0f);
        area.removeFromBottom(6.0f);
        auto previewLabelArea = area.removeFromTop(16.0f);
        g.setColour(midibuddyui::brandAmber());
        g.setFont(midibuddyui::smallCapsFont(10.0f));
        g.drawText("Current Clip", previewLabelArea.removeFromLeft(140.0f).toNearestInt(), juce::Justification::centredLeft, false);
        g.setColour(midibuddyui::textMuted());
        g.drawText(juce::String(juce::roundToInt(processor.getClipPreviewLengthInQuarterNotes())) + " Beats",
                   previewLabelArea.toNearestInt(),
                   juce::Justification::centredRight,
                   false);

        area.removeFromTop(4.0f);
        auto previewBox = area;
        juce::ColourGradient previewFill(juce::Colour(0xff0f161d), previewBox.getTopLeft(),
                                         juce::Colour(0xff0b1016), previewBox.getBottomLeft(), false);
        g.setGradientFill(previewFill);
        g.fillRoundedRectangle(previewBox, 7.0f);
        g.setColour(midibuddyui::outlineSoft());
        g.drawRoundedRectangle(previewBox, 7.0f, 1.0f);

        auto previewArea = previewBox.reduced(10.0f, 8.0f);
        const auto previewNotes = processor.getClipPreviewNotes();
        const double totalBeats = juce::jmax(1.0, processor.getClipPreviewLengthInQuarterNotes());
        const int totalGridBeats = juce::jmax(1, juce::roundToInt(std::ceil(totalBeats)));

        int minNote = 60;
        int maxNote = 72;
        if (! previewNotes.empty())
        {
            minNote = previewNotes.front().midiNote;
            maxNote = previewNotes.front().midiNote;

            for (const auto& note : previewNotes)
            {
                minNote = juce::jmin(minNote, note.midiNote);
                maxNote = juce::jmax(maxNote, note.midiNote);
            }

            minNote -= 2;
            maxNote += 2;
        }

        const int pitchSpan = juce::jmax(12, maxNote - minNote + 1);

        for (int semitone = 0; semitone <= pitchSpan; ++semitone)
        {
            const float y = previewArea.getBottom() - ((float) semitone / (float) pitchSpan) * previewArea.getHeight();
            const int pitch = minNote + semitone;
            g.setColour((pitch % 12 == 0) ? juce::Colours::white.withAlpha(0.12f)
                                          : juce::Colours::white.withAlpha(0.05f));
            g.drawHorizontalLine(juce::roundToInt(y), previewArea.getX(), previewArea.getRight());
        }

        for (int beat = 0; beat <= totalGridBeats; ++beat)
        {
            const float x = previewArea.getX() + ((float) beat / (float) totalBeats) * previewArea.getWidth();
            g.setColour((beat % 4 == 0) ? juce::Colours::white.withAlpha(0.16f)
                                        : juce::Colours::white.withAlpha(0.07f));
            g.drawVerticalLine(juce::roundToInt(x), previewArea.getY(), previewArea.getBottom());
        }

        for (const auto& note : previewNotes)
        {
            const float x = previewArea.getX() + (float) (note.startBeat / totalBeats) * previewArea.getWidth();
            const float w = juce::jmax(6.0f, (float) (note.durationBeats / totalBeats) * previewArea.getWidth());
            const float rowHeight = previewArea.getHeight() / (float) pitchSpan;
            const float y = previewArea.getBottom() - (float) (note.midiNote - minNote + 1) * rowHeight;
            const float h = juce::jmax(6.0f, rowHeight - 2.0f);

            auto noteBounds = juce::Rectangle<float>(x + 1.0f, y + 1.0f, juce::jmax(4.0f, w - 2.0f), h);
            juce::ColourGradient noteFill(accent.withAlpha(0.88f), noteBounds.getTopLeft(),
                                          midibuddyui::brandIce().withAlpha(0.92f), noteBounds.getBottomRight(), false);
            g.setGradientFill(noteFill);
            g.fillRoundedRectangle(noteBounds, 4.0f);
            g.setColour(juce::Colour(0xff071118).withAlpha(0.7f));
            g.drawRoundedRectangle(noteBounds, 4.0f, 1.0f);
        }

        if (previewNotes.empty())
        {
            g.setColour(midibuddyui::textMuted());
            g.setFont(midibuddyui::bodyFont(11.0f));
            g.drawFittedText("Randomize to generate a clip preview.",
                             previewArea.toNearestInt(),
                             juce::Justification::centred,
                             1);
        }

        g.setColour(midibuddyui::brandAmber().withAlpha(0.12f));
        g.fillRoundedRectangle(summaryBox, 6.0f);
        g.setColour(midibuddyui::outlineSoft());
        g.drawRoundedRectangle(summaryBox, 6.0f, 1.0f);

        g.setColour(midibuddyui::textStrong());
        g.setFont(midibuddyui::bodyFont(9.8f));
        g.drawFittedText(processor.getSettingsSummary(),
                         summaryBox.reduced(12.0f, 8.0f).toNearestInt(),
                         juce::Justification::centredLeft,
                         1);
        g.setColour(dragging ? midibuddyui::brandAmber() : midibuddyui::textMuted());
        g.setFont(midibuddyui::smallCapsFont(10.0f));
        g.drawText(dragging ? "Dragging file payload..." : "Click to bake or drag this panel directly into Ableton.",
                   footer.toNearestInt(),
                   juce::Justification::centredLeft,
                   false);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        dragStarted = false;
        dragging = false;
        stagedFile = processor.exportGeneratedMidiFile();
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (dragStarted || event.getDistanceFromDragStart() < 3)
            return;

        dragStarted = true;
        dragging = true;
        repaint();

        if (! stagedFile.existsAsFile())
            stagedFile = processor.exportGeneratedMidiFile();

        if (stagedFile.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles({ stagedFile.getFullPathName() }, false, this, [this]
            {
                dragging = false;
                stagedFile = juce::File();
                repaint();
                if (onUpdate)
                    onUpdate();
            });
        else
        {
            dragging = false;
            stagedFile = juce::File();
            repaint();
            if (onUpdate)
                onUpdate();
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragging = false;
        stagedFile = juce::File();
        repaint();
        if (onUpdate)
            onUpdate();
    }

private:
    PluginProcessor& processor;
    std::function<void()> onUpdate;
    bool dragStarted = false;
    bool dragging = false;
    juce::File stagedFile;
};
}

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p),
      pluginProcessor(p)
{
    setLookAndFeel(&midibuddyui::getVectorLookAndFeel());

    styleLabel(titleLabel, "SSP MIDI Buddy", midibuddyui::titleFont(22.0f), midibuddyui::textStrong(), juce::Justification::centredLeft);
    styleLabel(tagLabel, "WRITE CHORDS FAST. KEEP THEM IN KEY.", midibuddyui::smallCapsFont(10.5f), midibuddyui::textMuted(), juce::Justification::centredLeft);
    styleLabel(helperLabel, "Pick a mode, hit Randomize, tweak the slot, then drag the clip tile into Ableton.", midibuddyui::bodyFont(10.6f), midibuddyui::textMuted(), juce::Justification::centredLeft);
    styleLabel(settingsSummaryLabel, {}, midibuddyui::smallCapsFont(10.5f), midibuddyui::brandAmber(), juce::Justification::centredLeft);
    styleLabel(progressionSummaryLabel, {}, midibuddyui::bodyFont(10.2f), midibuddyui::textStrong(), juce::Justification::centredLeft);
    styleLabel(exportStatusLabel, {}, midibuddyui::bodyFont(10.5f), midibuddyui::textMuted(), juce::Justification::centredLeft);
    styleLabel(advancedHintLabel, "Drag notes up or down. Click empty lanes to add tones, or click an active note to remove it.", midibuddyui::bodyFont(10.5f), midibuddyui::textMuted(), juce::Justification::centredLeft);
    progressionSummaryLabel.setMinimumHorizontalScale(0.72f);

    for (auto* label : { &titleLabel, &tagLabel, &helperLabel, &settingsSummaryLabel, &progressionSummaryLabel, &exportStatusLabel, &advancedHintLabel })
        addAndMakeVisible(*label);

    setupCaption(modeCaptionLabel, "Mode");
    setupCaption(keyCaptionLabel, "Key");
    setupCaption(scaleCaptionLabel, "Scale");
    setupCaption(styleCaptionLabel, "Feel");
    setupCaption(countCaptionLabel, "Count");
    setupCaption(durationCaptionLabel, "Length");
    setupCaption(playbackCaptionLabel, "Playback");
    setupCaption(rateCaptionLabel, "Rate");
    setupCaption(octaveCaptionLabel, "Octaves");
    setupCaption(registerCaptionLabel, "Register");
    setupCaption(rangeCaptionLabel, "Range");
    setupCaption(spreadCaptionLabel, "Spread");
    setupCaption(velocityCaptionLabel, "Velocity");
    setupCaption(gateCaptionLabel, "Gate");
    setupCaption(strumCaptionLabel, "Strum");
    setupCaption(swingCaptionLabel, "Swing");
    setupCaption(progressionCaptionLabel, "Progression");
    setupCaption(dragCaptionLabel, "Drag Out");
    setupCaption(slotCaptionLabel, "Selected Slot");
    setupCaption(qualityCaptionLabel, "Quality");
    setupCaption(inversionCaptionLabel, "Inversion");

    for (auto* label : { &modeCaptionLabel, &keyCaptionLabel, &scaleCaptionLabel, &styleCaptionLabel, &countCaptionLabel, &durationCaptionLabel,
                         &playbackCaptionLabel, &rateCaptionLabel, &octaveCaptionLabel, &registerCaptionLabel,
                         &rangeCaptionLabel, &spreadCaptionLabel, &velocityCaptionLabel, &gateCaptionLabel,
                         &strumCaptionLabel, &swingCaptionLabel, &progressionCaptionLabel, &dragCaptionLabel,
                         &slotCaptionLabel, &qualityCaptionLabel, &inversionCaptionLabel })
    {
        addAndMakeVisible(*label);
    }

    setupCombo(modeBox, PluginProcessor::getGenerationModeNames());
    setupCombo(keyBox, PluginProcessor::getKeyNames());
    setupCombo(scaleBox, PluginProcessor::getScaleNames());
    setupCombo(styleBox, PluginProcessor::getStyleNames());
    setupCombo(chordCountBox, PluginProcessor::getChordCountNames());
    setupCombo(chordDurationBox, PluginProcessor::getChordDurationNames());
    setupCombo(playbackBox, PluginProcessor::getPlaybackModeNames());
    setupCombo(rateBox, PluginProcessor::getArpRateNames());
    setupCombo(octaveBox, PluginProcessor::getArpOctaveNames());
    setupCombo(registerBox, PluginProcessor::getRegisterNames());
    setupCombo(rangeBox, PluginProcessor::getMelodyRangeNames());
    setupCombo(spreadBox, PluginProcessor::getSpreadNames());
    setupCombo(slotRootBox, pluginProcessor.getScaleDegreeNames());
    setupCombo(slotQualityBox, PluginProcessor::getChordQualityNames());

    for (auto* box : { &modeBox, &keyBox, &scaleBox, &styleBox, &chordCountBox, &chordDurationBox,
                       &playbackBox, &rateBox, &octaveBox, &registerBox, &rangeBox, &spreadBox,
                       &slotRootBox, &slotQualityBox })
    {
        addAndMakeVisible(*box);
    }

    setupKnob(velocitySlider, midibuddyui::brandAmber(), [] (double value)
    {
        return juce::String(juce::roundToInt(value));
    });
    setupKnob(gateSlider, midibuddyui::brandTeal(), [] (double value)
    {
        return juce::String(juce::roundToInt(value * 100.0)) + "%";
    });
    setupKnob(strumSlider, midibuddyui::brandIce(), [] (double value)
    {
        return juce::String(juce::roundToInt(value)) + " ms";
    });
    setupKnob(swingSlider, midibuddyui::brandAmber(), [] (double value)
    {
        return juce::String(juce::roundToInt(value * 100.0)) + "%";
    });

    for (auto* slider : { &velocitySlider, &gateSlider, &strumSlider, &swingSlider })
        addAndMakeVisible(*slider);

    modeAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "generationMode", modeBox);
    keyAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "keyRoot", keyBox);
    scaleAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "scaleMode", scaleBox);
    styleAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "style", styleBox);
    chordCountAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "chordCount", chordCountBox);
    chordDurationAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "chordDuration", chordDurationBox);
    playbackAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "playbackMode", playbackBox);
    rateAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "arpRate", rateBox);
    octaveAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "arpOctaves", octaveBox);
    registerAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "register", registerBox);
    rangeAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "melodyRange", rangeBox);
    spreadAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.apvts, "spread", spreadBox);
    velocityAttachment = std::make_unique<SliderAttachment>(pluginProcessor.apvts, "velocity", velocitySlider);
    gateAttachment = std::make_unique<SliderAttachment>(pluginProcessor.apvts, "gate", gateSlider);
    strumAttachment = std::make_unique<SliderAttachment>(pluginProcessor.apvts, "strum", strumSlider);
    swingAttachment = std::make_unique<SliderAttachment>(pluginProcessor.apvts, "swing", swingSlider);

    modeBox.onChange = [this]
    {
        refreshStyleBox();
        pluginProcessor.randomizeProgression();
        selectProgressionSlot(0);
        resized();
        timerCallback();
    };

    rangeBox.onChange = [this]
    {
        if (pluginProcessor.isMelodyModeActive() || pluginProcessor.isMotifModeActive())
            pluginProcessor.randomizeProgression();

        resized();
        timerCallback();
    };

    scaleBox.onChange = [this]
    {
        refreshScaleDegreeBox();
        timerCallback();
    };

    keyBox.onChange = [this]
    {
        refreshScaleDegreeBox();
        timerCallback();
    };

    chordCountBox.onChange = [this]
    {
        selectProgressionSlot(juce::jmin(selectedProgressionSlot, pluginProcessor.getVisibleSlotCount() - 1));
        timerCallback();
    };

    setupButton(randomizeButton, "Randomize", midibuddyui::brandAmber(), juce::Colour(0xff181004));
    randomizeButton.onClick = [this]
    {
        pluginProcessor.randomizeProgression();
        timerCallback();
    };
    addAndMakeVisible(randomizeButton);

    setupButton(bakeButton, "Bake MIDI", midibuddyui::brandIce(), juce::Colour(0xff071118));
    bakeButton.onClick = [this]
    {
        pluginProcessor.exportGeneratedMidiFile();
        timerCallback();
    };
    addAndMakeVisible(bakeButton);

    setupButton(exportButton, "Export...", midibuddyui::brandTeal(), juce::Colour(0xff08121a));
    exportButton.onClick = [this]
    {
        auto suggestedFile = pluginProcessor.getDefaultExportFile();
        exportChooser = std::make_unique<juce::FileChooser>("Export MIDI Clip",
                                                            suggestedFile,
                                                            "*.mid");

        juce::Component::SafePointer<PluginEditor> safeThis(this);
        exportChooser->launchAsync(juce::FileBrowserComponent::saveMode
                                       | juce::FileBrowserComponent::canSelectFiles
                                       | juce::FileBrowserComponent::warnAboutOverwriting,
                                   [safeThis] (const juce::FileChooser& chooser)
        {
            if (safeThis == nullptr)
                return;

            auto file = chooser.getResult();
            if (file == juce::File())
            {
                safeThis->exportChooser.reset();
                return;
            }

            if (file.getFileExtension().isEmpty())
                file = file.withFileExtension(".mid");

            safeThis->pluginProcessor.exportGeneratedMidiFileTo(file);
            safeThis->timerCallback();
            safeThis->exportChooser.reset();
        });
    };
    addAndMakeVisible(exportButton);

    setupButton(progressionTabButton, "Progression", juce::Colour(0xff2a3340), midibuddyui::textStrong());
    progressionTabButton.onClick = [this]
    {
        progressionView = ProgressionView::progression;
        resized();
        timerCallback();
    };
    addAndMakeVisible(progressionTabButton);

    setupButton(advancedTabButton, "Advanced", juce::Colour(0xff26303a), midibuddyui::textStrong());
    advancedTabButton.onClick = [this]
    {
        progressionView = ProgressionView::advanced;
        resized();
        timerCallback();
    };
    addAndMakeVisible(advancedTabButton);

    setupButton(inversionButton, "Root", juce::Colour(0xff2b3440), midibuddyui::textStrong());
    inversionButton.onClick = [this]
    {
        pluginProcessor.cycleProgressionSlotInversion(selectedProgressionSlot);
        refreshSelectedSlotControls();
        timerCallback();
    };
    addAndMakeVisible(inversionButton);

    slotRootBox.onChange = [this]
    {
        if (slotRootBox.getSelectedItemIndex() >= 0)
        {
            pluginProcessor.setProgressionSlotDegree(selectedProgressionSlot, slotRootBox.getSelectedItemIndex());
            timerCallback();
        }
    };

    slotQualityBox.onChange = [this]
    {
        if (slotQualityBox.getSelectedItemIndex() >= 0)
        {
            pluginProcessor.setProgressionSlotQuality(selectedProgressionSlot, slotQualityBox.getSelectedItemIndex());
            refreshSelectedSlotControls();
            timerCallback();
        }
    };

    for (int index = 0; index < (int) progressionButtons.size(); ++index)
    {
        auto button = std::make_unique<ProgressionCardButton>();
        button->onPrimaryClick = [this, index]
        {
            selectProgressionSlot(index);
            if (progressionView == ProgressionView::progression && pluginProcessor.isChordModeActive())
                progressionView = ProgressionView::progression;
            timerCallback();
        };
        button->onContextClick = [this, index]
        {
            selectProgressionSlot(index);
            showChordPickerMenu(index);
        };
        addAndMakeVisible(*button);
        progressionButtons[(size_t) index] = std::move(button);
    }

    chordEditor = std::make_unique<ChordPianoRoll>(pluginProcessor,
                                                   [this] { return selectedProgressionSlot; },
                                                   [this]
                                                   {
                                                       refreshSelectedSlotControls();
                                                       timerCallback();
                                                   });
    addAndMakeVisible(*chordEditor);

    dragTile = std::make_unique<MidiDragTile>(pluginProcessor, [this] { timerCallback(); });
    addAndMakeVisible(*dragTile);

    refreshStyleBox();
    setSize(editorWidth, editorHeight);
    selectProgressionSlot(0);
    startTimerHz(12);
    timerCallback();
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel(nullptr);
}

void PluginEditor::selectProgressionSlot(int slotIndex)
{
    selectedProgressionSlot = juce::jlimit(0, juce::jmax(0, pluginProcessor.getVisibleSlotCount() - 1), slotIndex);
    refreshSelectedSlotControls();
    repaint(progressionArea);
}

void PluginEditor::showChordPickerMenu(int slotIndex)
{
    if (! pluginProcessor.isChordModeActive())
        return;

    const auto options = pluginProcessor.getScaleChordReplacementNames(slotIndex);
    if (options.isEmpty())
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader("Replace With In-Key Chord");

    const auto currentSlot = pluginProcessor.getProgressionSlot(slotIndex);
    for (int index = 0; index < options.size(); ++index)
        menu.addItem(index + 1, options[index], true, index == currentSlot.degree);

    juce::Component* targetComponent = progressionButtons[(size_t) juce::jlimit(0, (int) progressionButtons.size() - 1, slotIndex)].get();
    juce::Component::SafePointer<PluginEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(targetComponent),
                       [safeThis, slotIndex] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        safeThis->pluginProcessor.replaceProgressionSlotWithScaleChord(slotIndex, result - 1);
        safeThis->selectProgressionSlot(slotIndex);
        safeThis->timerCallback();
    });
}

void PluginEditor::refreshScaleDegreeBox()
{
    auto items = pluginProcessor.getScaleDegreeNames();
    if (items != lastScaleDegreeItems)
    {
        lastScaleDegreeItems = items;
        setComboItems(slotRootBox, items);
    }
}

void PluginEditor::refreshStyleBox()
{
    const auto& items = pluginProcessor.isChordModeActive()
                            ? PluginProcessor::getChordFeelNames()
                            : PluginProcessor::getStyleNames();

    if (items != lastStyleItems)
    {
        const int selectedIndex = styleBox.getSelectedItemIndex();
        lastStyleItems = items;
        setComboItems(styleBox, items);
        styleBox.setSelectedItemIndex(juce::jlimit(0, items.size() - 1, selectedIndex), juce::dontSendNotification);
    }
}

void PluginEditor::refreshSelectedSlotControls()
{
    selectedProgressionSlot = juce::jlimit(0, juce::jmax(0, pluginProcessor.getVisibleSlotCount() - 1), selectedProgressionSlot);
    refreshScaleDegreeBox();

    const auto slot = pluginProcessor.getProgressionSlot(selectedProgressionSlot);
    slotCaptionLabel.setText("Selected Slot " + juce::String(selectedProgressionSlot + 1), juce::dontSendNotification);
    slotRootBox.setSelectedItemIndex(slot.degree, juce::dontSendNotification);
    slotQualityBox.setSelectedItemIndex(slot.qualityIndex, juce::dontSendNotification);

    juce::String inversionText = "Root";
    switch (slot.inversion)
    {
        case 1: inversionText = "1st Inv"; break;
        case 2: inversionText = "2nd Inv"; break;
        case 3: inversionText = "3rd Inv"; break;
        default: break;
    }

    inversionButton.setButtonText(inversionText);
}

void PluginEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    midibuddyui::drawEditorBackdrop(g, bounds);

    midibuddyui::drawPanelBackground(g, generationArea.toFloat(), midibuddyui::brandAmber());
    midibuddyui::drawPanelBackground(g, performanceArea.toFloat(), midibuddyui::brandTeal());
    midibuddyui::drawPanelBackground(g, progressionArea.toFloat(), midibuddyui::brandIce());
    midibuddyui::drawPanelBackground(g, exportArea.toFloat(), midibuddyui::brandAmber());
}

void PluginEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20, 18);

    auto header = bounds.removeFromTop(126);
    titleLabel.setBounds(header.removeFromTop(32));
    tagLabel.setBounds(header.removeFromTop(18));
    helperLabel.setBounds(header.removeFromTop(20));
    settingsSummaryLabel.setBounds(header.removeFromTop(18));
    progressionSummaryLabel.setBounds(header.removeFromTop(32));
    exportStatusLabel.setBounds(header.removeFromTop(18));

    bounds.removeFromTop(10);

    const int leftColumnWidth = juce::roundToInt((float) bounds.getWidth() * 0.53f);
    auto topRow = bounds.removeFromTop(236);
    generationArea = topRow.removeFromLeft(leftColumnWidth);
    topRow.removeFromLeft(14);
    performanceArea = topRow;

    bounds.removeFromTop(14);

    auto bottomRow = bounds;
    progressionArea = bottomRow.removeFromLeft(leftColumnWidth);
    bottomRow.removeFromLeft(14);
    exportArea = bottomRow;

    auto generation = generationArea.reduced(22, 18);
    auto generationTopCaptions = generation.removeFromTop(18);
    modeCaptionLabel.setBounds(generationTopCaptions.removeFromLeft(110));
    generationTopCaptions.removeFromLeft(12);
    keyCaptionLabel.setBounds(generationTopCaptions.removeFromLeft(80));
    generationTopCaptions.removeFromLeft(12);
    scaleCaptionLabel.setBounds(generationTopCaptions);

    auto generationTopRow = generation.removeFromTop(34);
    modeBox.setBounds(generationTopRow.removeFromLeft(110));
    generationTopRow.removeFromLeft(12);
    keyBox.setBounds(generationTopRow.removeFromLeft(80));
    generationTopRow.removeFromLeft(12);
    scaleBox.setBounds(generationTopRow);

    generation.removeFromTop(16);

    auto generationBottomCaptions = generation.removeFromTop(18);
    styleCaptionLabel.setBounds(generationBottomCaptions.removeFromLeft(170));
    generationBottomCaptions.removeFromLeft(12);
    countCaptionLabel.setBounds(generationBottomCaptions.removeFromLeft(110));
    generationBottomCaptions.removeFromLeft(12);
    durationCaptionLabel.setBounds(generationBottomCaptions.removeFromLeft(130));

    auto generationBottomRow = generation.removeFromTop(34);
    styleBox.setBounds(generationBottomRow.removeFromLeft(170));
    generationBottomRow.removeFromLeft(12);
    chordCountBox.setBounds(generationBottomRow.removeFromLeft(110));
    generationBottomRow.removeFromLeft(12);
    chordDurationBox.setBounds(generationBottomRow.removeFromLeft(130));

    generation.removeFromTop(18);
    randomizeButton.setBounds(generation.removeFromTop(42).removeFromLeft(220));

    auto performance = performanceArea.reduced(22, 18);
    const bool chordMode = pluginProcessor.isChordModeActive();
    const bool melodicMode = pluginProcessor.isMelodyModeActive() || pluginProcessor.isMotifModeActive();

    if (chordMode)
    {
        auto playbackCaptions = performance.removeFromTop(18);
        playbackCaptionLabel.setBounds(playbackCaptions.removeFromLeft(160));
        playbackCaptions.removeFromLeft(12);
        rateCaptionLabel.setBounds(playbackCaptions.removeFromLeft(110));
        playbackCaptions.removeFromLeft(12);
        octaveCaptionLabel.setBounds(playbackCaptions.removeFromLeft(100));

        auto playbackRow = performance.removeFromTop(34);
        playbackBox.setBounds(playbackRow.removeFromLeft(160));
        playbackRow.removeFromLeft(12);
        rateBox.setBounds(playbackRow.removeFromLeft(110));
        playbackRow.removeFromLeft(12);
        octaveBox.setBounds(playbackRow.removeFromLeft(100));

        performance.removeFromTop(14);
    }

    auto utilityCaptions = performance.removeFromTop(18);
    registerCaptionLabel.setBounds(utilityCaptions.removeFromLeft(110));
    utilityCaptions.removeFromLeft(12);
    if (chordMode)
        spreadCaptionLabel.setBounds(utilityCaptions.removeFromLeft(110));
    else if (melodicMode)
        rangeCaptionLabel.setBounds(utilityCaptions.removeFromLeft(110));

    auto utilityRow = performance.removeFromTop(34);
    registerBox.setBounds(utilityRow.removeFromLeft(110));
    utilityRow.removeFromLeft(12);
    if (chordMode)
        spreadBox.setBounds(utilityRow.removeFromLeft(110));
    else if (melodicMode)
        rangeBox.setBounds(utilityRow.removeFromLeft(110));

    performance.removeFromTop(14);

    auto knobCaptions = performance.removeFromTop(18);
    const int knobWidth = 108;
    const int knobGap = 12;
    velocityCaptionLabel.setBounds(knobCaptions.removeFromLeft(knobWidth));
    knobCaptions.removeFromLeft(knobGap);
    gateCaptionLabel.setBounds(knobCaptions.removeFromLeft(knobWidth));
    knobCaptions.removeFromLeft(knobGap);
    strumCaptionLabel.setBounds(knobCaptions.removeFromLeft(knobWidth));
    knobCaptions.removeFromLeft(knobGap);
    swingCaptionLabel.setBounds(knobCaptions.removeFromLeft(knobWidth));

    auto knobRow = performance.removeFromTop(136);
    velocitySlider.setBounds(knobRow.removeFromLeft(knobWidth));
    knobRow.removeFromLeft(knobGap);
    gateSlider.setBounds(knobRow.removeFromLeft(knobWidth));
    knobRow.removeFromLeft(knobGap);
    strumSlider.setBounds(knobRow.removeFromLeft(knobWidth));
    knobRow.removeFromLeft(knobGap);
    swingSlider.setBounds(knobRow.removeFromLeft(knobWidth));

    auto progression = progressionArea.reduced(22, 18);
    progressionCaptionLabel.setBounds(progression.removeFromTop(22));
    progression.removeFromTop(8);

    auto tabRow = progression.removeFromTop(34);
    progressionTabButton.setBounds(tabRow.removeFromLeft(130));
    tabRow.removeFromLeft(10);
    advancedTabButton.setBounds(tabRow.removeFromLeft(120));
    tabRow.removeFromLeft(16);
    slotCaptionLabel.setBounds(tabRow);

    progression.removeFromTop(10);
    auto slotCaptions = progression.removeFromTop(18);
    slotCaptionLabel.setBounds(slotCaptionLabel.getBounds());
    auto controlsCaptions = slotCaptions;
    controlsCaptions.removeFromLeft(180);
    qualityCaptionLabel.setBounds(controlsCaptions.removeFromLeft(160));
    controlsCaptions.removeFromLeft(12);
    inversionCaptionLabel.setBounds(controlsCaptions.removeFromLeft(120));

    auto slotRow = progression.removeFromTop(34);
    slotRootBox.setBounds(slotRow.removeFromLeft(180));
    slotRow.removeFromLeft(12);
    slotQualityBox.setBounds(slotRow.removeFromLeft(160));
    slotRow.removeFromLeft(12);
    inversionButton.setBounds(slotRow.removeFromLeft(120));

    progression.removeFromTop(10);
    advancedHintLabel.setBounds(progression.removeFromTop(18));
    progression.removeFromTop(8);

    const bool showAdvanced = progressionView == ProgressionView::advanced;
    chordEditor->setVisible(showAdvanced);
    advancedHintLabel.setVisible(showAdvanced);

    const int cardWidth = (progression.getWidth() - 14) / 2;
    const int cardHeight = (progression.getHeight() - 21) / 4;
    for (int index = 0; index < 8; ++index)
    {
        const int row = index / 2;
        const int column = index % 2;
        progressionCardBounds[(size_t) index] = juce::Rectangle<int>(
            progression.getX() + column * (cardWidth + 14),
            progression.getY() + row * (cardHeight + 7),
            cardWidth,
            cardHeight);

        if (progressionButtons[(size_t) index] != nullptr)
        {
            progressionButtons[(size_t) index]->setVisible(! showAdvanced);
            progressionButtons[(size_t) index]->setBounds(progressionCardBounds[(size_t) index]);
        }
    }

    if (chordEditor != nullptr)
        chordEditor->setBounds(progression);

    auto exportContent = exportArea.reduced(20, 16);
    dragCaptionLabel.setBounds(exportContent.removeFromTop(22));
    exportContent.removeFromTop(10);
    auto buttons = exportContent.removeFromBottom(82);
    if (dragTile != nullptr)
        dragTile->setBounds(exportContent);

    bakeButton.setBounds(buttons.removeFromTop(35));
    buttons.removeFromTop(12);
    exportButton.setBounds(buttons.removeFromTop(35));
}

void PluginEditor::timerCallback()
{
    const bool chordMode = pluginProcessor.isChordModeActive();
    const bool melodyMode = pluginProcessor.isMelodyModeActive();
    const bool basslineMode = pluginProcessor.isBasslineModeActive();
    const bool motifMode = pluginProcessor.isMotifModeActive();
    const bool melodicMode = melodyMode || motifMode;

    refreshStyleBox();

    styleCaptionLabel.setText(chordMode ? "Emotion" : "Feel", juce::dontSendNotification);
    countCaptionLabel.setText(chordMode ? "Chords" : basslineMode ? "Bass Slots" : motifMode ? "Motif Slots" : "Notes", juce::dontSendNotification);
    durationCaptionLabel.setText(chordMode ? "Length" : "Step Length", juce::dontSendNotification);
    progressionCaptionLabel.setText(chordMode ? "Progression" : basslineMode ? "Bassline" : motifMode ? "Motif" : "Melody", juce::dontSendNotification);
    randomizeButton.setButtonText(chordMode ? "Randomize Chords"
                                            : basslineMode ? "Randomize Bassline"
                                                           : motifMode ? "Randomize Motif"
                                                                       : "Randomize Melody");

    playbackCaptionLabel.setVisible(chordMode);
    rateCaptionLabel.setVisible(chordMode);
    octaveCaptionLabel.setVisible(chordMode);
    playbackBox.setVisible(chordMode);
    rateBox.setVisible(chordMode);
    octaveBox.setVisible(chordMode);
    spreadCaptionLabel.setVisible(chordMode);
    spreadBox.setVisible(chordMode);

    rangeCaptionLabel.setVisible(melodicMode);
    rangeBox.setVisible(melodicMode);

    strumCaptionLabel.setVisible(chordMode);
    strumSlider.setVisible(chordMode);

    slotQualityBox.setVisible(chordMode);
    qualityCaptionLabel.setVisible(chordMode);
    inversionButton.setVisible(chordMode);
    inversionCaptionLabel.setVisible(chordMode);

    advancedHintLabel.setText(chordMode
                                  ? "Piano-roll edit: click an empty lane to add a note, drag note blocks up or down to move them, and right-click or press Delete to remove."
                                  : "Advanced editing is focused on chord voicings. Switch back to Chords mode to use the piano-roll editor.",
                              juce::dontSendNotification);

    progressionTabButton.setColour(juce::TextButton::buttonColourId,
                                   progressionView == ProgressionView::progression ? midibuddyui::brandAmber()
                                                                                    : juce::Colour(0xff2a3340));
    progressionTabButton.setColour(juce::TextButton::textColourOffId,
                                   progressionView == ProgressionView::progression ? juce::Colour(0xff181004)
                                                                                    : midibuddyui::textStrong());
    advancedTabButton.setColour(juce::TextButton::buttonColourId,
                                progressionView == ProgressionView::advanced ? midibuddyui::brandTeal()
                                                                             : juce::Colour(0xff26303a));
    advancedTabButton.setColour(juce::TextButton::textColourOffId,
                                progressionView == ProgressionView::advanced ? juce::Colour(0xff08121a)
                                                                             : midibuddyui::textStrong());

    settingsSummaryLabel.setText(pluginProcessor.getSettingsSummary(), juce::dontSendNotification);
    progressionSummaryLabel.setText(pluginProcessor.getProgressionSummary(), juce::dontSendNotification);
    exportStatusLabel.setText(pluginProcessor.getExportStatus(), juce::dontSendNotification);

    refreshSelectedSlotControls();

    for (int index = 0; index < (int) progressionButtons.size(); ++index)
    {
        if (auto* card = dynamic_cast<ProgressionCardButton*>(progressionButtons[(size_t) index].get()))
            card->setDisplay(pluginProcessor.getProgressionSlotDisplay(index),
                             index < pluginProcessor.getVisibleSlotCount() && progressionView == ProgressionView::progression,
                             index == selectedProgressionSlot);
    }

    if (chordEditor != nullptr)
        chordEditor->repaint();

    if (dragTile != nullptr)
        dragTile->repaint();

    repaint(progressionArea);
    repaint(exportArea);
}
