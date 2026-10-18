# Reverse-engineering ledger

This ledger separates observations from a locally examined physical reference disc, documentation claims, and partial static analysis. That disc contains **PCD_APPL 3.1**, not the separate 3.3 archive analyzed below.

## Physical reference disc

| Claim | Evidence | Confidence |
| --- | --- | --- |
| The optical reference disc was mounted read-only from `/dev/sr1` as ISO 9660, beneath `/media/`. | Live `lsblk`/`findmnt`; no files were written to the disc. The local mountpoint and volume label are omitted from this public ledger. | Verified for this session |
| The disc contains 14 numbered photographs and `STARTUP.PCD`, `OVERVIEW.PCD`, `INFO.PCD`, and `OPTIONS.PCD`. | Case-insensitive inventory of the mounted `photo_cd` tree; native player self-check decoded all 14 numbered files and startup artwork. | Verified |
| Its bundled player identifies itself as PCD_APPL version 3.1, April 14, 1993. | Version string in `cdi/pcd_exec.pcd`; file SHA-256 `781c9ce59ab5e6e4fbbb259e15e19717e48d590761e9ef85fd272b377711e5f1`. | Verified |
| `cdi/pcd_exec.dat` is a separate 23,872-byte data file. | Mounted filesystem inventory; SHA-256 `856a7b053bb7d3fb5b5a7214ecad10ffb31cfb90e12494f2b133969ba3f0ee4f`. | Verified |
| `STARTUP.PCD` decodes to a 384×256 image at the selected preview level. | The bundled pcdDecode implementation and the app's `--self-check`. | Verified |
| The decoder rejects `OVERVIEW.PCD` as an ordinary image and reports interleaved audio. | pcdDecode error while testing that file. The message alone does not establish the overview pack's internal format. | Verified error; format interpretation open |
| The disc includes CD-i assets such as `INTRO_P.6DY`, `HRMENU.6C4`, language `.6TX`/`.6IN` files, and `.6R7` menu resources. | Mounted `cdi/` inventory. Asset semantics and rendering have not yet been decoded. | Files verified; meaning open |

The examined ISO 9660 volume reports system ID `CD-RTOS CD-BRIDGE`, application ID `CDI/CDI_APPL.PCD;1`, and size 62,963,712 bytes. UDisks reports two sessions and ten tracks, with nine data tracks. These values describe the examined disc and should be rechecked for other media.

## Separate Philips 3.3 archive

The public ICDIA `photocd_on_cdi_33.zip` archive is a research reference, not the physical 3.1 disc. Archive SHA-256: `a0f5e8f45367f1f1b546e595469b6a0470a073dc4d6f6dfc8112232a3640dc25`. Its executable contains the PCD_APPL 3.3, March 18, 1996 banner.

The extracted `PCD_APPL` module is 177,412 bytes, SHA-256 `710c08c176cab3ec32269e6e1d3e08a9fad148c6b43daad0f79bcd006d39257a`. It begins with the OS-9/68K sync word `0x4AFC`; the module header parity and CRC-24 checks were valid, with residue `0x800FE3`; the module entry field is `0xB4`. The archive also contains `DSV_APPL`, a font module, `cdi_bpsys`, and corresponding data modules.

Ghidra 12.1.4 analyzed the extracted code slice as Motorola 68000 big-endian, with module-relative addresses and an initial analysis range through `0x2B500`; it identified 285 functions. This mapping does not model the OS-9 loader's runtime relocation or imported system calls. The entry at `0xB4` behaves like a startup wrapper: it calls routines at `0x2A8FC`, `0x29FCE`, `0x1B8`, `0x20C`, `0x3CC`, `0x2B3AC`, and `0x2B3C2`; the latter routines contain OS-9 traps. The decompilation is therefore evidence of startup structure, not recovered GUI semantics.

Module-relative strings include:

- `0x2AA1`: `PHOTO_CD/INFO.PCD;1`
- `0x4FF1`: `PLAYLIST.PCD`
- `0x4FFE`: `PHOTO_CD/AUDIO/AUDIO`
- `0x17E34`: `PHOTO_CD/`
- `0x1CBF2`: `PhotoCD on CD-Interactive`

These strings suggest file and playlist handling, but do not prove the behavior of the corresponding routines. Extracted 3.3 modules, language assets, Ghidra scripts, and decompilation are preserved locally under the ignored `research/private/` directory; they are not application source and are not redistributed.

## Interface and behavior evidence

The [Philips Photo CD 3.x Icon Menu Guide](https://icdia.co.uk/docs_sw/pcd_on_cdi_3x.jpg) documents an opening menu for viewing photographs, viewing an index, creating/editing and playing personal slideshows, file operations, and settings. It describes previous/next navigation, timed and continuous playback with pause, index selection, zooming a region, rotation, hide/unhide, and resolution switching. Personal slideshows can be reordered, have photos inserted/appended or removed, and be saved/opened/deleted. These are **guide-documented** capabilities, not individually confirmed in the physical 3.1 program.

The [CD-i Photo CD FAQ, section 5.9](https://www.icdia.co.uk/faq/cdifaq5.html) distinguishes application releases. The [ICDIA software archive](https://icdia.co.uk/sw_app/index.html) lists downloadable 3.2/3.3 material. Do not attribute the v3.3 module analysis to the current 3.1 disc.

## Tooling and decoder evidence

- REA 6.5.0 was run transiently through `npx`; its Ghidra provider check succeeded with the locally installed Ghidra 12.1.4. The published CLI rejected `--architecture m68k`, although the current raw-binary documentation advertises Motorola 68K. REA therefore did **not** perform the M68K decompilation in this investigation. The focused analysis above used Ghidra directly as a documented fallback.
- [pcdDecode](https://github.com/kmaragos/pcdtojpeg) was selected for native decoding and is integrated in-process. It returns sRGB image data with decoder orientation handling; this player currently requests up to 16Base for exports. Its code is GPL-2-or-later and is kept under `third_party/pcdDecode/`; the application is GPL-3-or-later. The older [libpcd](https://github.com/kraxel/libpcd) was not selected.
- The [OS-9 module parser in cdirecomp](https://github.com/mstan/cdirecomp/blob/master/recompiler/src/disc_parser.c) was used as structural reference only; its emulator/recompiler runtime is not part of this player.
- REA references: [raw-binary investigation](https://github.com/morluto/rea/blob/main/docs/raw-binary.md), [native investigation](https://github.com/morluto/rea/blob/main/docs/native-investigation.md), and [CLI reference](https://github.com/morluto/rea/blob/main/docs/cli.md).

## Current native implementation status

The Qt 6 application reads the mounted album directly, displays the startup image, creates thumbnails from the numbered image files, browses the photos, and supports zoom, pan, rotation, resolution selection, saved favourites, and timed slideshows. Export All writes maximum requested 16Base JPEGs locally, reports progress, skips existing names, supports cancellation between photographs in the GUI, and rejects destinations on the source disc. A real export of this disc produced all 14 JPEGs; the second pass skipped all 14, and an attempted source-disc destination was refused.

The original menu artwork, exact 3.1 screen composition, `OPTIONS.PCD`/`INFO.PCD` semantics, saved playlist format, CD-i graphics decoding, and the behavior behind the listed Ghidra functions remain unverified. The native interface is guide-informed and is not claimed to be an exact visual recreation.
