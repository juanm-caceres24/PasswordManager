#ifndef FILE_UTILS_H
#define FILE_UTILS_H

void find_and_append_data(const char *path, const char *find, const char *append);
char *find_and_return_inline_data(const char *path, const char *find);
char *return_inline_data_from_to(const char *path, const char *from, const char *to);
char *return_lines_from_to(const char *path, const char *from, const char *to);

#endif
