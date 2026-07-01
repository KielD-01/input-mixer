#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include "input-mixer-core/src/ffi.rs.h"

namespace input_mixer
{

inline juce::String toJuceString(const rust::String& s)
{
    return juce::String(std::string(rust::String(s)));
}

} // namespace input_mixer
