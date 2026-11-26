# Porting addresses to the Japanese disc (SLPS-01855)

Every hook and address in this repo is for US disc 1 (SLUS-01030). The Japanese disc's code is mostly the
same code at other addresses, so addresses are moved over by matching code, not by guessing an offset.

## Tool: `tools/port_addrs.py`
Builds a memory image for each disc from the recompiled C (`generated/`, `generated-jp/`) and/or raw files
loaded at an address, cuts the code into pieces at each `jr ra`, pairs pieces that have the same shape on
both discs (immediates and jump targets ignored, only if unique), then reads the answer off the pairs:
- **code address**: its position in the paired piece (`code` in the output);
- **data address / call target**: the addresses the paired code builds (lui + addiu/ori/load/store, jal),
  tallied over every use (`refs N`, N = how many places agree).
The first code word on each side is printed so a hook's guard word can be copied straight from it.

Quick start's list: `tools/port/quick-start-us.txt`. Run, with both menu modules (FDAT 206) at hand:

    python3 tools/port_addrs.py --us gen=generated --us bin=US-206.bin@0x80050DD8 \
        --jp gen=generated-jp --jp bin=JP-206.bin@0x80050DF8 --file tools/port/quick-start-us.txt

Self-test: US against US maps all 64 addresses to themselves; a module moved by 0x40 maps code by +0x40.

## Found so far (boot EXE only, 2026-11-23)
| What | US | JP | How sure |
|---|---|---|---|
| LoadImage | `8002C75C` | `8002C7C0` | same code |
| StoreImage | `8002C7BC` | `8002C820` | same code |
| MoveImage | `8002C81C` | `8002C880` | same code |
| DrawSync | `8002C4B0` | `8002C514` | same code (its data ref `8004E6FA` → `8004E71E`) |
| Menu module load address | `80050DD8` | **`80050DF8`** (likely) | 2 uses agree (and `80050DD0` → `80050DF0`, 14 uses) |

- Exe data moves **+0x24** near `8003EA64` and `80036C94` but **+0x20** from about `8003EA90`, so the
  variables the menu uses (`8003EA7A`, `8003EA94`, `80036FB0`, …) can't be offset-guessed: they come from the
  menu module's code once the JP module is matched.
- Still needs the JP disc's FDAT 206 (in `ma-jp-work.tar.xz`). Then also check the JP save layout (LOAD's
  unpack `80073610..80073B90`, "SC" header, checksum range, version byte) before LOAD GAME is turned on.
