#pragma once
// Latencia que cambia mientras suena (la calidad de sobremuestreo es automatizable): el hilo de audio pide el valor
// nuevo y el host se entera desde el hilo de mensajes. En prepareToPlay se informa directamente.

#include <JuceHeader.h>

namespace dh {

class LatencyReporter : private juce::AsyncUpdater
{
public:
    explicit LatencyReporter (juce::AudioProcessor& p) : proc (p) {}
    ~LatencyReporter() override { cancelPendingUpdate(); }
    void setNow (int samples) { wanted.store (samples); proc.setLatencySamples (samples); }
    void request (int samples) { if (wanted.exchange (samples) != samples) triggerAsyncUpdate(); }

private:
    void handleAsyncUpdate() override { proc.setLatencySamples (wanted.load()); }
    juce::AudioProcessor& proc;
    std::atomic<int> wanted { -1 };
};

} // namespace dh
