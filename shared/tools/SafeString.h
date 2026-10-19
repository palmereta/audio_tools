#pragma once
// Texto compartido entre hilos (p. ej. el nombre del preset): el host puede restaurar el estado o cambiar de
// programa desde otro hilo mientras el editor lo lee, y un juce::String no admite escritura y lectura a la vez
// (la lectura puede quedarse con el texto ya liberado). Se usa como un juce::String.

#include <JuceHeader.h>

namespace dh {

class SafeString
{
public:
    SafeString (const char* s = "") : value (s) {}
    SafeString& operator= (const juce::String& s) { const juce::SpinLock::ScopedLockType l (lock); value = s; return *this; }
    juce::String get() const { const juce::SpinLock::ScopedLockType l (lock); return value; }
    operator juce::String() const { return get(); }
    juce::String substring (int a, int b) const { return get().substring (a, b); }
    bool operator== (const juce::String& s) const { return get() == s; }
    bool isNotEmpty() const { return get().isNotEmpty(); }
    const char* toRawUTF8() const = delete;      // devolvería un puntero a una copia temporal

private:
    juce::String value;
    mutable juce::SpinLock lock;
};

inline juce::String operator+ (const juce::String& a, const SafeString& b) { return a + b.get(); }
inline juce::String operator+ (const char* a, const SafeString& b) { return juce::String (a) + b.get(); }
inline bool operator== (const juce::String& a, const SafeString& b) { return a == b.get(); }

} // namespace dh
