#include <string.h>
#include <stdlib.h>
int main() {
	char *a = malloc(0x100-8);
	char *b = malloc(0x100-8);
	char *c = malloc(0x80-8);
	char *d = malloc(0x120);
	memset(d,255,0x120);
	memset(b,22,0x100-8);
	memset(c,33,0x80-8);
	free(b);
	/* overflow in memcpy */
	memcpy(a+0x40,d,0xC0-8+1);
	char *e = malloc(0x100 + 0x80-8);
	return 0;
}
