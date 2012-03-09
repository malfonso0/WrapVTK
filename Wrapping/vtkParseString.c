/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkParseString.c

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkParseString.h"
#include <stdlib.h>
#include <string.h>

/*----------------------------------------------------------------
 * String utility methods
 *
 * Strings are centrally allocated and are const.  They should not
 * be freed until the parse is complete and all the data structures
 * generated by the parse have been freed.
 */

unsigned long numberOfChunks = 0;
char **stringArray = NULL;

/* allocate a string of n+1 bytes */
char *vtkParse_NewString(size_t n)
{
  static size_t stringChunkPos = 0;
  size_t chunk_size = 8176;
  size_t nextChunkPos;
  char *cp;

  // align next start position on an 8-byte boundary
  nextChunkPos = (((stringChunkPos + n + 8) | 7 ) - 7);

  if (numberOfChunks == 0 || nextChunkPos > chunk_size)
    {
    if (n + 1 > chunk_size)
      {
      chunk_size = n + 1;
      }
    cp = (char *)malloc(chunk_size);

    /* if empty, alloc for the first time */
    if (numberOfChunks == 0)
      {
      stringArray = (char **)malloc(sizeof(char *));
      }
    /* if count is power of two, reallocate with double size */
    else if ((numberOfChunks & (numberOfChunks-1)) == 0)
      {
      stringArray = (char **)realloc(
        stringArray, (2*numberOfChunks)*sizeof(char *));
      }

    stringArray[numberOfChunks++] = cp;

    stringChunkPos = 0;
    nextChunkPos = (((n + 8) | 7) - 7);
    }

  cp = &stringArray[numberOfChunks-1][stringChunkPos];
  cp[0] = '\0';

  stringChunkPos = nextChunkPos;

  return cp;
}

/* free all allocated strings */
void vtkParse_FreeStrings()
{
  unsigned long i;

  for (i = 0; i < numberOfChunks; i++)
    {
    free(stringArray[i]);
    }
  if (stringArray)
    {
    free(stringArray);
    }

  stringArray = NULL;
  numberOfChunks = 0;
}

/* duplicate the first n bytes of a string and terminate it */
const char *vtkParse_CopyString(const char *in, size_t n)
{
  char *res = NULL;

  res = vtkParse_NewString(n);
  strncpy(res, in, n);
  res[n] = '\0';

  return res;
}