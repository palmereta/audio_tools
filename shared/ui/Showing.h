#pragma once
// ¿El editor está de verdad a la vista? Live crea el editor al insertar el plugin y después sólo esconde su ventana
// (pista sin seleccionar, Live en segundo plano) sin cerrarlo; en macOS Component::isShowing() sigue en true porque
// sólo mira si la ventana está minimizada. Con la ventana nativa escondida los timers del editor no tienen que
// repintar ni animar (QA en Live 2026-10-22, H3: 8-26 % de un núcleo por editor escondido).
// Uso, al principio de cada timerCallback que repinta: if (! dh::reallyShowing (*this)) return;
//
// En macOS se mira además el estado de oclusión de la ventana (NSWindowOcclusionStateVisible): una ventana escondida,
// minimizada, en otro escritorio o tapada entera por otras no está a la vista aunque [window isVisible] diga que sí
// (no se sabe cómo esconde Live sus ventanas de plugins; la oclusión cubre todos los casos).

#include <JuceHeader.h>
#if JUCE_MAC
 #include <objc/message.h>
 #include <objc/runtime.h>
#endif

namespace dh {

inline bool reallyShowing (const juce::Component& c)
{
    if (! c.isShowing()) return false;
    auto* peer = c.getPeer();
    if (peer == nullptr || ! peer->isShowing()) return false;
   #if JUCE_MAC
    if (auto view = (id) peer->getNativeHandle())
    {
        static const SEL selWindow = sel_registerName ("window"), selOcclusion = sel_registerName ("occlusionState"),
                         selHidden = sel_registerName ("isHiddenOrHasHiddenAncestor");
        if (reinterpret_cast<BOOL (*) (id, SEL)> (objc_msgSend) (view, selHidden)) return false;   // vista escondida por el host
        auto win = reinterpret_cast<id (*) (id, SEL)> (objc_msgSend) (view, selWindow);
        if (win == nullptr) return false;
        const auto state = reinterpret_cast<unsigned long (*) (id, SEL)> (objc_msgSend) (win, selOcclusion);
        return (state & (1ul << 1)) != 0;      // NSWindowOcclusionStateVisible
    }
   #endif
    return true;
}

} // namespace dh
