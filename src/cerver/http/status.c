#include "cerver/http/status.h"

const char *http_status_str (const enum http_status s) {

	switch (s) {
		#define XX(num, name, string) case HTTP_STATUS_##name: return #string;
		HTTP_STATUS_MAP(XX)
		#undef XX
	}

	return http_status_str (HTTP_STATUS_NONE);

}