# Indicators of Compromise (IOCs)

## File Hashes

### Original Trojanized Binary
```
SHA256: c2e55c3158ea06aede09f36a1d35ca75c1209f323c092bfc078a992109dd1c53
File:    ScreenConnect_Patcher_V3.exe
Size:    1,306,624 bytes
Type:    PE32 GUI, x86 (32-bit)
```

### Embedded WizWorm v4 RAT
```
File:    javaw.com
Size:    76,800 bytes
Type:    PE32 .NET assembly (VB.NET)
Assembly: javaw
GUID:    bb11cdf9-cee3-43d7-a543-6d5e4e627559
Claims:  "Microsoft Visual C++ 2015-2022 Redistributable (x64) - 14.44.35112"
```

### Embedded ScreenConnect Patcher (legitimate)
```
File:    ScreenConnect Patcher V3.exe
Size:    827,392 bytes (decompressed from 786,578 LZNT1)
Type:    PE32 .NET assembly (VB.NET)
```

## Network IOCs

### Telegram C2
```
Domain:  api.telegram.org
URL:     https://api.telegram.org/bot{token}/sendMessage?chat_id={chat_id}
Method:  POST
Format:  JSON with "text" field containing command data
```

### User-Agent Spoofing
```
Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:66.0) Gecko/20100101 Firefox/66.0
Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1
Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.113 Safari/537.36
```

## Host IOCs

### Dropped Files
```
%TEMP%\javaw.com                          → WizWorm v4 RAT
%TEMP%\ScreenConnect Patcher V3.exe       → Legitimate patcher (also dropped)
%APPDATA%\{random}\                        → RAT persistence location
Startup\{name}.lnk                        → Persistence shortcut
```

### Registry
```
HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced\ShowSuperHidden
→ Set to hide super-hidden files
```

### Windows Defender Exclusion
```powershell
powershell.exe -ExecutionPolicy Bypass Add-MpPreference -ExclusionPath "{rat_path}"
powershell.exe -ExecutionPolicy Bypass Add-MpPreference -ExclusionProcess "{rat_process}"
```

### Process Protection
```
ntdll!RtlSetProcessIsCritical(TRUE, ...)
→ Makes process unkillable via Task Manager
→ BSOD if force-terminated
```

## Behavioral IOCs

### Crypto Clipper Regex Patterns
```
Bitcoin:  \b(bc1|[13])[a-zA-HJ-NP-Z0-9]{26,45}\b
Ethereum: \b(0x)[a-zA-HJ-NP-Z0-9]{40,45}\b
TRON:     T[A-Za-z1-9]{33}
```

### Anti-Analysis Hostname Blocklist
```
NfZtFbPfH  → VM/sandbox hostname
ELICZ       → Analysis environment
tz          → Minimal VM name
MAIN        → Sandbox host
```

### Anti-Analysis Path Blocklist
```
C:\Documents and Settings\Administrator\My Documents\mwsmpl.exe
C:\SELF.EXE
myapp.exe
```

## Packer IOCs

### XOR Decode Signature
```
Entry point: calls 0x53dfea (unpacker stub)
VirtualProtect → PAGE_READWRITE on .text section
ROL-5 + MUL-7 key schedule on 865,596 bytes
ROR-13 hash-based API resolution (no IAT)
```

### API Hash Values
```
Module hashes (UTF-16LE, ROR-13 + tolower):
  0x6a4abc5b = kernel32.dll
  0x3cfa685d = ntdll.dll
  0x9bac604d = shlwapi.dll
  0x40f1a814 = shell32.dll

Function hashes (ASCII, ROR-13):
  0xec0e4ea4 = LoadLibraryW
  0xa80eecae = GetProcessHeap
  0x73e2d87e = ExitProcess
  0x1be1bb74 = ShellExecuteW
  0xfd46a728 = RtlDecompressBuffer
  ... (see dropper/api_hashes.txt)
```
