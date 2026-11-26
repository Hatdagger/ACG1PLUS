# Disc versions on hand

Checked 2026-11-20 from the owner's own dumps. "Boot program differs" compares the boot EXE byte for byte.

| Disc | Serial | COPY.TXT says | Volume date | Boot EXE | Entry | Status here |
|---|---|---|---|---|---|---|
| Master of Arena USA disc 1 | SLUS-01030 | Version 2.0, © 2000 Agetec | 2000-02-16 | 190,464 B, load `0x80011000`, text `0x2E000` | `0x80011D84` | **recompiled** (`generated/`) |
| Master of Arena Japan disc 1 | SLPS-01855 | **Version 1.1**, © 1998 From Software | 1998-12-23 | same size, load and text size | `0x80011DB4` | **recompiled** (`generated-jp/`, 2026-11-23; see `regional/README.md`) |
| Project Phantasma USA | SLUS-00670 | Version 1.0, © 1997–98 From Software | 1998-08-31 | 178,176 B, load `0x80011000`, text `0x2B000` | `0x80011E94` | donor disc for the arena / missions import; not recompiled |

Notes
- Both MoA discs share the layout: `GG/`, `PDA.BIN` (20,132 B on both), `COPY.TXT`, a `_29M.DMY` filler (30,750,300 B).
- The Japanese boot EXE differs from the US one in 146,189 of 190,464 bytes: code has moved, so every address in
  `mapping/` needs re-finding for the Japanese disc (pattern matching by first instructions, as the engines doc does).
  It needs its own seeds + generate pass and its own digests in `game.toml`.
- Japanese dump: 701,578,080 B, 298,290 sectors, MD5 `5c07199863f701838f9723536d44b922`, CRC32 `7EDB80B5`,
  SHA-1 `f28ac02b18d0caf915110cf18eeb4c85330324c4`, SHA-256 `bd15f6c94361d535e79d8783dd46d832403c8dea8396575318c2f4adf9de8574`
  (copied with D2D, no errors, 0 re-reads). Single-track cue. Boot EXE SHA-256 `e075d8fd…511b935`.
  Same file names and layout as the US disc (`LICENSEJ.DAT` for `LICENSEA.DAT`); work pack `ma-jp-work`.
- Phantasma dump: 454,072,416 B, MD5 `91badb09339ee7e3b6d235609e8da1be`. Single-track cue.
- US dump: 700,507,920 B, MD5 `4d5774c4baa65d6eaf281db59ee8e945`, SHA-1 `96f5fcc9513ca47b19fd44bd72ee51c65a55cd43`,
  SHA-256 `9e18bb329c8c35555fafbefd2ef370c67fedec164497e5bf9f25c848a97b921e`.
  The old fork was built from this same image, so all research addresses carry over unchanged.
- Cheat-code sites list a "Version 1.1" code set (addresses +0x60 / +0x64, `ram-map-codes.md`). Open question: is
  that the Japanese disc rather than a US 1.1 pressing? All US copies checked so far share one ring code.
- 2026-11-24: an NTSC-J code list (Almar's Guides) shifts by +0x24 (profile) / +0x20 (player blocks), not the
  "v1.1" set's +0x64 / +0x60, so those are two different builds. Test for the JP dump: read credits at 0x8003EA98
  vs 0x8003EAD8 (`ram-map-codes.md` §6).
