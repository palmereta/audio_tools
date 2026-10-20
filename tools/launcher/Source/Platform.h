#pragma once

#include <juce_core/juce_core.h>

namespace atl::platform
{
/** True if the launcher can create files in this folder (creating it if needed). */
bool canWrite (const juce::File& dir);

/** Windows: starts this executable again through the UAC prompt ("runas"). False elsewhere or
    if the user declined. */
bool relaunchElevated (const juce::String& arguments = {});
}
