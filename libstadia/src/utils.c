/*
 * utils.c -- Misc utility routines.
 */

#include <ctype.h>

#include "utils.h"

PWCHAR wcsistr(PWCHAR haystack, const PWCHAR needle)
{
    do
    {
        PWCHAR h = haystack;
        PWCHAR n = needle;
        while (tolower((WCHAR)*h) == tolower((WCHAR)*n) && *n)
        {
            h++;
            n++;
        }
        if (*n == 0)
        {
            return (PWCHAR)haystack;
        }
    } while (*haystack++);
    return NULL;
}
