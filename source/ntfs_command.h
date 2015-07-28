/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#ifndef _NTFS_COMMAND_H
#define _NTFS_COMMAND_H

void ntfsrec_process_commands(struct ntfsrec_reader *reader);

#endif