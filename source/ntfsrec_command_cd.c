/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfsrec_command.h"
#include "ntfsrec_utility.h"
#include <zip.h>

static void ntfsrec_check_trailing_slash(char *string);

void ntfsrec_command_cd(struct ntfsrec_command_processor *state, char *arguments) {
    char cwd_buffer[MAX_PATH_LENGTH];
    ntfs_inode *inode;
    
    if (*arguments == '\0') {
        puts("Usage: cd <directory>");
        
        return;
    }
    
    ntfsrec_check_trailing_slash(arguments);
    
    if (ntfsrec_calculate_path(cwd_buffer, MAX_PATH_LENGTH, state->cwd, arguments) == NR_FALSE) {
        printf("Error: invalid path arugment %s\n", arguments);
        return;
    }
    
    inode = ntfs_pathname_to_inode(state->reader->mount.volume, NULL, cwd_buffer);
    
    if (inode == NULL) {
        printf("Error: can't find path %s\n", cwd_buffer);
        
        return;
    }
    
    if (inode->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
        strncpy(state->cwd, cwd_buffer, MAX_PATH_LENGTH);
    } else {
        printf("Error: %s isn't a directory.\n", cwd_buffer);
        ntfs_inode_close(inode);
        
        return;
    }
    
    if (state->cwd_inode != NULL)
        ntfs_inode_close(state->cwd_inode);
    
    state->cwd_inode = inode;
}

static void ntfsrec_check_trailing_slash(char *string) {
    size_t length = strlen(string);
    
    if (string[length - 1] != '/') {
        string[length] = '/';
        string[length + 1] = '\0';
    }
}
