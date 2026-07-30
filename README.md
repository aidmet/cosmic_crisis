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
| R / SELECT | Use held power-up |
| START | Pause |
| A+B+START+SELECT | Emergency restart to title |

## Modes

- **Play** — endless survival, escalating meteor storms
- **Story** — five-chapter campaign with commander briefings
- **Multiplayer** — battle royale duel (see backends below)
- **Options** — music/SFX toggles, story progress reset (SRAM)

## Multiplayer backends

Requires [gba-link-connection](https://github.com/afska/gba-link-connection) at the repo root:

```bash
git clone --depth 1 https://github.com/afska/gba-link-connection.git
```

| Menu | Hardware | Notes |
|---|---|---|
| LINK CABLE | GBA link cable / mGBA multiplayer windows | 2–4 players (Butano `bn::link`) |
| WIRELESS | Official Wireless Adapter | 2–5 players, room name `COSMIC` |
| ONLINE | Mobile Adapter GB (REON) | 2 players; A dials `127.0.0.1`, START waits for call |

## Power-ups

Shield, Slow field, Clear burst, Weapon upgrade, Extra life.
