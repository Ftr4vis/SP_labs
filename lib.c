#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "ftw.h"
#include "plugin_api.h"


static char *g_lib_name = "lib.so";

static char *g_plugin_purpose = "Search for files containing at least one section from a given number of repeated bytes";

static char *g_plugin_author = "Ftr4vis";

#define OPT_SAME_BYTES_STR "same-bytes"
#define OPT_SAME_BYTES_COMP_STR "same-bytes-comp"

int check_file(FILE*, int, char*);

static struct plugin_option g_po_arr[] = {
/*
    struct plugin_option {
        struct option {
           const char *name;
           int         has_arg;
           int        *flag;
           int         val;
        } opt,
        char *opt_descr
    }
*/
    {
        {
            OPT_SAME_BYTES_STR,
            required_argument,
            0, 0,
        },
        "Section length in bytes"
    },
    {
        {
            OPT_SAME_BYTES_COMP_STR,
            required_argument,
            0, 0,
        },
        "Comparison operator"
    },
    
};

static int g_po_arr_len = sizeof(g_po_arr)/sizeof(g_po_arr[0]);

int plugin_get_info(struct plugin_info* ppi) {
    if (!ppi) {
        fprintf(stderr, "ERROR %s: invalid argument\n", g_lib_name);
        return -1;
    }
    
    ppi->plugin_purpose = g_plugin_purpose;
    ppi->plugin_author = g_plugin_author;
    ppi->sup_opts_len = g_po_arr_len;
    ppi->sup_opts = g_po_arr;
    
    return 0;
}

int plugin_process_file(const char *fname,
        struct option in_opts[],
        size_t in_opts_len) {

    // Return error by default
    int ret = -1;
    
    // Pointer to file mapping
    //unsigned char *ptr = NULL;
    
    char *DEBUG = getenv("LAB1DEBUG");


    if (!fname || !in_opts || !in_opts_len) {
        errno = EINVAL;
        return -1;
    }
    
    if (DEBUG) {
        for (size_t i = 0; i < in_opts_len; i++) {
        fprintf(stderr, "DEBUG: %s: Got option '%s' with arg '%s'\n",
            g_lib_name, in_opts[i].name, (char*)in_opts[i].flag);
        }
    }

    int same_bytes = 0;
    char *same_bytes_comp = "";
    int got_same_bytes = 0;
    int got_same_bytes_comp = 0;

    
    #define OPT_CHECK(opt_var, is_double) \
    if (got_##opt_var) { \
        if (DEBUG) { \
            fprintf(stderr, "DEBUG: %s: Option '%s' was already supplied\n", \
                g_lib_name, in_opts[i].name); \
        } \
        errno = EINVAL; \
        return -1; \
    } \
    else { \
        char *endptr = NULL; \
        opt_var = is_double ? \
                    strtod((char*)in_opts[i].flag, &endptr): \
                    strtol((char*)in_opts[i].flag, &endptr, 0); \
        if (*endptr != '\0') { \
            if (DEBUG) { \
                fprintf(stderr, "DEBUG: %s: Failed to convert '%s'\n", \
                    g_lib_name, (char*)in_opts[i].flag); \
            } \
            errno = EINVAL; \
            return -1; \
        } \
        got_##opt_var = 1; \
    }
    for (size_t i = 0; i < in_opts_len; i++) {
        if (!strcmp(in_opts[i].name, OPT_SAME_BYTES_STR)) {
            OPT_CHECK(same_bytes, 0)
        }
        if (!strcmp(in_opts[i].name, OPT_SAME_BYTES_COMP_STR)) {
            same_bytes_comp = (char*)in_opts[i].flag;
            got_same_bytes_comp = 1;
            if (got_same_bytes_comp) {}
        }
    }

    if (!got_same_bytes) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: Section length value was not supplied.\n", g_lib_name);
        }
        errno = EINVAL;
        return -1;
    }
    if (same_bytes <= 1) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: same_bytes should be > 1", g_lib_name);
        }
        return -1;
    }
    if ((same_bytes <= 2) && (!strcmp(same_bytes_comp, "lt"))) {
        if (DEBUG) {
            fprintf(stderr, "DEBUG: %s: Section length lower than 2 cannot be", g_lib_name);
        }
        return -1;
    }


    if (DEBUG) {
        fprintf(stderr, "DEBUG: %s: Inputs: same_bytes = %d, same_bytes_comp = %s\n",
        g_lib_name, same_bytes, same_bytes_comp);
    }

    int saved_errno = 0;

    FILE* fp = fopen(fname, "r");
    // Проверка открытия файла
    if (fp == NULL) {
        fprintf(stderr, "DEBUG: %s: Openning file %s", g_lib_name, fname); 
        return -1;
    }

    int res = check_file(fp, same_bytes, same_bytes_comp);
    
    ret = res;

    if (fclose (fp) == EOF) {
        fprintf(stderr, "DEBUG: %s: Closing file %s", g_lib_name, fname); 
        return -1;
    }

    //Restore errno value
    errno = saved_errno;
    return ret;

    
}
    
int check_file(FILE* mf, int in_same_bytes, char* in_same_bytes_comp) {

    int ret = 1;
    int count = 1;
    int temp;
    int got_byte;

    got_byte = getc(mf);
    temp = got_byte;

    while (1) {

        if (temp == EOF){
            goto STOP_FUNC;
        }

        got_byte = getc(mf);

        if (got_byte == EOF && feof(mf) == 0){
            printf("Error after getc()\n");
            return -1;
            goto STOP_FUNC;
        }

        if (got_byte == temp) count++;
        else {
            if (count != 1) {

                if (count > in_same_bytes) {
                    ret = 0;
                    goto STOP_FUNC;
                }

                if (!strcmp(in_same_bytes_comp, "eq"))
                    if (count == in_same_bytes) {
                        ret = 0;
                        goto STOP_FUNC;
                    }

                if (!strcmp(in_same_bytes_comp, "ne"))
                    if (count != in_same_bytes) {
                        ret = 0;
                        goto STOP_FUNC;
                    }

                if (!strcmp(in_same_bytes_comp, "gt"))
                    if (count  > in_same_bytes) {
                        ret = 0;
                        goto STOP_FUNC;
                    }

                if (!strcmp(in_same_bytes_comp, "ge"))
                    if (count >= in_same_bytes) {
                        ret = 0;
                        goto STOP_FUNC;
                    }
                    
                if (!strcmp(in_same_bytes_comp, "le"))
                    if (count <= in_same_bytes) {
                        ret = 0;
                        goto STOP_FUNC;
                    }
                    
                if (!strcmp(in_same_bytes_comp, "lt"))
                    if ((count  < in_same_bytes)) {
                        ret = 0;
                        goto STOP_FUNC;
                    }
                count = 1;
            }
            temp = got_byte;
        }
    }
    STOP_FUNC:
    return ret;
}
