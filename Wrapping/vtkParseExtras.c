/*=========================================================================

  Program:   WrapVTK
  Module:    vtkParseExtras.c

  Copyright (c) 2011 David Gobbi
  All rights reserved.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.

=========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vtkParse.h"
#include "vtkParseInternal.h"
#include "vtkParseExtras.h"
#include "vtkType.h"

/* Search and replace, return the initial string if no replacements
 * occurred, otherwise return a new string. */
const char *vtkParse_Replace(
  const char *str1, unsigned long n, const char *name[], const char *val[])
{
  const char *cp = str1;
  char result_store[1024];
  size_t resultMaxLen = 1024;
  char *result, *tmp;
  unsigned long k;
  size_t i, j, l, m;
  size_t lastPos, nameBegin, nameEnd;
  int replaced = 0;
  int any_replaced = 0;

  result = result_store;

  if (n == 0)
    {
    return str1;
    }

  i = 0;
  j = 0;
  result[j] = '\0';

  while (cp[i] != '\0')
    {
    lastPos = i;

    /* skip all chars that aren't part of a name */
    while ((cp[i] < 'a' || cp[i] > 'z') &&
           (cp[i] < 'A' || cp[i] > 'Z') &&
           cp[i] != '_' && cp[i] != '\0') { i++; }
    nameBegin = i;

    /* skip all chars that are part of a name */
    while ((cp[i] >= 'a' && cp[i] <= 'z') ||
           (cp[i] >= 'A' && cp[i] <= 'Z') ||
           (cp[i] >= '0' && cp[i] <= '9') ||
           cp[i] == '_') { i++; }
    nameEnd = i;

    /* search through the list of names to replace */
    replaced = 0;
    m = nameEnd - nameBegin;
    for (k = 0; k < n; k++)
      {
      l = strlen(name[k]);
      if (l > 0 && l == m && strncmp(&cp[nameBegin], name[k], l) == 0)
        {
        m = strlen(val[k]);
        replaced = 1;
        any_replaced = 1;
        break;
        }
      }

    /* expand the storage space if needed */
    if (j + m + (nameBegin - lastPos) >= resultMaxLen)
      {
      resultMaxLen *= 2;
      tmp = (char *)malloc(resultMaxLen);
      strcpy(tmp, result);
      if (result != result_store)
         {
         free(result);
         }
       result = tmp;
       }

    /* copy the old bits */
    if (nameBegin > lastPos)
      {
      strncpy(&result[j], &cp[lastPos], nameBegin - lastPos);
      j += (nameBegin - lastPos);
      }

    /* do the replacement */
    if (replaced)
      {
      strncpy(&result[j], val[k], m);
      j += m;
      }
    else if (nameEnd > nameBegin)
      {
      strncpy(&result[j], &cp[nameBegin], nameEnd - nameBegin);
      j += (nameEnd - nameBegin);
      }

    result[j] = '\0';
    }

  if (any_replaced)
    {
    cp = vtkParse_DuplicateString(result, j);
    }

  if (result != result_store)
    {
    free(result);
    }

  return cp;
}

/* Wherever one of the specified names exists inside a Value or inside
 * a Dimension size, replace it with the corresponding val string. */
void vtkParse_ExpandValues(
  ValueInfo *valinfo, unsigned long n, const char *name[], const char *val[])
{
  unsigned long j, m, dim, count;
  const char *cp;

  if (valinfo->Class)
    {
    valinfo->Class = vtkParse_Replace(valinfo->Class, n, name, val);
    }

  if (valinfo->Value)
    {
    valinfo->Value = vtkParse_Replace(valinfo->Value, n, name, val);
    }

  m = valinfo->NumberOfDimensions;
  if (m)
    {
    count = 1;
    for (j = 0; j < m; j++)
      {
      cp = valinfo->Dimensions[j];
      if (cp)
        {
        cp = vtkParse_Replace(cp, n, name, val);
        valinfo->Dimensions[j] = cp;
        while (*cp != '\0' && *cp >= '0' && *cp <= '9') { cp++; }
        while (*cp != '\0' && (*cp == 'u' || *cp == 'l' ||
                               *cp == 'U' || *cp == 'L')) { cp++; }
        dim = 0;
        if (*cp == '\0')
          {
          dim = (int)strtol(valinfo->Dimensions[j], NULL, 0);
          }
        count *= dim;
        }
      }

    /* update count if all values are integer literals */
    if (count)
      {
      valinfo->Count = count;
      }
    }
}

/* Expand a typedef within a type declaration. */
void vtkParse_ExpandTypedef(ValueInfo *valinfo, ValueInfo *typedefinfo)
{
  const char *classname;
  unsigned int baseType;
  unsigned int pointers;
  unsigned int refbit;
  unsigned int qualifiers;
  unsigned int tmp1, tmp2;
  unsigned long i;

  classname = typedefinfo->Class;
  baseType = (typedefinfo->Type & VTK_PARSE_BASE_TYPE);
  pointers = (typedefinfo->Type & VTK_PARSE_POINTER_MASK);
  refbit = (valinfo->Type & VTK_PARSE_REF);
  qualifiers = (typedefinfo->Type & VTK_PARSE_CONST);

  /* handle const */
  if ((valinfo->Type & VTK_PARSE_CONST) != 0)
    {
    if ((pointers & VTK_PARSE_POINTER_LOWMASK) != 0)
      {
      if ((pointers & VTK_PARSE_POINTER_LOWMASK) != VTK_PARSE_ARRAY)
        {
        /* const turns into const pointer */
        pointers = (pointers & ~VTK_PARSE_POINTER_LOWMASK);
        pointers = (pointers | VTK_PARSE_CONST_POINTER);
        }
      }
    else
      {
      /* const remains as const value */
      qualifiers = (qualifiers | VTK_PARSE_CONST);
      }
    }

  /* make a reversed copy of the pointer bitfield */
  tmp1 = (valinfo->Type & VTK_PARSE_POINTER_MASK);
  tmp2 = 0;
  while (tmp1)
    {
    tmp2 = ((tmp2 << 2) | (tmp1 & VTK_PARSE_POINTER_LOWMASK));
    tmp1 = ((tmp1 >> 2) & VTK_PARSE_POINTER_MASK);
    }

  /* turn pointers into zero-element arrays where necessary */
  if ((pointers & VTK_PARSE_POINTER_LOWMASK) == VTK_PARSE_ARRAY)
    {
    tmp2 = ((tmp2 >> 2) & VTK_PARSE_POINTER_MASK);
    while (tmp2)
      {
      vtkParse_AddStringToArray(
        &valinfo->Dimensions, &valinfo->NumberOfDimensions, "");
      tmp2 = ((tmp2 >> 2) & VTK_PARSE_POINTER_MASK);
      }
    }
  else
    {
    /* combine the pointers */
    while (tmp2)
      {
      pointers = ((pointers << 2) | (tmp2 & VTK_PARSE_POINTER_LOWMASK));
      tmp2 = ((tmp2 >> 2) & VTK_PARSE_POINTER_MASK);
      }
    }

  /* combine the arrays */
  for (i = 0; i < typedefinfo->NumberOfDimensions; i++)
    {
    vtkParse_AddStringToArray(
      &valinfo->Dimensions, &valinfo->NumberOfDimensions,
      typedefinfo->Dimensions[i]);
    }
  if (valinfo->NumberOfDimensions > 1)
    {
    pointers = ((pointers & ~VTK_PARSE_POINTER_LOWMASK) | VTK_PARSE_ARRAY);
    }
    
  /* put everything together */
  valinfo->Type = (baseType | pointers | refbit | qualifiers);
  valinfo->Class = classname;
  valinfo->Function = typedefinfo->Function;
  valinfo->Count *= typedefinfo->Count;
}

/* Expand any unrecognized types within a variable, parameter, or typedef
 * that match any of the supplied typedefs. The expansion is done in-place. */
void vtkParse_ExpandTypedefs(
  ValueInfo *val, unsigned long n, ValueInfo *typedefinfo[])
{
  unsigned long i;

  if (((val->Type & VTK_PARSE_BASE_TYPE) == VTK_PARSE_OBJECT ||
       (val->Type & VTK_PARSE_BASE_TYPE) == VTK_PARSE_UNKNOWN) &&
      val->Class != 0)
   {
   for (i = 0; i < n; i++)
     {
     if (typedefinfo[i] && strcmp(val->Class, typedefinfo[i]->Name) == 0)
       {
       vtkParse_ExpandTypedef(val, typedefinfo[i]);
       break;
       }
     }
   }
}

/* skip over a name that might be scoped or templated, return the
 * total number of characters in the name */
size_t vtkParse_NameLength(const char *text)
{
  unsigned int depth = 0;
  size_t i = 0;

  if ((text[i] >= 'A' && text[i] <= 'Z') ||
      (text[i] >= 'a' && text[i] <= 'z') ||
      text[i] == '_' ||
      (text[i] == ':' && text[i+1] == ':'))
    {
    if (text[i] == ':') { i++; }
    i++;
    while ((text[i] >= 'A' && text[i] <= 'Z') ||
           (text[i] >= 'a' && text[i] <= 'z') ||
           (text[i] >= '0' && text[i] <= '9') ||
           text[i] == '_' ||
           (text[i] == ':' && text[i+1] == ':') ||
           text[i] == '<')
      {
      if (text[i] == '<')
        {
        while (text[i] != '\0' && text[i] != '\n')
          {
          if (text[i] == '<') { depth++; }
          if (text[i] == '>') { if (--depth == 0) { i++; break; } }
          i++;
          }
        }
      if (text[i] == ':') { i++; }
      i++;
      }
    }
  return i;
}

/* Helper struct for VTK-specific types */
struct vtk_type_struct
{
  size_t len;
  const char *name;
  int type;
};

/* Get a type from a type name, and return the number of characters used.
 * If the "classname" argument is not NULL, then it is used to return
 * the short name for the type, e.g. "long int" becomes "long", while
 * typedef names and class names are returned unchanged.  If "const"
 * appears in the type name, then the const bit flag is set for the
 * type, but "const" will not appear in the returned classname. */
size_t vtkParse_BasicTypeFromString(
  const char *text, unsigned int *type_ptr, const char **classname_ptr)
{
  /* The various typedefs and types specific to VTK */
  static struct vtk_type_struct vtktypes[] = {
    { 9,  "vtkIdType", VTK_ID_TYPE },
    { 12, "vtkStdString", VTK_STRING },
    { 16, "vtkUnicodeString", VTK_UNICODE_STRING },
    { 11, "vtkTypeInt8", VTK_TYPE_INT8 },
    { 12, "vtkTypeUInt8", VTK_TYPE_UINT8 },
    { 12, "vtkTypeInt16", VTK_TYPE_INT16 },
    { 13, "vtkTypeUInt16", VTK_TYPE_UINT16 },
    { 12, "vtkTypeInt32", VTK_TYPE_INT32 },
    { 13, "vtkTypeUInt32", VTK_TYPE_UINT32 },
    { 12, "vtkTypeInt64", VTK_TYPE_INT64 },
    { 13, "vtkTypeUInt64", VTK_TYPE_UINT64 },
    { 14, "vtkTypeFloat32", VTK_TYPE_FLOAT32 },
    { 14, "vtkTypeFloat64", VTK_TYPE_FLOAT64 },
    { 0, 0, 0 } };

  /* Other typedefs and types */
  static struct vtk_type_struct stdtypes[] = {
    { 6,  "size_t", VTK_PARSE_SIZE_T },
    { 7,  "ssize_t", VTK_PARSE_SSIZE_T },
    { 7,  "ostream", VTK_PARSE_OSTREAM },
    { 7,  "istream", VTK_PARSE_ISTREAM },
    { 8,  "string", VTK_PARSE_STRING },
    { 0, 0, 0 } };

  const char *cp = text;
  const char *tmpcp;
  char *tmp;
  size_t k, n, m;
  unsigned long i;
  unsigned int const_bits = 0;
  unsigned int static_bits = 0;
  unsigned int unsigned_bits = 0;
  unsigned int base_bits = 0;
  const char *classname = NULL;

  while (*cp == ' ' || *cp == '\t') { cp++; }

  while ((*cp >= 'a' && *cp <= 'z') ||
         (*cp >= 'A' && *cp <= 'Z') ||
         (*cp == '_') || (cp[0] == ':' && cp[1] == ':'))
    {
    /* skip all chars that are part of a name */
    n = vtkParse_NameLength(cp);

    if ((n == 6 && strncmp("static", cp, n) == 0) ||
        (n == 4 && strncmp("auto", cp, n) == 0) ||
        (n == 8 && strncmp("register", cp, n) == 0) ||
        (n == 8 && strncmp("volatile", cp, n) == 0))
      {
      if (strncmp("static", cp, n) == 0)
        {
        static_bits = VTK_PARSE_STATIC;
        }
      }
    else if (n == 5 && strncmp(cp, "const", n) == 0)
      {
      const_bits |= VTK_PARSE_CONST;
      }
    else if (n == 8 && strncmp(cp, "unsigned", n) == 0)
      {
      unsigned_bits |= VTK_PARSE_UNSIGNED;
      if (base_bits == 0)
        {
        classname = "int";
        base_bits = VTK_PARSE_INT;
        }
      }
    else if (n == 6 && strncmp(cp, "signed", n) == 0)
      {
      if (base_bits == VTK_PARSE_CHAR)
        {
        classname = "signed char";
        base_bits = VTK_PARSE_SIGNED_CHAR;
        }
      else
        {
        classname = "int";
        base_bits = VTK_PARSE_INT;
        }
      }
    else if (n == 3 && strncmp(cp, "int", n) == 0)
      {
      if (base_bits == 0)
        {
        classname = "int";
        base_bits = VTK_PARSE_INT;
        }
      }
    else if (n == 4 && strncmp(cp, "long", n) == 0)
      {
      if (base_bits == VTK_PARSE_LONG)
        {
        classname = "long long";
        base_bits = VTK_PARSE_LONG_LONG;
        }
      else
        {
        classname = "long";
        base_bits = VTK_PARSE_LONG;
        }
      }
    else if (n == 5 && strncmp(cp, "short", n) == 0)
      {
      classname = "short";
      base_bits = VTK_PARSE_SHORT;
      }
    else if (n == 4 && strncmp(cp, "char", n) == 0)
      {
      if (base_bits == VTK_PARSE_INT && unsigned_bits != VTK_PARSE_UNSIGNED)
        {
        classname = "signed char";
        base_bits = VTK_PARSE_SIGNED_CHAR;
        }
      else
        {
        classname = "char";
        base_bits = VTK_PARSE_CHAR;
        }
      }
    else if (n == 5 && strncmp(cp, "float", n) == 0)
      {
      classname = "float";
      base_bits = VTK_PARSE_FLOAT;
      }
    else if (n == 6 && strncmp(cp, "double", n) == 0)
      {
      classname = "double";
      base_bits = VTK_PARSE_DOUBLE;
      }
    else if (n == 4 && strncmp(cp, "bool", n) == 0)
      {
      classname = "bool";
      base_bits = VTK_PARSE_BOOL;
      }
    else if (n == 4 && strncmp(cp, "void", n) == 0)
      {
      classname = "void";
      base_bits = VTK_PARSE_VOID;
      }
    else if (n == 7 && strncmp(cp, "__int64", n) == 0)
      {
      classname = "__int64";
      base_bits = VTK_PARSE___INT64;
      }
    else
      {
      /* if type already found, break */
      if (base_bits != 0)
        {
        break;
        }

      /* check vtk typedefs */
      if (strncmp(cp, "vtk", 3) == 0)
        {
        for (i = 0; vtktypes[i].len != 0; i++)
          {
          if (n == vtktypes[i].len && strncmp(cp, vtktypes[i].name, n) == 0)
            {
            classname = vtktypes[i].name;
            base_bits = vtkParse_MapType((int)vtktypes[i].type);
            }
          }
        }

      /* check standard typedefs */
      if (base_bits == 0)
        {
        m = 0;
        if (strncmp(cp, "::", 2) == 0) { m = 2; }
        else if (strncmp(cp, "std::", 5) == 0) { m = 5; }
        else if (strncmp(cp, "vtkstd::", 8) == 0) { m = 8; }

        /* advance past the namespace */
        tmpcp = cp + m;

        for (i = 0; stdtypes[i].len != 0; i++)
          {
          if (n == stdtypes[i].len && strncmp(tmpcp, stdtypes[i].name, n) == 0)
            {
            classname = stdtypes[i].name;
            base_bits = stdtypes[i].type;
            }
          }

        /* include the namespace if present */
        if (base_bits != 0 && m > 0)
          {
          classname = vtkParse_DuplicateString(cp, n);
          }
        }

      /* anything else is assumed to be a class, enum, or who knows */
      if (base_bits == 0)
        {
        base_bits = VTK_PARSE_UNKNOWN;
        classname = vtkParse_DuplicateString(cp, n);

        /* VTK classes all start with vtk */
        if (strncmp(classname, "vtk", 3) == 0)
          {
          base_bits = VTK_PARSE_OBJECT;
          /* make sure the "vtk" isn't just part of the namespace */
          for (k = 0; k < n; k++)
            {
            if (cp[k] == ':')
              {
              base_bits = VTK_PARSE_UNKNOWN;
              break;
              }
            }
          }
        /* Qt objects and enums */
        else if (classname[0] == 'Q' &&
                 ((classname[1] >= 'A' && classname[2] <= 'Z') ||
                  strncmp(classname, "Qt::", 4) == 0))
          {
          base_bits = VTK_PARSE_QOBJECT;
          }
        }
      }

    cp += n;
    while (*cp == ' ' || *cp == '\t') { cp++; }
    }

  *type_ptr = (static_bits | const_bits | unsigned_bits | base_bits);

  if (classname_ptr)
    {
    *classname_ptr = classname;
    }

  if ((unsigned_bits & VTK_PARSE_UNSIGNED) != 0 &&
      (base_bits & VTK_PARSE_UNSIGNED) == 0)
    {
    n = strlen(classname) + 9;
    tmp = (char *)malloc(n+1);
    strcpy(tmp, "unsigned ");
    strcpy(&tmp[9], classname);
    classname = vtkParse_DuplicateString(tmp, n);
    free(tmp);
    }

  return (cp - text);
}

/* Parse a type description in "text" and generate a typedef named "name" */
void vtkParse_ValueInfoFromString(ValueInfo *data, const char *text)
{
  const char *cp = text;
  size_t n;
  unsigned long m, count;
  unsigned int base_bits = 0;
  unsigned int pointer_bits = 0;
  unsigned int ref_bits = 0;
  const char *classname = NULL;

  /* get the basic type with qualifiers */
  cp += vtkParse_BasicTypeFromString(cp, &base_bits, &classname);

  if ((base_bits & VTK_PARSE_STATIC) != 0)
    {
    data->IsStatic = 1;
    }

  /* look for pointers (and const pointers) */
  while (*cp == '*')
    {
    cp++;
    pointer_bits = (pointer_bits << 2);
    while (*cp == ' ' || *cp == '\t') { cp++; }
    if (strncmp(cp, "const", 5) == 0 &&
        (cp[5] < 'a' || cp[5] > 'z') &&
        (cp[5] < 'A' || cp[5] > 'Z') &&
        (cp[5] < '0' || cp[5] > '9') &&
        cp[5] != '_')
      {
      cp += 5;
      while (*cp == ' ' || *cp == '\t') { cp++; }
      pointer_bits = (pointer_bits | VTK_PARSE_CONST_POINTER);
      }
    else
      {
      pointer_bits = (pointer_bits | VTK_PARSE_POINTER);
      }
    pointer_bits = (pointer_bits & VTK_PARSE_POINTER_MASK);
    }

  /* look for ref */
  if (*cp == '&')
    {
    cp++;
    while (*cp == ' ' || *cp == '\t') { cp++; }
    ref_bits = VTK_PARSE_REF;
    }

  /* look for the variable name */
  if ((*cp >= 'a' && *cp <= 'z') ||
      (*cp >= 'A' && *cp <= 'Z') ||
      (*cp == '_'))
    {
    /* skip all chars that are part of a name */
    n = 0;
    while ((cp[n] >= 'a' && cp[n] <= 'z') ||
           (cp[n] >= 'A' && cp[n] <= 'Z') ||
           (cp[n] >= '0' && cp[n] <= '9') ||
           cp[n] == '_') { n++; }

    data->Name = vtkParse_DuplicateString(cp, n);

    cp += n;
    while (*cp == ' ' || *cp == '\t') { cp++; }
    }

  /* look for array brackets */
  if (*cp == '[')
    {
    count = 1;
    }

  while (*cp == '[')
    {
    m = 0;
    while (*cp == ' ' || *cp == '\t') { cp++; }
    if (*cp >= '0' && *cp <= '9')
      {
      m = (int)strtol(cp, NULL, 0);
      }
    count *= m;
    n = 0;
    while (cp[n] != ']' && cp[n] != '\0' && cp[n] != '\n') { n++; }
    vtkParse_AddStringToArray(&data->Dimensions,
                              &data->NumberOfDimensions,
                              vtkParse_DuplicateString(cp, n));
    cp += n;
    if (cp[n] == ']') { cp++; }
    while (*cp == ' ' || *cp == '\t') { cp++; }
    }

  data->Class = classname;

  /* add pointer indirection to correspond to first array dimension */
  if (data->NumberOfDimensions > 1)
    {
    pointer_bits = ((pointer_bits << 2) | VTK_PARSE_ARRAY);
    }
  else if (data->NumberOfDimensions == 1)
    {
    pointer_bits = ((pointer_bits << 2) | VTK_PARSE_POINTER);
    }
  pointer_bits = (pointer_bits & VTK_PARSE_POINTER_MASK);

  /* (Add code here to look for "=" followed by a value ) */

  data->Type = (pointer_bits | ref_bits | base_bits);
}



/* substitute template types and values with specialized types and values */
static void func_substitution(
  FunctionInfo *data, unsigned long m, const char *arg_names[],
  const char *arg_values[], ValueInfo *arg_types[]);

static void value_substitution(
  ValueInfo *data, unsigned long m, const char *arg_names[],
  const char *arg_values[], ValueInfo *arg_types[])
{
  vtkParse_ExpandValues(data, m, arg_names, arg_values);
  vtkParse_ExpandTypedefs(data, m, arg_types);

  if (data->Function)
    {
    func_substitution(data->Function, m, arg_names, arg_values, arg_types);
    }
}

static void func_substitution(
  FunctionInfo *data, unsigned long m, const char *arg_names[],
  const char *arg_values[], ValueInfo *arg_types[])
{
  unsigned long i, n;

  n = data->NumberOfArguments;
  for (i = 0; i < n; i++)
    {
    value_substitution(data->Arguments[i], m, arg_names, arg_values, arg_types);
    if (i < MAX_ARGS)
      {
      data->ArgTypes[i] = data->Arguments[i]->Type;
      data->ArgClasses[i] = data->Arguments[i]->Class;
      if (data->Arguments[i]->NumberOfDimensions == 1 &&
          data->Arguments[i]->Count > 0)
        {
        data->ArgCounts[i] = data->Arguments[i]->Count;
        }
      }
    }
  if (data->ReturnValue)
    {
    value_substitution(data->ReturnValue, m, arg_names, arg_values, arg_types);
    data->ReturnType = data->ReturnValue->Type;
    data->ReturnClass = data->ReturnValue->Class;
    if (data->ReturnValue->NumberOfDimensions == 1 &&
        data->ReturnValue->Count > 0)
      {
      data->HintSize = data->ReturnValue->Count;
      data->HaveHint = 1;
      }
    }
  if (data->Signature)
    {
    data->Signature =
      vtkParse_Replace(data->Signature, m, arg_names, arg_values);
    }
}

static void class_substitution(
  ClassInfo *data, unsigned long m, const char *arg_names[],
  const char *arg_values[], ValueInfo *arg_types[])
{
  unsigned long i, n;

  /* superclasses may be templated */
  n = data->NumberOfSuperClasses;
  for (i = 0; i < n; i++)
    {
    data->SuperClasses[i] = vtkParse_Replace(
      data->SuperClasses[i], m, arg_names, arg_values);
    }

  n = data->NumberOfClasses;
  for (i = 0; i < n; i++)
    {
    class_substitution(data->Classes[i], m, arg_names, arg_values, arg_types);
    }

  n = data->NumberOfFunctions;
  for (i = 0; i < n; i++)
    {
    func_substitution(data->Functions[i], m, arg_names, arg_values, arg_types);
    }

  n = data->NumberOfConstants;
  for (i = 0; i < n; i++)
    {
    value_substitution(data->Constants[i], m, arg_names, arg_values, arg_types);
    }

  n = data->NumberOfVariables;
  for (i = 0; i < n; i++)
    {
    value_substitution(data->Variables[i], m, arg_names, arg_values, arg_types);
    }

  n = data->NumberOfTypedefs;
  for (i = 0; i < n; i++)
    {
    value_substitution(data->Typedefs[i], m, arg_names, arg_values, arg_types);
    }
}

/* Extract template args from a comma-separated list enclosed
 * in angle brackets.  Returns zero if no angle brackets found. */
size_t vtkParse_DecomposeTemplatedType(
  const char *text, const char **classname,
  unsigned long *np, const char ***argp)
{
  size_t i, j, k, n;
  int depth = 0;
  const char **template_args = NULL;
  unsigned long template_arg_count = 0;
  char *new_text;

  n = vtkParse_NameLength(text);

  /* is the class templated? */
  for (i = 0; i < n; i++)
    {
    if (text[i] == '<')
      {
      break;
      }
    }

  new_text = (char *)malloc(i + 1);
  strncpy(new_text, text, i);
  new_text[i] = '\0';
  *classname = new_text;

  if (text[i] == '<')
    {
    i++;
    /* extract the template arguments */
    for (;;)
      {
      while (text[i] == ' ' || text[i] == '\t') { i++; }
      j = i;
      while (text[j] != ',' && text[j] != '>' &&
             text[j] != '\n' && text[j] != '\0')
        {
        if (text[j] == '<')
          {
          j++;
          depth = 1;
          while (text[j] != '\n' && text[j] != '\0')
            {
            if (text[j] == '<') { depth++; }
            if (text[j] == '>') { if (--depth == 0) { break; } }
            j++;
            }
          if (text[j] == '\n' || text[j] == '\0') { break; }
          }
        j++;
        }

      k = j;
      while (text[k-1] == ' ' || text[k-1] == '\t') { --k; } 

      new_text = (char *)malloc(k-i + 1);
      strncpy(new_text, &text[i], k-i);
      vtkParse_AddStringToArray(&template_args, &template_arg_count,
                                new_text);

      i = j + 1;

      if (text[j] != ',')
        {
        break;
        }
      } 
    }

  *np = template_arg_count;
  *argp = template_args;

  return i;
}

/* Free the list of strings returned by ExtractTemplateArgs.  */
void vtkParse_FreeTemplateDecomposition(
  const char *name, unsigned long n, const char **args)
{
  unsigned long i;

  if (name)
    {
    free((char *)name);
    }

  if (n > 0)
    {
    for (i = 0; i < n; i++)
      {
      free((char *)args[i]);
      }

    free((char **)args);
    }
}


/* Specialize a templated class by substituting the provided arguments. */
void vtkParse_SpecializeTemplatedClass(
  ClassInfo *data, unsigned long n, const char *args[])
{
  TemplateArgs *t = data->Template;
  const char **new_args = NULL;
  const char **arg_names = NULL;
  ValueInfo **arg_types = NULL;
  unsigned long i, m;

  if (t == NULL)
    {
    fprintf(stderr, "vtkParse_SpecializeTemplatedClass: "
            "attempt to specialize a non-templated class.\n");
    return;
    }

  m = t->NumberOfArguments;
  if (n > m)
    {
    fprintf(stderr, "vtkParse_SpecializeTemplatedClass: "
            "too many template args.\n");
    return;
    }

  for (i = n; i < m; i++)
    {
    if (t->Arguments[i]->Value == NULL ||
        t->Arguments[i]->Value[0] == '\0')
      {
      fprintf(stderr, "vtkParse_SpecializeTemplatedClass: "
              "too few template args.\n");
      return;
      }
    }

  if (n < m)
    {
    new_args = (const char **)malloc(m*sizeof(char **));
    for (i = 0; i < n; i++)
      {
      new_args[i] = args[i];
      }
    for (i = n; i < m; i++)
      {
      new_args[i] = t->Arguments[i]->Value;
      }
    args = new_args;
    }

  arg_names = (const char **)malloc(m*sizeof(char **));
  arg_types = (ValueInfo **)malloc(m*sizeof(ValueInfo *));
  for (i = 0; i < m; i++)
    {
    arg_names[i] = t->Arguments[i]->Name;
    arg_types[i] = NULL;
    if (t->Arguments[i]->Type == 0)
      {
      arg_types[i] = (ValueInfo *)malloc(sizeof(ValueInfo));
      vtkParse_InitValue(arg_types[i]);
      vtkParse_ValueInfoFromString(arg_types[i], args[i]);
      arg_types[i]->ItemType = VTK_TYPEDEF_INFO;
      arg_types[i]->Name = arg_names[i];
      }
    }

  /* no longer templated (has been specialized) */
  if (data->Template)
    {
    vtkParse_FreeTemplateArgs(data->Template);
    }
  data->Template = NULL;

  class_substitution(data, m, arg_names, args, arg_types);

  /* free all allocated arrays */
  if (new_args)
    {
    free((char **)new_args);
    }

  free((char **)arg_names);

  for (i = 0; i < m; i++)
    {
    if (arg_types[i])
      {
      vtkParse_FreeValue(arg_types[i]);
      }
    }
  free(arg_types);
}
