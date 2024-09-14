#include "cisp.h"
#include "console.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    char* cur = result.freelist;
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

/* ==== VM ==== */
// TODO: arena allocator
// TODO: implementent code generation


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

// TODO:
// (+|-)?[0-9]+ (\.[0-9]+)? (e(+|-)[0-9]+)?
static cs_Object* parse_number(cs_Context* c) 
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
        return null;
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
            return null;
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
    
    if (is_float) {
        if (is_neg) float_val *= -1;
        cs_Object* result = cs_make_object(c);
        cs_obj_settype(result, CS_ATOM_FLOAT); 
        result->cdr = reinterpret(float_val, void*);
        return result;
    } else {
        if (is_neg) int_val *= -1;
        cs_Object* result = cs_make_object(c);
        cs_obj_settype(result, CS_ATOM_INT); result->cdr = (void*)int_val;
        return result;
    }
}

static cs_Object* parse_str_lit(cs_Context* c)
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
    cs_Object* result = cs_make_object(c);
    cs_obj_settype(result, CS_ATOM_STR);
    result->cdr = (void*)str;
    return result;
}

static u32 fnv1a(char* start, char* end)
{
    u64 magic_prime = 16777619;
    u32 hash = 2166136261;

    for (; start < end; start++) {
        hash = (hash ^ *start) * magic_prime;
    }

    return hash;
}

static cs_Object* cs_parse_expr(cs_Context* c);

static cs_Object* parse_symbol(cs_Context* c) 
{
    char* start = c->cur;
    while (!is_whitespace(cur())) {
        advance();
    }
    char* end = c->cur;
    u64 hash = fnv1a(start, end);
    cs_Object* result = cs_make_object(c);
    cs_obj_settype(result, CS_ATOM_SYMBOL);
    result->cdr = (void*)hash;
    return result;
}

static cs_Object* parse_keyword(cs_Context* c)
{
    cs_Object* result = parse_symbol(c);
    cs_obj_settype(result, CS_ATOM_KEYWORD);
    return result;
}

static cs_Object* parse_function(cs_Context* c)
{
    skip_whitespace(c);   
    if (cur() == '[') {
        // parse arguments
    }
}

static cs_Object* parse_while(cs_Context* c) 
{
    skip_whitespace(c);
    cs_Object* condition = cs_parse_expr(c);
    
}

// (true|false|nil|int|float|str|symbol|keyword)
static cs_Object* parse_atom(cs_Context* c)
{
    skip_whitespace(c);
    switch (cur()) {
        case '\0': {
            cs_error(c, CS_UNEXPECTED_EOF);
            return null;
        }
        case '+':
        case '-': {
            char* cur_start = c->cur;
            cs_Object* res = parse_number(c);
            if (res == null) { 
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
                cs_Object* result = cs_make_object(c);
                cs_obj_settype(result, CS_ATOM_NIL);
                return result;
            } else return parse_symbol(c);
        } break;
        case 't': {
            advance();
            if (match(c, "rue")) {
                cs_Object* result = cs_make_object(c);
                cs_obj_settype(result, CS_ATOM_TRUE);
                return result;
            } else return parse_symbol(c);
        } break;
        case 'f': {
            advance();
            if (match(c, "alse")) {
                cs_Object* result = cs_make_object(c);
                cs_obj_settype(result, CS_ATOM_FALSE);
                return result;
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

cs_Object* cs_parse_expr(cs_Context* c)
{
    cs_Object* result = null;
    skip_whitespace(c);
    if (cur() == '(') {
        advance();
        // callable expr represented as a list
        result = cs_make_object(c);
        cs_obj_settype(result, CS_LIST);
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
    else result = parse_atom(c);
    return result;
}

// NOTE: RESETS LAST USED OBJECT POOL (if you reuse your context)
cs_Object* cs_parse_cstr(cs_Context* c, char* content, u32 len)
{
    if (len == 0) len = strlen(content);
    c->start = content;
    c->cur = c->start; c->len = len;
    // pool hopefully initalized at this point
    cs_pool_clear(&c->obj_pool);
    return cs_parse_expr(c);
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

cs_Object* cs_eval(cs_Context* c, cs_Object* code)
{
    // TODO: evaluation
    if (code == null) printf("<empty>\n");
    cs_serialize_object(code);
    return code;
}

cs_Object* cs_run(cs_Context* c, char* content, u32 len)
{
    const char* a = "+";
    const char* b = "(";
    u32 ah = fnv1a(a, a+1); u32 bh = fnv1a(b, b+1);
    cs_Object* code = cs_parse_cstr(c, content, len);
    return cs_eval(c, code);
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
        case CS_OUT_OF_MEM          : { msg = "Out of memory"; break; }
        default                     : { msg = "!Invalid Error! at %d:%d"; break; }
    }
    snprintf(err_buf, 512, msg, c->err_line, c->err_col);
    return err_buf;
}