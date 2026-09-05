# Zen UI developer guide

[Back to README](../../README.md)

Zen UI screens live in `examples/companion_radio/ui-new/` and are included into
`UITask.cpp` as one translation unit. Include order matters; cross-translation
unit helpers belong in normal headers.

## Screen contract

Every screen implements `UIScreen`:

```cpp
int render(DisplayDriver& display);
bool handleInput(char key);
void poll();
void onShow();
```

`render()` draws one frame and returns the delay before another render.
`handleInput()` consumes keys, `poll()` performs bounded background work, and
`onShow()` resets per-visit state. `UITask` owns frame start/end.

To add a screen: declare its pointer in `UITask.h`, construct it in `begin()`,
add a navigator, then add its menu or carousel entry.

## Layout and lists

Use `DisplayDriver` metrics rather than fixed pixel values:

- `lineStep()`, `headerH()` and `listStart()` for vertical layout;
- `getTextWidth()` and `drawTextEllipsized()` for user text;
- `drawCenteredHeader()` or `drawInvertedHeader()` for headers;
- `drawList()` for selection, scrolling and scrollbar reservation.

Use `MessageTranscriptView` for DM, room and channel history. Shared components
also include `PopupMenu`, `KeyboardWidget`, `DigitEditor`, `FullscreenMsgView`
and `AccordionList`.

## Input

Handle `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_ENTER`,
`KEY_CANCEL` and `KEY_CONTEXT_MENU`. Use `keyIsPrev()` and `keyIsNext()` for
editable values so rotary and directional inputs agree.

Back is the only physical screen-wake key. E-ink builds capture button edges
during panel refresh and replay queued input before one redraw.

## Text and emoji

`KeyboardWidget` provides UTF-8-safe ABC, predictive T9, completion,
placeholders and the message emoji picker. Use `expandMsg()` at send time and
`kbAddSensorPlaceholders()` for available sensor tokens. Do not implement a
second text editor or wrapper.

Display text accepts UTF-8 directly. Unsupported emoji use the shared diamond
fallback; glyph data and overrides are centralised under `src/helpers/ui/`.

## Persistence and policy

MeshCore settings remain in the upstream persistence path. Zen preferences use
the versioned sidecar in `solo/SoloPrefsCodec.h`; its internal name is retained
for stored-data compatibility.

Multi-field screens stage values and call `savePrefsIfDirty()` on exit. Mark a
screen dirty only after a real value change. Immediate one-shot actions may save
directly when their contract requires it.

Use the shared Child Mode, Quiet Time, notification, room-login, advert privacy,
GPS scheduling and message-delivery helpers. UI screens should present policy,
not duplicate it.

## Power

Return long render intervals for static screens. Keep background `poll()` work
bounded, suspend peripheral polling while the display sleeps, and avoid flash
writes when values have not changed. E-ink code should coalesce input and
redraws wherever possible.
