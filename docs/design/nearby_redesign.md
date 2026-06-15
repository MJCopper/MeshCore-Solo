# Nearby Nodes — analiza i propozycja uporządkowania

> Branch: `refactor/nearby-nodes`
> Plik źródłowy: [NearbyScreen.h](../../examples/companion_radio/ui-new/NearbyScreen.h)
> Status: **propozycja** (do akceptacji przed implementacją)

---

## 1. Jak narzędzie jest zbudowane dzisiaj

Ekran łączy w sobie **trzy osobne tryby** o własnych listach, widokach
szczegółów i popupach:

```
NearbyScreen
├── LIST (kontakty zapisane w mesh)        ← tryb domyślny
│   ├── filtr cyklowany LEFT/RIGHT (7 stanów)
│   ├── DETAIL  (Enter)  → Lat/Lon/Dist/Type/Seen
│   │   └── _opts popup (Hold Enter): Navigate / Ping / Save waypoint
│   │       └── _ping_menu popup
│   ├── NAV view (pełnoekranowa nawigacja)
│   └── _ctx_menu popup (Hold Enter): Discover / Navigate / Save waypoint
│
└── DISCOVER (skan na żywo NODE_DISCOVER_REQ) ← osobny pod-ekran
    ├── lista wyników (karty 2-liniowe)
    │   Hold Enter = ponowny skan (brak menu!)
    └── DETAIL (Enter) → pubkey / RSSI / SNR / status
        └── _ping_menu popup (Hold Enter = od razu ping, bez Options)
```

Stan ekranu trzyma **15+ pól** (`_detail`, `_nav`, `_discover_mode`,
`_ddetail`, `_pinging`, `_filter`, dwa komplety `_sel/_scroll`, trzy
bufory wyników ping…) i **trzy** instancje `PopupMenu` (`_ctx_menu`,
`_opts`, `_ping_menu`).

---

## 2. Co konkretnie jest nieuporządkowane

### 2.1 Jeden cykl filtra miesza trzy różne osie

`LEFT/RIGHT` przewija 7 stanów:

```
Fav · ALL · Comp · Rpt · Room · Snsr · TIME
```

Ale to nie jest jedna oś — to **trzy** wciśnięte w jeden liniowy cykl:

| Stan                        | Czym naprawdę jest                                   |
| --------------------------- | ---------------------------------------------------- |
| `Fav`                       | filtr **ulubionych** (flaga `ci.flags & 1`)          |
| `ALL/Comp/Rpt/Room/Snsr`    | filtr **po typie** węzła                             |
| `TIME`                      | **sortowanie** (po `lastmod` zamiast po dystansie)   |

Konsekwencje:
- użytkownik „scrolluje po kategoriach” na ślepo — nie widzi wszystkich
  naraz, musi cyklować, by trafić w to, czego szuka;
- **kombinacje są niemożliwe**: nie da się zobaczyć „ulubionych
  repeaterów posortowanych po dystansie" ani „repeaterów posortowanych
  po czasie” — model wymusza dokładnie jeden stan z siedmiu;
- `TIME` jako „filtr" jest mylące — zmienia kolejność, nie zawartość;
- etykieta w nagłówku (`NEARBY[TIME]`) nie mówi, że to sort.

### 2.2 Te same akcje, różne menu w zależności od miejsca

| Akcja          | Lista (`_ctx_menu`) | Detail kontaktu (`_opts`) | Detail discover |
| -------------- | :-----------------: | :-----------------------: | :-------------: |
| Navigate       |          ✓          |             ✓             |        —        |
| Save waypoint  |          ✓          |             ✓             |        —        |
| Ping           |          —          |             ✓             |        ✓        |
| Discover       |          ✓          |             —             |        —        |

Ten sam węzeł oferuje inny zestaw akcji w zależności od tego, gdzie na
niego patrzysz. Ping jest osiągalny tylko ze szczegółów, Discover tylko
z listy.

### 2.3 „Hold Enter” znaczy co innego na każdym ekranie

| Ekran             | Hold Enter (`KEY_CONTEXT_MENU`)      |
| ----------------- | ------------------------------------ |
| Lista nearby      | otwiera menu kontekstowe             |
| Detail kontaktu   | otwiera menu Options                 |
| Lista discover    | **ponowny skan** (żadnego menu)      |
| Detail discover   | **od razu Ping** (pomija Options)    |

Brak spójnego modelu „przytrzymaj = menu akcji”.

### 2.4 `_ping_menu` to bespoke widget

W przeciwieństwie do reszty popupów, ping-menu:
- ma wiersze tylko-do-odczytu (RTT / SNR), więc połyka `UP/DOWN`;
- przebudowuje się w trakcie (`rebuildPingMenu`) gdy przychodzą wyniki;
- zostaje otwarte po `SELECTED` (reszta popupów się zamyka);
- ma własny `handlePingMenuInput` z trybem `allow_enter_to_open`.

To działa, ale jest to czwarty, niestandardowy wzorzec interakcji w
jednym narzędziu.

### 2.5 Duplikacja list/detail/ping

`renderDiscover`/`handleInputDiscover`/`renderDiscoverDetail` to niemal
równoległa kopia logiki listy i szczegółów nearby (osobne `_dsel`,
`_dscroll`, `_d_visible`, własne rysowanie kart). Discover i Nearby
robią to samo — pokazują listę węzłów z możliwością wejścia w szczegóły
i pingowania — ale dwoma osobnymi ścieżkami kodu.

---

## 3. Propozycja uporządkowania

Trzy zasady przewodnie: **(a) jedno menu akcji wszędzie**, **(b) filtr
i sortowanie jako osobne, jawne osie**, **(c) jeden wzorzec listy**.

### 3.1 Rozdziel filtr od sortowania

Zamiast jednego cyklu 7-stanowego — dwie niezależne osie wybierane z
menu (nie przez ślepe cyklowanie):

**Filtr** (oś „co pokazujemy") i **sort** (oś „w jakiej kolejności") są
od siebie niezależne i **łączą się dowolnie**:

```
FILTR (jedna oś — typ węzła, z Ulubionymi)      SORT (przełącznik)
  • Wszystkie                                      • Dystans (domyślnie)
  • Ulubione                                       • Ostatnio słyszane
  • Companion
  • Repeater
  • Room
  • Sensor
```

**Sterowanie (zatwierdzone):**
- `LEFT/RIGHT` = szybki cykl **tylko po filtrze-typie** (jedna spójna oś,
  znany gest; bez „TIME" zanieczyszczającego cykl);
- **Sort** = przełącznik w menu akcji (`Dystans ↔ Ostatnio słyszane`),
  trzymany niezależnie od filtra;
- **Filter…** dostępny też w menu akcji (odkrywalność — cała lista
  widoczna naraz, nie tylko cyklowanie).

Dzięki temu „ulubione repeatery po dystansie" staje się możliwe, a w
nagłówku widać oba wymiary, np. `NEARBY · Rpt · ↓dist`.

### 3.2 Jedno spójne menu akcji (Hold Enter wszędzie)

Jeden `PopupMenu` (zamiast `_ctx_menu` + `_opts`), pozycje zależne od
kontekstu, ale **kolejność i nazwy stałe**:

```
Hold Enter →  Options
  • Navigate        (gdy węzeł ma GPS)
  • Ping            (gdy znamy pubkey)
  • Save waypoint   (gdy węzeł ma GPS)
  ──────────────
  • Filter…         (podmenu z 3.1)
  • Sort…           (toggle z 3.1)
  • Discover scan   (uruchamia skan na żywo = przełącza źródło)
```

To samo menu na liście i w szczegółach, w obu źródłach (Zapisane /
Skan). Pozycje niedostępne są pomijane (jak już teraz robi `has_node`),
ale nigdy nie zmieniają kolejności ani nazw. „Hold Enter = menu akcji" —
bez wyjątków. Ping zawsze przez Options (znika „od razu Ping" z detalu
Discover); rescan to pozycja menu, nie ukryty Hold Enter.

### 3.3 Jedna lista, dwa źródła (Zapisane / Skan na żywo)

**Zatwierdzone:** zamiast osobnego pod-ekranu Discover — **jeden
komponent** listy/szczegółów/menu/ping napędzany przełącznikiem
**źródła**:

- ta sama nawigacja, to samo menu akcji, ten sam wzorzec szczegółów i
  pingu (znika duplikacja `renderDiscover*`, niespójne Hold Enter i
  połowa pól stanu);
- różni się tylko **prawa kolumna** wiersza i pola w detalu:
  - źródło **Zapisane** → dystans / azymut / lastmod (jak dziś),
  - źródło **Skan na żywo** → RSSI / SNR (dane z `DiscoverResult`).

To nie jest dosłowne zlanie dwóch list w jedną tablicę (dane mają różny
kształt — kontakt ma GPS, wynik skanu ma sygnał), tylko **jedna ścieżka
interakcji nad dwoma źródłami**. Wybór źródła = „Discover scan" w menu
(uruchamia `NODE_DISCOVER_REQ` i przełącza widok na wyniki) oraz powrót
do Zapisanych przez Cancel.

---

## 4. Proponowany zakres (etapami)

| Etap | Zmiana                                                                   | Ryzyko |
| ---- | ------------------------------------------------------------------------ | :----: |
| 1    | Rozdziel sort od filtra: `TIME` znika z cyklu; `LEFT/RIGHT` cyklują tylko typ+Fav; sort jako stan + toggle | niskie |
| 2    | Scal `_ctx_menu` i `_opts` w jedno menu akcji o stałej kolejności; dodaj Filter…/Sort… | niskie |
| 3    | Ujednolić Hold Enter w Discover (menu zamiast bezpośredniego rescan/ping; Ping zawsze przez Options) | średnie |
| 4    | Scal listę Discover z listą Nearby w jeden komponent z przełącznikiem źródła | wyższe |

Kolejność implementacji: 1 → 2 → 3 → 4. Etapy 1–2 dają największą
poprawę „uporządkowania" przy najmniejszym ryzyku; 3–4 domykają
spójność (jedna lista, jedno menu wszędzie).

---

## 5. Decyzje (zatwierdzone 2026-06-14)

1. **Filtr i sort — pełne rozdzielenie osi.** `LEFT/RIGHT` = szybki cykl
   filtra-typu; Sort jako toggle w menu; Filter… też w menu dla
   odkrywalności. (Nie chowamy wszystkiego do menu — zachowujemy szybki
   gest.)
2. **Discover — jedna lista, dwa źródła.** Wspólny komponent
   interakcji; różni się tylko prawa kolumna/pola detalu (dystans vs
   sygnał).
