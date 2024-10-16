#pragma once

#define DEFAULT_ARENA_BUCKET_SIZE 4096
#define DEFAULT_BB_INS_START_CAP 5
#define DEFAULT_BB_PRED_CAP 1
#define FUNCTION_MAX_ARGS 32

#include "common.h"
#include "map.h"
#include "btree.h"

#define check_ssavar(var) if (ssa_eq(var, ssavar_invalid)) return ssavar_invalid;

/* ==== MAIN ==== */
typedef struct cs_BasicBlock cs_BasicBlock;
typedef struct cs_BasicBlockNode cs_BasicBlockNode;
typedef struct cs_Context cs_Context;
typedef struct cs_Object cs_Object;
typedef struct cs_Function cs_Function;
typedef struct cs_FunctionBody cs_FunctionBody;
typedef struct cs_Scope cs_Scope;
typedef struct cs_ComScope cs_ComScope;
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
    CS_MISSING_PAREN,
    CS_TEMP_RESERVED,
    CS_ENTRY_RESERVED,
    CS_FN_NOT_ALLOWED_HERE,
    CS_WHILE_NOT_ALLOWED_HERE,
    CS_VAL_NOT_CALLABLE,
    CS_SYMBOL_NOT_FOUND,
    CS_INVALID_NUMBER_OF_ARGUMENTS,
    CS_TOO_MANY_ARGUMENTS,

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
    CS_FUNC,    //cdr ^= cs_Function*
    CS_CFUNC,   // cdr ^= cs_Function*

    // used only for bytecode
    CS_REG,     // cdr ^= reg index
    CS_BLOCK,   // cdr ^= basic block pointer

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
    //          func: fn_id
    //          cfunc: fn_ptr
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

    cs_Arena functions;  // TODO: maybe another datastructure?
    u32 cur_fn_id;

    cs_Arena bbs;
    u32 cur_bb_id;

    cs_Arena comscopes;
    cs_ComScope* cur_scope;
    cs_BasicBlock* cur_bb;

    u64 cur_temp_id;

    // vm 
    cs_Object regs[16]; 
};

cs_Context cs_init();
cs_Object* cs_run(cs_Context* c, cs_Code* code);
cs_Code* cs_compile_file(cs_Context* c, char* content, u32 len);
char* cs_get_error_string(cs_Context* c);
void cs_cfunc(cs_Context* c, void* fn, i8 arg_count);
cs_Object* cs_make_object(cs_Context* c);
void cs_obj_settype(cs_Object* obj, cs_ObjectType type);
u16 cs_obj_gettype(cs_Object *obj);

/* ==== VM ==== */

#define CS_OPKIND_LIST \
    /* binops i=int int; v = var var; vi = var int; ..*/ \
    X(CS_ADDI) \
    X(CS_ADDV) \
    X(CS_ADDVI) \
    X(CS_ADDF) \
    X(CS_ADDVF) \
    X(CS_ADDS) \
    X(CS_SUBI) \
    X(CS_SUBV) \
    X(CS_SUBVI) \
    X(CS_SUBF) \
    X(CS_SUBVF) \
    X(CS_MULI) \
    X(CS_MULV) \
    X(CS_MULVI) \
    X(CS_MULF) \
    X(CS_MULVF) \
    X(CS_DIVI) \
    X(CS_DIVV) \
    X(CS_DIVVI) \
    X(CS_DIVF) \
    X(CS_DIVVF) \
    X(CS_MODI) \
    X(CS_MODV) \
    X(CS_MODVI) \
    X(CS_ANDI) \
    X(CS_ANDV) \
    X(CS_ANDVI) \
    X(CS_ORI) \
    X(CS_ORV) \
    X(CS_ORVI) \
    X(CS_LSHIFTI) \
    X(CS_LSHIFTV) \
    X(CS_LSHIFTVI) \
    X(CS_RSHIFTI) \
    X(CS_RSHIFTV) \
    X(CS_RSHIFTVI) \
    X(CS_GTI) \
    X(CS_GTV) \
    X(CS_GTVI) \
    X(CS_GTF) \
    X(CS_GTVF) \
    X(CS_LTI) \
    X(CS_LTV) \
    X(CS_LTVI) \
    X(CS_LTF) \
    X(CS_LTVF) \
    X(CS_GEQI) \
    X(CS_GEQV) \
    X(CS_GEQVI) \
    X(CS_GEQF) \
    X(CS_GEQVF) \
    X(CS_LEQI) \
    X(CS_LEQV) \
    X(CS_LEQVI) \
    X(CS_LEQF) \
    X(CS_LEQVF) \
    X(CS_EQI) \
    X(CS_EQV) \
    X(CS_EQVI) \
    X(CS_EQF) \
    X(CS_EQVF) \
    \
    X(CS_NOT) \
    X(CS_CALL) \
    X(CS_SCOPE_PUSH) \
    X(CS_SCOPE_POP) \
    X(CS_SET_LOCAL) \
    X(CS_GET_LOCAL) \
    X(CS_CONS) \
    X(CS_SETCAR) \
    X(CS_GETCAR) \
    X(CS_SETCDR) \
    X(CS_GETCDR) \
    X(CS_LOADI) \
    X(CS_LOADF) \
    X(CS_LOADFUN) \
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

#ifdef CISP_STRINGS_IMPLEMENTATION
const char* cs_OpKindStrings[] = {
    CS_OPKIND_LIST
};
#endif

#define ssavar(h, v) (cs_SSAVar) {.hash=h, .version=v}
#define ssa_new_temp() ssavar(tempvar_hash, c->cur_temp_id++)
#define ssa_eq(a, b) ((a.hash == b.hash) && (a.version == b.version))
#define ssa_invalid(s) (ssa_eq(s, ssavar_invalid))
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
        cs_BasicBlock* bb_;
    } a_as, b_as;
    cs_OpKind op;
};

struct cs_FunctionBody {
    u32* args;
    i8 arg_count;   // negative for native functions
    u8 calls;       // for eventual inlining and dce
    cs_BasicBlock* return_bb;
    union {
        void* native_func;
        cs_BasicBlock* entry; // the first phis are always the arguments
    };
    cs_SSAVar return_val;
};

struct cs_Function {
    cs_Str* title; // format: name_of_function" "arity
    cs_FunctionBody* variants;
    u8 variant_count;
};

/*struct cs_Scope {
    cs_HMap locals; // map_of (cs_Object)
};*/

struct cs_ComScope {
    cs_HMap locals; // map_of (cs_SSAVar)
    cs_HMap functions; // map_of (u32 => (fn_id)) 
};

struct cs_SSAPhi {
    struct cs_SSAPhi* next;
    cs_SSAVar dest;
    cs_SSAVar* options;
    u8 option_count;
};

struct cs_BasicBlockNode {
    cs_BasicBlock* head;
    cs_BasicBlockNode* tail;
};

struct cs_BasicBlock {
    cs_SSAPhi phis_head;
    cs_SSAIns* instrs;
    u32 instr_cap; u32 instr_count;
    // links
    cs_BasicBlockNode* preds_start;
    bool visited; 
    struct cs_BasicBlock* return_address;
    struct cs_BasicBlock* a;
    struct cs_BasicBlock* b;
    cs_SSAVar jump_cond; // hash == 0 if always a
    cs_Str* label;
};

struct cs_Code {
    // todo;
};