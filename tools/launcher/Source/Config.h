#pragma once

// The few values someone may want to change without reading the rest of the launcher.
namespace atl::config
{
    // Where the launcher reads the list of plugins. scripts/build-web.py writes it to
    // web/downloads/manifest.json, and the site serves web/ at this address.
    inline constexpr const char* kSiteUrl     = "https://audiotools.com.ar/";
    inline constexpr const char* kManifestUrl = "https://audiotools.com.ar/downloads/manifest.json";

    // The "Support Audio Tools" button. The manifest's "donate_url" wins over this value, so the
    // link can change without shipping a new launcher. Empty in both places hides the button.
    inline constexpr const char* kDonateUrl   = "https://buymeacoffee.com/efectopalma";
}
