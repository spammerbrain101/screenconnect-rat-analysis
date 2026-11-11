# .NET Payload Analysis - javaw.com (WizWorm v4)

## Overview
- **File**: javaw.com (76,800 bytes)
- **Type**: PE32 .NET assembly (VB.NET, MSVC compiled)
- **Disguised as**: "Microsoft Visual C++ 2015-2022 Redistributable (x64) - 14.44.35112"
- **Real identity**: **WizWorm v4** RAT (Remote Access Trojan)
- **Assembly name**: javaw
- **GUID**: bb11cdf9-cee3-43d7-a543-6d5e4e627559

## Class Structure (Obfuscated)

### My.* (VB.NET My namespace)
- `My.AuXJJhHH278fFfaib3mPYq3fHVtRbccwcURaJFqdbs7EhWNlOwyoGoD9AyO2ggNmC2nJ0s2NqwPe2MDrzAZrA466WURqEI1`
  - Extends ApplicationBase (VB.NET application framework)
- `My.lKUTcm0kQNZCGzO460KUTsGCflQ499RIkkyHn`
- `My.BIsTCLHJJO6y9ADCMp6T7evJfyGj4vmcmfaB0`
  - Contains MyWebServices, ThreadSafeObjectProvider

### Stub.* (Main RAT modules - all class names are obfuscated)
- `Stub.znf0FlPSWS1X44GGlfHKpW` - Core module (24 methods)
- `Stub.IxWGzdwYyRAnAT7bux8ANk` - C2/Network module (48 methods)
- `Stub.WGwqkdCzSHpwnlsxL5X2Xk` - Persistence/Installation module
- `Stub.1tYJcDHyuMG1VrUjTiiujxAEudyrAMP1bPKcgz0xTlP3FbOXbNo0cL` - **Keylogger** (LowLevelKeyboardProc)
- `Stub.ciByGbkdK89H0OpXYs08G4l1Ktgxhgp4LWyjnWl0ELGC5mOWWmzhHE` - NativeMethods (P/Invoke)
- `Stub.SWFM5IU8iSVKvxeT085ezOkJiZv58kYdCHgWWOZpmSChT41JsyvJGj` - NotificationForm (UI)
- `Stub.45OckD0gPqGvNcaUYgmazqFvgsNSqWAvWwqUgqSqSAj6FQXDUmlOK5` - Anti-idle (LASTINPUTINFO, EXECUTION_STATE)

### Other
- `l3KjadKNELmLssk7LZwiTRfOTTPoePIz7QO8Y57yWI9P7i07cuKZT3` - Entry/Loader

## P/Invoke (Native API Calls)

### External DLLs
- `avicap32.dll` - Video capture (webcam)
- `user32.dll` - Window management, keyboard hooks
- `kernel32.dll` - Process/memory management
- `NTdll.dll` - Native API (RtlSetProcessIsCritical)

## Functionality Map

### 1. Telegram C2 (Command and Control)
- **API**: `https://api.telegram.org/bot`
- **Endpoint**: `/sendMessage?chat_id=`
- Uses bot token + chat ID for command/receive

### 2. Keylogger
- LowLevelKeyboardProc hook
- Tracks: [SPACE], [ENTER], LControlKey, RControlKey, LShiftKey, RShiftKey, Capital (caps lock)
- Caps lock state tracking: [CAPSLOCK: ON/OFF]

### 3. Crypto Clipper
- **BTC Clipper** - Bitcoin wallet address replacement
- **ETH Clipper** - Ethereum wallet address replacement
- **TRC20 Clipper** - TRON wallet address replacement
- Regex patterns for detecting wallet addresses in clipboard:
  - `\b(bc1|[13])[a-zA-HJ-NP-Z0-9]{26,45}\b` (Bitcoin)
  - `\b(0x)[a-zA-HJ-NP-Z0-9]{40,45}\b` (Ethereum)
  - `T[A-Za-z1-9]{33}` (TRON)

### 4. System Control
- `PCShutdown` / `PCRestart` / `PCLogoff` via shutdown.exe

### 5. DDoS
- `StartDDos` / `StopDDos` - DDoS attack capability
- `StartReport` / `StopReport` - Reporting mode

### 6. Persistence and Installation
- **Windows Defender bypass**: `powershell.exe -ExecutionPolicy Bypass Add-MpPreference -ExclusionPath/ExclusionProcess`
- **Startup shortcut**: WScript.Shell CreateShortcut with TargetPath/WorkingDirectory
- **Registry persistence**: `Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced` ShowSuperHidden
- **AppData deployment**: `%AppData%`
- **LNK icon spoofing**: Reads `HKEY_LOCAL_MACHINE\software\classes\` for file type icons

### 7. Anti-Analysis and Evasion
- `RtlSetProcessIsCritical` - Makes process unkillable (critical system process)
- `SetThreadExecutionState` - Prevents sleep/screensaver
- File icon spoofing via registry (disguises as legitimate app)
- LNK shortcut creation with icon masquerading

### 8. Plugin System
- `sendPlugin` / `savePlugin` - Dynamic plugin loading
- `OfflineGet` - Offline command queuing

### 9. Remote Commands
- `Urlopen` / `Urlhide` - Show/hide URLs
- `MessageBox` - Display message boxes
- `RunRecovery` / `Recovery` - Recovery mode
- `RunOptions` / `UACFunc` - UAC bypass

### 10. System Info Collection
- `New Clinet :` / `UserName :` / `OSFullName :`
- WMI: `\root\SecurityCenter2` - `Select * from AntivirusProduct`
- Service Pack detection, HWID tracking

### 11. Network
- POST / HTTP/1.1 with keep-alive
- User-Agent spoofing (Firefox 66, iPhone Safari 11, Chrome 60)

### 12. Misc
- Webcam capture via avicap32.dll
- Screenshot capture via CopyFromScreen
- Base64-encoded encrypted config strings
- Log file: `\Log.tmp`
