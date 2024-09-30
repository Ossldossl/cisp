#define CISP_STRINGS_IMPLEMENTATION
#include "cisp.h"
#include "console.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

const u32 tempvar_hash = 0x1d91e0b5;
const u32 entrysym_hash = 0xa8dafb7d;
const u32 fnsym_hash = 0x6322e9d5;
const u32 whilesym_hash = 0x0dc628ce;
const u32 returnbb_hash = 0xb3e6dde1;
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
    void* result = advance_ptr(b->data, b->used);
    b->used += size;
    b->last_alloc = size;
    return result;
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

cs_Function* cs_get_fn(cs_Context* c, u32 id)
{
    const u32 fns_per_bucket = (u32)DEFAULT_ARENA_BUCKET_SIZE / sizeof(cs_Function);
    u32 bucket_num = id / fns_per_bucket;
    if (bucket_num > c->functions.buck_count) {
        log_fatal("Invalid function id: %u (bucket nr: %u)", id, bucket_num);
        exit(-1);
    }
    cs_ArenaBucket* buck = &c->functions.buckets[bucket_num];
    u32 fn_num = id - fns_per_bucket * bucket_num;
    cs_Function* fn = (cs_Function*)advance_ptr(buck->data, fn_num * sizeof(cs_Function));
    return fn;
}

cs_FunctionBody* cs_fn_get_variant(cs_Function* fn, u8 arg_count)
{
    cs_FunctionBody* cur = fn->variants;
    for (int i = 0; i < fn->variant_count; i++) {
        if (cur->arg_count == arg_count) return cur;
        cur = &cur[i];
    }
    return null;
}

cs_FunctionBody* cs_fn_add_variant(cs_Context* c, cs_Function* fn)
{
    fn->variant_count += 1;
    fn->variants = realloc(fn->variants, fn->variant_count * sizeof(cs_FunctionBody));
    return &fn->variants[fn->variant_count-1];
}

cs_Function* cs_make_fn(cs_Context* c, u32* fn_id)
{
    cs_Function* result = cs_arena_alloc(&c->functions, sizeof(cs_Function));
    if (fn_id) {
        *fn_id = c->cur_fn_id;
    }
    c->cur_fn_id += 1;
    result->variant_count = 0; result->variants = null;
    return result;
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
    result->phis_head.dest = ssavar_invalid;
    c->cur_bb_id += 1;
    return result;
}

void cs_bb_add_pred(cs_BasicBlock* bb, cs_BasicBlock* pred)
{
    if (bb->preds_start == null) {
        bb->preds_start = malloc(sizeof(cs_BasicBlockNode));
        bb->preds_start->head = pred; bb->preds_start->tail = null;
        return;
    }

    cs_BasicBlockNode* cur = bb->preds_start;
    while (true) {
        if (cur->tail == null) {
            cur->tail = malloc(sizeof(cs_BasicBlockNode));
            cur->tail->head = pred; cur->tail = null;
            break;
        } else {
            cur = cur->tail;
        }
    }
}

cs_Scope* cs_scope_push(cs_Context* c)
{
    cs_Scope* result = malloc(sizeof(cs_Scope));
    result->locals = cs_hm_init(sizeof(cs_Object));
    result->functions = cs_hm_init(sizeof(u32));
    result->up = c->cur_scope;
    c->cur_scope = result;
    return result;
}

void cs_scope_pop(cs_Context* c)
{
    cs_Scope* prev = c->cur_scope->up;
    cs_hm_free(&c->cur_scope->locals);
    cs_hm_free(&c->cur_scope->functions);
    free(c->cur_scope);
    c->cur_scope = prev;
}

cs_Object* cs_scope_lookup(cs_Context* c, u32 hash)
{
    cs_Scope* cur = c->cur_scope;
    while (cur) {
        cs_Object* result = cs_hm_geth(&cur->locals, hash);
        if (result != null) return result;
        cur = cur->up;
    }
    return null;
}

u32* cs_scope_set_fn(cs_Scope* s, u32 hash)
{
    if (s == null) return null;
    return cs_hm_seth(&s->functions, hash);
}

cs_Object* cs_scope_set(cs_Scope* s, u32 hash)
{
    if (s == null) return null;
    return cs_hm_seth(&s->locals, hash);
}

// looks for static functions in scope
u32* cs_scope_lookup_fn(cs_Context* c, u32 hash)
{
    cs_Scope* cur = c->cur_scope;
    while (cur) {
        u32* result = cs_hm_geth(&cur->functions, hash);
        if (result != null) return result;
        cur = cur->up;
    }
    return null;
}

cs_Context cs_init() 
{
    cs_Context result = {0};
    result.functions = arena_init();
    result.obj_pool = cs_pool_init(sizeof(cs_Object));
    cs_scope_push(&result);
    return result;
}

void cs_cfunc(cs_Context* c, void* fn, i8 arg_count)
{
    // TODO: cfunc
}

void cs_set_local(cs_Context* c, cs_Str* name, cs_Object value)
{
    cs_Object* val = cs_hm_sets(&c->cur_scope->locals, name);
    *val = value;
}

#define cs_emit(c, dest, op, a, b) _cs_emit((c), (dest), (op), insarg((a)), insarg((b)))
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

// returns true if the given character would *not* be valid to be in a symbol (expect at the start)
static bool is_disallowed_symbol_char(char c)
{
    if (is_whitespace(c)) return true;
    switch (c) {
        case '[':
        case ']':
        case '(':
        case ')':
        case '{':
        case '}':
        case '\\':
        case ':':
            return true;
        
        default: return false;
    }
}

static bool match(cs_Context* c, char* string)
{
    u32 len = 0;
    while (true) {
        char ch = string[len];
        if (ch == 0) {
            if (!is_disallowed_symbol_char(cur())) {
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
static cs_SSAVar gen_while(cs_Context* c);
static cs_SSAVar gen_function(cs_Context* c);

// (+|-)?[0-9]+ (\.[0-9]+)? (e(+|-)[0-9]+)?
static cs_SSAVar gen_number(cs_Context* c) 
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
        cs_emit(c, dest, CS_LOADF, float_val, 0ll);
    } else {
        if (is_neg) int_val *= -1;
        cs_emit(c, dest, CS_LOADI, int_val, 0ll);
    }
    return dest;
}

static cs_SSAVar gen_str_lit(cs_Context* c)
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
    cs_emit(c, dest, CS_LOADS, str, 0ll);
    return dest;
}

static u32 parse_symbol(cs_Context* c)
{
    char* start = c->cur;
    while (!is_disallowed_symbol_char(cur())) {
        advance();
    }
    char* end = c->cur;
    u64 hash = fnv1a(start, end);
    if (hash == tempvar_hash) {
        cs_error(c, CS_TEMP_RESERVED);
        return 0;
    } else if (hash == entrysym_hash) {
        cs_error(c, CS_ENTRY_RESERVED);
        return 0;
    }
    return hash;
}

static cs_SSAVar gen_symbol(cs_Context* c) 
{
    u32 hash = parse_symbol(c);
    cs_SSAVar dest = ssa_new_temp();
    cs_emit(c, dest, CS_LOADSYM, reinterpret(hash, i64), 0ll); 
    return dest;
}

static cs_SSAVar gen_keyword(cs_Context* c)
{
    u32 hash = parse_symbol(c);
    cs_SSAVar dest = ssa_new_temp();
    cs_emit(c, dest, CS_LOADK, reinterpret(hash, i64), 0ll); 
    return dest;
}

// (true|false|nil|int|float|str|symbol|keyword)
static cs_SSAVar gen_atom(cs_Context* c)
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
            cs_SSAVar res = gen_number(c);
            if (ssa_eq(res, ssavar_invalid)) { 
                c->cur = cur_start;
                return gen_symbol(c);
            }
            return res;
        } break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
            return gen_number(c);
        } break;
        case 'n': {
            if (match(c, "nil")) {
                cs_SSAVar dest = ssa_new_temp();
                cs_emit(c, dest, CS_LOADNIL, ssavar_invalid, ssavar_invalid);
                return dest;
            } else return gen_symbol(c);
        } break;
        case 't': {
            if (match(c, "true")) {
                cs_SSAVar dest = ssa_new_temp();
                cs_emit(c, dest, CS_LOADTRUE, ssavar_invalid, ssavar_invalid);
                return dest;
            } else return gen_symbol(c);
        } break;
        case 'f': {
            if (match(c, "false")) {
                cs_SSAVar dest = ssa_new_temp();
                cs_emit(c, dest, CS_LOADFALSE, ssavar_invalid, ssavar_invalid);
                return dest;
            } else return gen_symbol(c);
        } break;
        case '"':
        case '\'': {
            return gen_str_lit(c);
        } break;
        case ':': {
            return gen_keyword(c);
        } break;
        default: {
            return gen_symbol(c);
        } break;
    }
}

static u32 parse_function(cs_Context* c, u32 hash)
{
    skip_whitespace(c);   
    
    u32 fn_id = 0;
    cs_Function* fn = cs_make_fn(c, &fn_id);
    cs_FunctionBody* fb = cs_fn_add_variant(c, fn);
    fb->arg_count = 0;
    fb->args = null;
    u32 arg_buf[INT8_MAX] = {0};
    fb->calls = 0;

    // parse arguments
    if (cur() == '[') {
        advance();
        while (cur() != ']') {
            arg_buf[fb->arg_count] = parse_symbol(c);
            fb->arg_count++;
            skip_whitespace(c);
        }
        advance();
        fb->args = malloc(sizeof(u32) * fb->arg_count);
        memcpy_s(fb->args, sizeof(u32) * fb->arg_count, arg_buf, sizeof(u32) * fb->arg_count);
    }

    fb->entry = cs_make_bb(c);
    cs_BasicBlock* entry = fb->entry;
    char buf[50];
    u32 len = snprintf(buf, 20, "%d.entry", fn_id);
    entry->label = cs_make_str(buf, len);
    
    // construct phis for arguments
    cs_SSAPhi* cur_phi = &entry->phis_head;
    for (int i = 0; i < fb->arg_count; i++) {
        cur_phi->option_count = 0; cur_phi->options = null;
        cur_phi->dest = ssavar(fb->args[i], 0ll);
        cur_phi->next = malloc(sizeof(cs_SSAPhi));
        cur_phi = cur_phi->next;
    }
    // mark end of phi-chain
    cur_phi->dest = ssavar_invalid; 
    cs_BasicBlock* last_bb = c->cur_bb;
    c->cur_bb = entry;

    if (hash != 0) {
        // connect name to function id
        u32* slot = cs_scope_set_fn(c->cur_scope, hash);
        if (slot == null) return 0;
        *slot = fn_id;
    }

    // parse & generate body
    while (cur() != ')') {
        cs_parse_expr(c);
    }
    advance();
    
    // restore previously current bb
    c->cur_bb = last_bb; 
    
    return fn_id;
}

static cs_SSAVar gen_function(cs_Context* c)
{
    u32 fn_id = parse_function(c, 0ll);
    cs_SSAVar dest = ssa_new_temp();
    cs_emit(c, dest, CS_LOADFUN, (i64)fn_id, 0ll);
    return dest;
}

static cs_SSAVar gen_while(cs_Context* c) 
{
    skip_whitespace(c);
    cs_BasicBlock* bb_check_cond = cs_make_bb(c);
    c->cur_bb->jump_cond = ssavar_invalid; // signal unconditional jump
    c->cur_bb->a = bb_check_cond;
    bb_check_cond->phis_head.dest = ssavar_invalid;
    char buf[256];
    u32 len = snprintf(buf, 256, "%s.while_cond.%d", c->cur_bb->label->data, c->cur_bb_id); 
    bb_check_cond->label = cs_make_str(buf, len);
    
    cs_BasicBlock* initial_bb = c->cur_bb;
    c->cur_bb = bb_check_cond;
    cs_SSAVar cond = cs_parse_expr(c);
    check_ssavar(cond);
    c->cur_bb = initial_bb;
    bb_check_cond->jump_cond = cond;

    cs_BasicBlock* while_body = cs_make_bb(c);
    len = snprintf(buf, 256, "%s.while_body.%d", c->cur_bb->label->data, c->cur_bb_id);
    while_body->label = cs_make_str(buf, len);
    cs_BasicBlock* bb_end = cs_make_bb(c);
    len = snprintf(buf, 256, "%s.while_end.%d", c->cur_bb->label->data, c->cur_bb_id);
    bb_end->label = cs_make_str(buf, len);

    bb_check_cond->a = while_body;
    bb_check_cond->b = bb_end;
    c->cur_bb = while_body;
    while (cur() != ')') {
        cs_parse_expr(c);
    }
    advance();
    while_body->a = bb_check_cond;
    while_body->jump_cond = ssavar_invalid;

    c->cur_bb = bb_end;
    cs_SSAVar dest = ssa_new_temp();
    cs_emit(c, dest, CS_LOADNIL, ssavar_invalid, ssavar_invalid);
    return dest;
}

static cs_SSAVar gen_if(cs_Context* c)
{
    todo(if_clause);
    return ssavar_invalid;
}

cs_SSAVar cs_parse_expr(cs_Context* c)
{
    cs_SSAVar dest = ssavar_invalid;
    skip_whitespace(c);
    if (cur() == 0) return ssavar_invalid;
    
    if (cur() == '(') {
        advance();
        skip_whitespace(c);

        if (match(c, "fn")) {
            return gen_function(c);
        } else if (match(c, "defn")) {
            // parse name of function
            skip_whitespace(c);
            u32 hash = parse_symbol(c);
            u32 fn_id = parse_function(c, hash);
            // since every instruction has to generate a value, we "return" the id of the function
            cs_SSAVar dest = ssa_new_temp();
            cs_emit(c, dest, CS_LOADFUN, (i64)fn_id, 0ll);
            return dest;
        } else if (match(c, "while")) {
            return gen_while(c);
        } else if (match(c, "if")) {
            return gen_if(c);
        }
        
        cs_SSAVar fn_name = cs_parse_expr(c);
        check_ssavar(fn_name);

        cs_SSAIns last_instr = c->cur_bb->instrs[c->cur_bb->instr_count-1];
        // function to call
        cs_Function* fn = null;
        if (last_instr.op == CS_LOADFUN) {
            // function was created just for calling its
            c->cur_bb->instr_count--; // remove last instruction, since we statically add the call
            fn = cs_get_fn(c, last_instr.a_as.int_);
        } else if (last_instr.op == CS_LOADSYM) {
            u32 hash = reinterpret(last_instr.a_as.int_, u32);
            // function is provided as a symbol
            u32* fn_id = cs_scope_lookup_fn(c, hash);
            if (fn_id == null) {
                // check if symbol points to a variable holding the function ptr
                cs_Object* var = cs_scope_lookup(c, hash);
                if (var == null) {
                    cs_error(c, CS_SYMBOL_NOT_FOUND);
                    return ssavar_invalid;
                }
                // if a variable was found, emit dynamic dispatch of the function
                // TODO:
                todo(dynamic_dispatch)
                return ssavar_invalid;
            }
            fn = cs_get_fn(c, *fn_id);
        } else {
            cs_error(c, CS_VAL_NOT_CALLABLE);
            return ssavar_invalid;
        }

        // evaluate arguments
        u8 arg_count = 0;
        cs_SSAVar args[FUNCTION_MAX_ARGS] = {0};
        while (cur() != ')') {
            cs_SSAVar arg = cs_parse_expr(c);
            check_ssavar(arg);
            args[arg_count] = arg;
            arg_count += 1;
            if (arg_count == 32) {
                cs_error(c, CS_TOO_MANY_ARGUMENTS);
                return ssavar_invalid;
            }
            skip_whitespace(c);
        }
        advance();

        // find corresponding variant based on the number of arguments
        cs_FunctionBody* fn_variant = cs_fn_get_variant(fn, arg_count);
        if (fn_variant == null) {
            cs_error(c, CS_INVALID_NUMBER_OF_ARGUMENTS);
            return ssavar_invalid;
        }
        fn_variant->calls += 1;
        // statically dispatch the function
        cs_BasicBlock* cur_bb = c->cur_bb;
        cur_bb->jump_cond = ssavar_invalid; // indicate unconditional jump
        cur_bb->a = fn_variant->entry;
        cs_bb_add_pred(fn_variant->entry, cur_bb);
        cs_BasicBlock* fn_entry = fn_variant->entry;
        cs_SSAPhi* cur = &fn_entry->phis_head;
        // update phis
        for (int i = 0; i < arg_count; i++) {
            cur->option_count++;
            cur->options = realloc(cur->options, cur->option_count * sizeof(cs_SSAVar));
            cur->options[cur->option_count-1] = args[i];
            cur = cur->next;
        }
        // push return address
        cs_BasicBlock* return_bb = cs_make_bb(c);
        cs_SSAVar return_address_dest = ssa_new_temp();
        cs_emit(c, return_address_dest, CS_SET_LOCAL, (i64)returnbb_hash, return_bb);
        char buf[50];
        u32 len = snprintf(buf, 20, "%s.return_to.#%d", cur_bb->label->data, c->cur_bb_id);
        return_bb->label = cs_make_str(buf, len);

        c->cur_bb = return_bb;
        // return value phi
        cs_SSAVar dest = ssa_new_temp();
        return_bb->phis_head.dest = dest;
        return_bb->phis_head.next = null;
        return_bb->phis_head.option_count = 1;
        // TODO: functions need a footer bb where they collect their return values into a phi node
        return_bb->phis_head.options = malloc(sizeof(cs_SSAVar));
        return_bb->phis_head.options[0] = fn_variant->return_val;
        return dest;
    } else if (cur() == '\'') {
        // TODO: parse quote
        cs_error(c, CS_UNEXPECTED_CHAR);
    }
    else dest = gen_atom(c);
    return dest;
}

// NOTE: RESETS LAST USED OBJECT POOL (if you reuse your context)
void cs_parse_cstr(cs_Context* c, char* content, u32 len)
{
    if (len == 0) len = strlen(content);
    c->start = content;
    c->cur = c->start; c->len = len;
    c->cur_temp_id = 0;
    // pool hopefully initalized at this point
    cs_pool_clear(&c->obj_pool);

    cs_BasicBlock* entry = cs_make_bb(c);
    entry->label = make_str("entry");
    c->cur_bb = entry;
    // NOTE: entry_fn is expected to be the first function in the arena
    cs_Function* entry_fn = cs_make_fn(c, null);
    entry_fn->title = entry->label;
    
    cs_FunctionBody* fb = cs_fn_add_variant(c, entry_fn);
    fb->arg_count = 0; fb->args = null; 
    fb->entry = entry; 
    fb->calls = 1; fb->return_val = ssavar_invalid;

    cs_SSAVar result;
    do {
        result = cs_parse_expr(c);
    } while (!ssa_eq(result, ssavar_invalid));
    
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
        case CS_ATOM_KEYWORD: printf("keyword:#%llu ", (u64)o->cdr); break;
        case CS_ATOM_SYMBOL: printf("symbol:#%llu ", (u64)o->cdr); break;
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
    if (bb->visited == true) return;
    bb->visited = true;
    printf("\n\n%s:\n", bb->label->data);
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
            printf("%lld\n", ins->a_as.int_);
        } break;
        case CS_LOADF:
        {
            printf("%lf\n", ins->a_as.double_);
        } break;

        case CS_LOADS:
        {
            printf("%s\n", ins->a_as.str_->data);
        } break;

        case CS_LOADSYM:
        case CS_LOADK:
        case CS_LOADFUN:
        {
            printf("%u\n", reinterpret(ins->a_as.int_, u32));
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

        case CS_SET_LOCAL:
        {
            printf("%u %llu\n", reinterpret(ins->a_as.int_, u32), (u64)ins->b_as.bb_);
        } break;

        case CS_GET_LOCAL:
        {
            printf("%u\n", reinterpret(ins->a_as.int_, u32));
        } break;

        case CS_SCOPE_PUSH:
        case CS_SCOPE_POP:
        default:
          break;
        }
    }
    u8* a_label = bb->a != null ? bb->a->label->data : null;
    u8* b_label = bb->b != null ? bb->b->label->data : null;
    if (ssa_eq(bb->jump_cond, ssavar_invalid)) {
        printf("BR true %s", a_label);
    } else {
        printf("BR %u.%u %s %s", bb->jump_cond.hash, bb->jump_cond.version, a_label, b_label);
    }
    if (bb->a) cs_serialize_bb(bb->a);
    if (bb->b) cs_serialize_bb(bb->b);
}

cs_Object* cs_eval(cs_Context* c, cs_Code* code)
{
    // TODO: evaluation
    if (code == null) printf("<empty>\n");
    todo(eval);
    return null;
}

cs_Code* cs_compile_file(cs_Context* c, char* content, u32 len)
{
    cs_parse_cstr(c, content, len);
    if (c->err != CS_OK) {
        log_debug("had error, no serialization!");
        return null;
    }
    // todo
    cs_Function* fn = cs_get_fn(c, 0ll);
    for (int i = 0; i < fn->variant_count; i++) {
        cs_FunctionBody* fb = &fn->variants[i];
        cs_serialize_bb(fb->entry);
    }
    return null;
}

char err_buf[512];
char* cs_get_error_string(cs_Context* c)
{
    char* msg = null;
    switch (c->err) {
        case CS_OK                         : { msg = "Success"; break; }
        case CS_EXPONENT_AFTER_COMMA       : { msg = "Invalid exponent after comma. Remove the comma or provide decimal places before the comma at %d:%d"; break; }
        case CS_INVALID_ESCAPE_CHAR        : { msg = "Invalid Escape character at %d:%d"; break; }
        case CS_UNEXPECTED_EOF             : { msg = "Unexpected End of Input at %d:%d"; break; }
        case CS_UNEXPECTED_CHAR            : { msg = "Unexpected character at %d:%d"; break; }
        case CS_TEMP_RESERVED              : { msg = "Symbol '__temp' is reserved by the compiler. (%d:%d)"; break; }
        case CS_ENTRY_RESERVED             : { msg = "Symbol '__entry' is reserved by the compiler. (%d:%d)"; break; }
        case CS_OUT_OF_MEM                 : { msg = "Out of memory at %d:%d"; break; }
        case CS_FN_NOT_ALLOWED_HERE        : { msg = "Function definition is not allowed here at %d:%d"; break; }
        case CS_WHILE_NOT_ALLOWED_HERE     : { msg = "While-loop is not allowed here at %d:%d"; break; }
        case CS_VAL_NOT_CALLABLE           : { msg = "Value is not callable at %d:%d"; break; }
        case CS_SYMBOL_NOT_FOUND           : { msg = "Symbol could not be found at %d:%d"; break; }
        case CS_INVALID_NUMBER_OF_ARGUMENTS: { msg = "Invalid number of arguments at %d:%d"; break; }
        case CS_TOO_MANY_ARGUMENTS         : { msg = "Too many arguments for function at %d:%d"; break; }
        default                            : { msg = "!Invalid Error! at %d:%d"; break; }
    }
    snprintf(err_buf, 512, msg, c->err_line, c->err_col);
    return err_buf;
}