#pragma once

#include <stdint.h>
#include <string.h>

class DisplayDriver {
  int _w, _h;
protected:
  bool _vw_dirty = true;
  bool _vw_result = false;
  DisplayDriver(int w, int h) { _w = w; _h = h; }
  void setDimensions(int w, int h) { _w = w; _h = h; }
public:
  enum Color { DARK=0, LIGHT, RED, GREEN, BLUE, YELLOW, ORANGE }; // on b/w screen, colors will be !=0 synonym of light

  int width() const { return _w; }
  int height() const { return _h; }

  virtual bool isOn() = 0;
  virtual void turnOn() = 0;
  virtual void turnOff() = 0;
  virtual void clear() = 0;
  virtual void startFrame(Color bkg = DARK) = 0;
  virtual void setTextSize(int sz) = 0;
  virtual void setColor(Color c) = 0;
  virtual void setCursor(int x, int y) = 0;
  virtual void print(const char* str) = 0;
  virtual void printWordWrap(const char* str, int max_width) { print(str); }   // fallback to basic print() if no override
  virtual void fillRect(int x, int y, int w, int h) = 0;
  virtual void drawRect(int x, int y, int w, int h) = 0;
  virtual void drawXbm(int x, int y, const uint8_t* bits, int w, int h) = 0;
  virtual uint16_t getTextWidth(const char* str) = 0;
  virtual int getCharWidth() const { return 6; }   // typical character advance width (px)
  virtual int getLineHeight() const { return 8; }  // pixel rows per text line
  virtual void setLemonFont(bool) { }              // no-op; overridden by displays that support Lemon
  virtual bool isLemonFont() const { return false; }
  // Layout helpers — derived from font metrics and screen size.
  // Use these instead of hardcoded pixel values so layouts adapt to any display.
  int lineStep()             const { return getLineHeight() + 2; }         // row pitch: text + gap
  int headerH()              const { return getLineHeight() + 3; }         // title bar height
  int listStart()            const { return headerH(); }                   // y where list items begin
  int listVisible(int itemH) const { return (height() - listStart()) / itemH; }
  int listVisible()          const { return listVisible(lineStep()); }
  // x where a right-side value column starts (leaves ~8 chars for the value)
  int valCol()               const { return width() - getCharWidth() * 8; }
  // true only on landscape e-ink; use instead of comparing pixel counts or getLineHeight()
#ifdef EINK_DISPLAY_MODEL
  bool isLandscape()         const { return width() >= height(); }
#else
  bool isLandscape()         const { return false; }
#endif
  // separator line thickness: 2px on landscape e-ink, 1px everywhere else
  int sepH()                 const { return isLandscape() ? 2 : 1; }
  virtual void drawTextCentered(int mid_x, int y, const char* str) {
    char tmp[256]; translateUTF8ToBlocks(tmp, str, sizeof(tmp));
    int w = getTextWidth(tmp);
    setCursor(mid_x - w/2, y);
    print(tmp);
  }
  virtual void drawTextRightAlign(int x_anch, int y, const char* str) {
    char tmp[256]; translateUTF8ToBlocks(tmp, str, sizeof(tmp));
    int w = getTextWidth(tmp);
    setCursor(x_anch - w, y);
    print(tmp);
  }
  virtual void drawTextLeftAlign(int x_anch, int y, const char* str) {
    char tmp[256]; translateUTF8ToBlocks(tmp, str, sizeof(tmp));
    setCursor(x_anch, y);
    print(tmp);
  }
  
  // Sorted-codepoint → ASCII transliteration table.
  // Binary search beats a ~110-case switch on flash (parallel arrays = 3 bytes/entry,
  // no padding) and gives O(log n) lookup. Covers Latin Extended A/B + common
  // Latin-1 accented diacritics needed by EU languages and Turkish.
  static char transliterateCodepoint(uint32_t cp) {
    // Codepoints must stay sorted ascending — binary search assumes it.
    static const uint16_t CPS[] = {
      0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
      0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
      0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D8,
      0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
      0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
      0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
      0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F8,
      0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE,
      0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107,
      0x010C, 0x010D, 0x010E, 0x010F, 0x0110, 0x0111, 0x0112, 0x0113,
      0x0116, 0x0117, 0x0118, 0x0119, 0x011A, 0x011B, 0x011E, 0x011F,
      0x0122, 0x0123, 0x012A, 0x012B, 0x012E, 0x012F, 0x0131,
      0x0136, 0x0137, 0x0139, 0x013A, 0x013B, 0x013C, 0x013D, 0x013E,
      0x0141, 0x0142, 0x0143, 0x0144, 0x0145, 0x0146, 0x0147, 0x0148,
      0x0150, 0x0151, 0x0154, 0x0155, 0x0156, 0x0157, 0x0158, 0x0159,
      0x015A, 0x015B, 0x015E, 0x015F, 0x0160, 0x0161, 0x0162, 0x0163,
      0x0164, 0x0165, 0x016A, 0x016B, 0x016E, 0x016F, 0x0170, 0x0171,
      0x0172, 0x0173, 0x0179, 0x017A, 0x017B, 0x017C, 0x017D, 0x017E,
      0x0218, 0x0219, 0x021A, 0x021B
    };
    static const char ASCII[] = {
      'A','A','A','A','A','A','A','C',
      'E','E','E','E','I','I','I','I',
      'D','N','O','O','O','O','O','O',
      'U','U','U','U','Y','T','s',
      'a','a','a','a','a','a','a','c',
      'e','e','e','e','i','i','i','i',
      'd','n','o','o','o','o','o','o',
      'u','u','u','u','y','t',
      'A','a','A','a','A','a','C','c',
      'C','c','D','d','D','d','E','e',
      'E','e','E','e','E','e','G','g',
      'G','g','I','i','I','i','i',
      'K','k','L','l','L','l','L','l',
      'L','l','N','n','N','n','N','n',
      'O','o','R','r','R','r','R','r',
      'S','s','S','s','S','s','T','t',
      'T','t','U','u','U','u','U','u',
      'U','u','Z','z','Z','z','Z','z',
      'S','s','T','t'
    };
    static_assert(sizeof(CPS) / sizeof(CPS[0]) == sizeof(ASCII), "transliteration tables out of sync");
    int lo = 0, hi = (int)(sizeof(ASCII)) - 1;
    while (lo <= hi) {
      int mid = (lo + hi) >> 1;
      uint16_t v = CPS[mid];
      if (v == cp) return ASCII[mid];
      if (v < cp) lo = mid + 1; else hi = mid - 1;
    }
    return '\xDB';
  }

  // convert UTF-8 to ASCII, transliterating accented/diacritic characters (callable without a display instance)
  static void translateUTF8Static(char* dest, const char* src, size_t dest_size) {
    size_t j = 0;
    const uint8_t* p = (const uint8_t*)src;
    while (*p && j < dest_size - 1) {
      if (*p < 0x80) {
        uint8_t c = *p++;
        if (c >= 32) dest[j++] = c;
      } else {
        uint32_t cp = decodeCodepoint(p);
        // transliterateCodepoint already returns '\xDB' for unmapped values (incl. 0xFFFD).
        dest[j++] = transliterateCodepoint(cp);
      }
    }
    dest[j] = 0;
  }

  virtual void translateUTF8ToBlocks(char* dest, const char* src, size_t dest_size) {
    translateUTF8Static(dest, src, dest_size);
  }

  // Common selection-row pattern: when sel, fills (x,y,w,h) with ink and sets
  // colour to DARK so the next text render appears inverted; when not sel,
  // just sets ink colour to LIGHT. Replaces the 5-line if/else copy-paste
  // present in every list-style screen.
  void drawSelectionRow(int x, int y, int w, int h, bool sel) {
    setColor(LIGHT);
    if (sel) {
      fillRect(x, y, w, h);
      setColor(DARK);
    }
  }

  // Advance a UTF-8 pointer by one codepoint, returning the decoded value.
  // Invalid sequences return 0xFFFD and consume trailing continuation bytes.
  static uint32_t decodeCodepoint(const uint8_t*& p) {
    uint8_t c = *p++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
      uint32_t cp = c & 0x1F;
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      return cp;
    }
    if ((c & 0xF0) == 0xE0) {
      uint32_t cp = c & 0x0F;
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      return cp;
    }
    if ((c & 0xF8) == 0xF0) {
      uint32_t cp = c & 0x07;
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      if (*p) cp = (cp << 6) | (*p++ & 0x3F);
      return cp;
    }
    while (*p && (*p & 0xC0) == 0x80) p++;
    return 0xFFFD;
  }

  // Width of a single codepoint in pixels. Default: fall back to getTextWidth
  // on a one-codepoint UTF-8 string. Drivers can override for O(1) lookup.
  virtual uint16_t getCodepointWidth(uint32_t cp) {
    char buf[5];
    int n = 0;
    if (cp < 0x80) { buf[n++] = (char)cp; }
    else if (cp < 0x800) { buf[n++] = 0xC0 | (cp >> 6); buf[n++] = 0x80 | (cp & 0x3F); }
    else if (cp < 0x10000) { buf[n++] = 0xE0 | (cp >> 12); buf[n++] = 0x80 | ((cp >> 6) & 0x3F); buf[n++] = 0x80 | (cp & 0x3F); }
    else { buf[n++] = 0xF0 | (cp >> 18); buf[n++] = 0x80 | ((cp >> 12) & 0x3F); buf[n++] = 0x80 | ((cp >> 6) & 0x3F); buf[n++] = 0x80 | (cp & 0x3F); }
    buf[n] = '\0';
    return getTextWidth(buf);
  }


  // draw text with ellipsis if it exceeds max_width
  virtual void drawTextEllipsized(int x, int y, int max_width, const char* str) {
    char temp_str[256];  // reasonable buffer size
    translateUTF8ToBlocks(temp_str, str, sizeof(temp_str));
    
    if (getTextWidth(temp_str) <= max_width) {
      setCursor(x, y);
      print(temp_str);
      return;
    }
    
    // for variable-width fonts (GxEPD), add space after ellipsis
    // for fixed-width fonts (OLED), keep tight spacing to save precious characters
    const char* ellipsis;
    // use a simple heuristic: if 'i' and 'l' have different widths, it's variable-width
    if (_vw_dirty) {
      _vw_result = (getTextWidth("i") != getTextWidth("l"));
      _vw_dirty = false;
    }
    if (_vw_result) {
      ellipsis = "... ";  // variable-width fonts: add space
    } else {
      ellipsis = "...";   // fixed-width fonts: no space
    }
    
    int ellipsis_width = getTextWidth(ellipsis);
    int str_len = strlen(temp_str);
    
    while (str_len > 0 && getTextWidth(temp_str) > max_width - ellipsis_width) {
      temp_str[--str_len] = 0;
    }
    // Strip orphaned UTF-8 leading byte left by byte-at-a-time trimming above.
    while (str_len > 0 && ((uint8_t)temp_str[str_len - 1] & 0xC0) == 0xC0) {
      temp_str[--str_len] = 0;
    }
    strcat(temp_str, ellipsis);
    
    setCursor(x, y);
    print(temp_str);
  }
  
  virtual void setBrightness(uint8_t level) { }  // level 0-4 (min to max), no-op default
  virtual void setDisplayRotation(uint8_t rot) { }  // 0-3, no-op for fixed-orientation displays
  virtual void setFullRefreshInterval(uint8_t n) { }  // e-ink: do full refresh every n partial refreshes (0=never)
  virtual void endFrame() = 0;

#ifdef ENABLE_SCREENSHOT
  // Screenshot support — virtual so we never reinterpret_cast the concrete
  // display type (RTTI is disabled). Drivers that can dump their framebuffer
  // override these; default returns nullptr/0 so the caller fails cleanly.
  virtual const uint8_t* getBuffer()     { return nullptr; }
  virtual uint16_t       getBufferSize() { return 0; }
#endif
};
