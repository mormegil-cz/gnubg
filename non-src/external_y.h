/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     STRING = 258,
     NUMBER = 259,
     EVALUATION = 260,
     PLIES = 261,
     CUBE = 262,
     CUBEFUL = 263,
     CUBELESS = 264,
     NOISE = 265,
     REDUCED = 266,
     PRUNE = 267,
     FIBSBOARD = 268,
     AFIBSBOARD = 269,
     ON = 270,
     OFF = 271
   };
#endif
/* Tokens.  */
#define STRING 258
#define NUMBER 259
#define EVALUATION 260
#define PLIES 261
#define CUBE 262
#define CUBEFUL 263
#define CUBELESS 264
#define NOISE 265
#define REDUCED 266
#define PRUNE 267
#define FIBSBOARD 268
#define AFIBSBOARD 269
#define ON 270
#define OFF 271




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 50 "external_y.y"
typedef union YYSTYPE {
  int number;
  char *sval;
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 75 "external_y.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE extlval;



