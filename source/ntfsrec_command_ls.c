/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfsrec_command.h"
#include "ntfsrec_utility.h"

static int ntfsrec_ls_directory_visitor(struct ntfsrec_command_processor *state, const ntfschar *name,
                                        const int name_len, const int name_type, const s64 pos,
                                        const MFT_REF mref, const unsigned dt_type);

void ntfsrec_command_ls(struct ntfsrec_command_processor *state, char *arguments) {
    s64 position = 0;
    
    if (*arguments == '\0') {
        ntfs_readdir(state->cwd_inode, &position, state, (ntfs_filldir_t)ntfsrec_ls_directory_visitor);
    } else {
        char new_path[MAX_PATH_LENGTH];
        
        if (ntfsrec_calculate_path(new_path, sizeof new_path, state->cwd, arguments) < MAX_PATH_LENGTH) {
            ntfs_inode *inode = ntfs_pathname_to_inode(state->reader->mount.volume, NULL, new_path);

            if (inode != NULL) {
                
                if (inode->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
                    printf("Listing %s\n", new_path);
                    ntfs_readdir(inode, &position, state, (ntfs_filldir_t)ntfsrec_ls_directory_visitor);
                } else {
                    printf("Error: listing of individual files (%s) is currently unsupported\n", new_path);
                }
                
                ntfs_inode_close(inode);
            } else {
                printf("Error: unable to find %s\n", new_path);
            }
            
        } else {
            printf("Error: specified path %s is longer than the maximum allowed.\n", arguments);
        }
    }
}

static int ntfsrec_ls_directory_visitor(struct ntfsrec_command_processor *state, const ntfschar *name, const int name_len, const int name_type, const s64 pos, const MFT_REF mref, const unsigned dt_type) {
    char createtime_text[32], modtime_text[32];
    struct ntfsrec_file_meta meta;
    char *converted_name = NULL;
    struct tm *local;
    
    NR_UNUSED(pos);
    NR_UNUSED(name_type);
    NR_UNUSED(mref);
    
    if ((name_type & FILE_NAME_WIN32_AND_DOS) == FILE_NAME_DOS) {
        return 0;
    }
    
    if (ntfs_ucstombs(name, name_len, &converted_name, MAX_PATH_LENGTH) < 0) {
        puts("Error: this filename can't be represented in your locale.");
        return 0;
    }
    
    memset(&meta, 0, sizeof meta);
    
    ntfsrec_reader_get_file_meta(state->reader, mref, dt_type == NTFS_DT_DIR, &meta);
    
    local = localtime(&meta.modified.tv_sec);
    strftime(modtime_text, sizeof modtime_text, "%D %R", local);
    
    local = localtime(&meta.created.tv_sec);
    strftime(createtime_text, sizeof createtime_text, "%D %R", local);
    
    if (dt_type == NTFS_DT_DIR) {
        printf("%s\t%s\t--\t%s/\n", createtime_text, modtime_text, converted_name);
    } else {
        char size_text[8];
        
        ntfsrec_utility_format_size(size_text, sizeof size_text, meta.size);
        
        printf("%s\t%s\t%s\t%s/\n", createtime_text, modtime_text, size_text, converted_name);
    }
    
    free(converted_name);
    
    return 0;
}