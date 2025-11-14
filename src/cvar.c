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

/*
 * When using the CVAR_REGISTER preprocessor macro then every cvar will also have a pointer stored
 * in the cvars linker section (see cvar.h). This linker section will have __start_x and __stop_x
 * variables generated (by gcc), which we use here to iterate through the list of pointers.
 *
 * In Cvar_Init() those pointers will be sorted according to their name hash values.
 * When doing cvar lookups we hash the cvar name and do a binary search on the pointer array
 * (comparing name hashes) to find the matching one a quite a bit faster than looping through the
 * entire array.
 */
extern cvar_t *__start_pq_cvars;
extern cvar_t *__stop_pq_cvars;
#define PQ_CVARS_SIZE (&__stop_pq_cvars - &__start_pq_cvars)

static int Cvar_Comparator(void const *a, void const *b)
{
    auto const **aa = (cvar_t const **)a;
    auto const **bb = (cvar_t const **)b;
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

static void Cvar_InitVariable(cvar_t *variable)
{
#if 0
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
#endif

    if (variable->initial_value) {
        // copy the value off, because future sets will Z_Free it
        variable->string = static_cast<char *>(Z_Malloc(Q_strlen(variable->initial_value) + 1));
        Q_strcpy(variable->string, variable->initial_value);
    }
}

void Cvar_Init()
{
    qsort(&__start_pq_cvars, PQ_CVARS_SIZE, sizeof(void *), Cvar_Comparator);

    for (cvar_t **iter = &__start_pq_cvars; iter < &__stop_pq_cvars; ++iter) {
        Cvar_InitVariable(*iter);
    }

    // TODO: We could use something like a bloom filter here to keep track of which variables
    // have been defined and halt and catch fire when there's two with the same name.
    // Currently we don't check this anywhere and just hope that all's well :-).
    // I don't think it's really worth the effort, given that this is a novelty Quake engine.
}

static cvar_t *Cvar_FindVar(char const *var_name)
{
    uint32_t var_name_hash = pq_hash(var_name, strlen(var_name));
    return Cvar_FindVarHashed(var_name_hash);
}

cvar_t *Cvar_FindVarHashed(uint32_t var_name_hash)
{
    cvar_t const search{ var_name_hash };
    auto const *search_ptr = &search;

    void *result = bsearch(&search_ptr, &__start_pq_cvars, PQ_CVARS_SIZE, sizeof(void *), Cvar_Comparator);

    auto **ret = static_cast<cvar_t **>(result);
    if (ret == nullptr) {
        return nullptr;
    }
    return *ret;
}

float Cvar_VariableValue(char const *var_name)
{
    cvar_t *var;
    var = Cvar_FindVar(var_name);
    if (var == nullptr) {
        return 0;
    }
    // Why did this previously return Q_atof(var->string) when var->value seems to be kept up to date..?
    return var->value;
}

#ifdef CONSOLE_COMPLETION
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

static void Cvar_SetVar(cvar_t *var, char const *value)
{
    qboolean changed = true;

    if (var->string) {
        changed = Q_strcmp(var->string, value);
        Z_Free(var->string); // free the old value string
    }

    var->string = static_cast<char *>(Z_Malloc(Q_strlen(value) + 1));
    Q_strcpy(var->string, value);
    // NOLINTNEXTLINE(*-err34-c)
    var->value = Q_atof(var->string);

    if (var->is_server() && changed && sv.active) {
#ifdef ENABLE_CVAR_NAMES
        SV_BroadcastPrintf("\"%s\" changed to \"%s\"\n", var->name, var->string);
#else
        SV_BroadcastPrintf("0x%08X changed to \"%s\"\n", var->name_hash, var->string);
#endif
    }
}

void Cvar_Set(char const *var_name, char const *value)
{
    cvar_t *var = Cvar_FindVar(var_name);
    if (var == nullptr) {
        Con_Printf("Cvar_Set: variable %s not found\n", var_name);
        return;
    }
    Cvar_SetVar(var, value);
}

void Cvar_SetValue(char const *var_name, float value)
{
    char val[32];
    snprintf(val, sizeof(val), "%f", value);
    Cvar_Set(var_name, val);
}

/**
 * Handles variable inspection and changing from the console.
 */
qboolean Cvar_Command()
{
    char const *var_name = Cmd_Argv(0);

    // check variables
    cvar_t *v = Cvar_FindVar(var_name);
    if (!v) {
        return false;
    }

    // perform a variable print or set
    if (Cmd_Argc() == 1) {
        Con_Printf("\"%s\" is \"%s\"\n", var_name, v->string);
        return true;
    }

    Cvar_SetVar(v, Cmd_Argv(1));
    return true;
}

/**
 * Writes lines containing "set variable value" for all variables with the archive flag set to true.
 */
void Cvar_WriteVariables(FILE *f)
{
#ifdef ENABLE_CVAR_NAMES
    for (cvar_t **iter = &__start_pq_cvars; iter < &__stop_pq_cvars; ++iter) {
        if ((*iter)->is_archive()) {
            fprintf(f, "%s \"%s\"\n", (*iter)->name, (*iter)->string);
        }
    }
#else
    Con_Printf("PSXQuake is compiled without support for CVAR names, not saving variables to config.cfg");
#endif
}
