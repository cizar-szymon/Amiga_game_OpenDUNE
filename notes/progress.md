# OpenDUNE – postęp

## 28.10.2026
- Projekt założony: `upstream/` (GitHub, 0.9-137-gebea79ae), `novacoder/` (Aminet 0.9.042 + src.zip).
- Faza 0 zakończona, decyzje: aktualny upstream, natywny backend (wzór Atari), 8-bit, Paula.
- Port działa na Amidze 2 od intro do misji 1, 800×600 (2,5×3), 2–8% CPU (NovaCoder 90–100%).
  Szczegóły i pomiary: `docs/PORTING.md`, rozdział „Port (faza 1–2)”.
- Dalej: odsłuch głosów Paula (użytkownik), muzyka (MIDI → CAMD / syntezator), test zapisu i
  wczytania (`game1`), dłuższa gra i pomiar w ciężkiej scenie, ikona i Tool Types.
- Znane: kod testowy `debug_commands` włączony tylko w ini instalacji testowej.
