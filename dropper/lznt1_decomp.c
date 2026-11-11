
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* We'll use Wine's ntdll */
/* On Linux with Wine, we can use dlopen to load ntdll.so */

/* Actually, let's just implement a proper LZNT1 decompressor */
/* Reference: ReactOS source code and NT documentation */

int lznt1_decompress(const uint8_t *input, size_t input_size, 
                      uint8_t *output, size_t output_size, size_t *final_size) {
    size_t ipos = 0, opos = 0;
    
    while (ipos < input_size && opos < output_size) {
        if (ipos + 2 > input_size) break;
        
        uint16_t header = input[ipos] | (input[ipos + 1] << 8);
        ipos += 2;
        
        uint16_t chunk_size = header & 0x0FFF;
        int is_compressed = (header & 0x8000) != 0;
        
        if (chunk_size == 0) {
            if (!is_compressed) {
                /* Uncompressed 4096-byte block */
                size_t copy = 4096;
                if (copy > output_size - opos) copy = output_size - opos;
                if (copy > input_size - ipos) copy = input_size - ipos;
                memcpy(output + opos, input + ipos, copy);
                opos += copy;
                ipos += copy;
            }
            continue;
        }
        
        size_t chunk_end = ipos + chunk_size;
        if (chunk_end > input_size) chunk_end = input_size;
        
        if (!is_compressed) {
            /* Uncompressed chunk */
            size_t copy = chunk_size;
            if (copy > output_size - opos) copy = output_size - opos;
            memcpy(output + opos, input + ipos, copy);
            opos += copy;
            ipos = chunk_end;
            continue;
        }
        
        /* Compressed chunk */
        while (ipos < chunk_end && opos < output_size) {
            uint8_t tag = input[ipos++];
            
            for (int bit = 0; bit < 8 && ipos < chunk_end && opos < output_size; bit++) {
                if (!(tag & (1 << bit))) {
                    /* Literal byte */
                    output[opos++] = input[ipos++];
                } else {
                    /* Compressed reference */
                    if (ipos + 1 >= chunk_end) {
                        ipos = chunk_end;
                        break;
                    }
                    
                    uint8_t b0 = input[ipos++];
                    uint8_t b1 = input[ipos++];
                    
                    /* Length = top 4 bits of b1 + 3 */
                    int length = ((b1 >> 4) & 0xF) + 3;
                    
                    /* Displacement (12 bits) */
                    int displacement = b0 | ((b1 & 0x0F) << 8);
                    
                    if (displacement == 0) {
                        /* RLE or end marker - skip */
                        break;
                    }
                    
                    /* Copy from output buffer */
                    int src = opos - displacement;
                    if (src < 0) {
                        memset(output + opos, 0, length);
                        opos += length;
                    } else {
                        for (int i = 0; i < length && opos < output_size; i++) {
                            output[opos++] = output[src + i];
                        }
                    }
                }
            }
        }
        
        ipos = chunk_end;
    }
    
    if (final_size) *final_size = opos;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input> <output>\n", argv[0]);
        return 1;
    }
    
    FILE *fin = fopen(argv[1], "rb");
    if (!fin) { perror("fopen input"); return 1; }
    
    fseek(fin, 0, SEEK_END);
    size_t input_size = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    
    uint8_t *input = malloc(input_size);
    fread(input, 1, input_size, fin);
    fclose(fin);
    
    size_t output_size = 0xCA400; /* 828416 */
    uint8_t *output = malloc(output_size);
    
    size_t final_size = 0;
    lznt1_decompress(input, input_size, output, output_size, &final_size);
    
    printf("Decompressed: %zu bytes (expected %zu)\n", final_size, output_size);
    printf("First 16 bytes:");
    for (int i = 0; i < 16 && i < final_size; i++) printf(" %02x", output[i]);
    printf("\n");
    
    if (final_size >= 2 && output[0] == 0x4D && output[1] == 0x5A) {
        uint32_t e_lfanew = *(uint32_t*)(output + 0x3C);
        printf("e_lfanew: 0x%x\n", e_lfanew);
        if (e_lfanew < output_size - 4 && output[e_lfanew] == 0x50 && output[e_lfanew+1] == 0x45) {
            printf("VALID PE!\n");
        }
    }
    
    FILE *fout = fopen(argv[2], "wb");
    if (!fout) { perror("fopen output"); return 1; }
    fwrite(output, 1, final_size, fout);
    fclose(fout);
    
    free(input);
    free(output);
    return 0;
}
