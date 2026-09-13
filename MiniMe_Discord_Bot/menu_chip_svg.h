#ifndef MENU_CHIP_SVG_H
#define MENU_CHIP_SVG_H

// Dark UI chip (k9dtv.com menu-chip.svg)
static const char MENU_CHIP_SVG[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" role="img" aria-label="K9DTV menu chip">
  <rect x="1" y="1" width="30" height="30" rx="3" fill="#0d0f14" stroke="#2a3344" stroke-width="1"/>
  <g transform="translate(16 16) scale(1.1) translate(-16 -16)">
    <rect x="9" y="9" width="14" height="14" rx="1.2" fill="#161a22" stroke="#8899aa" stroke-width="1.1"/>
    <rect x="11.5" y="11.5" width="9" height="9" fill="#0d0f14" stroke="#2a3344" stroke-width="0.8"/>
    <g stroke="#8899aa" stroke-width="1" stroke-linecap="square" fill="none">
      <path d="M11.5 11 V8"/><path d="M16 11 V8"/><path d="M20.5 11 V8"/>
      <path d="M11.5 21 V24"/><path d="M16 21 V24"/><path d="M20.5 21 V24"/>
      <path d="M11 11.5 H8"/><path d="M11 16 H8"/><path d="M11 20.5 H8"/>
      <path d="M21 11.5 H24"/><path d="M21 16 H24"/><path d="M21 20.5 H24"/>
    </g>
  </g>
</svg>)SVG";

// Light UI chip (k9dtv.com menu-chip-bright.svg)
static const char MENU_CHIP_BRIGHT_SVG[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" role="img" aria-label="K9DTV menu chip">
  <rect x="1" y="1" width="30" height="30" rx="3" fill="#ffffff" stroke="#0e8499" stroke-width="1"/>
  <g transform="translate(16 16) scale(1.1) translate(-16 -16)">
    <rect x="9" y="9" width="14" height="14" rx="1.2" fill="#f0fafb" stroke="#0e8499" stroke-width="1.1"/>
    <rect x="11.5" y="11.5" width="9" height="9" fill="#e0f2f5" stroke="#0e8499" stroke-width="0.8"/>
    <g stroke="#0e8499" stroke-width="1" stroke-linecap="square" fill="none">
      <path d="M11.5 11 V8"/><path d="M16 11 V8"/><path d="M20.5 11 V8"/>
      <path d="M11.5 21 V24"/><path d="M16 21 V24"/><path d="M20.5 21 V24"/>
      <path d="M11 11.5 H8"/><path d="M11 16 H8"/><path d="M11 20.5 H8"/>
      <path d="M21 11.5 H24"/><path d="M21 16 H24"/><path d="M21 20.5 H24"/>
    </g>
    <circle cx="16" cy="16" r="1.35" fill="#c2410c" stroke="none"/>
  </g>
</svg>)SVG";

#endif
