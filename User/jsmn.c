#include "jsmn.h"

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser,
																	 jsmntok_t *tokens,
																	 size_t num_tokens)
{
	jsmntok_t *tok = NULL;

	if (parser->toknext >= num_tokens)
	{
		return NULL;
	}

	tok = &tokens[parser->toknext];
	tok->start = -1;
	tok->end = -1;
	tok->size = 0;
	tok->parent = -1;
	tok->type = JSMN_UNDEFINED;
	parser->toknext++;
	return tok;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type, int start, int end)
{
	token->type = type;
	token->start = start;
	token->end = end;
	token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *parser,
																const char *js,
																size_t len,
																jsmntok_t *tokens,
																size_t num_tokens)
{
	int start = (int)parser->pos;

	for (; parser->pos < len; parser->pos++)
	{
		char c = js[parser->pos];

		switch (c)
		{
		case '\t':
		case '\r':
		case '\n':
		case ' ':
		case ',':
		case ']':
		case '}':
			goto found;
		default:
			if ((unsigned char)c < 32u || (unsigned char)c >= 127u)
			{
				parser->pos = (unsigned int)start;
				return JSMN_ERROR_INVAL;
			}
			break;
		}
	}

found:
	if (tokens == NULL)
	{
		parser->pos--;
		return 0;
	}

	jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
	if (token == NULL)
	{
		parser->pos = (unsigned int)start;
		return JSMN_ERROR_NOMEM;
	}

	jsmn_fill_token(token, JSMN_PRIMITIVE, start, (int)parser->pos);
	token->parent = parser->toksuper;
	parser->pos--;
	return 0;
}

static int jsmn_parse_string(jsmn_parser *parser,
														 const char *js,
														 size_t len,
														 jsmntok_t *tokens,
														 size_t num_tokens)
{
	int start = (int)parser->pos;
	parser->pos++;

	for (; parser->pos < len; parser->pos++)
	{
		char c = js[parser->pos];

		if (c == '\"')
		{
			if (tokens == NULL)
			{
				return 0;
			}

			jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
			if (token == NULL)
			{
				parser->pos = (unsigned int)start;
				return JSMN_ERROR_NOMEM;
			}

			jsmn_fill_token(token, JSMN_STRING, start + 1, (int)parser->pos);
			token->parent = parser->toksuper;
			return 0;
		}

		if (c == '\\')
		{
			parser->pos++;
			if (parser->pos == len)
			{
				parser->pos = (unsigned int)start;
				return JSMN_ERROR_PART;
			}

			switch (js[parser->pos])
			{
			case '\"':
			case '/':
			case '\\':
			case 'b':
			case 'f':
			case 'r':
			case 'n':
			case 't':
				break;
			case 'u':
			{
				int i = 0;
				for (i = 0; i < 4; i++)
				{
					parser->pos++;
					if (parser->pos == len)
					{
						parser->pos = (unsigned int)start;
						return JSMN_ERROR_PART;
					}

					char ch = js[parser->pos];
					if (!((ch >= '0' && ch <= '9') ||
								(ch >= 'A' && ch <= 'F') ||
								(ch >= 'a' && ch <= 'f')))
					{
						parser->pos = (unsigned int)start;
						return JSMN_ERROR_INVAL;
					}
				}
				break;
			}
			default:
				parser->pos = (unsigned int)start;
				return JSMN_ERROR_INVAL;
			}
		}
	}

	parser->pos = (unsigned int)start;
	return JSMN_ERROR_PART;
}

void jsmn_init(jsmn_parser *parser)
{
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;
}

int jsmn_parse(jsmn_parser *parser,
							 const char *js,
							 size_t len,
							 jsmntok_t *tokens,
							 unsigned int num_tokens)
{
	int count = (int)parser->toknext;
	unsigned int i = 0u;

	for (; parser->pos < len; parser->pos++)
	{
		char c = js[parser->pos];
		jsmntok_t *tok = NULL;

		switch (c)
		{
		case '{':
		case '[':
			count++;
			if (tokens == NULL)
			{
				break;
			}

			tok = jsmn_alloc_token(parser, tokens, num_tokens);
			if (tok == NULL)
			{
				return JSMN_ERROR_NOMEM;
			}

			if (parser->toksuper != -1)
			{
				jsmntok_t *parent = &tokens[parser->toksuper];
				parent->size++;
				tok->parent = parser->toksuper;
			}

			tok->type = (c == '{') ? JSMN_OBJECT : JSMN_ARRAY;
			tok->start = (int)parser->pos;
			parser->toksuper = (int)(parser->toknext - 1u);
			break;

		case '}':
		case ']':
			if (tokens == NULL)
			{
				break;
			}

			{
				jsmntype_t type = (c == '}') ? JSMN_OBJECT : JSMN_ARRAY;
				for (i = parser->toknext; i > 0u; i--)
				{
					tok = &tokens[i - 1u];
					if (tok->start != -1 && tok->end == -1)
					{
						if (tok->type != type)
						{
							return JSMN_ERROR_INVAL;
						}
						tok->end = (int)parser->pos + 1;
						parser->toksuper = tok->parent;
						break;
					}
				}
				if (i == 0u)
				{
					return JSMN_ERROR_INVAL;
				}
			}
			break;

		case '\"':
			count++;
			{
				int ret = jsmn_parse_string(parser, js, len, tokens, num_tokens);
				if (ret < 0)
				{
					return ret;
				}
				if (parser->toksuper != -1 && tokens != NULL)
				{
					tokens[parser->toksuper].size++;
				}
			}
			break;

		case '\t':
		case '\r':
		case '\n':
		case ' ':
		case ':':
		case ',':
			break;

		default:
			count++;
			{
				int ret = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
				if (ret < 0)
				{
					return ret;
				}
				if (parser->toksuper != -1 && tokens != NULL)
				{
					tokens[parser->toksuper].size++;
				}
			}
			break;
		}
	}

	if (tokens != NULL)
	{
		for (i = parser->toknext; i > 0u; i--)
		{
			if (tokens[i - 1u].start != -1 && tokens[i - 1u].end == -1)
			{
				return JSMN_ERROR_PART;
			}
		}
	}

	return count;
}
