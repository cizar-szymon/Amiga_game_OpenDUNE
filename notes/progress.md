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
