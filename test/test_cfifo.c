#include <string.h>
#include "greatest.h"

#define CFIFO_IMPLEMENTATION
#include "../cfifo.h"

TEST init_and_capacity(void)
{
    char body[300];
    cfifo_t f = cfifo_init(body, sizeof(body));

    ASSERT_EQ(256, f.capacity);
    ASSERT_EQ(0, f.head);
    ASSERT_EQ(0, f.tail);
    ASSERT_EQ(0, cfifo_cnt(&f));
    ASSERT_EQ(255, cfifo_space(&f));
    PASS();
}

TEST benign_initialisation(void)
{
    char body[8];
    cfifo_t a = cfifo_init(NULL, 8);
    cfifo_t b = cfifo_init(body, 0);
    cfifo_t c = cfifo_init(body, -10);
    cfifo_t d = CFIFO_T_BENIGN_INITIALIZER;

    ASSERT_EQ(1, a.capacity);
    ASSERT_EQ(1, b.capacity);
    ASSERT_EQ(1, c.capacity);
    ASSERT_EQ(1, d.capacity);
    ASSERT_EQ(0, cfifo_space(&a));
    ASSERT_EQ(0, cfifo_cnt(&d));
    PASS();
}

TEST bulk_write_read(void)
{
    char body[8];
    char out[8] = {0};
    const char data[] = "abcdefg";
    cfifo_t f = cfifo_init(body, sizeof(body));

    ASSERT_EQ(7, cfifo_write(&f, data, 7));
    ASSERT_EQ(7, cfifo_cnt(&f));
    ASSERT_EQ(0, cfifo_space(&f));
    ASSERT_EQ(0, cfifo_write(&f, "x", 1));
    ASSERT_EQ(7, cfifo_read(&f, out, 7));
    ASSERT_MEM_EQ(data, out, 7);
    ASSERT_EQ(0, cfifo_cnt(&f));
    PASS();
}

TEST wrapping(void)
{
    char body[8];
    char out[8] = {0};
    cfifo_t f = cfifo_init(body, sizeof(body));

    ASSERT_EQ(6, cfifo_write(&f, "abcdef", 6));
    ASSERT_EQ(5, cfifo_read(&f, out, 5));
    ASSERT_MEM_EQ("abcde", out, 5);
    ASSERT_EQ(6, cfifo_write(&f, "ghijkl", 6));
    memset(out, 0, sizeof(out));
    ASSERT_EQ(7, cfifo_read(&f, out, 7));
    ASSERT_MEM_EQ("fghijkl", out, 7);
    PASS();
}

TEST char_operations(void)
{
    char body[4];
    char ch = 0;
    cfifo_t f = cfifo_init(body, sizeof(body));

    ASSERT(cfifo_write_char(&f, 'A'));
    ASSERT(cfifo_write_char(&f, 'B'));
    ASSERT(cfifo_write_char(&f, 'C'));
    ASSERT_FALSE(cfifo_write_char(&f, 'D'));
    ASSERT(cfifo_read_char(&f, &ch)); ASSERT_EQ('A', ch);
    ASSERT(cfifo_read_char(&f, &ch)); ASSERT_EQ('B', ch);
    ASSERT(cfifo_read_char(&f, &ch)); ASSERT_EQ('C', ch);
    ASSERT_FALSE(cfifo_read_char(&f, &ch));
    PASS();
}

TEST isr_char_operations(void)
{
    char body[4];
    char ch = 0;
    cfifo_t f = cfifo_init(body, sizeof(body));

    ASSERT(cfifo_write_char_isr(&f, 'x'));
    ASSERT(cfifo_write_char_isr(&f, 'y'));
    ASSERT(cfifo_write_char_isr(&f, 'z'));
    ASSERT_FALSE(cfifo_write_char_isr(&f, '!'));
    ASSERT(cfifo_read_char_isr(&f, &ch)); ASSERT_EQ('x', ch);
    ASSERT(cfifo_read_char_isr(&f, &ch)); ASSERT_EQ('y', ch);
    ASSERT(cfifo_read_char_isr(&f, &ch)); ASSERT_EQ('z', ch);
    ASSERT_FALSE(cfifo_read_char_isr(&f, &ch));
    PASS();
}

TEST null_and_invalid_sizes(void)
{
    char body[8] = {0};
    char out[4];
    cfifo_t f = cfifo_init(body, sizeof(body));

    ASSERT_EQ(0, cfifo_write(&f, "abc", 0));
    ASSERT_EQ(0, cfifo_write(&f, "abc", -1));
    ASSERT_EQ(3, cfifo_write(&f, NULL, 3));
    ASSERT_EQ(3, cfifo_cnt(&f));
    ASSERT_EQ(0, cfifo_read(&f, out, 0));
    ASSERT_EQ(0, cfifo_read(&f, out, -1));
    ASSERT_EQ(2, cfifo_read(&f, NULL, 2));
    ASSERT_EQ(1, cfifo_cnt(&f));
    PASS();
}

typedef struct callback_ctx_t
{
    const char *read_src;
    char *write_dst;
    int amount;
    int size_seen;
    int calls;
} callback_ctx_t;

static int read_callback(void *ctx_ptr, void *dst, int dst_size)
{
    callback_ctx_t *ctx = ctx_ptr;
    int n = ctx->amount < dst_size ? ctx->amount : dst_size;
    ctx->calls++;
    ctx->size_seen = dst_size;
    if(n > 0)
        memcpy(dst, ctx->read_src, n);
    return n;
}

static int write_callback(void *ctx_ptr, const void *src, int src_size)
{
    callback_ctx_t *ctx = ctx_ptr;
    int n = ctx->amount < src_size ? ctx->amount : src_size;
    ctx->calls++;
    ctx->size_seen = src_size;
    if(n > 0)
        memcpy(ctx->write_dst, src, n);
    return n;
}
TEST using_callbacks(void)
{
    char body[8];
    char out[8] = {0};
    cfifo_t f = cfifo_init(body, sizeof(body));
    callback_ctx_t r = {.read_src="abcdef", .amount=6};
    callback_ctx_t w = {.write_dst=out, .amount=4};

    ASSERT_EQ(6, cfifo_write_using(&f, read_callback, &r));
    ASSERT_EQ(1, r.calls);
    ASSERT_EQ(7, r.size_seen);
    ASSERT_EQ(6, cfifo_cnt(&f));
    ASSERT_EQ(4, cfifo_read_using(&f, write_callback, &w));
    ASSERT_EQ(1, w.calls);
    ASSERT_EQ(6, w.size_seen);
    ASSERT_MEM_EQ("abcd", out, 4);
    ASSERT_EQ(2, cfifo_cnt(&f));
    PASS();
}

TEST using_callbacks_wrap_boundary(void)
{
    char body[8];
    char tmp[8];
    cfifo_t f = cfifo_init(body, sizeof(body));
    callback_ctx_t r = {.read_src="WXYZ", .amount=4};

    ASSERT_EQ(6, cfifo_write(&f, "abcdef", 6));
    ASSERT_EQ(5, cfifo_read(&f, tmp, 5));
    ASSERT_EQ(2, cfifo_write_using(&f, read_callback, &r));
    ASSERT_EQ(2, r.size_seen);
    ASSERT_EQ(3, cfifo_cnt(&f));
    ASSERT_EQ(3, cfifo_read(&f, tmp, sizeof(tmp)));
    ASSERT_MEM_EQ("fWX", tmp, 3);
    PASS();
}

TEST using_callback_called_on_empty_or_full(void)
{
    char body[4];
    char out[4];
    cfifo_t f = cfifo_init(body, sizeof(body));
    callback_ctx_t w = {.write_dst=out, .amount=1};
    callback_ctx_t r = {.read_src="x", .amount=1};

    ASSERT_EQ(0, cfifo_read_using(&f, write_callback, &w));
    ASSERT_EQ(1, w.calls);
    ASSERT_EQ(0, w.size_seen);
    ASSERT_EQ(3, cfifo_write(&f, "abc", 3));
    ASSERT_EQ(0, cfifo_write_using(&f, read_callback, &r));
    ASSERT_EQ(1, r.calls);
    ASSERT_EQ(0, r.size_seen);
    PASS();
}

SUITE(cfifo_suite)
{
    RUN_TEST(init_and_capacity);
    RUN_TEST(benign_initialisation);
    RUN_TEST(bulk_write_read);
    RUN_TEST(wrapping);
    RUN_TEST(char_operations);
    RUN_TEST(isr_char_operations);
    RUN_TEST(null_and_invalid_sizes);
    RUN_TEST(using_callbacks);
    RUN_TEST(using_callbacks_wrap_boundary);
    RUN_TEST(using_callback_called_on_empty_or_full);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv)
{
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(cfifo_suite);
    GREATEST_MAIN_END();
}
