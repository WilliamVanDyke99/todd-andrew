#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>
#include <sys/stat.h>

#include "../include/rmdir.h"
#include "../include/util.h"
#include "../include/colors.h"
#include "../include/type.h"

extern char pathname[128];
extern int dev;
extern PROC *running;

int my_rmdir() {
    if (!strcmp(pathname, "")) {
        printf("No path was provided\n");
        return -1;
    }

    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);
    
    if (!S_ISDIR(mip->INODE.i_mode)) {
        printf("Not a directory\n");
        return -1;
    }

    if (mip->refCount > 1) {
        printf("Directory busy\n");
        return -1;
    }

    if (numblks(mip) != 2) {
        printf("Dir not empty\n");
        return -1;
    }

    int pino = findino(mip, ino);
    MINODE *pmip = iget(mip->dev, pino);
    char name[64];
    findmyname(pmip, ino, name);
    //rm_child(pmip, name);

    pmip->INODE.i_links_count -= 1;
    pmip->dirty = 1;
    iput(pmip);

    bdalloc(mip->dev, mip->INODE.i_block[0]);
    idalloc(mip->dev, mip->ino);
    iput(mip);

    return 0;
}

int rm_child(MINODE *pmip, char *name) {
    char buf[BLKSIZE];
    DIR *dp;
    char *cp;
    int i = 0;

    int ino = search(pmip, name);
    if (ino == 0) {
        printf("Couldn't find child to remove\n");
        return -1;
    }

    for (i = 0; i < pmip->INODE.i_size/BLKSIZE; i++) {
        if (pmip->INODE.i_block[i] == 0)
            break;
        get_block(dev, pmip->INODE.i_block[i], buf);
        
        dp = (DIR *)buf;
        cp = buf;
        int temp = 0;

        while (cp + dp->rec_len < buf + BLKSIZE) {

            if (dp->inode == ino && dp->rec_len == BLKSIZE) { // If first and only entry in data block
                bdalloc(dev, pmip->INODE.i_block[i]); // deallocate block
                pmip->INODE.i_size -= BLKSIZE; // Reduce parent size by BLKSIZE
                if (i < 0 && pmip->INODE.i_block[i+1] != 0) { // if block was between other blocks
                    memmove(&pmip->INODE.i_block[i], &pmip->INODE.i_block[i+1], BLKSIZE); // shift blocks down 1
                }
                return 0;
            } else if (dp->inode == ino) { // entry is first but not only entry, or in middle of block
                temp = dp->rec_len; // store rec_len
                memmove(dp, (DIR *)cp+dp->rec_len, dp->rec_len); // Shift items in block
                while (cp + dp->rec_len < buf + BLKSIZE) { // Move to last item
                    cp += dp->rec_len;
                    dp = (DIR *)cp;
                }
                dp->rec_len += temp; // add removed rec_len to end
                return 0;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
        if (dp->inode == ino) { // if last entry in block
            temp = dp->rec_len; // last item's rec_len
            cp -= dp->rec_len; // go to prev element
            dp = (DIR *)cp;
            dp->rec_len += temp; // Add old last item's rec len to new last item
        }
    }


    return 0; 
}
