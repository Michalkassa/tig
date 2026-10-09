#ifndef GRAPH_H
#define GRAPH_H

/* 1 if stdout is a terminal and NO_COLOR is unset. */
int use_color(void);

/* " (HEAD -> main, feature)" for refs pointing at hash, or "" if none. Caller frees. */
char *ref_decorations(const char *hash, int color);

/* Draws every branch's history as an ASCII tree. */
void print_graph(void);

#endif
