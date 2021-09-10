#include <stdio.h>
#include <string.h>

#include "../include/tree.h"
#include "../include/rmdir.h"

extern Node *cwd, *root, *start;

void rmdir(char *path) {
    Node *p;
    if (path[0] == '/')
        p = find_node(path, root);
    else
        p = find_node(path, cwd);
    if (p == NULL) {
        printf("Directory %s not found\n", path);
        return;
    }
    if (p->type == 'F') {
        printf("%s is not a directory\n", path);
        return;
    }

    if (!strcmp(path, "/") || !strcmp(path, cwd->name)) {
        printf("Cannot remove %s\n", path);
        return;
    }

    if (p->child != NULL) {
        printf("Directry not empty!\n");
        return;
    }
    delete(p);
}
