/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#ifndef _NTFS_REC_H
#define _NTFS_REC_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>

#define NR_UNUSED(arg) (void)arg

#define NR_FALSE 0
#define NR_TRUE 1

#define __LITTLE_ENDIAN 1
#define __BYTE_ORDER __LITTLE_ENDIAN

#include <ntfs-3g/types.h>
#include <ntfs-3g/attrib.h>
#include <ntfs-3g/volume.h>
#include <ntfs-3g/inode.h>
#include <ntfs-3g/dir.h>

struct ntfsrec_settings {
    unsigned int verbose;
    FILE *log;
};

#endif