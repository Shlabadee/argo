#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "argo.h"

typedef enum ErrorMsgID
{
	ErrorMsgID_NoError = 0,
	ErrorMsgID_MemAlloc = 1,
	ErrorMsgId_InvalidOption = 2,
	ErrorMsgID_ExpectedFlag = 3,
	ErrorMsgID_ExpectedValue = 4,
	ErrorMsgID_SameShortName = 5,
	ErrorMsgID_SameLongName = 6,
	ErrorMsgID_RequiredNotFound = 7,
} ErrorMsgID;

static ErrorMsgID error_msg_id = ErrorMsgID_NoError;

static const char* error_msgs[] = {
    "No error",       "Failed to allocate memory", "Invalid option", "Expected flag",
    "Expected value",
};

typedef struct ArgoErrorPosition
{
	int arg_token, argv_pos;
} ArgoErrorPosition;

typedef struct ArgoError
{
	ErrorMsgID type;
	ArgoErrorPosition arg_pos;
	size_t option_idx_a, option_idx_b;
	char msg[256];
} ArgoError;

ArgoError error_report = {0};

static bool h_valid_options(ArgoOption* options, size_t size)
{
	for (size_t a = 0; a < size; ++a)
	{
		for (size_t b = a + 1; b < size; ++b)
		{
			if (options[a].short_name == options[b].short_name)
			{
				error_report.type = ErrorMsgID_SameShortName;
				error_report.option_idx_a = a;
				error_report.option_idx_b = b;
				return false;
			}

			if (strncmp(options[a].long_name, options[b].long_name, 255) == 0)
			{
				error_report.type = ErrorMsgID_SameLongName;
				error_report.option_idx_a = a;
				error_report.option_idx_b = b;
				return false;
			}
		}
	}

	return true;
}

static size_t h_get_missing_required(ArgoInstance* instance)
{
	for (size_t i = 0; i < instance->size; ++i)
	{
		if (instance->options[i].required && !instance->options[i].found)
		{
			error_report.type = ErrorMsgID_RequiredNotFound;
			error_report.option_idx_a = i;
			return i;
		}
	}

	return -1;
}

bool Argo_Tokenize(ArgoInstance* instance, ArgoOption* options, size_t size, int argc,
                   char** argv)
{
	if (!h_valid_options(options, size))
		return false;

	instance->size = size;
	instance->options = options;
	instance->unformatted_args_begin = NULL;

	for (size_t i = 0; i < size; ++i)
	{
		options[i].found = false;
		options[i].value = NULL;
	}

	ArgoOption* awaiting_value = NULL;

	for (int i = 1; i < argc; ++i)
	{
		char* token = argv[i];

		// handle pending value first
		if (awaiting_value)
		{
			awaiting_value->value = token;
			awaiting_value = NULL;
			continue;
		}

		// end of options marker
		if (strcmp(token, "--") == 0)
		{
			if (i + 1 < argc)
				instance->unformatted_args_begin = &argv[i + 1];
			break;
		}

		// must start with '-'
		if (token[0] != '-')
		{
			error_report.type = ErrorMsgID_ExpectedFlag;
			error_report.arg_pos.argv_pos = i;
			return false;
		}

		if (token[1] == '-')
		{
			// Long option
			const char* name = token + 2;
			bool matched = false;
			for (size_t j = 0; j < size; ++j)
			{
				if (strcmp(name, options[j].long_name) == 0)
				{
					options[j].found = true;
					matched = true;
					// If it’s not boolean, expect value next
					if (options[j].type != ArgoOptionType_Boolean)
						awaiting_value = &options[j];
					break;
				}
			}
			if (!matched)
			{
				error_report.type = ErrorMsgId_InvalidOption;
				error_report.arg_pos.argv_pos = i;
				return false;
			}
		}
		else
		{
			// Short cluster, e.g. -abc
			for (int c = 1; token[c] != '\0'; ++c)
			{
				char name = token[c];
				bool matched = false;
				for (size_t j = 0; j < size; ++j)
				{
					if (name == options[j].short_name)
					{
						options[j].found = true;
						matched = true;
						// if non-boolean and not last char => error
						if (options[j].type != ArgoOptionType_Boolean)
						{
							if (token[c + 1] != '\0')
							{
								error_report.type = ErrorMsgID_ExpectedValue;
								error_report.arg_pos.argv_pos = i;
								return false;
							}
							awaiting_value = &options[j];
						}
						break;
					}
				}
				if (!matched)
				{
					error_report.type = ErrorMsgId_InvalidOption;
					error_report.arg_pos.argv_pos = i;
					return false;
				}
			}
		}
	}

	// expected a value but args ended
	if (awaiting_value)
	{
		error_report.type = ErrorMsgID_ExpectedValue;
		return false;
	}

	if (h_get_missing_required(instance) != (size_t)-1)
		return false;

	return true;
}

void Argo_PrintError(ArgoInstance* instance)
{
	switch (error_report.type)
	{
		case ErrorMsgID_ExpectedFlag:
			printf("Argo Error: Expected flag at idx %i\n", error_report.arg_pos.argv_pos);
			break;
		case ErrorMsgID_ExpectedValue:
			printf("Argo Error: Expected value at idx %i\n", error_report.arg_pos.argv_pos);
			break;
		case ErrorMsgID_RequiredNotFound:
			printf("Argo Error: required flag `%s` not found\n",
			       instance->options[error_report.option_idx_a].long_name);
			break;
		case ErrorMsgID_SameLongName:
			printf("Argo Error: Same long name `%s` at %li and %li\n",
			       instance->options[error_report.option_idx_a].long_name,
			       error_report.option_idx_a, error_report.option_idx_b);
			break;
		case ErrorMsgID_SameShortName:
			printf("Argo Error: Same short name `%c` at %li and %li\n",
			       instance->options[error_report.option_idx_a].short_name,
			       error_report.option_idx_a, error_report.option_idx_b);
			break;
		default:
			break;
	}
}
