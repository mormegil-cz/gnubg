#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union {
  int number;
  char *sval;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	STRING	257
# define	NUMBER	258
# define	EVALUATION	259
# define	PLIES	260
# define	CUBE	261
# define	CUBEFUL	262
# define	CUBELESS	263
# define	NOISE	264
# define	REDUCED	265
# define	PRUNE	266
# define	FIBSBOARD	267
# define	AFIBSBOARD	268
# define	ON	269
# define	OFF	270


extern YYSTYPE extlval;

#endif /* not BISON_Y_TAB_H */
