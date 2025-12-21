/*
 * utils.h - Makra i deklaracje pomocnicze
 * 
 * ERR - Makro do obsługi błędów (perror + exit)
 * usage - Funkcja help
 */

#include <errno.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage();
void print_help();