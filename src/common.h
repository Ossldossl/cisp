#pragma once

#include <stdbool.h>

#define null ((void*)0)
#define reinterpret(value, as) (*(as*)&value)
#define advance_ptr(ptr, by) (((u8*)ptr) + by)

typedef signed char         i8;
typedef short               i16;
typedef int                 i32;
typedef long long           i64;
typedef unsigned char       byte;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

/* ==== POOL ALLOCATOR ====*/
#define freelist_next(freelist) (*freelist)
#define pool_next(start) ((void**)start)[0]
#define pool_data(start) (start + 8)
typedef struct {
    void* mem;
    u32 element_size; // more than 8 (or 4 on 32-bit) bytes
    void** freelist;
} cs_Pool;
cs_Pool cs_pool_init(u32 element_size);
void* cs_pool_alloc(cs_Pool* p);
void cs_pool_free(cs_Pool* p, void** ptr);
void cs_pool_release(cs_Pool* p);

/* ==== ARENA ==== */
typedef struct {
    u8* data;
    u32 used;
    u32 last_alloc; 
} cs_ArenaBucket;

typedef struct {
    cs_ArenaBucket* buckets;
    u8 buck_count;
} cs_Arena;

cs_Arena arena_init(void);
void* arena_alloc(cs_Arena* a, u32 size);
void arena_free_last(cs_Arena* a);
void arena_clear(cs_Arena* a);

/* ==== STR ==== */
typedef struct {
    u32 size;
    u8 data[]; // null terminated string
} cs_Str;

#define cs_strhead(str) ((cs_Str*) (((u64)(str)) - 4))
#define cstr(str) (char*)(&((str)->data))
#define strlit(str) ((cs_Str) {.data=str, .len=sizeof(str)})

cs_Str* cs_make_str(char* data, u32 len);
#define make_str(str) cs_make_str(str, sizeof(str)-1);

cs_Str* cs_str_init(u32 len);

typedef struct {
    u32 len;
    u32 cap;
    u8* data;
} cs_StrBuilder;

cs_StrBuilder cs_strbuilder_init(u32 cap);
void cs_strbuilder_appendc(cs_StrBuilder* b, char c);
void cs_strbuilder_appends(cs_StrBuilder* b, char* c, u32 len);
cs_Str* cs_strbuilder_finish(cs_StrBuilder* b);