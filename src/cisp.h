#pragma once
#include <stdbool.h>

#define null ((void*)0)

typedef signed char        i8;
typedef short              i16;
typedef int                i32;
typedef long long          i64;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

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

/* ==== STR ==== */
typedef struct {
    u32 size;
    u8 data[]; // null terminated string
} cs_Str;

#define cs_strhead(str) ((cs_Str*) (((u64)(str)) - 4))
#define cstr(str) (&((str)->data))
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

/* ==== MAIN ==== */
#ifndef CS_POOL_MEM_SIZE
#define CS_POOL_MEM_SIZE 4096
#endif

typedef enum {
    CS_OK,
    CS_PARSER_ERRORS_START,
    CS_EXPONENT_AFTER_COMMA,
    CS_INVALID_ESCAPE_CHAR,
    CS_UNEXPECTED_EOF,
    CS_UNEXPECTED_CHAR,
    
    CS_RUNTIME_ERRORS_START,
    CS_OUT_OF_MEM,

    CS_COUNT,
} cs_Error;

typedef struct {
    cs_Pool obj_pool;
    char* cur; 
    char* start;
    u32 len;
    cs_Error err;
    u32 err_col, err_line;
} cs_Context;

typedef enum {
    CS_ATOM_INT,
    CS_ATOM_FLOAT,
    CS_ATOM_STR, // strings are immutable
    CS_ATOM_TRUE,
    CS_ATOM_FALSE,
    CS_ATOM_NIL,

    // symbol is associated with a value while keyword is not
    CS_ATOM_KEYWORD,
    CS_ATOM_SYMBOL,

    CS_LIST,
    CS_TYPECOUNT,
} cs_ObjectType;

#define CS_OBJECT_TYPE_OFFSET 56
#define CS_OBJECT_TYPE_MASK 0xFF00000000000000
#define CS_OBJECT_INV_TYPE_MASK 0x00FFFFFFFFFFFFFF
#define CS_OBJECT_REFCOUNT_MASK 0x00FFFFFFFF000000
#define CS_OBJECT_INV_LIST_MASK 0xFFFFFFFFFFFFFFFE
typedef struct cs_Object {
    // lowest bit decides if object is an atom
    // 0b1 => atom
    // 0b0 => list

    //  when obj is atom:
    //      car: 0xTTCCCCCCCCRRRR01
    //          T => type (8 bits)
    //          C => refcount (32 bits)
    //          R => reserved
    //      cdr: 
    //          int, float: value 
    //          string, cfunc: pointer to string
    //          symbol, keyword: hash 
    // when obj is list:
    //      car: 0xPPPPPPPPPPPPPPPP
    //          P => pointer to object
    struct cs_Object* car;
    struct cs_Object* cdr;
} cs_Object;

cs_Context cs_init();
cs_Object* cs_parse_cstr(cs_Context* c, char* content, u32 len);
cs_Object* cs_eval(cs_Context* c, cs_Object* code);
cs_Object* cs_run(cs_Context* c, char* content, u32 len);
char* cs_get_error_string(cs_Context* c);
cs_Object* cs_make_object(cs_Context* c);
void cs_obj_settype(cs_Object* obj, cs_ObjectType type);
u16 cs_obj_gettype(cs_Object *obj);