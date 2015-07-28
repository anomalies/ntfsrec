/*
 * ntfsrec - Recovery utility for damaged NTFS filesystems
 * Andrew Watts - 2015 <andrew@andrewwatts.info>
 */

#include "ntfsrec.h"
#include "ntfs_reader.h"
#include "ntfs_command.h"

#define MAX_PATH_LENGTH 1024
#define MAX_LINE_LENGTH 512
struct ntfsrec_command_processor {
    struct ntfsrec_reader *reader;
    
    ntfs_inode *cwd_inode;
    
    unsigned int running;
    char cwd[MAX_PATH_LENGTH];
};

static int ntfsrec_split_string_destroy(char *string, char **next, char delimiter);
static void ntfsrec_remove_newline(char *string);
static void ntfsrec_check_trailing_slash(char *string);
static int ntfsrec_calculate_path(struct ntfsrec_command_processor *state, const char *path, char *output, size_t max_length);

static void ntfsrec_dispatch_command(struct ntfsrec_command_processor *state, char *command, char *arguments);
static void ntfsrec_command_ls(struct ntfsrec_command_processor *state, char *arguments);
static void ntfsrec_command_cd(struct ntfsrec_command_processor *state, char *arguments);
static void ntfsrec_command_info(struct ntfsrec_command_processor *state, char *arguments);
static void ntfsrec_command_quit(struct ntfsrec_command_processor *state, char *arguments);

static int ntfsrec_directory_visitor(struct ntfsrec_command_processor *state, const ntfschar *name,
                                     const int name_len, const int name_type, const s64 pos,
                                     const MFT_REF mref, const unsigned dt_type);

static const struct ntfsrec_command_handler {
    const char *name;
    const char *help;
    void (*handler)(struct ntfsrec_command_processor *state, char *arguments);
} command_handlers[] = {
    { "ls",   "Lists files and folders in a directory",    &ntfsrec_command_ls   },
    { "cd",   "Changes the current directory to <folder>", &ntfsrec_command_cd   },
    { "info", "Lists info about the current volume",       &ntfsrec_command_info },
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

static void ntfsrec_check_trailing_slash(char *string) {
    size_t length = strlen(string);
    
    if (string[length - 1] != '/') {
        string[length] = '/';
        string[length + 1] = '\0';
    }
}

static int ntfsrec_calculate_path(struct ntfsrec_command_processor *state, const char *path, char *output, size_t max_length) {
    if (*path == '/') {
        /* Absolute */
        return snprintf(output, max_length, "%s", path) < max_length ? NR_TRUE : NR_FALSE;
    } else if (*path == '.') {
        /* Relative */
        
        if (path[1] == '.' && path[2] == '/') {
            return NR_FALSE;
        } else if (path[1] == '/') {
            return NR_FALSE;
        }
    }
    
    return snprintf(output, max_length, "%s%s", state->cwd, path);
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

static void ntfsrec_command_ls(struct ntfsrec_command_processor *state, char *arguments) {
    s64 position = 0;

    if (*arguments == '\0') {
        ntfs_readdir(state->cwd_inode, &position, state, (ntfs_filldir_t)ntfsrec_directory_visitor);
    } else {

    }
    
    
}

static void ntfsrec_command_cd(struct ntfsrec_command_processor *state, char *arguments) {
    char cwd_buffer[MAX_PATH_LENGTH];
    ntfs_inode *inode;
    
    if (*arguments == '\0') {
        puts("Usage: cd <directory>");
        
        return;
    }
    
    ntfsrec_check_trailing_slash(arguments);
    
    if (*arguments == '/') {
        if (snprintf(cwd_buffer, MAX_PATH_LENGTH, "%s", arguments) >= MAX_PATH_LENGTH) {
            puts("Error: path length exceeds the maximum allowable path length.");
            
            return;
        }
    } else {
        if (snprintf(cwd_buffer, MAX_PATH_LENGTH, "%s%s", state->cwd, arguments) >= MAX_PATH_LENGTH) {
            puts("Error: path length exceeds the maximum allowable path length.");
            
            return;
        }
    }
    
    inode = ntfs_pathname_to_inode(state->reader->mount.volume, NULL, cwd_buffer);
    
    if (inode == NULL) {
        printf("Error: can't find path %s\n", cwd_buffer);
        
        return;
    }
    
    if (inode->mrec->flags & MFT_RECORD_IS_DIRECTORY) {
        strncpy(state->cwd, cwd_buffer, MAX_PATH_LENGTH);
    } else {
        printf("Error: %s isn't a directory.\n", cwd_buffer);
        ntfs_inode_close(inode);
        
        return;
    }
    
    if (state->cwd_inode != NULL)
        ntfs_inode_close(state->cwd_inode);
    
    state->cwd_inode = inode;
}

static void ntfsrec_command_info(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(state);
    NR_UNUSED(arguments);
}

static void ntfsrec_command_quit(struct ntfsrec_command_processor *state, char *arguments) {
    NR_UNUSED(arguments);
    
    state->running = 0;
}

static int ntfsrec_directory_visitor(struct ntfsrec_command_processor *state, const ntfschar *name, const int name_len, const int name_type, const s64 pos, const MFT_REF mref, const unsigned dt_type) {
    char *converted_name = NULL;
    NR_UNUSED(pos);
    NR_UNUSED(name_type);
    NR_UNUSED(mref);
    
    if ((name_type & FILE_NAME_WIN32_AND_DOS) == FILE_NAME_DOS) {
        return 0;
    }
    
    if (ntfs_ucstombs(name, name_len, &converted_name, MAX_PATH_LENGTH) < 0) {
        puts("Error: this filename can't be represented in your locale.");
        return 0;
    }
    
    printf("%s", converted_name);
    
    if (dt_type == NTFS_DT_DIR)
        putchar('/');
    
    putchar('\n');
    
    free(converted_name);
    
    return 0;
}