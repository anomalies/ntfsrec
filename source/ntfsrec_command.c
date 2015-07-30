/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfsrec_command.h"
#include "ntfsrec_utility.h"

static int ntfsrec_split_string_destroy(char *string, char **next, char delimiter);
static void ntfsrec_remove_newline(char *string);

static void ntfsrec_dispatch_command(struct ntfsrec_command_processor *state, char *command, char *arguments);
extern void ntfsrec_command_ls(struct ntfsrec_command_processor *state, char *arguments);
extern void ntfsrec_command_cd(struct ntfsrec_command_processor *state, char *arguments);
extern void ntfsrec_command_cp(struct ntfsrec_command_processor *state, char *arguments);
extern void ntfsrec_command_cpz(struct ntfsrec_command_processor *state, char *arguments);
static void ntfsrec_command_info(struct ntfsrec_command_processor *state, char *arguments);
static void ntfsrec_command_quit(struct ntfsrec_command_processor *state, char *arguments);


static const struct ntfsrec_command_handler {
    const char *name;
    const char *help;
    void (*handler)(struct ntfsrec_command_processor *state, char *arguments);
} command_handlers[] = {
    { "ls",   "Lists files and folders in a directory",    &ntfsrec_command_ls   },
    { "cd",   "Changes the current directory to <folder>", &ntfsrec_command_cd   },
    { "cp",   "Copies files from cwd to <destination>",    &ntfsrec_command_cp   },
    { "cpz",  "Copies files from cwd to <dest> zip file",  &ntfsrec_command_cpz  },
    { "info", "Displays information about the volume",     &ntfsrec_command_info },
    { "quit", "Exits the application.",                    &ntfsrec_command_quit },
    { NULL,   NULL,                                        NULL                  }
};

void ntfsrec_process_commands(struct ntfsrec_reader *reader) {
    struct ntfsrec_command_processor state;
    char line[MAX_LINE_LENGTH];
    
    memset(&state, 0, sizeof state);
    memset(line, 0, sizeof line);
    
    state.reader = reader;
    state.running = 1;
    
    ntfsrec_command_cd(&state, "/");
    
    while(state.running) {
        
        char *command = line, *arguments;
        
        printf("%s> ", state.cwd);
        
        if (fgets(line, MAX_LINE_LENGTH, stdin) == NULL)
            break;
        
        ntfsrec_remove_newline(line);
        
        if (ntfsrec_split_string_destroy(command, &arguments, ' ') == NR_FALSE)
            continue;
        
        ntfsrec_dispatch_command(&state, command, arguments);
    }
    
    if (state.cwd_inode != NULL)
        ntfs_inode_close(state.cwd_inode);
}

static int ntfsrec_split_string_destroy(char *string, char **next, char delimiter) {
    if (*string == '\0')
        return NR_FALSE;
    
    for(; *string != '\0'; ++string) {
        if (*string == delimiter) {
            *string = '\0';
            *next = string + 1;
            
            return NR_TRUE;
        }
    }
    
    *next = string;
    return NR_TRUE;
}

static void ntfsrec_remove_newline(char *string) {
    for(; *string != '\0'; ++string) {
        if (*string == '\n')
            *string = '\0';
    }
}



static void ntfsrec_dispatch_command(struct ntfsrec_command_processor *state, char *command, char *arguments) {
    const struct ntfsrec_command_handler *handler;
    
    for(handler = command_handlers; handler->name != NULL; ++handler) {
        if (strcmp(handler->name, command) == 0) {
            handler->handler(state, arguments);
            
            return;
        }
    }
    
    printf("Unrecognised command: %s\n", command);
}

static void ntfsrec_command_info(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(state);
    NR_UNUSED(arguments);
}

static void ntfsrec_command_quit(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(arguments);
    
    state->running = 0;
}