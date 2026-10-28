# OpenDUNE dla Amigi z kartą Warp – plan i stan prac

## Stan prac

| Krok | Stan |
|---|---|
| 0. Źródła pobrane: upstream (GitHub) i port NovaCodera (Aminet) | ✅ 28.10.2026 |
| 0a. Uruchomienie i pomiar portu NovaCodera na Amidze 2 (punkt odniesienia) | ✅ 28.10 |
| 0b. Porównanie źródeł NovaCodera z upstream (na której wersji bazuje, co zmienił) | ✅ 28.10 |
| 0c. Decyzje (użytkownik, 28.10): **aktualny upstream**, **natywny backend Amigi** na wzór Atari (bez SDL), **pełny ekran 640×400 8-bit** (320×200 podwojone), **dźwięk Paula** | ✅ |
| 1. Build w Dockerze (`tools/build.sh`, `port/Makefile.amiga`, bebbo gcc 6.5, **-O1**) | ✅ 28.10 |
| 2. Natywny backend: obraz `video_amiga.c` (8-bit CLUT, 800×600 = 2,5×3, 640×400 = 2×2), mysz, klawiatura, timer `os/amiga.c`, katalogi `readdir_amiga.c`, głosy Paula `dsp_amiga.c` | ✅ 28.10 (gra od intro do misji 1) |
| 2b. Duża mapa: rozgrywka 640×480 1:1 (35×27 kafli), menu/mentat/fabryka 320×200 ×2 (`gui/layout_amiga.c`) | ✅ 28.10 (misja 1, Mentat, Options) |
| 2c. Sterowanie: zaznaczanie prostokątem, rozkaz prawym przyciskiem (`gui/rts_amiga.c`, `rts_controls=1`) | ✅ 28.10 (ruch grupy sprawdzony) |
| 3. Muzyka (MIDI: CAMD jak NovaCoder albo własny syntezator) – teraz `midi_none.c`, cisza | ⏳ |
| 4. Ikona, Tool Types, numer wydania, opisy | ⏳ |

## Źródła

| Co | Gdzie | Uwagi |
|---|---|---|
| OpenDUNE upstream | `upstream/` (`0.9-137-gebea79ae`, ostatni commit 30.09.2026) | C89, GPL-2.0; backendy obrazu: SDL, SDL2, Win32, DOS, OS X PPC, **Atari** (`video_atari.c`, `c2p1x1_8.s`); dźwięk: SDL, ALSA, OSS, Win32, …, **Atari** (`dsp_atari.c`, `atari_dma_sound.s`); MIDI: MT-32 (munt), FluidSynth, ALSA, Win32, Atari; wejście Atari `atari_ikbd.s` |
| Port NovaCodera | `novacoder/pkg/OpenDUNE` (paczka), `novacoder/src/Dune2_src` (źródła) | Aminet `game/strat/OpenDUNE_RTG.lha`, wersja 0.9.042 (na Amidze 2 zainstalowana starsza 0.9.023); SDL 1.2 (`video_sdl.c`, `dsp_sdl.c`), `amiga_main.c`, MIDI przez CAMD (`midi_camd.c`); wymagania: RTG, 68060, 64 MB, AHI |
| Dane gry | Amiga 2: `WorkSD:Games/OpenDUNE/data` | Dune II PC EU 1.07 |

## Charakterystyka gry (do potwierdzenia w fazie 0)

- Obraz wewnętrzny 320×200, 8 bit z paletą (jak oryginał), skalowanie opcjonalne (scale2x/3x, hq2x).
- Logika gry w C z odtworzonego kodu Dune II; tempo oryginału.
- Muzyka: oryginalne ścieżki przez emulator AdLib / MIDI (MT-32, GM).

## Doświadczenia z Dune Legacy (`../Dune`), które pewnie się przydadzą

- Własna cienka warstwa zamiast SDL (mini-SDL2) – pełna kontrola nad obrazem na Warpie.
- 8-bit CLUT na pełnym ekranie, składanie w VRAM, blitter karty, zapis do VRAM tylko longami,
  blokada bitmapy zdejmowana przed każdym wywołaniem systemu.
- Stałe tempo: klatka na stałą liczbę kroków logiki według harmonogramu.
- Narzędzia: AmiAgent, skrypty zdarzeń, profiler VERTB, `DUNE_STATS`, `clutshot`, `wbopen`.

## Faza 0 – wyniki (28.10.2026)

### Port NovaCodera 0.9.042 na Amidze 2 (`WorkSD:Games/OpenDUNE042`, dane z `OpenDUNE/data`)
- Ekran 8-bit 640×400 (320×200 ×2, `scalefactor=2`, `nearest`), intro, menu, wybór rodu, odprawa,
  gra – działają.
- **Obciążenie:** w spokojnej scenie skirmish (Ordos, misja 1) zadanie gry zajmuje **~90–92% CPU**
  (68060 @ 100 MHz), bezczynność 5–8% (`tools/amiga/taskload.c`: próbki ThisTask z przerwania VBlank).
  Histogram PC rozproszony (najczęstszy blok 6%) – to praca, nie pętla czekania. W menu też ~100%:
  zadanie o priorytecie -127 (`cpuload`) nie dostaje procesora – AmiAgent czekał na nie bez końca
  (wyglądało jak zawieszenie Amigi; nie używać `cpuload` przy tej grze).
- Licznik FPS gry (F8) nie pojawia się. Ctrl-C działa w intro, w grze nie (gra przestała reagować
  po dwóch Ctrl-C – restart Amigi).
- Porównanie: README.atari upstreamu – wersja 68030 ma w typowej scenie 1. poziomu **60 kl./s**
  (maksimum), 68000 na ST 6–15 kl./s. Rdzeń gry jest lekki; koszt portu NovaCodera to jego warstwa
  SDL 1.2 / RTG.

### Źródła NovaCodera wobec upstreamu
- Baza: upstream z ok. lutego–lipca 2019 (najbliżej `a4787e16` 2019-02-28 i `6dff8b6b` 2019-07-13;
  ~4100 linii różnic w plikach wspólnych). Upstream ma od tego czasu 7 lat poprawek i port Atari.
- Zmiany NovaCodera tylko w warstwie platformy: `video_sdl.c` (384 linie), przepisany 32-bitowy
  `hq2x.c`, `opendune.c` (230), `timer.c` (166), `dsp_sdl.c`, `input.c`, `file.c`, `inifile.c`,
  `mt32mpu.c`; nowe: `amiga_main.c`, `midi_camd.c`, `os/error_amiga.c`, `os/readdir_amiga.c`.
  Logika gry nietknięta (`gui.c` 17 linii, `gfx.c` 4, `cutscene.c` 3).

### Upstream – warstwa platformy (do decyzji)
- `video_sdl2.c` (760 linii) używa: okno, renderer, tekstura strumieniowa (`SDL_LockTexture`),
  `RenderCopy`, `RenderSetLogicalSize`, zdarzenia, `WarpMouseInWindow` – mini-SDL2 z `../Dune`
  musiałby dostać `LockTexture` i kilka drobiazgów.
- Port Atari: własny `video_atari.c` (632 linie, tryby 8bpp, c2p w asm), `dsp_atari.c` (DMA),
  własna obsługa klawiatury/myszy, `./configure --host=m68k-atari-mint --os=TOS --cpu=...` –
  wzorzec natywnego backendu bez SDL.

### Narzędzia (OpenDUNE/tools/amiga)
`cpuload.c` (tylko gdy nic nie zajmuje 100% CPU), `taskload.c` (bezpieczny: przerwanie VBlank;
`taskload N pc` – histogram PC najbardziej zajętego zadania), `inject.c` (mysz bezwzględna/względna,
klik, klawisze przez `input.device`; gra NovaCodera przechwytuje mysz – `rmove`, ok. 1,5 px ekranu
na krok). Przed każdym `inject` sprawdzić, że nikt inny nie gra.

## Port (faza 1–2) – stan 28.10.2026

### Budowa
- `port/src` = kopia `upstream/src` (+ `port/include`), zmiany wspólnego kodu tylko pod `__amigaos__`
  (wprowadzone skryptem `tools/amiga_patch.py`): ścieżki `PROGDIR:data`/`PROGDIR:saves` i łączenie
  ścieżek z `:` (`file.c`), `PROGDIR:opendune.ini` (`inifile.c`), log jak TOS (`os/error.c`),
  big endian (`os/endian.h`), bloki zmian 16 px (`gfx.h`), timer (`timer.c`), log
  `RAM:OpenDUNE.log` i brak konsoli przy starcie z ikony (`opendune.c`).
- Nowe pliki: `video/video_amiga.c`, `video/amiga_cmd.c`, `audio/dsp_amiga.c`, `os/amiga.c`
  (`$VER`, `__stack` 400000, `__chkabort`, zegar E-clock, sen przez timer.device),
  `os/readdir_amiga.c`.
- **-O1, nie -O2:** bebbo gcc 6.5 przy -O2 źle kompiluje `while (isspace(*p)) p++` w
  `Ini_GetString` (wskaźnik o 1 za daleko → `datadir` z doklejoną literą następnej linii).
- `tools/check060.sh`: 64-bitowe mul/div tylko w libnix (gmtime, strftime, stat) – poza grą.
- Binarka 350 KB (NovaCoder 598 KB).

### Obraz
- Własny ekran 8-bit CLUT (P96), `screenmode=` w ini: **800x600 (domyślnie) = 2,5×3, cały ekran,
  proporcje 4:3 oryginału** (kolumny na przemian 3 i 2 px), 640x400 = 2×2, 640x480 i 1024x768 –
  największa taka skala, wyśrodkowana.
- Wysyłane tylko bloki 16 px oznaczone przez grę (`g_dirty_blocks`), wiersz rozszerzany raz w Fast
  RAM i zapisywany longami do zablokowanej bitmapy (tylko zapis), lock tylko w `Video_Tick`.
- Paleta przez `LoadRGB32` (fade za darmo). F8: licznik klatek wysłanych / s w prawym górnym rogu.

### Tempo i wejście
- Kooperacyjnie jak Atari: `SleepAndProcessBackgroundTasks` **śpi** do następnego taktu 120 Hz
  (timer.device) zamiast kręcić pętlą. Ctrl-C (AmiAgent `stop`) sprawdzany w `Video_Tick` → `exit`
  z `atexit` (ekran, timer, audio) – działa.
- Mysz: pozycja (ukrytego) wskaźnika Intuition przeliczona na 320×200; gra ustawia pozycję przez
  `input.device`. (DELTAMOVE gubił ruch: Intuition odrzuca MOUSEMOVE przy pełnej kolejce.)
- Klawiatura: raw key Amigi → AT scancode (tabela w `video_amiga.c`).
- Pierwsze uruchomienie: intro nie do pominięcia, dopóki nie ma `saves/ONETIME.DAT` (jak oryginał).

### Pomiary (Amiga 2, 68060 @ 100 MHz, `taskload`)
| Scena | NovaCoder 0.9.042 (640×400) | Ten port (800×600) |
|---|---|---|
| intro | ~100% | **2–5%** |
| menu | ~100% | **5–8%** |
| misja 1 Atreides, początek | 90–92% | **~4–5%** |

### Testy zdalne (`debug_commands=1` w ini)
`tools/od.py "click X Y" "wait N" "key esc" "type TEXT" --shot plik.png --state` – polecenia w
układzie 320×200 przez `RAM:od-cmd` (`video/amiga_cmd.c`), zrzut z bufora gry (bez czytania VRAM),
stan (`gamemode`, ród, misja, mysz). Klik = ruch, potem naciśnięcie, 6 taktów.
Instalacja testowa: `WorkSD:Games/OpenDUNEWarp` (ini `tools/opendune-warp.ini`, dane z
`WorkSD:Games/OpenDUNE/data`). Zapis użytkownika „game1” (misja 1) w `saves/_SAVE000.DAT`.

### Duża mapa (decyzja użytkownika 28.10: rozgrywka 640×480 1:1, menu ×2 na środku)
- Etap A (`tools/amiga_patch_bigscreen_a.py`): `SCREEN_WIDTH/HEIGHT` = zmienne `g_screenWidth/Height`,
  bufory na 640×480, rozmiary `uint32`, bloki zmian 2 słowa × 480 wierszy, poprawione przepełnienia
  (`GUI_DrawChar` – przesunięcie w uint16, `GFX_GetSize`, tablice `GUI_Screen_FadeIn`), krok kafli
  liczony z bieżącej szerokości.
- Etap B (`tools/amiga_patch_bigscreen_b.py`, `gui/layout_amiga.c`): `Layout_Set(true)` w
  `GUI_DrawInterfaceAndRadar`, `Layout_Set(false)` przy `SELECTIONTYPE_MENTAT` i przycisku Mentat.
  Widok 35×27 kafli (`VIEWPORT_TILES_W/H`), panel i radar przesunięte o `SIDEBAR_DX` = 320 (ta sama
  wysokość co w oryginale), okna 14–18 i komunikat wyśrodkowane, krawędzie przewijania i obszary
  kliknięć z rozmiaru widoku, `SCREEN.CPS` składany (`Layout_ComposeBackground`), ramka radaru
  przycięta, widok ograniczany do mapy (najpierw górna granica – mapa bywa mniejsza niż widok),
  głośność dźwięków od środka widoku. Backend sam wykrywa zmianę rozmiaru (nowa skala, czyszczenie).
- Sprawdzone: wczytanie `game1`, mapa misji 1 (32×32 mieści się cała), Mentat i powrót, Options
  (wyśrodkowane). CPU w misji 1: 7–9%.
- Do sprawdzenia: przewijanie na dużej mapie (62×62, dalsze misje), fabryka/starport, koniec misji,
  zapis/wczytanie z gry, wypełnienie panelu pod radarem (teraz czarne).

### Sterowanie (prośba użytkownika 28.10, dodatek, `rts_controls=1` domyślnie, 0 = oryginał)
- Lewy przycisk wciśnięty na mapie i przeciągnięty (> 4 px) → prostokąt (rysowany przez sterownik
  obrazu w VRAM, pod nim obraz wysyłany ponownie), po puszczeniu zaznaczone wszystkie własne
  jednostki naziemne w środku (bez carryall/ornitoptera/rakiet), pierwsza w panelu, wszystkie ze
  znacznikiem. Wciśnięcie na mapie jest wstrzymane do puszczenia albo początku przeciągania; zwykły
  klik dostaje gra jako wciśnięcie + puszczenie (jak w oryginale; znika tylko „przytrzymaj, by
  przewinąć” na polu mapy).
- Prawy przycisk na mapie przy zaznaczonych własnych jednostkach: ruch (grupa zachowuje układ,
  przesunięcie do ±3 kafli od punktu), na wrogiej jednostce/budynku – atak (kto może). Bez
  zaznaczenia prawy przycisk działa jak w oryginale.
- Działa tylko na ekranie mapy w trybie UNIT/STRUCTURE, gdy pętla gry chodzi (nie w oknach).
- Testy: `od.py "ldown X Y" "lmove X Y" "lup"`, `rclick`, `--state` pokazuje `unit`/`group`.
- Sprawdzone: zaznaczenie 2 jednostek, ruch grupy; użytkownik: ponowne zaznaczanie i atak działają.
- Grupa w panelu (prośba 28.10): nazwa „SQUAD”, obrazek z miniatur (½) do 3 typów jednostek grupy
  (jeden typ: jego obrazek) + liczba jednostek, pasek DMG = średnie zdrowie w %, w pasku wiadomości
  „Squad of N units.” (`RTS_DrawSquadPicture`, `RTS_GroupHealthPercent`; miniatury przez SCREEN_3
  jako bufor roboczy). Obrazek: obrazek pierwszego typu + miniatury (½, kolor 0 przezroczysty)
  do 2 kolejnych typów po prawej, liczba jednostek na dole po lewej.
- Przyciski akcji dla całej grupy (`RTS_GroupAction`): akcja bez celu (Retreat, Guard, ze Shiftem
  Area Guard) od razu dla wszystkich, które ją mają; Move/Harvest/Attack po wskazaniu celu
  (`viewport.c`, blok SELECTIONTYPE_TARGET) – ruch z zachowaniem układu, atak na ten sam cel.
  Sprawdzone: MOVE grupy (oba trike'i dojechały), GUARD bez błędów.
- Narzędzie testowe: `Video_Amiga_Mouse` przesuwa też wskaźnik Intuition (inaczej po przesunięciu
  kursora przez grę następny MOUSEMOVE Intuition przywracał starą pozycję i klik trafiał obok).

### Panel boczny, wersja 2 (projekt użytkownika 28.10, `tools/amiga_patch_sidebar.py`)
- Na dużym ekranie radar na górze panelu (`RADAR_Y` = 42, w oryginale 136), pod nim złoty pasek
  z SCREEN.CPS (wiersze 124–135), niżej panel akcji przesunięty o `PANEL_DY` = 76 i wydłużony do
  dołu ekranu (widżet 6, wysokość 354); na dole ekranu znów pasek. Tło składa
  `Layout_ComposeBackground`.
- Oddział (grupa ≥ 2): „SQUAD”, pod nim obrazki jednostek (30×22, bez ramki) po dwa w rzędzie,
  jednakowe typy obok siebie, pod każdym linia zdrowia (zielona/żółta/czerwona jak pasek DMG),
  przy braku miejsca ostatnia komórka „+N”; pod listą „DMG” i pasek średniego zdrowia
  (`g_progressbarY`), pod nim cztery przyciski akcji (`RTS_PlaceSquadButtons`). Pojedyncza
  jednostka/budynek – jak w oryginale, tylko niżej.
- Kredyty: kopia z widżetu 4 do widżetu 5 bierze y widżetu 5 (było „y widżetu 4 − 40”).
- Sprawdzone na Amidze 2: układ, lista 3 jednostek, pasek, przyciski, kliknięcie w radar.
- Zmiana (prośba 28.10): jedna ikona na typ jednostki z liczbą sztuk (biała z czarnym cieniem)
  i linią średniego zdrowia typu; pasek uszkodzeń grupy na całą szerokość panelu pod ikonami
  (`g_progressbarX/W`), bez napisu DMG.
- Poprawka (zgłoszenie 28.10): zwykły klik na mapie kończy grupę – zostaje zaznaczone tylko to,
  co kliknięto (także jednostka z grupy, również prowadząca), panel wraca do jednej jednostki.
  Wcześniej klik na członka grupy zostawiał grupę i rozkazy szły dalej do wszystkich.

### Pomiary czasu (28.10.2026, Amiga 2, 68060 @ 100 MHz, 640×480, misja 1 Atreides, `game1`)
`debug_stats=1` → `RAM:OpenDUNE-stats.txt` co 5 s (`os/amiga_prof.h`, E-clock); na ekranie tylko
licznik FPS w prawym dolnym rogu (`show_fps=1` albo F8). fps = klatki wysłane do karty (obraz
zmienia się tylko, gdy gra coś zmieni), loop = przejścia pętli gry (maks. 120/s).

| Scena | fps | loop/s | logika | mapa (draw) | panel/UI | wysyłanie | sen (wolne) |
|---|---|---|---|---|---|---|---|
| postój | 2–6 | 120 | 0,8% | 0,9–1,1% | 0,7% | 0,8–1,0% (0,1 ms) | 94% |
| grupa w ruchu + przewijanie | 20–24 | 120 | 1,3% | 5% (0,4 ms/wywołanie) | 0,8% | 3% (0,5 ms/klatkę) | 87% |

- CPU 060/100 zajęty w 6–13%; fps ogranicza tempo gry (jednostki przesuwają się w krokach logiki),
  nie wydajność. Wczytanie zapisu: jednorazowo 1,8 s w `draw` (pełne rysowanie + przyciemnienie,
  które w środku śpi – czasy części zagnieżdżonych liczą się podwójnie).
- Do zmierzenia: duża mapa (62×62) z przewijaniem – każdy krok przewijania kopiuje widok
  560×432 w buforze i wysyła go cały do karty (możliwa poprawa: przewijanie blitterem karty
  VRAM→VRAM jak w AmiXcom/Settlers).

### Płynny ruch jednostek (decyzja użytkownika 28.10, `gui/smooth_amiga.c`, `smooth_units=1`)
- Logika bez zmian: ruch co 3 takty zegara gry (50 ms, 20/s). Na początku kroku zapisywane są
  pozycje wszystkich jednostek; rysowanie (pętle jednostek naziemnych i powietrznych w
  `viewport.c`) pokazuje jednostkę między poprzednią a bieżącą pozycją według czasu od kroku.
  W każdej klatce poruszające się jednostki i kafle wokół obu pozycji (`Map_Update(.., 0)`) są
  oznaczane do przerysowania, po ostatniej klatce pośredniej jeszcze raz w pozycji końcowej.
  Skok > 2 kafli bez wygładzania.
- Pomiar (misja 1, grupa w ruchu): fps 20–24 → **57–58**; rysowanie mapy 5% → 14,5%
  (1,2 ms/wywołanie), wysyłanie 3% → 4,5–5% (0,7–0,8 ms/klatkę), wolne 87% → 75%.
  Na serii zrzutów w ruchu brak smug.
