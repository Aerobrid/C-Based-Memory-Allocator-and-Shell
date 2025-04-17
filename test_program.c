#include <stdio.h>
#include <stdlib.h>

int main() 
{
	void *p = malloc(100);
    	if (p) {
        	printf("Allocated memory successfully\n");
        	free(p);
    	}

    	return 0;
}
