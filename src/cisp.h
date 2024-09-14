#pragma once
#include <stdbool.h>

#define null ((void*)0)
#define reinterpret(value, as) (*(as*)&value)

#define DEFAULT_ARENA_BUCKET_SIZE 4096

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
typedef struct cs_BasicBlock cs_BasicBlock;
typedef struct cs_Context cs_Context;
typedef struct cs_Object cs_Object;
typedef struct cs_Instr cs_Instr;
typedef struct cs_SSAVar cs_SSAVar;
typedef struct cs_SSAIns cs_SSAIns;
typedef struct cs_SSAPhi cs_SSAPhi;

typedef enum cs_Error cs_Error;
typedef enum cs_ObjectType cs_ObjectType;
typedef enum cs_OpKind cs_OpKind;

#ifndef CS_POOL_MEM_SIZE
#define CS_POOL_MEM_SIZE 4096
#endif

enum cs_Error {
    CS_OK,
    CS_PARSER_ERRORS_START,
    CS_EXPONENT_AFTER_COMMA,
    CS_INVALID_ESCAPE_CHAR,
    CS_UNEXPECTED_EOF,
    CS_UNEXPECTED_CHAR,
    
    CS_RUNTIME_ERRORS_START,
    CS_OUT_OF_MEM,

    CS_COUNT,
}; 

enum cs_ObjectType {
    CS_ATOM_INT,
    CS_ATOM_FLOAT,
    CS_ATOM_STR, // strings are immutable
    CS_ATOM_TRUE,
    CS_ATOM_FALSE,
    CS_ATOM_NIL,

    // symbol is associated with a value while keyword is not
    CS_ATOM_SYMBOL,
    CS_ATOM_KEYWORD,

    CS_LIST,

    // used only for bytecode
    CS_REG, // cdr ^= reg index
    CS_BLOCK, // cdr ^= basic block pointer

    CS_TYPECOUNT,
};

#define CS_OBJECT_TYPE_OFFSET 56
#define CS_OBJECT_TYPE_MASK 0xFF00000000000000
#define CS_OBJECT_INV_TYPE_MASK 0x00FFFFFFFFFFFFFF
#define CS_OBJECT_REFCOUNT_MASK 0x00FFFFFFFF000000
#define CS_OBJECT_INV_LIST_MASK 0xFFFFFFFFFFFFFFFE
struct cs_Object {
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
    //  	cdr: 0xRRRRRRRRRRRRRRRR
    //          R => pointer to rest of list
    struct cs_Object* car;
    struct cs_Object* cdr;
};

struct cs_Context {
    cs_Pool obj_pool;
    char* cur; 
    char* start;
    u32 len;
    cs_Error err;
    u32 err_col, err_line;

    cs_BasicBlock** functions;
    u32 fns_cap, fns_used;

    // vm 
    cs_Object regs[8]; 
};

cs_Context cs_init();
cs_Object* cs_parse_cstr(cs_Context* c, char* content, u32 len);
cs_Object* cs_eval(cs_Context* c, cs_Object* code);
cs_Object* cs_run(cs_Context* c, char* content, u32 len);
char* cs_get_error_string(cs_Context* c);
cs_Object* cs_make_object(cs_Context* c);
void cs_obj_settype(cs_Object* obj, cs_ObjectType type);
u16 cs_obj_gettype(cs_Object *obj);

/* ==== VM ==== */
enum cs_OpKind {
    CS_ADD,         // dest = a CS_ADD  b 
    CS_SUB,         // dest = a CS_SUB  b 
    CS_MUL,         // dest = a CS_MUL  b 
    CS_DIV,         // dest = a CS_DIV  b 

    CS_NOT,         // dest = a CS_NOT b 
    CS_AND,         // dest = a CS_AND b 
    CS_OR,          // dest = a CS_OR b 
    CS_LSHIFT,      // dest = a CS_LSHIFT b 
    CS_RSHIFT,      // dest = a CS_RSHIFT b

    CS_GT,          // dest = a CS_GT b 
    CS_LT,          // dest = a CS_LT b 
    CS_EQ,          // dest = a CS_EQ b 

    CS_LOADI,       // dest = CS_LOAD val 0
    CS_LOADF,       // dest = CS_LOAD val 0
    CS_LOADS,       // dest = CS_LOADS val 0

    CS_CREATE_OBJ,  // dest = CS_CREATE_OBJ 0 0

    CS_REF_RETAIN,  // a
    CS_REF_RELEASE, // a
};

struct cs_SSAVar {
    u32 hash;
    u32 version;
};

struct cs_SSAIns {
    cs_SSAVar dest, a, b;
    cs_OpKind op;
};

struct cs_SSAPhi {
    struct cs_SSAPhi* next;
    cs_SSAVar dest;
    cs_SSAVar* options;
    u8 option_count;
};

struct cs_BasicBlock {
    cs_SSAIns* instrs;
    u32 instr_cap; u32 instr_count;
    cs_SSAPhi phis_head; 
    // links
    cs_BasicBlock** preds;
    u16 preds_cap; u16 preds_count;
    struct cs_BasicBlock* a;
    struct cs_BasicBlock* b;
    cs_SSAVar jump_cond; // hash == 0 if always true
};