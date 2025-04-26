/***************************************************************************
 *                           elfcfg.c
 *                       -------------------
 *  begin                : Mon Apr 12 2025
 *  copyright            : (C) 2025 by fm
 *  email                : frank.meyer@cablelink.at
 *
 *      POSIX reimplementation of a microcontroller-based ELF watch tool;
 *      this source file implements the configuration file routines;
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "gmt.h"

/* --- prototypes ----
 */
void  strupr (char *str);


/* --------------------------------
 * ------------  code  ------------
 */

/* open the named (config) file,
 * and return a FILE pointer;
 * returns NULL upon failure;
 */
FILE  *openCfgfile (char *name)
{
    return (fopen (name, "r"));
}



/* look for the given key in a file,
 * and return a pointer to the value specified after "=';
 * if none was found, NULL is returned
 */
int  getstrcfgitem (FILE *pf, char *pItem, char *ptarget)
{
    char   lbuf[256];
    char  *p, *pr;
    int    done, rv;

    if (!pf)
        return 0;

    rewind (pf);
    done = 0;
    rv   = 0;
    pr   = NULL;
    do
    {
        lbuf[0] = '\0';
        if ((fgets (lbuf, 255, pf)) == NULL)
            done = 1;
        else
        {
            strupr (lbuf);
            if (lbuf[0] == '#')                      /* comment line ?  */
                continue;
            if ((p = strchr (lbuf, '\n')))           /* terminate at \n */
                *p = '\0';
            if ((p = strstr (lbuf, pItem)) == NULL)  /* find the key    */
                continue;
            p += strlen (pItem);                     /* skip behind key */
            if (!(pr = strchr(p, '=')))              /* must have a '=' */
                continue;
            p = pr;                                  /* skip past it !  */
            p++;
            while ((*p == ' ') || (*p == '\t'))      /* skip all spaces */
                p++;
            if ((*p != '\0') && (*p != '\n'))
            {
                strcpy (ptarget, p);
                done = 1;
                rv   = 1;
            }
        }
    }
    while (!done);
    return (rv);
}



/* read an integer configuration item;
 * if the item found, it is stored in the target pointer location,
 * and '1' is returned;
 * if the item was not found, the target pointer is not modified,
 * and 0 is returned;
 */
int  getintcfgitem (FILE *pf, char *pItem, int *pvalue)
{
    char   lbuf[256];
    char  *p, *pe;
    int    done, rv, i;

    if (!pf)
        return 0;

    rewind (pf);
    done = 0;
    rv   = 0;
    do
    {
        lbuf[0] = '\0';
        if ((fgets (lbuf, 255, pf)) == NULL)
            done = 1;
        else
        {
            strupr (lbuf);
            if (lbuf[0] == '#')                      /* comment line ?  */
                continue;
            if ((p = strchr (lbuf, '\n')))           /* terminate at \n */
                *p = '\0';
            if ((p = strstr (lbuf, pItem)) == NULL)  /* find the key    */
                continue;
            p += strlen (pItem);                     /* skip behind key */
            if (!(pe = strchr(p, '=')))              /* must have a '=' */
                continue;
            p = pe;                                  /* skip past it !  */
            p++;
            while ((*p == ' ') || (*p == '\t'))      /* skip spaces     */
                p++;
            if ((*p != '\0') && (*p != '\n'))
            {
                i = (int) strtol (p, &pe, 10);       /* check for some valid number */
                if ((i != 0) && (pe != p))
                {
                    *pvalue = i;
                    rv      = 1;
                }
                done = 1;
            }
        }
    }
    while (!done);
    return (rv);
}



/* helper function to convert a string into upper char
 */
void  strupr (char *str)
{
    int  i;

    if (str == NULL)
        return;

    i = 0;
    while ((str[i] != '\0') && (i < CFG_STR_MAX))
    {
        str[i] = toupper (str[i]);
        i++;
    }
}

