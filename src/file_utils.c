#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../includes/file_utils.h"

/**
 * @brief Appends content to lines in a file that contain a specified pattern.
 *
 * This function searches for a substring `find` in each line of the file specified
 * by `path`. If the substring is found within a line, the function appends the content
 * of `append` immediately after `find` in that line. The modified content is
 * written back to the original file.
 *
 * @param path Path to the input file.
 * @param find Substring to search for in each line of the file.
 * @param append Content to append immediately after `find` if `find` is found.
 *
 * @note The function modifies the file in place. A temporary file is used during the
 *       process to store the modified content, which is then written back to the
 *       original file.
 *
 * @warning If the function cannot open the input file or create the temporary file,
 *          it prints an error message to `stdout` and exits without modifying the file.
 *
 * @warning This function assumes that the file is not too large to be processed in memory.
 *
 * @note The function rewinds both the original and temporary files to the beginning
 *       before writing the modified content back to the original file.
 */
void find_and_append_data(const char *path, const char *find, const char *append)
{
    FILE *file = fopen(path, "r+");
    if (file == NULL)
    {
        printf("ERROR - file=NULL - Path: %s", path);
        return;
    }
    FILE *file_tmp = tmpfile();
    if (file_tmp == NULL)
    {
        printf("ERROR - file_tmp=NULL");
        fclose(file);
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char *position = strstr(line, find);
        if (position != NULL)
        {
            fwrite(line, 1, position - line + strlen(find), file_tmp);
            fwrite(append, 1, strlen(append), file_tmp);
            fputc('\n', file_tmp);
        }
        else
        {
            fputs(line, file_tmp);
        }
    }
    rewind(file);
    rewind(file_tmp);
    while (fgets(line, sizeof(line), file_tmp) != NULL)
    {
        fputs(line, file);
    }
    ftruncate(fileno(file), ftell(file));
    fclose(file);
    fclose(file_tmp);
}

/**
 * @brief Searches for a substring in a file and returns the remaining part of the line.
 *
 * This function searches for the first occurrence of the substring `find` in
 * each line of the file specified by `path`. When `find` is found, the
 * function returns the rest of the line after `find`. If the line ends with
 * a newline character (`\n`), it is removed from the returned string. The
 * search stops after the first match is found.
 *
 * @param path Path to the input file.
 * @param find Substring to search for in each line of the file.
 * @return A pointer to a dynamically allocated string containing the part of
 *         the line that comes after `find`. Returns `NULL` if the file
 *         cannot be opened, if `find` is not found, or if memory allocation fails.
 *
 * @note The caller is responsible for freeing the memory allocated for the
 *       returned string using `free()` when it is no longer needed.
 *
 * @warning If the function fails to open the file or allocate memory, it
 *          prints an error message to `stdout` and returns `NULL`.
 *
 * @warning This function only returns the first occurrence of `find` in the
 *          file. If there are multiple occurrences, only the first one is considered.
 */
char *find_and_return_inline_data(const char *path, const char *find)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("ERROR - file=NULL - Path: %s", path);
        return NULL;
    }
    char line[256];
    while (fgets(line, sizeof(line), file))
    {
        char *found = strstr(line, find);
        if (found)
        {
            found += strlen(find);
            fclose(file);
            size_t len = strlen(found);
            if (len > 0 && found[len - 1] == '\n')
            {
                found[len - 1] = '\0';
            }
            char *output = malloc(len + 1);
            if (output == NULL)
            {
                printf("ERROR - output=NULL - Allocation memory");
                return NULL;
            }
            strcpy(output, found);
            return output;
        }
    }
    fclose(file);
    return NULL;
}

/**
 * @brief Extracts and returns a substring from a file between two specified markers.
 *
 * This function searches for a substring in the file specified by `path` that is
 * located between the strings `from` and `to` within the same line. If both
 * markers are found on a line, the function extracts and returns the substring
 * between them. The function reads the file line by line, stopping after the
 * first occurrence of the pattern is found.
 *
 * @param path Path to the input file.
 * @param from The starting marker substring to search for.
 * @param to The ending marker substring to search for after `from`.
 * @return A pointer to a dynamically allocated string containing the substring
 *         found between `from` and `to`. Returns `NULL` if the file cannot be
 *         opened, if the markers are not found, or if memory allocation fails.
 *
 * @note The caller is responsible for freeing the memory allocated for the
 *       returned string using `free()` when it is no longer needed.
 *
 * @warning This function assumes that both `from` and `to` are on the same line.
 *          If `from` or `to` are not found on the same line, or if the file is
 *          empty, the function returns `NULL` without making any changes to the
 *          output file.
 *
 * @warning If there is an error in opening the file or allocating memory,
 *          the function prints an error message to `stdout` and returns `NULL`.
 */
char *return_inline_data_from_to(const char *path, const char *from, const char *to)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("ERROR - file=NULL - Path: %s", path);
        return NULL;
    }
    char *output = NULL;
    char line[1024];
    while (fgets(line, sizeof(line), file))
    {
        char *start = strstr(line, from);
        if (start)
        {
            start += strlen(from);
            char *end = strstr(start, to);
            if (end)
            {
                size_t length = end - start;
                output = malloc(length + 1);
                if (output == NULL)
                {
                    printf("ERROR - output=NULL - Allocation memory");
                    fclose(file);
                    return NULL;
                }
                strncpy(output, start, length);
                output[length] = '\0';
                break;
            }
        }
    }
    fclose(file);
    return output;
}

/**
 * @brief Extracts lines from a file between two specified markers.
 *
 * This function reads a file line by line and extracts all lines starting from the line
 * containing the substring `from` until the line containing the substring `to` is found.
 * The function returns a dynamically allocated string containing all the extracted lines.
 * If `to` is not found after `from`, the function will return `NULL`. If either `from`
 * or `to` is not found, or if there is an error in memory allocation, it will also return `NULL`.
 *
 * @param path Path to the input file.
 * @param from Substring that indicates the start of the extraction range.
 * @param to Substring that indicates the end of the extraction range.
 * @return A pointer to a dynamically allocated string containing the lines from `from`
 *         up to, but not including, the line containing `to`. Returns `NULL` if the file
 *         cannot be opened, if memory allocation fails, or if the extraction range is invalid.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned string
 *       using `free()` when it is no longer needed.
 *
 * @warning The function assumes that the markers `from` and `to` are on separate lines.
 *          If `from` and `to` are on the same line, or if `to` is not found after `from`,
 *          the function will return `NULL`.
 *
 * @warning If there is an error in opening the file or allocating memory, the function
 *          prints an error message to `stdout` and returns `NULL`.
 */
char *return_lines_from_to(const char *path, const char *from, const char *to)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        printf("ERROR - file=NULL - Path: %s\n", path);
        return NULL;
    }
    char *output = NULL;
    size_t output_size = 0;
    size_t output_length = 0;
    char line[1024];
    int recording = 0;
    while (fgets(line, sizeof(line), file))
    {
        if (recording)
        {
            if (strstr(line, to))
            {
                break; // Stop recording when 'to' is found
            }
            size_t line_length = strlen(line);
            char *new_result = realloc(output, output_size + line_length + 1);
            if (new_result == NULL)
            {
                printf("ERROR - result=NULL - Allocation memory\n");
                free(output);
                fclose(file);
                return NULL;
            }
            output = new_result;
            memcpy(output + output_size, line, line_length + 1);
            output_size += line_length;
            output_length += line_length;
        }
        else if (strstr(line, from))
        {
            recording = 1;
        }
    }
    fclose(file);
    if (output && output_length > 0 && output[output_length - 1] == '\n')
    {
        output[output_length - 1] = '\0';
    }
    return output;
}
