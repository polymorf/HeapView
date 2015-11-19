#include <string.h>
#include <stdlib.h>

void fn_overflow_loop(char *a,int max, int val) {
	for(int i=0; i<=max; i++)
		a[i]=val;
}

void fn_overflow_single(char *a, int pos, int val) {
	a[pos]=val;
}

int main() {
	char *a = malloc(0x100-8);
	char *b = malloc(0x100-8);
	char *c = malloc(0x80-8);
	free(b);
	fn_overflow_loop(a, 0x100-7, 0x81);
	fn_overflow_single(a,0x100-8,0x81);
	char *e = malloc(0x100 + 0x80-8);
	return 0;
}
