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
	bool ignore_unknown_flags;
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
                                         size_t* option_i)
{
	for (size_t i = 0; i < instance->size; ++i)
	{
		if (instance->options[i].long_name == Argo_NoLongFlag)
			continue;

		if (strncmp(long_name, instance->options[i].long_name, 255) == 0)
		{
			*option_i = i;
			return ArgoReturnType_Success;
		}
	}

	return ArgoReturnType_Failure;
}

static ArgoErrorType h_handle_long_option(ArgoInstance* instance, char* arg, size_t* option_i,
                                          bool ignore_unknown_flags)
{
	char* long_name = &arg[2];
	ArgoReturnType rt = h_find_long_option(instance, long_name, option_i);

	if (rt == ArgoReturnType_Failure)
	{
		if (ignore_unknown_flags)
			return ArgoErrorType_UnknownFlag;

		error_report.type = ArgoErrorType_UnknownFlag;
		error_report.flag_type = ArgoFlagType_Long;
		error_report.arg = long_name;
		error_report.type = ArgoErrorType_UnknownFlag;
		return ArgoErrorType_UnknownFlag;
	}

	instance->options[*option_i].found = true;
	return ArgoErrorType_NoError;
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
	const bool combined = arg[2] != '\0';
	size_t option_i;

	// Validate the whole group before marking any of its options as found.
	for (char* name = arg + 1; *name; ++name)
	{
		ArgoErrorType error;
		if (h_find_short_option(instance, *name, &option_i) == ArgoReturnType_Failure)
		{
			if (helper->ignore_unknown_flags)
				continue;
			error = ArgoErrorType_UnknownFlag;
		}
		else if (combined && instance->options[option_i].type != ArgoOptionType_Boolean)
			error = ArgoErrorType_NonBooleanInCompound;
		else
			continue;

		error_report.type = error;
		error_report.flag_type = ArgoFlagType_Short;
		error_report.arg_pos = helper->argv_i;
		error_report.arg = arg;
		error_report.arg_short = *name;
		return error;
	}

	for (char* name = arg + 1; *name; ++name)
	{
		if (h_find_short_option(instance, *name, &option_i) == ArgoReturnType_Failure)
			continue;

		instance->options[option_i].found = true;
		if (instance->options[option_i].type != ArgoOptionType_Boolean)
		{
			helper->option_i = option_i;
			helper->mode = ArgoTokenMode_Value;
		}
	}

	return ArgoErrorType_NoError;
}

static ArgoErrorType h_handle_flag(ArgoInstance* instance, size_t argc, char** argv,
                                   ArgoDataHelper* helper)
{
	char* flag = argv[helper->argv_i];
	switch (h_get_flag_type(flag))
	{
		ArgoErrorType error;

		case ArgoFlagType_Long:
			error = h_handle_long_option(instance, flag, &helper->option_i,
			                             helper->ignore_unknown_flags);
			if (error == ArgoErrorType_UnknownFlag)
			{
				if (!helper->ignore_unknown_flags)
				{
					error_report.flag_type = ArgoFlagType_Long;
					return ArgoErrorType_UnknownFlag;
				}

				helper->mode = ArgoTokenMode_Flag;
			}
			else
			{
				if (instance->options[helper->option_i].type != ArgoOptionType_Boolean)
					helper->mode = ArgoTokenMode_Value;
			}

			return ArgoErrorType_NoError;
			break;
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
			break;
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
                             size_t argc, char** argv, bool ignore_unknown_flags)
{
	ArgoDataHelper helper;
	helper.mode = ArgoTokenMode_Flag;
	helper.argv_i = 0;
	helper.option_i = 0;
	helper.ignore_unknown_flags = ignore_unknown_flags;
	instance->size = size;
	instance->options = options;
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

		printf("%-*s %s\n", (int)max_flag_length, flag_buffer,
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

#if defined __linux__ || defined __APPLE__ || defined __FreeBSD__
	#include <sys/ioctl.h>
	#include <unistd.h>

int Argo_GetTerminalSize(void)
{
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
	{
		if (ws.ws_col > 0)
			return ws.ws_col;
	}
	return 80; // fallback
}
#else
	#ifdef _WIN32
		#include <windows.h>

int Argo_GetTerminalSize(void)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	if (hStdOut != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hStdOut, &csbi))
		return (csbi.srWindow.Right - csbi.srWindow.Left) + 1;

	return 80; // fallback
}
	#endif
#endif
