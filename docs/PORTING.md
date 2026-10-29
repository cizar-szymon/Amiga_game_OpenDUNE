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
  **Od 29.10.2026 (życzenie użytkownika): menu i inne ekrany 320×200 zawsze w tej samej całkowitej
  skali w obu kierunkach, na środku z czarną ramką** – ×2 na 640×400 / 640×480 / 800×600 / 960×540,
  ×3 na 1024×768 / 1280×720; bez nierównych kolumn 2,5× (`Video_SetGeometry`). Sprawdzone w WinUAE
  800×600: menu 640×400 w (80,100), kliknięcie myszą trafia.
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

### Co się odświeża na postoju (28.10, `debug_stats`: klatki ze zmianami w częściach ekranu)
| Postój, misja 1 | fps | górny pasek / 5 s | mapa / 5 s | radar | panel |
|---|---|---|---|---|---|
| `smooth_units=1` | 4–6 | 11 | 11–23 | 0 | 0 |
| `smooth_units=0` | 1–3 | 0–11 | 9 | 0 | 0 |

Pasek: gra co kilka sekund przepisuje status wybranej jednostki. Mapa: pojedyncze jednostki, które
się poruszają (porównanie zrzutów); z płynnym ruchem każdy ich krok to ok. 3 klatki zamiast 1.
Licznik FPS przeniesiony do lewego dolnego rogu (prośba użytkownika).

### Przewijanie blitterem, przewijanie przy krawędziach ekranu, F10 (28.10.2026)
- `vram_scroll=1` (domyślnie): kopia ekranu w Fast RAM (układ bitmapy), wysyłane tylko bloki różne
  od kopii; przy przewinięciu mapy (`Video_ScrollView` z `GUI_DrawScreen`) widok przesuwa w VRAM
  blitter karty (`BltBitMap` + `WaitBlit`, bez blokady), kopia przesuwana tak samo. Nakładki
  (prostokąt, FPS) rysowane też w kopii. Tylko przy 1:1.
- Przewijanie przy krawędziach całego ekranu (skrajne 2 piksele, także nad paskiem i panelem),
  w narożnikach po skosie, strzałka kursora, 10 kafli/s (`RTS_EdgeScroll`).
- `debug_start_scenario=N`: Play a Game zaczyna od misji N (od 5 mapy 62×62, `MapScale=0`).
- F10: `bugNNN.txt` (stan), `bugNNN.dat` (zapis gry), `bugNNN-screen.bmp` (odczyt z karty),
  `-game.bmp` (SCREEN_0), `-copy.bmp` (kopia sterownika) w katalogu zapisów
  (`os/bugreport_amiga.c`).

Pomiar, misja 5 (62×62, Harkonnen, `misja5`), przewijanie przy krawędzi + ruch jednostek:

| | do karty | wysyłanie | rysowanie mapy | wolne |
|---|---|---|---|---|
| `vram_scroll=0` | 15–34 KB/klatkę | 2,0–2,8 ms/klatkę | 25–34% | 42–64% |
| `vram_scroll=1` | 0–1 KB/klatkę (23 przesunięcia blitterem / 5 s) | 2,1–2,9 ms/klatkę | 17–34% | 41–64% |
| + płynny ruch oznaczany raz na klatkę (60 Hz, nie 120) | 1 KB | 2,6 ms | **26,6%** (było 34%) | **47%** (było 40%) |

- Zapis na kartę spadł 20–30×, ale czas wysyłania nie: koszt to przejście po obszarze, który gra
  oznacza jako zmieniony (przy przewinięciu cały widok, przy płynnym ruchu jadące jednostki),
  porównanie z kopią kosztuje tyle co zapis 15–34 KB (`v.send` 2,6 ms, `v.lock` ~0). Zysk
  blittera ujawni się przy wolniejszym zapisie do VRAM / większym ekranie.
- Raport F10 po przewijaniu: obraz z karty = obraz gry (różnica tylko pod licznikiem FPS),
  kopia = karta.
- Statystyki dostały części `video`: `v.events`, `v.areas` (tylko przy `debug_stats=1`),
  `v.send`, `v.lock`, oraz `blits` i `KB/frame`.
- Przewinięcie jednym przejściem (`GFX_ShiftViewport`): widok przesuwany w SCREEN_0 w miejscu,
  każdy przesunięty wiersz od razu (z pamięci podręcznej) do SCREEN_1; znika wymuszona pełna kopia
  SCREEN_1→SCREEN_0 po przewinięciu (`viewport.c`, `hasScrolled`) – kopiowane tylko zmienione
  zakresy. Gdy karta przesuwa blitterem (`Video_ScrollView` zwraca true), przesuniętej części nie
  trzeba ani porównywać, ani wysyłać; inaczej widok oznaczany jako zmieniony.
  Ta sama scena: rysowanie mapy 26,6% → 23,8% (2,2 → 1,9 ms), wysyłanie 2,6 → 2,1 ms/klatkę,
  wolne 47% → **53%**. F10 w trakcie i po przewijaniu w różnych kierunkach: obraz z karty = obraz
  gry = kopia.

### Pauza, ślady płynnego ruchu (28.10.2026)
- P na ekranie mapy: pauza (zegar gry stoi, przewijanie/zaznaczanie/rozkazy działają, rozkazy po
  wznowieniu), drugie P – dalej; pauza trzyma się po oknach, które włączają zegar; komunikat w pasku.
  Sprawdzone: zegar gry stoi przez 300 taktów pauzy (382 → 382), po P rusza (691).
- Zgłoszenie użytkownika (F10, BUG001): pionowe kawałki przelatującej jednostki na mapie; obraz
  z karty = obraz gry, więc błąd w rysowaniu, nie w wysyłaniu. Przyczyna: płynny ruch oznaczał do
  przerysowania tylko kafle wokół końców drogi, a jednostka rysowana w pozycjach pośrednich (z
  cieniem +1,+3 i rozmyciem) zostawiała kawałki na kaflach tylko kopiowanych. Teraz przerysowywane
  są wszystkie kafle prostokąta obejmującego drogę od poprzedniej do bieżącej pozycji + 24 piksele
  (`Map_Update(.., 0)` dla każdego).
- Test śladów `tools/ghost_check.py`: gra, pauza, zrzut, `redraw` (pełne przerysowanie), zrzut,
  różnice w widoku; `--spawn` tworzy latającą jednostkę (polecenie testowe `spawn`), `--ignore`
  pomija animowany budynek. Stara wersja: okruchy wokół ornitoptera i na jego trasie; nowa: brak
  śladów (zostają tylko różnice przy wiszących w powietrzu ornitopterach – losowy szum ich cienia,
  DRAWSPRITE_FLAG_BLUR – i animacja budynków, które chodzą też w pauzie). Bez płynnego ruchu
  16 pikseli w 6 cyklach.
- Koszt: misja 5, przewijanie + ruch: rysowanie mapy 18,3% (1,5 ms), wolne 62%.

### Instalacja na Amidze 1 (A1200 + Warp 1260, 28.10.2026)
- `WorkSD:Games/OpenDUNEWarp`: OpenDUNE, ikona `OpenDUNE.info` (`tools/make_icon.py` z portu Vastar,
  kadr ornitoptera nad piaskiem), `opendune.ini` (`tools/opendune-amiga1.ini`: dane z
  `WorkSD:Games/OpenDUNE/data` – instalacja NovaCodera użytkownika, tylko odczyt – show_fps,
  debug_commands), własne `saves`. Ikona na pulpicie: `tools/amiga/leaveout.rexx` (ARexx
  Workbencha, ICONS.LEAVEOUT). Start sprawdzony (intro).
- `WorkSD:Games/OpenDUNE` na obu Amigach to instalacja użytkownika (NovaCoder) – nie zmieniać.
- Ikona od 29.10 (życzenie użytkownika): ekran tytułowy z raportu F10 (BUG002-GAME.BMP, Amiga 2),
  bez FPS, ramki menu i „V1.07”, pomniejszony do 112×70 (o 30% mniej niż pierwsze 160×100),
  119 kolorów; zaznaczona ma złotą, „podświetloną” ramkę (drugi obraz ikony, klasyczny i ColorIcon):
  `python tools/make_icon.py build/amiga/OpenDUNE.info --frame tools/icon/title112.png --crop 0 0 112 70 --lit-frame`.
  Przy pierwszej instalacji nadpisano tam przez pomyłkę `opendune.ini`; przywrócone domyślnym
  z paczki NovaCodera (własne ustawienia użytkownika utracone).

### LOAD / SAVE, Resolution, czarny panel, bez zabezpieczenia, bez intro (28.10.2026, Amiga 1)
- Górny pasek: przyciski LOAD i SAVE za OPTIONS (widżety 12, 13 – poniżej 39..45, bo 45 obejmuje
  ekran i bierze kliknięcia pierwszy). Grafika: końce przycisku OPTIONS przycięte do szerokości
  napisu; napis złożony z liter grafik MENTAT/OPTIONS (O, A, S, E; L, D z pnia I i połowy O,
  V kreskami), lekko pogrubiony, w wierszach liter oryginału. LOAD/SAVE otwierają od razu listę
  wczytania/zapisu. Przy 400×300 jeden LOAD/SAVE (czcionka 8 px) z oknem Load/Save/Continue.
- Options → Resolution (okno 10, 8 pozycji; `WindowDesc.widgets[8]`, `g_widgetProperties[24]`):
  rozdzielczości z Settlers 400×300, 640×400, 640×480, 800×600, 640×360, 960×540; ekran otwierany
  na nowo po zamknięciu Options (`Video_Amiga_SetScreen`), mapa 1:1 na cały ekran, wybór w
  `saves/RESOLUTION.CFG`; `screenmode=` w ini ma pierwszeństwo. Bufory do 960×600.
  Sprawdzone na Amidze 1: 800×600, 400×300, 960×540, powrót do 640×480.
- Panel akcji poniżej zawartości (oryginalne 82 wiersze lub najniższy widoczny przycisk) czarny,
  ramka skrócona (`Layout_PanelBlackBelow`).
- Zabezpieczenie (pytania o obrazki po misjach kampanii 1 i 7) wyłączone; `copy_protection=1`
  przywraca. `skip_intro=1`: bez intro przy starcie (testy).
- `tools/amiga_free.sh`: przerywa, gdy OpenDUNE działa (kanał `RAM:od-cmd` trafia do każdej kopii
  z `debug_commands=1` – 28.10 polecenia testowe trafiły do gry użytkownika).

### Poprawki LOAD/SAVE, Options, 960×540 (29.10.2026, Amiga 1)
- Options bez „Load a game” / „Save this game” (robią to przyciski LOAD i SAVE): 6 pozycji
  (Game controls, Resolution, Restart, Pick another house, Quit, Continue), tablica
  `s_optionsMap` na numery oryginału; okno 10 ma 117 wierszy, okno Resolution na czas pokazania 151.
- Przyciski: zawsze duże napisy; gdy nie mieszczą się na miejscach oryginału (400×300),
  MENTAT i OPTIONS przesuwają się w lewo i bliżej siebie (`s_barLayouts`: MENTAT 2, OPTIONS 83,
  LOAD 164, odstęp 3, margines 5 lub 4). Dopiero gdy i to nie wystarcza – małe LOAD i SAVE
  czcionką 8 px (kolor napisu OPTIONS, ciemnozłoty cień zamiast czarnej obwódki), a na końcu
  jeden LOAD/SAVE. Sprawdzone w 400×300: OPTIONS i SAVE klikają się na nowych miejscach.
- 960×540 i 640×360: Amiga 1 nie ma takich trybów 8-bit (`p96modes`: 320×200 … 800×600, 1024×768,
  1280×720, 1280×1024, 1360×768). Ekran był 960×540 w lewym górnym rogu trybu 1024×768; teraz
  ekran ma rozmiar trybu, a obraz jest wyśrodkowany (`s_reqW/H` = rozmiar gry, `s_width/height` =
  ekran). Licznik FPS w lewym dolnym rogu obrazu, nie ekranu.

### Options → „Main menu” zamiast „Quit playing” (29.10.2026, życzenie użytkownika)

Przycisk wraca do menu głównego (po pytaniu „Are you sure you want to quit playing?”) zamiast
kończyć program – z menu można zagrać przez LAN; z programu wychodzi Exit Game w menu.
`g_quitToMenu` z `widget_click.c`, `GameLoop_QuitToMenu` w pętli głównej (jak po grze LAN);
podpis z `String_Get_ByIndex(STR_QUIT_PLAYING)`. Sprawdzone w WinUAE (misja 5 → menu → Play on LAN).

### Płynny procent budowy (29.10.2026, uwaga użytkownika)

Gra liczy postęp budowy co 30 tików (0,5 s): przy krótkich budowach (CONCRETE) procent skakał o
kilka naraz. Panel pokazuje go teraz płynnie: między krokami przesuwa go o czas, który minął
(`Structure_TickFraction`, `GUI_Amiga_BuildPercent` w `widget_draw.c`) – gdy budowa idzie (nie
wstrzymana, są kredyty) – i przerysowuje się przy każdej zmianie o 1% (`displayedPercent`).
Tylko wyświetlanie: gra (i gra LAN) bez zmian. WinUAE, CONCRETE: 8, 21, 33, 46, 59, 71, 84, 97%
w zrzutach co ~0,6 s (wcześniej co 6,25%).

### Options → Amiga info, róg do Workbencha (29.10.2026, życzenie użytkownika)

- **Amiga info** (trzecia pozycja Options, także w grze LAN): te same dane i możliwości co okno
  Amiga w Settlers – procesor i rewizja (68060: `MOVEC PCR` w `Supervisor`), FPU, Kickstart,
  Chip/Fast RAM, karta P96 i jej pamięć, ekran, a z Warpem (`SYS:C/WarpTool Diag`) model, firmware
  i zegar CPU z przyciskami − / +. 68060: 50–100 MHz co 10 (powyżej zegara z otwarcia okna tylko
  rev. 6), 68040: 20–50 MHz co 1; zegar, który był przy otwarciu (Amiga 2: 105 MHz), zawsze da się
  przywrócić, kroki idą do pełnych dziesiątek (105 → 100 → 105). Kod: `os/sysinfo_amiga.c` (kopia
  `amiga_sysinfo.c` z Settlers, plik tymczasowy `RAM:OpenDUNE-warp.txt`),
  `GUI_Amiga_AmigaInfoWindow` w `widget_click.c` (okno 10, na czas pokazania 190 wierszy – mieści się
  w 320×200). Okno Options ma teraz 134 wiersze (7 pozycji).
- **Róg 2×2 piksele w prawym górnym rogu ekranu**: kliknięcie w dowolnej chwili przenosi na
  Workbench (`ScreenToBack` + `WBenchToFront` w `Video_HandleEvents`, kliknięcie nie trafia do
  gry); powrót Lewa Amiga + M. Gra w tym czasie dalej chodzi.
- Sprawdzone: WinUAE (bez Warpa: „No Warp”), Amiga 2 (Warp7060, firmware 3.058, 105 → 100 → 105 MHz
  potwierdzone przez `WarpTool Diag`; róg → Workbench na wierzchu).

### Resztki ramki zaznaczania po przewinięciu (29.10.2026, raport BUG007 z Amigi 2)

Ramka przeciągania jest rysowana prosto na karcie i w kopii ekranu. Gdy w trakcie przeciągania mapa
przewinęła się przy krawędzi ekranu, blitter przesunął ramkę razem z obrazem, a zmazywane było tylko
jej stare miejsce – przesunięta górna krawędź i kawałki boków zostawały. Teraz po przewinięciu
(`Video_DoScroll`) odświeżane jest też przesunięte miejsce (`Video_RectDirty`), a ramka rysowana od
nowa. WinUAE, misja 8, przeciągnięcie do dolnej krawędzi: stara wersja zostawia kreski, nowa nic.

### Zoom widoku mapy (29.10.2026, jak w Settlers, decyzja użytkownika)

- **Z** – po kolei ×1 → ×2 → ×3 → ×1; **kółko myszy** – stopień bliżej / dalej (RAWKEY 0x7A/0x7B,
  OS 3.2 / NewMouse); pole pod wskaźnikiem zostaje pod nim (życzenie użytkownika; wskaźnik poza
  widokiem: środek widoku). Największy zoom jak w Settlers: 4:3 powyżej 400
  szerokości ×2, od 800 ×3; 16:9 od 960 ×3, 640×360 bez zoomu. Także w grze LAN (zmienia tylko obraz).
- **Jak:** widok rysuje się z mniejszą liczbą pól (logicznie, w SCREEN_1 na zwykłym miejscu),
  a jego kopia do SCREEN_0 jest rozciągana na cały widok (`Layout_ZoomCopy` z `GUI_Screen_Copy`,
  tylko gdy kopiuje widok: licznik `g_zoomCopy` w `viewport.c` i `GUI_DrawInterfaceAndRadar`).
  Okna, panel, radar i kursor zostają 1:1, sterownik obrazu bez zmian (porównanie z kopią,
  blitter). Przewijanie: SCREEN_1 o dx, SCREEN_0 i karta (blitter) o dx·zoom
  (`Layout_ZoomShift`); ostatnie dwie kolumny/wiersze pól rozciągane ponownie (były ucięte);
  miejsce kursora przesunięte blitterem wysyłane ponownie (inaczej ślady kursora).
- **Mysz:** pozycja nad widokiem → piksel obrazu logicznego (`Layout_ViewPoint`) tam, gdzie gra
  zamienia ją na pole: klik w widok, prostokąt zaznaczania, rozkaz prawym przyciskiem;
  „czy nad widokiem” z pełnego widoku (`Layout_InView`).
- `Z` wpisane w pole tekstowe (nazwa zapisu) nie zmienia zoomu (`Layout_ZoomCancel` po edycji).
- Sprawdzone w WinUAE: 640×480 ×2, 800×600 ×3, przewijanie strzałkami, klik, zaznaczanie
  prostokątem, rozkaz, okno Options nad powiększonym widokiem, kółko. Pomiar fps na Amidze – do
  zrobienia (Amiga 2 nie odpowiadała).

## Gra LAN (gałąź `lan-game`, 29.10.2026)

Prośba użytkownika (29.10, w nocy): gra do 3 graczy w sieci, łączenie jak w Settlers; w menu
głównym „Play on LAN” zamiast „Replay Introduction”, okna logicznie jak w Settlers. Serwer
wybiera ród pierwszy (Atreides, Harkonnen, Ordos), pozostali z tego, co zostało, ostatni dostaje
ostatni. Gra może ruszyć już przy 2 graczach. Bez gracza komputerowego. Drugi gracz w WinUAE.

| Krok | Stan |
|---|---|
| Proces sieciowy (`os/net_amiga.c`): serwer + 2 graczy, UDP 6820 ogłoszenie / szukanie, TCP ramki, HELLO/BYE/PING | ✅ |
| Okna LAN w menu (`gui/lan_menu_amiga.c`): START SERVER / JOIN SERVER, gracze, rody, mapa, START GAME | ✅ |
| Lockstep (`lan_amiga.c`): rozkazy przez sieć, tury, suma kontrolna | ✅ 2 Amigi (Amiga 1 + WinUAE) + 3. gracz z PC (`tools/lan_peer.py`) – sumy równe do 75 s gry z rozkazami, budową, oknami |
| Mapy (`tools/lan_maps.py` → `lan_maps.h`) | ✅ 36 map 64×64 z misji: 7 dla 3 graczy, 29 dla 2 |
| Poziom 2–9 (start i technika jak w misji N) | ✅ sprawdzone poziomy 1, 2, 5 (Amiga 1/2 + WinUAE); poziomu 1 nie ma od 29.10: misja 1 nie ma koszar ani fabryki, nikt nie wygra (decyzja użytkownika) |
| Koniec gry: przegrana / wygrana / wyjście (Options → Quit) / serwer zamknięty | ✅ |
| Gra toczy się pod oknami (fabryka, Options, Mentat) | ✅ |
| Walka: czołgi Atreidów niszczą Construction Yard Harkonnenów (wybuchy, piechota z budynku) | ✅ sumy równe do kroku 16800 (4,5 min) |
| Test z trzema Amigami (Amiga 2 niedostępna 29.10) | ⏳ |

### Jak to działa

- **Lockstep jak w Settlers:** każda Amiga liczy tę samą symulację, przez sieć idą tylko rozkazy
  graczy. Tura = 6 kroków gry (100 ms); rozkaz wydany w turze T wykonuje się na początku tury
  T + opóźnienie na każdej Amidze; opóźnienie (2..8 tur) z pingu przy START GAME.
- **Gwiazda:** gracze wysyłają swoje tury serwerowi (`MT_TURN`), serwer, gdy ma turę T od
  wszystkich, wysyła ją wszystkim razem w tej samej kolejności (`MT_TURNALL`, najpierw swoje)
  i wykonuje. Brak tury: gra stoi („LAN: WAITING for …”), obraz i mysz działają.
- **Czas gry:** w grze LAN `g_timerGame` rośnie tylko w `Lan_Step()` (przerwanie zegara go nie
  rusza), 60 kroków/s; liczniki „następnych razów” pętli gry zerowane na starcie (`*_ResetTicks`).
  Eksplozje i animacje (zmieniają mapę i losują) idą w krokach na czasie gry, nie w rysowaniu.
- **Jedna symulacja:** `g_playerHouseID` = ród serwera na każdej Amidze (oryginalny „gracz” w
  symulacji); każda Amiga gra i widzi swój ród (`g_lanLocalHouse`, `LOCAL_HOUSE_ID`); zachowania
  „gracz / komputer” z flagi `House_IsHuman()` (poza grą LAN = `g_playerHouseID`, bez zmian).
  Sojusze: ludzie walczą ze wszystkimi. Komunikaty, dźwięki, kredyty na pasku – dla lokalnego rodu.
- **Mgła wojny każdego gracza osobno** (od 29.10, prośba użytkownika: nie widać wroga, dopóki
  nie dojadą tam nasze jednostki). Symulacja widzi całą mapę (`isUnveiled` wszędzie, to samo na
  każdej Amidze – zależą od tego eksplozje, animacje, ściany). Każda Amiga ma własną mapę
  widzianych pól (`s_seen` w `lan_amiga.c`): okolice jednostek i budynków lokalnego rodu
  (promienie `fogUncoverRadius` jak w oryginale), raz na turę z pozycji, które się zmieniły; pola
  nie zakrywają się z powrotem. Używa jej tylko rysowanie: `TILE_SHOWN()` (`house.h`) w
  `gui/viewport.c` (pola, jednostki, eksplozje, radar, wybór kliknięciem), `rts_amiga.c`,
  `widget_draw.c`; brzegi mgły z `Lan_FogOverlay()`. Sprawdzone: Amiga 1 + WinUAE, każdy widzi
  tylko swoją bazę, jazda odsłania pas terenu, sumy równe.
- **Interfejs nie zmienia gry:** `g_lanSim` = symulacja (krok, rozkazy tury, wczytanie);
  `LAN_UI` = kod interfejsu (także gdy symulacja woła `Unit_Select`, `Map_SetSelection`,
  `GUI_ChangeSelectionType`, `GUI_Widget_ActionPanel_Draw` – opakowane licznikiem `g_lanUI`). Poza
  symulacją: własny generator losowy (`Tools_RandomUI_*`; w `gui/*.c` makra zawsze), bez restartu
  animacji budynków (`Structure_UpdateMap`), bez `Animation_Start` / eksplozji, bez
  `Unit_UpdateMap` z przerysowania interfejsu.
- **Rozkazy** (`Lan_UnitOrder`, `Lan_StructureCommand`, 10 bajtów: typ, ród, 4 × 16 bit):
  rozkaz jednostki (przycisk akcji, klik celu, grupa RTS, prawy przycisk), dewiacja, buduj,
  ulepsz, naprawa/ulepszanie, wstrzymaj/wznów, postaw budynek (Construction Yard). Okno fabryki
  tylko wybiera (`Structure_LanFactoryWindow`). Poza grą LAN rozkaz wykonuje się od razu.
  `Structure_Place` sprawdza sąsiedztwo względem rodu budującego (`g_validateHouseID`).
- **Pod oknami gra idzie dalej:** okna fabryki, Options i Mentata zwiększają `g_lanModal`,
  `sleepIdle()` woła wtedy `Lan_Run()`; z symulacji nic się wtedy nie rysuje
  (`LAN_UNDER_WINDOW`). Skrypt jednostek `UNIT.EMC` jest na Amidze we własnej pamięci (był w
  `SCREEN_2`, który okna nadpisują – pod oknem jednostki traciły skrypty).
- **Suma kontrolna** co 5 s (300 kroków): jednostki, budynki, rody, oba generatory losowe;
  różna: „LAN: OUT OF STEP with …” na pasku. Log `RAM:OpenDUNE-lan.txt` (sumy + stan przy każdej
  sumie). Testy: `debug_lan_trace=1` (stan generatora co krok), `debug_lan_trace_from/_to`
  (kto losuje, adresy – `nm` z `build/amiga/OpenDUNE`).
- **Scenariusz** budowany w pamięci (`Lan_ScenarioText`): teren mapy (Seed, Field, Bloom),
  każdy gracz Construction Yard i kredyty poziomu. **Mapy:** każda z 36 map 64×64 misji; miejsca
  baz z wszystkich misji na tej mapie (różne misje stawiają bazy gdzie indziej), wybrane jak
  najdalej od siebie (≥ 16 pól): 7 map ma 3 miejsca, reszta 2 (START GAME tylko, gdy graczy nie
  więcej niż miejsc). **Poziom N (2–9; 1 usunięty – w misji 1 nie ma budynków wojskowych):** jak początek misji N rodu gracza (pierwszy scenariusz
  poziomu: 001, 002, 005, 008, 011, 014, 017, 020, 022): jego kredyty i jednostki, technika
  `g_campaignID = N − 1`. Jednostki stawia `Lan_PlaceStartUnits()` po wczytaniu (teren już jest):
  pierścienie wokół Construction Yard od drugiego (pierwszy wolny do budowy), najpierw skała,
  pole, po którym jednostka jeździ/chodzi – w symulacji, tak samo na każdej Amidze.
  W grze LAN nie ma Pałacu (jeden pocisk rodu na grę) ani Starportu (jego okno losuje z generatora
  symulacji), zapisu/wczytania (LOAD/SAVE ukryte), pauzy, Restart / Pick another house;
  szybkość gry stała (normalna).
- **Koniec:** ród bez budynków i jednostek przegrywa; ostatni ród z graczy wygrywa; wyjście przez
  Options → Quit playing („You have left the game.”); serwer zamknięty – koniec u wszystkich.
  Potem menu główne.

### Czat (29.10.2026, jak w Settlers, `gui/lan_chat_amiga.c`)

- **Tab** na ekranie mapy gry LAN (bez okna) otwiera okienko w lewym górnym rogu widoku: 8 ostatnich
  linii (nazwa w kolorze rodu), pod nimi pisana linia; **Return** wysyła, **Esc / Tab** zamyka
  (tekst zostaje), **Backspace** kasuje; do 32 znaków: litery, cyfry, spacja, `. , : ? ! -`.
  Linia odebrana przy zamkniętym okienku pokazuje je na 10 s (z dźwiękiem). W grze LAN Tab nie
  wybiera następnej jednostki (Shift+Tab bez zmian).
- Klawisze bierze wprost sterownik obrazu (`Lan_ChatKey`, surowe kody Amigi): przy otwartym
  okienku gra nie dostaje żadnego (Z nie zmienia zoomu, litery nie są skrótami).
- Sieć: `MT_CHAT` = [slot][tekst], od razu, poza turami (nie zmienia gry); serwer ustawia
  nadawcę i przekazuje dalej; bez połączenia notka „NOT SENT - NO CONNECTION”.
- Wygląd (życzenie użytkownika): jak pasek komunikatów u góry ekranu – jego końce, górny i dolny
  brzeg (wiersze 17–38, wzięte raz z ekranu mapy) wokół złotego wypełnienia; tekst ciemny.
- Okienko rysowane na SCREEN_0 po każdej klatce mapy (`Lan_ChatDraw` po `GUI_DrawScreen`); po
  zniknięciu widok rysuje się od nowa. Sprawdzone w WinUAE z `tools/lan_peer.py` (odpowiada „got
  …” po 3 s): wysłanie, odebranie, pokazanie na 10 s, zamknięcie.

### Pauza w grze LAN (29.10.2026, życzenie użytkownika)

**P** u dowolnego gracza: pauza dla wszystkich („Game paused by NAZWA - press P to continue.”),
drugie P (też u innego gracza) – dalej („Game continues.”). Pauzą rządzi serwer: gracz prosi
(`MT_PAUSE` [slot]), serwer przełącza i mówi wszystkim (`MT_PAUSE` [slot][0/1]); w pauzie serwer
nie zaczyna nowych tur, pozostali dochodzą do tej samej tury i czekają na jego turę – gra się nie
zmienia (sumy zgodne), zamiast „WAITING” napis pauzy. Po pauzie czas nie jest nadrabiany
(`s_stepBase`). Sprawdzone: Amiga 2 serwer + `lan_peer.py` (czat „pause” → prośba gracza).

LOAD/SAVE przyciemnione (29.10): widżety przycisków kopiowały flagi MENTAT z `greyWhenInvisible`
– szachownica przy ukryciu zostawała na nich (ich obraz jest w tle paska), w grze LAN po jej
końcu dwa szare prostokąty. Teraz bez wyszarzania.

### Zapis i wczytanie gry LAN (29.10.2026, wariant B – decyzja użytkownika)

- **Zapis – tylko serwer:** Options → „Save this game” (u graczy go nie ma); pliki LAN osobno:
  `_lanNNN.dat`. Przed zapisem serwer pobiera mgłę graczy (`MT_SEENREQ` → `MT_SEEN`, 3 części,
  do 3 s). Do zwykłego zapisu dochodzi chunk **`LAN `** (`Lan_SaveChunk`): mapa, poziom, ród
  „gracza” symulacji, krok, `g_timerGame`, oba generatory losowe (`Tools_Random_GetFull`), kredyty
  bez silosów, gracze (ród, nazwa) i mgła każdego. Zwykły zapis tego nie ma; wczytanie go pomija.
  Zapis w dowolnym momencie (między krokami): stan serwera w kroku S = stan każdej Amigi w S.
- **Wczytanie:** okno serwera, wiersz **Game** `< New game > / < Saved: NAZWA >` (lista
  `_lan*.dat`); mapa i poziom z zapisu; gracze wybierają tylko rody z zapisu (`g_lanSaveMask` w
  lobby, po graczach – starsze odczyty go pomijają), START GAME gdy graczy tylu, ile rodów w
  zapisie. Po START serwer wysyła plik każdemu (`MT_FILE` [rozmiar][offset][≤200 B],
  `MT_FILEEND` [rozmiar][suma FNV]) z postępem „Sending the saved game: NN%” / „Receiving …”;
  gracz zapisuje go jako `_lanrecv.dat` i potwierdza (`MT_FILEOK`); po wszystkich – START
  (bajt „z zapisu”). Wszyscy: `Lan_StartSaved` – wczytanie (`SaveGame_LoadFile` w symulacji),
  potem generatory, czas, krok, `s_turnBase` (pierwsza tura gry po wczytaniu), kredyty, mgła
  swojego rodu, widok na swój Construction Yard.
- Sprawdzone: Amiga 2 serwer + WinUAE – zapis w kroku 3624, wczytanie u obu (ten sam stan
  generatora `2a7f2639`), sumy zgodne w krokach 3900, 4200; mgła i pozycje jednostek wróciły.
  Nie sprawdzone: 3 graczy, zerwanie połączenia w trakcie przesyłania.

### `lan_info=1` (29.10.2026, jak LANINFO w Settlers)

Opcja ini (domyślnie wyłączona): w grze LAN nad licznikiem FPS linia – pozostali gracze, ich czas
odpowiedzi i ostatni krok, w którym sumy były zgodne („same N”; dołączający mierzy tylko serwer),
albo PAUSE / WAITING / OUT OF STEP N (na czerwono). Bez opcji te stany tylko na pasku
komunikatów, gdy się zdarzą. `Lan_InfoDraw` po każdej klatce mapy (jak okienko czatu). Sprawdzone
w WinUAE (serwer + `lan_peer.py`: „LAN PEER 0 ms”, „… PAUSE”); „same” z dwiema Amigami – do
sprawdzenia.

### Okna (320×200, `gui/lan_menu_amiga.c`)

- „Play on LAN”: START SERVER / JOIN SERVER / BACK (bez bsdsocket.library: komunikat).
- Serwer: Server name (MASTER), Server address, Map (`< Arrakis 1 (A014) 3P >`), Level
  (`< 1 > (as mission 1)`) – klik w lewą połowę cofa, w prawą dalej; gracze widzą mapę i poziom,
  Players 1..3 (nazwa, ród, ping), Your house (3 przyciski, zajęte wygaszone), RUN SERVER /
  STOP SERVER, START GAME (≥ 2 graczy i ród serwera wybrany), CANCEL.
- Dołącz: Servers found (SEARCH, 3 s nasłuchu UDP; klik wpisuje adres), Your name (SLAVE), Server
  address (cyfry i kropki), CONNECT, BACK; potem „joined”: gracze, rody, DISCONNECT.
- Wyszukiwanie nie przechodzi przez VPN – tam wpisuje się adres (Amiga 1 192.168.5.115).

### Testy (bez klikania)

- `tools/lan_test.sh [menu-y serwera] [menu-y WinUAE]`: Amiga 1 serwer (Harkonnen), WinUAE
  (`C:\amiga-dev\winuae\OpenDUNE_A1200_OS32_net.uae`, `ODUNE:` = `build/amiga`, `DDATA:` = `data/`,
  ini `tools/opendune-winuae.ini`) dołącza (Atreides), START GAME. `PEER=1`: trzeci gracz z PC
  (`tools/lan_peer.py` – dołącza, wybiera wolny ród, odpowiada pustymi turami).
- `tools/lan_build.sh`: gracz WinUAE wysyła czołgi, buduje płytę i ją stawia.
- Dane gry do WinUAE: `data/` (z `WorkSD:Games/OpenDUNE/data` Amigi 1, nie w repo).

### Co było nie tak po drodze (sumy różne)

1. `g_timerGame` przy starcie różny → ustawiany na 1000, liczniki pętli zerowane.
2. Animacje budynków restartowane przez zaznaczenie i przerysowanie (różnie na każdej Amidze).
3. `GUI_Screen_FadeIn` (radar lokalnego rodu, wołany z symulacji) losował z LCG symulacji.
4. Płyty betonu: sąsiedztwo sprawdzane względem lokalnego rodu (`Structure_Place`).
5. Pod oknem Options: skrypty jednostek w `SCREEN_2` nadpisane → jednostki bez skryptów.
6. Brak `g_lanSim = false` na końcu kroku → interfejs liczony jako symulacja.

Przepełnienie stosu przy starcie z ikony (29.10, raport F10 BUG001 z Amigi 1): czarny pas w
poprzek mapy na serwerze; w `BUG001.DAT` pola 3472–3597 `g_map` zawierały stos gry (adresy
powrotu, pozycja myszy 416,275). Ikona daje 16384 bajty stosu, a `__stack = 400000` nic nie
robiło – kod libnix zmieniający stos (`swapstack.o`, `__stkinit`) dołącza się tylko, gdy coś się
do niego odwołuje. Gra LAN (okna → `Lan_Run` → krok gry) przekroczyła 16 KB i pisała po `g_map`
(suma kontrolna nie obejmuje mapy, więc bez „OUT OF STEP”). Prawdopodobnie ta sama przyczyna:
SmartCrash „line-F” w WinUAE przy SEARCH. Poprawka: odwołanie do `__stkinit` w `os/amiga.c`;
raport F10 podaje teraz rozmiar stosu (z ikony: 400000).

Zawieszenie Amigi 2 przy wyjściu z gry LAN (29.10): po Ctrl-C (i przy każdym wyjściu z gry po
grze LAN, `Net_Quit` w atexit) cała Amiga stała – mysz, AmiAgent. Dziennik wyjścia
(`PROGDIR:exit-log.txt`, linia po linii z zamknięciem pliku, przetrwa reset) pokazał: gra wysłała
procesowi sieciowemu Ctrl-C, a `WaitSelect` Roadshow na Amidze 2 wracał wciąż z sygnałami 0 –
proces (priorytet 1) kręcił się dalej, gra czekała na niego bez końca. Na Amidze 1 i w WinUAE
sygnał dochodził. Poprawka: `Net_Quit` ustawia też flagę `s_quitRequest`, którą pętla procesu
sprawdza po każdym obudzeniu. Sprawdzone na Amidze 2: wyjście czyste, wszystkie kroki w dzienniku.

Napis „LAN: WAITING for …” zostawał na pasku komunikatów, choć gra szła dalej:
`GUI_DisplayText(NULL, -1)` zapomina linie, ale nie zmazuje pokazanej. `GUI_ClearTextLine()`
zamalowuje też pasek; wołane, gdy czekanie się skończy (pod oknem – po jego zamknięciu).
Sprawdzone na Amidze 2 (400×300) z WinUAE.

Pusty „Server address” na Amidze 2: jej stos to **warpSOCKET** (CS-Lab, bsdsocket.library 30.66
z 26.10.2026), nie Roadshow. `tools/amiga/netaddr.c` (29.10) pokazał różnice:
1. gniazdo UDP „połączone” na zewnątrz + `getsockname()` → 0.0.0.0 (Roadshow, WinUAE: własny adres);
2. `gethostbyname(gethostname())` („warp”) – brak adresu;
3. `ioctl SIOCGIFCONF` – działa: `lo0` 127.0.0.1, `wifi0` 192.168.9.187;
4. `WaitSelect` z oczekującym Ctrl-C zwraca −1 i sygnały 0 (WinUAE: 0 i 0x1000) – przyczyna
   zawieszenia przy wyjściu (wyżej).
Gra bierze teraz, gdy 1. da 0.0.0.0, pierwszy interfejs nie-127 z `SIOCGIFCONF`. Sprawdzone na Amidze 2.

### Znane ograniczenia / dalej

- Test z trzema Amigami (Amiga 2) – gdy będzie dostępna.
- Okno LAN w palecie menu (oliwkowe tło); po grze LAN brak ekranu statystyk.
- Zapora Windows: gdy serwerem ma być WinUAE, potrzebna reguła przychodząca TCP/UDP 6820 (jak
  „Settlers LAN 6810” – tylko za zgodą użytkownika).
- Ślad kursora na zakrytym mgłą (czarnym) polu w grze jednoosobowej przy ruchach z `od.py` –
  jest też na `main`, nie od LAN.
