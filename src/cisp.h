#pragma once
#include "map.h"
#include "common.h"

#define DEFAULT_ARENA_BUCKET_SIZE 4096
#define DEFAULT_BB_INS_START_CAP 5
#define DEFAULT_BB_PRED_CAP 1

/* ==== MAIN ==== */
typedef struct cs_BasicBlock cs_BasicBlock;
typedef struct cs_Context cs_Context;
typedef struct cs_Object cs_Object;
typedef struct cs_SSAVar cs_SSAVar;
typedef struct cs_SSAIns cs_SSAIns;
typedef struct cs_SSAPhi cs_SSAPhi;
typedef struct cs_Code cs_Code;

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
    CS_TEMP_RESERVED,

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

    cs_BasicBlock** functions; // [0] == entry point
    u32 fns_cap, fns_used;

    cs_BasicBlock* cur_bb;
    u32 cur_temp_id;

    // vm 
    cs_Object regs[16]; 
};

cs_Context cs_init();
cs_Object* cs_run(cs_Context* c, cs_Code* code);
cs_Code* cs_compile_file(cs_Context* c, char* content, u32 len);
char* cs_get_error_string(cs_Context* c);
cs_Object* cs_make_object(cs_Context* c);
void cs_obj_settype(cs_Object* obj, cs_ObjectType type);
u16 cs_obj_gettype(cs_Object *obj);

/* ==== VM ==== */
#define CS_OPKIND_LIST \
    X(CS_ADD) \
    X(CS_SUB) \
    X(CS_MUL) \
    X(CS_DIV) \
    X(CS_NOT) \
    X(CS_AND) \
    X(CS_OR) \
    X(CS_LSHIFT) \
    X(CS_RSHIFT) \
    X(CS_GT) \
    X(CS_LT) \
    X(CS_EQ) \
    X(CS_SCOPE_PUSH) \
    X(CS_SCOPE_POP) \
    X(CS_SET_VAR) \
    X(CS_GET_VAR) \
    X(CS_CONS) \
    X(CS_SETCAR) \
    X(CS_GETCAR) \
    X(CS_SETCDR) \
    X(CS_GETCDR) \
    X(CS_LOADI) \
    X(CS_LOADF) \
    X(CS_LOADS) \
    X(CS_LOADTRUE) \
    X(CS_LOADFALSE) \
    X(CS_LOADNIL) \
    X(CS_LOADSYM) \
    X(CS_LOADK) \
    X(CS_REF_RETAIN) \
    X(CS_REF_RELEASE) \


#define X(val) val,

enum cs_OpKind {
    CS_OPKIND_LIST
};
#undef X
#define X(val) #val,

const char* cs_OpKindStrings[] = {
    CS_OPKIND_LIST
};

#define ssavar(h, v) (cs_SSAVar) {.hash=h, .version=v}
#define ssa_new_temp() ssavar(tempvar_hash, c->cur_temp_id++)
#define ssa_eq(a, b) ((a.hash == b.hash) && (a.version == b.version))
struct cs_SSAVar {
    u32 hash;
    u32 version;
};

struct cs_SSAIns {
    cs_SSAVar dest;
    #define insarg(val) ((union ins_arg)val)
    union ins_arg {
        cs_SSAVar var;
        double double_;
        i64 int_;
        cs_Str* str_;
    } a_as, b_as;
    cs_OpKind op;
};

struct cs_SSAPhi {
    struct cs_SSAPhi* next;
    cs_SSAVar dest;
    cs_SSAVar* options;
    u8 option_count;
};

struct cs_BasicBlock {
    cs_SSAPhi phis_head;
    cs_SSAIns* instrs;
    u32 instr_cap; u32 instr_count;
    // links
    cs_BasicBlock** preds;
    u16 preds_cap; u16 preds_count;
    struct cs_BasicBlock* a;
    struct cs_BasicBlock* b;
    cs_SSAVar jump_cond; // hash == 0 if always a
    cs_Str* label;
};

struct cs_Code {
    // todo;
};