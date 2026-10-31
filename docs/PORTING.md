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
| 3. Muzyka: nagrania muzyki AdLib zrobione raz na PC (`tools/music/make_music.py`), odtwarzane z dysku przez Paulę (`music_amiga.c`), niżej „Muzyka” | ✅ |
| 4. Ikona, numer wydania, opisy, paczka ZIP | ✅ 0.50 31.10 (start z ikony i z paczki sprawdzony) |
| 5. Języki: niemiecki (pliki `.GER` gracza), polski (własny przekład, `lang/POLISH.PAK`, litery w czcionkach), wybór w Game controls – niżej „Języki” | ✅ 30.10, na Amidze 31.10 |

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
  skali w obu kierunkach, na środku z czarną ramką** – ×1 do szerokości 640 (400×300, 640×360,
  640×400, 640×480 – decyzja użytkownika: ×2 było tam za duże), ×2 na 800×600 / 960×540,
  ×3 na 1024×768 / 1280×720; bez nierównych kolumn 2,5× (`Video_SetGeometry`). Sprawdzone w WinUAE
  800×600: menu 640×400 w (80,100), kliknięcie myszą trafia; 640×480: menu 320×200 na środku.
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
  Ikona szuflady gry (ten sam obrazek, okno 400×200) obok katalogu, np. `WorkSD:Games/OpenDUNEWarp.info`:
  `python tools/make_icon.py build/OpenDUNEWarp.info --frame tools/icon/title112.png --crop 0 0 112 70 --lit-frame --drawer`.
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

### Mapa bez efektu pojawiania się, Show FPS w Game controls (29.10.2026, życzenie użytkownika)
- Po wyjściu z trybu stawiania budynku (i innych zmianach wyboru, gdzie gra ustawia
  `g_viewport_fadein`) mapa nie jest już „wsypywana” po kawałku (`GUI_Screen_FadeIn`, ~1 s),
  tylko kopiowana od razu – jak wcześniej przy zoomie ×2/×3 (`viewport.c`).
- Options → Game controls: nowy wiersz „Show FPS” ON/OFF (okno o 17 wierszy wyższe, Previous
  niżej). Wybór w `saves/SHOWFPS.CFG`, przy starcie ma pierwszeństwo przed `show_fps` z ini;
  F8 dalej przełącza licznik doraźnie (bez zapisu). Sprawdzone w WinUAE 640×480.

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
  gry); powrót Lewa Amiga + M. Gra w tym czasie dalej chodzi. Po powrocie Intuition pokazywała
  pasek tytułu ekranu u góry, a okno nie było aktywne (BUG010): gdy ekran gry wraca na wierzch
  (`IntuitionBase->FirstScreen`), `ShowTitle(FALSE)`, `ActivateWindow` i cały obraz od nowa.
  Test: `inject key 0x37 0x40` (Lewa Amiga + M; `inject key` ma teraz kwalifikator).
  Samo `ShowTitle` przy powrocie nie wystarczało: pasek, zamalowany obrazem gry, zostawał na wierzchu
  i brał kliknięcia w górne ~11 wierszy – MENTAT / OPTIONS / LOAD / SAVE działały „raz na jakiś
  czas” (WinUAE: y=4 nie, y=13 tak). Teraz w każdej klatce: ekran z przodu i flaga `SHOWTITLE` →
  `ShowTitle(FALSE)` i obraz od nowa. Amiga 2: 3× róg → Amiga+M → OPTIONS przy y=3 otwiera się.
  Przy wyjściu z gry ekran najpierw za Workbench (`ScreenToBack`), bo po zamknięciu okna Intuition
  na chwilę pokazywała pasek.
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

### Pomiar fps na dużej mapie (29.10.2026, Amiga 1, Warp1260X 68060 @ 100 MHz, 640×360 → 640×400)
Misja 8 Atreides (62×62, `debug_start_scenario=8`), `debug_stats=1`, `framerate=60`, `vram_scroll=1`,
`smooth_units=1`. Przewijanie strzałkami (`od.py key`, ~5 kroków/s), jednostki z `od.py spawn`
(8 quadów + 8 trike'ów w poprzek widoku, odsłaniają mapę). Okresy po 5 s:

| Scena | fps | rysowanie mapy | wysyłanie (`video`) | logika | wolne |
|---|---|---|---|---|---|
| postój (kilka jednostek startowych się rusza) | 29–44 | 4–6% (0,3–0,5 ms) | 6–7% (1,0–1,2 ms) | 2% | 80–82% |
| przewijanie (12–28 przesunięć blitterem / 5 s) | 30–53 | 10–14% (0,8–1,1 ms) | 10–14% (1,6–2,3 ms) | 2,5% | 65–73% |
| 16 jednostek w ruchu + odsłanianie mapy | 54–55 | **39–52% (3,3–4,6 ms)** | 21–23% (3,5–3,8 ms) | 4–5% | **22–30%** |
| przewijanie + 16 jednostek w ruchu | 53–55 | 20–24% (1,6–2,0 ms) | 13–17% (2,0–2,9 ms) | 3–4% | 50–59% |

- Najcięższe jest odsłanianie mapy przez wiele jadących jednostek naraz (rysowanie 4,6 ms na
  wywołanie, 16 KB/klatkę na kartę), a i tak zostaje ~22% wolnego CPU i 54–55 fps przy limicie 60.
- Znaleziony błąd: po przewinięciu w pionie na karcie zostaje kawałek kursora (biała strzałka) pod
  pozycją myszy – zrzut `clutshot` po 6 krokach w dół i 6 w górę. Poprawione, niżej.

### Ciężkie misje 7–9 (31.10.2026, Amiga 1, Warp1260X 68060 @ 100 MHz, polski, Atreides)
`debug_start_scenario=7/8/9`, `debug_stats=1`, `framerate=60`, ekran 640×480 (misja 9 też
800×600). Sceny (okresy po 5 s): postój 20 s; przewijanie strzałkami; bitwa – `od.py spawn`
24 czołgów (po 12 Atreides i Harkonnen, Tank / Siege Tank / Raider Trike) w 6 rzędach naprzeciw
siebie, częściowo w zakrytym terenie; lotnictwo i czerw – 4 ornitoptery, czerw, carryall.

| Misja, ekran | postój | przewijanie | bitwa (jednostki pojawiają się w zakrytym terenie) | ornitoptery + czerw |
|---|---|---|---|---|
| 7, 640×480 | 22–50 fps, wolne 48–82% | 49–50 fps, wolne 39–48% | 34–57 fps, wolne **0**–63%, rysowanie do 13,9 ms | 51–56 fps, wolne 31–56% |
| 8, 640×480 | 38–47 fps, wolne 73–75% | 39–52 fps, wolne 28–74% | 45–55 fps, wolne **0**–58%, rysowanie do 12,7 ms | 53–58 fps, wolne 26–39% |
| 9, 640×480 | 34–38 fps, wolne 72–76% | 37–51 fps, wolne 31–72% | 26–56 fps, wolne **0**–72%, rysowanie do 13,0 ms | 46–57 fps, wolne 38–53% |
| 9, 800×600 | 35–55 fps, wolne 49–73% | 39–47 fps, wolne 23–49% | 35–55 fps, wolne **0**–62%, rysowanie do 13,6 ms | 35–56 fps, wolne 22–59% |

- fps to klatki wysłane na kartę – bez zmian na ekranie (postój, koniec bitwy) jest ich mniej przy
  dużym zapasie CPU; nie jest to spadek tempa gry.
- **Te same 24 czołgi w terenie już odsłoniętym** (misja 7, druga bitwa w tym samym miejscu):
  53–56 fps, wolne 31–44%, rysowanie ~3 ms. Całe CPU (rysowanie 10–14 ms przez ~10–15 s) zabiera
  więc odsłanianie mapy przez kilkadziesiąt jednostek naraz, nie walka. W zwykłej grze jednostki
  wychodzą z odsłoniętej bazy i odsłaniają po kilka pól – jak w pomiarze z 29.10 (16 jednostek:
  54–55 fps, wolne 22–30%). Możliwa optymalizacja na później: koszt rysowania przy odsłanianiu.
- Misja 9 przy 800×600: czołgi Harkonnen dojechały do bazy i ją zniszczyły – „Misja zakończona
  porażką” po polsku, ekran porażki poprawny (pomiar drugiej bitwy przez to odrzucony).

### Pomiar gry LAN (29.10.2026, 3 graczy, `tools/lan_test.sh 154 160` z `PEER=1`)
Serwer Amiga 1 (Harkonnen, Warp1260X @ 100 MHz, 640×360, `lan_info=1`, `debug_stats=1`), WinUAE
(Atreides, 640×480), `lan_peer.py` z PC (bez rozkazów); poziom 2, mapa 0. Pomiary na serwerze:

| Scena | fps | rysowanie mapy | wysyłanie | wolne | tempo gry |
|---|---|---|---|---|---|
| postój | 59–60 | 12% (1,0 ms) | 11% (1,8 ms) | 68% | 58,5 taktów/s (z 60, z narzutem pomiaru) |
| przewijanie strzałkami | 59–60 | 14–19% (1,5 ms) | 12–15% (2,4 ms) | 57–58% | – |
| ruch jednostek serwera i WinUAE (rozkazy przez sieć) | 58–60 | 16–29% (1,3–2,4 ms) | 14–17% (2,5–2,8 ms) | 45–65% | 59,8 taktów/s |

- Przez cały test sumy stanu zgodne („same 13800”), żadnego OUT OF STEP.
- Czas odpowiedzi graczy zwykle 20–80 ms, w chwili wydania rozkazów chwilowo do 364–374 ms.
- 60 fps także na postoju, bo linia `lan_info` jest rysowana w widoku w każdej klatce (300 zmian
  widoku / 5 s), więc koszt wysyłania (~1,8 ms, 11%) jest stały. Bez `lan_info` byłoby taniej.
- Znalezione błędy (do poprawienia):
  - po przewinięciu w pionie linia `lan_info` zostaje skopiowana przez przesunięcie widoku
    (druga, stara linia niżej, nie znika);
  - prostokąt pod linią ma szerokość bieżącego tekstu, więc po krótszym tekście zostają końcówki
    dłuższego (np. „WAITING G3”, „PEER: WAITING ;”);
  - „WAITING” widać na serwerze prawie stale, choć tempo gry jest pełne – to krótkie czekanie na
    turę w każdym kroku, napis nie odróżnia go od prawdziwego zatrzymania.

### Gra LAN 3 graczy: Amiga 1 + WinUAE + Amiberry (30.10.2026, `main` 9455e0c)

Trzech prawdziwych graczy (wcześniej trzeci był `lan_peer.py` z PC): serwer Amiga 1 (Harkonnen,
Warp1260X 68060 @ 100 MHz, ekran 400×300), WinUAE na PC (Atreides, 640×480), Amiberry na Pi400
(Ordos, 640×480, 192.168.5.237, dołącza po adresie 192.168.5.115). Mapa „Arrakis 1 (A014) 3P”,
poziom 2. Na czas testu `debug_stats=1` (i `lan_info=1` na Amidze 1), potem ini przywrócone.
Ten sam program na wszystkich (md5 sprawdzone na Pi). Okresy po 5 s:

| Scena | Amiga 1 (serwer) | WinUAE | Amiberry (Pi400) |
|---|---|---|---|
| postój | 59–60 fps, wolne 71% | 59–60 fps, wolne 80% | 59–60 fps, wolne 74–75% |
| przewijanie strzałkami na serwerze | 59–60 fps, wolne 60–65% (`pan` do 6%) | – | – |
| ruch jednostek wszystkich trzech | 59–60 fps, wolne 58–69% (rysowanie do 1,9 ms) | 58–60 fps, wolne 62–66% | 59–60 fps, wolne 61–70% |

- Rozkazy z każdej maszyny (zaznaczenie ramką, prawy przycisk) widoczne u pozostałych.
- Sumy stanu zgodne co 300 kroków od początku do końca (krok 11 100, ~3 min gry), żadnego
  OUT OF STEP; czasy odpowiedzi w linii LAN 7–44 ms (Pi przez sieć domową 7–18 ms).
- Pełne tempo 60 fps wszędzie (limit `framerate=60`); najmniej wolnego CPU ma serwer przy ruchu
  jednostek (~58%).

### Gra LAN w trzech językach i rozdzielczościach (31.10.2026, życzenie użytkownika)

Serwer Amiga 1 (Harkonnen, polski, 400×300), WinUAE (Atreides, niemiecki, 800×600), Pi400
(Ordos, angielski, 640×480); ten sam program (md5), `debug_stats=1`, `lan_info=1`. Lobby i linia
LAN w językach graczy („zgodne” / „gleich” / „same”). ~6 min gry (jednostki tam i z powrotem,
przewijanie na serwerze), dwa razy: sumy zgodne do końca (krok 13 200–14 400), żadnego OUT OF
STEP, 56–60 fps (WinUAE chwilami 52), wolne CPU najmniej: Amiga 1 36–46%, Pi 64%.

Znalezione i poprawione:
- Pasek w grze LAN (i na małym ekranie) po polsku / niemiecku: obok węższych MENTAT / OPTIONS
  resztki oryginalnych napisów („AT”, „ONS”) – zamalowanie deseniem było tylko przy LOAD / SAVE.
  Teraz zawsze (`Layout_BarPattern`), MENTAT i OPTIONS obok siebie co 5 pikseli.
- Linia `lan_info`: przy płynnym przewijaniu (`smooth_scroll`, domyślnie) jej chowanie kopiowało
  tło przez `Layout_ZoomCopy`, która bierze piksele logiczne – wracało przesunięte o offset,
  zostawały końce liter („SLAVEE WARTE”). Nowe `Layout_RestoreReal` (piksele ekranu, przez offset
  i zoom) dla linii i okna czatu. Pas linii 12 wierszy zamiast 10 (czcionka ma 11: ogonki „g”
  zostawały jako „ms_gleich”). 16 zrzutów linii na każdej maszynie po poprawce – czyste.
- Linia `lan_info` przy 400×300 jest dłuższa niż widok (320 px) i wchodzi na pasek boczny: po
  „CZEKAM” zostawało „EKAM” – `Layout_RestoreReal` przywraca teraz część poza obszarem widoku 1:1
  z SCREEN_1.

### Długa gra LAN z walką (31.10.2026, noc, 67 min, poziom 9)

Ten sam skład (Amiga 1 serwer Harkonnen PL 400×300, WinUAE Atreides DE 800×600, Pi400 Ordos EN
640×480), mapa Arrakis 1 (A014) 3P, poziom 9, `debug_stats=1`. Skrypt (co ~2 min na każdej
maszynie przez `od.py`): radar → własna baza, Construction Yard → budowa / postawienie płyty
(CMD_BUILD, CMD_HOLD, CMD_RESUME, CMD_PLACE przez sieć), zaznaczenie jednostek ramką, radar →
baza następnego gracza, rozkaz; potem armie krążą między bazami (Harkonnen → Ordos → Atreides).
Bazy wszystkich trzech zniszczone, walki armii (komunikaty „Zniszczono: Dewiator Ordos.” /
„Ordos Deviator zerstört.” / „Ordos Deviator destroyed.” w tej samej chwili), na końcu jednostek:
Harkonnen 3, Atreides 25, Ordos 12.

- **Sumy stanu: serwer 1056 porównań, każdy gracz 529 – wszystkie zgodne, żadnego OUT OF STEP,
  nikt nie odpadł** (krok 158 700).
- fps (okresy 5 s): Amiga 1 58–60 (mediana 59), wolne CPU 51–73%; WinUAE 53–60, wolne 12–77%;
  Pi 58–60, wolne 50–74%; żadnego okresu poniżej 50 fps.
- Tempo gry tylko 39 kroków/s zamiast 60 – przez Amiberry na Pi (niżej, „Platforma testowa Pi400”:
  zegar emulacji 66% rzeczywistego); po zmianie `sound_output` 59,5 kroku/s w grze 3 graczy.
- Radar bez Outpost czarny (jak w oryginale) – rozkazy przez klik w radar przesuwają tylko widok.

### Testy przed wydaniem 0.50 (31.10.2026, rano)

| Test | Gdzie | Wynik |
|---|---|---|
| 1. Zapis / odczyt między językami | WinUAE 800×600, misja 7 | zapis PL → odczyt DE i EN, zapis DE, nadpisanie w EN, odczyt w trakcie gry, zapis w środku bitwy 20 czołgów → po odczycie te same jednostki w tych samych miejscach ✅ |
| 2. Rozłączenia w grze LAN | Amiga 1 + WinUAE + Pi400 | A: twarde zerwanie gracza (`pkill -9 amiberry`) – serwer „CZEKAM na SLAVE”, po chwili „SLAVE opuścił grę”, gra idzie dalej ✅; B: wyjście przez Opcje → Menu główne – reszta gra dalej (serwer sam = wygrana) ✅; C: twarde zerwanie serwera (Pi) – obaj gracze w 5 s „Serwer zakończył grę.” ✅; D: z menu bez restartu nowe lobby i gra 3 graczy, sumy zgodne ✅ |
| 3. Start z ikony (`wbstart`, jak dwuklik) | Amiga 1 | stos 400000 (raport F10), brak okna konsoli przy starcie i przy wyjściu (po poprawce), wyjście przez menu ✅ |
| 4. Paczka wydania 0.50 | Amiga 1, `RAM:` | publiczny ZIP (358 KB, bez danych) rozpakowany `UnZip`, dane gracza do `data/`, muzyka do `MUSIC/`: start z ikony, menu „AMIGA 0.50”, katalog `saves` tworzy się sam, misja 1 po polsku z muzyką, zapis, wyjście ✅ |

Znalezione i poprawione:
- **P w polu tekstu** (nazwa zapisu) włączało pauzę i znikało z tekstu: `video_amiga.c` przepuszcza
  P do gry, gdy `GUI_EditBox` jest aktywne (`g_amigaEditBox`, ustawiane w `gui/editbox.c`).
- **Komunikaty końca gry LAN** („You have won the battle!”, „The server ended the game.” …) i
  **błędy sieci w lobby** („Not an address.”, „No answer from the server.” …) były po angielsku:
  `Lang_T` w `GameLoop_LanEnd`, tłumaczenie tekstów `net_amiga.c` przy wyświetleniu (`INDIRECT` w
  `make_port_strings.py`), teksty z liczbą przez `Lang_T` w `net_amiga.c`.
- **Linia LAN u graczy** pokazywała gracza, który wyszedł, jak obecnego: teraz „SLAVE wyszedł” u
  wszystkich.
- **Start z ikony**: „Dziękujemy za grę w Dune II.” na końcu otwierało okno konsoli libnix
  (`PrintToConsole` nie sprawdzało `g_consoleActive`).
- **Data kompilacji w raporcie F10** była stara (`rev.o` nie budował się od nowa): w
  `Makefile.amiga` `rev.o` zależy od wszystkich innych obiektów.

Dalej tego samego dnia (życzenia użytkownika):
- **SAVE na górnym pasku w grze LAN** (tylko serwer; z Options zniknęło „Save this game”):
  `layout_amiga.c` rysuje sam SAVE w miejscu LOAD, `GUI_Widget_SaveGame_Click` z `g_lanModal`
  (gra idzie dalej pod oknem).
- **Rozjazd po zapisie (BRAK ZGODNOŚCI 6600)**: w zrzutach stanu różnił się tylko stan losowań
  (R). Przyczyna: na końcu okna Options / LOAD-SAVE `Structure_Recount()` i `Unit_Recount()`
  układają tablice `g_*FindArray` według indeksu; w grze kolejność zależy od historii (nowa
  jednostka na końcu, choć ma niższy indeks), a symulacja chodzi po nich w ich kolejności – na
  jednej Amidze zmieniała się kolejność kroków jednostek i ich losowań. W grze LAN bez
  przeliczania (nic się nie wczytuje). Po poprawce: Options u gracza + zapis u serwera, sumy
  zgodne do kroku 9900 na 3 maszynach.
- **PAUZA na środku mapy** (`Layout_PauseDraw`, `RTS_PauseDraw`): litery A E P S U Z z Noto Sans
  Bold (OFL) w `gui/pause_font.h` (`tools/make_pause_font.py`), 4 poziomy pokrycia na szarości
  palety, czarna obwódka; zdejmowane jak linia LAN (przed przesunięciem widoku).
- **Lobby**: „Arrakis 1  3P” bez nazwy scenariusza, poziom bez „(jak misja N)”.
- **Małe mapy 32×32** (misje 1–2 każdego rodu, SCENx001–004; wszystkie 36 map LAN to 62×62)
  na środku widoku, gdy widok jest od nich większy (640×480 w poziomie, 800×600, 960×540):
  `Layout_ViewMin` pozwala widokowi zacząć się przed brzegiem mapy, kafle poza mapą czarne,
  ramka na radarze przycięta.
- **Brak danych** (test z publicznym ZIP-em bez data/ na Amidze 1): gra z ikony kończyła się
  bez słowa (komunikat tylko w `RAM:OpenDUNE.log`). Teraz `Amiga_DataCheck` (`opendune.c`, po
  `File_Init`) sprawdza 11 plików .PAK z Install.txt i pokazuje requester (`Amiga_Message`,
  `os/amiga.c`, EasyRequest) z listą brakujących i katalogiem, EN/PL/DE (język jeszcze nieznany).
  MakeMusic bez SOUND.PAK pisał tylko po angielsku, a okno libnix (`CON://///AUTO/CLOSE/WAIT`)
  po „Press Return” zostawało otwarte do kliknięcia zamknięcia. Teraz własne `__stdiowin` bez
  WAIT (Return zamyka) i komunikaty (brak SOUND.PAK, pełny dysk) w trzech językach.
  Pełny przebieg MakeMusic z ikony na Amidze 1: 34 nagrania + 3 ciche, 2048 s.
- **Zanik gracza LAN (zawieszenie, reset)**: zawieszona Amiga nie zamyka połączenia, a sieć
  nie liczyła ciszy – wszyscy stali na „CZEKAM na …” bez końca. Teraz (`net_amiga.c`) peer bez
  żadnych danych przez `NET_SILENT_MS` = 10 s (pingi co 1 s) jest utracony (nie przy
  `s_rxBlocked`). Serwer (`Lan_PlayerGone`, nie „quit”) pauzuje wszystkich: „Brak połączenia z X.
  ZAPIS zapisuje, P gra dalej.” (powtarzane co 8 s, `s_lostSlot`); `Lan_CheckEnd` w pauzie nie
  ogłasza wygranej (2 graczy). Utracony gracz (`lost`) zostaje w zapisie LAN z mgłą, którą
  przysyła serwerowi co 30 s (`Lan_SendSeen`), więc po jego powrocie zapis wczytuje się dla
  wszystkich. Gracz, który stracił serwer: „Utracono połączenie z serwerem.” (koniec 5).
  Test (Pi zamrożone `kill -STOP` – połączenie zostaje otwarte jak przy zawieszeniu): pauza na
  Amidze 1 i WinUAE po ok. 10 s, ZAPIS w pauzie, wczytanie w lobby przez 3 maszyny od kroku 2124,
  sumy zgodne; 2 graczy: pauza zamiast wygranej, po P „Bitwa wygrana!”.

Uwagi: dwa domyślne imiona graczy „SLAVE” w jednym lobby nie przeszkadzają grze, ale komunikat
„SLAVE opuścił grę” nie mówi, który; 77 MB pełnego ZIP-a nie przejdzie przez AmiAgenta (limit
czasu), stąd test z publicznym ZIP-em i danymi skopiowanymi na Amidze.

### Ślady po przewijaniu blitterem, linia `lan_info` (29.10.2026, poprawione)
- Przyczyna śladów na karcie (kursor, przez chwilę licznik FPS o kafel obok): blitter przesuwa
  w VRAM to, co karta pokazywała – także kursor, który gra właśnie zdjęła (miejsce oznaczone jako
  zmienione w starym położeniu), zmiany jeszcze niewysłane i licznik FPS. Poprawka
  (`Video_DirtyMoved`, `video_amiga.c`): po przesunięciu oznaczenia bloków 16 px w widoku są
  dodawane także przesunięte o dx, dy (plus pole licznika FPS w nowym miejscu) – te miejsca są
  porównane z kopią i wysłane ponownie.
- Linia `lan_info` i okno czatu są rysowane w SCREEN_0 nad mapą; `GFX_ShiftViewport` przesuwał je
  razem z mapą, także do SCREEN_1 (stąd druga, trwała linia). Przed przesunięciem widoku
  `Lan_InfoHide` / `Lan_ChatHide` odtwarzają mapę pod nimi z SCREEN_1 (przy zoomie rozciągniętą),
  następna klatka rysuje je od nowa. Krótsza linia najpierw zdejmuje dłuższą (bez końcówek).
- „WAITING” w linii dopiero po 300 ms czekania (jak napis na pasku komunikatów).
- Sprawdzone w WinUAE 640×480 (misja 8 i gra LAN z `lan_peer.py 127.0.0.1`): przewijanie
  w pionie, poziomie, po skosie, przy krawędzi z otwartym czatem i przy zoomie ×2 – obraz karty =
  obraz gry (różnią się tylko licznik FPS i migająca ramka zaznaczenia); 8 zrzutów karty w trakcie
  ciągłego przewijania bez pikseli licznika poza jego miejscem.

### F10 na każdym ekranie (29.10.2026, życzenie użytkownika)
- Raport pisała tylko główna pętla gry, więc F10 w ekranach z własną pętlą (wynik misji, Hall of
  Fame, odprawa, okna Options/Load/Save, menu) czekał do powrotu na mapę i pokazywał inny moment.
  Teraz `AmigaBug_Tick` jest wołany z `Video_Tick` (każdy ekran, zadanie gry, między krokami
  logiki), zaraz po obsłudze klawiszy. Zapis gry `bugNNN.dat` jak dotąd tylko w trakcie misji.
- Sprawdzone w WinUAE: F10 w menu głównym i w Hall of Fame dają raport od razu.

### Muzyka (29.10.2026, decyzja użytkownika: nagrania robione raz, dokładane do katalogu gry)
- Wcześniej cisza (`midi_none.c`, „Music is ON” bez skutku). Dane PC mają muzykę jako AdLib
  (`.ADL`), General MIDI (`.XMI`) i MT-32 (`.C55`); wybrana wersja AdLib (Sound Blaster), bo da
  się ją nagrać bez ROM-ów MT-32 i bez banku brzmień.
- `tools/music/make_music.py` (raz, na PC): wyciąga `.ADL` z `SOUND.PAK`, w Dockerze buduje
  `render_adl.cpp` (AdPlug, odtwarzacz Westwood ADL + emulator Nuked OPL3) i nagrywa każdy z 37
  utworów `g_table_musics` (plik, numer = indeks gry) → `build/music/DUNE<n>_<nr>.RAW`:
  nagłówek `ODMU`, początek pętli, długość (32 bity, big endian), potem 8 bitów ze znakiem,
  mono, 22030 Hz (okres Pauli PAL 161), jedno wzmocnienie dla wszystkich. Pliki z danych gry –
  nigdy w repo (`build/`); kopiuje się je do katalogu gry jako `MUSIC` (`PROGDIR:MUSIC`, ini
  `music_dir`).
- Utwory, które oryginał zapętla (8 z nich, w tym menu), są nagrane raz z punktem powrotu.
  Pętla szukana po nutach uderzanych w każdym takcie (72 Hz): sterownik Westwoodu ma efekty
  losowe, więc nuty powtarzają się w 80–100%, nie wszystkie zapisy do OPL; wybrany najkrótszy
  okres z dopasowaniem w granicach 1 punktu od najlepszego. Pętle 43–183 s.
- Wynik: 33 pliki, 74,6 MB, 56 min. Cztery pozycje są w wersji AdLib puste (`DUNE17_4`,
  `DUNE1_8`, `DUNE0_3`, `DUNE0_5`) – brak pliku, gra wtedy nic nie gra.
- `audio/music_amiga.c`: dwa kanały Pauli, które zostawiły głosy (lewy i prawy, te same próbki),
  4 bufory po 0,5 s w Chip RAM w kolejce `audio.device`; bufor odegrany na obu kanałach jest
  czytany z pliku od nowa w `Music_Amiga_Tick` (z `Video_Tick`, każdy ekran). Pętla: `Seek` do
  początku pętli. Wyciszanie (`Driver_Music_FadeOut`) 2 s, głośność ini `music_volume`
  (0–64, domyślnie 40). Podpięte w `Driver_Music_Play/Stop/IsPlaying/FadeOut`; losowanie
  utworów w misji nie zależy już od MIDI (`g_enableSoundMusic`).
- Sprawdzone: WinUAE – menu `DUNE7_6`, odprawa `DUNE7_3`, mapa losowy utwór, Music OFF zatrzymuje,
  ON po chwili losuje następny; pozycja w nagraniu rośnie w czasie rzeczywistym (`od.py --state`,
  linia `music`). Amiga 1: to samo w menu. Odsłuch i szwy pętli – użytkownik.
- Pi400 (Amiberry): nagranie wyjścia HDMI (`pw-record` z monitora ujścia) – muzyka zgodna
  z plikiem `DUNE7_6` (korelacja obwiedni 0,95), mowa i efekty przy wyłączonej muzyce słychać;
  użytkownik potwierdził na słuch.

### Pasek tytułu na ekranach 320×200 (29.10.2026, raporty BUG007/008 z Amigi 1)
- Na końcu misji (ekran 320×200 na środku 640×480) u góry został pasek tytułu ekranu „OpenDUNE”.
  Gra chowa pasek (`ShowTitle(FALSE)`) i wysyła obraz od nowa, ale tylko obszar obrazu gry: na
  mapie (cały ekran) pasek znikał, przy mniejszym obrazie na środku zostawał w czarnym tle.
- Poprawka: po schowaniu paska i po powrocie z Workbencha najpierw `SetRast` całego okna, potem
  cały obraz od nowa. W WinUAE paska po powrocie nie dało się wywołać (także bez poprawki); na
  Amidze 1 przejście menu → Workbench → Amiga+M daje czyste górne wiersze. Do potwierdzenia przez
  użytkownika na ekranie końca misji.

### MakeMusic – nagrywanie muzyki na samej Amidze (30.10.2026, życzenie użytkownika)
- `tools/music/amiga/`: `makemusic.cpp` + z AdPlug 2.3.3 `adl.cpp/adl.h` (odtwarzacz Westwood ADL),
  `emuopl.*`, `fmopl.*` (emulator OPL2 z MAME, liczby całkowite; tablice raz przez double), własne
  zastępcze `player.h`, `fprovide.h` (plik ADL z pamięci), `debug.h`. LGPL-2.1 (zgodna z GPL).
  `build.sh` → `build/amiga/MakeMusic` (bebbo g++ 6.5, -O1, -mtune=68060, bez FPU), 182 KB.
- Ten sam algorytm co `make_music.py`: 37 utworów `g_table_musics`, wprost 22030 Hz, jedno
  wzmocnienie dla wszystkich (szczyt emulatora 16331 → 127 przy stałej 16384), pętla po nutach
  uderzanych w takcie (najkrótszy okres w granicach 1 punktu od najlepszego, start od 3 punktów),
  utwory zapętlone nagrywane do 400 s (pętle do 183 s). Ten sam format `ODMU`.
- Ścieżki: z ikony `datadir=` i `music_dir=` z `PROGDIR:opendune.ini` gry, domyślnie
  `PROGDIR:data` → `PROGDIR:MUSIC`; w Shellu `MakeMusic KATALOG_DANYCH KATALOG_MUZYKI`
  (bez „/” po nazwie woluminu); z ikony okno czeka na Return. Ikona 64×40 z ekranu tytułowego.
- Wynik wobec PC (Nuked OPL3): te same długości i pętle co 33 nagrania z `make_music.py`, obwiednie
  0,94–0,99 (0,67–0,75 tam, gdzie efekty losowe sterownika poszły inaczej), ta sama głośność;
  jeden utwór więcej – `DUNE17_4` (dźwięk 162, muzyka gry nr 5), na którym AdPlug z Nuked się
  wysypywał. Razem 34 nagrania, 3 ciche.
- Amiga 1 (Warp1260X 68060 @ 100 MHz): całość **2034 s (34 min)**, ok. 3× szybciej niż czas
  odtwarzania (utwór zapętlony, 400 s dźwięku: 115–149 s). Pliki z Amigi identyczne bajt w bajt
  z wersją PC tego samego programu; gra odtwarza je (`music_dir=RAM:mm`, menu: `DUNE7_6`).
- W paczce wydania (`tools/make_release.sh`) razem z ikoną; opisy dla graczy: „dwuklik na
  MakeMusic”, droga przez PC jako alternatywa.

### Platforma testowa Pi400 + Amiberry (29.10.2026)
- Raspberry Pi 400 (Debian 13, labwc), Amiberry 8.5.0, konfiguracja jak WinUAE:
  `~/Amiberry/Configurations/OpenDUNE_A1200_OS32_net.uae` (68060 + JIT, Z3 256 MB, RTG 32 MB,
  `bsdsocket_emu`), dyski `System` (kopia systemu z WinUAE z AmiAgentem), `OpenDUNE` (= `ODUNE:`,
  kopia `build/amiga` z `MUSIC`), `DuneData` (= `DDATA:`). AmiAgent: `amiagent-pi.cfg` (poza repo).
- Start i restart: `~/bin/amiberry-opendune.sh`; autostart po zalogowaniu
  (`~/.config/autostart/amiberry-opendune.desktop`).
- Workbench wracał do PAL 640×256: zapisany tryb WinUAE `UAE:1024x768` 32 bit BGRA (ID
  `77041303`) nie istnieje w Amiberry (32 bity jako RGBA, `77041302`); w kopii
  `screenmode.prefs` ID zmienione – Workbench w RTG 1024×768.
- **Zegar Amiberry wolniejszy od rzeczywistego (31.10.2026, znalezione w długim teście LAN):**
  z `sound_output=exact` emulacja szła w tempie wyjścia dźwięku Pi bez monitora (wyjście
  „Headless”) – 40 s czasu Amigi na 61 s rzeczywistych (66%), statystyki gry 2359 s przy 3614 s
  Amigi 1. W grze LAN wszyscy idą w tempie najwolniejszego: 39 kroków/s zamiast 60. Teraz
  `sound_output=normal` (kopia starej konfiguracji `.uae.bak-exact`): czas Amigi = rzeczywisty
  (60 s na 61 s w grze, Amiberry 100% jednego rdzenia), gra LAN 3 graczy 59,5 kroku/s.
- Raz (31.10.2026 02:4x, zaraz po zmianie `sound_output`, przy starcie gry LAN) SmartCrash na Pi:
  „Exception B line-F emulator”, PC 0x0004BCB8 (Chip RAM – skok w nie-kod), `CLI[9]:
  ODUNE:OpenDUNE`; nie wiadomo, czy przy wyjściu poprzedniego (Ctrl-C po minucie misji), czy przy
  starcie nowego. 9 prób powtórzenia (start, misja 1, 10–70 s gry, Ctrl-C, 3–8 s przerwy, start)
  bez awarii. Do obserwacji – na prawdziwych Amigach nie widziane.

### Płynne przewijanie co piksel (29.10.2026, decyzja użytkownika: widok o 1 kafel mniejszy)
- `smooth_scroll=1` (domyślnie; 0 = jak dawniej, co kafel). Gra rysuje widok w całych kaflach do
  SCREEN_1 (obraz logiczny, kafel więcej niż widać), ekran pokazuje go z przesunięciem 0..16
  pikseli (na dalekich krawędziach mapy więcej) – tym samym mechanizmem co zoom (`Layout_FillReal`,
  `Layout_ZoomCopy`, `Layout_ViewPoint`), także przy ×1. Widok na ekranie jest o 16 pikseli
  węższy i niższy (640×480: 544×416), pas przy panelu i na dole czarny.
- `Map_MoveDirection` (strzałki, krawędź ekranu, przyciski krawędzi) przesuwa tylko cel o 16
  pikseli (`Layout_ScrollStep`); `Layout_ScrollTick` (RTS_Tick, każde przejście pętli) dociąga
  widok do celu z prędkością 8 × odległość / s (min. 60 px/s): przesunięcie, a po przekroczeniu
  kafla przesunięcie widoku gry o kafel (`Layout_ZoomShift`, przesunięcie obrazu = ruch kafla –
  zmiana przesunięcia, zwykle 0). Ruch widoku przez grę (radar, jednostka, zoom) – przejęty bez
  przesunięcia.
- Koszt na prawdziwej Amidze (Amiga 1, Warp1260X 100 MHz, 640×360, misja 8, ciągłe przewijanie
  przy krawędzi; `debug_stats`, nowe części `pan` = przesunięcie w SCREEN_0, `v.scroll`, `v.blit`):

| | fps | pętla/s | wysyłanie (`video`) | `pan` | `v.scroll` / `v.blit` | wolne |
|---|---|---|---|---|---|---|
| postój | 44–47 | 120 | 9% (1,5 ms) | 0,4% | – | 73–76% |
| pierwsza wersja | 34–40 | 64–84 | 38–41% (8,8–9,6 ms) | (w other 13–15%) | – | 24–27% |
| + kopie wierszy długimi słowami (`GFX_MoveRow`) | 28–43 | 92 | 25–27% | 7–8% | 12 ms / 0,4 ms | 47–52% (poziomo) |
| + kopia karty nie przesuwana, oznaczenia tylko zaznaczonych bloków | **43–55** | **94–109** | **15–18% (2,5–3,1 ms)** | 17–30% (2,6 ms) | 0,5 ms / 0,4 ms | **30–40%** |

- Wnioski z pomiarów: blitter karty przesuwa widok w 0,4–0,5 ms (krok 1, 4 czy 8 pikseli – bez
  różnicy); drogie było przesuwanie w pamięci kopii karty (~165 KB, kopia Fast→Fast na Warpie
  ~42 MB/s) i przeliczanie oznaczeń bloku po bloku. Teraz kopia karty po przewinięciu nie jest
  przesuwana: w obszarze widoku zmienione bloki idą bez porównania, 300 ms po ostatnim
  przewinięciu raz cały obraz (`s_unsynced`); oznaczenia niewysłanych zmian przesuwane w chwili
  przewinięcia, tylko zaznaczone bloki, o dowolną liczbę pikseli (`Video_DirtyMoved`).
- Zostaje przesunięcie obrazu gry w SCREEN_0 (~4 ms na krok); usunąć je dałoby się tylko
  przesunięciem obrazu w sterowniku (SCREEN_0 bez przesunięcia), ale wtedy okna, kursor i ramki LAN
  rysowane w SCREEN_0 byłyby przesunięte – nie robione.
- Sprawdzone w WinUAE (misja 8, ×1 i ×2): obraz po przewijaniu = pełne przerysowanie (różni się
  tylko migająca ramka zaznaczenia), karta = obraz gry, w 6 zrzutach karty w trakcie przewijania
  bez śladów kursora, klik po przewinięciu trafia w jednostkę.

### Wydanie 0.46, GitHub (29.10.2026, decyzje użytkownika)
- Numer wydania w jednym miejscu: `port/VERSION` (0.46 = liczba commitów, beta); Makefile
  wstawia go do `rev.c` (`g_opendune_revision`, raporty F10) i `$VER` (`Version OpenDUNE`), menu
  główne pokazuje „Amiga 0.46” w lewym dolnym rogu.
- Publiczne repo `github.com/cizar-szymon/Amiga_game_OpenDUNE`: źródła + `releases/amiga`
  (program, ikony, `opendune.ini`, opisy PL/EN dla gracza, historia wydań, `COPYING` – GPL-2.0,
  puste `data/` i `MUSIC/` z ReadMe) i `OpenDUNE-<v>.zip`. Bez danych Dune II i bez nagrań muzyki
  (prawa autorskie): opis, skąd wziąć dane (własna kopia 1.07, lista plików, bez linków) i jak
  nagrać muzykę (`tools/music/make_music.py`).
- Pełna paczka tylko lokalnie: `private/OpenDUNE-<v>-full.zip` (+ `data/*.PAK`, `MUSIC/*.RAW`),
  `private/` poza gitem.
- `tools/make_release.sh` robi oba; ZIP (wybór użytkownika zamiast LHA) przez `tools/make_zip.py`
  z uprawnieniami Unix (program 755): bez nich Amiga UnZip rozpakowuje program bez bitu E i nie da
  się go uruchomić. Sprawdzone w WinUAE: UnZip 5.5 → `----rwed`, gra z rozpakowanej pełnej paczki
  startuje, dane i muzyka (`DUNE7_6`) działają.
- (LhA w WinUAE odrzucone: uruchomiony w tle bez konsoli kończy się błędem 20, zablokowane pliki po
  przerwanych próbach, pełna paczka trwałaby długo.)

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

## Języki: polski i niemiecki (gałąź `lang-pl-de`, 30.10.2026, życzenie użytkownika)

Wszystkie teksty gry po angielsku, niemiecku albo polsku; wybór w Options → Game controls →
**Language** (nowy wiersz pod Show FPS, każde kliknięcie: następny język, którego pliki są).

### Decyzje (użytkownik)

- **Niemiecki** = oryginalne teksty Westwood z danych gracza (nie ma ich w repozytorium).
  Pliki `.GER` w katalogu danych (luzem obok `.PAK` albo w `GERMAN.PAK`). Przetłumaczone
  (w programie) są tylko własne teksty portu.
- **Polski** = własny przekład wszystkich tekstów: gra (odprawy, komunikaty, nazwy i opisy,
  pomoc mentata, napisy intro i zakończeń, teksty mapy między misjami, napisy końcowe) i port.
  Polskie litery dorysowane do czcionek gry.

### Pliki `.GER` z Amigi (sprawdzone `tools/lang/check_lang.py`)

| Plik | Format wobec PC | Co robi gra |
|---|---|---|
| DUNE, MESSAGE, INTRO, TEXTA/H/O, PROTECT | jak PC (kodowanie CP437, ten sam skrót par liter), tylko **tabela przesunięć big endian** | `String_Load` rozpoznaje kolejność bajtów (`String_IsBigEndian`: pierwsze przesunięcie = rozmiar tabeli, parzyste, w pliku) |
| MENTATA/H/O | IFF FORM MENT – jak PC | bez zmian |
| CREDITS | tekst jak PC (lista ekipy wersji Amiga) | bez zmian |
| BTTN, CHOAM, MENTAT (SHP), AND, HERALD, MISC, TITLE (CPS) | **inny format**: obrazy planarne 5 bitplanów (40000 bajtów, paleta 32 kolory), kształty big endian | `String_GenerateGfxFilename` bierze `.GER` tylko w formacie PC (CPS: 64000 bajtów obrazu; SHP: liczba kształtów LE < 256), inaczej `.ENG` |
| SCREENA/H/O, NEW6P, DUNEGAME, INTRO1A | tylko Amiga (ekrany, czcionka Amigi, muzyka) | nieużywane; czcionka niemiecka: `NEW6PG.FNT` z `DUNE.PAK` (PC) |

Liczby: angielski 941 identyfikatorów tekstów (939 z treścią), niemiecki 899 (PROTECT ma 374
napisy zamiast 416 – mniej pytań zabezpieczenia; identyfikatory 0–524 te same), MENTATA/O po
niemiecku 49 tematów (angielskie 47, kolejność alfabetyczna niemiecka – plik czytany sam w sobie).

Głosy: `Sound_VoiceLanguage()` – francuskie/niemieckie tylko, gdy są ich pliki (`FAFFIRM.VOC`,
`GAFFIRM.VOC`, `GERMAN.PAK` PC); inaczej angielskie (Amiga `.GER` ich nie ma, polski też).
Wcześniej przy `language=german` gra szukała głosów `G*.VOC`, których w danych Amigi nie ma.

### Polski: gdzie co jest

| Co | Gdzie |
|---|---|
| Przekład (źródło, UTF-8) | `tools/lang/polish/*.txt` – DUNE, MESSAGE, INTRO, TEXTA/H/O (numer = numer napisu w pliku `.ENG`), MENTATA/H/O (tematy pomocy), REGIONA/H/O (teksty mapy), CREDITS |
| Plik dla gry | `tools/lang/make_polish.py` → `build/lang/POLISH.PAK` (DUNE.POL … w układzie plików `.ENG`, REGION?.POL – ini z kluczami `POLTXTn`); na Amidze `PROGDIR:lang/POLISH.PAK` |
| Ładowanie | `file.c`: PAK-i z `PROGDIR:lang` rejestrowane pełną ścieżką (`_File_Init_LangCallback`, `File_MakeCompleteFilename` z `:` w nazwie bierze ją wprost); `String_GenerateFilename` – plik języka albo `.ENG`, gdy go nie ma |
| Litery | `port/src/gui/font_polish.c` – rysunki w kodzie, kodowanie **ISO-8859-2** |
| Teksty portu (DE i PL) | `tools/lang/port_strings.txt` → `make_port_strings.py` → `port/src/lang_strings.h`; w kodzie `Lang_T("...")` (`lang_amiga.c`) |

Nie przetłumaczone (świadomie): pytania zabezpieczenia przed kopiowaniem (`PROTECT`, tylko z
`copy_protection=1`; odpowiedzi są w angielskiej instrukcji) – gra bierze `PROTECT.ENG`.
Ekran tytułowy zostaje angielski. Przyciski z napisami – patrz „Napisy na przyciskach” niżej.

Kolejność słów: po polsku nazwa przed rodem, jak po francusku („Czołg Atreides”, trzy miejsca:
`script/structure.c`, `script/unit.c`, `unit.c`); komunikaty bez rodzaju gramatycznego
(„Zniszczono: %s %s.”, „Rafineria Atreides – zniszczono.”, „ – ładunek %d%%”). Podpowiedź o
czerwiach (oryginał: tylko po angielsku) także po polsku.

### Litery (czcionki 8-bitowe, ISO-8859-2)

| | ą | ć | ę | ł | ń | ó | ś | ź | ż | Ą | Ć | Ę | Ł | Ń | Ó | Ś | Ź | Ż |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| kod | B1 | E6 | EA | B3 | F1 | F3 | B6 | BC | BF | A1 | C6 | CA | A3 | D1 | D3 | A6 | AC | AF |

Kody poza CP437 niemieckim/francuskim (0x80–0x9A, 0xE1), więc nie kolidują z umlautami.
Czcionki mają po 226 znaków (NEW6P, NEW6PG, NEW8P) – `Font_LoadFile` robi zawsze 256 (reszta
pusta, `Font.count` uint16), więc każdy bajt tekstu da się narysować. Polskie litery dokłada
`Font_AddPolish` (tylko przy języku polskim):

- rysunek to sama „twarz” litery (`#`, kolor 1); jasna krawędź (3) i cień (2) dokładane
  regułą `Font_Emboss`, którą sprawdziłem na oryginale: daje dokładnie 47 z 52 liter A–Z a–z
  NEW6P i NEW8P (reszta różni się pojedynczym pikselem cienia);
- NEW6P (8 wierszy, same wielkie litery): akcent w wierszu 0, litera wiersz niżej i o wiersz
  niższa – jak Ä Ö Ü w niemieckiej NEW6PG Westwood; ogonek w wierszach 6–7; małe = wielkie;
- NEW8P (11 wierszy): akcent małych liter w wierszach 0–1 jak w „é”, wielkie z akcentem jak
  „É” (litera o wiersz niższa), ogonek pod linią jak w „ç”;
- inne czcionki (INTRO.FNT z INTRO.PAK, gdy jest): litera bez znaku.

Obrazy: `tools/lang/render_fonts.py DATA build/lang` → `build/lang/polish_glyphs.png`
(litery powiększone obok zwykłych), `build/lang/polish_text.png` (zdania jak w grze, styl 0x22).

### Wybór języka, zapamiętanie

- Options → Game controls: okno o dwa wiersze wyższe (Show FPS, Language; 168 wierszy,
  y 31–199), widżety -33 (nazwa języka w nim samym: English / Deutsch / Polski) i -34 („Language”).
  `Lang_Next` pomija języki bez plików (`String_LanguageAvailable`: `DUNE.GER`, `DUNE.POL`).
- Zapis: `saves/LANGUAGE.CFG` (`POLISH` / `GERMAN` / `ENGLISH`); kolejność przy starcie:
  `dune.cfg` → `language=` z ini → `LANGUAGE.CFG` (`Lang_Init` po `SetLanguage_From_IniFile`);
  język bez plików → angielski.
- **Zmiana w menu głównym**, nie od razu: `Lang_ApplyChosen()` w `GameLoop_GameIntroAnimationMenu`
  wczytuje teksty i czcionki od nowa (`String_Uninit/Init`, `Font_Uninit/Init`), menu rysowane
  od nowa. W trakcie misji wiele miejsc trzyma wskaźniki do tekstów (pomoc mentata, przyciski,
  komunikat), więc zmiana w locie byłaby ryzykowna; po wyborze w grze komunikat „The language
  changes in the main menu.” Obrazki z napisami i głosy (tylko z danymi PC w innym języku)
  zmieniają się dopiero po ponownym uruchomieniu.
- Górny pasek LOAD/SAVE: duże litery składane z liter przycisków MENTAT/OPTIONS znają tylko
  L O A D S V E – w innych językach napis czcionką 8 px (styl 1 lub 0).

### Sprawdzenie bez Amigi

`python tools/lang/check_lang.py data build/data_ger` (dane gracza tylko do odczytu):
niemieckie pliki tą samą logiką co `String_Load` (kolejność bajtów, pomijanie pustych,
numeracja), polski PAK ładowany jak w grze (brak PROTECT → ENG): 941 identyfikatorów jak po
angielsku, 0 braków, te same `%d %s` w każdym napisie, te same znaki `\r`, tematy mentata z
tymi samymi flagami i animacjami, wszystkie klucze mapy; szerokości w pikselach napisów z
miejsc o stałym rozmiarze (przyciski, panel boczny – nie szersze niż angielski/niemiecki albo
znane miejsce: 64 px panelu, 80 px przycisków) i tekstów portu (`px:` w `port_strings.txt`).
Na koniec ten sam kod gry na PC: `tools/lang/host_check.c` (kompiluje `string.c`, `gui/font.c`,
`gui/font_polish.c` gcc z MSYS2, `HOST_GCC=` inny) czyta te same pliki – ENG 941, GER 899,
POL 941 napisów identycznych z modelem w Pythonie, nazwy obrazków (`.GER` z Amigi → `.ENG`),
36 z 36 polskich liter w NEW6P/NEW8P zgodnych z rysunkami.
Wynik 30.10: 0 błędów, 8 ostrzeżeń o szerokości (wszystkie sprawdzone: czcionka 8 px tam, gdzie
gra rysuje 6 px, albo miejsce szersze). Obok siebie EN | PL: `build/lang/polish_review.txt`.
Bufor tekstów: polski 36 865 bajtów (`MAX_CHARACTER_COUNT` podniesiony z 48 do 60 KB,
`String_Load` sprawdza teraz miejsce).

### Czy przekład może być publiczny – decyzja użytkownika

Przekład to tłumaczenie tekstów Westwood (utwór zależny). **Decyzja użytkownika (30.10.2026):
polski tylko lokalnie** – `tools/lang/polish/` jest w `.gitignore` (nie ma go w publicznym
repozytorium ani w historii `main`, gałąź `lang-pl-de` tylko na tym PC), `make_release.sh` dokłada
`lang/POLISH.PAK` wyłącznie do pełnej paczki w `private/`. Publiczne są litery (`font_polish.c`),
teksty portu (`port_strings.txt`) i narzędzia.

### Sprawdzone w WinUAE (30.10.2026, main po scaleniu, 0.49)

- Game controls: nowy wiersz Język / Language, okno wyższe o wiersz mieści się; kolejne kliknięcia
  Polski → English → Deutsch, wybór zapisany w `saves/LANGUAGE.CFG`.
- Zmiana po Options → Main menu: menu główne po polsku („Nowa gra”, „Gra w sieci”, „Galeria
  sławy”, „Koniec gry”) i po niemiecku („Spielen”, „Spiel im Netzwerk”, „Ruhmeshalle”).
- Polski: tytuł rodu, odprawa mentata, misja (WCZYTAJ / ZAPISZ, „Plac budowy”, „Uszk”,
  „Buduj”), Options, Ustawienia gry, O Amidze, pytanie przy wyjściu – polskie litery czytelne.
- Niemiecki z plików `.GER` wersji Amigi: opis rodu, odprawa („Willkommen.”), misja (LADEN /
  SICHERN, „Bauanlage”, „Schad”).
- Po angielsku zostały obrazki z napisami: MENTAT, OPTIONS, Credits, „Do you wish to join
  House …?”, Yes / No, Proceed / Repeat – zgłoszenie użytkownika, poprawione niżej.

### Napisy na przyciskach (30.10.2026, zgłoszenie użytkownika)

Przyciski z napisami to obrazki: BTTN (MENTAT, OPTIONS, Credits), CHOAM (Upgrade, Send Order,
Build This, Resume Game, Invoice), MENTAT (Yes, No, Exit, Proceed, Repeat), po dwa (zwykły i
wciśnięty), razem 25. Język bez własnych obrazków (polski; niemiecki z Amigi – planarne `.GER`)
dostaje angielskie z nowym napisem: `Lang_LetterButtons` (`lang_amiga.c`) rysuje oryginał w
SCREEN_3, zamalowuje środek kolorem tła przycisku (złote: wnętrze ramki, wiersze 2–12; mentata:
wiersze 2–21; „Credits”: litery zastąpione deseniem paska z lewego końca), pisze tekst gry
(`STR_YES`, `STR_PROCEED`, `STR_UPGRADE` …; MENTAT / OPTIONS / Credits z `port_strings.txt`)
czcionką 8 px (6 px, gdy się nie mieści, np. „ABBRECHEN”), złote wielkimi literami (z polskimi i
niemieckimi), kolorem liter oryginału (0x93 / 0x91 – kolory rodu, więc jak oryginał niebieskie,
czerwone, zielone) z cieniem czcionki; i koduje z powrotem jako sprite bez kompresji (serie zer
w wierszu). Sprite'y są przebudowywane **w miejscu** (bufor na 2 bajty na piksel), bo widżety
trzymają ich adresy – po zmianie języka w menu głównym (`Lang_ApplyChosen`) przyciski zmieniają
się też tam; powrót do języka obrazków kopiuje oryginały. `layout_amiga.c` (LOAD / SAVE) bierze
litery i kolory z oryginałów (`Lang_OriginalSprite`), a słowa LOAD / SAVE w innych językach
pisze tym samym stylem co MENTAT / OPTIONS. Pytanie „Do you wish to join House X?” (dwa kawałki
obrazka MISC) w innym języku pisane jak zdania mentata (`gui.c`, tekst portu z `%s`).

Sprawdzone w WinUAE: polski (Atreides, Harkonnen, Ordos – kolory rodów; pasek, Tak / Nie, Dalej /
Powtórz, pytanie o ród, okno budowy „ZBUDUJ TO” / „WRÓĆ DO GRY”), niemiecki (MENTAT, OPTIONEN,
LADEN, SICHERN, Credits, „ABBRECHEN” w oknie mentata), powrót na angielski w menu głównym –
oryginalne obrazki. Okno portu (Send Order / Invoice) nie oglądane – ten sam kod.

### Górny pasek w innych językach (30–31.10.2026, życzenie użytkownika)

W języku bez własnych obrazków MENTAT i OPTIONS (po polsku INDEKS, OPCJE) są węższe – tak
szerokie jak ich słowo (`Lang_LetterButton` wycina środek przycisku) – a obok nich osobne
LOAD i SAVE (ODCZYT, ZAPIS; `s_langBar` w `layout_amiga.c`). Odstęp między wszystkimi czterema
5 pikseli (sprite'y z napisem kończą się 2 pustymi kolumnami, więc ich odstęp to 3), napisy
na tej samej wysokości (MENTAT / OPTIONS o wiersz wyżej niż inne złote przyciski). Pod
przyciskami, aż do „Credits”, deseń gładkiej części paska za OPTIONS (kolumny 184–191, ten sam,
którym `Layout_ComposeBackground` poszerza pasek): bez krawędzi oryginalnego OPTIONS i szwu
poszerzenia. Sprawdzone w WinUAE (400×300, 640×480, 800×600) i na Amidze 2 (640×480), kliknięcia
wszystkich czterech.

### Mentat jako nowoczesny obrazek: `modern=1` (30–31.10.2026, życzenie użytkownika)

`opendune.ini` `modern=1`: gdy w `PROGDIR:lang/GFX.PAK` jest `MODERNx.CPS` (x = litera rodu:
A, H, O, M – M to ekran wyboru rodu z postacią Bene Gesserit), `GUI_Mentat_Display` bierze go
zamiast `MENTATx.CPS`; film odprawy gra w monitorze obrazka (widżet 8: 128,48, 184×112), oczy,
usta i ramię oryginału nie są rysowane. Imiona w tekstach mentata zamienione
(`GUI_Mentat_Loop`: Cyril → Cizar, Ammon → Sellen, Radnor → NineX, też odmienione; tekst
kopiowany, gdy zmienia się jego długość). Bez `modern=1` albo bez pliku – oryginał.

`tools/make_modern.py OBRAZEK RÓD X0 Y0 X1 Y1` (ekran monitora na obrazku): skalowanie na
miejsce filmu, wycięcie 320×200, brakujące wiersze z krawędzi obrazka (u góry ciemniej pod tekst,
w dół do czerni), kolory palety gry z ditheringiem – tylko te, których używa oryginalny ekran
(żaden nie miga); dla M paleta `BENE.PAL` (ładowana przez grę na tym ekranie), dla reszty
`IBM.PAL`. Zapis do `build/gfx/GFX.PAK` (inne rody w nim zostają). Napisy i przyciski wypalone
w obrazku (pytanie, Yes / No, Exit, tekst mentata) trzeba najpierw zamalować – gra rysuje własne.
Obrazki (`tools/gfx/`) i `GFX.PAK` tylko na tym PC (`.gitignore`). Sprawdzone w WinUAE (wszystkie
cztery ekrany) i na Amidze 2 (Atreides, Ordos).

### Do sprawdzenia na Amidze

- Wybór języka w Game controls, zmiana w menu głównym, start z `LANGUAGE.CFG`.
- Niemieckie `.GER` z Amigi w `data`: menu, mentat, misja, napisy końcowe.
- Polski: napisy w panelu bocznym (nazwy 6 px), okno LAN, Amiga info, pomoc mentata,
  mapa między misjami, intro (napisy czcionką 8 px, gdy nie ma INTRO.FNT).
