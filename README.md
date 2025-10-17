# Argo Lexer
Argo is an `argv` lexer intended for C and C++ programs. It does not parse the actual values and is intended for well-formed arguments. The programmer is responsible for properly converting strings to integers, floats, and potentially other formats in addition to checking the presence of required flags.

## Usage
What follows is a basic setup and use for Argo.

```c
#include <stdio.h>
#include <stdlib.h>

#include "argo.c"

#define OPTION_HELP 0
#define OPTION_NUMA 1
#define OPTION_NUMB 2

int main(int argc, char** argv)
{
    float a, b;

    ArgoOption options[] =
    {
        Argo_Set('h', "help", ArgoOptionType_Boolean,
                 "displays this help message and exits"),
        Argo_Set('a', "numA", ArgoOptionType_Float, "number A"),
        Argo_Set('b', "numB", ArgoOptionType_Float, "number B"),
    };

    ArgoInstance instance;
    ArgoReturnType rt = Argo_Tokenize(
        &instance,
        options,
        sizeof(options) / sizeof(options[0]),
        (size_t)argc,
        argv,
        true // ignore unknown flags
    );

    if (rt == ArgoReturnType_Failure)
    {
        Argo_PrintError();
        return 1;
    }

    if (options[OPTION_HELP].found)
    {
        Argo_Help(&instance);
        return 0;
    }

    if (!options[OPTION_NUMA].found)
    {
        fprintf(stderr, "-a is required.\n");
        return 1;
    }

    if (!options[OPTION_NUMB].found)
    {
        fprintf(stderr, "-b is required.\n");
        return 1;
    }

    a = strtof(options[OPTION_NUMA].value, NULL, 10);
    b = strtof(options[OPTION_NUMB].value, NULL, 10);

    printf("%f + %f = %f\n", a, b, a + b);

    return 0;
}
```
Argo's philosophy is to let the programmer decide how data should be parsed and to avoid heap memory allocations. So the program can safely exit without needing to clean up after Argo.