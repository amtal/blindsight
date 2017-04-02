#ifndef CMD_H
#define CMD_H 1

typedef struct {
        const char* keys; // optional, no keys = treated as section header
        const char* desc; // mandatory until last entry
} cmd;

void cmd_render_help(cmd cs[]);
void cmd_print_help(cmd cs[]);

#endif

