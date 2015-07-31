/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfsrec_command.h"
#include "ntfsrec_utility.h"
#include <dirent.h>
#include <sys/stat.h>
#include <zip.h>

#define NR_FILE_BUFFER_SIZE 8096
#define NR_FILE_MAX_RETRIES 4
struct ntfsrec_copy {
    ntfs_volume *volume;
    const char *output_name;
    struct zip *zipfile;
    
    struct {
        size_t files;
        size_t dirs;
        size_t errors;
        size_t retries;
    } stats;
    
    struct {
        unsigned int retries;
    } opt;
    
    char *file_buffer;
    
    char *current_path_end;
    char path[MAX_PATH_LENGTH];
};

struct ntfsrec_zip_source {
    struct ntfsrec_copy *state;
    ntfs_inode *inode;
    ntfs_attr *data_attr;
    MFT_REF mft_ref;
    const char *name;
    unsigned int block_size;
    s64 offset;
};

static int ntfsrec_recurse_directory(struct ntfsrec_copy* state, ntfs_inode* folder_node, const char* name);
static DIR * ntfsrec_create_or_open(const char *directory);
static int ntfsrec_cpz_directory_visitor(struct ntfsrec_copy *state, const ntfschar *name,
                                         const int name_len, const int name_type, const s64 pos,
                                         const MFT_REF mref, const unsigned dt_type);

static int ntfsrec_zip_add_file(struct ntfsrec_copy *state, ntfs_inode *inode, const char *name);
static zip_int64_t ntfsrec_zip_source(struct ntfsrec_zip_source *source, void *data, zip_uint64_t len, enum zip_source_cmd cmd);
static int ntfsrec_append_filename(struct ntfsrec_copy *state, const char *path, char **old_end);

void ntfsrec_command_cp(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(state);
    if (strlen(arguments) == 0) {
        puts("Usage: cp <host directory>");
        return;
    }

    puts("Error: this feature is currently unimplemented.\n");
    return;
}

void ntfsrec_command_cpz(struct ntfsrec_command_processor *state, char *arguments) {
    struct ntfsrec_copy copy_state;
    int open_error = 0;
    
    if (strlen(arguments) == 0) {
        puts("Usage: cpz <host output zip>");
        return;
    }
    
    memset(copy_state.path, 0, sizeof copy_state.path);
    
    copy_state.volume = state->reader->mount.volume;
    copy_state.output_name = arguments;
    copy_state.stats.files = 0;
    copy_state.stats.dirs = 0;
    copy_state.stats.errors = 0;
    copy_state.stats.retries = 0;
    copy_state.opt.retries = NR_FILE_MAX_RETRIES;
    
    copy_state.file_buffer = ntfsrec_allocate(NR_FILE_BUFFER_SIZE);
    
    copy_state.zipfile = zip_open(arguments, ZIP_CREATE | ZIP_EXCL, &open_error);
    
    if (copy_state.zipfile == NULL) {
        switch(open_error) {
            case ZIP_ER_EXISTS:
                printf("Error: output file %s already exists\n", arguments);
                break;
                
            case ZIP_ER_INVAL:
                printf("Error: invalid path %s\n", arguments);
                break;
            
            case ZIP_ER_OPEN:
                printf("Error: the file %s could not be opened\n", arguments);
                break;
                
            default:
                printf("Error: unable to create zip file %s\n", arguments);
                break;
        }
        
        return;
    }
    
    copy_state.current_path_end = copy_state.path;
    
    ntfsrec_recurse_directory(&copy_state, state->cwd_inode, "\0");
    
    printf("Done.\n");
    
    zip_close(copy_state.zipfile);
    free(copy_state.file_buffer);
}

static int ntfsrec_recurse_directory(struct ntfsrec_copy *state, ntfs_inode *folder_node, const char *name) {
    char *old_path_end;
    int name_length;
    s64 position = 0;
    const int max_length = MAX_PATH_LENGTH - (state->current_path_end - state->path);
    
    old_path_end = state->current_path_end;
    name_length = snprintf(state->current_path_end, max_length, "%s/", name);
    
    if (name_length >= max_length) {
        printf("Error: path %s exceeded the maximum allowable lenth.\n", state->path);
        *old_path_end = '\0';
        return NR_FALSE;
    }
    
    state->current_path_end = &state->current_path_end[name_length];
    
    printf("Adding directory %s\n", state->path);
    
    if (zip_dir_add(state->zipfile, state->path, ZIP_FL_ENC_UTF_8) < 0) {
        printf("Error: unable to create directory %s in zipfile.\n", state->path);
        state->current_path_end = old_path_end;
        *old_path_end = '\0';
        return NR_FALSE;
    }
    
    if (ntfs_readdir(folder_node, &position, state, (ntfs_filldir_t)ntfsrec_cpz_directory_visitor) != 0) {
        printf("Error: unable to traverse directory %s\n", state->path);
    }
    
    state->stats.dirs++;
    *old_path_end = '\0';
    state->current_path_end = old_path_end;
    return NR_TRUE;
}

static int ntfsrec_cpz_directory_visitor(struct ntfsrec_copy *state, const ntfschar *name, const int name_len, const int name_type, const s64 pos, const MFT_REF mref, const unsigned dt_type) {
    char *local_name = NULL;
    ntfs_inode *inode;
    
    NR_UNUSED(pos);
    
    if ((name_type & FILE_NAME_WIN32_AND_DOS) == FILE_NAME_DOS) {
        return 0;
    }
    
    if (ntfs_ucstombs(name, name_len, &local_name, MAX_PATH_LENGTH) < 0) {
        puts("Error: this filename can't be represented in your locale.");
        return 0;
    }
    
    if (dt_type & NTFS_DT_DIR) {
        ntfs_inode *dir_inode;
        
        if (strcmp(local_name, ".") == 0 || strcmp(local_name, "..") == 0 ||
            strcmp(local_name, "./") == 0 || strcmp(local_name, "../") == 0) {
            free(local_name);
            
            return 0;
        }
        
        dir_inode = ntfs_inode_open(state->volume, mref);
        
        if (dir_inode == NULL) {
            printf("Error: couldn't open folder %s\n", local_name);
            free(local_name);
            return 0;
        }
        
        ntfsrec_recurse_directory(state, dir_inode, local_name);
        
        ntfs_inode_close(dir_inode);
        free(local_name);
        return 0;
    }
    
    inode = ntfs_inode_open(state->volume, mref);
    
    if (inode != NULL) {
        ntfsrec_zip_add_file(state, inode, local_name);
        ntfs_inode_close(inode);
    } else {
        printf("Error: couldn't open file %s\n", local_name);
    }
    
    free(local_name);
    
    return 0;
}

static int ntfsrec_zip_add_file(struct ntfsrec_copy *state, ntfs_inode *inode, const char *name) {
    struct ntfsrec_zip_source source;
    struct zip_source *zip_source;
    char *old_path_end;
    int result = NR_FALSE;
    
    memset(&source, 0, sizeof source);
    source.state = state;
    source.inode = inode;
    
    printf("Adding file %s\n", name);
    
    if (ntfsrec_append_filename(state, name, &old_path_end) == NR_FALSE) {
        printf("Error: current path %s and filename %s are too long.\n", state->path, name);
        return NR_FALSE;
    }
    
    source.name = state->path;
    
    zip_source = zip_source_function(state->zipfile, (zip_source_callback)ntfsrec_zip_source, &source);
    
    if (zip_source != NULL) {
        if (zip_file_add(state->zipfile, state->path, zip_source, ZIP_FL_ENC_UTF_8) >= 0) {
            state->stats.files++;
            result = NR_TRUE;
        } else {
            zip_source_free(zip_source);
            printf("Error: unable to add file %s to zip; reason: %s\n", state->path, zip_strerror(state->zipfile));
        }
    } else {
        printf("Error: unable to create zip source for %s\n", state->path);
    }
    
    printf("2 Added file %s\n", state->path);
    
    *old_path_end = '\0';
    state->current_path_end = old_path_end;
    
    return result;
}

static zip_int64_t ntfsrec_zip_source(struct ntfsrec_zip_source *source, void *data, zip_uint64_t len, enum zip_source_cmd cmd) {
    switch (cmd) {
        case ZIP_SOURCE_OPEN: {
            printf("1 Reached callback for open\n");
            
            source->data_attr = ntfs_attr_open(source->inode, AT_DATA, NULL, 0);
            
            if (source->data_attr == NULL) {
                printf("Error: can't access the data for %s\n", source->name);
                return -1;
            }
            
            return 0;
        }
        
        case ZIP_SOURCE_CLOSE: {
            if (source->data_attr != NULL) {
                ntfs_attr_close(source->data_attr);
            }
            
            return 0;
        }
        
        case ZIP_SOURCE_READ: {
            unsigned int retries = 0;
            s64 bytes_read;
            
            for(;;) {
                bytes_read = ntfs_attr_pread(source->data_attr, source->offset, len, data);
                
                if (bytes_read == -1) {
                    if (retries++ < source->state->opt.retries) {
                        source->state->stats.retries++;
                        continue;
                    }
                    
                    source->state->stats.errors++;
                    printf("Error: failed %u times to read %s, skipping.\n", retries, source->name);
                    return -1;
                }
                
                break;
            }
            
            return bytes_read;
        }
        
        case ZIP_SOURCE_STAT: {
            struct zip_stat *stat = data;
            
            zip_stat_init(stat);
            
            return sizeof stat;
        }
        
        case ZIP_SOURCE_ERROR: {
            int *errors = data;
            errors[0] = ZIP_ER_INVAL;
            errors[1] = EINVAL;
            return sizeof (int) * 2;
        }
        
        default:
            break;
    }
    
    return 0;
}

static int ntfsrec_append_filename(struct ntfsrec_copy *state, const char *path, char **old_end) {
    int remaining_size = MAX_PATH_LENGTH - (state->current_path_end - state->path);
    int added_length;
    
    *old_end = state->current_path_end;
    
    added_length = snprintf(state->current_path_end, remaining_size, "%s", path);
    
    if (added_length >= remaining_size) {
        *state->current_path_end = '\0';
        return NR_FALSE;
    }
    
    state->current_path_end += added_length;
    return NR_TRUE;
}

/*
 static int ntfsrec_zip_add_file(struct ntfsrec_copy *state, ntfs_inode *inode, const char *name) {
    ntfs_attr *data_attribute;
    
    data_attribute = ntfs_attr_open(inode, AT_DATA, NULL, 0);
    
    if (data_attribute != NULL) {
        unsigned int block_size = 0, retries = 0;
        s64 offset = 0;
        
        if (inode->mft_no < 2) {
            block_size = state->volume->mft_record_size;
        }
        
        for(;;) {
            s64 bytes_read = 0;
            
            if (block_size > 0) {
                bytes_read = ntfs_attr_mst_pread(data_attribute, offset, 1, block_size, state->file_buffer);
                bytes_read *= block_size;
            } else {
                bytes_read = ntfs_attr_pread(data_attribute, offset, NR_FILE_BUFFER_SIZE, state->file_buffer);
            }
            
            if (bytes_read == -1) {
                unsigned int actual_size = block_size > 0 ? block_size : NR_FILE_BUFFER_SIZE;
                
                if (retries++ < state->opt.retries) {
                    state->stats.retries++;
                    continue;
                }
                
                state->stats.errors++;
                printf("Error: failed %u times to read %s, skipping %d bytes\n", retries, name, actual_size);
                offset += actual_size;
                continue;
            }
            
            if (bytes_read == 0) {
                break;
            }
            
            offset += bytes_read;
        }
        
        state->stats.files++;
        ntfs_attr_close(data_attribute);
    } else {
        printf("Error: can't access the data for %s\n", name);
    }
    
    return NR_TRUE;
}
*/
