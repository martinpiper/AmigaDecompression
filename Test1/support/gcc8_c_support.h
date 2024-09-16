#pragma once

#ifdef __cplusplus
	extern "C" {
#endif

// VSCode's IntelliSense doesn't know about 68000 registers, so suppress warnings
#ifndef __INTELLISENSE__
    #define ASM __asm
#else
    #define ASM(...)
#endif

void *memcpy (void *, const void *, unsigned long);

#define INCBIN(name, file) INCBIN_SECTION(name, file, ".rodata", "")
#define INCBIN_CHIP(name, file) INCBIN_SECTION(name, file, ".INCBIN.MEMF_CHIP", "aw")
#define INCBIN_SECTION(name, file, section, flags) \
    __asm__(".pushsection " #section ", " #flags "\n" \
            ".global incbin_" #name "_start\n" \
            ".type incbin_" #name "_start, @object\n" \
            ".balign 2\n" \
            "incbin_" #name "_start:\n" \
            ".incbin \"" file "\"\n" \
			".popsection\n" \
    ); \
    extern const __attribute__((aligned(2))) char incbin_ ## name ## _start[1024*1024];

#ifdef __cplusplus
	} // extern "C"
#endif
