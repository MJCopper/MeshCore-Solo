# Trail (Tools › Trail) — analiza i propozycja uporządkowania

> Branch: `refactor/trail-screen` (zmergowany do `main`)
> Plik źródłowy: [TrailScreen.h](../../examples/companion_radio/ui-new/TrailScreen.h)
> Status: **zaimplementowane** — dokument zachowany jako zapis analizy/decyzji.
> Aktualny opis funkcji od strony użytkownika: [tools_screen.md › GPS Trail](../solo_features/tools_screen/tools_screen.md).

---

## 1. Jak ekran jest zbudowany dzisiaj

```
TrailScreen
├── 3 widoki (LEFT/RIGHT):  Summary · Map · List
├── popup akcji  (Hold Enter)  — JEDNA płaska lista, do 12 pozycji
│   ├── ustawienia (LEFT/RIGHT cykluje w miejscu): Min dist · Readout · Grid
│   ├── toggle:    Start/Stop tracking
│   ├── waypointy: Mark here · Waypoints · Clear waypoints
│   └── trail:     Save · Load · Export(live) · Export(saved) · Reset
├── pod-ekrany waypointów (nakładane na widoki):
│   ├── WP_LIST  (lista + dystanse; Trail-start + „+ Add by coords")
│   ├── WP_NAV   (navview)
│   ├── WP_ADD   (formularz lat/lon/label)
│   └── _wp_ctx popup: Rename · Delete · Send
└── KeyboardWidget (label / lat / lon)  — nakładka pełnoekranowa
```

Renderowanie mapy: `renderMap()` (≈140 linii) + `renderGrid()` (≈130 linii)
+ 7 funkcji rysujących markery.

---

## 2. Co jest nieuporządkowane

### 2.1 Popup akcji to jedna płaska lista 12 pozycji z mieszanymi rolami

`openActionMenu()` (`TrailScreen.h:363`) buduje jedno menu, które miesza
**cztery różne klasy** pozycji:

| Klasa            | Pozycje                                   | Interakcja            |
| ---------------- | ----------------------------------------- | --------------------- |
| Ustawienia       | Min dist, Readout, Grid                   | LEFT/RIGHT (w miejscu) |
| Stan nagrywania  | Start/Stop tracking                       | Enter                 |
| Waypointy        | Mark here, Waypoints, Clear waypoints     | Enter                 |
| Plik trasy       | Save, Load, Export(live), Export(saved), Reset | Enter            |

Problemy:
- **Długi scroll** — na OLED widać ~4 wiersze naraz, więc do „Reset trail"
  trzeba przewinąć przez całą listę;
- **Dwa wzorce interakcji w jednym menu** — część pozycji reaguje na
  LEFT/RIGHT (ustawienia), część na Enter (akcje). Enter na wierszu
  ustawień nic sensownego nie robi — tylko zamyka i otwiera menu od nowa
  (`reopenAt`, `TrailScreen.h:581`);
- **Brak kontekstu widoku** — `Grid` (dotyczy tylko mapy) i `Readout`
  (dotyczy tylko Summary) są widoczne zawsze, też tam, gdzie nie mają
  efektu;
- **`Grid` ma dwie ścieżki** — i LEFT/RIGHT (`:221`) i Enter (`:234`)
  robią to samo; lekko myli.

### 2.2 `renderGrid` dostaje 11 skalarnych parametrów — brak wspólnej projekcji

`renderMap` liczy projekcję (lokalne lambdy `projectLL`/`project`,
`:816`), a `renderGrid` (`:876`) dostaje **11 osobnych liczb**
(`area_*`, `min/max_lat`, `min_lon`, `lon_scale_geo`, `scale`, `off_*`) i
**powtarza tę samą matematykę projekcji** ręcznie w pętli (`:988`,
`:992`). To samo równanie żyje w trzech miejscach. Każda zmiana modelu
mapy wymaga edycji w kilku miejscach naraz.

### 2.3 Wybór kroku siatki — wielostopniowa heurystyka z nieaktualnym komentarzem

`renderGrid` wybiera krok siatki w **czterech** następujących po sobie
korektach (`:912`–`:952`):
1. największy krok ≤ `target_m`,
2. zwiększaj aż odstęp pikseli ≥ `MIN_GRID_PX` (22 px),
3. zmniejszaj aż zmieszczą się ≥2 interwały,
4. zwiększaj aż liczba linii ≤ `MAX_GRID_LINES` (40).

Uwagi:
- Komentarz przy kroku 4 (`:937`) mówi o „static buffers (40×40 = ~1600
  intersections)" — **takich buforów już nie ma**; pętla rysuje na bieżąco
  z `continue`-guardami (`:986`–`1002`). Cap 40 ogranicza dziś tylko
  liczbę iteracji pętli (wydajność), nie chroni żadnego bufora. Komentarz
  wprowadza w błąd.
- Kroki 2 i 3 mogą sobie przeczyć na bardzo małych ekranach
  (`MIN_GRID_PX = 22` vs `shorter_px/2`, gdy `shorter_px < 44`).
  Nie powoduje błędu, ale „ostateczny" krok bywa wtedy przypadkowy.

### 2.4 Drobne

- `_act_map[16]` z komentarzem „12 used today; pad" — ręczne pilnowanie
  rozmiaru; `pushAction` już to zabezpiecza, więc magiczna 16 jest zbędna.
- Bounding-box mapy i markery mają sporo powtarzalnego clamp-to-edge
  (`:852`, `:1011`).

---

## 3. Propozycja uporządkowania

### 3.1 Popup: dwa poziomy zamiast jednej płaskiej listy

Górne menu krótkie (akcje), ustawienia i operacje na pliku w podmenu:

```
Hold Enter →  Trail
  • Start / Stop tracking
  • Mark here
  • Waypoints…          → istniejący WP_LIST
  • Trail file…         → Save / Load / Export (live) / Export (saved) / Reset
  • Settings…           → Min dist · Readout · Grid   (LEFT/RIGHT w miejscu)
```

Korzyści:
- górne menu to **~5 pozycji, bez scrolla** na OLED;
- **jeden wzorzec na poziom**: górny i „Trail file" = Enter-akcje;
  „Settings" = wartości cyklowane LEFT/RIGHT — bez mieszania w jednym
  widoku;
- destrukcyjny `Reset` przeniesiony do „Trail file…", dalej od przypadkowego
  Entera;
- (opcjonalnie) `Grid` pokazywać tylko gdy aktywny jest widok Map, a
  `Readout` tylko przy Summary — menu zależne od kontekstu widoku.

> Wariant minimalny (mniej kodu): zostać przy jednej liście, ale
> **pogrupować** (ustawienia → akcje → plik), `Reset` na sam dół, usunąć
> podwójną ścieżkę `Grid`. Mniej porządku niż podmenu, ale tańsze.

### 3.2 Mapa: wspólny obiekt projekcji

Wydzielić mały `MapProjection` liczony raz w `renderMap` i przekazywany
do `renderGrid` oraz markerów:

```cpp
struct MapProjection {
  int32_t min_lat, max_lat, min_lon;
  float   lon_scale_geo, scale;
  int     off_x, off_y, area_x, area_y, area_w, area_h;
  void project(int32_t lat, int32_t lon, int& px, int& py) const;
};
```

- `renderGrid(display, proj)` zamiast 11 parametrów;
- jedno równanie projekcji (dziś powielone 3×);
- markery/waypointy też przez `proj.project(...)`.

Czysto refaktoryzacyjne — bez zmiany wyglądu mapy.

### 3.3 Siatka: uproszczenie i naprawa komentarza

- poprawić/skasować komentarz o „static buffers" (już nieaktualny);
- scalić wybór kroku w jedną pętlę „znajdź najmniejszy krok, który daje
  odstęp ≥ MIN_GRID_PX i ≤ MAX_GRID_LINES linii" zamiast czterech
  następujących korekt;
- bbox etykiety/strzałki północy liczyć z jednej funkcji pomocniczej.

Wynik wizualnie identyczny, logika krótsza i łatwiejsza do utrzymania.

---

## 4. Proponowany zakres (etapami)

| Etap | Zmiana                                                              | Ryzyko |
| ---- | ----------------------------------------------------------------- | :----: |
| 1    | Popup → dwa poziomy (Trail file…, Settings…); Reset głębiej; usuń podwójny Grid | niskie |
| 2    | Wydziel `MapProjection`; `renderGrid` i markery przez projekcję    | średnie (czysty refactor) |
| 3    | Uprość wybór kroku siatki; popraw nieaktualne komentarze; sprzątnij `_act_map` | niskie |

Etap 1 = największa poprawa „uporządkowania popupu" (główna prośba).
Etapy 2–3 = czyszczenie logiki mapy/siatki bez zmiany wyglądu.

---

## 5. Decyzje (zatwierdzone 2026-06-14)

1. **Popup — pełne podmenu.** Górne menu krótkie; `Trail file…` i
   `Settings…` jako podmenu.
2. **Menu zależne od widoku — tak.** `Grid` widoczny w Settings tylko na
   widoku Map, `Readout` tylko na Summary.
3. **Mapa — `MapProjection` + uproszczenie siatki** (etap 2 i 3 razem).
4. **Siatka — kwadratowe oczka, dociągnięte do krótszego boku.** Krok w
   przestrzeni pikseli (skala izotropiczna). Krótszy bok dzielony na
   całkowitą liczbę równych kwadratów (siatka dotyka tej pary krawędzi —
   na poziomym OLED: góra/dół), na dłuższym boku mieści się całkowita
   liczba tych samych kwadratów, wyśrodkowana. Dzięki temu oczka są
   **kwadratowe**, siatka wpisana w ramkę i symetryczna (brak
   jednostronnego przesunięcia). Etykieta skali = **nominalna** okrągła
   wartość. (Kwadraty + linie na wszystkich 4 krawędziach są
   geometrycznie niemożliwe dla dowolnego prostokąta — wybrano kwadraty +
   2 krawędzie + symetria.)
