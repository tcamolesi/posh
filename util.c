#include <stdlib.h>

char* str_unescape(char *str) {
  char *scan, *wrt;

  for(wrt = scan = str; *scan != '\0'; scan++, wrt++) {
    if(*scan == '\\')
      scan++;
    *wrt = *scan;
  }

  *wrt = '\0';

  return str;
}

char* str_unquote(char *str) {
  int i;
  int shift = (str[0] == '"');

  for(i = 1; str[i-1] != '\0'; i++) {
    if(shift) {
      str[i-1] = str[i];
    }
  }

  i-= 3; /*i := strlen(str) - 1*/

  if(str[i] == '"') {
    str[i] = '\0';
  }

  return str;
}

int safe_free(void *ptr) {
  if(ptr) {
    free(ptr);
    return 1;
  }
  return 0;
}
