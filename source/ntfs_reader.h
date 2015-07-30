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

struct ntfsrec_file_meta {
    int64_t size;
    FILE_ATTR_FLAGS flags;
    struct timespec modified;
    struct timespec created;
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

int ntfsrec_reader_get_file_meta(struct ntfsrec_reader *reader, MFT_REF node_ref, int is_dir, struct ntfsrec_file_meta *meta);

#endif