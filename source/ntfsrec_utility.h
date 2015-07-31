/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#ifndef _NTFSREC_UTILITY_H
#define _NTFSREC_UTILITY_H

void *ntfsrec_allocate(size_t length);
void ntfsrec_utility_format_size(char *buffer, size_t maxsize, int64_t size_value);
int ntfsrec_calculate_path(char *output, size_t max_length, const char *base, const char *path);

#endif