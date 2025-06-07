#include "types.h"
#include "utils.h"
#include "lib.h"

union DirEntry {
	uint8_t byte[128];
	struct {
		int32_t inode;
		char name[64];
	};
};

typedef union DirEntry DirEntry;

int ls(char *destFilePath) {
    printf("ls %s\n", destFilePath);
    int i = 0;
    int fd = 0;
    int ret = 0;
    DirEntry *dirEntry = 0;
    uint8_t buffer[512 * 2];
    fd = open(destFilePath, O_READ | O_DIRECTORY);
    if (fd == -1)
        return -1;
    ret = read(fd, buffer, 512 * 2);
    while (ret != 0) {
        // 处理读取到的目录项数据
        dirEntry = (DirEntry*)buffer;
        for (i = 0; i < ret / sizeof(DirEntry); i++) {
            // 检查目录项是否有效（inode不为0表示有效）
            if (dirEntry[i].inode != 0) {
                // 额外检查文件名的有效性
                if (dirEntry[i].name[0] >= 32 && dirEntry[i].name[0] < 127) {
                    // 只打印ASCII可打印字符开头的文件名
                    printf("%s  ", dirEntry[i].name);
                } else {
                    // 显示有问题的目录项的inode号
                    printf("[无效条目:inode=%d]  ", dirEntry[i].inode);
                }
            }
        }
        // 继续读取下一块目录数据
        ret = read(fd, buffer, 512 * 2);
    }
    printf("\n");
    close(fd);
    return 0;
}

int cat(char *destFilePath) {
    printf("cat %s\n", destFilePath);
    int fd = 0;
    int ret = 0;
    uint8_t buffer[512 * 2];
    int i = 0;

    fd = open(destFilePath, O_READ);
    if (fd == -1)
        return -1;
    
    ret = read(fd, buffer, 512 * 2);
    while (ret != 0) {
        // 将读取到的内容逐字符打印出来
        for (i = 0; i < ret; i++) {
            printf("%c", buffer[i]);
        }
        // 继续读取文件内容
        ret = read(fd, buffer, 512 * 2);
    }
    
    close(fd);
    return 0;
}

int uEntry(void) {
	int fd = 0;
	int i = 0;
	char tmp = 0;
	
	ls("/");
	ls("/boot/");
	ls("/dev/");
	ls("/usr/");

	printf("create /usr/test and write alphabets to it\n");
	fd = open("/usr/test", O_WRITE | O_READ | O_CREATE);
	for (i = 0; i < 26; i ++) {
		tmp = (char)(i % 26 + 'A');
		write(fd, (uint8_t*)&tmp, 1);
	}
	close(fd);
	ls("/usr/");
	cat("/usr/test");
	printf("\n");
	printf("rm /usr/test\n");
	remove("/usr/test");
	ls("/usr/");
	printf("rmdir /usr/\n");
	remove("/usr/");
	ls("/");
	printf("create /usr/\n");
	fd = open("/usr/", O_CREATE | O_DIRECTORY);
	close(fd);
	ls("/");
	
	exit();
	return 0;
}
