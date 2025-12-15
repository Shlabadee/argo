# Argo Lexer
Argo is an `argv` lexer intended for C and C++ programs. It does not parse the actual values and is intended for well-formed arguments. The programmer is responsible for properly converting strings to integers, floats, and potentially other formats in addition to checking the presence of required flags.

## Features

- Long flags
- Short flags
- Combined short flags
- Help/usage screen
- Error reporting

## Usage
What follows is a basic setup and use for Argo.

```c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "argo.h"

enum
{
	OPTION_HELP,
	OPTION_NUMA,
	OPTION_NUMB
};

int main(int argc, char** argv)
{
	float a, b;
	bool a_set = false, b_set = false;

	ArgoOption options[] = {
	    Argo_Set("hH?", "help", ArgoOptionType_Boolean, "displays this help message and exits"),
	    Argo_Set("aA", "numA", ArgoOptionType_Float, "number A"),
	    Argo_Set("bB", "numB", ArgoOptionType_Float, "number B"),
	};

	ArgoInstance instance;
	ArgoReturnType rt = Argo_Tokenize(&instance, options, sizeof(options) / sizeof(options[0]),
	                                  (size_t)argc, argv,
	                                  true // ignore unknown flags
	);

	if (!rt)
	{
		Argo_PrintError();
		return 1;
	}

	if (options[OPTION_HELP].found)
	{
		Argo_PrintHelp(&instance);
		return 0;
	}

	if (options[OPTION_NUMA].found)
	{
		a = strtol(options[OPTION_NUMA].value, NULL, 10);
		a_set = true;
	}

	if (options[OPTION_NUMB].found)
	{
		b = strtol(options[OPTION_NUMB].value, NULL, 10);
		b_set = true;
	}

	if (instance.unformatted)
	{
		for (size_t i = 0; i < instance.unformatted_size; ++i)
		{
			if (!a_set)
			{
				a = strtol(instance.unformatted[i], NULL, 10);
				a_set = true;
				continue;
			}

			if (!b_set)
			{
				b = strtol(instance.unformatted[i], NULL, 10);
				b_set = true;
				continue;
			}
		}
	}
	else
	{
		puts("No unformatted.");
	}

	if (!a_set || !b_set)
	{
		fprintf(stderr, "Both A and B must be defined.\n");
		return 1;
	}

	printf("%f + %f = %f\n", a, b, a + b);

	return 0;
}
```
Argo's philosophy is to let the programmer decide how data should be parsed and to avoid heap memory allocations. So the program can safely exit without needing to clean up after Argo.
