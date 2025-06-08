// An nginx config file parser.
//
// See:
//   http://wiki.nginx.org/Configuration
//   http://blog.martinfjordvald.com/2010/07/nginx-primer/
//
// How Nginx does it:
//   http://lxr.nginx.org/source/src/core/ngx_conf_file.c

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>
#include "config_parser.h"
#include "logger.h"


const char *NginxConfigParser::TokenTypeAsString(TokenType type) {
  switch (type) {
  case TOKEN_TYPE_START:
    return "TOKEN_TYPE_START";
  case TOKEN_TYPE_NORMAL:
    return "TOKEN_TYPE_NORMAL";
  case TOKEN_TYPE_START_BLOCK:
    return "TOKEN_TYPE_START_BLOCK";
  case TOKEN_TYPE_END_BLOCK:
    return "TOKEN_TYPE_END_BLOCK";
  case TOKEN_TYPE_COMMENT:
    return "TOKEN_TYPE_COMMENT";
  case TOKEN_TYPE_STATEMENT_END:
    return "TOKEN_TYPE_STATEMENT_END";
  case TOKEN_TYPE_EOF:
    return "TOKEN_TYPE_EOF";
  case TOKEN_TYPE_ERROR:
    return "TOKEN_TYPE_ERROR";
  default:
    return "Unknown token type";
  }
}

void printStack(std::stack<char> s) {
  if (s.empty()) {
    BOOST_LOG_TRIVIAL(debug) << "";
    return;
  }
  int topElement = s.top();
  s.pop();
  std::stringstream ss;
  ss << static_cast<char>(topElement) << " ";
  BOOST_LOG_TRIVIAL(debug) << ss.str();
  printStack(s);
}

NginxConfigParser::TokenType
NginxConfigParser::ParseToken(std::istream *input, std::string *value,
                              TokenType last_token_type) {
  TokenParserState state = TOKEN_STATE_INITIAL_WHITESPACE;
  bool escape = false;
  while (input->good()) {
    const char c = input->get();
    if (!input->good()) {
      break;
    }
    switch (state) {
    case TOKEN_STATE_INITIAL_WHITESPACE:
      switch (c) {
      case '{':
        *value = c;
        return TOKEN_TYPE_START_BLOCK;
      case '}':
        *value = c;
        return TOKEN_TYPE_END_BLOCK;
      case '#':
        *value = c;
        state = TOKEN_STATE_TOKEN_TYPE_COMMENT;
        continue;
      case '"': // Explicitly reject quoted strings per API spec
      case '\'':
        *value = c;
        return TOKEN_TYPE_ERROR;
      case ';':
        *value = c;
        return TOKEN_TYPE_STATEMENT_END;
      case ' ':
      case '\t':
        continue;
      case '\n':
      case '\r':
        state = TOKEN_STATE_NEWLINE;
        continue;
      default:
        *value += c;
        state = TOKEN_STATE_TOKEN_TYPE_NORMAL;
        continue;
      }
    case TOKEN_STATE_TOKEN_TYPE_COMMENT:
      if (c == '\n' || c == '\r') {
        return TOKEN_TYPE_COMMENT;
      }
      *value += c;
      continue;
    case TOKEN_STATE_TOKEN_TYPE_NORMAL:
      if (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == ';' ||
          c == '{' || c == '}') {
        input->unget();
        return TOKEN_TYPE_NORMAL;
      }
      *value += c;
      continue;
    case TOKEN_STATE_NEWLINE:
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        // ignore whitespace after a newline token
        continue;
      }
      // but for other chars after newline, must have a semicolon preceeding it
      // (unless it is tokenstart)
      if (last_token_type == TOKEN_TYPE_START) {
        input->unget();
        state = TOKEN_STATE_INITIAL_WHITESPACE;
        continue;
      }
      if (last_token_type != TOKEN_TYPE_STATEMENT_END &&
          last_token_type != TOKEN_TYPE_START_BLOCK &&
          last_token_type != TOKEN_TYPE_END_BLOCK) {
        input->unget();
        return TOKEN_TYPE_ERROR;
      }
      input->unget();
      state = TOKEN_STATE_INITIAL_WHITESPACE;
      continue;
    }
  }

  return TOKEN_TYPE_EOF;
}

bool NginxConfigParser::Parse(std::istream *config_file, NginxConfig *config) {
  std::stack<NginxConfig *> config_stack;
  config_stack.push(config);
  TokenType last_token_type = TOKEN_TYPE_START;
  TokenType token_type;
  std::stack<char> brackets;
  while (true) {
    std::string token;
    token_type = ParseToken(config_file, &token, last_token_type);
    BOOST_LOG_TRIVIAL(debug) << TokenTypeAsString(token_type) << ": " << token;
    if (token_type == TOKEN_TYPE_ERROR) {
      break;
    }

    if (token_type == TOKEN_TYPE_COMMENT) {
      // Skip comments.
      continue;
    }

    if (token_type == TOKEN_TYPE_START) {
      // Error.
      break;
    } else if (token_type == TOKEN_TYPE_NORMAL) {
      if (last_token_type == TOKEN_TYPE_START ||
          last_token_type == TOKEN_TYPE_STATEMENT_END ||
          last_token_type == TOKEN_TYPE_START_BLOCK ||
          last_token_type == TOKEN_TYPE_END_BLOCK ||
          last_token_type == TOKEN_TYPE_NORMAL) {
        if (last_token_type != TOKEN_TYPE_NORMAL) {
          config_stack.top()->statements_.emplace_back(
              new NginxConfigStatement);
        }
        config_stack.top()->statements_.back().get()->tokens_.push_back(token);
      } else {
        // Error.
        break;
      }
    } else if (token_type == TOKEN_TYPE_STATEMENT_END) {
      if (last_token_type != TOKEN_TYPE_NORMAL) {
        // Error.
        break;
      }
    } else if (token_type == TOKEN_TYPE_START_BLOCK) {
      if (last_token_type != TOKEN_TYPE_NORMAL) {
        // Error.
        break;
      }
      NginxConfig *const new_config = new NginxConfig;
      config_stack.top()->statements_.back().get()->child_block_.reset(
          new_config);
      config_stack.push(new_config);

      // push new start block onto stack, print stack for debugging
      brackets.push('{');
      printStack(brackets);
    } else if (token_type == TOKEN_TYPE_END_BLOCK) {
      // Allow END_BLOCK if the current block is empty OR if the last token is a
      // statement end.
      config_stack.pop();
      if (last_token_type != TOKEN_TYPE_STATEMENT_END &&
          last_token_type != TOKEN_TYPE_END_BLOCK &&
          last_token_type != TOKEN_TYPE_START_BLOCK) {
        break;
      }

      if (!brackets.empty()) {
        brackets.pop();
        printStack(brackets);
      } else
        break;

    } else if (token_type == TOKEN_TYPE_EOF) {
      if (last_token_type != TOKEN_TYPE_STATEMENT_END &&
          last_token_type != TOKEN_TYPE_END_BLOCK) {
        // Error.
        break;
      }
      // printf("immediate before: ");
      printStack(brackets);
      if (brackets.empty() == false)
        break;
      return true;
    } else {
      // Error. Unknown token.
      break;
    }
    last_token_type = token_type;
  }

  if (last_token_type != TOKEN_TYPE_STATEMENT_END &&
      last_token_type != TOKEN_TYPE_END_BLOCK &&
      last_token_type != TOKEN_TYPE_START_BLOCK) {
    BOOST_LOG_TRIVIAL(error) << "Bad transition from " << TokenTypeAsString(last_token_type) << " to " << TokenTypeAsString(token_type);
    return false;
  }
  return false;
}

bool NginxConfigParser::Parse(const char *file_name, NginxConfig *config) {
  std::ifstream config_file;
  config_file.open(file_name);
  if (!config_file.good()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to open config file: " << file_name;
    return false;
  }

  const bool return_value =
      Parse(dynamic_cast<std::istream *>(&config_file), config);
  config_file.close();
  return return_value;
}
