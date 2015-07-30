/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfsrec_command.h"
#include "ntfsrec_utility.h"

void ntfsrec_command_cp(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(state);
    NR_UNUSED(arguments);
}

void ntfsrec_command_cpt(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(state);
    NR_UNUSED(arguments);
}
