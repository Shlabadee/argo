#ifndef ARGO_PARSER_H
#define ARGO_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

// Purely for cosmetic reasons
typedef enum ArgoOptionType
{
	ArgoOptionType_Boolean,
	ArgoOptionType_Integer,
	ArgoOptionType_Float,
	ArgoOptionType_String
} ArgoOptionType;

typedef struct ArgoOption
{
	char short_name;
	char* long_name;
	ArgoOptionType type;
	bool required;
	bool found;
	char* value;
} ArgoOption;

#define Argo_Set(_short_name, _long_name, _type, _required)                                    \
	{.short_name = _short_name,                                                                \
	 .long_name = _long_name,                                                                  \
	 .type = _type,                                                                            \
	 .required = _required,                                                                    \
	 false,                                                                                    \
	 NULL}

typedef struct ArgoInstance
{
	ArgoOption* options;
	size_t size;
	char** unformatted_args_begin;
} ArgoInstance;

bool Argo_Tokenize(ArgoInstance* instance, ArgoOption* options, size_t size, int argc,
                   char** argv);
void Argo_PrintError(ArgoInstance* instance);

#ifdef __cplusplus
}
#endif

#endif
