#ifndef FILE_UTILS_H
#define FILE_UTILS_H

void find_and_append_to_file(const char *path, const char *find, const char *append);
char *find_and_extract_line_data(const char *path, const char *find);
char *extract_data_between_markers(const char *path, const char *from, const char *to);
char *extract_lines_between_markers(const char *path, const char *from, const char *to);

#endif
