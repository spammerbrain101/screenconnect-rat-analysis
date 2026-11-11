/*
 * =============================================================================
 * Unpacker Analysis - ScreenConnect Patcher V3
 * =============================================================================
 * 
 * PACKER TYPE: Custom XOR-based packer with junk code obfuscation
 * 
 * PACKING STRUCTURE:
 * 
 *   Original PE Layout:
 *   +------------------+
 *   | PE Headers       | 0x000 - 0x1FF
 *   | .text section    | 0x200 - 0x13E3FF
 *   |   [ENCRYPTED]    | 0x200 - 0xD453C+0x200 (payload, XOR encrypted)
 *   |   [UNPACKER]     | 0x53DFEA - 0x53FXXX (unpacker stub + anti-analysis)
 *   |   [JUNK CODE]    | (filler/garbage between)
 *   | .idata section   | 0x140000 (import table - minimal imports)
 *   | .rsrc section    | 0x141000 (resources - embedded binaries)
 *   +------------------+
 * 
 * ENTRY POINT SEQUENCE:
 *   0x401000: call 0x53DFEA    ; jump to unpacker
 *   0x401005: push 0           ; ExitProcess(0)
 *   0x401007: call ExitProcess
 * 
 * UNPACKER FLOW:
 * 
 *   1. FUN_0053E46D() - Anti-Analysis Checks
 *      ├── malloc(100MB) / free - memory allocation test
 *      ├── SetErrorMode(0x400) / SetErrorMode(0) - SEM_FAILCRITICALERRORS check
 *      ├── VirtualAllocExNuma(-1, NULL, 1000, 0x3000, 4, 0) - NUMA detection
 *      ├── GetComputerNameA() vs blocklist:
 *      │   ├── "NfZtFbPfH"  (sandbox/VM hostname)
 *      │   ├── "ELICZ"       (analysis environment)
 *      │   ├── "tz"          (minimal VM)
 *      │   └── "MAIN"        (sandbox host)
 *      ├── GetModuleFileNameA() vs path blocklist:
 *      │   ├── "C:\Documents and Settings\Administrator\My Documents\mwsmpl.exe"
 *      │   ├── "C:\SELF.EXE"
 *      │   └── "myapp.exe"
 *      └── ExitProcess(0) if any check triggers
 * 
 *   2. API Resolution (inline, no IAT)
 *      hKernel32 = GetModuleHandleW(L"kernel32.dll")
 *      pVirtualProtect = GetProcAddress(hKernel32, "VirtualProtect")
 * 
 *   3. Payload Decryption
 *      VirtualProtect(0x401000, 0xD353C, PAGE_READWRITE, &oldProtect)
 *      
 *      XOR-Decode Algorithm:
 *        key1 = 0xE583A01B  (32-bit rolling XOR key)
 *        key2 = 0x0DA45DD3  (32-bit skip control key)
 *        src_ptr = 0x40100E  (encrypted data start, RVA)
 *        dst_ptr = 0x401000  (decrypted output start, RVA)
 *        count   = 0xD353C  (865,596 bytes)
 *        
 *        while (count > 0):
 *            *dst_ptr = *src_ptr XOR (key1 & 0xFF)
 *            key1 = (ROL32(key1, 5) * 7) & 0xFFFFFFFF
 *            bit = key2 & 1
 *            key2 = ROR32(key2, 1)
 *            src_ptr += 2 if bit else 1   // selective source skip
 *            dst_ptr += 1
 *            count -= 1
 *      
 *      VirtualProtect(0x401000, 0xD353C, PAGE_EXECUTE_READ, &oldProtect)
 * 
 *   4. Payload Execution
 *      CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)0x401000, ..., 0, NULL)
 *      while (TRUE) Sleep(1000)
 * 
 * DECODED PAYLOAD CHARACTERISTICS:
 *   - Entry: push ebp; mov ebp, esp (0x55 0x89 0xE5) - standard prologue
 *   - 4 real functions found after full analysis
 *   - Uses hash-based API resolution (ROR-13 hashes) for all API calls
 *   - No visible IAT usage - all imports resolved dynamically
 *   - Payload contains embedded data:
 *     * javaw.com binary at 0x4018AA (76,800 bytes)
 *     * Compressed data at 0x4140AA (786,578 bytes -> 827,392 bytes)
 *   - Decompression uses ntdll!RtlDecompressBuffer (LZNT1 format)
 * 
 * JUNK CODE OBFUSCATION PATTERNS OBSERVED:
 *   - pushfd / popfd (save/restore flags - no effect)
 *   - xor reg, reg / xor reg, reg (cancel out)
 *   - inc reg / dec reg (cancel out)
 *   - mov reg, imm / mov reg, imm2 (dead store)
 *   - and reg, 0 / or reg, 0 (no effect)
 *   - Dead xchg, in, int, ror instructions
 *   - FPU instructions in integer code (fsubr, etc.)
 */

