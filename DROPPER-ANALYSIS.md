# Dropper Analysis — ScreenConnect_Patcher_V3.exe

## Binary Overview

| Property | Value |
|---|---|
| File | ScreenConnect_Patcher_V3.exe |
| Size | 1,306,624 bytes (1.3 MB) |
| SHA256 | c2e55c3158ea06aede09f36a1d35ca75c1209f323c092bfc078a992109dd1c53 |
| Type | PE32 GUI, x86 (32-bit), Windows |
| Compiler | MSVC (C/C++) |
| Sections | .text (entropy 6.24), .idata (4.02), .rsrc (4.47) |

## Architecture: Three-Stage Dropper

```
┌─────────────────────────────────────────────┐
│ Stage 1: Custom XOR Unpacker (entry=0x53dfea)│
│   • Anti-analysis checks                    │
│   • ROL-5/MUL-7 XOR decode (865,596 bytes) │
│   • Hash-based API resolution (ROR-13)      │
└──────────────────┬──────────────────────────┘
                   │ jumps to
┌──────────────────▼──────────────────────────┐
│ Stage 2: Native C Dropper (0x401000)        │
│   • Load DLLs via hash resolution            │
│   • Extract javaw.com → WizWorm v4 RAT      │
│   • Extract + decompress patcher (LZNT1)    │
│   • ShellExecuteW both payloads              │
└──────────────────┬──────────────────────────┘
                   │ drops + executes
┌──────────────────▼──────────────────────────┐
│ Stage 3a: javaw.com (WizWorm v4 RAT)       │ ← SILENT INFECTION
│ Stage 3b: ScreenConnect Patcher V3.exe     │ ← USER SEES THIS
└─────────────────────────────────────────────┘
```

## Protection Layers

### Custom XOR Packer
- **Entry point** (0x401000) immediately jumps to unpacker at 0x53dfea
- Unpacker performs anti-analysis checks, then XOR-decodes the real payload
- All import addresses are resolved at runtime via hash → **empty IAT**
- Junk code (pushfd/popfd, xor-cancel, inc/dec) throughout

### Hash-Based API Resolution
Instead of calling `LoadLibraryW`/`GetProcAddress` normally, the dropper uses ROR-13 hashes:

```c
// Example: resolving ExitProcess
void *fn = resolve_api(0x6a4abc5b, 0x73e2d87e);
//                  ^kernel32.dll hash  ^ExitProcess hash
fn();  // calls ExitProcess
```

This makes static analysis nearly impossible — no import table reveals what the binary does.

### Anti-Analysis Checks (in unpacker)
| Check | Technique | Effect |
|---|---|---|
| Memory | malloc/free 100MB | Detects constrained sandboxes |
| Error mode | SetErrorMode(SEM_FAILCRITICALERRORS) | Sandbox detection |
| NUMA | VirtualAllocExNuma(-1, ...) | Fails in emulators |
| Hostname | GetComputerNameA vs blocklist | VM/analysis environment detection |
| Path | GetModuleFileNameA vs blocklist | Known sandbox paths |

**Blocklist**: `NfZtFbPfH`, `ELICZ`, `tz`, `MAIN`, `mwsmpl.exe`, `SELF.EXE`, `myapp.exe`

## Unpacked Functions (4 Total)

### entry() @ 0x401000
Main function. Loads DLLs, drops both payloads, executes them.

### FUN_00401272() @ 0x401272
Hash-based API resolver. Walks PEB → loaded modules → export tables.

### FUN_00401366() @ 0x401366
LZNT1 decompression via ntdll!RtlDecompressBuffer.

### FUN_004013ff() @ 0x4013ff
File deployment. Writes data to 11 different Windows special folders, optionally executes.

## Embedded Payloads

### Payload 1: javaw.com (76,800 bytes)
- **Offset**: 0x4018AA in unpacked binary
- **Size**: 0x12800 bytes (76,800)
- **Type**: PE32 .NET assembly
- **Identity**: **WizWorm v4 RAT** (disguised as VC++ Redistributable)
- **Deployed to**: Temp directory, then executed

### Payload 2: ScreenConnect Patcher V3.exe (827,392 bytes)
- **Offset**: 0x4140AA in unpacked binary (compressed)
- **Compressed size**: 786,578 bytes (LZNT1 format)
- **Decompressed size**: 827,392 bytes
- **Type**: PE32 .NET assembly (VB.NET, uses dnlib)
- **Identity**: Legitimate ScreenConnect patcher
- **Deployed to**: Temp directory, then executed via ShellExecuteW

## API Hash Table

See `dropper/api_hashes.txt` for the complete ROR-13 hash → API mapping.

| Module Hash | Module | Function Hashes |
|---|---|---|
| 0x6a4abc5b | kernel32.dll | LoadLibraryW, GetProcessHeap, HeapFree, ExitProcess, GetTempPathW, GetModuleFileNameW, lstrcpyW, DeleteFileW, CreateFileW, WriteFile, CloseHandle |
| 0x3cfa685d | ntdll.dll | RtlAllocateHeap, RtlDecompressBuffer |
| 0x9bac604d | shlwapi.dll | PathFindFileNameW, PathCombineW |
| 0x40f1a814 | shell32.dll | SHGetFolderPathW, SHGetKnownFolderPath, ShellExecuteW |
