/*
 * Test Program for json_parse
 * compile with,
 * gcc json_parser.c test.c -o test 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef JSON_LINUX_TEST
#define debug(...)	wmprintf(__VA_ARGS__)
#else
#define debug(...)
#endif

#ifdef JSON_LINUX_TEST
#include "../../incl/sdk/json.h"
#else
#include <cli.h>
#include <cli_utils.h>
#include <wmstdio.h>
#include <json.h>
#endif

#ifdef JSON_LINUX_TEST
#define wmprintf printf
#endif

/* Maximum value to set json objects of various types */
#define MAX_JSON_BUF_LEN 512

/* Buffer to store various json test objects */
static char tmpstr[MAX_JSON_BUF_LEN];
static char str[MAX_JSON_STR_LEN];
static struct json_str jstr;
static struct json_str jstr_c;
static struct json_object tmp;

void json_test(int argc, char **argv)
{
	int num;
	int fmt;
	char text[] =
	    "{\n     \"precision\": \"zip\",\n       \"Latitude\":  37668,\n       \"Longitude\": -12259,\n     \"Address\":   \"\",\n  \"City\":      \"SAN FRANCISCO\",\n     \"State\":     \"CA\",\n        \"Zip\":       \"94107\",\n     \"Country\":   \"US\"\n                                                                                                    }";
	char complex_obj[] =
	    "{ \"glossary\": { \"title\": \"example glossary\", \"GlossDiv\": { \"title\": \"S\", 		\"GlossList\": { \"GlossEntry\": { \"ID\": \"SGML\", \"SortAs\": \"SGML\",\"GlossTerm\": \"Standard Generalized Markup Language\", \"Acronym\": \"SGML\", 	\"Abbrev\": \"ISO 8879:1986\", 	\"GlossDef\": { \"para\": \"language\",	\"GlossSeeAlso\": \"XML\" },	\"GlossSee\": \"markup\"  } } } } }";

	debug("#### json_parser ####\n");

	json_object_init(&tmp, text);

	if (!json_get_val_str(&tmp, "Country", str, MAX_JSON_STR_LEN)) {
		if (!strcmp(str, "US")) {
			debug("***Json_parser ---str_get match\n");
		} else {
			debug("***Err ---str_get mismatch\n");
			goto ERROR;
		}
	} else {
		debug("***Err ---str_get failed\n");
		goto ERROR;
	}

	if (!json_get_val_int(&tmp, "Longitude", &num)) {
		if (num == -12259) {
			debug("***Json_parser ---int_get match\n");
		} else {
			debug("***Err ---int_get mismatch\n");
			goto ERROR;
		}
	} else {
		debug("***Err ---int_get failed\n");
		goto ERROR;
	}

	fmt = 1;
	debug("***Json_parser ---Simple Set Example\n");
	json_str_init(&jstr, tmpstr, sizeof(tmpstr), fmt);
	json_start_object(&jstr);
	json_set_val_str(&jstr, "name", "John Galt");
	json_set_val_int(&jstr, "age", 29);
	json_set_val_str(&jstr, "numberstr", "123");
	json_close_object(&jstr);
	debug("The JSON String is :%s:\n", jstr.buff);

	debug("***Json_parser --- Complex object Set Example\n");
	json_str_init(&jstr_c, tmpstr, sizeof(tmpstr), fmt);
	json_start_object(&jstr_c);
	json_push_object(&jstr_c, "menu");
	json_set_val_str(&jstr_c, "pictures", "tmp");

	json_push_object(&jstr_c, "Image");
	json_set_val_str(&jstr_c, "name", "sun");
	json_set_val_int(&jstr_c, "length", 1000);
	json_set_val_str(&jstr_c, "size", "1600pi");

	json_pop_object(&jstr_c);	/* Close Image */

	json_push_object(&jstr_c, "text");
	json_set_val_str(&jstr_c, "style", "bold");
	json_set_val_str(&jstr_c, "alignment", "center");
	json_set_val_str(&jstr_c, "name", "text1");

	json_pop_object(&jstr_c);	/* Close text */
	json_set_val_str(&jstr_c, "object", "close");
	json_pop_object(&jstr_c);	/* Close menu */
	json_close_object(&jstr_c);	/* Close main */

	debug("The JSON String is *%s*\n", jstr_c.buff);

	struct json_object obj;
	json_object_init(&obj, jstr_c.buff);
	if (!json_get_composite_object(&obj, "text")) {
		if (!json_get_val_int(&obj, "length", &num))
			debug("val is %d \n", num);
		else
			debug("***Json_parser ---passed, no such member\n");

		if (!json_get_val_str(&obj, "style", str, MAX_JSON_STR_LEN)) {
			if (!strcmp(str, "bold")) {
				debug
				    ("***Json_parser ---composite_str_get match\n");
				debug("member->style:val->%s\n", str);
			} else {
				debug("***Err ---composite_str_get mismatch\n");
				goto ERROR;
			}
		} else {
			debug("***Err ---composite_str_get failed\n");
			goto ERROR;
		}

	} else {
		debug("***Err ---get_object_offsets failed\n");
		goto ERROR;
	}

	json_release_composite_object(&obj);

	json_object_init(&obj, complex_obj);
	if (!json_get_composite_object(&obj, "GlossEntry")) {
		if (!json_get_val_str(&obj, "GlossSee", str, 64)) {
			if (!strcmp(str, "markup")) {
				debug
				    ("***Json_parser ---composite_str_get match\n");
				debug("member->GlossEntry:val->%s\n", str);
			} else {
				debug("***Err ---composite_str_get mismatch\n");
				goto ERROR;
			}
		} else {
			debug("***Err ---composite_str_get failed\n");
			goto ERROR;
		}
	} else {
		debug("***Err ---get_object_offsets failed\n");
		goto ERROR;
	}

	json_release_composite_object(&obj);
	goto SUCCESS;
ERROR:
	wmprintf("Error");
SUCCESS:
	wmprintf("Success");
}

#ifdef JSON_LINUX_TEST
int main()
{
	json_test(0, NULL);
	return 0;
}
#else
struct cli_command json_command = {
	"json-test", "Runs Json Tests", json_test
};

int json_cli_init(void)
{
	if (cli_register_command(&json_command))
		return 1;
	return 0;
}
#endif
