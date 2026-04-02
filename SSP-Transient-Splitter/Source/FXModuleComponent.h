#pragma once

#include <JuceHeader.h>

class FXModuleComponent : public juce::Component
{
public:
    ~FXModuleComponent() override = default;
    virtual int getPreferredHeight() const = 0;
};
