#include <stdio.h>
#include "hellogenerate.h"
int main(int argc, char *argv[])
{
    rst_init(10, 20);
    int x = 0;
    printf("Hello!\n");
    
    rst_event(1);
	rst_event_si(5,"x value", x);	
    rst_event_lws(2, 1, 2, "3");
    rst_event_wlsfcd(3, 1, 2, "3", 4, '5', 6);
    rst_event_iwlsifcd(4, 1, 2, 3, "4", 5, 6, '7', 8);
	x++;
   rst_event_si(6,"new x value", x);	

    rst_finalize();

    return 0;
}

