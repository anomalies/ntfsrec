/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include <sys/stat.h>

enum ntfsrec_test_device_result {
    NR_TEST_DEVICE_RESULT_SUCCESS = 0,
    NR_TEST_DEVICE_RESULT_NOT_FOUND,
    NR_TEST_DEVICE_RESULT_IS_MOUNTED
};

static int ntfsrec_reader_test_device(struct ntfsrec_reader *reader, const char *device_name, unsigned int options);
static void ntfsrec_reader_print_mount_error(struct ntfsrec_reader *reader);

int ntfsrec_reader_mount(struct ntfsrec_reader *reader, const char *device_name, unsigned int options) {
    unsigned long mount_flags;
    
    if (ntfsrec_reader_test_device(reader, device_name, options) == NR_FALSE) {
        return NR_FALSE;
    }
    
    mount_flags = NTFS_MNT_RDONLY | NTFS_MNT_RECOVER | NTFS_MNT_FORENSIC;
    
    if (options & NR_MOUNT_OPTION_EXCLUSIVE)
        mount_flags |= NTFS_MNT_EXCLUSIVE;
    
    reader->mount.volume = ntfs_mount(device_name, NTFS_MNT_RDONLY);
    
    if (reader->mount.volume == NULL) {
        fprintf(reader->settings->log, "Error: unrecoverable fault during mount of %s\n", device_name);
        ntfsrec_reader_print_mount_error(reader);
        
        return NR_FALSE;
    }
    
    reader->mount.name = device_name;
    return NR_TRUE;
}

void ntfsrec_reader_release(struct ntfsrec_reader *reader) {
    if (reader->mount.volume != NULL) {
        ntfs_umount(reader->mount.volume, FALSE);
        reader->mount.volume = NULL;
    }
}

int ntfsrec_reader_get_file_meta(struct ntfsrec_reader *reader, MFT_REF node_ref, int is_dir, struct ntfsrec_file_meta *meta) {
    ntfs_inode *inode;
    int result = NR_FALSE;
    
    inode = ntfs_inode_open(reader->mount.volume, node_ref);
    
    if (inode != NULL) {
        ntfs_attr_search_ctx *search_ctx;
        
        search_ctx = ntfs_attr_get_search_ctx(inode, NULL);
        
        if (search_ctx != NULL) {
            if (ntfs_attr_lookup(AT_FILE_NAME, AT_UNNAMED, 0, 0, 0, NULL, 0, search_ctx) == 0) {
                FILE_NAME_ATTR *filename_attr;
                
                filename_attr = (FILE_NAME_ATTR *)((char *)search_ctx->attr + le16_to_cpu(search_ctx->attr->value_offset));
                
                if (filename_attr != NULL) {
                    meta->created = ntfs2timespec(filename_attr->creation_time);
                    meta->modified = ntfs2timespec(filename_attr->last_data_change_time);
                    meta->flags = filename_attr->file_attributes;
                    
                    result = NR_TRUE;
                }
            }
            
            if (!is_dir && ntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, 0, 0, NULL, 0, search_ctx) == 0) {
                meta->size = ntfs_get_attribute_value_length(search_ctx->attr);
                
                result = NR_TRUE;
            }
            
            ntfs_attr_put_search_ctx(search_ctx);
        }

        ntfs_inode_close(inode);
    }
    
    return result;
}

static int ntfsrec_reader_test_device(struct ntfsrec_reader *reader, const char *device_name, unsigned int options) {
    struct stat stat_result;
    
    memset(&stat_result, 0, sizeof stat_result);
    
    if (stat(device_name, &stat_result) != 0) {        
        if (errno == ENOENT) {
            fprintf(reader->settings->log, "Error: unable to mount %s as that device can't be found.\n", device_name);
        } else if (errno == EACCES) {
            fprintf(reader->settings->log, "Error: unable to mount %s as you lack access.\n", device_name);
        } else {
            fprintf(reader->settings->log, "Error: unable to mount %s as there was an error (%d) calling stat(2) on that path.\n", device_name, errno);
        }
        
        return NR_FALSE;
    }
    
    if ((options & NR_MOUNT_OPTION_IGNORE_PREMOUNT) == 0) {
        long unsigned int flags = 0;
        
        if (ntfs_check_if_mounted(device_name, &flags) != 0) {
            fprintf(reader->settings->log, "Error: can't tell if %s is mounted and the option to ignore mount status is not set.\n", device_name);
            return NR_FALSE;
        }
        
        if (flags & NTFS_MF_MOUNTED) {
            fprintf(reader->settings->log, "Error: %s is already mounted and the option to igonre mount status isn't set.\n", device_name);
            return NR_FALSE;
        }
    }
    
    return NR_TRUE;
}

static void ntfsrec_reader_print_mount_error(struct ntfsrec_reader *reader) {
    switch(errno) {
        case EINVAL:
            fprintf(reader->settings->log, "Reason: this device does not appear to be a valid NTFS volume.\n");
            break;
            
        case EIO:
            fprintf(reader->settings->log, "Reason: there was an IO error while mounting the volume.\n");
            break;
            
        case EBUSY:
            fprintf(reader->settings->log, "Reason: this device is currently reporting as busy.\n");
            break;
    }
}