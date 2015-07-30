/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfsrec_utility.h"

static int ntfsrec_calculate_up_path(char *buffer, size_t max_length, const char *base, const char *path);
static void ntfsrec_move_up_one(char *base, char **pend);

void ntfsrec_utility_format_size(char *buffer, size_t maxsize, int64_t size_value) {
    static const char *prefixes[] = { "B", "K", "M", "G", "T", "P", "E" };
    int prefix = 0;
    double size_decimal = size_value;
    
    while(size_decimal >= 1024) {
        size_decimal /= 1024;
        ++prefix;
    }
    
    snprintf(buffer, maxsize, "%.1f%s", size_decimal, prefixes[prefix]);
}

int ntfsrec_calculate_path(char* output, size_t max_length, const char* base, const char* path) {
    if (*path == '/') {
        /* Absolute */
        return (size_t)snprintf(output, max_length, "%s", path) < max_length ? NR_TRUE : NR_FALSE;
    } else if (*path == '.') {        
        if (path[1] == '.' && path[2] == '/') {
            return ntfsrec_calculate_up_path(output, max_length, base, path);
        } else if (path[1] == '/') {
            /* Relative */
            path += 2;
        }
    }
    
    return (size_t)snprintf(output, max_length, "%s%s", base, path) < max_length ? NR_TRUE : NR_FALSE;
}

static int ntfsrec_calculate_up_path(char *buffer, size_t max_length, const char *base, const char *path) {
    char *end;
    size_t path_length, length = (size_t)snprintf(buffer, max_length, "%s", base);
    
    if (length >= max_length)
        return NR_FALSE;
    
    end = &buffer[length];
    
    while(strncmp(path, "../", 3) == 0) {
        ntfsrec_move_up_one(buffer, &end);
        
        path += 3;
    }
    
    path_length = strlen(path);
    
    if ((path_length + (end - buffer) + 1) >= max_length)
        return NR_FALSE;
    
    memcpy(end + 1, path, path_length + 1);
    
    return NR_TRUE;
}

static void ntfsrec_move_up_one(char *base, char **pend) {
    char *end = *pend;
    
    if ((end - 2) <= base) {
        return;
    }
    
    end -= 2;
    
    while(end > base) {
        if (*end == '/') {
            *pend = end - 1;
            return;
        }
        
        --end;
    }
    
    *pend = base;
    return;
}