/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cmd.c -- Quake script command processing module

#include "quakedef.h"

typedef struct cmdalias_s {
    struct cmdalias_s *next;
    uint32_t name_hash;
#ifdef DEBUG
    char name_debug[32];
#endif
    char *value;
} cmdalias_t;

/* Since aliases are allocated at runtime then we can't do the linker section trickery with those */
static cmdalias_t *cmd_alias;

static qboolean cmd_wait;

/*
 * See explanation in crc.c
 */
extern cvar_t *__start_pq_cmds;
extern cvar_t *__stop_pq_cmds;
#define PQ_CMDS_SIZE (&__stop_pq_cmds - &__start_pq_cmds)

static int Cmd_Comparator(void const *a, void const *b)
{
    auto const **aa = (cmd_function_t const **)a;
    auto const **bb = (cmd_function_t const **)b;
    auto const &a_val = (*aa)->name_hash;
    auto const &b_val = (*bb)->name_hash;
    if (a_val > b_val) {
        return 1;
    }
    if (a_val < b_val) {
        return -1;
    }
    return 0;
}

/**
 * Causes execution of the remainder of the command buffer to be delayed until
 * next frame.  This allows commands like:
 * bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
 */
void Cmd_Wait_f()
{
    cmd_wait = true;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

static sizebuf_t cmd_text;

void Cbuf_Init()
{
    SZ_Alloc(&cmd_text, 8192); // space for commands and script files
}

/**
 * Adds command text at the end of the buffer
 */
void Cbuf_AddText(char const *text)
{
    int l;

    l = Q_strlen(text);

    if (cmd_text.cursize + l >= cmd_text.maxsize) {
        Con_Printf("Cbuf_AddText: overflow\n");
        return;
    }

    SZ_Write(&cmd_text, text, Q_strlen(text));
}

/**
 * Adds command text immediately after the current command
 * Adds a \n to the text
 * FIXME: actually change the command buffer to do less copying
 */
void Cbuf_InsertText(char const *text)
{
    char *temp = nullptr;

    // copy off any commands still remaining in the exec buffer
    int templen = cmd_text.cursize;
    if (templen) {
        temp = Z_Malloc(templen);
        Q_memcpy(temp, cmd_text.data, templen);
        SZ_Clear(&cmd_text);
    }

    // add the entire text of the file
    Cbuf_AddText(text);

    // add the copied off data
    if (templen) {
        SZ_Write(&cmd_text, temp, templen);
        Z_Free(temp);
    }
}

void Cbuf_Execute()
{
    char line[1024];

    while (cmd_text.cursize) {
        // find a \n or ; line break
        char * text = (char *)cmd_text.data;
        int i;
        int quotes = 0;

        for (i = 0; i < cmd_text.cursize; i++) {
            if (text[i] == '"') {
                quotes++;
            }
            if (!(quotes & 1) && text[i] == ';') {
                break; // don't break if inside a quoted string
            }
            if (text[i] == '\n') {
                break;
            }
        }

        memcpy(line, text, i);
        line[i] = 0;

        // delete the text from the command buffer and move remaining commands down
        // this is necessary because commands (exec, alias) can insert data at the
        // beginning of the text buffer

        if (i == cmd_text.cursize) {
            cmd_text.cursize = 0;
        } else {
            i++;
            cmd_text.cursize -= i;
            Q_memcpy(text, text + i, cmd_text.cursize);
        }

        // execute the command line
        Cmd_ExecuteString(line, src_command);

        // skip out while text still remains in buffer, leaving it for next frame
        if (cmd_wait) {
            cmd_wait = false;
            break;
        }
    }
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/**
 * Adds command line parameters as script statements
 * Commands lead with a +, and continue until a - or another +
 * quake +prog jctest.qp +cmd amlev1
 * quake -nosound +cmd amlev1
 */
void Cmd_StuffCmds_f()
{
    int i, j;
    int s;
    char *text, *build, c;

    if (Cmd_Argc() != 1) {
        Con_Printf("stuffcmds : execute command line parameters\n");
        return;
    }

    // build the combined string to parse from
    s = 0;
    for (i = 1; i < com_argc; i++) {
        if (!com_argv[i]) {
            continue; // NEXTSTEP nulls out -NXHost
        }
        s += Q_strlen(com_argv[i]) + 1;
    }
    if (!s) {
        return;
    }

    text = Z_Malloc(s + 1);
    text[0] = 0;
    for (i = 1; i < com_argc; i++) {
        if (!com_argv[i]) {
            continue; // NEXTSTEP nulls out -NXHost
        }
        Q_strcat(text, com_argv[i]);
        if (i != com_argc - 1) {
            Q_strcat(text, " ");
        }
    }

    // pull out the commands
    build = Z_Malloc(s + 1);
    build[0] = 0;

    for (i = 0; i < s - 1; i++) {
        if (text[i] == '+') {
            i++;

            for (j = i; (text[j] != '+') && (text[j] != '-') && (text[j] != 0); j++)
                ;

            c = text[j];
            text[j] = 0;

            Q_strcat(build, text + i);
            Q_strcat(build, "\n");
            text[j] = c;
            i = j - 1;
        }
    }

    if (build[0])
        Cbuf_InsertText(build);

    Z_Free(text);
    Z_Free(build);
}

void Cmd_Exec_f()
{
    char *f;
    int mark;

    if (Cmd_Argc() != 2) {
        Con_Printf("exec <filename> : execute a script file\n");
        return;
    }

    mark = Hunk_LowMark();
    f = (char *)COM_LoadHunkFile(Cmd_Argv(1));
    if (!f) {
        Con_Printf("couldn't exec %s\n", Cmd_Argv(1));
        return;
    }
    Con_Printf("execing %s\n", Cmd_Argv(1));

    Cbuf_InsertText(f);
    Hunk_FreeToLowMark(mark);
}

/**
 * Just prints the rest of the line to the console
 */
void Cmd_Echo_f()
{
    for (int i = 1; i < Cmd_Argc(); i++) {
        Con_Printf("%s ", Cmd_Argv(i));
    }
    Con_Printf("\n");
}

/**
 * Creates a new command that executes a command string (possibly ; seperated)
 */
char *CopyString(char const *in)
{
    char * out = Z_Malloc(strlen(in) + 1);
    strcpy(out, in);
    return out;
}

static cmdalias_t * Cmd_AliasFind(uint32_t name_hash)
{
    for (cmdalias_t * a = cmd_alias; a; a = a->next) {
        if (a->name_hash == name_hash) {
            return a;
        }
    }
    return nullptr;
}

void Cmd_Alias_f()
{
    cmdalias_t *a;
    char cmd[1024];
    int i, c;

    if (Cmd_Argc() == 1) {
        Con_Printf("Current alias commands:\n");
        for (a = cmd_alias; a; a = a->next)
#ifdef DEBUG
            Con_Printf("%s (%x): %s\n", a->name_debug, a->name_hash, a->value);
#else
            Con_Printf("%x: %s\n", a->name_hash, a->value);
#endif
        return;
    }

    char const * s = Cmd_Argv(1);
    uint32_t alias_name_hash = pq_hash(s, strlen(s));

    // if the alias already exists, reuse it
    a = Cmd_AliasFind(alias_name_hash);
    if (a != nullptr) {
        Z_Free(a->value);
    }

    if (!a) {
        a = Z_Malloc(sizeof(cmdalias_t));
        a->next = cmd_alias;
        cmd_alias = a;
    }
#ifdef DEBUG
    if (strlen(s) >= sizeof(a->name_debug)) {
        Sys_Error("Cmd_Alias_f: name too long\n");
    }
    strcpy(a->name_debug, s);
#endif
    a->name_hash = alias_name_hash;

    // copy the rest of the command line
    cmd[0] = 0; // start out with a null string
    c = Cmd_Argc();
    for (i = 2; i < c; i++) {
        strcat(cmd, Cmd_Argv(i));
        if (i != c) {
            strcat(cmd, " ");
        }
    }
    strcat(cmd, "\n");

    a->value = CopyString(cmd);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

#define MAX_ARGS 80

static int cmd_argc;
static char *cmd_argv[MAX_ARGS];
static char const *cmd_null_string = "";
static char *cmd_args = NULL;

cmd_source_t cmd_source;

static cmd_function_t * Cmd_FunctionFind(uint32_t name_hash)
{
    cmd_function_t const search{ name_hash };
    auto const *search_ptr = &search;

    void *result = bsearch(&search_ptr, &__start_pq_cmds, PQ_CMDS_SIZE, sizeof(void *), Cmd_Comparator);

    auto **ret = static_cast<cmd_function_t **>(result);
    if (ret == nullptr) {
        return nullptr;
    }
    return *ret;
}

void Cmd_Init()
{
    qsort(&__start_pq_cmds, PQ_CMDS_SIZE, sizeof(void *), Cmd_Comparator);
}

int Cmd_Argc()
{
    return cmd_argc;
}

char *Cmd_Argv(int arg)
{
    if ((unsigned) arg >= cmd_argc) {
        return cmd_null_string;
    }
    return cmd_argv[arg];
}

char *Cmd_Args()
{
    return cmd_args;
}

/**
 * Parses the given string into command line tokens.
 */
void Cmd_TokenizeString(char const *text)
{
    unsigned i;

    // clear the args from the last string
    for (i = 0; i < cmd_argc; i++) {
        Z_Free(cmd_argv[i]);
    }

    cmd_argc = 0;
    cmd_args = NULL;

    while (1) {
        // skip whitespace up to a /n
        while (*text && *text <= ' ' && *text != '\n') {
            text++;
        }

        if (*text == '\n') { // a newline seperates commands in the buffer
            text++;
            break;
        }

        if (!*text) {
            return;
        }

        if (cmd_argc == 1) {
            cmd_args = text;
        }

        text = COM_Parse(text);
        if (!text)
            return;

        if (cmd_argc < MAX_ARGS) {
            cmd_argv[cmd_argc] = Z_Malloc(Q_strlen(com_token) + 1);
            Q_strcpy(cmd_argv[cmd_argc], com_token);
            cmd_argc++;
        }
    }
}

qboolean Cmd_Exists(char *cmd_name)
{
    uint32_t cmd_name_hash = pq_hash(cmd_name, strlen(cmd_name));
    return Cmd_FunctionFind(cmd_name_hash) != nullptr;
}

#ifdef CMD_FUNCTION_HAS_NAME
char const *Cmd_CompleteCommand(char *partial)
{
    cmd_function_t *cmd;
    int len;

    len = Q_strlen(partial);

    if (!len)
        return NULL;

    // check functions
    for (cmd = cmd_functions; cmd; cmd = cmd->next)
        if (!Q_strncmp(partial, cmd->name_debug, len))
            return cmd->name_debug;

    return NULL;
}
#endif

/**
 * A complete command line has been parsed, so try to execute it
 * FIXME: lookupnoadd the token to speed search?
 */
void Cmd_ExecuteString(char const *text, cmd_source_t src)
{
    cmd_source = src;
    Cmd_TokenizeString(text);

    // execute the command line
    if (!Cmd_Argc()) {
        return; // no tokens
    }

    uint32_t cmd_name_hash = pq_hash(cmd_argv[0], strlen(cmd_argv[0]));

    // check functions
    if (cmd_function_t const *cmd = Cmd_FunctionFind(cmd_name_hash)) {
        cmd->function();
        return;
    }

    // check alias
    if (cmdalias_t const *a = Cmd_AliasFind(cmd_name_hash)) {
        Cbuf_InsertText(a->value);
        return;
    }

    // check cvars
    if (!Cvar_Command()) {
        Con_Printf("Unknown command \"%s\"\n", Cmd_Argv(0));
    }
}

/**
 * Sends the entire command line over to the server
 */
void Cmd_ForwardToServer()
{
    if (cls.state != ca_connected) {
        Con_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
        return;
    }

    if (cls.demoplayback) {
        return; // not really connected
    }

    MSG_WriteByte(&cls.message, clc_stringcmd);
    if (Q_strcasecmp(Cmd_Argv(0), "cmd") != 0) {
        SZ_Print(&cls.message, Cmd_Argv(0));
        SZ_Print(&cls.message, " ");
    }
    if (Cmd_Argc() > 1) {
        SZ_Print(&cls.message, Cmd_Args());
    } else {
        SZ_Print(&cls.message, "\n");
    }
}

/**
 * Returns the position (1 to argc-1) in the command's argument list
 * where the given parameter appears, or 0 if not present
 */
int Cmd_CheckParm(char *parm)
{
    if (parm == nullptr) {
        Sys_Error("Cmd_CheckParm: NULL");
    }

    for (int i = 1; i < Cmd_Argc(); i++) {
        if (!Q_strcasecmp(parm, Cmd_Argv(i))) {
            return i;
        }
    }

    return 0;
}

CMD_REGISTER("stuffcmds", Cmd_StuffCmds_f);
CMD_REGISTER("exec", Cmd_Exec_f);
CMD_REGISTER("echo", Cmd_Echo_f);
CMD_REGISTER("alias", Cmd_Alias_f);
CMD_REGISTER("cmd", Cmd_ForwardToServer);
CMD_REGISTER("wait", Cmd_Wait_f);
