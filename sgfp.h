#ifndef BISON_SGFP_H
# define BISON_SGFP_H

#ifndef YYSTYPE
typedef union {
    char ach[ 2 ]; /* property identifier */
    char *pch; /* property value */
    property *pp; /* complete property */
    list *pl; /* nodes, sequences, gametrees */
} yystype;
# define YYSTYPE yystype
#endif
# define	PROPERTY	257
# define	VALUETEXT	258


extern YYSTYPE sgflval;

#endif /* not BISON_SGFP_H */
