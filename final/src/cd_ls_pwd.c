#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>


#include "../include/util.h" 
#include "../include/type.h"
#include "../include/cd_ls_pwd.h" 
#include "../include/terminal.h"

extern PROC *running;
extern MINODE *root;
extern MOUNT mountTable[NMOUNT];

extern char pathname[128];
extern int dev;

/************* cd_ls_pwd.c file **************/

int cd() {
    int ino = 0;
    
    if (!strcmp(pathname, "")) { // If global path is empty
        ino = 2; // use the root inode
    } else { // If global path is NOT empty
        ino = getino(pathname); // get inode number
        if (ino <= 0) {
            //fprintf(stderr, YEL "ino = 0\n" RESET);
            return -1;
        }
    }

    //printf("Getting INODE with dev = %d and ino = %d\n", dev, ino);
    MINODE *mip = iget(dev, ino); // get minode of path dir
    //printf("Got INODE with dev = %d and ino = %d\n", mip->dev, mip->ino);

    if (my_access(pathname, 'x') != 1) { // If the dir has x (execute) permission
        printf(YEL "Permission denied\n" RESET);
        return -1;
    }
    
    if (S_ISLNK(mip->INODE.i_mode)) { // Check if link
        // If symlink
        char buf[BLKSIZE];
        get_block(dev, mip->INODE.i_block[0], buf);
        DIR *dp = (DIR *)buf;
        int lino = getino(dp->name);
        put_block(dev, mip->INODE.i_block[0], buf);
        iput(mip);
        mip = iget(dev, lino);
    } else if(!S_ISDIR(mip->INODE.i_mode)) { // If inode is not a dir, quit
        fprintf(stderr, YEL "INODE is not a dir\n" RESET);
        return -1;
    }

    iput(running->cwd);
    running->cwd = mip; // Set running cwd to new minode
    return 0;
}

int ls_file(MINODE *mip, char *name, char *lname) {
    char *t1 = "xwrxwrxwr-------"; 
    char *t2 = "----------------";

    struct group *grp; // For group name
    struct passwd *pwd; // for User name
    
    time_t file_time = mip->INODE.i_ctime; // time
    struct tm ts = *localtime(&file_time); // time
    char ftime[64]; // Formatted time

    int ftype = 0;

    if ((mip->INODE.i_mode & 0xF000) == 0x8000) // Check if file
        printf("%c", '-');
    if ((mip->INODE.i_mode & 0xF000) == 0x4000) { // Check if dir
        printf("%c", 'd');
        ftype = 1;
    }
    if ((mip->INODE.i_mode & 0xF000) == 0xA000) { // Check if link
        printf("%c", 'l');
        ftype = 3;
    }
    for (int i = 8; i >= 0; i--) { // Print out permissions
        if (mip->INODE.i_mode & (1 << i)) {
            printf("%c", t1[i]);
        }
        else
            printf("%c", t2[i]);
    }

    if (mip->INODE.i_mode & S_IXUSR && ftype != 1) {
        if (ftype != 3)
            ftype = 2;
    }

    printf("%2d ", mip->INODE.i_links_count); // Print link count

    if ((pwd = getpwuid(mip->INODE.i_uid)) != NULL) // Print name
        printf("%-8s ", pwd->pw_name);
    else
        printf("%-4d ", mip->INODE.i_uid);

    if ((grp = getgrgid(mip->INODE.i_gid)) != NULL) // Print group
        printf("%-8s", grp->gr_name); // originally -4.8
    else
        printf("%-4d", mip->INODE.i_gid);

    printf("%6d ", mip->INODE.i_size); // Print file size

    strftime(ftime, sizeof(ftime), "%b %d %H:%M", &ts); // Format the time
    printf("%s ", ftime); // Print time
    
    if (ftype == 1) {
        printf(BLD BLU "%s" RESET "/\n", name); // Print dir name blue
    } else if (ftype == 2) {
        printf(BLD GRN "%s" RESET "*\n", name); // print executable green
    } else if (ftype == 3) {
        printf(BLD CYN "%s" RESET " -> " "%s\n", name, lname); // print symlink cyan
    } else {
        printf("%s\n", name); // print name normally
    }
    return 0;
}

int ls_dir(MINODE *mip) {
    char buf[BLKSIZE], temp[256], lname[64];
    DIR *dp;
    char *cp;
    MINODE *mmip;
 
    if (S_ISREG(mip->INODE.i_mode)) {
        return -1; // if file, return
    }

    if ((mip->INODE.i_mode & 0xF000) == 0xA000) { // Check if link
        // If symlink
        char buf[BLKSIZE];
        get_block(dev, mip->INODE.i_block[0], buf);
        DIR *dp = (DIR *)buf;
        int lino = getino(dp->name);
        //strncpy(lname, dp->name, strlen(dp->name));
        put_block(dev, mip->INODE.i_block[0], buf);
        iput(mip);
        mip = iget(dev, lino);
    }

    get_block(dev, mip->INODE.i_block[0], buf);
    dp = (DIR *)buf;
    cp = buf;
  
    while (cp < buf + BLKSIZE){
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;
        
        mmip = iget(mip->dev, dp->inode); // Child inode
        if ((mmip->INODE.i_mode & 0xF000) == 0xA000) { // Check if link
            // If symlink
            char lbuf[BLKSIZE];
            get_block(dev, mmip->INODE.i_block[0], lbuf);
            DIR *ldp = (DIR *)lbuf;
            int lino = getino(ldp->name);
            strncpy(lname, ldp->name, strlen(ldp->name));
            put_block(dev, mmip->INODE.i_block[0], lbuf);
            iput(mmip);
            mmip = iget(dev, dp->inode);
        }
        ls_file(mmip, temp, lname); // Call ls_file with each child node
        iput(mmip); // NEW
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }
    put_block(dev, mip->INODE.i_block[0], buf); // NEW
    return 0;
}

int ls() {
    if (!strcmp(pathname, "")) // If path empty, use cwd
        ls_dir(running->cwd);
    else {
        int ino = getino(pathname); // get path inode number
        if (ino < 2)
            return -1;
        //int d = running->cwd->dev; // get device num
        //printf("Getting INODE with dev = %d and ino = %d\n", dev, ino);
        MINODE *m = iget(dev, ino); // get minode of path dir
        ls_dir(m);
        iput(m); // NEW
    }
    return 0;
}

void rpwd(MINODE *wd) {
    u32 ino, pino;
    char my_name[64];
    MINODE *pip;

    if (wd == root) // If root, return
        return;

    ino = wd->ino; // cwd inode num

    if (ino == 2 && wd->dev != mountTable[0].dev) {
        for (int i = 0; i < NMOUNT; i++) {
            if (mountTable[i].dev == wd->dev) {
                ino = mountTable[i].mounted_inode->ino;
                dev = mountTable[i].mounted_inode->dev;
                pino = findino(mountTable[i].mounted_inode, &ino);
                break;
            }
        }
    } else {
        pino = findino(wd, &ino); // cwd parent inode num
    }

    pip = iget(dev, pino); // parent minode
    findmyname(pip, ino, my_name); // get name of cwd from parent
    rpwd(pip); // recursive call, from cwd to root
    printf("/%s", my_name);
    //iput(pip);
    return;
}

void pwd(MINODE *wd) {
    if (wd == root){
        printf("/\n");
        return;
    } else {
        rpwd(wd);
        printf("\n");
    }
    return;
}
