#define _GNU_SOURCE         
#define _XOPEN_SOURCE 500
#define MAX_FILE_NAME_LENGTH 256

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ftw.h>
#include <getopt.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include "plugin_api.h"

int file_analysis(const char*, const struct stat*, int, struct FTW*);

int check_if_so(const char*);

typedef int (*pgi_func_t)(struct plugin_info*);
typedef int (*ppf_func_t)(const char*, struct option*, size_t);

struct rules {
    unsigned int AND;
    unsigned int OR;
    unsigned int NOT;
};

struct plugin { 
    void* dl;
    char* name;
    struct plugin_option* sup_long_opts;
    int sup_opts_len;
    struct option* opts_to_pass;
    int opts_to_pass_len;
    int used;
};

struct plugin* plugins;
struct rules rules = { 0, 0, 0 };
struct option* longopts;
char* dir_plugins;
int lib_amount = 0;
int at_least_one_file = 0;

int main(int argc, char* argv[])  {
    char* dir_to_analyze = strdup(argv[argc - 1]);
    dir_plugins = get_current_dir_name();

    char* DEBUG = getenv("LAB1DEBUG");

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <<options> dir>\nFor help enter flag -h\n", argv[0]);
        goto END;
    }

    int is_P = 0;
    for (int i = 0; i < argc; i++) {

        if (strcmp("-h", argv[i]) == 0) {
            printf("-P dir\tКаталог с плагинами.\n");
            printf("-A\tОбъединение опций плагинов с помощью операции «И» (действует по умолчанию)..\n");
            printf("-O\tОбъединение опций плагинов с помощью операции «ИЛИ».\n");
            printf("-N\tИнвертирование условия поиска (после объединения опций плагинов с помощью -A или -O).\n");
            printf("-v\tВывод версии программы и информации о программе (ФИО исполнителя, номер группы, номер варианта лабораторной).\n");
            printf("-h\tВывод справки по опциям.\n");
            goto END;
        }

        if (strcmp("-v", argv[i]) == 0) {
            printf("Version: 1.0\nAuthor: Ftr4vis\n");
            goto END;
        }

        if (strcmp(argv[i], "-P") == 0) {
            if (is_P) { 
                fprintf(stderr, "ERROR: Expected only one option -- 'P'\n");
                goto END;
            }
            
            if (i == argc - 1) {
                fprintf(stderr, "ERROR: Expected last parameter to be directory\n");
                goto END;
            }

            else {
                free(dir_plugins);
                dir_plugins = strdup(argv[i + 1]);
                is_P = 1;
            }

        }
    }

    if (DEBUG) fprintf(stderr, "LAB1DEBUG: Plugins directory: %s\n", dir_plugins);

    DIR* dp;
    struct dirent* dir;
    dp = opendir(dir_plugins);

    if (dp != NULL) {
        while ((dir = readdir(dp)) != NULL) {
            if ((dir->d_type) == DT_REG) lib_amount++;
        }
        closedir(dp);
    }
    
    else {
        fprintf(stderr, "ERROR: opendir '%s'", dir_plugins);
        goto END;
    }

    plugins = (struct plugin*)calloc(lib_amount, sizeof(struct plugin));
    if (!plugins) {
        fprintf(stderr, "ERROR: calloc");
        goto END;
    }

    dp = opendir(dir_plugins);
    lib_amount = 0;

    fprintf(stdout, "\nInformation about plugins:\n\n");
    if (dp != NULL) {
        
        while ((dir = readdir(dp)) != NULL) {
            
            if ((dir->d_type) == DT_REG) {

                if (check_if_so(dir->d_name)) {

                    char lib_name[MAX_FILE_NAME_LENGTH + 1];
                    snprintf(lib_name, sizeof(lib_name), "%s/%s", dir_plugins, dir->d_name);

                    void* dl = dlopen(lib_name, RTLD_LAZY);
                    if (!dl) {
                        fprintf(stderr, "ERROR: dlopen() failed: %s\n", dlerror());
                        continue;
                    }

                    void* func = dlsym(dl, "plugin_get_info");
                    if (!func) {
                        fprintf(stderr, "ERROR: dlsym() failed: %s\n", dlerror());
                    }

                    struct plugin_info pi = { 0 };

                    pgi_func_t pgi_func = (pgi_func_t)func;

                    int ret = pgi_func(&pi);
                    if (ret < 0) {
                        fprintf(stderr, "ERROR: plugin_get_info() failed: '%s'\n", dir->d_name);
                    }

                    plugins[lib_amount].sup_opts_len = pi.sup_opts_len;
                    plugins[lib_amount].name = strdup(dir->d_name);
                    plugins[lib_amount].sup_long_opts = pi.sup_opts;
                    plugins[lib_amount].dl = dl;
                    plugins[lib_amount].opts_to_pass = calloc(pi.sup_opts_len, sizeof(struct option));
                    plugins[lib_amount].opts_to_pass_len = 0;
                    plugins[lib_amount].used = 0;

                    // Plugin info
                    fprintf(stdout, "Plugin name:\t\t%s\n", dir->d_name);
                    fprintf(stdout, "Plugin purpose:\t\t%s\n", pi.plugin_purpose);
                    fprintf(stdout, "Plugin author:\t\t%s\n", pi.plugin_author);
                    fprintf(stdout, "Supported options: ");
                    if (pi.sup_opts_len > 0) {
                        fprintf(stdout, "\n");
                        for (size_t i = 0; i < pi.sup_opts_len; i++) {
                            fprintf(stdout, "\t--%s\t\t%s\n", pi.sup_opts[i].opt.name, pi.sup_opts[i].opt_descr);
                        }
                    }
                    else {
                        fprintf(stdout, "none (!?)\n");
                    }
                    fprintf(stdout, "\n");

                    lib_amount++;
                }
            }
        }
        closedir(dp);
    }


    if (!lib_amount) {
        fprintf(stderr, "ERROR: Can't find any library at '%s'\n", dir_plugins);
        goto END;
    }

    int opt_amount = 0;
    for (int i = 0; i < lib_amount; i++) {
        opt_amount += plugins[i].sup_opts_len;
    }

    longopts = (struct option*)calloc(opt_amount, sizeof(struct option));
    if (!longopts) {
        fprintf(stderr, "ERROR: calloc");
        goto END;
    }

    opt_amount = 0;
    for (int i = 0; i < lib_amount; i++) {
        for (int j = 0; j < plugins[i].sup_opts_len; j++) {
            longopts[opt_amount] = plugins[i].sup_long_opts[j].opt;
            opt_amount++;
        }
    }

    int res;
    while (1)
    {
        int opt_ind = -1;
        res = getopt_long(argc, argv, "P:AON", longopts, &opt_ind);
        if (res == -1)
            break;
        switch (res) {
        case 'A':
            if (!rules.AND) {
                if (!rules.OR) {
                    rules.AND = 1;
                }
                else {
                    fprintf(stderr, "ERROR: Didn't expect -O and -A at the sametime \n");
                    goto END;
                }
            }
            else {
                fprintf(stderr, "ERROR: Expected only one option: -A\n");
                goto END;
            }
            break;
        case 'O':
            if (!rules.OR) {
                if (!rules.AND) {
                    rules.OR = 1;
                }
                else {
                    fprintf(stderr, "ERROR: Didn't expect -O and -A at the sametime\n");
                    goto END;
                }
            }
            else {
                fprintf(stderr, "ERROR: expected only one option: -O\n");
                goto END;
            }
            break;
        case 'N':
            if (!rules.NOT) {
                rules.NOT = 1;
            }
            else {
                fprintf(stderr, "ERROR: expected only one option: 'N'\n");
                goto END;
            }
            break;
        case ':':
            goto END;
        case '?':
            goto END;
        default:
            if (opt_ind != -1) {
                for (int i = 0; i < lib_amount; i++) {
                    for (int j = 0; j < plugins[i].sup_opts_len; j++) {
                        if (strcmp(longopts[opt_ind].name, plugins[i].sup_long_opts[j].opt.name) == 0) {
                            memcpy(plugins[i].opts_to_pass + plugins[i].opts_to_pass_len, longopts + opt_ind, sizeof(struct option));
                            // Argument (if any) is passed in flag
                            if ((longopts + opt_ind)->has_arg) {
                                (plugins[i].opts_to_pass + plugins[i].opts_to_pass_len)->flag = (int*)strdup(optarg);
                            }
                            plugins[i].opts_to_pass_len++;
                            plugins[i].used = 1;
                        }
                    }
                }

            }
        }
    }

    if (!rules.OR && !rules.AND) {
        rules.AND = 1;
    }

    if (DEBUG) {
        fprintf(stderr, "AND: %d OR: %d NOT: %d\n", rules.AND, rules.OR, rules.NOT);
    }

    if (DEBUG) {
        fprintf(stderr, "Searching in directory: %s\n", dir_to_analyze);
    }

    printf("Result:\n\n");
    res = nftw(dir_to_analyze, file_analysis, 10, FTW_PHYS);
    if (res < 0) {
        fprintf(stderr, "ERROR: nftw\n");
        goto END;
    }

    if (!at_least_one_file) {
        fprintf(stdout, "Nothing found.\n");
    }



END:
    // Cleaning memory
    for (int i = 0; i < lib_amount; i++) {
        for (int j = 0; j < plugins[i].opts_to_pass_len; j++) {
            if ((plugins[i].opts_to_pass + j)->flag) free((plugins[i].opts_to_pass + j)->flag);
        }
        if (plugins[i].name) free(plugins[i].name);
        if (plugins[i].opts_to_pass) free(plugins[i].opts_to_pass);
        if (plugins[i].dl) dlclose(plugins[i].dl);
    }

    if (plugins) free(plugins);
    if (longopts) free(longopts);
    if (dir_plugins) free(dir_plugins);
    if (dir_to_analyze) free(dir_to_analyze);
    if (dir) free(dir);
    return 0;

}

int file_analysis(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf) {
    sb = sb;
    typeflag = typeflag;
    ftwbuf = ftwbuf;

    
    if (typeflag == FTW_F) {
        int fits = 0;
        int counter = 0;
        for (int i = 0; i < lib_amount; i++) {
            if (plugins[i].used) {

                void* func = dlsym(plugins[i].dl, "plugin_process_file");
                ppf_func_t ppf_func = (ppf_func_t)func;
                int res = ppf_func(fpath, plugins[i].opts_to_pass, plugins[i].opts_to_pass_len);
                if (res < 0) {
                    fprintf(stderr, "ERROR (Plugin: %s) Something's wrong with plugin_process_file %s\n", plugins[i].name, fpath);
                    return 1;
                }
                if (res == 0) counter++;
            }
        }
        if (rules.NOT == 1) {

            if ((rules.AND == 1) && (counter != lib_amount)) {
                fits = 1;
            }

            if ((rules.OR == 1) && (counter == 0)){
                fits = 1;
            }
        }
        if (rules.NOT == 0) {

            if ((rules.AND == 1) && (counter == lib_amount)) {
                fits = 1;
            }

            if ((rules.OR == 1) && (counter > 0)) {
                fits = 1;
            }
        }

        if (fits) {
            at_least_one_file = 1;
            printf("The file %s fits the criteria and rules\n", fpath);
        }
    }
    return 0;
}

int check_if_so (const char* filename) {

    int filename_len = 0;
    while(*(filename+filename_len)) filename_len++;

    if ((*(filename + filename_len-1) == 'o') &&
        (*(filename + filename_len-2) == 's') &&
        (*(filename + filename_len-3) == '.')) 
        return 1;
    else
        return 0;

}

