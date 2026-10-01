# cfifo

`cfifo` is a small single-header circular FIFO library for C, intended primarily for embedded systems and producer/consumer communication between normal code and interrupt service routines.

The implementation uses the circular-buffer arithmetic from the Linux kernel `circ_buf.h` approach. Buffer sizes are powers of two and one element is deliberately left unused, allowing `head == tail` to unambiguously represent an empty FIFO.

## Features

- Single-header implementation (`cfifo.h`)
- Bulk and single-character reads and writes
- Efficient power-of-two index wrapping
- Optional atomic access hooks for 8-bit targets
- Fast ISR-specific character read/write macros
- Callback-based read/write functions for direct transfer to another API
- Reads to `NULL` can discard data
- Writes from `NULL` can reserve/advance FIFO space
- No dynamic memory allocation

## Adding cfifo to a project

Include `cfifo.h` normally wherever the declarations are required:

```c
#include "cfifo.h"
```

In exactly one C source file, define `CFIFO_IMPLEMENTATION` before including the header:
```c
#define CFIFO_IMPLEMENTATION
#include "cfifo.h"
```

## Basic use

The backing buffer is supplied by the application:

```c
char storage[128];
cfifo_t fifo = cfifo_init(storage, sizeof(storage));

const char message[] = "hello";
cfifo_write(&fifo, message, 5);

char result[5];
int count = cfifo_read(&fifo, result, sizeof(result));
```

`cfifo_init()` rounds the supplied capacity down to the largest power of two that fits. Because one byte is reserved to distinguish full from empty, the usable capacity is one less than the resulting power of two. For example, a 128-byte backing buffer has 127 bytes of usable FIFO space.

A FIFO with internal capacity 1 has zero usable space and may safely have a `NULL` body. `CFIFO_T_BENIGN_INITIALIZER` provides this state directly.

## API

### Initialisation

```c
cfifo_t cfifo_init(void *body, int capacity);
```
### Reading and writing

```c
int  cfifo_write(cfifo_t *dst, const void *src, int src_size);
int  cfifo_read(cfifo_t *src, void *dst, int dst_size);
bool cfifo_write_char(cfifo_t *dst, char src);
bool cfifo_read_char(cfifo_t *src, char *dst);
```

`cfifo_write()` and `cfifo_read()` return the number of bytes actually transferred. A request larger than the available space or data is automatically limited. Non-positive requested sizes transfer nothing.

Passing `NULL` as the source to `cfifo_write()` advances the head without modifying the backing storage. Passing `NULL` as the destination to `cfifo_read()` removes and discards the corresponding data.

### State queries

```c
int cfifo_cnt(cfifo_t *src);
int cfifo_space(cfifo_t *src);
int cfifo_cnt_to_end(cfifo_t *src);
int cfifo_space_to_end(cfifo_t *src);
```

The `_to_end` forms report only the contiguous region between the current head or tail and the physical end of the backing buffer.

## Callback transfers

The callback APIs allow data to move directly between a FIFO and an external read/write-style function without an intermediate buffer:

```c
int cfifo_write_using(cfifo_t *dst,
    int (*readfunc)(void *ctx, void *dst, int dst_size), void *ctx);

int cfifo_read_using(cfifo_t *src,
    int (*writefunc)(void *ctx, const void *src, int src_size), void *ctx);
```
Each callback is called exactly once, including when no transfer is possible, in which case the supplied size is zero. The supplied region is always contiguous and therefore may be smaller than the FIFO's total count or total free space.

A callback must not report that it transferred more bytes than the size it was given. Contract violations are not checked by `cfifo`.

## Interrupt and atomic access

The FIFO is designed for single-producer/single-consumer use. On platforms where an `int` cannot be read atomically, define `CFIFO_ATOMIC_READ_INT` and `CFIFO_ATOMIC_BLOCK` before the implementation include.

For example, on AVR:

```c
#include <util/atomic.h>

#define CFIFO_ATOMIC_READ_INT(arg) \
    ({ volatile int v; do { v = (volatile int)(arg); } \
       while(v != (volatile int)(arg)); v; })

#define CFIFO_ATOMIC_BLOCK ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
#define CFIFO_IMPLEMENTATION
#include "cfifo.h"
```

For character-at-a-time operations inside an ISR, `cfifo_read_char_isr()` and `cfifo_write_char_isr()` avoid atomic access overhead. They should only be used where another ISR cannot interrupt and concurrently access the relevant FIFO indices.

## Tests

The `test/` directory contains a test suite using the [greatest](https://github.com/silentbicycle/greatest) C testing framework.

Build and run it with:

```sh
cd test
make test
```
The suite covers capacity rounding and benign initialization, full/empty operation, bulk transfers, wraparound, character and ISR operations, zero/negative transfer sizes, `NULL` discard/reserve behaviour, and callback transfers including physical wrap boundaries.

## Implementation notes

`cfifo` uses GNU C extensions, including statement expressions and `typeof`, and is intended to be compiled with a GNU C dialect such as `-std=gnu99`.

The circular-buffer macros are derived from the Linux kernel circular buffer documentation:

https://www.kernel.org/doc/Documentation/circular-buffers.txt

## Credits

`cfifo` was written by Michael Clift.

This README and the `greatest`-based test suite were authored by **OpenAI GPT-5.6 Sol** in collaboration with Michael Clift during review and testing of the library.

The circular-buffer arithmetic is based on macros from the Linux kernel `circ_buf.h` implementation. The test suite uses the `greatest` C testing framework by Scott Vokes and contributors.
