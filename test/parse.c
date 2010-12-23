#include <stdio.h>

#include "at_parse.h"			/* at_parse_*() */
#include "helpers.h"			/* ITEMS_OF() */


int ok = 0;
int faults = 0;

#/* */
void test_parse_cnum()
{
	static const struct test_case {
		const char	* input;
		const char	* result;
	} cases[] = {
		{ "+CNUM: \"*Subscriber Number\",\"+79139131234\",145", "+79139131234" },
		{ "+CNUM: \"Subscriber Number\",\"\",145", "" },
		{ "+CNUM: \"Subscriber Number\",,145", "" },
		{ "+CNUM: \"\",\"+79139131234\",145", "+79139131234" },
		{ "+CNUM: ,\"\",145", "" },
		{ "+CNUM: ,,145", "" },
		{ "+CNUM: \"\",+79139131234\",145", "+79139131234" },
		{ "+CNUM: \"\",+79139131234,145", "+79139131234" },
	};
	unsigned idx = 0;
	char * input;
	const char * res;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_cnum", input);
		res = at_parse_cnum(input);
		free(input);
		if(strcmp(res, cases[idx].result) == 0) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = \"%s\"\t%s\n", res, msg);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_cops()
{
	static const struct test_case {
		const char	* input;
		const char	* result;
	} cases[] = {
		{ "+COPS: 0,0,\"TELE2\",0", "TELE2" },
		{ "+COPS: 0,0,\"TELE2,0", "TELE2" },
		{ "+COPS: 0,0,TELE2,0", "TELE2" },
	};
	unsigned idx = 0;
	char * input;
	const char * res;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_cops", input);
		res = at_parse_cops(input);
		free(input);
		if(strcmp(res, cases[idx].result) == 0) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = \"%s\"\t%s\n", res, msg);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_creg()
{
	struct result {
		int	res;
		int	gsm_reg;
		int	gsm_reg_status;
		char 	* lac;
		char	* ci;
	};
	static const struct test_case {
		const char	* input;
		struct result 	result;
	} cases[] = {
		{ "+CREG: 2,1,9110,7E6", { 0, 1, 1, "9110", "7E6"} },
		{ "+CREG: 2,1,XXXX,AAAA", { 0, 1, 1, "XXXX", "AAAA"} },
	};
	unsigned idx = 0;
	char * input;
	struct result result;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_creg", input);
		result.res = at_parse_creg(input, strlen(input), &result.gsm_reg, &result.gsm_reg_status, &result.lac, &result.ci);
		free(input);
		if(result.res == cases[idx].result.res
			&&
		   result.gsm_reg == cases[idx].result.gsm_reg
			&&
		   result.gsm_reg_status == cases[idx].result.gsm_reg_status
			&&
		   strcmp(result.lac, cases[idx].result.lac) == 0
			&&
		   strcmp(result.ci, cases[idx].result.ci) == 0
			) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = %d (%d,%d,\"%s\",\"%s\")\t%s\n", result.res, result.gsm_reg, result.gsm_reg_status, result.lac, result.ci, msg);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_cmti()
{
	static const struct test_case {
		const char	* input;
		int		result;
	} cases[] = {
		{ "+CMTI: \"ME\",41", 41 },
		{ "+CMTI: 0,111", 111 },
		{ "+CMTI: ", -1 },
	};
	unsigned idx = 0;
	char * input;
	int result;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_cmti", input);
		result = at_parse_cmti(input);
		free(input);
		if(result == cases[idx].result) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = %d\t%s\n", result, msg);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_cmgr()
{
}

#/* */
void test_parse_cusd()
{
}

#/* */
void test_parse_cpin()
{
}

#/* */
void test_parse_csq()
{
}

#/* */
void test_parse_rssi()
{
}

#/* */
void test_parse_mode()
{
}

#/* */
void test_parse_csca()
{
}

#/* */
void test_parse_clcc()
{
	struct result {
		int		res;

		unsigned	index;
		unsigned	dir;
		unsigned	stat;
		unsigned	mode;
		unsigned	mpty;
		char		* number;
		unsigned	toa;
	};
	static const struct test_case {
		const char	* input;
		struct result	result;
	} cases[] = {
		{ "+CLCC: 1,1,4,0,0,\"\",145", { 0, 1, 1, 4, 0, 0, "", 145} },
		{ "+CLCC: 1,1,4,0,0,\"+79139131234\",145", { 0, 1, 1, 4, 0, 0, "+79139131234", 145} },
		{ "+CLCC: 1,1,4,0,0,\"+7913913ABCA\",145", { 0, 1, 1, 4, 0, 0, "+7913913ABCA", 145} },
		{ "+CLCC: 1,1,4,0,0,\"+7913913ABCA\"", { -1, 0, 0, 0, 0, 0, "", 0} },
	};
	unsigned idx = 0;
	char * input;
	struct result result;
	const char * msg;
	
	for(; idx < ITEMS_OF(cases); ++idx) {
		input = strdup(cases[idx].input);
		fprintf(stderr, "%s(\"%s\")...", "at_parse_clcc", input);
		result.res = at_parse_clcc(input, &result.index, &result.dir, &result.stat, &result.mode, &result.mpty, &result.number, &result.toa);
		free(input);
		if(result.res == cases[idx].result.res
			&&
		   result.index == cases[idx].result.index
			&&
		   result.dir == cases[idx].result.dir
			&&
		   result.stat == cases[idx].result.stat
			&&
		   result.mode == cases[idx].result.mode
			&&
		   result.mpty == cases[idx].result.mpty
			&&
		   strcmp(result.number, cases[idx].result.number) == 0
			&&
		   result.toa == cases[idx].result.toa
			) {
			msg = "OK";
			ok++;
		} else {
			msg = "FAIL";
			faults++;
		}
		fprintf(stderr, " = %d (%d,%d,%d,%d,%d,\"%s\",%d)\t%s\n", result.res, result.index, result.dir, result.stat, result.mode, result.mpty, result.number, result.toa, msg);
	}
	fprintf(stderr, "\n");
}

#/* */
void test_parse_ccwa()
{
}

#/* */
int main()
{
	test_parse_cnum();
	test_parse_cops();
	test_parse_creg();
	test_parse_cmti();
	test_parse_cmgr();
	test_parse_cusd();
	test_parse_cpin();
	test_parse_csq();
	test_parse_rssi();
	test_parse_mode();
	test_parse_csca();
	test_parse_clcc();
	test_parse_ccwa();
	
	fprintf(stderr, "done %d tests: %d OK %d FAILS\n", ok + faults, ok, faults);
	return 0;
}