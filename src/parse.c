#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arena.h"
#include "ast.h"
#include "parse.h"
#include "report.h"
#include "unicode.h"

#define MAXWORDLENGTH 256
#define MAXRULEWORDS 32
#define MAXNESTEDEXPR 32

enum {
	PMODE_BODY,
	PMODE_RULE,
	PMODE_VALUE
};

static line_t line = 0;
static int column = 0;
static struct word *starword = 0;
static int status;

static int valid_varname_char(uint8_t ch) {
	return
		(ch >= 'a' && ch <= 'z')
		|| (ch >= 'A' && ch <= 'Z')
		|| (ch >= '0' && ch <= '9')
		|| ch >= 128
		|| ch == '_'
		|| ch == '-';
}

static int lexer_getc(struct lexer *lexer) {
	if(lexer->ungetcbuf) {
		int ch = lexer->ungetcbuf;
		lexer->ungetcbuf = 0;
		return ch;
	}
	if(lexer->string) {
		if(*lexer->string) {
			return *lexer->string++;
		} else {
			lexer->string = 0;
		}
	}
	if(lexer->file) {
		int ch = fgetc(lexer->file);
		if(ch == 9
		|| ch == 10
		|| ch == 13
		|| (ch >= 0x20 && ch < 0x7f)
		|| ch >= 0x80
		|| ch == EOF) {
			return ch;
		} else {
			report(LVL_WARN, line, "Ignoring control character 0x%02x in source code file", ch);
		}
	}
	return EOF;
}

static void lexer_ungetc(char ch, struct lexer *lexer) {
	lexer->ungetcbuf = ch;
}

static int next_token(struct lexer *lexer, int parsemode) {
	int ch;
	char buf[MAXWORDLENGTH + 1];
	uint16_t wbuf[MAXWORDLENGTH + 1];
	int pos = 0, i;
	int at_start;
	long val;

	for(;;) {
		ch = lexer_getc(lexer);
		column++;

		if(ch == EOF) {
			return 0;
		} else if(ch == '\n') {
			column = 0;
			line++;
		} else if(ch == ' ' || ch == '\t' || ch == '\r') {
		} else if(ch == '$' || ch == '#' || ch == '@') {
			if(ch == '$') {
				lexer->kind = TOK_VARIABLE;
			} else if(ch == '#') {
				lexer->kind = TOK_TAG;
			} else {
				lexer->kind = TOK_DICTWORD;
			}
			at_start = (column == 1);
			for(;;) {
				ch = lexer_getc(lexer);
				column++;
				if(ch != EOF && (
					valid_varname_char((uint8_t) ch)
					|| (lexer->kind == TOK_DICTWORD && !strchr("\n\r\t $#[|](){}@~*%", ch))))
				{
					if(ch == '\\') {
						ch = lexer_getc(lexer);
						column++;
						if(ch == EOF) {
							report(LVL_ERR, line, "Backslash not allowed at end of file.");
							lexer->errorflag = 1;
							return 0;
						}
					}
					if(pos >= MAXWORDLENGTH) {
						report(LVL_ERR, line, "Name too long.");
						lexer->errorflag = 1;
						return 0;
					}
					buf[pos++] = ch;
				} else {
					if(ch != EOF) {
						lexer_ungetc(ch, lexer);
						column--;
					}
					buf[pos] = 0;
					utf8_to_unicode(wbuf, MAXWORDLENGTH + 1, (uint8_t *) buf);
					if(lexer->kind == TOK_TAG && !wbuf[0]) {
						report(LVL_ERR, line, "Invalid object name.");
						lexer->errorflag = 1;
						return 0;
					}
					if(lexer->kind == TOK_DICTWORD && !wbuf[0] && !at_start) {
						report(LVL_ERR, line, "Invalid dictionary word.");
						lexer->errorflag = 1;
						return 0;
					}
					lexer->word = find_word(lexer->program, buf);
					if(lexer->kind == TOK_TAG) {
						create_worldobj(lexer->program, lexer->word);
					}
					return 1 + at_start;
				}
			}
		} else if(strchr("[|](){}~", ch)) {
			at_start = (column == 1);
			lexer->kind = ch;
			return 1 + at_start;
		} else if(ch == '/') {
			at_start = (column == 1);
			if(parsemode == PMODE_BODY) {
				buf[pos++] = '/';
				buf[pos] = 0;
				lexer->kind = TOK_BAREWORD;
				lexer->word = find_word(lexer->program, buf);
			} else {
				lexer->kind = ch;
			}
			return 1 + at_start;
		} else if(ch == '*') {
			at_start = (column == 1);
			ch = lexer_getc(lexer);
			if(ch == '(') {
				column++;
				lexer->kind = TOK_STARPAREN;
			} else {
				if(ch != EOF) {
					lexer_ungetc(ch, lexer);
					column--;
				}
				lexer->kind = '*';
			}
			return 1 + at_start;
		} else if(ch == '%') {
			at_start = (column == 1);
			ch = lexer_getc(lexer);
			column++;
			if(ch == '%') {
				do {
					ch = lexer_getc(lexer);
					if(ch == EOF) return 0;
				} while(ch != '\n');
				column = 0;
				line++;
			} else {
				if(ch != EOF) {
					lexer_ungetc(ch, lexer);
					column--;
				}
				buf[pos++] = '%';
				buf[pos] = 0;
				lexer->kind = TOK_BAREWORD;
				lexer->word = find_word(lexer->program, buf);
				return 1 + at_start;
			}
		} else {
			at_start = (column == 1);
			if(ch == '\\') {
				ch = lexer_getc(lexer);
				column++;
				if(ch == EOF) {
					report(LVL_ERR, line, "Backslash not allowed at end of file.");
					lexer->errorflag = 1;
					return 0;
				} else if(ch < 0x20) {
					report(LVL_ERR, line, "Invalid character after backslash.");
					lexer->errorflag = 1;
					return 0;
				}
			}
			buf[pos++] = ch;
			for(;;) {
				ch = lexer_getc(lexer);
				column++;
				if(ch == '\\') {
					ch = lexer_getc(lexer);
					if(ch == '\n') {
						line++;
						column = 0;
					} else {
						column++;
					}
					if(ch == EOF) {
						report(LVL_ERR, line, "Backslash not allowed at end of file.");
						lexer->errorflag = 1;
						return 0;
					} else if(ch < 0x20) {
						report(LVL_ERR, line, "Invalid character after backslash.");
						lexer->errorflag = 1;
						return 0;
					}
					if(pos >= MAXWORDLENGTH) {
						report(LVL_ERR, line, "Word too long.");
						lexer->errorflag = 1;
						return 0;
					}
					buf[pos++] = ch;
				} else if(ch == EOF || strchr("\n\r\t $#[|](){}@~*%/", ch)) {
					if(ch != EOF) {
						lexer_ungetc(ch, lexer);
						column--;
					}
					buf[pos] = 0;
					utf8_to_unicode(wbuf, MAXWORDLENGTH + 1, (uint8_t *) buf);
					if(!wbuf[0]) {
						report(LVL_ERR, line, "Syntax error.");
						lexer->errorflag = 1;
						return 0;
					}
					for(i = 0; i < pos; i++) {
						if(buf[i] < '0' || buf[i] > '9') break;
					}
					if(i == pos && (pos == 1 || buf[0] != '0')) {
						val = strtol(buf, 0, 10);
						if(val < 0 || val > 0x3fff) {
							if(parsemode != PMODE_BODY) {
								report(LVL_ERR, line, "Integer out of range (%ld)", val);
								lexer->errorflag = 1;
								return 0;
							}
							lexer->kind = TOK_BAREWORD;
							lexer->word = find_word(lexer->program, buf);
						} else {
							lexer->kind = TOK_INTEGER;
							lexer->value = val;
						}
					} else {
						lexer->kind = TOK_BAREWORD;
						lexer->word = find_word(lexer->program, buf);
					}
					return 1 + at_start;
				} else {
					if(pos >= MAXWORDLENGTH) {
						report(LVL_ERR, line, "Word too long.");
						lexer->errorflag = 1;
						return 0;
					}
					buf[pos++] = ch;
				}
			}
		}
	}
}

static int look_ahead_for_slash(struct lexer *lexer) {
	int ch;
	int found;

	do {
		ch = lexer_getc(lexer);
		column++;
	} while(ch == ' ' || ch == '\t');

	found = (ch == '/');

	if(ch != EOF) {
		lexer_ungetc(ch, lexer);
		column--;
	}

	return found;
}

static struct astnode *parse_expr(int parsemode, struct lexer *lexer, struct arena *arena);
static struct astnode *parse_expr_nested(struct astnode **nested_rules, int *nnested, int parsemode, int *nonvar_detected, struct lexer *lexer, struct arena *arena);

static struct astnode *fold_disjunctions(struct astnode *body, struct lexer *lexer, struct arena *arena) {
	struct astnode *an, **prev, *or;

	for(prev = &body; (an = *prev); prev = &an->next_in_body) {
		if(an->kind == AN_RULE) {
			if(an->predicate->special == SP_OR) {
				*prev = 0;
				or = mkast(AN_OR, 2, arena, an->line);
				or->children[0] = body;
				or->children[1] = fold_disjunctions(an->next_in_body, lexer, arena);
				return or;
			} else if(an->predicate->special == SP_ELSE
			|| an->predicate->special == SP_ELSEIF
			|| an->predicate->special == SP_ENDIF
			|| an->predicate->special == SP_THEN
			|| an->predicate->special == SP_STOPPING
			|| an->predicate->special == SP_RANDOM
			|| an->predicate->special == SP_P_RANDOM
			|| an->predicate->special == SP_T_RANDOM
			|| an->predicate->special == SP_T_P_RANDOM
			|| an->predicate->special == SP_CYCLING) {
				report(LVL_ERR, an->line, "Unexpected %s.", an->predicate->printed_name);
				lexer->errorflag = 1;
			}
		}
		if(an->kind == AN_RULE || an->kind == AN_NEG_RULE) {
			if(an->predicate->builtin == BI_RESOURCEDEF) {
				report(LVL_ERR, an->line, "Program may not query %s directly.", an->predicate->printed_name);
			}
		}
	}

	return body;
}

static struct astnode *parse_rule_head(struct astnode **nested_rules, int *nnested, int *nested_nonvar, struct lexer *lexer, struct arena *arena) {
	int n = 0, i, j, nparam = 0;
	struct astnode *nodes[MAXRULEWORDS], *an;
	struct word *words[MAXRULEWORDS];

	for(;;) {
		status = next_token(lexer, PMODE_RULE);
		if(lexer->errorflag) return 0;
		if(!status) {
			report(LVL_ERR, line, "Unterminated rule expression.");
			lexer->errorflag = 1;
			return 0;
		}
		if(status == 2) {
			report(LVL_ERR, line, "Unexpected token at beginning of line.");
			lexer->errorflag = 1;
			return 0;
		}

		if(lexer->kind == ')') break;

		if(n >= MAXRULEWORDS) {
			report(LVL_ERR, line, "Too many words in rule name.");
			lexer->errorflag = 1;
			return 0;
		}
		nodes[n] = parse_expr_nested(nested_rules, nnested, PMODE_RULE, nested_nonvar, lexer, arena);
		if(!nodes[n]) return 0;
		n++;
	}

	for(i = 0; i < n; i++) {
		if(nodes[i]->kind != AN_BAREWORD) {
			nparam++;
		}
	}
	an = mkast(AN_RULE, nparam, arena, line);
	j = 0;
	for(i = 0; i < n; i++) {
		if(nodes[i]->kind == AN_BAREWORD) {
			words[i] = nodes[i]->word;
		} else {
			words[i] = 0;
			assert(j < nparam);
			an->children[j++] = nodes[i];
		}
	}
	assert(j == nparam);
	an->predicate = find_predicate(lexer->program, n, words);
	assert(an->predicate->arity == nparam);

	return an;
}

static struct astnode *parse_rule(struct lexer *lexer, struct arena *arena) {
	int n = 0, i, j, nparam = 0;
	struct astnode *nodes[MAXRULEWORDS], *an;
	struct word *words[MAXRULEWORDS];

	for(;;) {
		status = next_token(lexer, PMODE_RULE);
		if(lexer->errorflag) return 0;
		if(!status) {
			report(LVL_ERR, line, "Unterminated rule expression.");
			lexer->errorflag = 1;
			return 0;
		}
		if(status == 2) {
			report(LVL_ERR, line, "Unexpected token at beginning of line.");
			lexer->errorflag = 1;
			return 0;
		}

		if(lexer->kind == ')') break;

		if(n >= MAXRULEWORDS) {
			report(LVL_ERR, line, "Too many words in predicate name.");
			lexer->errorflag = 1;
			return 0;
		}
		nodes[n] = parse_expr(PMODE_RULE, lexer, arena);
		if(!nodes[n]) return 0;
		n++;
	}

	for(i = 0; i < n; i++) {
		if(nodes[i]->kind != AN_BAREWORD) {
			nparam++;
		}
	}

	if(nparam > MAXPARAM) {
		report(LVL_ERR, line, "Predicates can have a maximum of %d parameters. Put some of them in a list.", MAXPARAM);
		lexer->errorflag = 1;
		return 0;
	}

	an = mkast(AN_RULE, nparam, arena, line);
	j = 0;
	for(i = 0; i < n; i++) {
		if(nodes[i]->kind == AN_BAREWORD) {
			words[i] = nodes[i]->word;
		} else {
			words[i] = 0;
			assert(j < nparam);
			an->children[j++] = nodes[i];
		}
	}
	assert(j == nparam);
	an->predicate = find_predicate(lexer->program, n, words);
	assert(an->predicate->arity == nparam);

	return an;
}

static struct clause *parse_clause(int is_macro, struct lexer *lexer) {
	struct astnode *an, **dest, **folddest;
	struct astnode *nestednodes[MAXNESTEDEXPR];
	int nnested = 0, nested_nonvar = 0;
	struct clause *cl;
	int i;
	struct predname *predname;

	if(is_macro) {
		an = parse_rule(lexer, &lexer->temp_arena);
	} else {
		an = parse_rule_head(nestednodes, &nnested, &nested_nonvar, lexer, &lexer->temp_arena);
	}
	if(!an) return 0;

	predname = an->predicate;

	if(predname->arity > MAXPARAM) {
		report(LVL_ERR, line, "Maximum number of arguments exceeded.");
		lexer->errorflag = 1;
		return 0;
	}

	if(nested_nonvar
	&& predname->special != SP_GLOBAL_VAR
	&& predname->special != SP_GENERATE) {
		report(LVL_ERR, an->line, "First parameter of nested rule must be a variable.");
		lexer->errorflag = 1;
		return 0;
	}

	if(predname->special && !(predname->nameflags & PREDNF_META)) {
		report(LVL_ERR, line, "Special syntax cannot be redefined.");
		lexer->errorflag = 1;
		return 0;
	} else {
		cl = arena_calloc(&predname->pred->arena, sizeof(*cl));
		cl->predicate = predname;
		cl->arena = &predname->pred->arena;
		cl->line = line;
		cl->params = arena_alloc(cl->arena, predname->arity * sizeof(struct astnode *));
		for(i = 0; i < an->nchild; i++) {
			assert(an->children[i]->kind != AN_BAREWORD);
			cl->params[i] = deepcopy_astnode(an->children[i], cl->arena, 0);
		}
		dest = &cl->body;
		for(i = 0; i < nnested; i++) {
			an = deepcopy_astnode(nestednodes[i], cl->arena, 0);
			*dest = an;
			dest = &an->next_in_body;
		}
		folddest = dest;
		for(;;) {
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				*dest = 0;
				break;
			}
			an = parse_expr(PMODE_BODY, lexer, cl->arena);
			if(!an) return 0;
			*dest = an;
			dest = &an->next_in_body;
		}
		*folddest = fold_disjunctions(*folddest, lexer, cl->arena);
	}

	return cl;
}

static struct astnode *parse_if(struct lexer *lexer, struct arena *arena) {
	struct astnode *ifnode, *sub, **dest;

	ifnode = mkast(AN_IF, 3, arena, line);

	dest = &ifnode->children[0];
	for(;;) {
		status = next_token(lexer, PMODE_BODY);
		if(lexer->errorflag) return 0;
		if(status != 1) {
			report(LVL_ERR, ifnode->line, "(if) without (then).");
			lexer->errorflag = 1;
			return 0;
		}
		sub = parse_expr(PMODE_BODY, lexer, arena);
		if(!sub) return 0;
		if(sub->kind == AN_RULE && sub->predicate->special == SP_THEN) {
			*dest = 0;
			break;
		}
		*dest = sub;
		dest = &sub->next_in_body;
	}
	ifnode->children[0] = fold_disjunctions(ifnode->children[0], lexer, arena);
	dest = &ifnode->children[1];
	for(;;) {
		status = next_token(lexer, PMODE_BODY);
		if(lexer->errorflag) return 0;
		if(status != 1) {
			report(LVL_ERR, line, "Unterminated (then)-expression.");
			lexer->errorflag = 1;
			return 0;
		}
		sub = parse_expr(PMODE_BODY, lexer, arena);
		if(!sub) return 0;
		if(sub->kind == AN_RULE && sub->predicate->special == SP_ENDIF) {
			*dest = 0;
			ifnode->children[2] = 0;
			break;
		} else if(sub->kind == AN_RULE && sub->predicate->special == SP_ELSE) {
			*dest = 0;
			dest = &ifnode->children[2];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (else)-expression.");
					lexer->errorflag = 1;
					return 0;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				if(sub->kind == AN_RULE && sub->predicate->special == SP_ENDIF) {
					*dest = 0;
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			break;
		} else if(sub->kind == AN_RULE && sub->predicate->special == SP_ELSEIF) {
			*dest = 0;
			ifnode->children[2] = parse_if(lexer, arena);
			break;
		}
		*dest = sub;
		dest = &sub->next_in_body;
	}
	ifnode->children[1] = fold_disjunctions(ifnode->children[1], lexer, arena);
	ifnode->children[2] = fold_disjunctions(ifnode->children[2], lexer, arena);

	return ifnode;
}

static struct astnode *parse_expr(int parsemode, struct lexer *lexer, struct arena *arena) {
	struct astnode *an, *sub, **dest, *body;
	int i, id, have_arg, did_create;
	struct clause *cl;
	struct predname *predname;
	line_t orig_line;

	switch(lexer->kind) {
	case TOK_VARIABLE:
		an = mkast(AN_VARIABLE, 0, arena, line);
		an->word = lexer->word;
		break;
	case TOK_TAG:
		an = mkast(AN_TAG, 0, arena, line);
		an->word = lexer->word;
		break;
	case TOK_BAREWORD:
		if(parsemode == PMODE_VALUE) {
			an = mkast(AN_DICTWORD, 0, arena, line);
		} else {
			an = mkast(AN_BAREWORD, 0, arena, line);
			if(parsemode == PMODE_BODY
			&& (isalnum(lexer->word->name[0]) || lexer->word->name[1])) {
				lexer->wordcount++;
				lexer->word->flags |= WORDF_OUTPUT;
			}
		}
		an->word = lexer->word;
		break;
	case TOK_DICTWORD:
		an = mkast(AN_DICTWORD, 0, arena, line);
		an->word = lexer->word;
		break;
	case TOK_INTEGER:
		an = mkast(AN_INTEGER, 0, arena, line);
		an->value = lexer->value;
		break;
	case TOK_STARPAREN:
		if(parsemode != PMODE_BODY) {
			report(LVL_ERR, line, "Nested rules are only allowed inside rule heads.");
			lexer->errorflag = 1;
			return 0;
		}
		an = parse_rule(lexer, arena);
		if(!an) return 0;
		if(an->predicate->special) {
			report(LVL_ERR, line, "Syntax error.");
			lexer->errorflag = 1;
			return 0;
		}
		an->subkind = RULE_MULTI;
		break;
	case '*':
		if(!starword) {
			report(LVL_ERR, line, "Star syntax used before current topic has been defined.");
			lexer->errorflag = 1;
			return 0;
		}
		an = mkast(AN_TAG, 0, arena, line);
		an->word = starword;
		break;
	case '(':
		if(parsemode != PMODE_BODY) {
			report(LVL_ERR, line, "Nested rules are only allowed inside rule heads.");
			lexer->errorflag = 1;
			return 0;
		}
		an = parse_rule(lexer, arena);
		if(!an) return 0;
		if(an->predicate->special == SP_NOW) {
			an = mkast(AN_NOW, 1, arena, line);
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (now).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[0] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[0]) return 0;
			if(an->children[0]->kind == AN_NEG_RULE
			&& an->children[0]->predicate->builtin == BI_HASPARENT
			&& an->children[0]->children[0]->kind == AN_VARIABLE
			&& !an->children[0]->children[0]->word->name[0]) {
				// (now) ~($ has parent $)
				sub = mkast(AN_BLOCK, 1, arena, line);
				sub->children[0] = mkast(AN_RULE, 1, arena, line);
				sub->children[0]->subkind = RULE_MULTI;
				sub->children[0]->predicate = find_builtin(lexer->program, BI_OBJECT);
				sub->children[0]->children[0] = mkast(AN_VARIABLE, 0, arena, line);
				sub->children[0]->children[0]->word = fresh_word(lexer->program);
				sub->children[0]->next_in_body = an;
				an->children[0]->children[0] = deepcopy_astnode(sub->children[0]->children[0], arena, 0);
				an = mkast(AN_EXHAUST, 1, arena, line);
				an->children[0] = sub;
			}
		} else if(an->predicate->special == SP_EXHAUST) {
			an = mkast(AN_EXHAUST, 1, arena, line);
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (exhaust).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[0] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[0]) return 0;
		} else if(an->predicate->special == SP_JUST) {
			an = mkast(AN_JUST, 0, arena, line);
		} else if(an->predicate->special == SP_SELECT) {
			an = mkast(AN_SELECT, 1, arena, line);
			an->value = -1;
			dest = &an->children[0];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated select.");
					lexer->errorflag = 1;
					return 0;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				if(sub->kind == AN_RULE && sub->predicate->special == SP_OR) {
					*dest = 0;
					dest = arena_alloc(arena, (an->nchild + 1) * sizeof(struct astnode *));
					memcpy(dest, an->children, an->nchild * sizeof(struct astnode *));
					an->children = dest;
					dest = &an->children[an->nchild++];
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_STOPPING) {
					*dest = 0;
					an->subkind = SEL_STOPPING;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_RANDOM) {
					*dest = 0;
					an->subkind = SEL_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_P_RANDOM) {
					*dest = 0;
					an->subkind = SEL_P_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_T_RANDOM) {
					*dest = 0;
					an->subkind = SEL_T_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_T_P_RANDOM) {
					*dest = 0;
					an->subkind = SEL_T_P_RANDOM;
					break;
				} else if(sub->kind == AN_RULE && sub->predicate->special == SP_CYCLING) {
					*dest = 0;
					an->subkind = SEL_CYCLING;
					break;
				} else {
					*dest = sub;
					dest = &sub->next_in_body;
				}
			}
			for(i = 0; i < an->nchild; i++) {
				an->children[i] = fold_disjunctions(an->children[i], lexer, arena);
			}
		} else if(an->predicate->special == SP_COLLECT) {
			sub = an;
			an = mkast(AN_COLLECT, 3, arena, line);
			an->children[1] = sub->children[0];
			dest = &an->children[0];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (collect $).");
					lexer->errorflag = 1;
					return 0;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				if(sub->kind == AN_RULE && sub->predicate->special == SP_INTO) {
					*dest = 0;
					an->children[2] = sub->children[0];
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[0] = fold_disjunctions(an->children[0], lexer, arena);
		} else if(an->predicate->special == SP_COLLECT_WORDS) {
			an = mkast(AN_COLLECT_WORDS, 2, arena, line);
			dest = &an->children[0];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (collect words).");
					lexer->errorflag = 1;
					return 0;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				if(sub->kind == AN_RULE && sub->predicate->special == SP_INTO) {
					*dest = 0;
					an->children[1] = sub->children[0];
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[0] = fold_disjunctions(an->children[0], lexer, arena);
		} else if(an->predicate->special == SP_DETERMINE_OBJECT) {
			sub = an->children[0];
			if(sub->kind != AN_VARIABLE || !sub->word->name[0]) {
				report(LVL_ERR, line, "Expected variable name in (determine object $).");
				lexer->errorflag = 1;
				return 0;
			}
			an = mkast(AN_DETERMINE_OBJECT, 4, arena, line);
			an->children[0] = sub;
			dest = &an->children[1];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (determine object $).");
					lexer->errorflag = 1;
					return 0;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				if(sub->kind == AN_RULE && sub->predicate->special == SP_FROM_WORDS) {
					*dest = 0;
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[1] = fold_disjunctions(an->children[1], lexer, arena);
			dest = &an->children[2];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated (determine object $).");
					lexer->errorflag = 1;
					return 0;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				if(sub->kind == AN_RULE && sub->predicate->special == SP_MATCHING_ALL_OF) {
					*dest = 0;
					an->children[3] = sub->children[0];
					break;
				}
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[2] = fold_disjunctions(an->children[2], lexer, arena);
		} else if(an->predicate->special == SP_STOPPABLE) {
			an = mkast(AN_STOPPABLE, 1, arena, line);
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (stoppable).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[0] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[0]) return 0;
			if(contains_just(an->children[0])) {
				report(LVL_ERR, line, "(just) not allowed inside (stoppable).");
				lexer->errorflag = 1;
				return 0;
			}
		} else if(an->predicate->special == SP_STATUSBAR) {
			sub = an->children[0];
			an = mkast(AN_OUTPUTBOX, 2, arena, line);
			an->subkind = BOX_STATUS;
			an->children[0] = sub;
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (status bar $).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[1] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[1]) return 0;
			if(contains_just(an->children[1])) {
				report(LVL_ERR, line, "(just) not allowed inside (status bar $).");
				lexer->errorflag = 1;
				return 0;
			}
		} else if(an->predicate->special == SP_OUTPUTBOX) {
			sub = an->children[0];
			an = mkast(AN_OUTPUTBOX, 2, arena, line);
			an->subkind = BOX_DIV;
			an->children[0] = sub;
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (div $).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[1] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[1]) return 0;
			if(contains_just(an->children[1])) {
				report(LVL_ERR, line, "(just) not allowed inside (div $).");
				lexer->errorflag = 1;
				return 0;
			}
		} else if(an->predicate->special == SP_LINK_SELF) {
			an = mkast(AN_LINK_SELF, 1, arena, line);
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (link).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[0] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[0]) return 0;
			if(contains_just(an->children[0])) {
				report(LVL_ERR, line, "(just) not allowed inside (link).");
				lexer->errorflag = 1;
				return 0;
			}
			if(an->children[0]->kind != AN_BAREWORD) {
				sub = an->children[0];
				if(sub->kind == AN_BLOCK && sub->children[0]) {
					sub = sub->children[0];
					while(sub && sub->kind == AN_BAREWORD) {
						sub = sub->next_in_body;
					}
				}
				if(sub) {
					report(LVL_ERR, line, "(link) may only be followed by a block of words or a single word.");
					lexer->errorflag = 1;
					return 0;
				}
			}
		} else if(an->predicate->special == SP_LINK) {
			sub = an->children[0];
			an = mkast(AN_LINK, 2, arena, line);
			an->children[0] = sub;
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (link $).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[1] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[1]) return 0;
			if(contains_just(an->children[1])) {
				report(LVL_ERR, line, "(just) not allowed inside (link $).");
				lexer->errorflag = 1;
				return 0;
			}
		} else if(an->predicate->special == SP_LINK_RES) {
			sub = an->children[0];
			an = mkast(AN_BLOCK, 1, arena, line);
			an->children[0] = mkast(AN_RULE, 2, arena, line);
			an->children[0]->subkind = RULE_SIMPLE;
			an->children[0]->predicate = find_builtin(lexer->program, BI_RESOLVERESOURCE);
			an->children[0]->children[0] = sub;
			an->children[0]->children[1] = mkast(AN_VARIABLE, 0, arena, line);
			an->children[0]->children[1]->word = fresh_word(lexer->program);
			sub = mkast(AN_LINK_RES, 2, arena, line);
			an->children[0]->next_in_body = sub;
			sub->children[0] = mkast(AN_VARIABLE, 0, arena, line);
			sub->children[0]->word = an->children[0]->children[1]->word;
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (link resource $).");
				lexer->errorflag = 1;
				return 0;
			}
			sub->children[1] = parse_expr(PMODE_BODY, lexer, arena);
			if(!sub->children[1]) return 0;
			if(contains_just(sub->children[1])) {
				report(LVL_ERR, line, "(just) not allowed inside (link resource $).");
				lexer->errorflag = 1;
				return 0;
			}
		} else if(an->predicate->special == SP_LOG) {
			an = mkast(AN_LOG, 1, arena, line);
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Expected expression after (log).");
				lexer->errorflag = 1;
				return 0;
			}
			an->children[0] = parse_expr(PMODE_BODY, lexer, arena);
			if(!an->children[0]) return 0;
			if(contains_just(an->children[0])) {
				report(LVL_ERR, line, "(just) not allowed inside (log).");
				lexer->errorflag = 1;
				return 0;
			}
		} else if(an->predicate->special == SP_IF) {
			an = parse_if(lexer, arena);
			if(!an) return 0;
		}
		break;
	case '{':
		if(parsemode == PMODE_BODY) {
			an = mkast(AN_BLOCK, 1, arena, line);
			dest = &an->children[0];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated block.");
					lexer->errorflag = 1;
					return 0;
				}
				if(lexer->kind == '}') {
					*dest = 0;
					break;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[0] = fold_disjunctions(an->children[0], lexer, arena);
			if(an->children[0] && !an->children[0]->next_in_body) {
				an = an->children[0];
			}
		} else {
			// Closure
			if(lexer->string) {
				report(LVL_ERR, line, "Closures cannot be defined interactively.");
				lexer->errorflag = 1;
				return 0;
			}
			orig_line = line;
			body = 0;
			dest = &body;
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, orig_line, "Unterminated block in closure.");
					lexer->errorflag = 1;
					return 0;
				}
				if(lexer->kind == '}') {
					*dest = 0;
					break;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				*dest = sub;
				dest = &sub->next_in_body;
			}
			body = fold_disjunctions(body, lexer, arena);
			id = find_closurebody(lexer->program, body, &did_create);
			an = mkast(AN_PAIR, 2, arena, line);
			an->children[0] = mkast(AN_INTEGER, 0, arena, orig_line);
			an->children[0]->value = id;
			an->children[1] = mkast(AN_EMPTY_LIST, 0, arena, orig_line);
			predname = find_builtin(lexer->program, BI_INVOKE_CLOSURE);
			if(did_create) {
				cl = arena_calloc(&predname->pred->arena, sizeof(*cl));
				cl->predicate = predname;
				cl->arena = &predname->pred->arena;
				cl->line = orig_line;
				cl->params = arena_alloc(cl->arena, predname->arity * sizeof(struct astnode *));
				cl->params[0] = mkast(AN_INTEGER, 0, cl->arena, orig_line);
				cl->params[0]->value = id;
				cl->params[1] = mkast(AN_EMPTY_LIST, 0, cl->arena, orig_line); // placeholder
				cl->params[2] = mkast(AN_VARIABLE, 0, cl->arena, orig_line);
				cl->params[2]->word = find_word(lexer->program, "");
				cl->body = deepcopy_astnode(body, cl->arena, 0);
				add_clause(cl, predname->pred);
				analyse_clause(lexer->program, cl);
				assert(cl == predname->pred->clauses[id]);
			} else {
				cl = predname->pred->clauses[id];
			}
			have_arg = 0;
			for(i = 0; i < cl->nvar; i++) {
				if(!strcmp(cl->varnames[i]->name, "_")) {
					have_arg = 1;
				} else {
					sub = an->children[1];
					an->children[1] = mkast(AN_PAIR, 2, cl->arena, orig_line);
					an->children[1]->children[0] = mkast(AN_VARIABLE, 0, cl->arena, orig_line);
					an->children[1]->children[0]->word = cl->varnames[i];
					an->children[1]->children[1] = sub;
				}
			}
			if(did_create) {
				cl->params[1] = deepcopy_astnode(an->children[1], cl->arena, 0);
				if(have_arg) {
					cl->params[2]->word = find_word(lexer->program, "_");
				}
			}
		}
		break;
	case '~':
		if(parsemode != PMODE_BODY) {
			report(LVL_ERR, line, "Nested rules are only allowed inside rule heads.");
			lexer->errorflag = 1;
			return 0;
		}
		status = next_token(lexer, PMODE_BODY);
		if(lexer->errorflag) return 0;
		if(status != 1 || (lexer->kind != '(' && lexer->kind != '{')) {
			report(LVL_ERR, line, "Syntax error after ~.");
			lexer->errorflag = 1;
			return 0;
		}
		if(lexer->kind == '(') {
			an = parse_rule(lexer, arena);
			if(!an) return 0;
			if(an->predicate->special) {
				report(LVL_ERR, line, "Special syntax cannot be negated.");
				lexer->errorflag = 1;
				return 0;
			}
			an->kind = AN_NEG_RULE;
		} else {
			an = mkast(AN_NEG_BLOCK, 1, arena, line);
			dest = &an->children[0];
			for(;;) {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Unterminated block.");
					lexer->errorflag = 1;
					return 0;
				}
				if(lexer->kind == '}') {
					*dest = 0;
					break;
				}
				sub = parse_expr(PMODE_BODY, lexer, arena);
				if(!sub) return 0;
				*dest = sub;
				dest = &sub->next_in_body;
			}
			an->children[0] = fold_disjunctions(an->children[0], lexer, arena);
			if(an->children[0] && !an->children[0]->next_in_body && an->children[0]->kind == AN_RULE) {
				an = an->children[0];
				an->kind = AN_NEG_RULE;
			}
		}
		break;
	case '[':
		dest = &an;
		for(;;) {
			status = next_token(lexer, PMODE_VALUE);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Unterminated list.");
				lexer->errorflag = 1;
				return 0;
			}
			if(lexer->kind == ']') {
				*dest = mkast(AN_EMPTY_LIST, 0, arena, line);
				break;
			} else if(lexer->kind == '|') {
				status = next_token(lexer, PMODE_VALUE);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Syntax error near end of list.");
					lexer->errorflag = 1;
					return 0;
				}
				*dest = parse_expr(PMODE_VALUE, lexer, arena);
				if(!*dest) return 0;
				status = next_token(lexer, PMODE_VALUE);
				if(lexer->errorflag) return 0;
				if(status != 1 || lexer->kind != ']') {
					report(LVL_ERR, line, "Syntax error near end of list.");
					lexer->errorflag = 1;
					return 0;
				}
				break;
			} else {
				sub = mkast(AN_PAIR, 2, arena, line);
				sub->children[0] = parse_expr(PMODE_VALUE, lexer, arena);
				if(!sub->children[0]) return 0;
				*dest = sub;
				dest = &sub->children[1];
			}
		}
		break;
	default:
		report(LVL_ERR, line, "Unexpected %c.", lexer->kind);
		lexer->errorflag = 1;
		return 0;
	}

	assert(an);
	return an;
}

static struct astnode *parse_expr_nested(
	struct astnode **nested_rules,
	int *nnested,
	int parsemode,
	int *nonvar_detected,
	struct lexer *lexer,
	struct arena *arena)
{
	struct astnode *an, *var, *sub, **dest, *list;
	int negated = 0;

	switch(lexer->kind) {
	case '~':
		status = next_token(lexer, PMODE_RULE);
		if(lexer->errorflag) return 0;
		if(status != 1 || lexer->kind != '(') {
			report(LVL_ERR, line, "Expected ( after ~ in rule-head.");
			lexer->errorflag = 1;
			return 0;
		}
		negated = 1;
		/* drop through */
	case '(':
	case TOK_STARPAREN:
		if(lexer->kind == TOK_STARPAREN) {
			an = parse_rule(lexer, arena);
			if(!an) return 0;
			an->subkind = RULE_MULTI;
		} else {
			an = parse_rule(lexer, arena);
			if(!an) return 0;
		}
		if(an->predicate->special) {
			report(LVL_ERR, line, "Special rule can't appear nested.");
			lexer->errorflag = 1;
			return 0;
		}
		if(negated) an->kind = AN_NEG_RULE;
		if(!an->nchild) {
			report(LVL_ERR, line, "Nested rule must have at least one parameter.");
			lexer->errorflag = 1;
			return 0;
		}
		if(*nnested >= MAXNESTEDEXPR) {
			report(LVL_ERR, an->line, "Too many nested expressions in rule head.");
			lexer->errorflag = 1;
			return 0;
		}
		if(an->children[0]->kind == AN_VARIABLE) {
			var = an->children[0];
			if(!var->word->name[0]) {
				var->word = fresh_word(lexer->program);
			}
			nested_rules[(*nnested)++] = an;
			an = mkast(AN_VARIABLE, 0, arena, line);
			an->word = var->word;
			return an;
		} else {
			*nonvar_detected = 1;
			nested_rules[(*nnested)++] = an;
			return an->children[0];
		}
		break;
	case '[':
		dest = &an;
		for(;;) {
			status = next_token(lexer, PMODE_VALUE);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, line, "Unterminated list.");
				lexer->errorflag = 1;
				return 0;
			}
			if(lexer->kind == ']') {
				*dest = mkast(AN_EMPTY_LIST, 0, arena, line);
				break;
			} else if(lexer->kind == '|') {
				status = next_token(lexer, PMODE_VALUE);
				if(lexer->errorflag) return 0;
				if(status != 1) {
					report(LVL_ERR, line, "Syntax error near end of list.");
					lexer->errorflag = 1;
					return 0;
				}
				*dest = parse_expr_nested(nested_rules, nnested, PMODE_VALUE, nonvar_detected, lexer, arena);
				if(!*dest) return 0;
				status = next_token(lexer, PMODE_VALUE);
				if(lexer->errorflag) return 0;
				if(status != 1 || lexer->kind != ']') {
					report(LVL_ERR, line, "Syntax error near end of list.");
					lexer->errorflag = 1;
					return 0;
				}
				break;
			} else {
				sub = mkast(AN_PAIR, 2, arena, line);
				sub->children[0] = parse_expr_nested(nested_rules, nnested, PMODE_VALUE, nonvar_detected, lexer, arena);
				if(!sub->children[0]) return 0;
				*dest = sub;
				dest = &sub->children[1];
			}
		}
		return an;
	case '{':
		report(LVL_ERR, line, "Unexpected block in rule-head.");
		lexer->errorflag = 1;
		return 0;
	default:
		an = parse_expr(parsemode, lexer, arena);
		if(!an) return 0;
		if(parsemode != PMODE_BODY
		&& an->kind != AN_BAREWORD
		&& look_ahead_for_slash(lexer)) {
			if(*nnested >= MAXNESTEDEXPR) {
				report(LVL_ERR, an->line, "Too many nested expressions in rule head.");
				lexer->errorflag = 1;
				return 0;
			}
			list = mkast(AN_PAIR, 2, arena, an->line);
			list->children[0] = an;
			dest = &list->children[1];
			do {
				status = next_token(lexer, parsemode);
				if(lexer->errorflag) return 0;
				assert(status && lexer->kind == '/');
				status = next_token(lexer, parsemode);
				if(lexer->errorflag) return 0;
				if(!status) {
					report(LVL_ERR, list->line, "Syntax error near end of slash-expression.");
					lexer->errorflag = 1;
					return 0;
				}
				an = parse_expr(parsemode, lexer, arena);
				if(!an) return 0;
				if(an->kind == AN_VARIABLE
				|| an->kind == AN_PAIR
				|| an->kind == AN_BAREWORD) {
					report(LVL_ERR, an->line, "Invalid kind of value inside a slash-expression.");
					lexer->errorflag = 1;
					return 0;
				}
				*dest = mkast(AN_PAIR, 2, arena, an->line);
				(*dest)->children[0] = an;
				dest = &(*dest)->children[1];
			} while(look_ahead_for_slash(lexer));
			*dest = mkast(AN_EMPTY_LIST, 0, arena, an->line);
			var = mkast(AN_VARIABLE, 0, arena, list->line);
			var->word = fresh_word(lexer->program);
			an = mkast(AN_RULE, 2, arena, list->line);
			an->subkind = RULE_MULTI;
			an->predicate = find_builtin(lexer->program, BI_IS_ONE_OF);
			an->children[0] = mkast(AN_VARIABLE, 0, arena, list->line);
			an->children[0]->word = var->word;
			an->children[1] = list;
			nested_rules[(*nnested)++] = an;
			an = var;
		}
		return an;
	}
}

int parse_file(struct lexer *lexer, int filenum, struct clause ***clause_dest_ptr) {
	struct clause *clause;
	struct astnode *an;
	int i;

	line = MKLINE(filenum, 1);
	column = 0;
	lexer->wordcount = 0;

	status = next_token(lexer, PMODE_RULE);
	if(lexer->errorflag) return 0;
	if(status == 1) {
		report(LVL_ERR, line, "Code must begin in leftmost column.");
		return 0;
	}

	while(status) {
		if(lexer->kind == TOK_TAG) {
			starword = lexer->word;
			status = next_token(lexer, PMODE_RULE);
			if(lexer->errorflag) return 0;
			if(status == 1) {
				report(LVL_ERR, line, "Unexpected expression after declaration of current topic.");
				return 0;
			}
		} else if(lexer->kind == TOK_DICTWORD) {
			if(lexer->word->name[0]) {
				report(LVL_ERR, line, "Expected ( after @.");
				return 0;
			}
			status = next_token(lexer, PMODE_RULE);
			if(lexer->errorflag) return 0;
			if(!status) {
				report(LVL_ERR, line, "Unexpected end of file.");
				return 0;
			}
			if(lexer->kind != '(') {
				report(LVL_ERR, line, "Expected ( after @.");
				return 0;
			}
			clause = parse_clause(1, lexer);
			if(!clause) {
				return 0;
			}
			if(clause->predicate->builtin) {
				report(
					LVL_ERR,
					line,
					"Access predicate definition collides with built-in predicate: %s",
					clause->predicate->printed_name);
				return 0;
			}
			if(clause->predicate->pred->macrodef) {
				report(
					LVL_ERR,
					line,
					"Multiple access predicate definitions with the same name: @%s",
					clause->predicate->printed_name);
				return 0;
			}
			for(i = 0; i < clause->predicate->arity; i++) {
				if(clause->params[i]->kind != AN_VARIABLE) {
					report(
						LVL_ERR,
						clause->line,
						"Parameters of access predicate heads must be variables (parameter #%d).",
						i + 1);
					break;
				}
			}
			if(i < clause->predicate->arity) {
				return 0;
			}
			for(an = clause->body; an; an = an->next_in_body) {
				if(an->kind != AN_RULE
				&& an->kind != AN_NEG_RULE) {
					report(
						LVL_ERR,
						clause->line,
						"Access predicate body must be a simple conjunction of queries.");
					break;
				}
			}
			if(an) {
				return 0;
			}
			clause->predicate->pred->macrodef = clause;
			clause->predicate->pred->flags |= PREDF_MACRO;
		} else {
			if(lexer->kind == '~') {
				status = next_token(lexer, PMODE_RULE);
				if(lexer->errorflag) return 0;
				if(!status) {
					report(LVL_ERR, line, "Unexpected end of file.");
					return 0;
				} else if(lexer->kind != '(') {
					report(LVL_ERR, line, "Expected ( after ~.");
					return 0;
				}
				clause = parse_clause(0, lexer);
				if(!clause) {
					return 0;
				}
				clause->negated = 1;
			} else if(lexer->kind == '(') {
				clause = parse_clause(0, lexer);
				if(!clause) {
					return 0;
				}
			} else {
				report(LVL_ERR, line, "Bad kind of expression at beginning of line.");
				return 0;
			}
			if(clause->predicate->builtin == BI_QUERY || (
				clause->predicate->builtin
				&& clause->predicate->builtin != BI_HASPARENT
				&& !(clause->predicate->nameflags & (PREDNF_META | PREDNF_DEFINABLE_BI))))
			{
				report(LVL_ERR, line, "Rule definition collides with built-in predicate.");
				return 0;
			}
			**clause_dest_ptr = clause;
			*clause_dest_ptr = &clause->next_in_source;
		}
	}

	report(LVL_INFO, 0, "Word count for \"%s\": %d", sourcefile[filenum], lexer->wordcount);

	lexer->totallines += LINEPART(line);
	lexer->totalwords += lexer->wordcount;

	return !lexer->errorflag;
}

struct astnode *parse_injected_query(struct lexer *lexer, struct predicate *pred) {
	struct astnode *an, *body, **andest = &body;

	line = 0;
	column = 1;
	lexer->wordcount = 0;

	status = next_token(lexer, PMODE_RULE);
	assert(!lexer->errorflag);
	do {
		assert(status == 1);
		assert(lexer->kind == '(' || lexer->kind == TOK_STARPAREN);
		if(lexer->kind == '(') {
			an = parse_rule(lexer, &pred->arena);
			if(!an) return 0;
		} else {
			an = parse_rule(lexer, &pred->arena);
			if(!an) return 0;
			an->subkind = RULE_MULTI;
		}
		assert(an->kind == AN_RULE);
		if(an->predicate->special == SP_NOW) {
			an = mkast(AN_NOW, 1, &pred->arena, 0);
			status = next_token(lexer, PMODE_BODY);
			if(lexer->errorflag) return 0;
			if(status != 1) {
				report(LVL_ERR, 0, "Expected expression after (now).");
				return 0;
			}
			if(lexer->kind == '(') {
				an->children[0] = parse_rule(lexer, &pred->arena);
				if(!an->children[0]) return 0;
			} else if(lexer->kind == '~') {
				status = next_token(lexer, PMODE_BODY);
				if(lexer->errorflag) return 0;
				if(status != 1 || lexer->kind != '(') {
					report(LVL_ERR, 0, "Expected ( after ~.");
					return 0;
				}
				an->children[0] = parse_rule(lexer, &pred->arena);
				if(!an->children[0]) return 0;
				an->children[0]->kind = AN_NEG_RULE;
			} else {
				report(LVL_ERR, 0, "Syntax error in '(now)'-expression.");
				return 0;
			}
			if(an->children[0]->predicate->special) {
				report(LVL_ERR, 0, "Syntax error in '(now)'-expression.");
				return 0;
			}
		} else if(an->predicate->special) {
			report(LVL_ERR, 0, "Only queries, multi-queries, and '(now)'-expressions may be entered interactively.");
			return 0;
		}
		*andest = an;
		andest = &an->next_in_body;
		status = next_token(lexer, PMODE_BODY);
		if(lexer->errorflag) return 0;
		if(status) {
			if(lexer->kind != '(' && lexer->kind != TOK_STARPAREN) {
				report(LVL_ERR, 0, "Only queries, multi-queries, and '(now)'-expressions may be entered interactively.");
				return 0;
			}
		}
	} while(status == 1);
	*andest = 0;

	assert(body);
	return body;
}
