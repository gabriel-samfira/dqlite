#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"

#include "../../include/dqlite.h"
#ifdef _WIN32
#include <windows.h>
#endif

/* How large is the buffer currently */
#define SIZE(B) (B->n_pages * B->page_size)

/* How many remaining bytes the buffer currently */
#define CAP(B) (SIZE(B) - B->offset)

int buffer__init(struct buffer *b)
{
	#ifdef _WIN32
		SYSTEM_INFO system_info;
		GetSystemInfo(&system_info);
		b->page_size = (unsigned)system_info.dwPageSize;
	#else
		b->page_size = (unsigned)sysconf(_SC_PAGESIZE);
	#endif
	b->n_pages = 1;
	b->data = malloc(SIZE(b));
	if (b->data == NULL) {
		return DQLITE_NOMEM;
	}
	b->offset = 0;
	return 0;
}

void buffer__close(struct buffer *b)
{
	free(b->data);
}

/* Ensure that the buffer as at least @size spare bytes */
static bool ensure(struct buffer *b, size_t size)
{
	/* Double the buffer until we have enough capacity */
	while (size > CAP(b)) {
		void *data;
		b->n_pages *= 2;
		data = realloc(b->data, SIZE(b));
		if (data == NULL) {
			return false;
		}
		b->data = data;
	}
	return true;
}

void *buffer__advance(struct buffer *b, size_t size)
{
	void *cursor;

	if (!ensure(b, size)) {
		return NULL;
	}

	cursor = buffer__cursor(b, b->offset);
	b->offset += size;
	return cursor;
}

size_t buffer__offset(struct buffer *b) {
	return b->offset;
}

void *buffer__cursor(struct buffer *b, size_t offset)
{
	return b->data + offset;
}

void buffer__reset(struct buffer *b)
{
	b->offset = 0;
}
