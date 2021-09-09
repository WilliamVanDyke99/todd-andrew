#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "../include/tree.h"
#include "../include/cd.h"

extern Node *root, *cwd, *start;

// Train of thought:
// The node creation is messing up the name and/or parent assignment
// inside of either mkdir.c or tree.c

void cd(char *path) {
    int b = cd_helper(path);
    if (b == 2) {
        Node *p = find_node(path);
        if (p != NULL)
            cwd = p;
        else
            printf("Can't CD into %s\n", path);
    }
    else if (b == 0)
        cwd = root; 
    else if (b == 1)
        cwd = cwd->parent;
}

Node *find_node(char *path) {
    Node *p;
    if (path[0] == '/') {
        printf("Searching for node in path\n");
        //start = root;
        //find_node(basename(path));
    } else {
        printf("Searching for node in current DIR\n");
        start = cwd;
    }
    if (start->child == NULL) {
            return NULL;
        } else {
            p = start->child;
            while(p) {
                if (strcmp(p->name, path) == 0)
                    return p;
                p = p->sibling;
            }
            return NULL;
        }
}

int cd_helper(char *path) {
    if (!strcmp(path, "")) {
        printf("cd into /\n");
        return 0;
    } else if (!strcmp(path, "..")) {
        printf("cd into parent\n");
        return 1;
    } else {
        printf("cd into %s\n", path);
        return 2;
    }
}
