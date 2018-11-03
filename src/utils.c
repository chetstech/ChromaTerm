/* This program is protected under the GNU GPL (See COPYING) */

#include "defs.h"

/* The outer-most brackets (if any) are stripped; all else left as is */
char *getArg(char *string, char *result) {
  char *pti = string, *pto = result;

  while (isspace((int)*pti)) { /* advance to the next none-space character */
    pti++;
  }

  /* Not wrapped in brackets; use space as separator */
  if (*pti != DEFAULT_OPEN) {
    while (*pti) {
      if (isspace((int)*pti)) {
        pti++;
        break;
      }
      *pto++ = *pti++;
    }
  } else { /* Wrapped in brackets; use outer-most as separator */
    int nest = 1;

    pti++; /* Advance past the DEFAULT_OPEN (nest is 1 for this reason) */

    while (*pti) {
      if (*pti == DEFAULT_OPEN) {
        nest++;
      } else if (*pti == DEFAULT_CLOSE) {
        nest--;

        /* Stop once we've got the close bracket for the first open bracket */
        if (nest == 0) {
          break;
        }
      }
      *pto++ = *pti++;
    }

    if (*pti == 0) {
      fprintf(stderr, "ERROR: Missing %i closing bracket(s)\n", nest);
    } else {
      pti++; /* Move over the closing bracket */
    }
  }

  *pto = '\0';
  return pti;
}

/* TRUE if s1 is an abbrevation of s2 (case-insensitive) */
int isAbbrev(char *s1, char *s2) {
  if (*s1 == 0) {
    return FALSE;
  }
  return !strncasecmp(s2, s1, strlen(s1));
}

/* if waitForNewLine, process lines until the one without \n at the end */
void processInput(int waitForNewLine) {
  char *line, *nextLine;

  gd.inputBuf[gd.inputBufLen] = 0;

  /* separate into lines and process. Next interation = next line */
  for (line = gd.inputBuf; line && *line; line = nextLine) {
    char lineBuf[INPUT_MAX * 2];

    nextLine = strchr(line, '\n');

    if (nextLine) {
      *nextLine = 0;             /* Replace \n with a null-terminator */
      nextLine++;                /* Move the pointer to just after that \n */
    } else if (waitForNewLine) { /* Reached the last line */
      strcpy(lineBuf, line);
      strcpy(gd.inputBuf, lineBuf);
      gd.inputBufLen = (int)strlen(lineBuf);

      return; /* Leave and wait until called again without having to wait */
    }

    /* Print the output after processing it */
    strcpy(lineBuf, line);
    highlightString(lineBuf);

    printf("%s%s", lineBuf, nextLine ? "\n" : "");

    fflush(stdout);
  }

  /* If we reached this point, then there's no more output in the buffer */
  gd.inputBufLen = 0;
}

void readConfig(char *file) {
  FILE *fp;
  struct stat fileData;
  char *bufi, *bufo, fileName[BUFFER_SIZE], *pti, *pto;
  int nest = 0, com = FALSE, lineNumber = 1;
  wordexp_t p;

  if (wordexp(file, &p, 0) == 0) {
    strcpy(fileName, *p.we_wordv);
    wordfree(&p);
  } else {
    fprintf(stderr, "ERROR: Failed while performing word expansion on {%s}\n",
            file);
    return;
  }

  if ((fp = fopen(fileName, "r")) == NULL) {
    fprintf(stderr, "ERROR: File {%s} not found\n", fileName);
    return;
  }

  stat(fileName, &fileData);

  if ((bufi = (char *)calloc(1, fileData.st_size + 2)) == NULL) {
    fprintf(stderr,
            "ERROR: Failed to allocate i_buffer memory to process file {%s}\n",
            fileName);
    fclose(fp);
    return;
  } else if ((bufo = (char *)calloc(1, fileData.st_size + 2)) == NULL) {
    fprintf(stderr,
            "ERROR: Failed to allocate o_buffer memory to process file {%s}\n",
            fileName);
    free(bufi);
    fclose(fp);
    return;
  }

  if (fread(bufi, 1, fileData.st_size, fp) == 0) {
    fprintf(stderr, "ERROR: File {%s} is empty\n", fileName);
    free(bufi);
    free(bufo);
    fclose(fp);
    return;
  };

  fclose(fp); /* Done with FILE */

  pti = bufi;
  pto = bufo;

  while (*pti) {
    if (com == FALSE) { /* Not in a comment */
      switch (*pti) {
      case DEFAULT_OPEN:
        *pto++ = *pti++;
        nest++;
        break;
      case DEFAULT_CLOSE:
        *pto++ = *pti++;
        nest--;
        break;
      case '/':                           /* Check if comment */
        if (nest == 0 && pti[1] == '*') { /* Not in an argument */
          pti += 2;
          com = TRUE;
        } else {
          *pto++ = *pti++;
        }
        break;
      case '\r': /* skip \r (we expect \n) */
        pti++;
        break;
      case '\n':
        if (nest) { /* Closing brackets missing; remove command */
          char *previousLine = strrchr(bufo, '\n');

          fprintf(stderr, "ERROR: Missing %i closing bracket(s) at line %i\n",
                  nest, lineNumber);

          nest = 0; /* Reset the level to 0 (brackets are all matched) */

          if (previousLine) {   /* There's a previous line */
            pto = previousLine; /* Go back to the last sane line */
          } else { /* Go back to beginning of bufo (no previous line) */
            pto = bufo;
          }
        }

        lineNumber++;
        *pto++ = *pti++;
        break;
      default:
        *pto++ = *pti++;
        break;
      }
    } else { /* In a comment */
      switch (*pti) {
      case '*':
        if (pti[1] == '/') { /* Comment close */
          pti += 2;
          com = FALSE;
        } else {
          pti++;
        }
        break;
      case '\n':
        lineNumber++;
        pti++;
        break;
      default: /* Advance forward (we're in a comment) */
        pti++;
        break;
      }
    }
  }

  *pto++ = '\n'; /* Ensure there's a \n at the end of the last command */
  *pto = 0;

  pti = bufo;

  while (*pti) {
    char *args, command[BUFFER_SIZE];

    while (isspace((int)*pti)) {
      pti++;
    }

    if (*pti == 0) {
      break;
    }

    pto = pti;               /* Start of command */
    pti = strchr(pti, '\n'); /* End of command */
    *pti = 0;                /* Replace \n with null */
    pti++; /* Move to the position after the null-terminator */

    if (strlen(pto) > BUFFER_SIZE) {
      *(pto + 20) = 0; /* Only output the first 20 characters  */
      fprintf(stderr, "ERROR: Command too long {%s}\n", pto);
      continue;
    }

    args = getArg(pto, command);

    if (isAbbrev(command, "HIGHLIGHT")) {
      char condition[BUFFER_SIZE], action[BUFFER_SIZE], priority[BUFFER_SIZE];

      args = getArg(args, condition);
      args = getArg(args, action);
      getArg(args, priority);

      highlight(condition, action, priority);
    } else if (isAbbrev(command, "SHOWME")) {
      char buf[BUFFER_SIZE];

      strcpy(buf, pto);

      highlightString(buf);
      fprintf(stderr, "%s\n", buf);
    } else if (isAbbrev(command, "UNHIGHLIGHT")) {
      getArg(args, args);
      unhighlight(args);
    } else {
      fprintf(stderr, "ERROR: Unknown command {%s}\n", command);
    }
  }

  free(bufi);
  free(bufo);
}

struct regExRes regExCompare(PCRE_CODE *compiledRegEx, char *str) {
  struct regExRes res;
#ifdef HAVE_PCRE2_H
  PCRE2_SIZE *resPos;
  pcre2_match_data *match =
      pcre2_match_data_create_from_pattern(compiledRegEx, NULL);

  if (pcre2_match(compiledRegEx, (PCRE2_SPTR)str, (int)strlen(str), 0, 0, match,
                  NULL) <= 0) {
    pcre2_match_data_free(match);
    res.start = -1;
    return res;
  }

  resPos = pcre2_get_ovector_pointer(match);

  res.start = (int)resPos[0];
  res.end = (int)resPos[1];

  pcre2_match_data_free(match);
#else
  int resPos[600];

  if (pcre_exec(compiledRegEx, NULL, str, (int)strlen(str), 0, 0, resPos,
                600) <= 0) {
    res.start = -1;
    return res;
  }

  res.start = (int)resPos[0];
  res.end = (int)resPos[1];
#endif

  if (res.start > res.end) {
    res.start = -1;
    return res;
  }

  return res;
}
