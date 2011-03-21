#ifndef UTIL_H_DEFINED_2346614573415316
#define UTIL_H_DEFINED_2346614573415316

/**
* Unescapes a string, according to the C convention
* \param The string to be processed
* \return The unescaped string
*/
char* str_unescape(char *str);

/**
* Removes quotes surrounding a string
* \param The string to be processed
* \return The unquoted string
*/
char* str_unquote(char *str);

/**
* Frees a pointer allocated with malloc, if it's not NULL
* \param ptr The pointer to be freed
* \return 1 if the pointer was deallocated, 0 otherwise
*/
int safe_free(void *ptr);

#endif
