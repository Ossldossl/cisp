#include "cisp.h"
#include "console.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const u32 tempvar_hash = 0x1d91e0b5;
const cs_SSAVar ssavar_invalid = ssavar(0,0);

void cs_error(cs_Context* c, cs_Error error) {
    c->err = error;
    if (c == null) return;
    // calculate line and col
    u32 index = (u64)c->cur - (u64)c->start;
    u32 line = 1; u32 col = 1;
    char* cur = c->start;
    while (true) {
        if (*cur == 0) break;
        if (((u64)cur - (u64)c->start) == index) { break; }
        if (*cur == '\r') {
            cur += 2;
            line++;
            col = 1;
            continue;
        }
        if (*cur == '\n') {
            cur++;
            line++;
            col = 1;
            continue;
        }
        col++; cur++;
    }
    c->err_col = col; c->err_line = line;
    // TODO: print callstack
}

/* ==== POOL ALLOCATOR ==== */
cs_Pool cs_pool_init(u32 element_size)
{
    cs_Pool result;
    result.element_size = element_size;
    result.mem = malloc(CS_POOL_MEM_SIZE);
    pool_next(result.mem) = null;
    
    result.freelist = pool_data(result.mem);
    char* cur = (char*)result.freelist;
    char* end = (char*)result.mem + CS_POOL_MEM_SIZE;
    while ((u64)cur < (u64)end) {
        *(void**)cur = cur + element_size;
        cur += element_size;
    }
    void** last = (void**)(end - element_size);
    *last = 0;
    return result;
}

void* cs_pool_alloc(cs_Pool* p)
{
    if (p->freelist == null) {
        // TODO: allocate new bucket
        log_fatal("OUT OF MEMORY!");
        exit(-1);
    }
    void* result = p->freelist;
    p->freelist = freelist_next(p->freelist);
    return result;
}

// frees an element from the pool
void cs_pool_free(cs_Pool* p, void** ptr)
{
    if ((u64)ptr < (u64)p->freelist) {
        *ptr = p->freelist;
        p->freelist = ptr;
        return;
    }
    void** cur = p->freelist;
    void** last = null;
    while ((u64)cur < (u64)ptr) {
        last = cur;
        cur = freelist_next(cur);
    }
    *last = ptr;
    *ptr = cur;
    return;
}

// resets the pool
void cs_pool_clear(cs_Pool* p) {
    p->freelist = pool_data(p->mem);
    u64 cur = (u64)p->freelist;
    u64 end = (u64)cur + CS_POOL_MEM_SIZE;
    while (cur < end) {
        *(void**)cur = (void*) (cur + p->element_size);
        cur += p->element_size;
    }
    void** last = (void**)(end - p->element_size);
    *last = 0;
}

// frees the pool
void cs_pool_release(cs_Pool* p) {
    // TODO: support multiple buckets
    free(p->mem);
    // indicates that the pool was freed (or never initialized)
    p->element_size = 0; p->freelist = null;
}

/* ==== ARENA ==== */
cs_Arena arena_init(void) 
{
    cs_Arena result;
    result.buck_count = 1;
    result.buckets = malloc(sizeof(cs_ArenaBucket));
    result.buckets->last_alloc = 0;
    result.buckets->used = 0; 
    result.buckets->data = malloc(DEFAULT_ARENA_BUCKET_SIZE);
    if (result.buckets->data == null) {
        log_fatal("Not enough memory!");
        exit(-1);
    }
    return result;
}

void* cs_arena_alloc(cs_Arena* a, u32 size) 
{
    cs_ArenaBucket* b = &a->buckets[a->buck_count-1];
    if ((u64)b->used + (u64)size >= DEFAULT_ARENA_BUCKET_SIZE) { 
        log_fatal("Arena too smol :(");
        exit(-1);
    }
    b->used += size;
    b->last_alloc = size;
    return (void*)(b->data + b->used);
}

void arena_free_last(cs_Arena *a)
{
    cs_ArenaBucket* b = &a->buckets[a->buck_count-1];
    b->used -= b->last_alloc;
    b->last_alloc = 0;
}

void arena_clear(cs_Arena *a) 
{
    for (int i = 1; i < a->buck_count; i++) {
        cs_ArenaBucket* b = &a->buckets[i];
        free(b->data);
        free(b);
        a->buck_count--;
    }
    a->buckets->last_alloc = 0; 
    a->buckets->used = 0;
}

/* ==== STR ==== */
cs_Str* cs_str_init(u32 len) {   
    cs_Str* result = malloc(sizeof(cs_Str) + len);
    result->size = len;
    return result;
}

cs_Str* cs_make_str(char* data, u32 len)
{
    cs_Str* result = malloc(sizeof(cs_Str) + len + 1);
    result->size = len;
    memcpy_s(result->data, len+1, data, len);
    result->data[len] = 0;
    return result;
}

cs_StrBuilder cs_strbuilder_init(u32 cap)
{
    cs_StrBuilder result;
    result.cap = cap;
    result.len = 0;
    result.data = malloc(cap);
    memset(result.data, 0, cap);
    return result;
}

void cs_strbuilder_appendc(cs_StrBuilder* b, char c)
{
    b->len++;
    if (b->len > b->cap) {
        b->cap *= 2;
        b->data = realloc(b->data, b->cap);
    }
    b->data[b->len-1] = c;
}

void cs_strbuilder_appends(cs_StrBuilder* b, char* c, u32 len)
{
    if (len == 0) len = strlen(c);
    u8* cur = &b->data[len-1];
    b->len += len;
    while (b->len > b->cap) {
        b->cap *= 2;
    }
    b->data = realloc(b->data, b->cap);

    memcpy(cur, c, len);
}

cs_Str* cs_strbuilder_finish(cs_StrBuilder* b)
{
    cs_Str* result = cs_str_init(b->len+1);
    memcpy(cstr(result), b->data, b->len);
    result->data[b->len] = 0;
    result->size -= 1; // don't count the zero terminator
    free(b->data);
    return result;
}

/* ==== MAIN ==== */
#define advance() (c->cur++)
#define retreat() (c->cur--)
#define cur() (*c->cur)

cs_Context cs_init() 
{
    cs_Context result = {0};
    result.obj_pool = cs_pool_init(sizeof(cs_Object));
    return result;
}

cs_Object* cs_make_object(cs_Context* c) 
{
    cs_Object* result = cs_pool_alloc(&c->obj_pool);
    memset(result, 0, sizeof(cs_Object));
    return result;
}

u32 cs_ensure_cap(void** data, u32 element_size, u32* cur_cap, u32 wanted_cap)
{
    while (wanted_cap >= *cur_cap) {
        *cur_cap = (*cur_cap) * 1.75;
        *data = realloc(*data, element_size * (*cur_cap));
    }
    return *cur_cap;
}

cs_BasicBlock* cs_make_bb(cs_Context* c) 
{
    // TODO: use a better allocator for this
    cs_BasicBlock* result = malloc(sizeof(cs_BasicBlock));
    if (result == null) {
        log_fatal("OUT OF MEMORY!");
        exit(-1);
    }
    memset(result, 0, sizeof(cs_BasicBlock));
    result->instrs = malloc(sizeof(cs_SSAIns) * DEFAULT_BB_INS_START_CAP);
    if (result->instrs == null) {
        log_fatal("OUT OF MEMORY!");
        exit(-1);
    }
    result->instr_cap = DEFAULT_BB_INS_START_CAP; 
    result->preds = malloc(sizeof(cs_BasicBlock*) * DEFAULT_BB_PRED_CAP);
    if (result->preds == null) {
        log_fatal("OUT OF MEMORY!");
        exit(-1);
    }
    result->preds_cap = DEFAULT_BB_PRED_CAP;
    return result;
}

#define cs_emit(c, dest, op, a, b) _cs_emit((c), (dest), (op), insarg((a)), insarg((a)))
void _cs_emit(cs_Context* c, cs_SSAVar dest, cs_OpKind op, union ins_arg a, union ins_arg b)
{
    cs_BasicBlock* bb = c->cur_bb;
    bb->instr_count += 1;
    bb->instr_cap = cs_ensure_cap((void**)&bb->instrs, sizeof(cs_SSAIns), &bb->instr_cap, bb->instr_count);
    bb->instrs[bb->instr_count-1] = (cs_SSAIns) {
        .op = op,
        .dest = dest,
        .a_as = a,
        .b_as = b,
    };
}

void cs_obj_settype(cs_Object *obj, cs_ObjectType type)
{
    obj->car = (void*)(((u64)type << 56) | ((u64)obj->car & CS_OBJECT_INV_TYPE_MASK));
    if (type == CS_LIST) {
        obj->car = (void*)((u64)obj->car & CS_OBJECT_INV_LIST_MASK); // set last bit to 0
    } else {
        obj->car = (void*) ((u64)obj->car | 0b1); // set last bit to 1
    }
}

u16 cs_obj_gettype(cs_Object *obj)
{
    if (((u64)obj->car & 1) == 0) return CS_LIST;
    return ((u64)obj->car & CS_OBJECT_TYPE_MASK) >> CS_OBJECT_TYPE_OFFSET;
}

static inline bool is_whitespace(char c) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') return true;
    return false;
}

static void skip_whitespace(cs_Context* c)
{
    while (is_whitespace(cur())) {
        advance();
    }
}

// returns true if the given character would be valid to be in a symbol (except at the start)
static bool is_allowed_symbol_char(char c)
{
    if (c == '/' || c == '_' || c == '.' || c == '$' || c == '?') { return true; }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) { return true; }
    if (c >= '0' && c <= '9') return true;

    return false;
}

static bool match(cs_Context* c, char* string)
{
    u32 len = 0;
    while (true) {
        char ch = string[len];
        if (ch == 0) {
            if (is_allowed_symbol_char(cur())) {
                for (int i = 0; i < len; i++) {
                    retreat();
                }
                return false;
            } else return true;
        }
        if (cur() != ch) {
            for (int i = 0; i < len; i++) {
                retreat();
            }
            return false;
        }
        len++;
        advance();
    }
    return true;
}

static cs_SSAVar cs_parse_expr(cs_Context* c);
static cs_SSAVar parse_while(cs_Context* c);
static cs_SSAVar parse_function(cs_Context* c);

// (+|-)?[0-9]+ (\.[0-9]+)? (e(+|-)[0-9]+)?
static cs_SSAVar parse_number(cs_Context* c) 
{
    i64 int_val = 0;
    double float_val = 0;
    bool is_neg = false;
    bool is_float = false;
    if (cur() == '+') advance();
    if (cur() == '-') {
        is_neg = true;
        advance();
    }

    if (cur() < '0' || cur() > '9') {
        return ssavar_invalid;
    }

    do {
        int_val *= 10;
        int_val += cur() - '0'; 
        advance();
    } while (cur() >= '0' && cur() <= '9');

    if (cur() == '.') {
        advance();
        float_val = (double)int_val;
        is_float = true;
        if (cur() == 'e') {
            cs_error(c, CS_EXPONENT_AFTER_COMMA);
            return ssavar_invalid;
        }

        u64 decimals = 0; u32 count = 0;
        while (cur() >= '0' && cur() <= '9') {
           decimals *= 10;
           decimals += cur() - '0'; 
           count++;
           advance();
        }
        double frac = (double)decimals / pow(10, count);
        float_val += frac;
    }

    // (e(+|-)[0-9]+)?
     if (cur() == 'e') {
        advance();
        bool div;
        if (cur() == '+') advance();
        if (cur() == '-') { div = true; advance(); }
        u64 exponent = 0;
        while (cur() >= '0' && cur() <= '9') {
           exponent *= 10;
           exponent += cur() - '0'; 
           advance();
        }

        if (is_float) {
            if (div) {
                float_val /= pow(10, exponent);
            } else {
                float_val *= pow(10, exponent);
            }
        } else {
            if (div) {
                int_val /= pow(10, exponent);
            } else {
                int_val *= pow(10, exponent);
            }
        }
    }
    
    cs_SSAVar dest = ssa_new_temp();
    if (is_float) {
        if (is_neg) float_val *= -1;
        cs_emit(c, dest, CS_LOADF, float_val, 0);
    } else {
        if (is_neg) int_val *= -1;
        cs_emit(c, dest, CS_LOADI, int_val, 0);
    }
    return dest;
}

static cs_SSAVar parse_str_lit(cs_Context* c)
{
    // parse string literal
    char end_char = cur(); advance();
    cs_StrBuilder sb = cs_strbuilder_init(5);
    while (true) {
        if (cur() == 0) {
            cs_error(c, CS_UNEXPECTED_EOF);
            exit(-1);
        }
        if (cur() == end_char) break;
        if (cur() == '\\') {
            advance();
            char val = 0;
            switch (cur()) {
                case 'a':  { val = 0x07; } break;
                case 'b':  { val = 0x08; } break;
                case 'e':  { val = 0x1B; } break;
                case 'f':  { val = 0x0C; } break;
                case 'n':  { val = 0x0A; } break;
                case 'r':  { val = 0x0D; } break;
                case 't':  { val = 0x09; } break;
                case 'v':  { val = 0x0B; } break;
                case '\'': { val = 0x5C; } break;
                case '\"': { val = 0x27; } break;
                case '\?': { val = 0x3F; } break;
                default: {
                    cs_error(c, CS_INVALID_ESCAPE_CHAR);
                    exit(-1);
                } break;
            }
            cs_strbuilder_appendc(&sb, val);
            continue;
        }
        cs_strbuilder_appendc(&sb, cur());
        advance();
    }
    advance();
    cs_Str* str = cs_strbuilder_finish(&sb);
    cs_SSAVar dest = ssa_new_temp();
    cs_emit(c, dest, CS_LOADS, str, 0);
    return dest;
}

static cs_SSAVar parse_symbol(cs_Context* c) 
{
    char* start = c->cur;
    while (!is_whitespace(cur())) {
        advance();
    }
    char* end = c->cur;
    u64 hash = fnv1a(start, end);
    if (hash == tempvar_hash) {
        cs_error(c, CS_TEMP_RESERVED);
        return ssavar_invalid;
    }
    cs_SSAVar dest = ssa_new_temp();
    cs_emit(c, dest, CS_LOADSYM, reinterpret(hash, i64), 0); 
    return dest;
}

static cs_SSAVar parse_keyword(cs_Context* c)
{
    char* start = c->cur;
    while (!is_whitespace(cur())) {
        advance();
    }
    char* end = c->cur;
    u64 hash = fnv1a(start, end);
    if (hash == tempvar_hash) {
        cs_error(c, CS_TEMP_RESERVED);
        return ssavar_invalid;
    }
    cs_SSAVar dest = ssa_new_temp();
    cs_emit(c, dest, CS_LOADK, reinterpret(hash, i64), 0); 
    return dest;
}

// (true|false|nil|int|float|str|symbol|keyword)
static cs_SSAVar parse_atom(cs_Context* c)
{
    skip_whitespace(c);
    switch (cur()) {
        case '\0': {
            cs_error(c, CS_UNEXPECTED_EOF);
            return ssavar_invalid;
        }
        case '+':
        case '-': {
            char* cur_start = c->cur;
            cs_SSAVar res = parse_number(c);
            if (ssa_eq(res, ssavar_invalid)) { 
                c->cur = cur_start;
                return parse_symbol(c);
            }
            return res;
        } break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
            return parse_number(c);
        } break;
        case 'n': {
            advance();
            if (match(c, "il")) {
                cs_SSAVar dest = ssa_new_temp();
                cs_emit(c, dest, CS_LOADNIL, ssavar_invalid, ssavar_invalid);
                return dest;
            } else return parse_symbol(c);
        } break;
        case 't': {
            advance();
            if (match(c, "rue")) {
                cs_SSAVar dest = ssa_new_temp();
                cs_emit(c, dest, CS_LOADTRUE, ssavar_invalid, ssavar_invalid);
                return dest;
            } else return parse_symbol(c);
        } break;
        case 'f': {
            advance();
            if (match(c, "alse")) {
                cs_SSAVar dest = ssa_new_temp();
                cs_emit(c, dest, CS_LOADFALSE, ssavar_invalid, ssavar_invalid);
                return dest;
            } else if (match(c, "n")) {
                return parse_function(c);
            } else return parse_symbol(c);
        } break;
        case 'w': {
            advance();
            if (match(c, "hile")) {
                return parse_while(c);
            } else return parse_symbol(c);
        } break;
        case '"':
        case '\'': {
            return parse_str_lit(c);
        } break;
        case ':': {
            return parse_keyword(c);
        } break;
        default: {
            return parse_symbol(c);
        } break;
    }
}

cs_SSAVar cs_parse_expr(cs_Context* c)
{
    cs_SSAVar dest = ssavar_invalid;
    skip_whitespace(c);
    if (cur() == '(') {
        advance();
        // callable expr represented as a list
        cs_emit(c, ssavar_invalid, CS_SCOPE_PUSH, ssavar_invalid, ssavar_invalid);

        result->car = cs_parse_expr(c);
        cs_Object* cur = result;
        while (cur() != ')') {
            cs_Object* next_val = cs_parse_expr(c);
            if (next_val == null) return null;
            cs_Object* next = cs_make_object(c); // (cur) -> (next_val) -> (next_val) -> ..
            cs_obj_settype(next, CS_LIST);
            next->car = next_val;
            cur->cdr = next;
            cur = next;
            skip_whitespace(c);
        }
        advance();
    } else if (cur() == '\'') {
        // TODO: parse quote
        cs_error(c, CS_UNEXPECTED_CHAR);
    }
    else dest = parse_atom(c);
    return dest;
}

static cs_SSAVar parse_function(cs_Context* c)
{
    skip_whitespace(c);   
    if (cur() == '[') {
        // parse arguments
    }
}

static cs_SSAVar parse_while(cs_Context* c) 
{
    skip_whitespace(c);
    cs_Object* condition = cs_parse_expr(c);
    
}

// NOTE: RESETS LAST USED OBJECT POOL (if you reuse your context)
void cs_parse_cstr(cs_Context* c, char* content, u32 len)
{
    if (len == 0) len = strlen(content);
    c->start = content;
    c->cur = c->start; c->len = len;
    // pool hopefully initalized at this point
    cs_pool_clear(&c->obj_pool);

    cs_BasicBlock* entry = cs_make_bb(c);
    entry->label = make_str("entry");
    c->cur_bb = entry;
    c->functions[0] = entry;
    c->cur_temp_id = 0;
    
    cs_parse_expr(c);
    return;
}

void cs_serialize_object(cs_Object* o)
{
    if (o == null) return;
    cs_ObjectType type = cs_obj_gettype(o);
    switch (type) {
        case CS_ATOM_INT: printf("%lld ", (i64)o->cdr); break;
        case CS_ATOM_FLOAT: printf("%lf ", reinterpret(o->cdr, double)); break;
        case CS_ATOM_STR: printf("\"%s\" ", cstr((cs_Str*)o->cdr)); break;
        case CS_ATOM_TRUE: printf("true "); break;
        case CS_ATOM_FALSE: printf("false "); break;
        case CS_ATOM_NIL: printf("nil "); break;
        case CS_ATOM_KEYWORD: printf("keyword:#%u ", (u32)o->cdr); break;
        case CS_ATOM_SYMBOL: printf("symbol:#%u ", (u32)o->cdr); break;
        case CS_LIST: {
            printf("(");
            while(o) {
                cs_serialize_object(o->car);
                o = o->cdr;
            }
            printf(")");
        } break;
        default: printf("invalid!"); break;
    }
}

void cs_serialize_bb(cs_BasicBlock* bb) {
    printf("%s:\n", bb->label->data);
    for (u32 i = 0; i < bb->instr_count; i++) {
        cs_SSAIns* ins = &bb->instrs[i];
        printf("%d.%d = ", ins->dest.hash, ins->dest.version);
        printf("%s ", cs_OpKindStrings[ins->op]);
        switch (ins->op) {
        case CS_REF_RETAIN:
        case CS_REF_RELEASE:
        case CS_GETCAR:
        case CS_GETCDR:
        case CS_NOT: 
        {
            printf("%s\n", ins->a_as.int_ == 0 ? "false" : "true");
        } break;
        
        case CS_ADD:
        case CS_SUB:
        case CS_MUL:
        case CS_DIV:
        case CS_LSHIFT:
        case CS_RSHIFT:
        case CS_GT:
        case CS_LT:
        case CS_EQ:
        {
            printf("%lld %lld\n", ins->a_as.int_, ins->b_as.int_);
        } break;

        case CS_AND:
        case CS_OR:
        {
            printf("%s %s\n", ins->b_as.int_ == 0 ? "false" : "true", ins->a_as.int_ == 0 ? "false" : "true");
        } break;
        
        case CS_LOADI:
        {
            printf("#%d.%d %lld\n", ins->a_as.var.hash, ins->a_as.var.version, ins->b_as.int_);
        } break;
        case CS_LOADF:
        {
            printf("#%d.%d %lf\n", ins->a_as.var.hash, ins->a_as.var.version, ins->b_as.double_);
        } break;

        case CS_LOADS:
        {
            printf("#%d.%d %s\n", ins->a_as.var.hash, ins->a_as.var.version, ins->b_as.str_->data);
        } break;

        case CS_LOADSYM:
        case CS_LOADK:
        {
            printf("#%d.%d %llu\n", ins->a_as.var.hash, ins->a_as.var.version, ins->b_as.int_);
        } break;

        case CS_LOADFALSE:
        {
            printf("true\n");
        } break;
        case CS_LOADTRUE:
        {
            printf("false\n");
        } break;

        case CS_LOADNIL:
        {
            printf("nil\n");
        } break;

        case CS_SETCAR:
        case CS_SETCDR:
        case CS_CONS:
        {
            printf("#%d.%d #%d.%d\n", ins->a_as.var.hash, ins->a_as.var.version, ins->b_as.var.hash, ins->b_as.var.version);
        } break;

        case CS_SET_VAR:
        {
            printf("%lld %lld\n", ins->a_as.int_, ins->b_as.int_);
        } break;

        case CS_GET_VAR:
        {
            printf("%lld\n", ins->a_as.int_);
        } break;

        case CS_SCOPE_PUSH:
        case CS_SCOPE_POP:
        default:
          break;
        }
    }
    u8* a_label = bb->a->label->size > 0 ? bb->a->label->data : null;
    u8* b_label = bb->b->label->size > 0 ? bb->b->label->data : null;
    printf("BR %d.%d %s %s", bb->jump_cond.hash, bb->jump_cond.version, a_label, b_label);
}

cs_Object* cs_eval(cs_Context* c, cs_Code* code)
{
    // TODO: evaluation
    if (code == null) printf("<empty>\n");
    cs_serialize_object(code);
    return code;
}

cs_Code* cs_compile_file(cs_Context* c, char* content, u32 len)
{
    cs_parse_cstr(c, content, len);
    if (c->err != CS_OK) {
        log_debug("had error, no serialization!");
        return null;
    }
    // todo
    cs_BasicBlock* bb = c->functions[0];
    cs_serialize_bb(bb);
    return null;
}

char err_buf[512];
char* cs_get_error_string(cs_Context* c)
{
    char* msg = null;
    switch (c->err) {
        case CS_OK                  : { msg = "Success"; break; }
        case CS_EXPONENT_AFTER_COMMA: { msg = "Invalid exponent after comma. Remove the comma or provide decimal places before the comma at %d:%d"; break; }
        case CS_INVALID_ESCAPE_CHAR : { msg = "Invalid Escape character at %d:%d"; break; }
        case CS_UNEXPECTED_EOF      : { msg = "Unexpected End of Input at %d:%d"; break; }
        case CS_UNEXPECTED_CHAR     : { msg = "Unexpected character at %d:%d"; break; }
        case CS_TEMP_RESERVED       : { msg = "Symbol '__temp' is reserved by the compiler. (%d:%d)"; break; }
        case CS_OUT_OF_MEM          : { msg = "Out of memory"; break; }
        default                     : { msg = "!Invalid Error! at %d:%d"; break; }
    }
    snprintf(err_buf, 512, msg, c->err_line, c->err_col);
    return err_buf;
}