#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#include "bitmap.h"

//Attribution: most parts of bitmap_get and bitmap_put function
//come from stack overflow
//stackoverflow.com/questions/16947492/looking-for-a-bitmap-implementation-api-in-linux-c

//bm: map beginning. ii: position.
int
bitmap_get(void* bm, int ii) {
	uint8_t* bitmap = (uint8_t*) bm;
	return bitmap[ii/8] & (1 << (ii&7)) ? 1 : 0;
}

//bm: ~. ii: position. vv: value want to set
void
bitmap_put(void* bm, int ii, int vv) {
	uint8_t* bitmap = (uint8_t*) bm;
	if(vv) {
		bitmap[ii/8] |= 1 << (ii & 7);
	}
	else {
		bitmap[ii/8] &= ~(1 << (ii&7));
	}
}
