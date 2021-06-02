#ifndef _CERVER_TESTS_HTTP_H_
#define _CERVER_TESTS_HTTP_H_

#define STATIC_DATE		1607555686
#define STATIC_ID		"1607555686"

extern const char *private_key;
extern const char *public_key;

extern const char *base_token;
extern const char *bearer_token;

extern void http_tests_headers (void);

extern void http_tests_methods (void);

extern void http_tests_contents (void);

extern void http_tests_requests (void);

extern void http_tests_responses (void);

extern void http_tests_multiparts (void);

extern void http_tests_jwt (void);

extern void http_tests_jwt_encode (void);

extern void http_tests_jwt_decode (void);

extern void http_tests_admin (void);

#endif