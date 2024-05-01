#include <stdio.h>
#include <string.h>
#include "regex.h"

#define LEN(a)	(sizeof(a) / sizeof((a)[0]))

static struct tag {
	char *ext;	/* file extension */
	int grp;	/* tag group in pat */
	char *pat;	/* tag pattern */
	char *loc;	/* optional tag location; may reference groups in pat */
} tags[] = {
	{".c", 1, "^[a-zA-Z_].*\\<([a-zA-Z_][a-zA-Z_0-9]*)\\(.*[^;]$", "/^[a-zA-Z_].*\\<\\1\\>\\(.*[^;]$/"},
	{".c", 1, "^#define +\\<([a-zA-Z_0-9]+)\\>", "/^#define +\\<(\\1)\\>/"},
	{".go", 3, "^(func|var|const|type)( +\\([^()]+\\))? +([a-zA-Z_0-9]+)\\>", "/^\\1( +\\(.*\\))? +\\3\\>/"},
	{".sh", 2, "^(function +)?([a-zA-Z_][a-zA-Z_0-9]*)(\\(\\))? *\\{", "/^\\1\\2(\\(\\))? *\\{$/"},
	{".py", 2, "^(def|class) +([a-zA-Z_][a-zA-Z_0-9]*)\\>", "/^\\1 +\\2\\>/"},
	{".ex", 2, "^[ \t]*(def|defp|defmodule)[ \t]+([a-zA-Z_0-9]+\\>[?!]?)", "/^[ \\t]*\\1[ \\t]+\\2\\>[?!]?/"},
};

static int tags_match(int idx, char *path)
{
	int len = strlen(tags[idx].ext);
	if (strlen(path) > len && !strcmp(strchr(path, '\0') - len, tags[idx].ext))
		return 0;
	return 1;
}

static void replace(char *dst, char *rep, char *ln, regmatch_t *subs)
{
	while (rep[0]) {
		if (rep[0] == '\\' && rep[1]) {
			if (rep[1] >= '0' && rep[1] <= '9') {
				int grp = rep[1] - '0';
				int beg = subs[grp].rm_so;
				int end = subs[grp].rm_eo;
				int len = end - beg;
				memcpy(dst, ln + beg, len);
				dst += len;
			} else {
				*dst++ = rep[0];
				*dst++ = rep[1];
			}
			rep++;
		} else {
			*dst++ = rep[0];
		}
		rep++;
	}
	dst[0] = '\0';
}

static int mktags(char *path, regex_t *re, int grp, char *rep)
{
	char ln[128];
	char loc[256];
	char tag[120];
	int lnum = 0;
	regmatch_t grps[32];
	FILE *fp = fopen(path, "r");
	if (fp == NULL)
		return 1;
	while (fgets(ln, sizeof(ln), fp) != NULL) {
		if (regexec(re, ln, LEN(grps), grps, 0) == 0) {
			int len = grps[grp].rm_eo - grps[grp].rm_so;
			if (len + 1 > sizeof(tag))
				len = sizeof(tag) - 1;
			memcpy(tag, ln + grps[grp].rm_so, len);
			tag[len] = '\0';
			if (rep != NULL)
				replace(loc, rep, ln, grps);
			else
				sprintf(loc, "%d", lnum + 1);
			printf("%s\t%s\t%s\n", tag, path, loc);
		}
		lnum++;
	}
	fclose(fp);
	return 0;
}

int main(int argc, char *argv[])
{
	int i, j;
	if (argc == 1) {
		printf("usage: %s files >tags\n", argv[0]);
		return 0;
	}
	for (i = 1; i < argc; i++) {
		for (j = 0; j < LEN(tags); j++) {
			regex_t re;
			if (tags_match(j, argv[i]) != 0)
				continue;
			if (regcomp(&re, tags[j].pat, REG_EXTENDED) != 0) {
				fprintf(stderr, "mktags: bad pattern %s\n", tags[j].pat);
				continue;
			}
			if (mktags(argv[i], &re, tags[j].grp, tags[j].loc))
				fprintf(stderr, "mktags: failed to read %s\n", argv[i]);
			regfree(&re);
		}
	}
	return 0;
}
