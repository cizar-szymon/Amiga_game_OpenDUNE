# OpenDUNE – postęp

## 28.10.2026
- Projekt założony: `upstream/` (GitHub, 0.9-137-gebea79ae), `novacoder/` (Aminet 0.9.042 + src.zip).
- Faza 0 zakończona, decyzje: aktualny upstream, natywny backend (wzór Atari), 8-bit, Paula.
- Port działa na Amidze 2 od intro do misji 1, 800×600 (2,5×3), 2–8% CPU (NovaCoder 90–100%).
  Szczegóły i pomiary: `docs/PORTING.md`, rozdział „Port (faza 1–2)”.
- Dalej: odsłuch głosów Paula (użytkownik), muzyka (MIDI → CAMD / syntezator), test zapisu i
  wczytania (`game1`), dłuższa gra i pomiar w ciężkiej scenie, ikona i Tool Types.
- Znane: kod testowy `debug_commands` włączony tylko w ini instalacji testowej.

## 1.11.2026 (po południu)
- Sprawdzony start widoku w grze LAN (3 graczy: Amiga 1, WinUAE, `lan_peer.py`): położenie dobre;
  „czarny widok” to przewijanie przy krawędzi ekranu, bo wskaźnik stał na górnej krawędzi
  (`docs/PORTING.md`, „Testy przed wydaniem 0.50, część 2”). Do decyzji: przewijanie krawędzią
  dopiero po ruchu myszy, widok wyśrodkowany na bazie.
- Amiga 2 nie odpowiadała – test z trzema prawdziwymi Amigami dalej otwarty.
- Poprawka: przewijanie krawędzią ekranu dopiero po zjechaniu wskaźnikiem z krawędzi na mapie
  (`gui/rts_amiga.c`, `s_edgeArmed`); sprawdzone w grze LAN (Amiga 1 + WinUAE).
- Ustawienia (decyzja użytkownika): rozdzielczość, język, licznik FPS wybrane w grze wygrywają
  z ikoną i ini; czysta gra: 640×480, angielski, bez licznika. `vram_scroll`, `rts_controls`,
  `smooth_units`, `smooth_scroll` usunięte (zawsze włączone). Sprawdzone na Amidze 1; opisy
  `OpenDUNE.txt`/`_en`, `opendune.ini` poprawione. Paczka 0.50 nieprzebudowana, nic nie zacommitowane.
- Ikona z paczki (`tools/make_release.sh`) bez podpowiedzi `(SCREENMODE=)`, `(LANGUAGE=)`, `(SHOW_FPS=)` –
  te ustawienia wybiera się w grze; przykłady w `OpenDUNE.txt`/`_en` zmienione na DATADIR / MUSIC_VOLUME.

## 2.11.2026
- Amiga 2 (`WorkSD:Games/OpenDUNEWarp`): wgrana nowa binarka (0.50 z 1.11, 18:04), MakeMusic + ikona,
  opisy PL/EN, Install.txt/pdf z ikonami, COPYING. `opendune.ini`, `saves`, `lang`, `OpenDUNE.info`
  bez zmian. Start sprawdzony: gra w misji (Ordos, po polsku), zrzut `od.py --shot`.
- Amiga 2: podmienione `OpenDUNE.info` (ikona z paczki, nowe podpowiedzi Tool Types) i `opendune.ini`
  (z paczki + `datadir=WorkSD:Games/OpenDUNE/data`); stare jako `*.amiga2` obok. Bez `debug_commands`
  `od.py` tam nie działa. `modern=1` (nowy obraz mentatki z `lang/GFX.PAK`) dopisane z powrotem –
  w paczce go nie ma (domyślnie 0, `GFX.PAK` nie w paczce). Start sprawdzony.
- Amiga 1 (`WorkSD:Games/OpenDUNEWarp`): binarka OpenDUNE.s, opisy z ikonami, COPYING, ikona z paczki,
  `opendune.ini` jak na Amidze 2 (paczka + datadir + modern=1), stary jako `opendune.ini.amiga1`. Start ✅.
- Amiga 2 miała stary `lang/GFX.PAK` (128 056 B, sprzed 30.10; aktualny `build/gfx/GFX.PAK` 256 108 B) –
  stąd brak nowego obrazu mentatki. Do wgrania, gdy Amiga 2 odpowie (przestała przy wgrywaniu).
- Wydanie 0.51 (decyzja użytkownika): `port/VERSION`, wpisy w `OpenDUNE_release.txt`/`_en` (przewijanie
  krawędzią, ustawienia z gry wygrywają, mniej podpowiedzi w ikonie); `tools/build.sh` +
  `tools/make_release.sh` → `releases/amiga/OpenDUNE-0.51.zip` (483 665 B), `private/OpenDUNE-0.51-full.zip`.
  Test na Amidze 1: publiczny ZIP rozpakowany `UnZip` w `RAM:`, dane z `WorkSD:Games/OpenDUNE/data`,
  muzyka z `OpenDUNEWarp/MUSIC`: start, menu „AMIGA 0.51”, 640×480, angielski, bez licznika ✅.
  Nic nie zacommitowane. `OpenDUNEWarp` na Amidze 1: binarka i historia wydań 0.51 (start ✅); Amiga 2 jeszcze 0.50.
- README (GitHub) i opisy `OpenDUNE.txt`/`_en` (decyzja użytkownika): port specjalnie dla Amigi
  500/1200/3000/4000 z kartą Warp (560, 1240, 1260, 1260 Extreme, 4070), testowany na Warpach, szybka
  karta graficzna daje parametry; README uaktualnione (języki, ustawienia w grze, sprawdzenie przy
  starcie, 8-bit 640x480). Paczka 0.51 przebudowana, opisy wgrane na Amigę 1.

## 10.11.2026
- Życzenie użytkownika: w Dune to samo, co Settlers ma od 0.131 (`../GAME_SERVER.md`): gra przez
  internet przez serwer gry, zgłoszenia F10 z gry, raport rozejścia; ten sam serwer, podstrona `/dune`;
  od razu 2–3 graczy. Opis i tabela testów: `docs/PORTING.md`, „Gra przez internet, zgłoszenia (0.52)”.
- Serwer (repo Settlers, gałąź `amiga-port`, nie zacommitowane): `ATTACH`, pole maks. graczy, filtr
  listy po grze, podstrony `/settlers` i `/dune`; 22 testy ✅. **Na Pi nie wdrożone.**
- OpenDUNE 0.52 (nie zacommitowane, wydanie nie zrobione): `Net_Relay`, okno INTERNET GAME,
  `os/report_amiga.c`, `gui/report_amiga.c`, raport rozejścia w `lan_amiga.c`, teksty PL/DE, opisy.
- Testy na kopii serwera na PC z Amigą 2 (`OpenDUNE-test` obok wydania 0.50) i WinUAE: gra 2 i 3
  graczy, F10, rozejście – wyniki w PORTING.
- Test trzech prawdziwych maszyn (Amiga 2 + Amiga 1 w sieci lokalnej + WinUAE) przez kopię serwera:
  16,5 min, użytkownik grał na obu Amigach, 0 różnych sum.
- Serwer wdrożony na Pi (zgoda użytkownika, VPN), kopia w `~/settlers-server/backup-20261110/`;
  sprawdzony przez adres publiczny fałszywymi Amigami (Settlers stary format + Dune 3 graczy).
- Dalej: gra z Amig przez Pi, prawdziwy Settlers 0.131 przez Pi, wydanie 0.52, commity (oba repo),
  `@` z klawiatury. Na Amigach 1 i 2 został `OpenDUNE-test` w `OpenDUNEWarp`.
- Zgłoszenie użytkownika w grze 3 graczy: płyt nie da się stawiać po 1–2 sztukach. Przyczyna i poprawka
  (wspólny obiekt płyty wszystkich rodów, `Lan_Place`) w PORTING; sprawdzone na Amidze 2 ✅.
- Życzenie użytkownika: równe szanse na starcie. Pomiar skały na starcie wszystkich 36 map
  (`debug_lan_maps=1`, tabela w PORTING): większość map nierówna (np. domyślna SCENA014: 119 / 855 / 855).
  Propozycje do decyzji: (1) miejsca startu wybierane przy starcie gry według skały (każdy własna skała
  podobnej wielkości, daleko od siebie), (2) tylko mapy o stosunku do ok. 1,5, (3) jedno i drugie.
- Decyzja użytkownika: droga 1 (miejsca startu wyliczane z mapy). Zrobione: `Lan_ChooseStarts` i reszta
  w `lan_amiga.c`; tabela wyników dla 36 map w PORTING; trzy maszyny wyliczają to samo, 0 różnych sum;
  użytkownik potwierdził wygląd. Otwarte: przyprawa nie liczona, rogi mapy, mapy „2P” dla 3 graczy,
  widok po wznowieniu zapisu LAN; anulowanie płyty / zniszczenie placu z płytą.
- Stan: nic nie zacommitowane (OpenDUNE i Settlers `server/`), wydanie 0.52 nie zbudowane; serwer na Pi
  wdrożony. Na Amigach 1 i 2 `OpenDUNE-test` (ostatni build) w `OpenDUNEWarp`.
- Pomiary czasu procesora (Amiga 2): spokojna scena i bitwa trzech rodów (`debug_lan_battle=20`) – tabele w
  PORTING; w bitwie rysowanie mapy 56%, wysyłka 22%, wolne 7%.
- Decyzja użytkownika: następny krok to test rysowania w pamięci karty na osobnej gałęzi, dopiero gdy
  będzie nowa funkcja FPGA (PORTING, „Następny krok”). Wcześniej można rozbić pomiar `draw` na części.
- Amigi posprzątane (bez `OpenDUNE-test`), commity na gałęzi `internet-game` wysłane na GitHuba; `main` bez
  zmian do wydania 0.52 (nie zbudowane). Do sprawdzenia: `@` w oknie F10.
- Wydanie 0.52 zrobione i wysłane (paczka publiczna sprawdzona na Amidze 2 z Shella; pełna w `private`).
- Amiga 2: `lang/GFX.PAK` podmieniony na aktualny (stary jako `GFX.PAK.old`); gra z ikony zamknięta
  narzędziem `breaktask` z UFO.
- 0.53: `font=clean` (X11 fixed 6×10 zamiast pierwotnie wybranego Terminusa – PORTING), belka bez cienia i
  napisy przycisków o rząd niżej, komunikat „baza jest atakowana” tylko dla atakowanego gracza.

- Amiga 2 (10.11, zmiana użytkownika): stary port NovaCodera usunięty, nasz port przeniesiony z `OpenDUNEWarp` do
  `WorkSD:Games/OpenDUNE`; dane gry wgrane z PC do `WorkSD:Games/OpenDUNE/data` (35 plików, bez INTRO.PAK), start ✅.
  Na Amidze 1 katalog dalej `OpenDUNEWarp`.
