#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <string.h>

#include "argo.h"

typedef enum ArgoErrorType
{
	ArgoErrorType_NoError,
	ArgoErrorType_UnknownFlag,
	ArgoErrorType_ExpectedValue,
	ArgoErrorType_DashAsShort,
	ArgoErrorType_NULLAsShort,
	ArgoErrorType_NonBooleanInCompound,
	ArgoErrorType_UnexpectedAssignment,
	ArgoErrorType_UnknownError,
	ArgoErrorType_Enum_Size
} ArgoErrorType;

static const char* error_msgs[ArgoErrorType_Enum_Size] = {
    "No error",
    "Unknown flag",
    "Expected value",
    "Found dash as short",
    "Found NULL as short",
    "Combined short flags must all be boolean",
    "Assignment requires a single value-taking option",
    "Unknown error",
};

typedef enum ArgoFlagType
{
	ArgoFlagType_Long,
	ArgoFlagType_Short,
	ArgoFlagType_Unformatted,
	ArgoFlagType_NotAFlag,
} ArgoFlagType;

typedef enum ArgoTokenMode
{
	ArgoTokenMode_Flag,
	ArgoTokenMode_Value,
	ArgoTokenMode_Unformatted,
} ArgoTokenMode;

typedef struct ArgoErrorReport
{
	ArgoErrorType type;
	ArgoFlagType flag_type;
	size_t arg_pos;
	char* arg;
	char arg_short;
} ArgoErrorReport;

static ArgoErrorReport error_report;

typedef struct ArgoDataHelper
{
	size_t argv_i;
	size_t option_i;
	ArgoTokenMode mode;
	uint32_t argo_flags;
	bool store_value;
} ArgoDataHelper;

static ArgoFlagType h_get_flag_type(char* arg)
{
	if (arg[0] == '-')
	{
		if (arg[1] == '-')
		{
			if (arg[2] == '-')
				return ArgoFlagType_NotAFlag;
			else if (arg[2] == '\0')
				return ArgoFlagType_Unformatted;

			return ArgoFlagType_Long;
		}

		if (arg[1] == '\0')
			return ArgoFlagType_NotAFlag;

		return ArgoFlagType_Short;
	}

	return ArgoFlagType_NotAFlag;
}

static ArgoReturnType h_find_long_option(ArgoInstance* instance, char* long_name,
                                         size_t name_length, size_t* option_i)
{
	for (size_t i = 0; i < instance->size; ++i)
	{
		if (instance->options[i].long_name == Argo_NoLongFlag)
			continue;

		if (strlen(instance->options[i].long_name) == name_length
		    && memcmp(long_name, instance->options[i].long_name, name_length) == 0)
		{
			*option_i = i;
			return ArgoReturnType_Success;
		}
	}

	return ArgoReturnType_Failure;
}

static void h_accept_option(ArgoInstance* instance, size_t option_i, char* equals,
                            ArgoDataHelper* helper)
{
	ArgoOption* option = &instance->options[option_i];
	uint32_t policy = option->flags & (ARGO_FIRST_IN | ARGO_LAST_IN);

	if (!policy)
		policy = helper->argo_flags & (ARGO_FIRST_IN | ARGO_LAST_IN);

	helper->store_value = !option->found || policy != ARGO_FIRST_IN;
	option->found = true;

	if (equals)
	{
		if (helper->store_value)
			option->value = &equals[1];
	}
	else if (option->type != ArgoOptionType_Boolean)
	{
		helper->option_i = option_i;
		helper->mode = ArgoTokenMode_Value;
	}
}

static ArgoErrorType h_handle_long_option(ArgoInstance* instance, char* arg,
                                          ArgoDataHelper* helper)
{
	char* name = &arg[2];
	char* equals = strchr(name, '=');
	size_t length = equals ? (size_t)(equals - name) : strlen(name);
	size_t option_i;

	ArgoErrorType error;
	if (length == 0
	    || h_find_long_option(instance, name, length, &option_i) == ArgoReturnType_Failure)
	{
		if (helper->argo_flags & ARGO_IGNORE_UNKNOWN_FLAGS)
			return ArgoErrorType_NoError;

		error = ArgoErrorType_UnknownFlag;
	}
	else if (equals && instance->options[option_i].type == ArgoOptionType_Boolean)
	{
		error = ArgoErrorType_UnexpectedAssignment;
	}
	else
	{
		h_accept_option(instance, option_i, equals, helper);
		return ArgoErrorType_NoError;
	}

	error_report.type = error;
	error_report.flag_type = ArgoFlagType_Long;
	error_report.arg = arg;
	error_report.arg_pos = helper->argv_i;
	return error;
}

static ArgoReturnType h_find_short_option(ArgoInstance* instance, char arg, size_t* option_i)
{
	for (size_t i = 0; i < instance->size; ++i)
	{
		if (instance->options[i].short_name == Argo_NoShortFlag)
			continue;

		// if arg is in option short name
		if (strchr(instance->options[i].short_name, arg))
		{
			*option_i = i;
			return ArgoReturnType_Success;
		}
	}

	return ArgoReturnType_Failure;
}

static ArgoErrorType h_handle_short_option(ArgoInstance* instance, char* arg,
                                           ArgoDataHelper* helper)
{
	char* equals = strchr(&arg[2], '=');
	char* end = equals ? equals : &arg[strlen(arg)];
	const bool combined = end - (&arg[1]) > 1;
	size_t option_i;

	// Validate the whole group before marking any of its options as found.
	for (char* name = &arg[1]; name < end; ++name)
	{
		ArgoErrorType error;
		if (h_find_short_option(instance, *name, &option_i) == ArgoReturnType_Failure)
		{
			if (helper->argo_flags & ARGO_IGNORE_UNKNOWN_FLAGS)
				continue;

			error = ArgoErrorType_UnknownFlag;
		}
		else if (combined && instance->options[option_i].type != ArgoOptionType_Boolean)
		{
			error = ArgoErrorType_NonBooleanInCompound;
		}
		else if (equals
		         && (combined || instance->options[option_i].type == ArgoOptionType_Boolean))
		{
			error = ArgoErrorType_UnexpectedAssignment;
		}
		else
		{
			continue;
		}

		error_report.type = error;
		error_report.flag_type = ArgoFlagType_Short;
		error_report.arg_pos = helper->argv_i;
		error_report.arg = arg;
		error_report.arg_short = *name;
		return error;
	}

	for (char* name = &arg[1]; name < end; ++name)
	{
		if (h_find_short_option(instance, *name, &option_i) == ArgoReturnType_Failure)
			continue;

		h_accept_option(instance, option_i, equals, helper);
	}

	return ArgoErrorType_NoError;
}

static ArgoErrorType h_handle_flag(ArgoInstance* instance, size_t argc, char** argv,
                                   ArgoDataHelper* helper)
{
	char* flag = argv[helper->argv_i];

	switch (h_get_flag_type(flag))
	{
		case ArgoFlagType_Long:
			return h_handle_long_option(instance, flag, helper);

		case ArgoFlagType_Short:
			return h_handle_short_option(instance, flag, helper);

		case ArgoFlagType_Unformatted:
			if (argc - helper->argv_i > 1)
			{
				instance->unformatted = &argv[helper->argv_i + 1];
				instance->unformatted_size = argc - (helper->argv_i + 1);
			}

			helper->mode = ArgoTokenMode_Unformatted;
			return ArgoErrorType_NoError;

		case ArgoFlagType_NotAFlag:
			if (instance->positionals
			    && instance->positional_size < instance->positional_capacity)
			{
				instance->positionals[instance->positional_size++] = flag;

				if (instance->positional_size < instance->positional_capacity)
					instance->positionals[instance->positional_size] = NULL;
			}

			return ArgoErrorType_NoError;

		default:
			break;
	}

	return ArgoErrorType_UnknownError;
}

ArgoReturnType Argo_Tokenize(ArgoInstance* instance, ArgoOption* options, size_t size,
                             size_t argc, char** argv, uint32_t argo_flags)
{
	ArgoDataHelper helper;
	helper.mode = ArgoTokenMode_Flag;
	helper.argv_i = 0;
	helper.option_i = 0;
	helper.argo_flags = argo_flags;
	instance->size = size;
	instance->options = options;

	for (size_t i = 0; i < size; ++i)
	{
		options[i].found = false;
		options[i].value = NULL;
	}

	instance->unformatted = NULL;
	instance->unformatted_size = 0;
	instance->positional_size = 0;

	if (instance->positionals && instance->positional_capacity)
		instance->positionals[0] = NULL;

	error_report.arg = NULL;
	error_report.arg_short = '\0';
	error_report.type = ArgoErrorType_NoError;
	error_report.flag_type = ArgoFlagType_NotAFlag;

	for (helper.argv_i = 1; helper.argv_i < argc; ++helper.argv_i)
	{
		if (helper.mode == ArgoTokenMode_Flag)
		{
			ArgoErrorType error = h_handle_flag(instance, argc, argv, &helper);

			if (error != ArgoErrorType_NoError)
				return ArgoReturnType_Failure;
		}
		else if (helper.mode == ArgoTokenMode_Value)
		{
			if (helper.store_value)
				instance->options[helper.option_i].value = argv[helper.argv_i];

			helper.mode = ArgoTokenMode_Flag;
		}
		else // ArgoTokenMode_Unformatted
		{
			return ArgoReturnType_Success;
		}
	}

	if (helper.mode == ArgoTokenMode_Value)
	{
		error_report.type = ArgoErrorType_ExpectedValue;
		error_report.flag_type = ArgoFlagType_Long;
		error_report.arg_pos = helper.argv_i - 1;
		error_report.arg = argv[helper.argv_i - 1];
		return ArgoReturnType_Failure;
	}

	return ArgoReturnType_Success;
}

ArgoReturnType Argo_PrintHelp(ArgoInstance* instance)
{
	// calculate maximum width for flag column
	size_t max_flag_length = 0;
	for (size_t i = 0; i < instance->size; ++i)
	{
		ArgoOption* option = &instance->options[i];
		char short_buffer[64] = {0};
		char temp[256];

		// format multiple short flags as "-a, -b, -c"
		if (option->short_name != Argo_NoShortFlag && option->short_name && *option->short_name)
		{
			const char* short_name = option->short_name;

			for (size_t j = 0; short_name[j] != '\0' && j < sizeof(short_buffer) - 4; ++j)
			{
				size_t pos = strlen(short_buffer);
				snprintf(short_buffer + pos, sizeof(short_buffer) - pos, "-%c%s", short_name[j],
				         (short_name[j + 1] != '\0') ? ", " : "");
			}
		}

		if (option->short_name != Argo_NoShortFlag && option->long_name != Argo_NoLongFlag)
			snprintf(temp, sizeof(temp), "  %s, --%s", short_buffer, option->long_name);
		else if (option->short_name != Argo_NoShortFlag)
			snprintf(temp, sizeof(temp), "  %s", short_buffer);
		else if (option->long_name != Argo_NoLongFlag)
			snprintf(temp, sizeof(temp), "      --%s", option->long_name);
		else
			snprintf(temp, sizeof(temp), "  (none)");

		size_t length = strlen(temp);

		if (option->type != ArgoOptionType_Boolean)
			length += 8; // space for hints

		if (length > max_flag_length)
			max_flag_length = length;
	}

	// print formatted help lines
	for (size_t i = 0; i < instance->size; ++i)
	{
		ArgoOption* option = &instance->options[i];
		char short_buffer[64] = {0};
		char flag_buffer[256];
		const char* type_hint = "";

		switch (option->type)
		{
			case ArgoOptionType_Integer:
				type_hint = " <int>";
				break;
			case ArgoOptionType_Float:
				type_hint = " <num>";
				break;
			case ArgoOptionType_String:
				type_hint = " <str>";
				break;
			default:
				break; // Boolean flags show no hint
		}

		// same multi-short handling as above
		if (option->short_name != Argo_NoShortFlag && option->short_name && *option->short_name)
		{
			const char* short_name = option->short_name;
			for (size_t j = 0; short_name[j] != '\0' && j < sizeof(short_buffer) - 4; ++j)
			{
				size_t pos = strlen(short_buffer);
				snprintf(short_buffer + pos, sizeof(short_buffer) - pos, "-%c%s", short_name[j],
				         short_name[j + 1] != '\0' ? ", " : "");
			}
		}

		if (option->short_name != Argo_NoShortFlag && option->long_name != Argo_NoLongFlag)
		{
			snprintf(flag_buffer, sizeof(flag_buffer), "  %s, --%s%s", short_buffer,
			         option->long_name, type_hint);
		}
		else if (option->short_name != Argo_NoShortFlag)
		{
			snprintf(flag_buffer, sizeof(flag_buffer), "  %s%s", short_buffer, type_hint);
		}
		else if (option->long_name != Argo_NoLongFlag)
		{
			snprintf(flag_buffer, sizeof(flag_buffer), "      --%s%s", option->long_name,
			         type_hint);
		}
		else
		{
			snprintf(flag_buffer, sizeof(flag_buffer), "  (none)%s", type_hint);
		}

		printf("%-*s %s%s\n", (int)max_flag_length, flag_buffer,
		       option->flags & ARGO_REQUIRED ? "[required] " : "",
		       option->description ? option->description : "");
	}

	return ArgoReturnType_Success;
}

void Argo_PrintError(void)
{
	const char* error_prefix = "Argo State: ";

	if (error_report.type == ArgoErrorType_NoError)
	{
		printf("%s%s\n", error_prefix, error_msgs[error_report.type]);
		return;
	}

	if (error_report.type == ArgoErrorType_DashAsShort
	    || error_report.type == ArgoErrorType_NULLAsShort)
	{
		printf("%s%s (contact program developer)\n", error_prefix,
		       error_msgs[error_report.type]);
		return;
	}

	printf("%s%s (argv %lu, ", error_prefix, error_msgs[error_report.type],
	       error_report.arg_pos);

	if (error_report.flag_type == ArgoFlagType_Short)
		printf("%c, %s)\n", error_report.arg_short, error_report.arg);
	else
		printf("%s)\n", error_report.arg);
}
