# CLAUDE.md — OpenDUNE (Amiga 68k, Warp RTG)

@C:/Workprojects/AmigaGAMES/PORTING_PLAN.md
@C:/Workprojects/AmigaGAMES/AmiAgent/AMIAGENT.md

## Zanim cokolwiek zrobisz
1. Streszczenie PDF-ów o karcie Warp: `../Dune/notes/pdf-summary.md` (pełne PDF-y w `..`).
2. Stan prac, decyzje i pomiary: `docs/PORTING.md`.
3. Doświadczenia z Dune Legacy (ten sam sprzęt, ta sama gra, mini-SDL2 dla Warpa, VRAM, pamięć
   terenu, stałe tempo): `../Dune/docs/PORTING.md`, `../Dune/notes/progress.md`.

## Cel projektu
OpenDUNE (otwarte odtworzenie Dune II, C89, GPL-2.0) na Amidze z kartą Warp: płynna, wierna gra.

## Źródła
- `upstream/` – klon github.com/OpenDUNE/OpenDUNE (28.10.2026, `0.9-137-gebea79ae`). Ma backendy
  SDL1/SDL2/Win32/DOS/OS X/Atari; port Atari (2026) zawiera kod 68k (`c2p1x1_8.s`,
  `atari_dma_sound.s`, `atari_ikbd.s`).
- `novacoder/` – port NovaCodera z Aminetu (`game/strat/OpenDUNE_RTG.lha`, wersja 0.9.042):
  `pkg/` paczka, `src/Dune2_src/` źródła (SDL 1.2, `amiga_main.c`, `midi_camd.c`).
- Nie zmieniaj `upstream/` ani `novacoder/` – to wzorce do porównań. Pracuj w `port/`.

## Docelowy sprzęt
Jak w PORTING_PLAN: Amiga z Warp 060 (testy na Amidze 2: A4000 + Warp, 68060 @ 100 MHz,
`C:/amiga-dev/amiagent-2.cfg`), RTG Picasso96, AmigaOS 3.2, dźwięk AHI/Paula. Koprocesor ARM
na karcie NIE jest używany.

## Dane gry
- Pliki .PAK Dune II PC (na Amidze 2: EU 1.07 w `WorkSD:Games/OpenDUNE/data`).
- NIGDY nie commituj danych gry.

## Zasady kodu i pracy
- Zmiany amigowe w `#ifdef` (makro ustalić w fazie 0), lokalne i minimalne.
- Soft-float (wariant 68881 psuł kolory w Dune Legacy – libnix bez multilib 68881); gorące
  obliczenia w stałym przecinku. Bez instrukcji emulowanych przez 68060.library (`tools/check060.sh`
  w `../Dune`).
- Każda optymalizacja z pomiarem (ms/klatkę) na Amidze; szybka ścieżka z trybem porównania.
- Zmiany architektury (renderer, dźwięk, zapis gier) – najpierw propozycja i pytanie.
- Commity tylko na prośbę użytkownika.
- Na koniec sesji notka w `notes/progress.md`.
