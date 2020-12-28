/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <cerver/http/json/json.h>

#include "json.h"
#include "errors.h"

static void start (void) {
	json_t *json;
	json_error_t error;
	const char str[] = "[\"A\", {\"B\": \"C\"}, 1, 2, 3]garbage";
	size_t len = strlen(str) - strlen("garbage");

	json = json_loadb(str, len, 0, &error);
	if (!json) {
		fail("json_loadb failed on a valid JSON buffer");
	}
	json_decref(json);

	json = json_loadb(str, len - 1, 0, &error);
	if (json) {
		json_decref(json);
		fail("json_loadb should have failed on an incomplete buffer, but it "
			 "didn't");
	}
	if (error.line != 1) {
		fail("json_loadb returned an invalid line number on fail");
	}
	if (strcmp(error.text, "']' expected near end of file") != 0) {
		fail("json_loadb returned an invalid error message for an unclosed "
			 "top-level array");
	}
}

void json_tests_loadb (void) {

	(void) printf ("Testing JSON loadb...\n");

	start ();

	(void) printf ("Done!\n");

}