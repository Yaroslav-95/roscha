#include "tests/tests.h"
#include "lexer.h"

#include <string.h>

#include "slice.h"
#include "token.h"

static void
test_next_token(void)
{
	char *input = "{% extends \"template\" %}\n"
				  "{% block rooster %}\n"
				  "{% if true %}\n"
				  "some content\n"
				  "{% elif not false %}\n"
				  "other content\n"
				  "{% else %}\n"
				  "yet something else\n"
				  "{% endif %}\n"
				  "{% for v in list %}\n"
				  "{{ v+(1-2)*4/5 }}\n"
				  "{% break %}\n"
				  "{% endfor %}\n"
				  "{% endblock %}\n"
				  "{{ list[1] }}\n"
				  "{{ map.value }}\n"
				  "{{ 5 < 10 }}\n"
				  "{{ 10 > 5 }}\n"
				  "{{ 5 <= 10 }}\n"
				  "{{ 10 >= 5 }}\n"
				  "{{ 10 != 5 }}\n"
				  "{{ 5 == 5 }}\n"
				  "{{ 5 and 5 }}\n"
				  "{{ 5 or 5 }}\n";

	token_init_keywords();
	struct lexer *lexer = lexer_new(input);
	struct token expected[] = {
		{ TOKEN_LBRACE, slice_whole("{"), 1, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 1, 2 },
		{ TOKEN_EXTENDS, slice_whole("extends"), 1, 4 },
		{ TOKEN_STRING, slice_whole("\"template\""), 1, 12 },
		{ TOKEN_PERCENT, slice_whole("%"), 1, 23 },
		{ TOKEN_RBRACE, slice_whole("}"), 1, 24 },
		{ TOKEN_CONTENT, slice_whole("\n"), 1, 25 },

		{ TOKEN_LBRACE, slice_whole("{"), 2, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 2, 2 },
		{ TOKEN_BLOCK, slice_whole("block"), 2, 4 },
		{ TOKEN_IDENT, slice_whole("rooster"), 2, 10 },
		{ TOKEN_PERCENT, slice_whole("%"), 2, 18 },
		{ TOKEN_RBRACE, slice_whole("}"), 2, 19 },
		{ TOKEN_CONTENT, slice_whole("\n"), 2, 20 },

		{ TOKEN_LBRACE, slice_whole("{"), 3, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 3, 2 },
		{ TOKEN_IF, slice_whole("if"), 3, 4 },
		{ TOKEN_TRUE, slice_whole("true"), 3, 7 },
		{ TOKEN_PERCENT, slice_whole("%"), 3, 12 },
		{ TOKEN_RBRACE, slice_whole("}"), 3, 13 },
		{ TOKEN_CONTENT, slice_whole("\nsome content\n"), 3, 14 },

		{ TOKEN_LBRACE, slice_whole("{"), 5, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 5, 2 },
		{ TOKEN_ELIF, slice_whole("elif"), 5, 4 },
		{ TOKEN_NOT, slice_whole("not"), 5, 9 },
		{ TOKEN_FALSE, slice_whole("false"), 5, 12 },
		{ TOKEN_PERCENT, slice_whole("%"), 5, 18 },
		{ TOKEN_RBRACE, slice_whole("}"), 5, 19 },
		{ TOKEN_CONTENT, slice_whole("\nother content\n"), 5, 20 },

		{ TOKEN_LBRACE, slice_whole("{"), 7, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 7, 2 },
		{ TOKEN_ELSE, slice_whole("else"), 7, 4 },
		{ TOKEN_PERCENT, slice_whole("%"), 7, 9 },
		{ TOKEN_RBRACE, slice_whole("}"), 7, 10 },
		{ TOKEN_CONTENT, slice_whole("\nyet something else\n"), 7, 11 },

		{ TOKEN_LBRACE, slice_whole("{"), 9, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 9, 2 },
		{ TOKEN_ENDIF, slice_whole("endif"), 9, 4 },
		{ TOKEN_PERCENT, slice_whole("%"), 9, 10 },
		{ TOKEN_RBRACE, slice_whole("}"), 9, 11 },
		{ TOKEN_CONTENT, slice_whole("\n"), 9, 12 },

		{ TOKEN_LBRACE, slice_whole("{"), 10, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 10, 2 },
		{ TOKEN_FOR, slice_whole("for"), 10, 4 },
		{ TOKEN_IDENT, slice_whole("v"), 10, 8 },
		{ TOKEN_IN, slice_whole("in"), 10, 10 },
		{ TOKEN_IDENT, slice_whole("list"), 10, 13 },
		{ TOKEN_PERCENT, slice_whole("%"), 10, 18 },
		{ TOKEN_RBRACE, slice_whole("}"), 10, 19 },
		{ TOKEN_CONTENT, slice_whole("\n"), 10, 20 },

		{ TOKEN_LBRACE, slice_whole("{"), 11, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 11, 2 },
		{ TOKEN_IDENT, slice_whole("v"), 11, 4 },
		{ TOKEN_PLUS, slice_whole("+"), 11, 5 },
		{ TOKEN_LPAREN, slice_whole("("), 11, 6 },
		{ TOKEN_INT, slice_whole("1"), 11, 7 },
		{ TOKEN_MINUS, slice_whole("-"), 11, 8 },
		{ TOKEN_INT, slice_whole("2"), 11, 9 },
		{ TOKEN_RPAREN, slice_whole(")"), 11, 10 },
		{ TOKEN_ASTERISK, slice_whole("*"), 11, 11 },
		{ TOKEN_INT, slice_whole("4"), 11, 12 },
		{ TOKEN_SLASH, slice_whole("/"), 11, 13 },
		{ TOKEN_INT, slice_whole("5"), 11, 14 },
		{ TOKEN_RBRACE, slice_whole("}"), 11, 16 },
		{ TOKEN_RBRACE, slice_whole("}"), 11, 17 },
		{ TOKEN_CONTENT, slice_whole("\n"), 11, 18 },

		{ TOKEN_LBRACE, slice_whole("{"), 12, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 12, 2 },
		{ TOKEN_BREAK, slice_whole("break"), 12, 4 },
		{ TOKEN_PERCENT, slice_whole("%"), 12, 10 },
		{ TOKEN_RBRACE, slice_whole("}"), 12, 11 },
		{ TOKEN_CONTENT, slice_whole("\n"), 12, 12 },

		{ TOKEN_LBRACE, slice_whole("{"), 13, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 13, 2 },
		{ TOKEN_ENDFOR, slice_whole("endfor"), 13, 4 },
		{ TOKEN_PERCENT, slice_whole("%"), 13, 11 },
		{ TOKEN_RBRACE, slice_whole("}"), 13, 12 },
		{ TOKEN_CONTENT, slice_whole("\n"), 13, 13 },

		{ TOKEN_LBRACE, slice_whole("{"), 14, 1 },
		{ TOKEN_PERCENT, slice_whole("%"), 14, 2 },
		{ TOKEN_ENDBLOCK, slice_whole("endblock"), 14, 4 },
		{ TOKEN_PERCENT, slice_whole("%"), 14, 13 },
		{ TOKEN_RBRACE, slice_whole("}"), 14, 14 },
		{ TOKEN_CONTENT, slice_whole("\n"), 14, 15 },

		{ TOKEN_LBRACE, slice_whole("{"), 15, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 15, 2 },
		{ TOKEN_IDENT, slice_whole("list"), 15, 4 },
		{ TOKEN_LBRACKET, slice_whole("["), 15, 8 },
		{ TOKEN_INT, slice_whole("1"), 15, 9 },
		{ TOKEN_RBRACKET, slice_whole("]"), 15, 10 },
		{ TOKEN_RBRACE, slice_whole("}"), 15, 12 },
		{ TOKEN_RBRACE, slice_whole("}"), 15, 13 },
		{ TOKEN_CONTENT, slice_whole("\n"), 15, 14 },

		{ TOKEN_LBRACE, slice_whole("{"), 16, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 16, 2 },
		{ TOKEN_IDENT, slice_whole("map"), 16, 4 },
		{ TOKEN_DOT, slice_whole("."), 16, 7 },
		{ TOKEN_IDENT, slice_whole("value"), 16, 8 },
		{ TOKEN_RBRACE, slice_whole("}"), 16, 10 },
		{ TOKEN_RBRACE, slice_whole("}"), 16, 11 },
		{ TOKEN_CONTENT, slice_whole("\n"), 16, 12 },

		{ TOKEN_LBRACE, slice_whole("{"), 17, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 17, 2 },
		{ TOKEN_INT, slice_whole("5"), 17, 4 },
		{ TOKEN_LT, slice_whole("<"), 17, 6 },
		{ TOKEN_INT, slice_whole("10"), 17, 8 },
		{ TOKEN_RBRACE, slice_whole("}"), 17, 11 },
		{ TOKEN_RBRACE, slice_whole("}"), 17, 12 },
		{ TOKEN_CONTENT, slice_whole("\n"), 17, 13 },

		{ TOKEN_LBRACE, slice_whole("{"), 18, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 18, 2 },
		{ TOKEN_INT, slice_whole("10"), 18, 4 },
		{ TOKEN_GT, slice_whole(">"), 18, 7 },
		{ TOKEN_INT, slice_whole("5"), 18, 9 },
		{ TOKEN_RBRACE, slice_whole("}"), 18, 11 },
		{ TOKEN_RBRACE, slice_whole("}"), 18, 12 },
		{ TOKEN_CONTENT, slice_whole("\n"), 18, 13 },

		{ TOKEN_LBRACE, slice_whole("{"), 19, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 19, 2 },
		{ TOKEN_INT, slice_whole("5"), 19, 4 },
		{ TOKEN_LTE, slice_whole("<="), 19, 6 },
		{ TOKEN_INT, slice_whole("10"), 19, 9 },
		{ TOKEN_RBRACE, slice_whole("}"), 19, 12 },
		{ TOKEN_RBRACE, slice_whole("}"), 19, 13 },
		{ TOKEN_CONTENT, slice_whole("\n"), 19, 14 },

		{ TOKEN_LBRACE, slice_whole("{"), 20, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 20, 2 },
		{ TOKEN_INT, slice_whole("10"), 20, 4 },
		{ TOKEN_GTE, slice_whole(">="), 20, 7 },
		{ TOKEN_INT, slice_whole("5"), 20, 10 },
		{ TOKEN_RBRACE, slice_whole("}"), 20, 12 },
		{ TOKEN_RBRACE, slice_whole("}"), 20, 13 },
		{ TOKEN_CONTENT, slice_whole("\n"), 20, 14 },

		{ TOKEN_LBRACE, slice_whole("{"), 21, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 21, 2 },
		{ TOKEN_INT, slice_whole("10"), 21, 4 },
		{ TOKEN_NOTEQ, slice_whole("!="), 21, 7 },
		{ TOKEN_INT, slice_whole("5"), 21, 10 },
		{ TOKEN_RBRACE, slice_whole("}"), 21, 12 },
		{ TOKEN_RBRACE, slice_whole("}"), 21, 13 },
		{ TOKEN_CONTENT, slice_whole("\n"), 21, 14 },

		{ TOKEN_LBRACE, slice_whole("{"), 22, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 22, 2 },
		{ TOKEN_INT, slice_whole("5"), 22, 4 },
		{ TOKEN_EQ, slice_whole("=="), 22, 6 },
		{ TOKEN_INT, slice_whole("5"), 22, 9 },
		{ TOKEN_RBRACE, slice_whole("}"), 22, 11 },
		{ TOKEN_RBRACE, slice_whole("}"), 22, 12 },
		{ TOKEN_CONTENT, slice_whole("\n"), 22, 13 },

		{ TOKEN_LBRACE, slice_whole("{"), 23, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 23, 2 },
		{ TOKEN_INT, slice_whole("5"), 23, 4 },
		{ TOKEN_AND, slice_whole("and"), 23, 6 },
		{ TOKEN_INT, slice_whole("5"), 23, 10 },
		{ TOKEN_RBRACE, slice_whole("}"), 23, 12 },
		{ TOKEN_RBRACE, slice_whole("}"), 23, 13 },
		{ TOKEN_CONTENT, slice_whole("\n"), 23, 14 },

		{ TOKEN_LBRACE, slice_whole("{"), 24, 1 },
		{ TOKEN_LBRACE, slice_whole("{"), 24, 2 },
		{ TOKEN_INT, slice_whole("5"), 24, 4 },
		{ TOKEN_OR, slice_whole("or"), 24, 6 },
		{ TOKEN_INT, slice_whole("5"), 24, 9 },
		{ TOKEN_RBRACE, slice_whole("}"), 24, 11 },
		{ TOKEN_RBRACE, slice_whole("}"), 24, 12 },
		{ TOKEN_CONTENT, slice_whole("\n"), 24, 13 },

		{ TOKEN_EOF, },
	};
	size_t i = 0;

	do {
		struct token token = lexer_next_token(lexer);
		asserteq(token.type, expected[i].type);
		asserteq(slice_cmp(&token.literal, &expected[i].literal), 0);
		i++;
	} while (expected[i].type != TOKEN_EOF);

	lexer_destroy(lexer);
	token_free_keywords();
}

int
main(void)
{
	INIT_TESTS();
	RUN_TEST(test_next_token);
}
