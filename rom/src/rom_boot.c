#include <stdio.h>
#include <libapi.h>

#define SECTION(x) __attribute__((section(x)))

typedef struct
{
    const void* entryPointFn;
    const char licenseString[0x2C];
    const char ttyDebugString[1/*0x50*/];
} rom_header;
// Total size is 0x50+0x2C+0x4 = 0x80

void pre_boot_entry_point(void);

#ifdef DEBUG
#define LOG(format, args...) printf(format, args)
#else
#define LOG(format, args...)
#endif

// The ROM file must start with this data, however there are 2 rom headers. Once for post boot and one for preboot. As we don't use
// post boot we force some code into there to save space.
const rom_header SECTION(".gRomHeader_1F000080") gRomHeader = 
{
    pre_boot_entry_point,
    "Licensed by Sony Computer Entertainment Inc.",
     "", // A single null, the rest of the space is filled with compiled C code.
};

// This function contains code that will be used to overwrite the COP0 exception handler vector.
// The handler will be called when a hardware break point that we install is hit. Hence at that point
// the code below will execute which will jump to our mid boot entry point function.
void SECTION(".mid_boot_entry_point_1F000000") replacement_code(void)
{
    // $26 = k0, kernel temp
    __asm__
    ("
        # These 3 nops are used so we can can find the start of the payload
        # the compiler will insert stack handling and other code that we don't want
        # we only want the last 4 instructions.
        nop
        nop
        nop
        # we manually load the address of the function, this should be the same as using the pesudo
        # instruction li
        lui $26, ((mid_boot_entry_point>>16)&0xFFFF)
        ori $26, (mid_boot_entry_point&0xFFFF)
        jr  $26
        nop
    ");  
}

// Search the instructions of the above function and return a pointer to the instructions
// after the 3 nop's.
static unsigned int* SECTION(".mid_boot_entry_point_1F000000") get_replacement_code_start(void)
{
    int nopCount = 0;
    unsigned int* pFuncInstructions = (unsigned int*)replacement_code;
    for (;;)
    {
        if (!*pFuncInstructions)
        {
            // Found a nop instruction (32bits of zeroes)
            nopCount++;
        }
        else
        {
            // Isn't a nop instruction, have we found 3 consecutives nops?
            if (nopCount == 3)
            {
                // Yes and the next instruction isn't a nop so this must be the start of the code we are looking for
                return pFuncInstructions;
            }
            // Else reset the count and keep searching
            nopCount = 0;
        }
        pFuncInstructions++;
    }
    return pFuncInstructions;
}

static inline void overwrite_cop0_breakpoint_vector(void)
{
    // Same as 0x80000040 but using the uncached I/O space. Using the cached space might cause the CPU 
    // to use the cached data which will be the data before we overwrote it.
    unsigned int* kBreakPointVectorAddress = (unsigned int*)0xa0000040;

    unsigned int* pPayload = get_replacement_code_start();
    int i;

    // Looking at replacement_code() you'll see we need to overwrite 4 instructions.
    for (i=0; i<4; i++)
    {
        kBreakPointVectorAddress[i] = pPayload[i];
    }
}

#define set_BDAM(val) __asm__("mtc0	%0, $9" : : "r"(val))
#define set_BPCM(val) __asm__("mtc0	%0, $11" : : "r"(val))
#define set_BDA(val) __asm__("mtc0	%0, $5" : : "r"(val))
#define set_BPC(val) __asm__("mtc0	%0, $3" : : "r"(val))
#define set_DCIC(val) __asm__("mtc0	%0, $7" : : "r"(val))
#define set_SP(val)  __asm__("move $29, %0" : : "r"(val)) // Unlike the PSYQ SetSp this can be inlined

// Sets a break point on access to 0x80030000, this address is where the bios copies
// a small PS-EXE which is the shell program that contains the CD-Player and memory card manager.
// Therefore at this point if we take control we know that the system is booted enough to be in a usable state.
static inline void install_break_point_on_bios_shell_copy(void)
{
    // Set BPCM and BDAM masks
    set_BDAM(0xffffffff);
    set_BPCM(0xffffffff);

    // Set break on PC and data-write address

    // BPC break is for compatibility with no$psx as it does not emulate break on BDA
    set_BDA(0x80030000);
    set_BPC(0x80030000);

    // Enable break on data-write and and break on PC to DCIC control register
    set_DCIC(0xeb800000);
}

static void inline clear_break_point(void)
{
    set_DCIC(0x0);
    set_BDAM(0x0);
    set_BPCM(0x0);
    set_BDA(0x0);
    set_BPC(0x0);
}

extern const unsigned int exe_data_start;
extern const unsigned int exe_data_end;

static inline void copy_ps_exe_to_ram()
{
    unsigned char* pExeTarget = (unsigned char*)0x80010000;
    int i;
    const unsigned int* pSrc = (const unsigned int*)(0x1F0001B0); 
    unsigned int* pDst = (unsigned int*)pExeTarget;
    LOG("Copy start from 0x%X08 to 0x%X08\n", pSrc, pDst);
    for (i=0; i<61440 / 4; i++)
    {
        *pDst = *pSrc;
        pSrc++;
        pDst++;
    }
    LOG("Copy end at 0x%X08 from 0x%X08\n", pDst, pSrc);
}

typedef void(*TEntryPoint)(void);

static inline void call_ps_exe_entry_point(void)
{
    ((TEntryPoint)0x800161F8)();
}

void mid_boot_entry_point(void)
{
    clear_break_point();

    set_SP(0x801ffff0);

    // Re-enable interrupts
    ExitCriticalSection();

    LOG("Copy exe\n");
    copy_ps_exe_to_ram();

    LOG("Call proc\n");
    call_ps_exe_entry_point();

    LOG("proc returned!\n");
}

void pre_boot_entry_point(void)
{
    overwrite_cop0_breakpoint_vector();
    install_break_point_on_bios_shell_copy();
}
