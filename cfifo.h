/*
Single header utility providing fifo buffers which use macros from the linux kernal's circ_buf.h

In one C file, if the platform is 8 bit provide the macros:

	CFIFO_ATOMIC_READ_INT(arg)

	This must read an integer atomically, either by wrapping the read in an atomic block
	, or performing a 2 consecutive matching reads.
	Example:
	#define CFIFO_ATOMIC_READ_INT(arg1)	({volatile int _arst; do{_arst = (volatile int)(arg1);}while(_arst != (volatile int)(arg1)); _arst;})


	CFIFO_ATOMIC_BLOCK
	An example of this for AVR would be:

	#include <util/atomic.h>
	#define	CFIFO_ATOMIC_BLOCK	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)

Then:
	#define CFIFO_IMPLEMENTATION
	#include "cfifo.h"

*/
#ifndef _CFIFO_H_
#define _CFIFO_H_

	#include <stdbool.h>

//********************************************************************************************************
// Defines
//********************************************************************************************************

	typedef struct cfifo_t
	{
		char *body;		// May be NULL if capacity == 1
		int head;
		int tail;
		int capacity;	// *Must* be non0. Should be a power of 2. Actual capacity is this -1. If =1 then body may safely be NULL and the actual capacity will be 0
	} cfifo_t;

	#define CFIFO_T_BENIGN_INITIALIZER	((cfifo_t){.body=NULL,.head=0,.tail=0,.capacity=1})

//	Same behaviour and signature as cfifo_read_char() but in macro form, and does not access the head or tail atomically.
//	Suitable for use in ISR's where it is known that another ISR cannot interrupt access of the head or tail.
	#define cfifo_read_char_isr(src_ptr, dst_ptr)									\
	({																				\
		bool retval = ((src_ptr)->head != (src_ptr)->tail);							\
		if(retval)																	\
		{																			\
			*(dst_ptr) = (src_ptr)->body[(src_ptr)->tail];							\
			(src_ptr)->tail = ((src_ptr)->tail + 1) & ((src_ptr)->capacity - 1);	\
		};																			\
		retval;																		\
	})

//	Same behaviour and signature as cfifo_write_char() but in macro form, and does not access the head or tail atomically.
//	Suitable for use in ISR's where it is known that another ISR cannot interrupt access of the head or tail.
	#define cfifo_write_char_isr(dst_ptr, src)												\
	({																						\
		bool retval = (((dst_ptr)->head+1) & ((dst_ptr)->capacity-1) != (dst_ptr)->tail)	\
		if(retval)																			\
		{																					\
			(dst_ptr)->body[(dst_ptr)->head] = (src);										\
			(dst_ptr)->head = ((dst_ptr)->head + 1) & ((dst_ptr)->capacity - 1);			\
		};																					\
		retval;																				\
	})

//********************************************************************************************************
// Public prototypes
//********************************************************************************************************

//	Return a cfifo_t initialised with the body and capacity given, capacity should be a power of 2.
	cfifo_t cfifo_init(void *body, int capacity);

//	Attempt to write src_size bytes to the destination fifo. The actual number of bytes written is returned.
//	If src is NULL, the buffer body will not be modified, and only the head will be updated as if the write had taken place.
//	The new .head is updated within an atomic block.
	int cfifo_write(cfifo_t *dst, const void *src, int src_size);

//	Write to a cfifo_t using a read function: int (*readfunc)(void *ctx, void *dst, int dst_size)
//	readfunc() will always be called exactly once, even if the fifo is empty, in which case it will be passed a size of 0
//	The argument passed to dst_size will be the space from the head to the buffer end. This may be less than the total available space.
//	readfunc() must write up to a maximum of dst_size bytes to *dst, and return the actual number of bytes written (>=0) or a negative value.
//	The return value is that returned by readfunc()
	int cfifo_write_using(cfifo_t *dst, int (*readfunc)(void *ctx, void *dst, int dst_size), void *readfunc_ctx);

//	Attempt to write a single char to the destination fifo.
//	Returns true if a character was written. Faster than using cfifo_write()
	bool cfifo_write_char(cfifo_t *dst, char src);

//	Attempt to read src_size bytes from the source fifo. The actual number of bytes read is returned.
//	If dst is NULL, the data will be removed from the buffer and discarded.
//	The new .tail is updated within an atomic block.
	int cfifo_read(cfifo_t *src, void *dst, int dst_size);

//	Read from a cfifo_t to a write function: int (*writefunc)(void *ctx, const void *src, int src_size)
//	writefunc() will always be called exactly once, even if the fifo is empty, in which case it will be passed a size of 0
//	The argument passed to src_size will be the count from the tail to the buffer end. This may be less than the total count.
//	writefunc() must accept up to a maximum of src_size bytes from *src, and return the actual number of bytes accepted (>=0) or a negative value.
//	The return value is that returned by writefunc()
	int cfifo_read_using(cfifo_t *dst, int (*writefunc)(void *ctx, const void *src, int src_size), void *writefunc_ctx);

//	Attempt to read a single char from the source fifo.
//	If dst is NULL, the char will be removed from the buffer and discarded.
//	Returns true if a char was read. Faster than using cfifo_read()
	bool cfifo_read_char(cfifo_t *src, char *dst);

//	Return number of bytes used in the buffer.
	int cfifo_cnt(cfifo_t *src);

//	Return available space in the buffer.
	int cfifo_space(cfifo_t *src);

//	Return number of bytes used in the buffer, up to the end of the buffer.
	int cfifo_cnt_to_end(cfifo_t *src);

//	Return available space in the buffer, up to the end of the buffer.
	int cfifo_space_to_end(cfifo_t *src);

#endif	// _CFIFO_H_

#ifdef CFIFO_IMPLEMENTATION

	#include <string.h>

//********************************************************************************************************
// Local defines
//********************************************************************************************************

	#define MIN(a,b) ({typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })


	#ifndef CFIFO_ATOMIC_READ_INT
		#ifdef __AVR__
		#warning "Is this an 8-bit platform? you should provide CFIFO_ATOMIC_READ_INT()"
		#endif
		#define CFIFO_ATOMIC_READ_INT(arg)	(arg)
	#endif

	#ifndef CFIFO_ATOMIC_BLOCK
		#ifdef __AVR__
		#warning "Is this an 8-bit platform? you should provide CFIFO_ATOMIC_BLOCK()"
		#endif
		#define CFIFO_ATOMIC_BLOCK
	#endif

// The below macros are from circ_buf.h in the linux kernal 
// https://www.kernel.org/doc/Documentation/circular-buffers.txt

/* Return count in buffer.  */
	#define CIRC_CNT(head,tail,size) (((head) - (tail)) & ((size)-1))

/* Return space available, 0..size-1.  We always leave one free char
   as a completely full buffer has head == tail, which is the same as
   empty.  */
	#define CIRC_SPACE(head,tail,size) CIRC_CNT((tail),((head)+1),(size))

/* Return count up to the end of the buffer.  Carefully avoid
   accessing head and tail more than once, so they can change
   underneath us without returning inconsistent results.  */
	#define CIRC_CNT_TO_END(head,tail,size) \
	({int end = (size) - (tail); \
	  int n = ((head) + end) & ((size)-1); \
	  n < end ? n : end;})

/* Return space available up to the end of the buffer.  */
	#define CIRC_SPACE_TO_END(head,tail,size) \
	({int end = (size) - 1 - (head); \
	  int n = (end + (tail)) & ((size)-1); \
	  n <= end ? n : end+1;})

//********************************************************************************************************
// Private prototypes
//********************************************************************************************************

    // memcpy src_size bytes from src to dst, where dst wraps within dst_start->(dst_start+dst_size)
    static void memwrap(void *dst_start, int dst_size, void *dst, const void *src, int src_size);

    // memcpy dst_size bytes from src to dst, where src wraps within src_start->(src_start+src_size).
    static void memunwrap(void *dst, int dst_size, const void *src_start, int src_size, const void *src);

	// calculate the largest power of 2 that is =< x
	static int largest_power_of_two(int x);

//********************************************************************************************************
// Public functions
//********************************************************************************************************

int cfifo_write(cfifo_t *dst, const void *src, int src_size)
{
	int tail = CFIFO_ATOMIC_READ_INT(dst->tail);
	int write_size = MIN(CIRC_SPACE(dst->head, tail, dst->capacity), src_size);
	int new_head;
	if(write_size)
	{
		if(src)
			memwrap(dst->body, dst->capacity, &dst->body[dst->head], src, write_size);
		new_head = (dst->head + write_size) & (dst->capacity - 1);
		CFIFO_ATOMIC_BLOCK
		{
			dst->head = new_head;
		};
	};
	return write_size;
}

int cfifo_write_using(cfifo_t *dst, int (*readfunc)(void *ctx, void *dst, int dst_size), void *readfunc_ctx)
{
	int tail = CFIFO_ATOMIC_READ_INT(dst->tail);
	int write_size = CIRC_SPACE_TO_END(dst->head, tail, dst->capacity);
	int new_head;
	int retval = readfunc(readfunc_ctx, dst->body ? &dst->body[dst->head]:NULL, write_size);
	if(retval > 0)
	{
		new_head = (dst->head + retval) & (dst->capacity - 1);
		CFIFO_ATOMIC_BLOCK
		{
			dst->head = new_head;
		};
	};
	return retval;
}

bool cfifo_write_char(cfifo_t *dst, char src)
{
	int tail = CFIFO_ATOMIC_READ_INT(dst->tail);
	bool do_write = !!CIRC_SPACE(dst->head, tail, dst->capacity);
	int new_head;
	if(do_write)
	{
		dst->body[dst->head] = src;
		new_head = (dst->head + 1) & (dst->capacity - 1);
		CFIFO_ATOMIC_BLOCK
		{
			dst->head = new_head;
		};
	};
	return do_write;
}

int cfifo_read(cfifo_t *src, void *dst, int dst_size)
{
	int head = CFIFO_ATOMIC_READ_INT(src->head);
	int read_size = MIN(CIRC_CNT(head, src->tail, src->capacity), dst_size);
	int new_tail;
	if(read_size)
	{
		if(dst)
			memunwrap(dst, read_size, src->body, src->capacity, &src->body[src->tail]);
		new_tail = (src->tail + read_size) & (src->capacity - 1);
		CFIFO_ATOMIC_BLOCK
		{
			src->tail = new_tail;
		};
	};
	return read_size;
}

int cfifo_read_using(cfifo_t *src, int (*writefunc)(void *ctx, const void *src, int src_size), void *writefunc_ctx)
{
	int head = CFIFO_ATOMIC_READ_INT(src->head);
	int read_size = CIRC_CNT_TO_END(head, src->tail, src->capacity);
	int new_tail;
	int retval = writefunc(writefunc_ctx, src->body ? &src->body[src->tail]:NULL, read_size);
	if(retval > 0)
	{
		new_tail = (src->tail + retval) & (src->capacity - 1);
		CFIFO_ATOMIC_BLOCK
		{
			src->tail = new_tail;
		};
	};
	return retval;
}

bool cfifo_read_char(cfifo_t *src, char *dst)
{
	int head = CFIFO_ATOMIC_READ_INT(src->head);
	bool do_read = (head != src->tail);
	int new_tail;
	if(do_read)
	{
		if(dst)
			*dst = src->body[src->tail];
		new_tail = (src->tail + 1) & (src->capacity - 1);
		CFIFO_ATOMIC_BLOCK
		{
			src->tail = new_tail;
		};
	};
	return do_read;
}

cfifo_t cfifo_init(void *body, int capacity)
{
	cfifo_t retval = (cfifo_t){.body=body, .head=0, .tail=0, .capacity=largest_power_of_two(capacity)};
	if(!body || !retval.capacity)
		retval.capacity = 1;
	return retval;
}

int cfifo_cnt(cfifo_t *src)
{
	int head = CFIFO_ATOMIC_READ_INT(src->head);
	return CIRC_CNT(head, src->tail, src->capacity);
}

int cfifo_space(cfifo_t *src)
{
	int tail = CFIFO_ATOMIC_READ_INT(src->tail);
	return CIRC_SPACE(src->head, tail, src->capacity);
}

int cfifo_cnt_to_end(cfifo_t *src)
{
	int head = CFIFO_ATOMIC_READ_INT(src->head);
	return CIRC_CNT_TO_END(head, src->tail, src->capacity);
}

int cfifo_space_to_end(cfifo_t *src)
{
	int tail = CFIFO_ATOMIC_READ_INT(src->tail);
	return CIRC_SPACE_TO_END(src->head, tail, src->capacity);
}

//********************************************************************************************************
// Private functions
//********************************************************************************************************

static int largest_power_of_two(int x)
{
	int n = 0;
	if(x < 0)
		x = 0;
	while(x)
	{
		x >>= 1;
		n++;
	};
	return n ? (1 << (n-1)):0;
}

static void memwrap(void *dst_start, int dst_size, void *dst, const void *src, int src_size)
{
    int space_to_end;
    int chunk_size;

    while(src_size)
    {    
        space_to_end = dst_size - (dst - dst_start);
        chunk_size = space_to_end < src_size ? space_to_end:src_size;
        memcpy(dst, src, chunk_size);
        src += chunk_size;
        src_size -= chunk_size;
        dst += chunk_size;
        if(dst == dst_start + dst_size)
            dst = dst_start;
    };
}

static void memunwrap(void *dst, int dst_size, const void *src_start, int src_size, const void *src)
{
    int space_to_end;
    int chunk_size;

    while(dst_size)
    {
        space_to_end = src_size - (src - src_start);
        chunk_size = space_to_end < dst_size ? space_to_end:dst_size;
        memcpy(dst, src, chunk_size);
        dst += chunk_size;
        dst_size -= chunk_size;
        src += chunk_size;
        if(src == src_start + src_size)
            src = src_start;
    };
}

#endif //CFIFO_IMPLEMENTATION
