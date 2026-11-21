# Debug menu (F1)

The debug panel is an ImGui overlay for development cheats and host tweaks.
It is **off by default**. The Escape options menu (RmlUi) does not turn it on;
you enable it in `options.cfg`, then open it with F1 (or the gamepad).

## How to enable

1. Run the game once so it creates `options.cfg` next to the executable
   (or under `--data` / `RECOMPLAUNCHER_DATA` if you set a data folder).
2. Quit, open `options.cfg` in a text editor, and set:

```text
debug_menu=1
```

3. Start the game again.
4. Press **F1** to open or close the panel. On a gamepad, **click the right
   stick (R3)** does the same when `debug_menu=1`.

Changes you make in the panel are saved back into `options.cfg` (same as the
Escape menu). To hide the panel forever, set `debug_menu=0` again (F1 will
then do nothing).

### Related without the panel

| Want | How |
|------|-----|
| Unlock FPS from the Escape menu | System tab → **Unlock FPS** (same as `uncap_fps`) |
| Mono mix from Escape | Audio tab → **Mono** (same as `mono_audio`) |
| Fast boot once from the CLI | `splatterhouse.exe --skip-checksum` (does not persist unless you also set `skip_checksum=1`) |

## Panel contents

Window title: **Debug** (EN) / **Depuración** (ES). Language follows the
Escape menu language setting.

### Game

| Label (EN) | `options.cfg` key | Effect |
|------------|-------------------|--------|
| **Stage select** | `stage_select` | Acts like DIP SW3 On: Start can open the arcade stage-select / debug stage screen instead of only continuing play. |
| **Fast boot** | `skip_checksum` | Speeds through RAM/VRAM self-tests until the title (ROM checksum still runs). Same idea as `--skip-checksum` for one launch. |
| **Invincible** | `invincible` | Keeps the player’s hearts topped up during play. |
| **Untouchable** | `untouchable` | Contact does not apply recoil or heart loss (`skip_hurt` on the main CPU while in play). |
| **Unlock FPS** | `uncap_fps` | Drops the ~60 Hz present cap and ignores vsync pacing so frames can run as fast as the host allows. Also available on Escape → System. |

### Audio

| Label (EN) | `options.cfg` key | Effect |
|------------|-------------------|--------|
| **Mono** | `mono_audio` | Downmixes stereo output to mono (PCB JP4-style). Also available on Escape → Audio. |

## Defaults

All of the flags above default to **off** (`0`) in a fresh `options.cfg`.
`debug_menu` itself also defaults to `0`.

## Notes

- Escape still opens the normal **Options** menu (video, audio, controls,
  language). That menu does not list the Game cheats; those stay on F1.
- Left-stick click (L3) toggles the Escape options menu on a gamepad; it does
  not open Debug.
- Headless builds (`--headless`) have no SDL/ImGui window, so there is no
  debug panel.
