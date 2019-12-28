#include <stdlib.h>
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>
#include <libgs.h>

// Mostly stolen from psxdev.net hello world - will be rewritten later

#define OT_LENGTH 1       // the ordertable length
#define PACKETMAX 18      // the maximum number of objects on the screen
#define SCREEN_WIDTH 320  // screen width
#define SCREEN_HEIGHT 240 // screen height (240 NTSC, 256 PAL)

GsOT myOT[2];                         // ordering table header
GsOT_TAG myOT_TAG[2][1 << OT_LENGTH]; // ordering table unit
PACKET GPUPacketArea[2][PACKETMAX];   // GPU packet data

static short CurrentBuffer = 0; // holds the current buffer number

static void display()
{
    // refresh the font
    FntFlush(-1);

    // get the current buffer
    CurrentBuffer = GsGetActiveBuff();

    // setup the packet workbase
    GsSetWorkBase((PACKET *)GPUPacketArea[CurrentBuffer]);

    // clear the ordering table
    GsClearOt(0, 0, &myOT[CurrentBuffer]);

    // wait for all drawing to finish
    DrawSync(0);

    // wait for v_blank interrupt
    VSync(0);

    // flip the double buffers
    GsSwapDispBuff();

    // clear the ordering table with a background color (R,G,B)
    GsSortClear(50, 50, 50, &myOT[CurrentBuffer]);

    // draw the ordering table
    GsDrawOt(&myOT[CurrentBuffer]);
}

static void graphics()
{
    SetVideoMode(1); // PAL mode
    //SetVideoMode(0); // NTSC mode

    GsInitGraph(SCREEN_WIDTH, SCREEN_HEIGHT, GsINTER | GsOFSGPU, 1, 0); // set the graphics mode resolutions (GsNONINTER for NTSC, and GsINTER for PAL)
    GsDefDispBuff(0, 0, 0, SCREEN_HEIGHT);                              // tell the GPU to draw from the top left coordinates of the framebuffer

    // init the ordertables
    myOT[0].length = OT_LENGTH;
    myOT[1].length = OT_LENGTH;
    myOT[0].org = myOT_TAG[0];
    myOT[1].org = myOT_TAG[1];

    // clear the ordertables
    GsClearOt(0, 0, &myOT[0]);
    GsClearOt(0, 0, &myOT[1]);
}

void mid_boot_entry_point(void)
{
    graphics();        // setup the graphics (seen below)
    FntLoad(960, 256); // load the font from the BIOS into the framebuffer
    SetDumpFnt(FntOpen(5, 20, 320, 240, 0, 512));

    while (1) // draw and display forever
    {
        FntPrint("             HELLO WORLD\n\n          HTTP://PSXDEV.NET/");
        display();
    }
}

void post_boot_entry_point(void)
{
    // Called when showing the PS-Logo on the boot screen.
    // Do nothing, just return control to the BIOS.

    // temp test
    mid_boot_entry_point();
}

void pre_boot_entry_point(void)
{
    // Called very early in boot up, not much stuff init'ed so not much can be done here.
    // The purpose of this entry point is to install the mid boot hook.
}
