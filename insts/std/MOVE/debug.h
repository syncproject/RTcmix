#define DBLOC(loc) fprintf(stderr, "Now at line loc\n");fflush(stderr)
#define DBPRT(var) fprintf(stderr, "var = %8.2f\n", (float) var);fflush(stderr)
#define DBENT(fun) fprintf(stderr, "Entering fun\n");fflush(stderr)
#define DBEXT(fun) fprintf(stderr, "Leaving fun\n");fflush(stderr)
#define DBCOM(comment) fprintf(stderr, "comment\n");fflush(stderr)
