#include <stdio.h>
#include <libapi.h>

typedef struct
{
    const void* entryPointFn;
    const char licenseString[0x2C];
    const char ttyDebugString[0x50];
} rom_header;

void pre_boot_entry_point(void);
void post_boot_entry_point(void);

// This must be the first thing in the produced binary for the ROM file format to be correct
const rom_header gRomHeader[2] = 
{
    {
        post_boot_entry_point,
        "Licensed by Sony Computer Entertainment Inc.",
        "post boot message"
    },
    {
        pre_boot_entry_point,
        "Licensed by Sony Computer Entertainment Inc.",
        "pre boot message"
    }
};

// This function contains code that will be used to overwrite the COP0 exception handler vector.
// The handler will be called when a hardware break point that we install is hit. Hence at that point
// the code below will execute which will jump to our mid boot entry point function.
void replacement_code(void)
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
unsigned int* get_replacement_code_start(void)
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

// Same as 0x80000040 but using the uncached I/O space. Using the cached space might cause the CPU 
// to use the cached data which will be the data before we overwrote it.
unsigned int* BREAK_ADDR  =(unsigned int*)0xa0000040;

void overwrite_cop0_exception_vector(void)
{
    unsigned int* pPayload = get_replacement_code_start();
    int i;

    // Looking at replacement_code() you'll see we need to overwrite 4 instructions.
    for (i=0; i<4; i++)
    {
        BREAK_ADDR[i] = pPayload[i];
    }
}

#define set_BDAM(val) __asm__("mtc0	%0, $9" : : "r"(val))
#define set_BPCM(val) __asm__("mtc0	%0, $11" : : "r"(val))
#define set_BDA(val) __asm__("mtc0	%0, $5" : : "r"(val))
#define set_BPC(val) __asm__("mtc0	%0, $3" : : "r"(val))
#define set_DCIC(val) __asm__("mtc0	%0, $7" : : "r"(val))

// Sets a break point on access to 0x80030000, this address is where the bios copies
// a small PS-EXE which is the shell program that contains the CD-Player and memory card manager.
// Therefore at this point if we take control we know that the system is booted enough to be in a usable state.
void install_break_point(void)
{
    // Set BPCM and BDAM masks
    set_BDAM(0xffffffff);
    set_BPCM(0xffffffff);

    // Set break on PC and data-write address

    // BPC break is for compatibility with no$psx as it does not emulate break on BDA
    set_BDA(0x80030000); // vs 0xbfc0db74
    set_BPC(0x80030000);

    // Enable break on data-write and and break on PC to DCIC control register
    set_DCIC(0xeb800000);
}

void clear_break_point(void)
{
    set_DCIC(0x0);
    set_BDAM(0x0);
    set_BPCM(0x0);
    set_BDA(0x0);
    set_BPC(0x0);
}

void SetGp(unsigned int* gp);

void main();

void SetDefaultExitFromException(void);

void mid_boot_entry_point(void)
{
    //unsigned int gp =0x8000c000; // 0x1F006CF4;

    clear_break_point();

    //SetSp(0x801ffff0);
    //SetGp(&gp);

    //
    //SetDefaultExitFromException();

    // Re-enable interrupts
    ExitCriticalSection();
   
    main();
}

void pre_boot_entry_point(void)
{
    overwrite_cop0_exception_vector();
    install_break_point();
}

void post_boot_entry_point(void)
{
    printf("postboot\n");

    // Called when showing the PS-Logo on the boot screen.
    // Do nothing, just return control to the BIOS.
}
