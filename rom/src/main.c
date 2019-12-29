
#include <stdlib.h>
#include <libgte.h>
#include <libgpu.h>
#include <libetc.h>
#include <libgs.h>
#include <libapi.h>
#include <stdio.h>

void main()
{
    DISPENV	disp;
	u_long	padd, opadd = 0;
	int	i;
	
   printf("+main\n");

 
    printf("stop cb\n");
	StopCallback();
    printf("pad\n");
	PadInit(0); // crashes
    printf("graph\n");
	ResetGraph(0);
    printf("disp\n");
	SetDefDispEnv(&disp, 0, 0,  512,  240);
	setRECT(&disp.screen, 0, 0, 256, 256);
	
	PutDispEnv(&disp);
	SetDispMask(1);

    printf("enter loop\n");
	for (;;)
    {
		VSync(0);
		PutDispEnv(&disp);
	}
	PadStop();
	ResetGraph(3);
	StopCallback();

}
