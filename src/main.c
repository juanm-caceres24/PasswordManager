#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include "../includes/fil_eutils.h"
#include "file_utils.c"

int main(int argc, char *argv[])
{
    find_and_append_data("../files/test.txt", "name=", "NAME_GENERIC");
    printf("%s\n", find_and_return_inline_data("../files/test.txt", "name="));
    printf("%s\n", return_inline_data_from_to("../files/test.txt", "list=[", "]"));
    printf("%s\n", return_lines_from_to("../files/test.txt", "values={", "}=values"));
    return 0;
}
