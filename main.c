#include <stdio.h>
#include <stdlib.h>

#include "src/cisp.h"
#include "src/console.h"

int main(int argc, char** argv) {
    if (argc == 2) {
        // [file] => run file and exit
        FILE* f;
        errno_t err = fopen_s(&f, argv[1], "r");
        if (err != 0) {
            log_fatal("Datei \"%s\" konnte nicht geöffnet werden.");
            return -1;
        }
        fseek(f, 0, SEEK_END);
        u32 file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* content = malloc(file_size+1);
        u32 real_size = fread_s(content, file_size, 1, file_size, f);
        content[real_size] = 0;

        cs_Context ctx = cs_init();
        //cs_Object* code = cs_parse_cstr(ctx, content, real_size);
        //cs_Object* result = cs_eval(ctx, code);
        return 0;
    }
// [] => run as repl
}