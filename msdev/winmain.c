/*
 * winmain.c -- A wrapper WinMain->main, with command line parser
 *
 * by Petr Kadlec <mormegil@centrum.cz>, 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "config.h"
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

extern int main(int argc, char *argv[]);

static char **CommandLineToArgv(const char *cmdline, int *pargc)
{
    char state = 0;
    const char *start = NULL;
    const char *p = cmdline;
    int qcnt = 0;
    int argc = 1;
    char **result = malloc(sizeof(*result));
    {
        DWORD len;
        result[0] = malloc(MAX_PATH + 2);
        len = GetModuleFileName(NULL, result[0], MAX_PATH + 1);
        if (!len || len >= MAX_PATH + 1) {
            free(result[0]);
            result[0] = strdup("gnubg.exe");
        } else result[0] = realloc(result[0], len + 1);
    }

    while (*p) {
        switch (state) {
        case 0: /* between arguments */
            if (*p == '"') {
                state = 1;
                start = p;
                qcnt = 0;
            } else if (iswspace(*p)) /* skipping whitespace */;
            else {
                /* start of parameter */
                start = p;
                state = 2;
                qcnt = 0;
            }
            break;

        case 1: /* inside quotes */
            if (*p == '"') {
                state = 2;
                ++qcnt;
            } else {
                /* still searching the end of quoted string */
            }
            break;
            
        case 2: /* inside argument, not inside quotes */
            if (*p == '"') {
                state = 1;
                ++qcnt;
            } else if (iswspace(*p)) {
                char *arg, *r;
                const char *q;
                size_t len = p - start - qcnt;
                state = 0;
                ++argc;
                result = realloc(result, argc * sizeof(*result));
                arg = malloc(len);
                for (q = start, r = arg; q < p; ++q) {
                    if (*q != '"') *r++ = *q;
                }
                *r = '\0';
                result[argc-1] = arg;
            } else {
                /* still searching the end of argument */
            }
            break;

        default:
            assert(0);
        }

        ++p;
    }

    switch (state) {
    case 0:
        /* OK */
        break;

    case 1:
    case 2:
        /* store the last argument */
        {
            char *arg, *r;
            const char *q;
            size_t len = p - start - qcnt;
            state = 0;
            ++argc;
            result = realloc(result, argc * sizeof(*result));
            arg = malloc(len);
            for (q = start, r = arg; q < p; ++q) {
                if (*q != '"') *r++ = *q;
            }
            *r = '\0';
            result[argc-1] = arg;
        }
        break;

    default:
        assert(0);
    }

    result = realloc(result, (argc + 1) * sizeof(*result));
    result[argc] = strdup("");

    *pargc = argc;
    return result;
}

extern int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                          LPSTR lpCmdLine, int nCmdShow)
{
    char **argv;
    int argc;
    argv = CommandLineToArgv(lpCmdLine, &argc);
    return main(argc, argv);
}
