/*=========================================================================

  Program:   WrapVTK
  Module:    vtkParseVariables.c

  Copyright (c) 2010 David Gobbi
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
#include "vtkParseUtils.h"
#include "vtkParseVariables.h"
#include "vtkConfigure.h"

/* A struct that lays out the function information in a way
 * that makes it easy to find methods that act on the same ivars.
 * Only ivar methods will properly fit this struct. */

typedef struct _MethodAttributes
{
  const char *Name;
  int Type;
  int Count;
  const char *ClassName;
  const char *Comment;
  int IsPublic;
  int IsProtected;
  int IsLegacy;
  int IsHinted;
  int IsMultiValue;
  int IsIndexed;
  int IsEnumerated;
  int IsBoolean;
} MethodAttributes;

typedef struct _ClassVariableMethods
{
  int NumberOfMethods;
  MethodAttributes *Methods;
} ClassVariableMethods;

/*-------------------------------------------------------------------
 * Checks for various common method names for variable access */

int isSetMethod(const char *name)
{
  return (name && name[0] == 'S' && name[1] == 'e' && name[2] == 't' &&
          isupper(name[3]));
}

int isSetNthMethod(const char *name)
{
  if (isSetMethod(name))
    {
    return (name[3] == 'N' && name[4] == 't' && name[5] == 'h' &&
            isupper(name[6]));
    }

  return 0;
}

int isSetNumberOfMethod(const char *name)
{
  int n;

  if (isSetMethod(name))
    {
    n = strlen(name);
    return (name[3] == 'N' && name[4] == 'u' && name[5] == 'm' &&
            name[6] == 'b' && name[7] == 'e' && name[8] == 'r' &&
            name[9] == 'O' && name[10] == 'f' && isupper(name[11]) &&
            name[n-1] == 's');
    }

  return 0;
}

int isGetMethod(const char *name)
{
  return (name && name[0] == 'G' && name[1] == 'e' && name[2] == 't' &&
          isupper(name[3]));
}

int isGetNthMethod(const char *name)
{
  if (isGetMethod(name))
    {
    return (name[3] == 'N' && name[4] == 't' && name[5] == 'h' &&
            isupper(name[6]));
    }

  return 0;
}

int isGetNumberOfMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    return (name[3] == 'N' && name[4] == 'u' && name[5] == 'm' &&
            name[6] == 'b' && name[7] == 'e' && name[8] == 'r' &&
            name[9] == 'O' && name[10] == 'f' && isupper(name[11]) &&
            name[n-1] == 's');
    }

  return 0;
}

int isAddMethod(const char *name)
{
  return (name && name[0] == 'A' && name[1] == 'd' && name[2] == 'd' &&
          isupper(name[3]));
}

int isRemoveMethod(const char *name)
{
  return (name && name[0] == 'R' && name[1] == 'e' && name[2] == 'm' &&
          name[3] == 'o' && name[4] == 'v' && name[5] == 'e' &&
          isupper(name[6]));
}

int isRemoveAllMethod(const char *name)
{
  int n;

  if (isRemoveMethod(name))
    {
    n = strlen(name);
    return (name[6] == 'A' && name[7] == 'l' && name[8] == 'l' &&
            isupper(name[9]) && name[n-1] == 's');
    }

  return 0;
}

int isBooleanMethod(const char *name)
{
  int n;

  if (name)
    {
    n = strlen(name);
    if ((n > 2 && name[n-2] == 'O' && name[n-1] == 'n') ||
        (n > 3 && name[n-3] == 'O' && name[n-2] == 'f' && name[n-1] == 'f'))
      {
      return 1;
      }    
    }

  return 0;
}

int isEnumeratedMethod(const char *name)
{
  size_t i, n;

  if (isSetMethod(name))
    {
    n = strlen(name) - 3;
    for (i = 3; i < n; i++)
      {
      if (name[i+0] == 'T' && name[i+1] == 'o' &&
          (isupper(name[i+2]) || isdigit(name[i+2])))
        {
        return 1;
        }
      }
    }

  return 0;
}

int isAsStringMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    if (n > 11)
      {
      if (name[n-8] == 'A' && name[n-7] == 's' && name[n-6] == 'S' &&
          name[n-5] == 't' && name[n-4] == 'r' && name[n-3] == 'i' &&
          name[n-2] == 'n' && name[n-1] == 'g')
        {
        return 1;
        }
      }
    }

  return 0;
}

int isGetMinValueMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    if (n > 11 && strcmp("MinValue", &name[n-8]) == 0)
      {
      return 1;
      }    
    }

  return 0;
}

int isGetMaxValueMethod(const char *name)
{
  int n;

  if (isGetMethod(name))
    {
    n = strlen(name);
    if (n > 11 && strcmp("MaxValue", &name[n-8]) == 0)
      {
      return 1;
      }    
    }

  return 0;
}

/*-------------------------------------------------------------------
 * Return the method category bit for the given method, based on the
 * method name and other information in the MethodAttributes struct.
 * If shortForm in on, then suffixes such as On, Off, AsString,
 * and ToSomething are considered while doing the categorization */

unsigned int methodCategory(MethodAttributes *meth, int shortForm)
{
  int n;
  const char *name;
  name = meth->Name;

  if (isSetMethod(name))
    {
    if (meth->IsEnumerated)
      {
      return VTKVAR_ENUM_SET;
      }
    else if (meth->IsIndexed)
      {
      if (isSetNthMethod(name))
        {
        return VTKVAR_NTH_SET;
        }
      else
        {
        return VTKVAR_INDEX_SET;
        }
      }
    else if (meth->IsMultiValue)
      {
      return VTKVAR_MULTI_SET;
      }
    else if (isSetNumberOfMethod(name) && shortForm)
      {
      return VTKVAR_SET_NUM;
      }
    else
      {
      return VTKVAR_BASIC_SET;
      }
    }
  else if (meth->IsBoolean)
    {
    n = strlen(name);
    if (name[n-1] == 'n')
      {
      return VTKVAR_BOOL_ON;
      }
    else
      {
      return VTKVAR_BOOL_OFF;
      }
    }
  else if (isGetMethod(name))
    {
    if (isGetMinValueMethod(name) && shortForm)
      {
      return VTKVAR_MIN_GET;
      }
    else if (isGetMaxValueMethod(name) && shortForm)
      {
      return VTKVAR_MAX_GET;
      }
    else if (isAsStringMethod(name) && shortForm)
      {
      return VTKVAR_ENUM_GET;
      }
    else if (meth->IsIndexed && meth->Count > 0 && !meth->IsHinted)
      {
      if (isGetNthMethod(name))
        {
        return VTKVAR_NTH_RHS_GET;
        }
      else
        {
        return VTKVAR_INDEX_RHS_GET;
        }
      }
    else if (meth->IsIndexed)
      {
      if (isGetNthMethod(name))
        {
        return VTKVAR_NTH_GET;
        }
      else
        {
        return VTKVAR_INDEX_GET;
        }
      }
    else if (meth->IsMultiValue)
      {
      return VTKVAR_MULTI_GET;
      }
    else if (meth->Count > 0 && !meth->IsHinted)
      {
      return VTKVAR_RHS_GET;
      }
    else if (isGetNumberOfMethod(name) && shortForm)
      {
      return VTKVAR_GET_NUM;
      }
    else
      {
      return VTKVAR_BASIC_GET;
      }
    }
  else if (isRemoveMethod(name))
    {
    if (isRemoveAllMethod(name))
      {
      return VTKVAR_REMOVEALL;
      }
    else if (meth->IsIndexed)
      {
      return VTKVAR_INDEX_REM;
      }
    else
      {
      return VTKVAR_BASIC_REM;
      }
    }
  else if (isAddMethod(name))
    {
    if (meth->IsIndexed)
      {
      return VTKVAR_INDEX_ADD;
      }
    else if (meth->IsMultiValue)
      {
      return VTKVAR_MULTI_ADD;
      }
    else
      {
      return VTKVAR_BASIC_ADD;
      }
    }

  return 0;
}

/*-------------------------------------------------------------------
 * remove the following prefixes from a method name:
 * Set, Get, Add, Remove */

const char *nameWithoutPrefix(const char *name)
{
  if (isGetNthMethod(name) || isSetNthMethod(name))
    {
    return &name[6];
    }
  else if (isGetMethod(name) || isSetMethod(name) || isAddMethod(name))
    {
    return &name[3];
    }
  else if (isRemoveAllMethod(name))
    {
    return &name[9];
    }
  else if (isRemoveMethod(name))
    {
    return &name[6];
    }

  return name;
} 

/*-------------------------------------------------------------------
 * check for a valid suffix, i.e. "On" or "Off" or "ToSomething" */

int isValidSuffix(
  const char *methName, const char *varName, const char *suffix)
{
  if ((suffix[0] == 'O' && suffix[1] == 'n' && suffix[2] == '\0') ||
      (suffix[0] == 'O' && suffix[1] == 'f' && suffix[2] == 'f' &&
       suffix[3] == '\0'))
    {
    return 1;
    }

  else if (isSetMethod(methName) &&
      suffix[0] == 'T' && suffix[1] == 'o' &&
      (isupper(suffix[2]) || isdigit(suffix[2])))
    {
    return 1;
    }

  else if (isGetMethod(methName) &&
      (suffix[0] == 'A' && suffix[1] == 's' &&
       (isupper(suffix[2]) || isdigit(suffix[2]))) ||
      (((suffix[0] == 'M' && suffix[1] == 'a' && suffix[2] == 'x') ||
        (suffix[0] == 'M' && suffix[1] == 'i' && suffix[2] == 'n')) &&
       (suffix[3] == 'V' && suffix[4] == 'a' && suffix[5] == 'l' &&
        suffix[6] == 'u' && suffix[7] == 'e' && suffix[8] == '\0')))
    {
    return 1;
    }

  else if (isRemoveAllMethod(methName))
    {
    return (suffix[0] == 's' && suffix[1] == '\0');
    }

  else if (isGetNumberOfMethod(methName) ||
           isSetNumberOfMethod(methName))
    {
    if (strncmp(varName, "NumberOf", 8) == 0)
      {
      return (suffix[0] == '\0');
      }
    else
      {
      return (suffix[0] == 's' && suffix[1] == '\0');
      }
    }
  else if (suffix[0] == '\0')
    {
    return 1;
    }

  return 0;
}

/*-------------------------------------------------------------------
 * Convert the FunctionInfo into a MethodAttributes, which will make
 * it easier to find matched Set/Get methods.  A return value of zero
 * means the conversion failed, i.e. the method signature is too complex
 * for the MethodAttributes struct */

int getMethodAttributes(FunctionInfo *func, MethodAttributes *attrs)
{
  size_t i, n;
  int tmptype = 0;
  int allSame = 0;
  int indexed = 0;

  attrs->Name = func->Name;
  attrs->Type = 0;
  attrs->Count = 0;
  attrs->ClassName = 0;
  attrs->Comment = func->Comment;
  attrs->IsPublic = func->IsPublic;
  attrs->IsProtected = func->IsProtected;
  attrs->IsLegacy = func->IsLegacy;
  attrs->IsMultiValue = 0;
  attrs->IsHinted = 0;
  attrs->IsIndexed = 0;
  attrs->IsEnumerated = 0;
  attrs->IsBoolean = 0;

  /* check for indexed methods: the first argument will be an integer */
  if (func->NumberOfArguments > 0 &&
      (typeBaseType(func->ArgTypes[0]) == VTK_PARSE_INT ||
       typeBaseType(func->ArgTypes[0]) == VTK_PARSE_ID_TYPE) &&
      !typeIsIndirect(func->ArgTypes[0]))
    {
    /* methods of the form "void SetValue(int i, type value)" */
    if (typeBaseType(func->ReturnType) == VTK_PARSE_VOID &&
        !typeIsIndirect(func->ReturnType) &&
        func->NumberOfArguments == 2)
      {
      indexed = 1;

      if (!isSetNumberOfMethod(func->Name))
        {
        /* make sure this isn't a multi-value int method */
        tmptype = func->ArgTypes[0];
        allSame = 1;

        n = func->NumberOfArguments;
        for (i = 0; i < n; i++)
          {
          if (func->ArgTypes[i] != tmptype)
            {
            allSame = 0;
            }
          }
        indexed = !allSame;
        }
      }
    /* methods of the form "type GetValue(int i)" */
    if (!(typeBaseType(func->ReturnType) == VTK_PARSE_VOID &&
          !typeIsIndirect(func->ReturnType)) &&
        func->NumberOfArguments == 1)
      {
      indexed = 1;
      }

    attrs->IsIndexed = indexed;
    }

  /* if return type is not void and no args or 1 index */
  if (!(typeBaseType(func->ReturnType) == VTK_PARSE_VOID &&
        !typeIsIndirect(func->ReturnType)) &&
      func->NumberOfArguments == indexed)
    {
    /* methods of the form "type GetValue()" or "type GetValue(i)" */
    if (isGetMethod(func->Name))
      {
      attrs->Type = func->ReturnType;
      attrs->Count = (func->HaveHint ? func->HintSize : 0);
      attrs->IsHinted = func->HaveHint;
      attrs->ClassName = func->ReturnClass;

      return 1;
      }
    }

  /* if return type is void and 1 arg or 1 index and 1 arg */
  if (typeBaseType(func->ReturnType) == VTK_PARSE_VOID &&
      !typeIsIndirect(func->ReturnType) &&
      func->NumberOfArguments == (1 + indexed))
    {
    /* "void SetValue(type)" or "void SetValue(int, type)" */
    if (isSetMethod(func->Name))
      {
      attrs->Type = func->ArgTypes[indexed];
      attrs->Count = func->ArgCounts[indexed];
      attrs->ClassName = func->ArgClasses[indexed];

      return 1;
      }
    /* "void GetValue(type *)" or "void GetValue(int, type *)" */
    else if (isGetMethod(func->Name) &&
             func->ArgCounts[indexed] > 0 &&
             typeIsIndirect(func->ArgTypes[indexed]) &&
             !typeIsConst(func->ArgTypes[indexed]))
      {
      attrs->Type = func->ArgTypes[indexed];
      attrs->Count = func->ArgCounts[indexed];
      attrs->ClassName = func->ArgClasses[indexed];

      return 1;
      }
    /* "void AddValue(vtkObject *)" or "void RemoveValue(vtkObject *)" */
    else if (((isAddMethod(func->Name) || isRemoveMethod(func->Name)) &&
             typeBaseType(func->ArgTypes[indexed]) == VTK_PARSE_VTK_OBJECT &&
             typeIndirection(func->ArgTypes[indexed]) == VTK_PARSE_POINTER))
      {
      attrs->Type = func->ArgTypes[indexed];
      attrs->Count = func->ArgCounts[indexed];
      attrs->ClassName = func->ArgClasses[indexed];

      return 1;
      }
    }

  /* check for multiple arguments of the same type */
  if (func->NumberOfArguments > 1 && !indexed)
    {
    tmptype = func->ArgTypes[0];
    allSame = 1;

    n = func->NumberOfArguments;
    for (i = 0; i < n; i++)
      {
      if (func->ArgTypes[i] != tmptype)
        {
        allSame = 0;
        }
      }

    if (allSame)
      {
      /* "void SetValue(type x, type y, type z)" */
      if (isSetMethod(func->Name) && !typeIsIndirect(tmptype) &&
          typeBaseType(func->ReturnType) == VTK_PARSE_VOID &&
          !typeIsIndirect(func->ReturnType))
        {
        attrs->Type = tmptype;
        attrs->Count = n;
        attrs->IsMultiValue = 1;

        return 1;
        }
      /* "void GetValue(type& x, type& x, type& x)" */
      else if (isGetMethod(func->Name) && 
               typeIndirection(tmptype) == VTK_PARSE_REF &&
               !typeIsConst(tmptype) &&
               typeBaseType(func->ReturnType) == VTK_PARSE_VOID &&
              !typeIsIndirect(func->ReturnType))
        {
        attrs->Type = tmptype;
        attrs->Count = n;
        attrs->IsMultiValue = 1;

        return 1;
        }
      /* "void AddValue(type x, type y, type z)" */
      else if (isAddMethod(func->Name) && !typeIsIndirect(tmptype) &&
               (typeBaseType(func->ReturnType) == VTK_PARSE_VOID ||
                typeBaseType(func->ReturnType) == VTK_PARSE_INT ||
                typeBaseType(func->ReturnType) == VTK_PARSE_ID_TYPE) &&
               !typeIsIndirect(func->ReturnType))
        {
        attrs->Type = tmptype;
        attrs->Count = n;
        attrs->IsMultiValue = 1;

        return 1;
        }
      }
    }

  /* if return type is void, and there are no arguments */
  if (typeBaseType(func->ReturnType) == VTK_PARSE_VOID &&
      !typeIsIndirect(func->ReturnType) &&
      func->NumberOfArguments == 0)
    {
    attrs->Type = VTK_PARSE_VOID;

    /* "void ValueOn()" or "void ValueOff()" */
    if (isBooleanMethod(func->Name))
      {
      attrs->IsBoolean = 1;
      return 1;
      }
    /* "void SetValueToEnum()" */
    else if (isEnumeratedMethod(func->Name))
      {
      attrs->IsEnumerated = 1;
      return 1;
      }
    /* "void RemoveAllValues()" */
    else if (isRemoveAllMethod(func->Name))
      {
      return 1;
      }
    }

  return 0;
}

/*-------------------------------------------------------------------
 * Check to see if the specified method is a match with the specified
 * variable, i.e the name, type, and array count of the variable
 * must match.  The longMatch value is set to '1' if the prefix/suffix
 * was part of the name match. */

int methodMatchesVariable(
  VariableAttributes *var, MethodAttributes *meth, int *longMatch)
{
  size_t i, n;
  int varType, methType;
  const char *varName;
  const char *name;
  const char *methSuffix;
  int methodBitfield = 0;

  /* get the bitfield containing all found methods for this variable */
  if (meth->IsPublic)
    {
    methodBitfield = var->PublicMethods;
    }
  else if (meth->IsProtected)
    {
    methodBitfield = var->ProtectedMethods;
    }
  else
    {
    methodBitfield = var->PrivateMethods;
    }

  /* get the variable name and compare it to the method name */ 
  varName = var->Name;
  name = nameWithoutPrefix(meth->Name);

  if (name == 0 || varName == 0)
    {
    return 0;
    }

  /* longMatch is only set for full matches of GetNumberOf(),
   * SetNumberOf(), GetVarMinValue(), GetVarMaxValue() methods */ 
  *longMatch = 0;
  n = strlen(varName);
  if (isGetNumberOfMethod(meth->Name) || isSetNumberOfMethod(meth->Name))
    {
    if (strncmp(varName, "NumberOf", 8) == 0 && isupper(varName[8]))
      {
      /* longer match */
      *longMatch = 1;
      }
    else
      {
      /* longer prefix */
      name = &meth->Name[11];
      }
    }
  else if (isGetMinValueMethod(meth->Name))
    {
    if (n >= 8 && strcmp(varName-8, "MinValue") == 0)
      {
      *longMatch = 1;
      } 
    }
  else if (isGetMaxValueMethod(meth->Name))
    {
    if (n >= 8 && strcmp(varName-8, "MaxValue") == 0)
      {
      *longMatch = 1;
      } 
    }
  else if (isAsStringMethod(meth->Name))
    {
    if (n >= 8 && strcmp(varName-8, "AsString") == 0)
      {
      *longMatch = 1;
      } 
    }

  /* make sure the method name contains the variable name */
  if (!strncmp(name, varName, n) == 0)
    {
    return 0;
    }

  /* make sure that any non-matching bits are valid suffixes */
  methSuffix = &name[n];
  if (!isValidSuffix(meth->Name, varName, methSuffix))
    {
    return 0;
    }

  /* check for type match */
  methType = meth->Type;
  varType = var->Type;

  /* remove "const" and "static" */
  if (typeHasQualifier(methType))
    {
    methType = (methType & VTK_PARSE_UNQUALIFIED_TYPE);
    }

  /* check for RemoveAll method matching an Add method*/
  if (isRemoveAllMethod(meth->Name) &&
      methType == VTK_PARSE_VOID &&
      !typeIsIndirect(methType) &&
      ((methodBitfield & (VTKVAR_BASIC_ADD | VTKVAR_MULTI_ADD)) != 0))
    {
    return 1;
    }

  /* check for GetNumberOf and SetNumberOf for indexed variables */
  if (isGetNumberOfMethod(meth->Name) &&
      (methType == VTK_PARSE_INT || methType == VTK_PARSE_ID_TYPE) &&
      !typeIsIndirect(methType) &&
      ((methodBitfield & (VTKVAR_INDEX_GET | VTKVAR_NTH_GET)) != 0))
    {
    return 1;
    }

  if (isSetNumberOfMethod(meth->Name) &&
      (methType == VTK_PARSE_INT || methType == VTK_PARSE_ID_TYPE) &&
      !typeIsIndirect(methType) &&
      ((methodBitfield & (VTKVAR_INDEX_SET | VTKVAR_NTH_SET)) != 0))
    {
    return 1;
    }

  /* remove ampersands i.e. "ref" */
  if (typeIndirection(methType) == VTK_PARSE_REF)
    {
    methType = (methType & ~VTK_PARSE_INDIRECT);
    }
  else if (typeIndirection(methType) == VTK_PARSE_POINTER_REF)
    {
    methType = ((methType & ~VTK_PARSE_INDIRECT) | VTK_PARSE_POINTER);
    } 

  /* if method is multivalue, e.g. SetColor(r,g,b), then the
   * referenced variable is a pointer */
  if (meth->IsMultiValue)
    {
    if (typeIndirection(methType) == VTK_PARSE_POINTER)
      {
      methType = (methType & ~VTK_PARSE_INDIRECT);
      methType = (methType | VTK_PARSE_POINTER_POINTER);
      }
    else if (typeIndirection(methType) == 0)
      {
      methType = (methType | VTK_PARSE_POINTER);
      }
    else
      {
      return 0;
      }
    }

  /* promote "void" to "int" for e.g. boolean methods */
  if (meth->IsBoolean || meth->IsEnumerated)
    {
    methType = VTK_PARSE_INT;
    }

  /* check for GetValueAsString method, assume it has matching enum  */
  if (isAsStringMethod(meth->Name) &&
      typeBaseType(methType) == VTK_PARSE_CHAR &&
      typeIndirection(methType) == VTK_PARSE_POINTER)
    {
    methType = VTK_PARSE_INT;
    }

  /* check for matched type and count */
  if (methType != varType || meth->Count != var->Count)
    {
    return 0;
    }

  /* if vtkObject, check that classes match */
  if (typeBaseType(methType) == VTK_PARSE_VTK_OBJECT)
    {
    if (meth->IsMultiValue || !typeIsPointer(methType) || meth->Count != 0 ||
        meth->ClassName == 0 || var->ClassName == 0 ||
        strcmp(meth->ClassName, var->ClassName) != 0)
      {
      return 0;
      }
    }

  /* done, match was found! */
  return 1;
}

/*-------------------------------------------------------------------
 * initialize a VariableAttributes struct from a MethodAttributes
 * struct, only valid if the method name has no suffixes such as
 * On/Off, AsString, ToSomething, RemoveAllSomethings, etc. */ 

void initializeVariableAttributes(
  VariableAttributes *var, MethodAttributes *meth)
{
  unsigned int methodBit;
  int type;
  type = meth->Type;

  /* for ValueOn()/Off() or SetValueToEnum() methods, set type to int */
  if (meth->IsBoolean || meth->IsEnumerated)
    {
    type = VTK_PARSE_INT;
    }

  var->Name = nameWithoutPrefix(meth->Name);

  /* get variable type, but don't include "ref" as part of type,
   * and use a pointer if the method is multi-valued */
  var->Type = typeBaseType(type);
  if ((!meth->IsMultiValue &&
       (typeIndirection(type) == VTK_PARSE_POINTER ||
        typeIndirection(type) == VTK_PARSE_POINTER_REF)) ||
      (meth->IsMultiValue) &&
       (typeIndirection(type) == 0 ||
        typeIndirection(type) == VTK_PARSE_REF))
    {
    var->Type = (var->Type | VTK_PARSE_POINTER);
    }
  else if (typeIndirection(type) == VTK_PARSE_POINTER_POINTER ||
           (typeIndirection(type) == VTK_PARSE_POINTER &&
            meth->IsMultiValue))
    {
    var->Type = (var->Type | VTK_PARSE_POINTER_POINTER);
    }

  var->ClassName = meth->ClassName;
  var->Count = meth->Count;
  var->EnumConstantNames = 0;
  var->PublicMethods = 0;
  var->ProtectedMethods = 0;
  var->PrivateMethods = 0;
  var->LegacyMethods = 0;
  var->Comment = meth->Comment;

  /* add this as the initial member of the method bitfield,
   * and ignore any suffixes while doing the categorization */
  methodBit = methodCategory(meth, 0);

  if (meth->IsPublic)
    {
    var->PublicMethods = methodBit;
    }
  else if (meth->IsProtected)
    {
    var->ProtectedMethods = methodBit;
    }
  else
    {
    var->PrivateMethods = methodBit;
    }

  if (meth->IsLegacy)
    {
    var->LegacyMethods = methodBit;
    }
}

/*-------------------------------------------------------------------
 * Find all the methods that match the specified method, and add
 * flags to the VariableAttributes struct */

void findAllMatches(
  VariableAttributes *var, ClassVariableMethods *methods,
  int matchedMethods[])
{
  int i, j, n;
  size_t m;
  MethodAttributes *meth;
  unsigned int methodBit;
  int longMatch;
  int foundNoMatches = 0;

  n = methods->NumberOfMethods;

  /* loop repeatedly until no more matches are found */
  while (!foundNoMatches)
    {
    foundNoMatches = 1;

    for (i = 0; i < n; i++)
      {
      if (matchedMethods[i]) { continue; }

      meth = &methods->Methods[i];
      if (methodMatchesVariable(var, meth, &longMatch))
        {
        matchedMethods[i] = 1;
        foundNoMatches = 0;

        /* add this as a member of the method bitfield, and consider method
         * suffixes like On, MaxValue, etc. while doing the categorization */
        methodBit = methodCategory(meth, !longMatch);

        if (meth->IsPublic)
          {
          var->PublicMethods |= methodBit;
          }
        else if (meth->IsProtected)
          {
          var->ProtectedMethods |= methodBit;
          }
        else
          {
          var->PrivateMethods |= methodBit;
          }

        if (meth->IsLegacy)
          {
          var->LegacyMethods |= methodBit;
          }

        if (meth->IsEnumerated)
          {
          m = strlen(var->Name);
          if (meth->Name[3+m] == 'T' && meth->Name[4+m] == 'o' &&
              (isdigit(meth->Name[5+m]) || isupper(meth->Name[5+m])))
            {
            if (var->EnumConstantNames == 0)
              {
              var->EnumConstantNames =
                (const char **)malloc(sizeof(char *)*8);
              var->EnumConstantNames[0] = 0;
              }

            j = 0;
            while (var->EnumConstantNames[j] != 0) { j++; }
            var->EnumConstantNames[j++] = &meth->Name[5+m];
            if (j % 8 == 0)
              {
              var->EnumConstantNames =
                (const char **)realloc(var->EnumConstantNames,
                                       sizeof(char *)*(j+8));
              }
            var->EnumConstantNames[j] = 0; 
            }
          }
        }
      }
    }
}

/*-------------------------------------------------------------------
 * This is the method that finds out everything that it can about
 * all variables that can be accessed by the methods of a class */

void categorizeVariables(
  ClassVariableMethods *methods, ClassVariables *variables)
{
  int i, n;
  int methodId;
  int *matchedMethods;
  MethodAttributes *meth;
  VariableAttributes *var;

  variables->NumberOfVariables = 0;

  n = methods->NumberOfMethods;
  matchedMethods = (int *)malloc(sizeof(int)*n);
  for (i = 0; i < n; i++)
    {
    matchedMethods[i] = 0;
    }

  /* start with the set methods */
  for (i = 0; i < n; i++)
    {
    if (matchedMethods[i]) { continue; }

    meth = &methods->Methods[i];
    /* all set methods except for SetValueToEnum() methods
     * and SetNumberOf() methods */
    if (isSetMethod(meth->Name) && !meth->IsEnumerated &&
        !isSetNumberOfMethod(meth->Name))
      {
      matchedMethods[i] = 1;
      var = &variables->Variables[variables->NumberOfVariables++];
      initializeVariableAttributes(var, meth);
      findAllMatches(var, methods, matchedMethods);
      }
    }

  /* sweep SetNumberOf() methods that didn't have
   * matching indexed Set methods */
  for (i = 0; i < n; i++)
    {
    if (matchedMethods[i]) { continue; }
    meth = &methods->Methods[i];
    if (isSetNumberOfMethod(meth->Name))
      {
      matchedMethods[i] = 1;
      var = &variables->Variables[variables->NumberOfVariables++];
      initializeVariableAttributes(var, meth);
      findAllMatches(var, methods, matchedMethods);
      }
    }

  /* next do the get methods that didn't have matching set methods */
  for (i = 0; i < n; i++)
    {
    if (matchedMethods[i]) { continue; }

    meth = &methods->Methods[i];
    /* all get methods except for GetValueAs() methods
     * and GetNumberOf() methods */
    if (isGetMethod(meth->Name) && !isAsStringMethod(meth->Name) &&
        !isGetNumberOfMethod(meth->Name))
      {
      matchedMethods[i] = 1;
      var = &variables->Variables[variables->NumberOfVariables++];
      initializeVariableAttributes(var, meth);
      findAllMatches(var, methods, matchedMethods);
      }
    }

  /* sweep the GetNumberOf() methods that didn't have
   * matching indexed Get methods */
  for (i = 0; i < n; i++)
    {
    if (matchedMethods[i]) { continue; }
    meth = &methods->Methods[i];
    if (isGetNumberOfMethod(meth->Name))
      {
      matchedMethods[i] = 1;
      var = &variables->Variables[variables->NumberOfVariables++];
      initializeVariableAttributes(var, meth);
      findAllMatches(var, methods, matchedMethods);
      }
    }

  /* finally do the add methods */
  for (i = 0; i < n; i++)
    {
    if (matchedMethods[i]) { continue; }

    meth = &methods->Methods[i];
    /* all add methods */
    if (isAddMethod(meth->Name))
      {
      matchedMethods[i] = 1;
      var = &variables->Variables[variables->NumberOfVariables++];
      initializeVariableAttributes(var, meth);
      findAllMatches(var, methods, matchedMethods);
      }
    }

  free(matchedMethods);
}

/*-------------------------------------------------------------------
 * search for methods that are repeated with minor variations */
int searchForRepeatedMethods(
  MethodAttributes *attrs, ClassVariableMethods *methods)
{
  int i, n;
  n = methods->NumberOfMethods;
  MethodAttributes *meth;

  for (i = 0; i < n; i++)
    {
    meth = &methods->Methods[i];

    /* check whether the function name and basic structure are matched */
    if (strcmp(attrs->Name, meth->Name) == 0 &&
        typeIndirection(attrs->Type) == typeIndirection(meth->Type) &&
        attrs->IsPublic == meth->IsPublic &&
        attrs->IsProtected == meth->IsProtected &&
        attrs->IsHinted == meth->IsHinted &&
        attrs->IsMultiValue == meth->IsMultiValue &&
        attrs->IsIndexed == meth->IsIndexed &&
        attrs->IsEnumerated == meth->IsEnumerated &&
        attrs->IsBoolean == meth->IsBoolean)
      {
      /* check to see if the types are compatible:
       * prefer "double" over "float",
       * prefer higher-counted arrays,
       * prefer non-legacy methods */

      if ((typeBaseType(attrs->Type) == VTK_PARSE_FLOAT &&
           typeBaseType(meth->Type) == VTK_PARSE_DOUBLE) ||
          (typeBaseType(attrs->Type) == typeBaseType(meth->Type) &&
           attrs->Count < meth->Count) ||
          attrs->IsLegacy && !meth->IsLegacy)
        {
        /* keep existing method */
        return 0;
        }

      if ((typeBaseType(attrs->Type) == VTK_PARSE_DOUBLE &&
           typeBaseType(meth->Type) == VTK_PARSE_FLOAT) ||
          (typeBaseType(attrs->Type) == typeBaseType(meth->Type) &&
           attrs->Count > meth->Count) ||
          !attrs->IsLegacy && meth->IsLegacy)
        {
        /* replace existing method */
        meth->IsLegacy = attrs->IsLegacy;
        meth->Comment = attrs->Comment;
        meth->Type = attrs->Type;
        meth->Count = attrs->Count;
        return 0;
        }
      }
    }

  /* no matches */
  return 1;
}

/*-------------------------------------------------------------------
 * categorize methods that get/set/add/remove values */

void categorizeVariableMethods(FileInfo *data, ClassVariableMethods *methods)
{
  int i, n, methodId;
  int *functionIds;
  FunctionInfo *func;
  MethodAttributes *attrs;

  methods->NumberOfMethods = 0;

  /* create a list of function ids */
  n = 0;
  functionIds = (int *)malloc(sizeof(int)*data->NumberOfFunctions);
  for (i = 0; i < data->NumberOfFunctions; i++)
    {
    if (data->Functions[i].Name &&
        !data->Functions[i].ArrayFailure &&
        !data->Functions[i].IsOperator)
      {
      functionIds[n++] = i;
      }
    }

  /* build up the ClassVariableMethods struct */
  for (i = 0; i < n; i++)
    {
    func = &data->Functions[functionIds[i]];
    attrs = &methods->Methods[methods->NumberOfMethods];

    /* copy the func into a MethodAttributes struct if possible */ 
    if (getMethodAttributes(func, attrs))
      {
      /* check for repeats e.g. SetPoint(float *), SetPoint(double *) */
      if (searchForRepeatedMethods(attrs, methods))
        {      
        methods->NumberOfMethods++;
        }
      }
    }

  free(functionIds);
}

/*-------------------------------------------------------------------
 * build a ClassVariables struct from the info in a FileInfo struct */

ClassVariables *buildClassVariablesStruct(FileInfo *data)
{
  ClassVariables *vars;
  ClassVariableMethods *methods;

  methods = (ClassVariableMethods *)malloc(sizeof(ClassVariableMethods));
  methods->Methods = (MethodAttributes *)malloc(sizeof(MethodAttributes)*
                                                data->NumberOfFunctions);

  /* categorize the methods according to what variables they reference
   * and what they do to that variable */
  categorizeVariableMethods(data, methods);

  vars = (ClassVariables *)malloc(sizeof(ClassVariables));
  vars->Variables = (VariableAttributes *)malloc(sizeof(VariableAttributes)*
                                                 methods->NumberOfMethods);

  /* synthesize a list of variables from the list of methods */
  categorizeVariables(methods, vars);
  
  free(methods->Methods);
  free(methods);

  return vars;
}

/*-------------------------------------------------------------------
 * free a class variables struct */

void freeClassVariablesStruct(ClassVariables *vars)
{
  free(vars->Variables);
  free(vars);
}

/*-------------------------------------------------------------------
 * get a string representation of method bitfield value */

const char *methodTypeString(unsigned int methodType)
{
  switch (methodType)
    {
    case VTKVAR_BASIC_GET:
      return "BASIC_GET";
    case VTKVAR_BASIC_SET:
      return "BASIC_SET";
    case VTKVAR_MULTI_GET:
      return "MULTI_GET";
    case VTKVAR_MULTI_SET:
      return "MULTI_SET";
    case VTKVAR_INDEX_GET:
      return "INDEX_GET";
    case VTKVAR_INDEX_SET:
      return "INDEX_SET";
    case VTKVAR_NTH_GET:
      return "NTH_GET";
    case VTKVAR_NTH_SET:
      return "NTH_SET";
    case VTKVAR_RHS_GET:
      return "RHS_GET";
    case VTKVAR_INDEX_RHS_GET:
      return "INDEX_RHS_GET";
    case VTKVAR_NTH_RHS_GET:
      return "NTH_RHS_GET";
    case VTKVAR_ENUM_GET:
      return "ENUM_GET";
    case VTKVAR_ENUM_SET:
      return "ENUM_SET";
    case VTKVAR_BOOL_ON:
      return "BOOL_ON";
    case VTKVAR_BOOL_OFF:
      return "BOOL_OFF";
    case VTKVAR_MIN_GET:
      return "MIN_GET";
    case VTKVAR_MAX_GET:
      return "MAX_GET";
    case VTKVAR_GET_NUM:
      return "GET_NUM";
    case VTKVAR_SET_NUM:
      return "SET_NUM";
    case VTKVAR_BASIC_ADD:
      return "BASIC_ADD";
    case VTKVAR_MULTI_ADD:
      return "MULTI_ADD";
    case VTKVAR_INDEX_ADD:
      return "INDEX_ADD";
    case VTKVAR_BASIC_REM:
      return "BASIC_REM";
    case VTKVAR_INDEX_REM:
      return "INDEX_REM";
    case VTKVAR_REMOVEALL:
      return "REMOVEALL";
    }

  return "";
}
