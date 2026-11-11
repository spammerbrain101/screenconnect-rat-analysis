# ScreenConnect Patcher V3 — Trojanized Binary Analysis

## ⚠️ WARNING: This repository documents a RAT-laced binary

The widely distributed **ScreenConnect_Patcher_V3.exe** (1.3 MB) is a **trojanized dropper** that silently deploys **WizWorm v4 RAT** alongside the legitimate ScreenConnect patcher.

---

## TL;DR — What's Inside

| Component | Original (Trojanized) | Clean Rebuild |
|---|---|---|
| ScreenConnect Patcher | ✅ Legitimate | ✅ Preserved |
| **WizWorm v4 RAT** | ✅ **HIDDEN** | ❌ Removed |
| Telegram C2 | ✅ | ❌ |
| Keylogger | ✅ | ❌ |
| Crypto Clipper (BTC/ETH/TRX) | ✅ | ❌ |
| Webcam Capture | ✅ | ❌ |
| DDoS Module | ✅ | ❌ |
| Anti-Kill (RtlSetProcessIsCritical) | ✅ | ❌ |
| Defender Exclusion Bypass | ✅ | ❌ |
| Custom XOR Packer | ✅ | ❌ |

---

## Infection Chain

```
ScreenConnect_Patcher_V3.exe (1.3 MB native dropper)
│
├── [Custom XOR packer] → ROL-5/MUL-7 key, 865,596 bytes decoded
├── [Hash-based API resolution] → ROR-13 hashes, no IAT
├── [Anti-analysis checks] → VM/sandbox hostname detection
│
├── Extracts → javaw.com (76,800 bytes) 
│   └── **WizWorm v4 RAT** disguised as "Microsoft Visual C++ 2015-2022 Redistributable"
│       ├── Telegram C2 (api.telegram.org/bot)
│       ├── Keylogger (LowLevelKeyboardProc hook)
│       ├── Crypto Clipper (BTC/ETH/TRC20 regex)
│       ├── Webcam (avicap32.dll)
│       ├── DDoS module
│       ├── RtlSetProcessIsCritical (anti-kill)
│       ├── Defender exclusion bypass
│       └── Persistence (Startup LNK + Registry)
│
└── Extracts → ScreenConnect Patcher V3.exe (827 KB .NET)
    └── Legitimate ScreenConnect patching functionality
```

## Repository Structure

```
├── README.md                          ← You are here
├── DROPPER-ANALYSIS.md                ← Full native dropper reverse engineering
├── RAT-ANALYSIS.md                    ← WizWorm v4 RAT functionality breakdown
├── INFECTION-CHAIN.md                 ← Step-by-step infection flow
├── IOC.md                             ← Indicators of Compromise
│
├── dropper/
│   ├── annotated_source.c             ← Reverse-engineered dropper (4 functions)
│   ├── packer_stub.c                  ← Custom XOR unpacker stub
│   ├── unpacker_analysis.c            ← Anti-analysis checks
│   └── api_hashes.txt                 ← ROR-13 hash → API mapping
│
├── rat/
│   └── wizworm-v4/
│       ├── analysis.md                ← Full RAT capability analysis
│       ├── il_disasm.il               ← .NET IL disassembly
│       └── strings.txt                ← Extracted strings (C2, regex, commands)
│
└── patcher/
    └── clean-build/
        └── ScreenConnectPatcherV3.cs  ← Clean rebuild (RAT removed)
```

## Key Findings

1. **Double payload**: The EXE drops both a RAT and the real patcher — the patcher works normally, hiding the RAT's presence
2. **Disguised RAT**: `javaw.com` masquerades as "Microsoft Visual C++ 2015-2022 Redistributable (x64) - 14.44.35112" in assembly metadata
3. **Telegram C2**: Uses `api.telegram.org/bot` for command and control — no custom server needed
4. **Crypto stealing**: Clipboard regex replaces BTC/ETH/TRX wallet addresses with attacker's addresses
5. **Anti-kill**: `RtlSetProcessIsCritical` makes the RAT process unkillable via Task Manager
6. **Defender bypass**: Automatically adds itself to Windows Defender exclusions via PowerShell

## Hashes

| File | SHA256 |
|---|---|
| ScreenConnect_Patcher_V3.exe (original) | `c2e55c3158ea06aede09f36a1d35ca75c1209f323c092bfc078a992109dd1c53` |
| javaw.com (WizWorm v4 RAT) | See `ioc/hashes.txt` |
| ScreenConnectPatcherV3.exe (clean) | See `patcher/clean-build/` |

## Credit

Reverse engineered using Ghidra + ILSpy. Clean rebuild preserves the legitimate ScreenConnect patching functionality with all malicious code removed.
