#include "gcc8_c_support.h"
#include <proto/exec.h>
extern struct ExecBase* SysBase;




__attribute__((optimize("no-tree-loop-distribute-patterns"))) 
void* memcpy(void *dest, const void *src, unsigned long len) {
	char *d = (char *)dest;
	const char *s = (const char *)src;
	while(len--)
		*d++ = *s++;
	return dest;
}





int main();

__attribute__((used)) __attribute__((section(".text.unlikely"))) void _start() {
	main();
}


