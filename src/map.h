#pragma once
#include "common.h"

typedef struct cs_HMap cs_HMap;
typedef struct cs_HMap_bucket cs_HMap_bucket;

struct cs_HMap_bucket {
    u8 psl;  // 255 => empty
    u32 hash;
    u8 data[];
};

struct cs_HMap {
    cs_HMap_bucket* data;
    // cap has to be power of 2
    u32 data_cap; u32 data_used;
    u8 element_size;
};

u32 fnv1a(char* start, char* end);

cs_HMap cs_hm_init(u8 element_size);
void* cs_hm_geth(cs_HMap* hm, u32 hash);
void* cs_hm_gets(cs_HMap* hm, cs_Str* key);

void* cs_hm_seth(cs_HMap* hm, u32 hash);
void* cs_hm_sets(cs_HMap* hm, cs_Str* key);

void cs_hm_free(cs_HMap* hm);