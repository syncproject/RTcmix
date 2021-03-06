/* RTcmix  - Copyright (C) 2004  The RTcmix Development Team
   See ``AUTHORS'' for a list of contributors. See ``LICENSE'' for
   the license to this software and for a DISCLAIMER OF ALL WARRANTIES.
*/
#ifndef _MINC_H_
#define _MINC_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

/* Anything here is available for export outside of the Minc directory. */

typedef enum _MincError
{
	MincInstrumentError		=	-1,		// call to instrument or internal routing failed
	MincParserError			=	-2,		// mistake in grammar
	MincSystemError			= 	-3,		// memory error, etc.
	MincInternalError		=	-4		// something wrong internally
} MincError;

int yyparse(void);
int yylex_destroy(void);
void yyset_lineno (int line_number);
int yyget_lineno(void);

void reset_parser();
int configure_minc_error_handler(int exit);
void clear_tree_state();	// The only exported function from Node.cpp

#ifdef __cplusplus
}

// We only throw errors from the parser in embedded systems.  In others we simply exit.
#ifdef EMBEDDED
#define minc_throw(err) throw(err)
#define minc_try try
#define minc_catch(err) catch(err)
#else
#define minc_throw(err)
#define minc_try
#define minc_catch(err) if (0)
#endif

#endif

#endif /* _MINC_H_ */
