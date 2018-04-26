// This program is protected under the GNU GPL (See COPYING)

#include "defs.h"

#include <pcre.h>
#include <sys/types.h>

DO_COMMAND(do_regexp) {
  char left[BUFFER_SIZE], right[BUFFER_SIZE], is_true[BUFFER_SIZE],
      is_false[BUFFER_SIZE];

  arg = get_arg_in_braces(ses, arg, left, 0);
  arg = get_arg_in_braces(ses, arg, right, 0);
  arg = get_arg_in_braces(ses, arg, is_true, 1);
  arg = get_arg_in_braces(ses, arg, is_false, 1);

  if (*is_true == 0) {
    display_printf2(ses,
                    "SYNTAX: #REGEXP {string} {expression} {true} {false}.");
  } else {
    substitute(ses, left, left, SUB_VAR | SUB_FUN);
    substitute(ses, right, right, SUB_VAR | SUB_FUN);

    if (regexp(ses, NULL, left, right, 0, SUB_CMD)) {
      substitute(ses, is_true, is_true, SUB_CMD);

      ses = script_driver(ses, -1, is_true);
    } else if (*is_false) {
      ses = script_driver(ses, -1, is_false);
    }
  }
  return ses;
}

int match(struct session *ses, char *str, char *exp, int flags) {
  char expbuf[BUFFER_SIZE];

  sprintf(expbuf, "^%s$", exp);

  substitute(ses, expbuf, expbuf, flags);

  return regexp(ses, NULL, str, expbuf, 0, 0);
}

int find(struct session *ses, char *str, char *exp, int sub) {
  char expbuf[BUFFER_SIZE], strbuf[BUFFER_SIZE];

  if (ses) {
    substitute(ses, str, strbuf, SUB_VAR | SUB_FUN);
    substitute(ses, exp, expbuf, SUB_VAR | SUB_FUN);

    return regexp(ses, NULL, strbuf, expbuf, 0, sub);
  } else {
    return regexp(ses, NULL, str, exp, 0, sub);
  }
}

int regexp_compare(pcre *nodepcre, char *str, char *exp, int option, int flag) {
  pcre *regex;
  const char *error;
  int i, j, matches, match[303];

  if (nodepcre == NULL) {
    regex = pcre_compile(exp, option, &error, &i, NULL);
  } else {
    regex = nodepcre;
  }

  if (regex == NULL) {
    return FALSE;
  }

  matches = pcre_exec(regex, NULL, str, strlen(str), 0, 0, match, 303);

  if (matches <= 0) {
    if (nodepcre == NULL) {
      free(regex);
    }
    return FALSE;
  }

  switch (flag) {
  case SUB_CMD:
    for (i = 0; i < matches; i++) {
      gtd->cmds[i] =
          refstring(gtd->cmds[i], "%.*s", match[i * 2 + 1] - match[i * 2],
                    &str[match[i * 2]]);
    }
    break;

  case SUB_CMD + SUB_FIX:
    for (i = 0; i < matches; i++) {
      j = gtd->args[i];

      gtd->cmds[j] =
          refstring(gtd->cmds[j], "%.*s", match[i * 2 + 1] - match[i * 2],
                    &str[match[i * 2]]);
    }
    break;

  case SUB_ARG:
    for (i = 0; i < matches; i++) {
      gtd->vars[i] =
          refstring(gtd->vars[i], "%.*s", match[i * 2 + 1] - match[i * 2],
                    &str[match[i * 2]]);
    }
    break;

  case SUB_ARG + SUB_FIX:
    for (i = 0; i < matches; i++) {
      j = gtd->args[i];

      gtd->vars[j] =
          refstring(gtd->vars[j], "%.*s", match[i * 2 + 1] - match[i * 2],
                    &str[match[i * 2]]);
    }
    break;
  }

  if (nodepcre == NULL) {
    free(regex);
  }

  return TRUE;
}

// copy *string into *result, but substitute the various expressions with the
// values they stand for.
int substitute(struct session *ses, char *string, char *result, int flags) {
  char temp[BUFFER_SIZE], buf[BUFFER_SIZE], buffer[BUFFER_SIZE], *pti, *pto,
      *ptt;
  char old[6] = {0};
  int i, cnt, flags_neol = flags;

  pti = string;
  pto = (string == result) ? buffer : result;

  DEL_BIT(flags_neol, SUB_EOL | SUB_LNF);

  while (TRUE) {
    switch (*pti) {
    case '\0':
      if (HAS_BIT(flags, SUB_EOL)) {
        *pto++ = '\r';
      }

      if (HAS_BIT(flags, SUB_LNF)) {
        *pto++ = '\n';
      }

      *pto = 0;

      if (string == result) {
        strcpy(result, buffer);

        return pto - buffer;
      } else {
        return pto - result;
      }
      break;
    case '$':
      if (HAS_BIT(flags, SUB_VAR) &&
          (pti[1] == DEFAULT_OPEN || isalpha((int)pti[1]) || pti[1] == '$')) {
        int def = FALSE;

        if (pti[1] == '$') {
          while (pti[1] == '$') {
            *pto++ = *pti++;
          }

          if (pti[1] == DEFAULT_OPEN || isalnum((int)pti[1])) {
            pti++;
          } else {
            *pto++ = *pti++;
          }
          continue;
        }

        pti++;

        if (*pti == DEFAULT_OPEN) {
          def = TRUE;

          pti = get_arg_in_braces(ses, pti, buf, TRUE);

          substitute(ses, buf, temp, flags_neol);
        } else {
          ptt = temp;

          while (isalnum((int)*pti) || *pti == '_') {
            *ptt++ = *pti++;
          }
          *ptt = 0;
        }

        pti = get_arg_at_brackets(ses, pti, temp + strlen(temp));

        substitute(ses, temp, buf, flags_neol);

        substitute(ses, temp, pto, flags_neol - SUB_VAR);

        pto += strlen(pto);
      } else {
        *pto++ = *pti++;
      }
      break;
    case '<':
      if (HAS_BIT(flags, SUB_COL)) {
        if (HAS_BIT(flags, SUB_CMP) && !strncmp(old, pti, 5)) {
          pti += 5;
        } else if (isdigit((int)pti[1]) && isdigit((int)pti[2]) &&
                   isdigit((int)pti[3]) && pti[4] == '>') {
          if (pti[1] != '8' || pti[2] != '8' || pti[3] != '8') {
            *pto++ = ESCAPE;
            *pto++ = '[';

            switch (pti[1]) {
            case '2':
              *pto++ = '2';
              *pto++ = '2';
              *pto++ = ';';
              break;
            case '8':
              break;
            default:
              *pto++ = pti[1];
              *pto++ = ';';
            }
            switch (pti[2]) {
            case '8':
              break;
            default:
              *pto++ = '3';
              *pto++ = pti[2];
              *pto++ = ';';
              break;
            }
            switch (pti[3]) {
            case '8':
              break;
            default:
              *pto++ = '4';
              *pto++ = pti[3];
              *pto++ = ';';
              break;
            }
            pto--;
            *pto++ = 'm';
          }
          pti += sprintf(old, "<%c%c%c>", pti[1], pti[2], pti[3]);
        } else if (pti[1] >= 'a' && pti[1] <= 'f' && pti[2] >= 'a' &&
                   pti[2] <= 'f' && pti[3] >= 'a' && pti[3] <= 'f' &&
                   pti[4] == '>') {
          *pto++ = ESCAPE;
          *pto++ = '[';
          *pto++ = '3';
          *pto++ = '8';
          *pto++ = ';';
          *pto++ = '5';
          *pto++ = ';';
          cnt = 16 + (pti[1] - 'a') * 36 + (pti[2] - 'a') * 6 + (pti[3] - 'a');
          *pto++ = '0' + cnt / 100;
          *pto++ = '0' + cnt % 100 / 10;
          *pto++ = '0' + cnt % 10;
          *pto++ = 'm';
          pti += sprintf(old, "<%c%c%c>", pti[1], pti[2], pti[3]);
        } else if (pti[1] >= 'A' && pti[1] <= 'F' && pti[2] >= 'A' &&
                   pti[2] <= 'F' && pti[3] >= 'A' && pti[3] <= 'F' &&
                   pti[4] == '>') {
          *pto++ = ESCAPE;
          *pto++ = '[';
          *pto++ = '4';
          *pto++ = '8';
          *pto++ = ';';
          *pto++ = '5';
          *pto++ = ';';
          cnt = 16 + (pti[1] - 'A') * 36 + (pti[2] - 'A') * 6 + (pti[3] - 'A');
          *pto++ = '0' + cnt / 100;
          *pto++ = '0' + cnt % 100 / 10;
          *pto++ = '0' + cnt % 10;
          *pto++ = 'm';
          pti += sprintf(old, "<%c%c%c>", pti[1], pti[2], pti[3]);
        } else if (pti[1] == 'g' && isdigit((int)pti[2]) &&
                   isdigit((int)pti[3]) && pti[4] == '>') {
          *pto++ = ESCAPE;
          *pto++ = '[';
          *pto++ = '3';
          *pto++ = '8';
          *pto++ = ';';
          *pto++ = '5';
          *pto++ = ';';
          cnt = 232 + (pti[2] - '0') * 10 + (pti[3] - '0');
          *pto++ = '0' + cnt / 100;
          *pto++ = '0' + cnt % 100 / 10;
          *pto++ = '0' + cnt % 10;
          *pto++ = 'm';
          pti += sprintf(old, "<%c%c%c>", pti[1], pti[2], pti[3]);
        } else if (pti[1] == 'G' && isdigit((int)pti[2]) &&
                   isdigit((int)pti[3]) && pti[4] == '>') {
          *pto++ = ESCAPE;
          *pto++ = '[';
          *pto++ = '4';
          *pto++ = '8';
          *pto++ = ';';
          *pto++ = '5';
          *pto++ = ';';
          cnt = 232 + (pti[2] - '0') * 10 + (pti[3] - '0');
          *pto++ = '0' + cnt / 100;
          *pto++ = '0' + cnt % 100 / 10;
          *pto++ = '0' + cnt % 10;
          *pto++ = 'm';
          pti += sprintf(old, "<%c%c%c>", pti[1], pti[2], pti[3]);
        } else {
          *pto++ = *pti++;
        }
      } else {
        *pto++ = *pti++;
      }
      break;
    case '%':
      if (HAS_BIT(flags, SUB_ARG) && (isdigit((int)pti[1]) || pti[1] == '%')) {
        if (pti[1] == '%') {
          while (pti[1] == '%') {
            *pto++ = *pti++;
          }
          pti++;
        } else {
          i = isdigit((int)pti[2]) ? (pti[1] - '0') * 10 + pti[2] - '0'
                                   : pti[1] - '0';

          ptt = gtd->vars[i];

          while (*ptt) {
            if (HAS_BIT(flags, SUB_SEC)) {
              switch (*ptt) {
              case '\\':
                *pto++ = '\\';
                *pto++ = '\\';
                break;

              case '{':
                *pto++ = '\\';
                *pto++ = 'x';
                *pto++ = '7';
                *pto++ = 'B';
                break;

              case '}':
                *pto++ = '\\';
                *pto++ = 'x';
                *pto++ = '7';
                *pto++ = 'D';
                break;

              case COMMAND_SEPARATOR:
                *pto++ = '\\';
                *pto++ = COMMAND_SEPARATOR;
                break;

              default:
                *pto++ = *ptt;
                break;
              }
              ptt++;
            } else {
              *pto++ = *ptt++;
            }
          }
          pti += isdigit((int)pti[2]) ? 3 : 2;
        }
      } else {
        *pto++ = *pti++;
      }
      break;

    case '&':
      if (HAS_BIT(flags, SUB_CMD) && (isdigit((int)pti[1]) || pti[1] == '&')) {
        if (pti[1] == '&') {
          while (pti[1] == '&') {
            *pto++ = *pti++;
          }
          if (isdigit((int)pti[1])) {
            pti++;
          } else {
            *pto++ = *pti++;
          }
        } else {
          i = isdigit((int)pti[2]) ? (pti[1] - '0') * 10 + pti[2] - '0'
                                   : pti[1] - '0';

          for (cnt = 0; gtd->cmds[i][cnt]; cnt++) {
            *pto++ = gtd->cmds[i][cnt];
          }
          pti += isdigit((int)pti[2]) ? 3 : 2;
        }
      } else if (HAS_BIT(flags, SUB_VAR) &&
                 (pti[1] == DEFAULT_OPEN || isalpha((int)pti[1]) ||
                  pti[1] == '&')) {
        int def = 0;

        if (pti[1] == '&') {
          while (pti[1] == '&') {
            *pto++ = *pti++;
          }

          if (pti[1] == DEFAULT_OPEN || isalnum((int)pti[1])) {
            pti++;
          } else {
            *pto++ = *pti++;
          }
          continue;
        }

        pti++;

        if (*pti == DEFAULT_OPEN) {
          def = TRUE;

          pti = get_arg_in_braces(ses, pti, buf, TRUE);

          substitute(ses, buf, temp, flags_neol);
        } else {
          for (ptt = temp; isalnum((int)*pti) || *pti == '_'; i++) {
            *ptt++ = *pti++;
          }
          *ptt = 0;
        }

        pti = get_arg_at_brackets(ses, pti, temp + strlen(temp));

        substitute(ses, temp, buf, flags_neol);

        substitute(ses, temp, pto, flags_neol - SUB_VAR);

        pto += strlen(pto);
      } else {
        *pto++ = *pti++;
      }
      break;

    case '\\':
      if (HAS_BIT(flags, SUB_ESC)) {
        pti++;

        switch (*pti) {
        case 'a':
          *pto++ = '\a';
          break;
        case 'b':
          *pto++ = '\b';
          break;
        case 'c':
          if (pti[1]) {
            pti++;
            *pto++ = *pti % 32;
          }
          break;
        case 'e':
          *pto++ = '\033';
          break;
        case 'n':
          *pto++ = '\n';
          break;
        case 'r':
          *pto++ = '\r';
          break;
        case 't':
          *pto++ = '\t';
          break;
        case 'x':
          if (pti[1] && pti[2]) {
            pti++;
            *pto++ = hex_number(pti);
            pti++;
          }
          break;
        case '0':
          if (pti[1] && pti[2]) {
            pti++;
            *pto++ = oct_number(pti);
            pti++;
          }
          break;
        case '\0':
          DEL_BIT(flags, SUB_EOL);
          DEL_BIT(flags, SUB_LNF);
          continue;
        default:
          *pto++ = *pti;
          break;
        }
        pti++;
      } else {
        *pto++ = *pti++;
      }
      break;

    case ESCAPE:
      *pto++ = *pti++;
      break;

    default:
      if (HAS_BIT(flags, SUB_SEC) && !HAS_BIT(flags, SUB_ARG)) {
        switch (*pti) {
        case '\\':
          *pto++ = '\\';
          *pto++ = '\\';
          break;

        case '{':
          *pto++ = '\\';
          *pto++ = 'x';
          *pto++ = '7';
          *pto++ = 'B';
          break;

        case '}':
          *pto++ = '\\';
          *pto++ = 'x';
          *pto++ = '7';
          *pto++ = 'D';
          break;

        case COMMAND_SEPARATOR:
          *pto++ = '\\';
          *pto++ = COMMAND_SEPARATOR;
          break;

        default:
          *pto++ = *pti;
          break;
        }
        pti++;
      } else {
        *pto++ = *pti++;
      }
      break;
    }
  }
}

// Calls regexp checking if the string matches, and automatically fills
// in the text represented by the wildcards on success.
int check_one_regexp(struct session *ses, struct listnode *node, char *line,
                     char *original, int option) {
  char *exp, *str;

  if (node->regex == NULL) {
    char result[BUFFER_SIZE];

    substitute(ses, node->left, result, SUB_VAR | SUB_FUN);

    exp = result;
  } else {
    exp = node->left;
  }

  if (HAS_BIT(node->flags, NODE_FLAG_META)) {
    exp++;
    str = original;
  } else {
    str = line;
  }

  return regexp(ses, node->regex, str, exp, option, SUB_ARG);
}

int regexp(struct session *ses, pcre *nodepcre, char *str, char *exp,
           int option, int flag) {
  char out[BUFFER_SIZE], *pti, *pto;
  int arg = 1, var = 1, fix = 0;

  pti = exp;
  pto = out;

  while (*pti == '^') {
    *pto++ = *pti++;
  }

  while (*pti) {
    switch (pti[0]) {
    case '\\':
      *pto++ = *pti++;
      *pto++ = *pti++;
      break;

    case '{':
      gtd->args[up(var)] = up(arg);
      *pto++ = '(';
      pti = get_arg_in_braces(ses, pti, pto, TRUE);
      pto += strlen(pto);
      *pto++ = ')';
      break;

    case '[':
    case ']':
    case '(':
    case ')':
    case '|':
    case '.':
    case '?':
    case '+':
    case '*':
    case '^':
      *pto++ = '\\';
      *pto++ = *pti++;
      break;

    case '$':
      if (pti[1] != DEFAULT_OPEN && !isalnum((int)pti[1])) {
        int i = 0;

        while (pti[++i] == '$') {
          continue;
        }

        if (pti[i]) {
          *pto++ = '\\';
        }
      }
      *pto++ = *pti++;
      break;

    case '%':
      switch (pti[1]) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        fix = SUB_FIX;
        arg = isdigit((int)pti[2]) ? (pti[1] - '0') * 10 + (pti[2] - '0')
                                   : pti[1] - '0';
        gtd->args[up(var)] = up(arg);
        pti += isdigit((int)pti[2]) ? 3 : 2;
        strcpy(pto, *pti == 0 ? "(.*)" : "(.*?)");
        pto += strlen(pto);
        break;

      case 'd':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "([0-9]*)" : "([0-9]*?)");
        pto += strlen(pto);
        break;

      case 'D':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "([^0-9]*)" : "([^0-9]*?)");
        pto += strlen(pto);
        break;

      case 'i':
        pti += 2;
        strcpy(pto, "(?i)");
        pto += strlen(pto);
        break;

      case 'I':
        pti += 2;
        strcpy(pto, "(?-i)");
        pto += strlen(pto);
        break;

      case 's':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "(\\s*)" : "(\\s*?)");
        pto += strlen(pto);
        break;

      case 'S':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "(\\S*)" : "(\\S*?)");
        pto += strlen(pto);
        break;

      case 'w':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "([a-zA-Z]*)" : "([a-zA-Z]*?)");
        pto += strlen(pto);
        break;

      case 'W':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "([^a-zA-Z]*)" : "([^a-zA-Z]*?)");
        pto += strlen(pto);
        break;

      case '?':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "(.?)"
                              : "(.?"
                                "?)");
        pto += strlen(pto);
        break;

      case '*':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "(.*)" : "(.*?)");
        pto += strlen(pto);
        break;

      case '+':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, *pti == 0 ? "(.+)" : "(.+?)");
        pto += strlen(pto);
        break;

      case '.':
        gtd->args[up(var)] = up(arg);
        pti += 2;
        strcpy(pto, "(.)");
        pto += strlen(pto);
        break;

      case '%':
        *pto++ = *pti++;
        pti++;
        break;

      default:
        *pto++ = *pti++;
        break;
      }
      break;

    default:
      *pto++ = *pti++;
      break;
    }
  }
  *pto = 0;

  return regexp_compare(nodepcre, str, out, option, flag + fix);
}
