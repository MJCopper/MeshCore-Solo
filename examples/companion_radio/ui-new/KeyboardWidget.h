#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <Arduino.h>
#include "PopupMenu.h"
#include "icons.h"   // mini-icons for the special-key row (⇧ ⌫ ⎵ ✓)
#include "../NodePrefs.h"

// Layout constants shared by all keyboard users.
// Two pages: letters (page 0) and symbols (page 1), toggled by the "#@"/"abc"
// special key. Space lives only on the ⎵ special key now, so the freed grid
// slot on page 0 holds the comma; punctuation is grouped as . , ! ?
static const int KB_PAGES      = 2;
static const char KB_CHARS[KB_PAGES][4][10] = {
  { // page 0 — letters + digits
    {'a','b','c','d','e','f','g','h','i','j'},
    {'k','l','m','n','o','p','q','r','s','t'},
    {'u','v','w','x','y','z','.',',','!','?'},
    {'1','2','3','4','5','6','7','8','9','0'},
  },
  { // page 1 — symbols + digits (ASCII only — one byte per key)
    {'@','#','&','*','(',')','-','_','+','='},
    {'/','\\',':',';','\'','"','<','>','[',']'},
    {'{','}','|','~','^','$','%','`',',','.'},
    {'1','2','3','4','5','6','7','8','9','0'},
  },
};
static const int KB_ROWS_CHAR  = 4;
static const int KB_COLS_CHAR  = 10;
static const int KB_SPECIAL    = 6;   // ⇧ ⎵ ⌫ {} #@/abc ✓

// T9 multi-tap layout (Settings › Keyboard). A classic phone keypad: 9 cells (keys
// 1-9) laid out 3x3, each holding a handful of letters/symbols. Repeated Enter
// presses on the same cell within KB_T9_TIMEOUT_MS cycle through the group, ending
// on the cell's own digit (computed as '1'+cell, not stored here) before wrapping.
// Keys 0/*/# aren't part of the grid — space/backspace/etc. already live on the
// special row below, shared with the ABC layout.
static const int KB_T9_ROWS = 3;
static const int KB_T9_COLS = 3;
static const uint32_t KB_T9_TIMEOUT_MS = 800;
static const char* const KB_T9_GROUPS[KB_PAGES][9] = {
  { ".,!?'-", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz" },   // page 0 — letters
  { "@#&", "*()", "-_+", "=/\\", ":;'\"", "<>[]", "{}|~", "^$%`", ",." },  // page 1 — symbols
};

// Additional (non-Latin) keyboard alphabets — NodePrefs::keyboard_alt_alphabet
// picks which one (if any) joins the Latin/symbols page cycle. Every alphabet
// here must fall inside Lemon's U+0020-04FF range (src/helpers/ui/LemonFont.h)
// since that's what actually draws these glyphs on-screen.
//
// Unlike KB_CHARS (single ASCII byte per cell), these hold UTF-8 strings —
// Cyrillic is 2 bytes/codepoint — so cells are `const char*`, not `char`.
// KeyboardWidget's insertion/backspace/T9-cycle logic works in codepoints via
// the kbUtf8*() helpers below, not raw bytes, to stay correct for either.

// Cyrillic ABC grid: rows 0-2 hold the 30 letters that fit alphabetically
// (а-э); row 3 holds the remaining 3 (ё, ю, я) plus basic punctuation. No
// digit row on this page (digits are already reachable via the Symbols page,
// same as the Latin page's own row 3 duplicates them there).
static const char* const KB_CYRILLIC_CHARS[4][10] = {
  { "а","б","в","г","д","е","ж","з","и","й" },
  { "к","л","м","н","о","п","р","с","т","у" },
  { "ф","х","ц","ч","ш","щ","ъ","ы","ь","э" },
  { "ю","я","ё",".",",","!","?","-","'","\"" },
};
// Cyrillic T9 groups: the classic Russian phone-keypad distribution. Cell 0
// (digit '1') is punctuation, matching KB_T9_GROUPS' page-0 convention;
// cells 1-8 (digits '2'-'9') hold the 33 letters, 3-5 per key.
static const char* const KB_T9_GROUPS_CYRILLIC[9] = {
  ".,!?'-", "абвг", "деёжз", "ийкл", "мноп", "рсту", "фхцч", "шщъыь", "эюя"
};

// Greek ABC grid: the 24-letter modern alphabet plus final sigma (ς, used
// only at the end of a word — σ is the regular form) = 25 letters, fitting
// rows 0-2 with 5 basic punctuation marks to spare; row 3 keeps the digit row
// (unlike Cyrillic/Ext.Latin, Greek has room left over).
// NOTE: monotonic Modern Greek normally marks stress with a tonos accent
// (ά έ ή ί ό ύ ώ) — omitted here to keep this a single, simple page. Fine for
// informal/transliteration-style typing; flag if proper accented Greek
// composition turns out to matter and we can add a second Greek page for them.
static const char* const KB_GREEK_CHARS[4][10] = {
  { "α","β","γ","δ","ε","ζ","η","θ","ι","κ" },
  { "λ","μ","ν","ξ","ο","π","ρ","σ","ς","τ" },
  { "υ","φ","χ","ψ","ω",".",",","!","?","'" },
  { "1","2","3","4","5","6","7","8","9","0" },
};
// Greek T9 groups: cell 0 (digit '1') is punctuation; cells 1-8 (digits
// '2'-'9') split the 25 letters roughly evenly, final sigma grouped with the
// regular sigma it's a variant of.
static const char* const KB_T9_GROUPS_GREEK[9] = {
  ".,!?'-", "αβγ", "δεζ", "ηθι", "κλμ", "νξο", "πρσς", "τυφ", "χψω"
};

// Extended Latin ABC grid: diacritics for Polish, Czech/Slovak, German,
// French, Spanish/Portuguese and Nordic — the common non-ASCII Latin letters,
// not full Unicode coverage. 34 letters + 6 basic punctuation marks fill all
// 40 cells; no digit row on this page (digits are reachable via the Symbols
// page, same as Cyrillic).
static const char* const KB_EXTLATIN_CHARS[4][10] = {
  { "ą","ć","ę","ł","ń","ó","ś","ź","ż","ö" },   // Polish (9) + German ö
  { "ü","ä","ß","č","ď","ě","ň","ř","š","ť" },   // German (3) + Czech/Slovak (7)
  { "ů","ž","é","è","ê","ë","à","ç","ù","ñ" },   // Czech/Slovak (2) + French (6) + Spanish ñ
  { "å","ø","æ","õ",".",",","!","?","-","'" },   // Nordic (3) + Portuguese õ + punctuation
};
// Extended Latin T9 groups: cell 0 (digit '1') is punctuation; cells 1-8
// (digits '2'-'9') cluster by language for memorability.
static const char* const KB_T9_GROUPS_EXTLATIN[9] = {
  ".,!?'-", "ąćęł", "ńóśźż", "äöüß", "čďěň", "řšťůž", "éèêë", "àçùñ", "åøæõ"
};
// Buffer cap for typed text, in bytes. Matches MeshCore's MAX_TEXT_LEN
// (10*CIPHER_BLOCK_SIZE = 160) so a full-length message can be composed; each
// field passes its own smaller max to begin() where its store is smaller.
static const int KB_MAX_LEN    = 160;

// Longest preview line we render per row. Caps the per-line stack buffers so a
// very wide display (small font → many chars per line) can't overrun them.
static const int KB_PREVIEW_CAP = 46;

// ── UTF-8 helpers for the alt-alphabet pages ─────────────────────────────────
// KB_CHARS/KB_T9_GROUPS content is ASCII (1 byte/char); KB_CYRILLIC_CHARS and
// KB_T9_GROUPS_CYRILLIC are UTF-8 (2 bytes/char in this range). These helpers
// let insertion, backspace and T9 cycling work in codepoints for either,
// instead of assuming 1 byte == 1 character.

// Apply Shift/caps to every codepoint in a UTF-8 string, writing the result
// (same codepoint count, each independently shifted) into `out`. Every script
// here pairs lower/uppercase differently, so each gets its own rule:
//  - ASCII a-z and Cyrillic а-я: flat -0x20 codepoint offset. ё/Ё (U+0451/
//    U+0401) break the Cyrillic pattern by 0x50 and are special-cased.
//  - Latin-1 Supplement à-þ (U+00E0-00FE, Ext.Latin's ö/ü/é/etc.): also a flat
//    -0x20 offset, same as ASCII — the block was designed as parallel case
//    pairs. U+00F7 (÷, division sign) sits in that numeric range but isn't a
//    letter; excluded. ß (U+00DF) has no simple uppercase in this range
//    (its uppercase ẞ is U+1E9E, outside Lemon's U+0020-04FF) — left as-is.
//  - Latin Extended-A (Ext.Latin's ą/č/etc., U+0100-017F): NOT a flat offset
//    like Latin-1 — this block alternates even=uppercase/odd=lowercase in
//    adjacent pairs, so lowercase - 1 = uppercase. Verified true for every
//    character actually in KB_EXTLATIN_CHARS/KB_T9_GROUPS_EXTLATIN; NOT a
//    universal rule for the whole block (it has a handful of unpaired/
//    irregular codepoints elsewhere) — recheck before adding more from it.
//  - Greek α-ω (U+03B1-03C9): flat -0x20 offset, same shape as Cyrillic/ASCII.
//    Final sigma ς (U+03C2) is the one exception — it has no uppercase of its
//    own; -0x20 would land on U+03A2, which is unassigned. It capitalizes to
//    regular Σ (U+03A3) instead, same as σ, and is special-cased before the
//    general range rule (03C2 falls inside 03B1-03C9, so order matters here).
// Used both for a single cell (one codepoint) and a whole T9 group label/string.
static void kbApplyCapsUtf8(const char* in, bool caps, char* out, size_t out_size) {
  size_t o = 0;
  const uint8_t* p = (const uint8_t*)in;
  while (*p && o + 2 < out_size) {
    uint32_t cp = DisplayDriver::decodeCodepoint(p);
    if (caps) {
      if (cp == 0x0451)                                    cp = 0x0401;  // ё -> Ё
      else if (cp == 0x03C2)                               cp = 0x03A3;  // ς -> Σ
      else if (cp >= 0x0430 && cp <= 0x044F)                cp -= 0x20;   // а-я -> А-Я
      else if (cp >= 0x03B1 && cp <= 0x03C9)                cp -= 0x20;   // α-ω -> Α-Ω
      else if (cp >= 0x00E0 && cp <= 0x00FE && cp != 0x00F7) cp -= 0x20;  // à-þ -> À-Þ
      else if (cp >= 0x0100 && cp < 0x0180 && (cp & 1) == 1) cp -= 1;     // ą-ż (odd=lower) -> Ą-Ż
      else if (cp >= 'a' && cp <= 'z')                      cp -= 0x20;   // a-z -> A-Z
    }
    if (cp < 0x80) {
      out[o++] = (char)cp;
    } else {
      out[o++] = (char)(0xC0 | (cp >> 6));
      out[o++] = (char)(0x80 | (cp & 0x3F));
    }
  }
  out[o] = '\0';
}

// Codepoint count of a UTF-8 string (byte length overcounts once a 2-byte
// alphabet is involved — this is what T9 cycling needs instead of strlen()).
static int kbUtf8Len(const char* s) {
  int n = 0;
  const uint8_t* p = (const uint8_t*)s;
  while (*p) { DisplayDriver::decodeCodepoint(p); n++; }
  return n;
}

// Extract the idx-th codepoint of a UTF-8 string as its own NUL-terminated
// UTF-8 bytes in `out` (>= 5 bytes). Empty string if idx is out of range.
static void kbUtf8CharAt(const char* s, int idx, char* out) {
  const uint8_t* p = (const uint8_t*)s;
  for (int i = 0; *p; i++) {
    const uint8_t* start = p;
    DisplayDriver::decodeCodepoint(p);
    if (i == idx) {
      int n = (int)(p - start);
      memcpy(out, start, n);
      out[n] = '\0';
      return;
    }
  }
  out[0] = '\0';
}

// Byte width of the LAST codepoint in buf[0..len) — for backspace and the T9
// in-place replace, which must remove/overwrite a whole codepoint, not one
// byte (a lone trailing continuation byte would otherwise corrupt a 2-byte
// character). Capped at 4 (UTF-8's max), though this codebase's alphabets are
// all <= 2 bytes today.
static int kbUtf8LastCharBytes(const char* buf, int len) {
  if (len <= 0) return 0;
  int n = 1;
  while (n < len && n < 4 && ((uint8_t)buf[len - n] & 0xC0) == 0x80) n++;
  return n;
}

static const int KB_PH_MAX     = 12;  // max placeholders in list
static const int KB_PH_LEN     = 9;   // max placeholder string length incl. null
static const int KB_PH_VISIBLE = 3;   // items shown at once in overlay

struct KeyboardWidget {
  char buf[KB_MAX_LEN + 1];
  int  len;
  int  max_len;
  int  row, col;
  int  page;        // see totalPages()/pageIsAltAlphabet()/pageIsSymbols() below
  bool caps;
  char _ph_buf[KB_PH_MAX][KB_PH_LEN];
  int  _ph_count;
  PopupMenu _ph_menu;

  // Live setting lookup — set once by UITask::begin(). NULL only in tests/tools
  // that construct a KeyboardWidget standalone, in which case isT9() defaults
  // to ABC and hasAltAlphabet() defaults to Latin-only.
  NodePrefs* prefs = nullptr;
  bool isT9() const { return prefs && prefs->keyboard_type == 1; }
  bool hasAltAlphabet() const {
    return prefs && prefs->keyboard_alt_alphabet != NodePrefs::KB_ALPHABET_LATIN_ONLY;
  }

  // T9 multi-tap state: which grid cell is mid-cycle (-1 = none), its cycle
  // position, and when the last Enter landed on it (for the timeout).
  int      t9_cell = -1;
  int      t9_cycle = 0;
  uint32_t t9_last_ms = 0;

  int gridRows() const { return isT9() ? KB_T9_ROWS : KB_ROWS_CHAR; }
  int gridCols() const { return isT9() ? KB_T9_COLS : KB_COLS_CHAR; }

  // ── Page model ────────────────────────────────────────────────────────────
  // Logical page order: 0 = Latin letters, [1 = the enabled alt alphabet],
  // last = symbols. Without an alt alphabet this is exactly the original
  // 2-page Latin/symbols cycle; enabling one inserts its page in the middle,
  // so the #@/abc key's existing cycle (case 4 below) reaches it for free.
  int totalPages() const { return hasAltAlphabet() ? 3 : 2; }
  bool pageIsAltAlphabet(int pg) const { return hasAltAlphabet() && pg == 1; }
  bool pageIsSymbols(int pg) const { return pg == totalPages() - 1; }

  // ABC-grid cell content as a NUL-terminated UTF-8 string (1 codepoint).
  // Latin/symbols cells come back through a small scratch buffer since
  // KB_CHARS stores single ASCII bytes, not strings; alt-alphabet cells are
  // literal string-table entries, returned directly.
  const char* cellStr(int r, int c) const {
    if (pageIsAltAlphabet(page)) {
      switch (prefs->keyboard_alt_alphabet) {
        case NodePrefs::KB_ALPHABET_CYRILLIC:  return KB_CYRILLIC_CHARS[r][c];
        case NodePrefs::KB_ALPHABET_GREEK:     return KB_GREEK_CHARS[r][c];
        case NodePrefs::KB_ALPHABET_EXT_LATIN: return KB_EXTLATIN_CHARS[r][c];
        default: return "?";
      }
    }
    static char single[2];
    single[0] = KB_CHARS[pageIsSymbols(page) ? 1 : 0][r][c];
    single[1] = '\0';
    return single;
  }

  // T9 group string (UTF-8) for the given cell (0-8), page-aware like cellStr.
  const char* t9GroupStr(int cell) const {
    if (pageIsAltAlphabet(page)) {
      switch (prefs->keyboard_alt_alphabet) {
        case NodePrefs::KB_ALPHABET_CYRILLIC:  return KB_T9_GROUPS_CYRILLIC[cell];
        case NodePrefs::KB_ALPHABET_GREEK:     return KB_T9_GROUPS_GREEK[cell];
        case NodePrefs::KB_ALPHABET_EXT_LATIN: return KB_T9_GROUPS_EXTLATIN[cell];
        default: return "?";
      }
    }
    return KB_T9_GROUPS[pageIsSymbols(page) ? 1 : 0][cell];
  }

  // Compact ASCII hint for the #@/abc key when it's about to switch to the
  // alt-alphabet page. Deliberately ASCII (not the alphabet's own script) so
  // it renders correctly even when Lemon isn't the user's current font choice
  // — unlike the alphabet's own grid, this hint is visible from the Latin/
  // symbols pages too, where render() doesn't force Lemon on (see render()).
  static const char* altAlphabetHint(uint8_t alt) {
    switch (alt) {
      case NodePrefs::KB_ALPHABET_CYRILLIC:  return "CY";
      case NodePrefs::KB_ALPHABET_GREEK:     return "GR";
      case NodePrefs::KB_ALPHABET_EXT_LATIN: return "EL";
      default: return "?";
    }
  }

  enum Result { NONE, DONE, CANCELLED };

  void begin(const char* initial = "", int max = KB_MAX_LEN) {
    max_len = (max > KB_MAX_LEN) ? KB_MAX_LEN : max;
    strncpy(buf, initial, max_len);
    buf[max_len] = '\0';
    len = strlen(buf);
    row = col = 0;
    page = 0;
    caps = false;
    t9_cell = -1;
    t9_cycle = 0;
    _ph_menu.active = false;
    // default placeholders — always available
    _ph_count = 0;
    addPlaceholder("{loc}");
    addPlaceholder("{time}");
  }

  void clearPlaceholders() { _ph_count = 0; }

  void addPlaceholder(const char* ph) {
    if (_ph_count < KB_PH_MAX) {
      strncpy(_ph_buf[_ph_count], ph, KB_PH_LEN - 1);
      _ph_buf[_ph_count][KB_PH_LEN - 1] = '\0';
      _ph_count++;
    }
  }

  int render(DisplayDriver& display) {
    // A stale mid-cycle T9 press (no further input since) finalizes on its own —
    // the character is already committed to buf, this just stops a later Enter
    // on the same cell from being treated as a continued cycle.
    if (t9_cell >= 0 && millis() - t9_last_ms > KB_T9_TIMEOUT_MS) t9_cell = -1;

    // Lemon is the only font that can draw the alt-alphabet page (or any
    // already-typed non-ASCII bytes sitting in buf, e.g. Cyrillic composed
    // earlier, now viewed from the Latin/symbols page) — force it on for just
    // this render() call if the user's own font choice wouldn't otherwise show
    // it, then restore exactly the state found on entry. Self-contained: never
    // leaks a forced font into whatever renders next frame.
    bool want_lemon = pageIsAltAlphabet(page);
    if (!want_lemon) for (int i = 0; i < len; i++) if ((uint8_t)buf[i] >= 0x80) { want_lemon = true; break; }
    bool had_lemon = display.isLemonFont();
    if (want_lemon && !had_lemon) display.setLemonFont(true);

    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);

    const int rows = gridRows();
    const int cols = gridCols();
    const int lh      = display.getLineHeight();
    const int cw      = display.getCharWidth();
    const int cell_w  = display.width() / cols;
    // compact: don't stretch cells beyond lh; freed vertical space goes to preview lines
    const int kb_h      = (rows + 1) * lh;
    const int preview_h = display.height() - kb_h - display.sepH();
    const int prev_lines = (preview_h / lh) > 1 ? (preview_h / lh) : 1;
    const int sep_y   = prev_lines * lh;
    const int chars_y = sep_y + display.sepH();
    const int cell_h  = (display.height() - chars_y) / (rows + 1);
    const int spec_y  = chars_y + rows * cell_h;
    const int spec_w  = display.width() / KB_SPECIAL;

    // multi-line text preview: cursor always on last preview line
    int cpl = display.width() / cw;  // chars per preview line
    if (cpl < 1) cpl = 1;
    if (cpl > KB_PREVIEW_CAP) cpl = KB_PREVIEW_CAP;  // never overrun linebuf below
    int cursor_line = len / cpl;
    int first_line  = (cursor_line >= prev_lines) ? (cursor_line - prev_lines + 1) : 0;
    int start = first_line * cpl;
    for (int pl = 0; pl < prev_lines; pl++) {
      int ps = start + pl * cpl;
      int pe = ps + cpl;
      bool cursor_here = (ps <= len && (len < pe || pl == prev_lines - 1));
      char linebuf[KB_PREVIEW_CAP + 2];   // cpl chars + cursor '_' + NUL
      if (cursor_here) {
        int nc = len - ps; if (nc < 0) nc = 0; if (nc > cpl - 1) nc = cpl - 1;
        snprintf(linebuf, sizeof(linebuf), "%.*s_", nc, buf + ps);
      } else if (len > ps) {
        int nc = (len < pe) ? (len - ps) : cpl;
        snprintf(linebuf, sizeof(linebuf), "%.*s", nc, buf + ps);
      } else {
        linebuf[0] = '\0';
      }
      char linebuf_t[KB_PREVIEW_CAP + 2];
      display.translateUTF8ToBlocks(linebuf_t, linebuf, sizeof(linebuf_t));
      display.setCursor(0, pl * lh);
      display.print(linebuf_t);
    }
    display.fillRect(0, sep_y, display.width(), display.sepH());

    // character grid
    if (isT9()) {
      for (int r = 0; r < rows; r++) {
        int y = chars_y + r * cell_h;
        for (int c = 0; c < cols; c++) {
          bool sel = (row == r && col == c);
          int cell = r * cols + c;
          // Label the cell "<digit><group>" so it reads like a phone keypad. The
          // digit is what the multi-tap cycle lands on after the letters (see
          // handleInput: '1'+cell). No separator space — the widest group
          // (Cyrillic "деёжз"/"шщъыь", 5 letters x up to 2 UTF-8 bytes) + digit
          // still fits with room to spare.
          char group_shown[12];
          kbApplyCapsUtf8(t9GroupStr(cell), caps, group_shown, sizeof(group_shown));
          char label[14];
          snprintf(label, sizeof(label), "%c%s", (char)('1' + cell), group_shown);
          int cx = c * cell_w;
          display.drawSelectionRow(cx, y - 1, cell_w - 1, cell_h, sel);
          int tw = display.getTextWidth(label);
          display.setCursor(cx + (cell_w - tw) / 2, y);
          display.print(label);
        }
      }
    } else {
      for (int r = 0; r < rows; r++) {
        int y = chars_y + r * cell_h;
        for (int c = 0; c < cols; c++) {
          bool sel = (row == r && col == c);
          char ch_buf[3];
          kbApplyCapsUtf8(cellStr(r, c), caps, ch_buf, sizeof(ch_buf));
          if (ch_buf[0] == ' ' && ch_buf[1] == '\0') ch_buf[0] = '_';
          int cx = c * cell_w;
          display.drawSelectionRow(cx, y - 1, cell_w - 1, cell_h, sel);
          int tw = display.getTextWidth(ch_buf);
          display.setCursor(cx + (cell_w - tw) / 2, y);
          display.print(ch_buf);
        }
      }
    }

    // special row: caps ⇧ · space ⎵ · delete ⌫ · placeholders {} (text) · OK ✓
    const int s   = miniIconScale(display);
    const int icy = spec_y + (cell_h - lh) / 2;   // centre icons within the cell
    for (int i = 0; i < KB_SPECIAL; i++) {
      bool sel    = (row == rows && col == i);
      bool active = (i == 0 && caps);
      int sx = i * spec_w;
      display.drawSelectionRow(sx, spec_y - 1, spec_w - 1, cell_h, sel || active);
      if (i == 3 || i == 4) {               // text keys: {} picker, page toggle
        // Shows what pressing it lands on next, same "reads as the
        // destination" convention as the original 2-page abc<->#@ toggle,
        // generalized to however many pages are in the cycle right now.
        const char* lbl;
        if (i == 3) {
          lbl = "{}";
        } else {
          int next = (page + 1) % totalPages();
          lbl = pageIsSymbols(next)     ? "#@"
              : pageIsAltAlphabet(next) ? altAlphabetHint(prefs->keyboard_alt_alphabet)
                                        : "abc";
        }
        int tw = display.getTextWidth(lbl);
        display.setCursor(sx + (spec_w - tw) / 2, spec_y);
        display.print(lbl);
      } else if (i == 1) {                  // space ⎵ — two halves side by side
        int icw = (ICON_SPACE_L.w + ICON_SPACE_R.w) * s;
        int ix  = sx + (spec_w - icw) / 2;
        miniIconDraw(display, ix, icy, ICON_SPACE_L);
        miniIconDraw(display, ix + ICON_SPACE_L.w * s, icy, ICON_SPACE_R);
      } else {
        const MiniIcon& ic = (i == 0) ? ICON_SHIFT
                           : (i == 2) ? ICON_BACKSPACE
                                      : ICON_CHECK;   // i == 5 → OK
        int ix = sx + (spec_w - ic.w * s) / 2;
        miniIconDraw(display, ix, icy, ic);
      }
      display.setColor(DisplayDriver::LIGHT);
    }

    // placeholder picker overlay (drawn on top of keyboard)
    if (_ph_menu.active) _ph_menu.render(display);

    if (want_lemon && !had_lemon) display.setLemonFont(false);   // restore exactly what we found
    return 50;
  }

  Result handleInput(char c) {
    // placeholder overlay consumes all input
    if (_ph_menu.active) {
      auto res = _ph_menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int idx = _ph_menu.selectedIndex();
        const char* ph = _ph_buf[idx];
        int ph_len = strlen(ph);
        if (len + ph_len <= max_len) {
          memcpy(buf + len, ph, ph_len);
          len += ph_len;
          buf[len] = '\0';
        }
      }
      return NONE;
    }

    if (c == KEY_CANCEL || c == KEY_CONTEXT_MENU) return CANCELLED;

    const int rows = gridRows();
    const int cols = gridCols();

    if (c == KEY_UP) {
      if (row > 0) {
        row--;
        if (row == rows - 1)  // leaving special row upward
          col = col * cols / KB_SPECIAL;
      } else {
        row = rows;             // wrap up onto the special row
        col = col * KB_SPECIAL / cols;
      }
      t9_cell = -1;   // navigating away finalizes any pending multi-tap cycle
      return NONE;
    }
    if (c == KEY_DOWN) {
      if (row < rows) {
        row++;
        if (row == rows)  // entering special row
          col = col * KB_SPECIAL / cols;
      } else {
        row = 0;                        // wrap down onto the first char row
        col = col * cols / KB_SPECIAL;
      }
      t9_cell = -1;
      return NONE;
    }
    if (c == KEY_LEFT) {
      int max_col = (row == rows) ? KB_SPECIAL - 1 : cols - 1;
      col = (col > 0) ? col - 1 : max_col;
      t9_cell = -1;
      return NONE;
    }
    if (c == KEY_RIGHT) {
      int max_col = (row == rows) ? KB_SPECIAL - 1 : cols - 1;
      col = (col < max_col) ? col + 1 : 0;
      t9_cell = -1;
      return NONE;
    }
    if (c == KEY_ENTER) {
      if (row < rows && isT9()) {
        int cell = row * cols + col;
        const char* group = t9GroupStr(cell);
        int glen = kbUtf8Len(group);       // codepoint count, not byte length
        int total = glen + 1;   // + the cell's own digit, at the end of the cycle
        bool cycling = (t9_cell == cell) && (millis() - t9_last_ms < KB_T9_TIMEOUT_MS);
        if (cycling) {
          t9_cycle = (t9_cycle + 1) % total;
          if (len > 0) {
            char one[5];
            if (t9_cycle < glen) kbUtf8CharAt(group, t9_cycle, one);
            else { one[0] = (char)('1' + cell); one[1] = '\0'; }
            char shown[5];
            kbApplyCapsUtf8(one, caps, shown, sizeof(shown));
            int old_n = kbUtf8LastCharBytes(buf, len);
            int new_len = len - old_n;
            int n = (int)strlen(shown);
            if (new_len + n <= max_len) {
              memcpy(buf + new_len, shown, n);
              len = new_len + n;
              buf[len] = '\0';
            }
          }
        } else if (len < max_len) {
          char one[5]; kbUtf8CharAt(group, 0, one);
          char shown[5]; kbApplyCapsUtf8(one, caps, shown, sizeof(shown));
          int n = (int)strlen(shown);
          if (len + n <= max_len) {
            memcpy(buf + len, shown, n);
            len += n;
            buf[len] = '\0';
            t9_cell = cell;
            t9_cycle = 0;
          }
        }
        t9_last_ms = millis();
      } else if (row < rows) {
        char shown[3];
        kbApplyCapsUtf8(cellStr(row, col), caps, shown, sizeof(shown));
        int n = (int)strlen(shown);
        if (len + n <= max_len) {
          memcpy(buf + len, shown, n);
          len += n;
          buf[len] = '\0';
        }
      } else {
        t9_cell = -1;   // any special-row action finalizes a pending multi-tap cycle
        switch (col) {
          case 0: caps = !caps; break;
          case 1:
            if (len < max_len) { buf[len++] = ' '; buf[len] = '\0'; }
            break;
          case 2:
            if (len > 0) { len -= kbUtf8LastCharBytes(buf, len); buf[len] = '\0'; }
            break;
          case 3:
            _ph_menu.begin("Placeholder:", KB_PH_VISIBLE);
            for (int i = 0; i < _ph_count; i++) _ph_menu.addItem(_ph_buf[i]);
            break;
          case 4:
            page = (page + 1) % totalPages();   // cycle letters -> [alt alphabet] -> symbols
            break;
          case 5:
            return DONE;
        }
      }
    }
    return NONE;
  }
};
