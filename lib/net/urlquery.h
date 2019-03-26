#ifndef YANS_URLQUERY_H__
#define YANS_URLQUERY_H__

/* decodes a URL query part in-place. 'str' will point to the beginning
 * of the decoded string afterwards, a pointer to the end of the decoded
 * string (it's '\0'-byte) will be returned */
char *urlquery_decode(char *str);

/* splits a query string pair pointed at by *str into its key and value
 * part and urldecodes both, in-place. Updates *str to point to the
 * beginning of the next pair, if any. Returns 1 if a pair is parsed or
 * 0 when done */
int urlquery_next_pair(char **str, char **key, char **val);

#endif

