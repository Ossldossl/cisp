#include "map.h"
#include "common.h"
#include "console.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TEST HMAP

int main()
{
    cs_HMap hm = cs_hm_init(5);
    init_console();
    for (int i = 0; i < 1500; i++) {
        u32 hash = rand() % UINT32_MAX;
        log_debug("%d: %u", i, hash);
        char buf[10];
        snprintf(buf, 10, "%04d", i);
        char* str = cs_hm_seth(&hm, hash);
        memcpy_s(str, 5, buf, 5);
        str[4] = 0;
        char* result = cs_hm_geth(&hm, hash);
        if (str == result) {
            log_debug("%d: %s => %s; %s\n", i, str, result, "PASSED");
        } else {
            log_error("%d: %s => %s; %s\n", i, str, result, "FAILED");
            return -1;
        }
    }    
    cs_hm_free(&hm);
}