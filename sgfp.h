typedef union {
    char ach[ 2 ]; /* property identifier */
    char *pch; /* property value */
    property *pp; /* complete property */
    list *pl; /* nodes, sequences, gametrees */
} YYSTYPE;
#define	PROPERTY	257
#define	VALUETEXT	258


extern YYSTYPE sgflval;
