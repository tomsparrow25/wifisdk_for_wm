#ifndef __HTTP_LAYER_CONSTANTS_H__
#define __HTTP_LAYER_CONSTANTS_H__

#ifdef __cplusplus
extern "C" {
#endif

// set of constants
extern const char* const XI_HTTP_POST;
extern const char* const XI_HTTP_GET;
extern const char* const XI_HTTP_PUT;
extern const char* const XI_HTTP_DELETE;
extern const char* const XI_HTTP_CRLF;
extern const char* const XI_HTTP_TEMPLATE_FEED;
extern const char* const XI_HTTP_TEMPLATE_CSV;
extern const char* const XI_HTTP_TEMPLATE_HTTP;
extern const char* const XI_HTTP_TEMPLATE_HOST;
extern const char* const XI_HTTP_TEMPLATE_USER_AGENT;
extern const char* const XI_HTTP_TEMPLATE_X_API_KEY;
extern const char* const XI_HTTP_TEMPLATE_ACCEPT;
extern const char* const XI_HTTP_CONTENT_LENGTH;
extern const char* const XI_CSV_TIMESTAMP_PATTERN;
extern const char* const XI_CSV_SLASH;
extern const char* const XI_CSV_COMMA;
extern const char* const XI_CSV_DATASTREAMS;
extern const char* const XI_CSV_DATAPOINTS;
extern const char* const XI_CSV_DOT_CSV_SPACE;
extern const char* const XI_HTTP_AMPERSAND;
extern const char* const XI_HTTP_SPACE;
extern const char* const XI_HTTP_NEWLINE;
extern const char* const XI_HTTP_EMPTY;

extern char buffer_32[];

#ifdef __cplusplus
}
#endif

#endif // __HTTP_LAYER_CONSTANTS_H__
