#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <string.h>

#include "argo.h"

typedef enum ArgoErrorType
{
	ArgoErrorType_NoError,
	ArgoErrorType_UnknownFlag,
	ArgoErrorType_InvalidFlag,
	ArgoErrorType_ExpectedFlag,
	ArgoErrorType_ExpectedValue,
	ArgoErrorType_DashAsShort,
	ArgoErrorType_NULLAsShort,
	ArgoErrorType_UnknownError
} ArgoErrorType;

static const char* error_msgs[] = {
    "No error",       "Unknown flag",        "Invalid flag",        "Expected flag",
    "Expected value", "Found dash as short", "Found NULL as short", "Unknown error",
};

typedef enum ArgoFlagType
{
	ArgoFlagType_Long,
	ArgoFlagType_Short,
	ArgoFlagType_Unformatted,
	ArgoFlagType_NotAFlag,
	ArgoFlagType_InvalidFlag
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
	bool combined_flag;
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

static ArgoErrorType h_handle_short_option(ArgoInstance* instance, char* arg, size_t* option_i,
                                           bool ignore_unknown_flags, bool* combined_flag)
{
	char short_name;
	size_t sn_i = 1;
	short_name = arg[sn_i];

	while (short_name != '\0')
	{
		if (h_find_short_option(instance, short_name, option_i) == ArgoReturnType_Failure)
		{
			if (!ignore_unknown_flags)
			{
				error_report.arg_short = short_name;
				error_report.flag_type = ArgoFlagType_Short;
				error_report.arg = arg;
				error_report.type = ArgoErrorType_UnknownFlag;
				return ArgoErrorType_UnknownFlag;
			}

			++sn_i;
			short_name = arg[sn_i];

			continue;
		}

		instance->options[*option_i].found = true;
		++sn_i;
		short_name = arg[sn_i];
	}

	*combined_flag = sn_i > 2 ? true : false;

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
			helper->combined_flag = false;
			error = h_handle_short_option(instance, flag, &helper->option_i,
			                              helper->ignore_unknown_flags, &helper->combined_flag);
			if (error == ArgoErrorType_UnknownFlag)
			{
				error_report.flag_type = ArgoFlagType_Short;
				error_report.arg_pos = helper->argv_i;
				return ArgoErrorType_UnknownFlag;
			}

			if (!helper->combined_flag
			    && instance->options[helper->option_i].type != ArgoOptionType_Boolean)
			{
				helper->mode = ArgoTokenMode_Value;
			}

			return ArgoErrorType_NoError;

			break;
		case ArgoFlagType_Unformatted:
			if (argc - helper->argv_i > 1)
				instance->unformatted_args_begin = &argv[helper->argv_i + 1];
			helper->mode = ArgoTokenMode_Unformatted;
			return ArgoErrorType_NoError;
			break;
		case ArgoFlagType_NotAFlag:
			error_report.type = ArgoErrorType_InvalidFlag;
			error_report.flag_type = ArgoFlagType_NotAFlag;
			error_report.arg_pos = helper->argv_i;
			error_report.arg = argv[helper->argv_i];
			return ArgoErrorType_ExpectedFlag;

			break;
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
	helper.combined_flag = false;

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
	size_t max_flag_len = 0;
	for (size_t i = 0; i < instance->size; ++i)
	{
		ArgoOption* opt = &instance->options[i];
		char shortbuf[64] = {0};
		char temp[256];

		// format multiple short flags as "-a, -b, -c"
		if (opt->short_name != Argo_NoShortFlag && opt->short_name && *opt->short_name)
		{
			const char* sn = opt->short_name;
			for (size_t j = 0; sn[j] != '\0' && j < sizeof(shortbuf) - 4; ++j)
			{
				size_t pos = strlen(shortbuf);
				snprintf(shortbuf + pos, sizeof(shortbuf) - pos, "-%c%s", sn[j],
				         (sn[j + 1] != '\0') ? ", " : "");
			}
		}

		if (opt->short_name != Argo_NoShortFlag && opt->long_name != Argo_NoLongFlag)
			snprintf(temp, sizeof(temp), "  %s, --%s", shortbuf, opt->long_name);
		else if (opt->short_name != Argo_NoShortFlag)
			snprintf(temp, sizeof(temp), "  %s", shortbuf);
		else if (opt->long_name != Argo_NoLongFlag)
			snprintf(temp, sizeof(temp), "      --%s", opt->long_name);
		else
			snprintf(temp, sizeof(temp), "  (none)");

		size_t len = strlen(temp);
		if (opt->type != ArgoOptionType_Boolean)
			len += 8; // space for hints
		if (len > max_flag_len)
			max_flag_len = len;
	}

	// print formatted help lines
	for (size_t i = 0; i < instance->size; ++i)
	{
		ArgoOption* opt = &instance->options[i];
		char shortbuf[64] = {0};
		char flagbuf[256];
		const char* type_hint = "";

		switch (opt->type)
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
		if (opt->short_name != Argo_NoShortFlag && opt->short_name && *opt->short_name)
		{
			const char* sn = opt->short_name;
			for (size_t j = 0; sn[j] != '\0' && j < sizeof(shortbuf) - 4; ++j)
			{
				size_t pos = strlen(shortbuf);
				snprintf(shortbuf + pos, sizeof(shortbuf) - pos, "-%c%s", sn[j],
				         (sn[j + 1] != '\0') ? ", " : "");
			}
		}

		if (opt->short_name != Argo_NoShortFlag && opt->long_name != Argo_NoLongFlag)
			snprintf(flagbuf, sizeof(flagbuf), "  %s, --%s%s", shortbuf, opt->long_name,
			         type_hint);
		else if (opt->short_name != Argo_NoShortFlag)
			snprintf(flagbuf, sizeof(flagbuf), "  %s%s", shortbuf, type_hint);
		else if (opt->long_name != Argo_NoLongFlag)
			snprintf(flagbuf, sizeof(flagbuf), "      --%s%s", opt->long_name, type_hint);
		else
			snprintf(flagbuf, sizeof(flagbuf), "  (none)%s", type_hint);

		printf("%-*s %s\n", (int)max_flag_len, flagbuf,
		       opt->description ? opt->description : "");
	}

	puts("\n");
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
