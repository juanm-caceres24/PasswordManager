#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// #include "../include/file_utils.h"
// #include "../include/user_interface.h"
#include "file_utils.c"
#include "user_interface.c"

int main(int argc, char *argv[])
{
    int menu_id = 0;
    int function_id = 0;
    while (1)
    {
        function_id = get_menu(&menu_id);
    }
    return 0;
}
