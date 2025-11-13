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
// cvar.c -- dynamic variable tracking

#include "quakedef.h"

static cvar_t *cvar_vars;

/*
============
Cvar_FindVar
============
*/
static cvar_t *Cvar_FindVar(char const *var_name)
{
    cvar_t *var;
    uint32_t var_name_hash = pq_hash(var_name, strlen(var_name));
    for (var = cvar_vars; var; var = var->next)
        if (var->name_hash == var_name_hash)
            return var;

    return NULL;
}

cvar_t *Cvar_FindVarHashed(uint32_t var_name_hash)
{
    cvar_t *var;
    for (var = cvar_vars; var; var = var->next)
        if (var->name_hash == var_name_hash)
            return var;

    return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue(char const *var_name)
{
    cvar_t *var;

    var = Cvar_FindVar(var_name);
    if (!var)
        return 0;
    return var->value; // Q_atof(var->string);
}

#ifdef CONSOLE_COMPLETION
/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable(char *partial)
{
    cvar_t *cvar;
    int len;

    len = Q_strlen(partial);

    if (!len)
        return NULL;

    // check functions
    for (cvar = cvar_vars; cvar; cvar = cvar->next)
        if (!Q_strncmp(partial, cvar->name, len))
            return cvar->name;

    return NULL;
}
#endif

/*
============
Cvar_Set
============
*/
static void Cvar_SetVar(cvar_t * var, char const *value)
{
    qboolean changed = true;

    if (var->string) {
        changed = Q_strcmp(var->string, value);
        Z_Free(var->string); // free the old value string
    }

    var->string = Z_Malloc(Q_strlen(value) + 1);
    Q_strcpy(var->string, value);
    var->value = Q_atof(var->string);
    if (var->is_server() && changed) {
        if (sv.active) {
#ifdef ENABLE_CVAR_NAMES
            SV_BroadcastPrintf("\"%s\" changed to \"%s\"\n", var->name, var->string);
#else
            SV_BroadcastPrintf("0x%08X changed to \"%s\"\n", var->name_hash, var->string);
#endif
        }
    }
}

void Cvar_Set(char const *var_name, char const *value)
{
    cvar_t * var = Cvar_FindVar(var_name);
    if (!var) { // there is an error in C code if this happens
        Con_Printf("Cvar_Set: variable %s not found\n", var_name);
        return;
    }

    Cvar_SetVar(var, value);
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue(char const *var_name, float value)
{
    char val[32];

    sprintf(val, "%f", value);
    Cvar_Set(var_name, val);
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable(cvar_t *variable)
{
    char *oldstr;

    // first check to see if it has already been defined
    if (Cvar_FindVarHashed(variable->name_hash)) {
#ifdef ENABLE_CVAR_NAMES
        Con_Printf("Can't register variable %s, already defined\n", variable->name);
#else
        Con_Printf("Can't register variable 0x%08X, already defined\n", variable->name_hash);
#endif
        return;
    }

    // check for overlap with a command
    if (Cmd_ExistsHashed(variable->name_hash)) {
        Con_Printf("Cvar_RegisterVariable: 0x%08X is a command\n", variable->name_hash);
        return;
    }

    if (variable->initial_value) {
        // copy the value off, because future sets will Z_Free it
        variable->string = Z_Malloc(Q_strlen(variable->initial_value) + 1);
        Q_strcpy(variable->string, variable->initial_value);
        variable->value = Q_atof(variable->string);
    }

    // link the variable in
    variable->next = cvar_vars;
    cvar_vars = variable;
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command(void)
{
    cvar_t *v;
    char const * var_name = Cmd_Argv(0);

    // check variables
    v = Cvar_FindVar(var_name);
    if (!v)
        return false;

    // perform a variable print or set
    if (Cmd_Argc() == 1) {
        Con_Printf("\"%s\" is \"%s\"\n", var_name, v->string);
        return true;
    }

    Cvar_SetVar(v, Cmd_Argv(1));
    return true;
}

/*
============
Cvar_WriteVariables

Writes lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables(FILE *f)
{
#ifdef ENABLE_CVAR_NAMES
    cvar_t *var;

    for (var = cvar_vars; var; var = var->next) {
        if (var->is_archive()) {
            fprintf(f, "%s \"%s\"\n", var->name, var->string);
        }
    }
#else
    Con_Printf("PSXQuake is compiled without support for CVAR names, not saving variables to config.cfg");
#endif
}
