#pragma once

// The free and Pro editions are built from the same code; VK_PRO picks which.
#ifndef VK_PRO
 #define VK_PRO 0
#endif

namespace edition
{
    constexpr bool isPro = VK_PRO != 0;

    // How many moods can sit around the knob.
    constexpr int maxRingMoods = 8;

    inline const char* name()    { return isPro ? "Vibe Knob Pro" : "Vibe Knob"; }
    inline const char* proName() { return "Vibe Knob Pro"; }
}
