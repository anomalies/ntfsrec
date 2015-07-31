/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfsrec_command.h"
#include "ntfsrec_utility.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NR_FILE_BUFFER_SIZE 8096
#define NR_FILE_MAX_RETRIES 4

struct ntfsrec_copy {
    ntfs_volume *volume;
    const char *output_name;

    struct {
        unsigned int files;
        unsigned int dirs;
        unsigned int errors;
        unsigned int retries;
    } stats;
    
    struct {
        unsigned int retries;
    } opt;
    
    char *file_buffer;
    
    char *current_path_end;
    char path[MAX_PATH_LENGTH];
};

static int ntfsrec_recurse_directory(struct ntfsrec_copy* state, ntfs_inode* folder_node, const char* name);
static int ntfsrec_cpz_directory_visitor(struct ntfsrec_copy *state, const ntfschar *name,
                                         const int name_len, const int name_type, const s64 pos,
                                         const MFT_REF mref, const unsigned dt_type);

static int ntfsrec_append_filename(struct ntfsrec_copy *state, const char *path, char **old_end);
static int ntfsrec_emit_file(struct ntfsrec_copy *state, ntfs_inode *inode, const char *name);

void ntfsrec_command_cp(struct ntfsrec_command_processor *state, char *arguments) {
    char dest_path[128];
    struct ntfsrec_copy copy_state;
    
    if (strlen(arguments) != 0) {
        if ((size_t)snprintf(dest_path, sizeof dest_path, "./%s", arguments) >= sizeof dest_path) {
            printf("Error: path %s is too long\n", arguments);
            return;
        }
    } else {
        strcpy(dest_path, ".");
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
    
    copy_state.current_path_end = copy_state.path;
    
    ntfsrec_recurse_directory(&copy_state, state->cwd_inode, dest_path);
    
    printf("Done.\nFiles:\t%u\nDirectories:\t%u\nErrors:\t%u\n", copy_state.stats.files, copy_state.stats.dirs, copy_state.stats.errors);
    
    free(copy_state.file_buffer);
    return;
}

void ntfsrec_command_cpz(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(state);
    NR_UNUSED(arguments);
    
    puts("This command isn't implemented.");
    return;
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
    
    if (mkdir(state->path, 0755) != 0) {
        printf("Error: unable to create directory %s\n", state->path);
        
        *old_path_end = '\0';
        state->current_path_end = old_path_end;
        return NR_FALSE;
    }
    
    printf("Adding directory %s\n", state->path);
    
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
        ntfsrec_emit_file(state, inode, local_name);
        ntfs_inode_close(inode);
    } else {
        printf("Error: couldn't open file %s\n", local_name);
    }
    
    free(local_name);
    
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

static int ntfsrec_emit_file(struct ntfsrec_copy *state, ntfs_inode *inode, const char *name) {
    ntfs_attr *data_attribute;
    char *old_path_end;
    
    
    if (ntfsrec_append_filename(state, name, &old_path_end) == NR_FALSE) {
        printf("Error: path %s and filename %s are too long.\n", state->path, name);
        return NR_FALSE;
    }
    
    data_attribute = ntfs_attr_open(inode, AT_DATA, NULL, 0);

    if (data_attribute != NULL) {
        int output_fd;
        unsigned int block_size = 0, retries = 0;
        s64 offset = 0;
        
        output_fd = open(state->path, O_WRONLY | O_CREAT);
        
        if (output_fd != -1) {
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
                    
                    lseek(output_fd, actual_size, SEEK_CUR);
                    
                    offset += actual_size;
                    continue;
                }
                
                if (bytes_read == 0) {
                    break;
                }
                
                if (write(output_fd, state->file_buffer, bytes_read) < 0) {
                    printf("Error: unable to write to output file %s\n", state->path);
                    
                    if (retries++ < state->opt.retries) {
                        state->stats.retries++;
                        continue;
                    }
                    
                    break;
                }
                
                retries = 0;
                offset += bytes_read;
            }
            
            close(output_fd);
        }
        
        state->stats.files++;
        ntfs_attr_close(data_attribute);
    } else {
        printf("Error: can't access the data for %s\n", name);
    }

    *old_path_end = '\0';
    state->current_path_end = old_path_end;
    return NR_TRUE;
}

