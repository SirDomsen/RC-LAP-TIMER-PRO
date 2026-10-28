# Strict Embedded & Systems Engineering Rules

## 1. No Speculative Explanations (Root Cause Policy)
- Never invent causes outside visible code (no "SDK quirks", "flash memory ghosts", or hypothetical cache states) unless verified by reproducible hardware facts.
- If a cause is unknown, explicitly state: "Unclear based on current code" and request specific telemetry or define the null hypothesis.

## 2. Dry-Run / Static Execution Trace (Mandatory)
- Before outputting code involving state machines, timers, loops, or network mode transitions:
  - Perform a mental step-by-step trace of the execution flow.
  - Check variable states line-by-line across blocking sections (delays, network handshakes, yield).
  - Explicitly verify unsigned integer math: inspect every `millis() - lastTime` for underflows ($A - B$ where $A < B$).
  - Verify Software and Hardware Watchdog feeds in every blocking loop.

## 3. Zero Unsolicited "Best Practice" Bloat
- Do NOT add automatic timeouts, auto-reboot dead-man switches, EEPROM caching, or extra fallback tiers unless explicitly requested.
- Maintain simple, deterministic states. Keep it minimal and robust.

## 4. Full File Delivery
- Never truncate or omit code sections when providing revised project files. Always deliver complete, compilable files without placeholders.