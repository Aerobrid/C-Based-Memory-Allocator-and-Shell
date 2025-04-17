#include <stdio.h>
#include <stdlib.h>

int main() 
{
	// you are allocating 100 bytes of memory using malloc()
	void *p = malloc(100);
		// if successful: print confirmation and then use free()
    	if (p) {
        	printf("Allocated memory successfully\n");
        	free(p);
    	}

    	return 0;
}
