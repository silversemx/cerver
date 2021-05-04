#include <string.h>

#include "cerver/http/headers.h"

const char *http_header_string (
	const HttpHeader header
) {

	switch (header) {
		#define XX(num, name, string, description) case HTTP_HEADER_##name: return #string;
		HTTP_HEADERS_MAP(XX)
		#undef XX
	}

	return http_header_string (HTTP_HEADER_INVALID);

}

const char *http_header_description (
	const HttpHeader header
) {

	switch (header) {
		#define XX(num, name, string, description) case HTTP_HEADER_##name: return #description;
		HTTP_HEADERS_MAP(XX)
		#undef XX
	}

	return http_header_description (HTTP_HEADER_INVALID);

}

const HttpHeader http_header_type_by_string (
	const char *header_type_string
) {

	#define XX(num, name, string, description) if (!strcasecmp (#string, header_type_string)) return HTTP_HEADER_##name;
	HTTP_HEADERS_MAP(XX)
	#undef XX

	return HTTP_HEADER_INVALID;

}