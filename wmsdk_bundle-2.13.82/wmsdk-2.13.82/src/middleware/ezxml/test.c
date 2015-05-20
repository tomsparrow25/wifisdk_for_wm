/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <string.h>

#include <wm_os.h>
#include <wmstdio.h>
#include <cli.h>
#include <ezxml.h>


char xml_buf[4096];

/*	Test to check Parsing of the given XML string		*/
void xml_parsing_test()
{
	ezxml_t f1, team, driver;
	const char *teamname;

	char *input_teamname = "McLaren";
	char *input_names[] = { "Kimi Raikkonen", "driver2" };
	char *input_points[] = { "112", "60" };
	int i = 0;

	strcpy(xml_buf,
	       "<?xml version=\"1.0\"?><formula1><team name=\"McLaren\"><driver><name>Kimi Raikkonen</name><points>112</points></driver><driver><name>driver2</name><points>60</points></driver></team></formula1>");

	f1 = ezxml_parse_str(xml_buf, strlen(xml_buf));

	for (team = ezxml_child(f1, "team"); team; team = team->next) {
		teamname = ezxml_attr(team, "name");
		if (strcmp(input_teamname, teamname))
			goto ERROR;

		for (driver = ezxml_child(team, "driver"); driver;
		     driver = driver->next) {
			if (strcmp
			    (ezxml_child(driver, "name")->txt, input_names[i]))
				goto ERROR;
			if (strcmp
			    (ezxml_child(driver, "points")->txt,
			     input_points[i++]))
				goto ERROR;
		}
	}
	ezxml_free(f1);
	goto SUCCESS;

ERROR:
	wmprintf("Error");
	return;
SUCCESS:
	wmprintf("Success");
}

/*	Test creation of ezXML internal representation. Test Set 1
 *	APIs tested:
 *  ezxml_new, ezxml_add_child, ezxml_set_txt, ezxml_set_attr, ezxml_toxml,
 * ezxml_free
 */
void xml_creation_test1()
{
	ezxml_t f1, child1, child2, child3;

	int status;
	char input_xml_string[] =
	    "<a><b>xxx</b><c>yyy</c><d name=\"AAA\"></d></a>";

	/*      Create the root node a  */
	f1 = ezxml_new("a");

	/*      Add first child b to root node a        */
	child1 = ezxml_add_child(f1, "b", 0);

	/*      Add second child c to root node a       */
	child2 = ezxml_add_child(f1, "c", 0);

	/*      Add third child d to root node a        */
	child3 = ezxml_add_child(f1, "d", 0);

	/*      Set value to node b     */
	child1 = ezxml_set_txt(child1, "xxx");

	/*      Set value to node c     */
	child2 = ezxml_set_txt(child2, "yyy");

	/*      Set attribute name to node d    */
	child3 = ezxml_set_attr(child3, "name", "AAA");

	status = strcmp(ezxml_toxml(f1), input_xml_string);
	if (status != 0)
		goto ERROR;

	ezxml_free(f1);
	goto SUCCESS;

ERROR:
	wmprintf("Error");
	return;
SUCCESS:
	wmprintf("Success");
}

/*	Test the creation of ezXML internal representation. Test Set 2
 *	APIs tested:
 *	ezxml_new, ezxml_add_child, ezxml_set_txt, ezxml_set_attr,
 *	ezxml_cut, ezxml_insert, ezxml_toxml, ezxml_free
 */
void xml_creation_test2()
{
	ezxml_t f1, child1, child2, child3, child4;

	int status;
	char input_xml_string[] =
	    "<a><b><c name=\"AAA\"></c></b><d><e>yyy</e></d></a>";

	/*      Create the root node a  */
	f1 = ezxml_new("a");

	/*      Add child b to root node a      */
	child1 = ezxml_add_child(f1, "b", 0);

	/*      Add child c to node b   */
	child2 = ezxml_add_child(child1, "c", 0);

	/*      Add child d to node c   */
	child3 = ezxml_add_child(child2, "d", 0);

	/*      Add child e to node d   */
	child4 = ezxml_add_child(child3, "e", 0);

	/*      Set value for node e    */
	child4 = ezxml_set_txt(child4, "yyy");

	/*      Add attribute name to node c    */
	child2 = ezxml_set_attr(child2, "name", "AAA");

	/*      Remove the node d       */
	child3 = ezxml_cut(child3);

	/*      Insert the node d to other location     */
	ezxml_insert(child3, f1, 0);

	status = strcmp(ezxml_toxml(f1), input_xml_string);
	if (status != 0)
		goto ERROR;

	ezxml_free(f1);
	goto SUCCESS;

ERROR:
	wmprintf("Error");
	return;
SUCCESS:
	wmprintf("Success");
}

struct cli_command ezxml_commands[] = {
	{"xml-parsing-test", "Runs ezxml parsing test", xml_parsing_test},
	{"xml-creation-test1", "Runs xml creation test set 1",
	 xml_creation_test1},
	{"xml-creation-test2", "Runs xml creation test set 2",
	 xml_creation_test2},
};

int ezxml_cli_init(void)
{
	if (cli_register_commands
	    (&ezxml_commands[0],
	     sizeof(ezxml_commands) / sizeof(struct cli_command)))
		return 1;
	return 0;
}
