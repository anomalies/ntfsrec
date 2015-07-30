/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#ifndef _NTFS_COMMAND_H
#define _NTFS_COMMAND_H

#define MAX_PATH_LENGTH 1024
#define MAX_LINE_LENGTH 512
struct ntfsrec_command_processor {
    struct ntfsrec_reader *reader;
    
    ntfs_inode *cwd_inode;
    
    unsigned int running;
    char cwd[MAX_PATH_LENGTH];
};

void ntfsrec_process_commands(struct ntfsrec_reader *reader);

#endif