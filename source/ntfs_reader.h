/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#ifndef _NTFS_REC_READER_H
#define _NTFS_REC_READER_H

enum ntfsrec_mount_option {
    NR_MOUNT_OPTION_IGNORE_PREMOUNT = 1,
    NR_MOUNT_OPTION_EXCLUSIVE = 2
};

struct ntfsrec_reader {
    struct ntfsrec_settings *settings;
    
    struct {
        const char *name;
        ntfs_volume *volume;
    } mount;
};

int ntfsrec_reader_mount(struct ntfsrec_reader *reader, const char *device_name, unsigned int options);

void ntfsrec_reader_release(struct ntfsrec_reader *reader);

#endif