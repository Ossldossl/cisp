#include "map.h"
#include "console.h"
#include <stdlib.h>
#include <string.h>

u32 fnv1a(char* start, char* end)
{
    u64 magic_prime = 16777619;
    u32 hash = 2166136261;

    for (; start < end; start++) {
        hash = (hash ^ *start) * magic_prime;
    }

    return hash;
}

cs_HMap cs_hm_init(u8 element_size)
{
    cs_HMap result;
    result.element_size = sizeof(cs_HMap_bucket) + element_size;
    result.data_cap = 16;
    result.data_used = 0;
    result.data = malloc(result.element_size * result.data_cap);
    cs_HMap_bucket* buck = result.data;
    for (int i = 0; i < result.data_cap; i++) {
        buck->psl = 255;
        buck->hash = 0;
        buck = (cs_HMap_bucket*)advance_ptr(buck, result.element_size);
    }
    return result;
}

void* cs_hm_geth(cs_HMap* hm, u32 hash)
{
    hash = hash ^ (hash >> 16);
    u32 slot = hash & (hm->data_cap - 1);
    cs_HMap_bucket* end = advance_ptr(hm->data, hm->element_size * hm->data_cap);
    cs_HMap_bucket* buck = advance_ptr(hm->data, hm->element_size * slot);
    u8 psl = 0;
    while (true) {
        if (buck >= end) {
            buck = hm->data;
        }
        if (buck->hash == hash) return buck->data; // found bucket
        
        // if bucket is empty or if we hit a bucket with a psl lower than the distance from the starting location 
        if (buck->psl == 255 || buck->psl < psl) return null;
        buck = advance_ptr(buck, hm->element_size);
        psl++;
    }
}

void* cs_hm_gets(cs_HMap* hm, cs_Str* key)
{
    u32 hash = fnv1a((char*)key->data, (char*)key->data + key->size);
    return cs_hm_geth(hm, hash);
}

static cs_HMap_bucket* place_bucket(cs_HMap_bucket* cur, u8 psl, u32 hash, u32 element_size, cs_HMap_bucket* end, cs_HMap_bucket* start)
{
    while (psl <= cur->psl) {
        if (cur >= end) {
            cur = start;
        }
        if (cur->psl == 255 || cur->hash == hash) {
            // empty bucket found, finished
            cur->psl = psl;
            cur->hash = hash;
            return cur;
        }
        cur = advance_ptr(cur, element_size);
        psl++;
    }
    // found bucket to replace
    cs_HMap_bucket* next = place_bucket(advance_ptr(cur, element_size), cur->psl+1, cur->hash, element_size, end, start);
    memcpy_s(next, element_size, cur, element_size);
    cur->psl = psl; cur->hash = hash;
    return cur;
}

static void resize_hm(cs_HMap* hm)
{
    u32 old_cap = hm->data_cap;
    hm->data_cap <<= 1;
                                                        // next power of 2
    cs_HMap_bucket* new = malloc(hm->element_size * (hm->data_cap));
    
    // init new data
    cs_HMap_bucket* new_end = advance_ptr(new, hm->element_size * (hm->data_cap));
    cs_HMap_bucket* new_cur = new;
    while (new_cur < new_end) {
        new_cur->hash = 0; new_cur->psl = 255;
        new_cur = advance_ptr(new_cur, hm->element_size);
    }

    // resize & rehash
    cs_HMap_bucket* end = advance_ptr(hm->data, hm->element_size * (old_cap));
    cs_HMap_bucket* cur = hm->data;
    while (cur < end) {
        // if bucket is not empty
        if (cur->psl != 255) {
            // transfer bucket from old to new position
            u32 new_slot = cur->hash & (hm->data_cap - 1);
            cs_HMap_bucket* buck = advance_ptr(new, hm->element_size * new_slot);
            cs_HMap_bucket* result = place_bucket(buck, 0, cur->hash, hm->element_size, new_end, new);
            memcpy_s(result->data, hm->element_size - sizeof(cs_HMap_bucket), buck->data, hm->element_size - sizeof(cs_HMap_bucket));
        } 
        cur = advance_ptr(cur, hm->element_size);
    }
    free(hm->data);
    hm->data = new;
}

void* cs_hm_seth(cs_HMap* hm, u32 hash)
{
    if ((hm->data_used + 1.0) / hm->data_cap > 0.8) {
        resize_hm(hm);
    }
    hash = hash ^ (hash >> 16);
    u32 slot = hash & (hm->data_cap - 1);
    cs_HMap_bucket* buck = advance_ptr(hm->data, hm->element_size * slot);
    hm->data_used++;
    cs_HMap_bucket* result = place_bucket(buck, 0, hash, hm->element_size, advance_ptr(hm->data, hm->element_size * hm->data_cap), hm->data);
    return result->data;
}

void* cs_hm_sets(cs_HMap* hm, cs_Str* key)
{
    u32 hash = fnv1a((char*)key->data, (char*)key->data + key->size);
    return cs_hm_seth(hm, hash);
}

void cs_hm_free(cs_HMap* hm)
{
    free(hm->data);
}
