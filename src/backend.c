#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "backend_z.h"
#include "backend_aa.h"
#include "frontend.h"
#include "compile.h"
#include "report.h"
#include "ifid.h"

static struct arena backend_arena;

static const char ifid_template[] = "NNNNNNNN-NNNN-NNNN-NNNN-NNNNNNNNNNNN";

static int match_template(const char *txt, const char *template) {
	for(;;) {
		if(*template == 'N') {
			if(!((*txt >= '0' && *txt <= '9') || (*txt >= 'A' && *txt <= 'F'))) {
				return 0;
			}
		} else if(*template != *txt) {
			return 0;
		}
		if(!*template) return 1;
		template++;
		txt++;
	}
}

static void get_timestamp(char *dest, char *longdest) {
	time_t t = 0;
	struct tm *tm;

	time(&t);
	tm = localtime(&t);
	snprintf(dest, 8, "%02d%02d%02d", tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday);
	snprintf(longdest, 11, "%04d-%02d-%02d", 1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday);
}

void suggest_new_ifid(int level) {
	char suggestion[37];
	int unavailable = generate_ifid(suggestion);
	if(unavailable) {
		report(level, 0, "You can get one at <https://www.tads.org/ifidgen/ifidgen>.");
	} else {
		report(level, 0, "Add this one to your story:");
		report(level, 0, "(story ifid) %s", suggestion);
	}
}

void usage(char *prgname) {
	fprintf(stderr, "Dialog compiler " VERSION ".\n");
	fprintf(stderr, "Original compiler copyright 2018-2021 Linus Akesson.\n");
	fprintf(stderr, "Further development copyright 2024-2026 Dialog Project contributors.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Usage: %s [options] [source code filename ...]\n", prgname);
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--version         -V    Display the program version.\n");
	fprintf(stderr, "--help            -h    Display this information.\n");
	fprintf(stderr, "--verbose         -v    Increase verbosity (may be used multiple times).\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--output          -o    Set output filename.\n");
	fprintf(stderr, "--format          -t    Set output format (zblorb, z8, z5, aa).\n");
	fprintf(stderr, "--resources       -r    Set resource directory (default '.').\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--heap            -H    Set main heap size (default 1000 words).\n");
	fprintf(stderr, "--aux             -A    Set aux heap size (default 500 words).\n");
	fprintf(stderr, "--long-term       -L    Set long-term heap size (default 500 words).\n");
	fprintf(stderr, "--strip           -s    Strip internal object names.\n");
	fprintf(stderr, "--warn-not-topic        Always warn about objects not used as topics.\n");
	fprintf(stderr, "--no-warn-not-topic     Never warn about objects not used as topics.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Only for z5, z8, or zblorb format:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--no-default-unicode    Don't preserve the default Unicode translation table.\n");
	fprintf(stderr, "--optimize-alphabet     Increase dictionary resolution for non-English letters.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Only for zblorb format:\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "--cover           -c    Cover image filename (PNG or JPEG, max 1200x1200).\n");
	fprintf(stderr, "--cover-alt       -a    Textual description of cover image.\n");
	exit(1);
}

extern int topic_warning_level; // Defined in frontend.c
extern int zmachine_optimize_alphabet; // Defined in backend_z.c
extern int zmachine_preserve_zscii; // Defined in backend_z.c

int main(int argc, char **argv) {
	struct option longopts[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{"output", 1, 0, 'o'},
		{"format", 1, 0, 't'},
		{"resources", 1, 0, 'r'},
		{"cover", 1, 0, 'c'},
		{"cover-alt", 1, 0, 'a'},
		{"heap", 1, 0, 'H'},
		{"aux", 1, 0, 'A'},
		{"long-term", 1, 0, 'L'},
		{"strip", 0, 0, 's'},
		{"no-default-uni", 0, &zmachine_preserve_zscii, 0},
		{"no-default-unicode", 0, &zmachine_preserve_zscii, 0},
		{"warn-not-topic", 0, &topic_warning_level, 1},
		{"no-warn-not-topic", 0, &topic_warning_level, 2},
		{"optimize-alphabet", 0, &zmachine_optimize_alphabet, 1},
		{0, 0, 0, 0}
	};

	char *prgname = argv[0];
	char *outname = 0;
	char *format = "z8";
	char *coverfname = 0;
	char *coveralt = 0;
	char *resdir = 0;
	int auxsize = 500, heapsize = 1000, ltssize = 500;
	int strip = 0;
	int opt, i;
	struct program *prg;
	int aamachine = 0;
	struct predname *predname;
	struct predicate *pred;
	char compiletime_buf[8], reldate_buf[16];
	int need_meta;

	arena_init(&backend_arena, 1024);
	comp_init();

	do {
		opt = getopt_long(argc, argv, "?hVvo:t:r:c:a:H:A:L:s", longopts, 0);
		switch(opt) {
			case 0:
				break; // Added DMS so long-only options are possible
			case '?':
			case 'h':
				usage(prgname);
				break;
			case 'V':
				fprintf(stderr, "Dialog compiler " VERSION "\n");
				exit(0);
			case 'v':
				verbose++;
				break;
			case 'o':
				outname = strdup(optarg);
				break;
			case 't':
				format = strdup(optarg);
				break;
			case 'r':
				resdir = strdup(optarg);
				break;
			case 'c':
				coverfname = strdup(optarg);
				break;
			case 'a':
				coveralt = strdup(optarg);
				break;
			case 'H':
				heapsize = strtol(optarg, 0, 10);
				if(heapsize < 23 || heapsize > 8192) {
					report(LVL_ERR, 0, "Bad main heap size (max 8192 words)");
					exit(1);
				}
				break;
			case 'A':
				auxsize = strtol(optarg, 0, 10);
				if(auxsize < 1 || auxsize > 16351) {
					report(LVL_ERR, 0, "Bad aux heap size (max 16351 words)");
					exit(1);
				}
				break;
			case 'L':
				ltssize = strtol(optarg, 0, 10);
				if(ltssize < 1 || ltssize > 16351) {
					report(LVL_ERR, 0, "Bad long-term heap size (max 16351 words)");
					exit(1);
				}
				break;
			case 's':
				strip = 1;
				break;
			default:
				if(opt >= 0) {
					report(LVL_ERR, 0, "Unimplemented option '%c'", opt);
					exit(1);
				}
				break;
		}
	} while(opt >= 0);

	if(strcmp(format, "zblorb")
	&& strcmp(format, "z8")
	&& strcmp(format, "z5")
	&& strcmp(format, "aa")) {
		report(LVL_ERR, 0, "Unsupported output format \"%s\".", format);
		exit(1);
	}

	if(!strcmp(format, "aa")) {
		aamachine = 1;
	}
	
	if(aamachine && !zmachine_preserve_zscii) {
		report(LVL_WARN, 0, "The --no-default-unicode option has no effect on the aa output format");
	}
	if(aamachine && zmachine_optimize_alphabet) {
		report(LVL_WARN, 0, "The --optimize-alphabet option has no effect on the aa output format");
	}

	if(!outname) {
		if(optind < argc) {
			outname = malloc(strlen(argv[optind]) + 8);
			strcpy(outname, argv[optind]);
			for(i = strlen(outname) - 1; i >= 0; i--) {
				if(outname[i] == '.') break;
			}
			if(i < 0) {
				i = strlen(outname);
			}
			outname[i++] = '.';
			if(!strcmp(format, "aa")) {
				strcpy(outname + i, "aastory");
			} else {
				strcpy(outname + i, format);
			}
		} else {
			report(LVL_ERR, 0, "No output filename specified, and none can be deduced from the input filenames.");
			exit(1);
		}
	}

	prg = new_program();
	frontend_add_builtins(prg);
	prg->optflags |= OPTF_BOUND_PARAMS | OPTF_TAIL_CALLS | OPTF_ENV_FRAMES;
	prg->optflags |= OPTF_SIMPLE_SELECT | OPTF_NO_LOG | OPTF_INLINE;
	if(!aamachine) {
		prg->optflags |= OPTF_NO_LINKS;
	}
	prg->optflags |= OPTF_NO_TRACE; // This gets cleared by the frontend if (trace on) is reachable.
	if(aamachine) {
		prg->max_temp = aa_get_max_temp();
	}

	if(!frontend(
		prg,
		argc - optind,
		argv + optind,
		aamachine? prepare_dictionary_aa : prepare_dictionary_z
		)
	) {
		exit(1);
	}

	need_meta = prg->totallines > 100;

	prg->meta_ifid = decode_metadata_str(BI_STORY_IFID, 0, prg, &prg->arena);
	if(!prg->meta_ifid) {
		if(!strcmp(format, "zblorb")) { // Mandatory for zblorb, make it an error
			report(LVL_ERR, 0, "An IFID is mandatory for the blorb output format.");
			suggest_new_ifid(LVL_ERR);
			exit(1);
		} else if(need_meta) {
			report(LVL_WARN, 0, "No IFID declared.");
			suggest_new_ifid(LVL_WARN);
		}
	} else if(!match_template(prg->meta_ifid, ifid_template)) {
		report(LVL_WARN, find_builtin(prg, BI_STORY_IFID)->pred->clauses[0]->line, "Ignoring invalid IFID. It should have the format %s, where N is an uppercase hexadecimal digit.", ifid_template);
		prg->meta_ifid = 0;
	}

	prg->meta_author = decode_metadata_str(BI_STORY_AUTHOR, 0, prg, &prg->arena);
	if(!prg->meta_author) {
		if(need_meta) {
			report(LVL_WARN, 0, "No author declared.");
		}
		prg->meta_author = "Anonymous";
	}
	prg->meta_title = decode_metadata_str(BI_STORY_TITLE, 0, prg, &prg->arena);
	if(!prg->meta_title) {
		if(need_meta) {
			report(LVL_WARN, 0, "No title declared.");
		}
		prg->meta_title = "An Interactive Fiction";
	}
	prg->meta_noun = decode_metadata_str(BI_STORY_NOUN, 0, prg, &prg->arena);
	if(!prg->meta_noun) {
		prg->meta_noun = "An Interactive Fiction";
	}
	prg->meta_blurb = decode_metadata_str(BI_STORY_BLURB, 0, prg, &prg->arena);

	predname = find_builtin(prg, BI_STORY_RELEASE);
	if(predname && (pred = predname->pred)->nclause) {
		if(pred->clauses[0]->body || pred->clauses[0]->params[0]->kind != AN_INTEGER) {
			report(LVL_ERR, pred->clauses[0]->line, "Malformed story release declaration. Use e.g. (story release 1).");
			exit(1);
		}
		prg->meta_release = pred->clauses[0]->params[0]->value;
	} else if(need_meta) {
		report(LVL_WARN, 0, "No release number declared.");
	}

	get_timestamp(compiletime_buf, reldate_buf);

	prg->meta_serial = arena_strdup(&prg->arena, compiletime_buf);
	prg->meta_reldate = arena_strdup(&prg->arena, reldate_buf);

	if(aamachine) {
		backend_aa(outname, format, coverfname, coveralt, heapsize, auxsize, ltssize, strip, prg, &backend_arena, resdir);
	} else {
		backend_z(outname, format, coverfname, coveralt, heapsize, auxsize, ltssize, strip, prg, &backend_arena);
	}

	free_program(prg);
	arena_free(&backend_arena);

	return 0;
}
