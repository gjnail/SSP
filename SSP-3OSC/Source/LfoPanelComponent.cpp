#include "LfoPanelComponent.h"
#include "ReactorUI.h"

namespace
{
juce::Colour lfoAccent()
{
    return juce::Colour(0xff6fe0ff);
}

void styleLfoSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, lfoAccent());
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, reactorui::outline());
    slider.setColour(juce::Slider::textBoxTextColourId, reactorui::textStrong());
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0c1218));
    slider.setColour(juce::Slider::textBoxOutlineColourId, reactorui::outline());
}

void styleLfoCombo(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10161d));
    box.setColour(juce::ComboBox::outlineColourId, reactorui::outline());
    box.setColour(juce::ComboBox::textColourId, reactorui::textStrong());
    box.setColour(juce::ComboBox::arrowColourId, lfoAccent());
}

void styleLfoToggle(juce::ToggleButton& button)
{
    button.setColour(juce::ToggleButton::textColourId, reactorui::textStrong());
    button.setColour(juce::ToggleButton::tickColourId, lfoAccent());
    button.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff5b6678));
}

const std::vector<juce::Point<float>>& getLorenzDisplayPoints()
{
    static const std::vector<juce::Point<float>> points = []
    {
        constexpr int warmupSteps = 5000;
        constexpr int pointCount = 3200;
        constexpr int stride = 2;
        constexpr float sigma = 10.0f;
        constexpr float rho = 28.0f;
        constexpr float beta = 8.0f / 3.0f;
        constexpr float dt = 0.0038f;

        std::vector<juce::Point<float>> raw;
        raw.reserve((size_t) pointCount);

        float x = 0.1f;
        float y = 0.0f;
        float z = 24.0f;

        for (int i = 0; i < warmupSteps; ++i)
        {
            const float dx = sigma * (y - x);
            const float dy = x * (rho - z) - y;
            const float dz = x * y - beta * z;
            x += dx * dt;
            y += dy * dt;
            z += dz * dt;
        }

        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();

        for (int i = 0; i < pointCount; ++i)
        {
            for (int step = 0; step < stride; ++step)
            {
                const float dx = sigma * (y - x);
                const float dy = x * (rho - z) - y;
                const float dz = x * y - beta * z;
                x += dx * dt;
                y += dy * dt;
                z += dz * dt;
            }

            raw.emplace_back(x, z);
            minX = juce::jmin(minX, x);
            maxX = juce::jmax(maxX, x);
            minZ = juce::jmin(minZ, z);
            maxZ = juce::jmax(maxZ, z);
        }

        const float rangeX = juce::jmax(0.0001f, maxX - minX);
        const float rangeZ = juce::jmax(0.0001f, maxZ - minZ);
        constexpr float xPadding = 0.06f;
        constexpr float yPadding = 0.08f;

        for (auto& point : raw)
        {
            point.x = juce::jmap((point.x - minX) / rangeX, xPadding, 1.0f - xPadding);
            point.y = juce::jmap((point.y - minZ) / rangeZ, 1.0f - yPadding, yPadding);
        }

        return raw;
    }();

    return points;
}
}

class LfoPanelComponent::DragBadge final : public juce::Component
{
public:
    void setSelectedLfoIndex(int index)
    {
        selectedLfoIndex = juce::jmax(0, index);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(juce::Colour(0xff0b1016));
        g.fillRoundedRectangle(bounds, 9.0f);
        g.setColour(lfoAccent().withAlpha(isMouseOverOrDragging() ? 0.95f : 0.72f));
        g.drawRoundedRectangle(bounds, 9.0f, 1.4f);
        g.setColour(reactorui::textStrong());
        g.setFont(reactorui::sectionFont(10.8f));
        g.drawFittedText("DRAG LFO " + juce::String(selectedLfoIndex + 1), getLocalBounds(), juce::Justification::centred, 1);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (! event.mouseWasDraggedSinceMouseDown())
            return;

        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            if (! container->isDragAndDropActive())
                container->startDragging("LFO:" + juce::String(selectedLfoIndex + 1), this);
    }

private:
    int selectedLfoIndex = 0;
};

class TargetListComponent final : public juce::Component
{
public:
    TargetListComponent(PluginProcessor& p, int lfoIndex)
        : processor(p), selectedLfoIndex(juce::jmax(0, lfoIndex))
    {
        addAndMakeVisible(rowsViewport);
        rowsViewport.setViewedComponent(&rowsContent, false);
        rowsViewport.setScrollBarsShown(true, false);
        rebuildRows();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10, 10);
        titleLabel.setBounds(area.removeFromTop(20));
        infoLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(8);

        const int rowHeight = 30;
        const int rowGap = 6;
        rowsViewport.setBounds(area);
        const int contentHeight = juce::jmax(rowsViewport.getHeight(),
                                             (int) rows.size() * rowHeight + juce::jmax(0, (int) rows.size() - 1) * rowGap + 8);
        rowsContent.setSize(rowsViewport.getWidth(), contentHeight);

        auto rowsArea = rowsContent.getLocalBounds().reduced(0, 4);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            rows[i]->setBounds(rowsArea.removeFromTop(rowHeight));
            if (i + 1 < rows.size())
                rowsArea.removeFromTop(rowGap);
        }
    }

private:
    class TargetRow final : public juce::Component
    {
    public:
        TargetRow(PluginProcessor& p, PluginProcessor::ModulationRouteInfo routeInfo, std::function<void()> onChanged)
            : processor(p), route(routeInfo), changedCallback(std::move(onChanged))
        {
            addAndMakeVisible(nameLabel);
            addAndMakeVisible(enabledButton);
            addAndMakeVisible(removeButton);

            nameLabel.setText(route.destinationName, juce::dontSendNotification);
            nameLabel.setFont(reactorui::bodyFont(11.0f));
            nameLabel.setColour(juce::Label::textColourId, reactorui::textStrong());
            nameLabel.setJustificationType(juce::Justification::centredLeft);

            enabledButton.setButtonText(route.enabled ? "ON" : "OFF");
            enabledButton.setClickingTogglesState(true);
            enabledButton.setToggleState(route.enabled, juce::dontSendNotification);
            enabledButton.onClick = [this]
            {
                const bool enabled = enabledButton.getToggleState();
                enabledButton.setButtonText(enabled ? "ON" : "OFF");
                processor.setMatrixSlotEnabled(route.slotIndex, enabled);
                changedCallback();
            };

            removeButton.setButtonText("REMOVE");
            removeButton.onClick = [this]
            {
                processor.removeMatrixSlot(route.slotIndex);
                changedCallback();
            };
        }

        void resized() override
        {
            auto area = getLocalBounds();
            nameLabel.setBounds(area.removeFromLeft(150));
            area.removeFromLeft(8);
            enabledButton.setBounds(area.removeFromLeft(54));
            area.removeFromLeft(8);
            removeButton.setBounds(area.removeFromLeft(74));
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(0.5f);
            g.setColour(juce::Colour(0xff0d1218));
            g.fillRoundedRectangle(bounds, 7.0f);
            g.setColour(route.enabled ? reactorui::modulationSourceColour(route.sourceIndex).withAlpha(0.72f)
                                      : reactorui::outline());
            g.drawRoundedRectangle(bounds, 7.0f, 1.0f);
        }

    private:
        PluginProcessor& processor;
        PluginProcessor::ModulationRouteInfo route;
        std::function<void()> changedCallback;
        juce::Label nameLabel;
        juce::TextButton enabledButton;
        juce::TextButton removeButton;
    };

    void rebuildRows()
    {
        for (auto& row : rows)
            rowsContent.removeChildComponent(row.get());
        rows.clear();

        addAndMakeVisible(titleLabel);
        addAndMakeVisible(infoLabel);

        const auto routes = processor.getRoutesForLfo(selectedLfoIndex);
        titleLabel.setText("LFO " + juce::String(selectedLfoIndex + 1) + " Targets", juce::dontSendNotification);
        titleLabel.setFont(reactorui::sectionFont(12.0f));
        titleLabel.setColour(juce::Label::textColourId, reactorui::textStrong());
        titleLabel.setJustificationType(juce::Justification::centredLeft);

        if (routes.empty())
        {
            infoLabel.setText("No assigned targets yet.", juce::dontSendNotification);
            infoLabel.setColour(juce::Label::textColourId, reactorui::textMuted());
        }
        else
        {
            int enabledCount = 0;
            for (const auto& route : routes)
                if (route.enabled)
                    ++enabledCount;

            infoLabel.setText(juce::String(enabledCount) + " active / " + juce::String((int) routes.size()) + " total",
                              juce::dontSendNotification);
            infoLabel.setColour(juce::Label::textColourId, reactorui::textMuted());
        }
        infoLabel.setFont(reactorui::bodyFont(10.5f));
        infoLabel.setJustificationType(juce::Justification::centredLeft);

        for (const auto& route : routes)
        {
            auto row = std::make_unique<TargetRow>(processor, route, [this] { rebuildRows(); });
            rowsContent.addAndMakeVisible(*row);
            rows.push_back(std::move(row));
        }

        const int height = 58 + (int) rows.size() * 36 + 12;
        setSize(300, juce::jlimit(86, 320, height));
        if (auto* parent = getParentComponent())
            parent->resized();
        resized();
    }

    PluginProcessor& processor;
    int selectedLfoIndex = 0;
    juce::Label titleLabel;
    juce::Label infoLabel;
    juce::Viewport rowsViewport;
    juce::Component rowsContent;
    std::vector<std::unique_ptr<TargetRow>> rows;
};

class LfoPanelComponent::ShapeEditor final : public juce::Component,
                                             private juce::Timer
{
public:
    explicit ShapeEditor(PluginProcessor& p) : processor(p)
    {
        startTimerHz(24);
    }

    void setSelectedLfoIndex(int index)
    {
        selectedLfoIndex = juce::jmax(0, index);
        repaint();
    }

    void paintLorenzView(juce::Graphics& g,
                         juce::Rectangle<float> plot,
                         const reactormod::DynamicLfoData& lfo,
                         float playheadPhase) const
    {
        const auto& points = getLorenzDisplayPoints();
        if (points.size() < 2)
            return;

        const auto mapPoint = [plot] (juce::Point<float> point)
        {
            return juce::Point<float>(plot.getX() + point.x * plot.getWidth(),
                                      plot.getY() + point.y * plot.getHeight());
        };

        g.setColour(lfoAccent().withAlpha(0.18f));
        for (const auto& point : points)
            g.fillEllipse(juce::Rectangle<float>(1.6f, 1.6f).withCentre(mapPoint(point)));

        const int totalPoints = (int) points.size();
        const int headIndex = juce::jlimit(0, totalPoints - 1,
                                           juce::roundToInt(playheadPhase * (float) (totalPoints - 1)));
        const bool wraps = lfo.triggerMode != reactormod::TriggerMode::oneShot;
        const int trailLength = juce::jmin(totalPoints - 1, 360);
        const int visibleTrail = wraps ? trailLength : juce::jmin(trailLength, juce::jmax(1, headIndex));

        juce::Path trailPath;
        bool started = false;
        for (int i = visibleTrail; i >= 0; --i)
        {
            int pointIndex = headIndex - i;
            if (wraps)
                pointIndex = (pointIndex % totalPoints + totalPoints) % totalPoints;
            else if (pointIndex < 0 || pointIndex >= totalPoints)
                continue;

            const auto mapped = mapPoint(points[(size_t) pointIndex]);
            if (! started)
            {
                trailPath.startNewSubPath(mapped);
                started = true;
            }
            else
            {
                trailPath.lineTo(mapped);
            }
        }

        g.setColour(lfoAccent().withAlpha(0.92f));
        g.strokePath(trailPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto headPoint = mapPoint(points[(size_t) headIndex]);
        g.setColour(lfoAccent().withAlpha(0.30f));
        g.fillEllipse(juce::Rectangle<float>(18.0f, 18.0f).withCentre(headPoint));
        g.setColour(lfoAccent().withAlpha(0.98f));
        g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre(headPoint));
        g.setColour(juce::Colour(0xfff6fdff).withAlpha(0.88f));
        g.drawEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre(headPoint), 1.0f);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        reactorui::drawDisplayPanel(g, bounds, lfoAccent());

        auto plot = getPlotBounds();
        reactorui::drawSubtleGrid(g, plot, reactorui::displayLine().withAlpha(0.24f), 8, 4);

        const auto lfo = processor.getModulationLfo(selectedLfoIndex);
        auto stroke = buildShapePath(plot, lfo);
        const bool chaosLocked = isChaosLocked(lfo);
        const bool lorenzView = lfo.type == reactormod::LfoType::lorenz;
        const float playheadPhase = processor.getModulationLfoDisplayPhase(selectedLfoIndex);

        if (lorenzView)
        {
            paintLorenzView(g, plot, lfo, playheadPhase);
        }
        else
        {
            juce::Path fill(stroke);
            fill.lineTo(plot.getRight(), plot.getBottom());
            fill.lineTo(plot.getX(), plot.getBottom());
            fill.closeSubPath();

            g.setColour(lfoAccent().withAlpha(0.16f));
            g.fillPath(fill);
            g.setColour(lfoAccent().withAlpha(0.98f));
            g.strokePath(stroke, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float playheadValue = evaluatePlayheadValue(lfo, playheadPhase);
            const float playheadX = plot.getX() + playheadPhase * plot.getWidth();
            const float playheadY = plot.getBottom() - juce::jmap(playheadValue, -1.0f, 1.0f, 0.0f, plot.getHeight());
            g.setColour(juce::Colour(0xffffffff).withAlpha(0.12f));
            g.drawVerticalLine(juce::roundToInt(playheadX), plot.getY(), plot.getBottom());
            g.setColour(lfoAccent().withAlpha(0.98f));
            g.fillEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre({ playheadX, playheadY }));
            g.setColour(juce::Colour(0xfff6fdff).withAlpha(0.92f));
            g.drawEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre({ playheadX, playheadY }), 1.0f);
        }

        if (! chaosLocked)
        {
            for (size_t i = 0; i < lfo.nodes.size(); ++i)
            {
                const auto nodePoint = nodeToPoint(plot, lfo.nodes[i]);
                g.setColour(juce::Colour(0xfff3fbff).withAlpha(i == (size_t) hoveredNode ? 1.0f : 0.95f));
                g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(nodePoint));

                if (i + 1 < lfo.nodes.size())
                {
                    const auto handlePoint = curveHandlePoint(plot, lfo.nodes[i], lfo.nodes[i + 1]);
                    g.setColour(lfoAccent().withAlpha(0.55f));
                    g.drawLine(nodePoint.x, nodePoint.y, handlePoint.x, handlePoint.y, 1.0f);
                    g.drawLine(nodeToPoint(plot, lfo.nodes[i + 1]).x, nodeToPoint(plot, lfo.nodes[i + 1]).y, handlePoint.x, handlePoint.y, 1.0f);
                    g.setColour(juce::Colour(0xffc5f6ff).withAlpha((int) i == hoveredCurve ? 0.95f : 0.78f));
                    g.fillEllipse(juce::Rectangle<float>(6.0f, 6.0f).withCentre(handlePoint));
                }
            }
        }
        else
        {
            g.setColour(juce::Colour(0xff0b1016).withAlpha(0.78f));
            g.fillRoundedRectangle(juce::Rectangle<float>(148.0f, 24.0f).withCentre({ plot.getCentreX(), plot.getY() + 16.0f }), 8.0f);
            g.setColour(lfoAccent().withAlpha(0.84f));
            g.drawRoundedRectangle(juce::Rectangle<float>(148.0f, 24.0f).withCentre({ plot.getCentreX(), plot.getY() + 16.0f }), 8.0f, 1.0f);
            g.setColour(reactorui::textStrong());
            g.setFont(reactorui::sectionFont(10.2f));
            g.drawFittedText(lorenzView ? "LORENZ ATTRACTOR - SHAPE LOCKED" : "CHAOS TYPE - SHAPE LOCKED",
                             juce::Rectangle<int>(juce::roundToInt(plot.getCentreX() - 74.0f), juce::roundToInt(plot.getY() + 4.0f), 148, 24),
                             juce::Justification::centred,
                             1);
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        updateHoverTargets(event.position);
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        hoveredNode = -1;
        hoveredCurve = -1;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        const auto lfo = processor.getModulationLfo(selectedLfoIndex);
        if (isChaosLocked(lfo))
            return;
        activeNode = findNodeAt(event.position, lfo);
        activeCurve = activeNode < 0 ? findCurveAt(event.position, lfo) : -1;
        lastMouse = event.position;

        if (event.mods.isRightButtonDown())
        {
            if (activeNode > 0 && activeNode < (int) lfo.nodes.size() - 1)
            {
                auto updated = lfo;
                updated.nodes.erase(updated.nodes.begin() + activeNode);
                processor.updateModulationLfo(selectedLfoIndex, updated);
                activeNode = -1;
            }
            return;
        }

        if (event.getNumberOfClicks() >= 2 && activeNode < 0)
        {
            auto updated = lfo;
            addNodeAt(updated, event.position);
            processor.updateModulationLfo(selectedLfoIndex, updated);
            return;
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        auto lfo = processor.getModulationLfo(selectedLfoIndex);
        if (isChaosLocked(lfo))
            return;
        auto plot = getPlotBounds();

        if (activeNode >= 0 && juce::isPositiveAndBelow(activeNode, (int) lfo.nodes.size()))
        {
            auto& node = lfo.nodes[(size_t) activeNode];
            node.y = juce::jlimit(0.0f, 1.0f, 1.0f - (event.position.y - plot.getY()) / juce::jmax(1.0f, plot.getHeight()));

            if (activeNode > 0 && activeNode < (int) lfo.nodes.size() - 1)
            {
                const float x = juce::jlimit(0.0f, 1.0f, (event.position.x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
                node.x = juce::jlimit(lfo.nodes[(size_t) activeNode - 1].x + 0.01f,
                                      lfo.nodes[(size_t) activeNode + 1].x - 0.01f,
                                      x);
            }

            processor.updateModulationLfo(selectedLfoIndex, lfo);
            return;
        }

        if (activeCurve >= 0 && juce::isPositiveAndBelow(activeCurve, (int) lfo.nodes.size() - 1))
        {
            auto& node = lfo.nodes[(size_t) activeCurve];
            const float delta = juce::jlimit(-0.95f, 0.95f, (lastMouse.y - event.position.y) / 120.0f);
            node.curveToNext = juce::jlimit(-0.95f, 0.95f, node.curveToNext + delta);
            lastMouse = event.position;
            processor.updateModulationLfo(selectedLfoIndex, lfo);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        activeNode = -1;
        activeCurve = -1;
    }

private:
    juce::Rectangle<float> getPlotBounds() const
    {
        return getLocalBounds().toFloat().reduced(18.0f, 16.0f);
    }

    juce::Point<float> nodeToPoint(juce::Rectangle<float> plot, const reactormod::LfoNode& node) const
    {
        return {
            plot.getX() + node.x * plot.getWidth(),
            plot.getBottom() - node.y * plot.getHeight()
        };
    }

    juce::Point<float> curveHandlePoint(juce::Rectangle<float> plot,
                                        const reactormod::LfoNode& a,
                                        const reactormod::LfoNode& b) const
    {
        const float t = 0.5f;
        const float curved = reactormod::applyCurve(t, a.curveToNext);
        const float x = juce::jmap(t, a.x, b.x);
        const float y = juce::jmap(curved, a.y, b.y);
        return nodeToPoint(plot, { x, y, 0.0f });
    }

    juce::Path buildShapePath(juce::Rectangle<float> plot, const reactormod::DynamicLfoData& lfo) const
    {
        juce::Path path;
        if (plot.isEmpty())
            return path;

        constexpr int steps = 96;
        for (int i = 0; i <= steps; ++i)
        {
            const float phase = (float) i / (float) steps;
            const float value = reactormod::evaluateLfoValue(lfo, phase, selectedLfoIndex + 1);
            const auto point = juce::Point<float>(plot.getX() + phase * plot.getWidth(),
                                                  plot.getBottom() - juce::jmap(value, -1.0f, 1.0f, 0.0f, plot.getHeight()));
            if (i == 0)
                path.startNewSubPath(point);
            else
                path.lineTo(point);
        }
        return path;
    }

    float evaluatePlayheadValue(const reactormod::DynamicLfoData& lfo, float phase) const
    {
        if (lfo.triggerMode == reactormod::TriggerMode::oneShot && phase >= 0.9999f)
            return reactormod::evaluateLfoValue(lfo, 0.9999f, selectedLfoIndex + 1);

        return reactormod::evaluateLfoValue(lfo, phase, selectedLfoIndex + 1);
    }

    int findNodeAt(juce::Point<float> position, const reactormod::DynamicLfoData& lfo) const
    {
        const auto plot = getPlotBounds();
        for (size_t i = 0; i < lfo.nodes.size(); ++i)
        {
            if (nodeToPoint(plot, lfo.nodes[i]).getDistanceFrom(position) <= 10.0f)
                return (int) i;
        }
        return -1;
    }

    int findCurveAt(juce::Point<float> position, const reactormod::DynamicLfoData& lfo) const
    {
        const auto plot = getPlotBounds();
        for (size_t i = 0; i + 1 < lfo.nodes.size(); ++i)
        {
            if (curveHandlePoint(plot, lfo.nodes[i], lfo.nodes[i + 1]).getDistanceFrom(position) <= 10.0f)
                return (int) i;
        }
        return -1;
    }

    void addNodeAt(reactormod::DynamicLfoData& lfo, juce::Point<float> position)
    {
        auto plot = getPlotBounds();
        reactormod::LfoNode node;
        node.x = juce::jlimit(0.0f, 1.0f, (position.x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
        node.y = juce::jlimit(0.0f, 1.0f, 1.0f - (position.y - plot.getY()) / juce::jmax(1.0f, plot.getHeight()));
        node.curveToNext = 0.0f;

        auto insertIt = std::lower_bound(lfo.nodes.begin(), lfo.nodes.end(), node.x,
            [] (const auto& existing, float x) { return existing.x < x; });

        if (insertIt != lfo.nodes.end())
            insertIt->curveToNext = 0.0f;
        if (insertIt != lfo.nodes.begin())
            (insertIt - 1)->curveToNext = 0.0f;

        lfo.nodes.insert(insertIt, node);
    }

    void updateHoverTargets(juce::Point<float> position)
    {
        const auto lfo = processor.getModulationLfo(selectedLfoIndex);
        if (isChaosLocked(lfo))
        {
            hoveredNode = -1;
            hoveredCurve = -1;
            repaint();
            return;
        }
        hoveredNode = findNodeAt(position, lfo);
        hoveredCurve = hoveredNode < 0 ? findCurveAt(position, lfo) : -1;
        repaint();
    }

    bool isChaosLocked(const reactormod::DynamicLfoData& lfo) const
    {
        return reactormod::isChaosType(lfo.type);
    }

    void timerCallback() override
    {
        repaint();
    }

    PluginProcessor& processor;
    int selectedLfoIndex = 0;
    int activeNode = -1;
    int activeCurve = -1;
    int hoveredNode = -1;
    int hoveredCurve = -1;
    juce::Point<float> lastMouse;
};

LfoPanelComponent::LfoPanelComponent(PluginProcessor& p)
    : processor(p)
{
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(subLabel);
    addAndMakeVisible(selectorLabel);
    addAndMakeVisible(selectorBox);
    addAndMakeVisible(typeLabel);
    addAndMakeVisible(typeBox);
    addAndMakeVisible(rateLabel);
    addAndMakeVisible(rateSlider);
    addAndMakeVisible(addButton);
    addAndMakeVisible(targetsButton);
    addAndMakeVisible(countLabel);
    addAndMakeVisible(syncButton);
    addAndMakeVisible(dottedButton);
    addAndMakeVisible(triggerLabel);
    addAndMakeVisible(triggerBox);

    dragBadge = std::make_unique<DragBadge>();
    shapeEditor = std::make_unique<ShapeEditor>(processor);
    addAndMakeVisible(*dragBadge);
    addAndMakeVisible(*shapeEditor);

    titleLabel.setText("LFO EDITOR", juce::dontSendNotification);
    reactorui::styleTitle(titleLabel, 15.0f);

    subLabel.setText("Double-click to add a point, right-click a point to remove it, and drag the segment handles to curve the shape.",
                     juce::dontSendNotification);
    reactorui::styleMeta(subLabel, 11.2f);

    selectorLabel.setText("Active LFO", juce::dontSendNotification);
    selectorLabel.setFont(reactorui::bodyFont(11.0f));
    selectorLabel.setColour(juce::Label::textColourId, reactorui::textMuted());

    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setFont(reactorui::bodyFont(11.0f));
    typeLabel.setColour(juce::Label::textColourId, reactorui::textMuted());

    rateLabel.setText("Rate", juce::dontSendNotification);
    rateLabel.setFont(reactorui::bodyFont(11.0f));
    rateLabel.setColour(juce::Label::textColourId, reactorui::textMuted());
    rateLabel.setJustificationType(juce::Justification::centred);

    triggerLabel.setText("Trigger", juce::dontSendNotification);
    triggerLabel.setFont(reactorui::bodyFont(11.0f));
    triggerLabel.setColour(juce::Label::textColourId, reactorui::textMuted());

    styleLfoCombo(selectorBox);
    styleLfoCombo(typeBox);
    styleLfoCombo(triggerBox);
    typeBox.addItemList(reactormod::getLfoTypeNames(), 1);
    triggerBox.addItemList(reactormod::getTriggerModeNames(), 1);

    styleLfoSlider(rateSlider);

    syncButton.setButtonText("Sync");
    dottedButton.setButtonText("Dotted");
    styleLfoToggle(syncButton);
    styleLfoToggle(dottedButton);

    rateSlider.onValueChange = [this]
    {
        if (syncing)
            return;
        auto lfo = processor.getModulationLfo(selectedLfoIndex);
        if (lfo.syncEnabled)
            lfo.syncDivisionIndex = juce::jlimit(0, (int) reactormod::getSyncDivisionDefinitions().size() - 1,
                                                 juce::roundToInt((float) rateSlider.getValue()));
        else
            lfo.rateHz = (float) rateSlider.getValue();
        processor.updateModulationLfo(selectedLfoIndex, lfo);
    };

    syncButton.onClick = [this]
    {
        if (syncing)
            return;
        auto lfo = processor.getModulationLfo(selectedLfoIndex);
        lfo.syncEnabled = syncButton.getToggleState();
        processor.updateModulationLfo(selectedLfoIndex, lfo);
        syncFromProcessor();
    };

    dottedButton.onClick = [this]
    {
        if (syncing)
            return;
        auto lfo = processor.getModulationLfo(selectedLfoIndex);
        lfo.dotted = dottedButton.getToggleState();
        processor.updateModulationLfo(selectedLfoIndex, lfo);
        configureRateSliderForCurrentMode();
        syncFromProcessor();
    };

    triggerBox.onChange = [this]
    {
        if (syncing)
            return;
        auto lfo = processor.getModulationLfo(selectedLfoIndex);
        lfo.triggerMode = (reactormod::TriggerMode) juce::jmax(0, triggerBox.getSelectedItemIndex());
        processor.updateModulationLfo(selectedLfoIndex, lfo);
    };

    selectorBox.onChange = [this]
    {
        if (selectorBox.getNumItems() <= 0)
            return;
        selectedLfoIndex = juce::jmax(0, selectorBox.getSelectedItemIndex());
        syncFromProcessor();
    };

    typeBox.onChange = [this]
    {
        if (syncing)
            return;
        auto lfo = processor.getModulationLfo(selectedLfoIndex);
        lfo.type = (reactormod::LfoType) juce::jmax(0, typeBox.getSelectedItemIndex());
        processor.updateModulationLfo(selectedLfoIndex, lfo);
        syncFromProcessor();
    };

    addButton.setButtonText("ADD LFO");
    addButton.onClick = [this]
    {
        selectedLfoIndex = processor.addModulationLfo();
        refreshSelector();
        syncFromProcessor();
    };

    targetsButton.setButtonText("No Targets");
    targetsButton.onClick = [this]
    {
        auto content = std::make_unique<TargetListComponent>(processor, selectedLfoIndex);
        juce::CallOutBox::launchAsynchronously(std::move(content), targetsButton.getScreenBounds(), nullptr);
    };

    countLabel.setFont(reactorui::bodyFont(11.0f));
    countLabel.setColour(juce::Label::textColourId, lfoAccent().withAlpha(0.82f));
    countLabel.setJustificationType(juce::Justification::centredRight);

    refreshSelector();
    syncFromProcessor();
    startTimerHz(8);
}

LfoPanelComponent::~LfoPanelComponent() = default;

void LfoPanelComponent::paint(juce::Graphics& g)
{
    reactorui::drawPanelBackground(g, getLocalBounds().toFloat(), lfoAccent());
}

void LfoPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(18, 14);
    titleLabel.setBounds(area.removeFromTop(22));
    subLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(12);

    const int controlWidth = juce::jlimit(280, 360, area.getWidth() / 4);
    auto controlColumn = area.removeFromLeft(controlWidth);
    area.removeFromLeft(16);

    selectorLabel.setBounds(controlColumn.removeFromTop(16));
    selectorBox.setBounds(controlColumn.removeFromTop(30));
    controlColumn.removeFromTop(10);
    typeLabel.setBounds(controlColumn.removeFromTop(16));
    typeBox.setBounds(controlColumn.removeFromTop(30));
    controlColumn.removeFromTop(10);
    addButton.setBounds(controlColumn.removeFromTop(32));
    controlColumn.removeFromTop(8);
    auto statsRow = controlColumn.removeFromTop(24);
    countLabel.setBounds(statsRow.removeFromLeft(78));
    statsRow.removeFromLeft(8);
    targetsButton.setBounds(statsRow);
    controlColumn.removeFromTop(10);
    dragBadge->setBounds(controlColumn.removeFromTop(36));
    controlColumn.removeFromTop(14);

    syncButton.setBounds(controlColumn.removeFromTop(24));
    controlColumn.removeFromTop(8);
    dottedButton.setBounds(controlColumn.removeFromTop(24));
    controlColumn.removeFromTop(10);

    triggerLabel.setBounds(controlColumn.removeFromTop(16));
    triggerBox.setBounds(controlColumn.removeFromTop(30));
    controlColumn.removeFromTop(12);

    rateLabel.setBounds(controlColumn.removeFromTop(16));
    rateSlider.setVisible(true);
    rateSlider.setBounds(controlColumn.removeFromTop(126));

    shapeEditor->setBounds(area);
}

void LfoPanelComponent::refreshSelector()
{
    syncing = true;
    selectorBox.clear(juce::dontSendNotification);
    const auto names = processor.getModulationLfoNames();
    for (int i = 1; i < names.size(); ++i)
        selectorBox.addItem(names[i], i);

    if (selectorBox.getNumItems() == 0)
        selectedLfoIndex = processor.addModulationLfo();

    selectedLfoIndex = juce::jlimit(0, juce::jmax(0, selectorBox.getNumItems() - 1), selectedLfoIndex);
    selectorBox.setSelectedItemIndex(selectedLfoIndex, juce::dontSendNotification);
    syncing = false;
}

void LfoPanelComponent::syncFromProcessor()
{
    const auto lfo = processor.getModulationLfo(selectedLfoIndex);
    syncing = true;
    selectorBox.setSelectedItemIndex(selectedLfoIndex, juce::dontSendNotification);
    syncButton.setToggleState(lfo.syncEnabled, juce::dontSendNotification);
    dottedButton.setToggleState(lfo.dotted, juce::dontSendNotification);
    typeBox.setSelectedItemIndex((int) lfo.type, juce::dontSendNotification);
    triggerBox.setSelectedItemIndex((int) lfo.triggerMode, juce::dontSendNotification);
    configureRateSliderForCurrentMode();
    rateSlider.setValue(lfo.syncEnabled ? (double) lfo.syncDivisionIndex
                                        : (double) lfo.rateHz,
                        juce::dontSendNotification);
    syncing = false;

    refreshRouteSummary();
    updateEditorDescription();
    dragBadge->setSelectedLfoIndex(selectedLfoIndex);
    shapeEditor->setSelectedLfoIndex(selectedLfoIndex);
    resized();
    repaint();
}

void LfoPanelComponent::timerCallback()
{
    const int processorLfoCount = processor.getModulationLfoCount();
    if (processorLfoCount != selectorBox.getNumItems())
    {
        refreshSelector();
        syncFromProcessor();
        return;
    }

    refreshRouteSummary();
}

void LfoPanelComponent::refreshRouteSummary()
{
    countLabel.setText(juce::String(processor.getModulationLfoCount()) + " LFOs", juce::dontSendNotification);

    const auto routes = processor.getRoutesForLfo(selectedLfoIndex);
    const auto routeCount = (int) routes.size();
    targetsButton.setButtonText(routeCount == 0 ? "No Targets"
                                                : juce::String(routeCount) + (routeCount == 1 ? " Target" : " Targets"));
}

void LfoPanelComponent::configureRateSliderForCurrentMode()
{
    const auto lfo = processor.getModulationLfo(selectedLfoIndex);
    const bool syncEnabled = lfo.syncEnabled;
    const bool dotted = lfo.dotted;

    if (syncEnabled)
    {
        const int maxIndex = juce::jmax(0, (int) reactormod::getSyncDivisionDefinitions().size() - 1);
        rateSlider.setRange(0.0, (double) maxIndex, 1.0);
        rateSlider.setNumDecimalPlacesToDisplay(0);
        rateSlider.setTextValueSuffix({});
        rateSlider.textFromValueFunction = [dotted] (double value)
        {
            return reactormod::getSyncDivisionName(juce::roundToInt((float) value), dotted);
        };
        rateSlider.valueFromTextFunction = [] (const juce::String& text)
        {
            const auto trimmed = text.trim();
            const bool dottedText = trimmed.endsWithChar('.');
            const auto names = reactormod::getSyncDivisionNames(dottedText);
            for (int i = 0; i < names.size(); ++i)
                if (names[i].equalsIgnoreCase(trimmed))
                    return (double) i;

            return 0.0;
        };
    }
    else
    {
        rateSlider.setRange(0.05, 20.0, 0.001);
        rateSlider.setNumDecimalPlacesToDisplay(3);
        rateSlider.setTextValueSuffix(" Hz");
        rateSlider.textFromValueFunction = {};
        rateSlider.valueFromTextFunction = {};
    }

    rateSlider.repaint();
}

void LfoPanelComponent::updateEditorDescription()
{
    const auto lfo = processor.getModulationLfo(selectedLfoIndex);
    if (reactormod::isChaosType(lfo.type))
    {
        subLabel.setText("Chaos type is generating the shape automatically. Editing is locked like Serum while this type is active.",
                         juce::dontSendNotification);
    }
    else
    {
        subLabel.setText("Double-click to add a point, right-click a point to remove it, and drag the segment handles to curve the shape.",
                         juce::dontSendNotification);
    }
}
