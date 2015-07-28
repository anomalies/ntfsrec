/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfs_command.h"
#include <locale.h>

int main(int argc, char **argv) {
    struct ntfsrec_settings settings;
    struct ntfsrec_reader reader;

    if (argc != 2) {
        printf("Usage: ntfsrec <device path>\n");
        return 1;
    }
    
    memset(&settings, 0, sizeof settings);
    memset(&reader, 0, sizeof reader);
    
    setlocale(LC_ALL, "");
    
    settings.log = stdout;
    settings.verbose = 1;
    
    reader.settings = &settings;
    
    if (ntfsrec_reader_mount(&reader, argv[1], 0) == NR_FALSE)
        return NR_FALSE;
    
    if (settings.verbose)
        printf("Opened NTFS volume %s\n", argv[1]);

    ntfsrec_process_commands(&reader);
    
    ntfsrec_reader_release(&reader);
        
    return 0;
}