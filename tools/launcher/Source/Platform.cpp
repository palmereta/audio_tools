#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <shellapi.h>
 #pragma comment (lib, "shell32.lib")
#endif

#include "Platform.h"

namespace atl::platform
{
bool canWrite (const juce::File& dir)
{
    if (! dir.createDirectory()) return false;
    const auto probe = dir.getNonexistentChildFile (".audiotools-probe", ".tmp", false);
    const bool ok = probe.replaceWithText ("x");
    probe.deleteFile();
    return ok;
}

bool relaunchElevated()
{
   #if JUCE_WINDOWS
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
    const auto r = (INT_PTR) ShellExecuteW (nullptr, L"runas", exe.toWideCharPointer(), nullptr, nullptr, SW_SHOWNORMAL);
    return r > 32;
   #else
    return false;
   #endif
}
}
