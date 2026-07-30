# Cosmic Crisis

GBA action game built with [Butano](https://github.com/GValiente/butano).

## Build

```bash
make -j8
```

Produces `cosmic_crisis.gba`.

Refresh IntelliSense database:

```bash
make -j8 GENERATE_COMPILE_COMMANDS=1
cp build/compile_commands.json .
```

## Controls

| Input | Action |
|---|---|
| D-pad | Move / menu |
| A | Confirm / fire |
| B | Back / brake |
| R | Use held power-up |
| START | Pause |
| A+B+START+SELECT | Emergency restart to title |

## Modes

- **Play** — endless survival, escalating meteor storms
- **Story** — five-chapter campaign with commander briefings
- **Multiplayer** — link cable (2–4), wireless adapter lobby, online/mobile lobby; last survivor wins
- **Options** — music/SFX toggles, story progress reset (SRAM)

## Power-ups

Shield, Slow field, Clear burst, Weapon upgrade, Extra life.
