#ifndef ARGO_TOKENIZER_H
#define ARGO_TOKENIZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define Argo_NoShortFlag '\0'
#define Argo_NoLongFlag NULL

typedef enum ArgoReturnType
{
	ArgoReturnType_Failure,
	ArgoReturnType_Success,
} ArgoReturnType;

typedef enum ArgoOptionType
{
	ArgoOptionType_Boolean,
	ArgoOptionType_Integer,
	ArgoOptionType_Float,
	ArgoOptionType_String,
} ArgoOptionType;

typedef struct ArgoOption
{
	char short_name;
	char* long_name;
	ArgoOptionType type;
	char* description;
	bool found;
	char* value;
} ArgoOption;

#define Argo_Set(_short_name, _long_name, _type, _description)                                 \
	{                                                                                          \
		.short_name = _short_name, .long_name = _long_name, .type = _type,                     \
		.description = _description, false, NULL,                                              \
	}

typedef struct ArgoInstance
{
	ArgoOption* options;
	size_t size;
	char** unformatted_args_begin;
	uint8_t help_size;
} ArgoInstance;

ArgoReturnType Argo_Tokenize(ArgoInstance* instance, ArgoOption* options, size_t size,
                             size_t argc, char** argv, bool ignore_unknown_flags);
ArgoReturnType Argo_PrintHelp(ArgoInstance* instance);
void Argo_PrintError();

#ifdef __cplusplus
}
#endif

#endif // ARGO_TOKENIZER_H
