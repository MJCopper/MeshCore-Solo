# Contributing to MeshCore Zen

Zen extends MeshCore for the Wio Tracker L1. Keep changes focused, modular and
compatible with the upstream MeshCore baseline where practical.

## Issues

For bugs, include the Zen version, OLED or E-ink model, steps to reproduce and
any relevant **Diagnostics › Events** entries. For feature requests, describe
the use case and expected device behaviour.

## Changes

Use a focused branch and keep each pull request to one feature or fix. Preserve
the existing style, update tests and documentation where applicable, and bump
the Zen build number once for the completed change set.

Before submitting:

- Run `pio test -e native`.
- Build the relevant Wio Tracker target.
- Check `git diff --check`.
- Follow [Building Zen](./docs/building_zen.md) for commands and output paths.

## Style

Follow the existing C++ style and `.clang-format`:

- 2 spaces indentation (no tabs)
- `camelCase` for functions and variables
- `UpperCamelCase` for class names
- `#define` constants in `ALL_CAPS`
- Keep lines near 100 characters when practical
