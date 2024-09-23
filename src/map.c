#include "map.h"
#include <stdlib.h>

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
    cs_HMap_bucket* buck = (cs_HMap_bucket*)advance_ptr(hm->data, hm->element_size * slot);
    while (buck->hash != hash) {
        buck = (cs_HMap_bucket*)advance_ptr(buck, hm->element_size);
        if (buck->psl == 255) return null;
    }
    return buck->data;
}

void* cs_hm_gets(cs_HMap* hm, cs_Str* key)
{
    u32 hash = fnv1a((char*)key->data, (char*)key->data + key->size);
    return cs_hm_geth(hm, hash);
}

void* cs_hm_seth(cs_HMap* hm, u32 hash)
{
    if ((hm->data_used + 1.0) / hm->data_cap > 0.8) {
        // TODO: resize
        return null;
    }
    hash = hash ^ (hash >> 16);
    u32 slot = hash & (hm->data_cap - 1);
    cs_HMap_bucket* buck = (cs_HMap_bucket*)advance_ptr(hm->data, hm->element_size * slot);
    if (buck->psl == 255) {
        // insert value here
        buck->psl = 0; buck->hash = hash;
        return buck->data;
    }
    u8 psl = 0;
    // TODO: robin hoood hashing
}

void* cs_hm_sets(cs_HMap* hm, cs_Str* key)
{
    u32 hash = fnv1a((char*)key->data, (char*)key->data + key->size);
    return cs_hm_seth(hm, hash);
}

void cs_hm_free(cs_HMap* hm);