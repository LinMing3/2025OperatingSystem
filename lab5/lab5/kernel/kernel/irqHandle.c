#include "x86.h"
#include "device.h"
#include "fs.h"
// #include <stdlib.h>

#define SYS_OPEN 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_LSEEK 3
#define SYS_CLOSE 4
#define SYS_REMOVE 5
#define SYS_FORK 6
#define SYS_EXEC 7
#define SYS_SLEEP 8
#define SYS_EXIT 9
#define SYS_SEM 10

#define STD_OUT 0
#define STD_IN 1

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

extern TSS tss;

extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];
extern File file[MAX_FILE_NUM];

extern SuperBlock sBlock;
extern GroupDesc gDesc[MAX_GROUP_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void keyboardHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallOpen(struct StackFrame *sf);
void syscallWrite(struct StackFrame *sf);
void syscallRead(struct StackFrame *sf);
void syscallLseek(struct StackFrame *sf);
void syscallClose(struct StackFrame *sf);
void syscallRemove(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExec(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);
void syscallSem(struct StackFrame *sf);

void syscallWriteStdOut(struct StackFrame *sf);
void syscallWriteFile(struct StackFrame *sf);

void syscallReadStdIn(struct StackFrame *sf);
void syscallReadFile(struct StackFrame *sf);

void syscallSemInit(struct StackFrame *sf);
void syscallSemWait(struct StackFrame *sf);
void syscallSemPost(struct StackFrame *sf);
void syscallSemDestroy(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf) { // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	/* Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch(sf->irq) {
		case -1:
			break;
		case 0xd:
			GProtectFaultHandle(sf);
			break;
		case 0x20:
			timerHandle(sf);
			break;
		case 0x21:
			keyboardHandle(sf);
			break;
		case 0x80:
			syscallHandle(sf);
			break;
		default:assert(0);
	}
	/* Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}

void GProtectFaultHandle(struct StackFrame *sf) {
	assert(0);
	return;
}

void timerHandle(struct StackFrame *sf) {
	int i;
	uint32_t tmpStackTop;
	i = (current+1) % MAX_PCB_NUM;
	while (i != current) {
		if (pcb[i].state == STATE_BLOCKED && pcb[i].sleepTime != -1) {
			pcb[i].sleepTime --;
			if (pcb[i].sleepTime == 0)
				pcb[i].state = STATE_RUNNABLE;
		}
		i = (i+1) % MAX_PCB_NUM;
	}

	if (pcb[current].state == STATE_RUNNING &&
		pcb[current].timeCount != MAX_TIME_COUNT) {
		pcb[current].timeCount++;
		return;
	}
	else {
		if (pcb[current].state == STATE_RUNNING) {
			pcb[current].state = STATE_RUNNABLE;
			pcb[current].timeCount = 0;
		}
		
		i = (current+1) % MAX_PCB_NUM;
		while (i != current) {
			if (i !=0 && pcb[i].state == STATE_RUNNABLE)
				break;
			i = (i+1) % MAX_PCB_NUM;
		}
		if (pcb[i].state != STATE_RUNNABLE)
			i = 0;
		current = i;
		/* echo pid of selected process */
		//putChar('0'+current);
		pcb[current].state = STATE_RUNNING;
		pcb[current].timeCount = 1;
		/* recover stackTop of selected process */
		tmpStackTop = pcb[current].stackTop;
		pcb[current].stackTop = pcb[current].prevStackTop;
		tss.esp0 = (uint32_t)&(pcb[current].stackTop); // setting tss for user process
		asm volatile("movl %0, %%esp"::"m"(tmpStackTop)); // switch kernel stack
		asm volatile("popl %gs");
		asm volatile("popl %fs");
		asm volatile("popl %es");
		asm volatile("popl %ds");
		asm volatile("popal");
		asm volatile("addl $8, %esp");
		asm volatile("iret");
	}
}

void keyboardHandle(struct StackFrame *sf) {
	ProcessTable *pt = NULL;
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0) // illegal keyCode
		return;
	//putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode;
	bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;
	if (dev[STD_IN].value < 0) { // with process blocked
		dev[STD_IN].value ++;

		pt = (ProcessTable*)((uint32_t)(dev[STD_IN].pcb.prev) -
					(uint32_t)&(((ProcessTable*)0)->blocked));
		pt->state = STATE_RUNNABLE;
		pt->sleepTime = 0;

		dev[STD_IN].pcb.prev = (dev[STD_IN].pcb.prev)->prev;
		(dev[STD_IN].pcb.prev)->next = &(dev[STD_IN].pcb);
	}
	return;
}

void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case SYS_OPEN:
            putChar('O');
            syscallOpen(sf);
            break; // for SYS_OPEN
		case SYS_WRITE:
			syscallWrite(sf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(sf);
			break; // for SYS_READ
		case SYS_LSEEK:
			syscallLseek(sf);
			break; // for SYS_SEEK
		case SYS_CLOSE:
			syscallClose(sf);
			break; // for SYS_CLOSE
		case SYS_REMOVE:
			syscallRemove(sf);
			break; // for SYS_REMOVE
		case SYS_FORK:
			syscallFork(sf);
			break; // for SYS_FORK
		case SYS_EXEC:
			syscallExec(sf);
			break; // for SYS_EXEC
		case SYS_SLEEP:
			syscallSleep(sf);
			break; // for SYS_SLEEP
		case SYS_EXIT:
			syscallExit(sf);
			break; // for SYS_EXIT
		case SYS_SEM:
			syscallSem(sf);
			break; // for SYS_SEM
		default:break;
	}
}
void syscallOpen(struct StackFrame *sf) {
    int i;
    int ret = 0;
    int size = 0;
    int baseAddr = (current + 1) * 0x100000; // base address of user process
    char *str = (char*)sf->ecx + baseAddr; // file path
    Inode fatherInode;
    Inode destInode;
    int fatherInodeOffset = 0;
    int destInodeOffset = 0;

    ret = readInode(&sBlock, gDesc, &destInode, &destInodeOffset, str);
    
    int mode = sf->edx;
    
    if (ret == 0) { // file exists
        // 检查文件类型是否与打开模式匹配
        if ((mode & 0x08) && destInode.type != DIRECTORY_TYPE)
        {
            // 请求目录但不是目录
            pcb[current].regs.eax = -1;
            return;
        }
        else if (!(mode & 0x08) && destInode.type == DIRECTORY_TYPE)
        {
            // 没有请求目录但是目录
            pcb[current].regs.eax = -1;
            return;
        }

        // 检查文件类型是否支持
        if (destInode.type == FIFO_TYPE || destInode.type == SOCKET_TYPE || destInode.type == UNKNOWN_TYPE) {
            pcb[current].regs.eax = -1;
            return;
        }
        
        // 如果是设备文件，检查是否正在使用
        if (destInode.type == CHARACTER_TYPE || destInode.type == BLOCK_TYPE) {
            int devIndex = (destInode.size >> 16) & 0xFFFF;
            if (dev[devIndex].state == 0) { // 设备未使用
                dev[devIndex].state = 1;
                pcb[current].regs.eax = devIndex;
                return;
            } else { // 设备已被使用
                pcb[current].regs.eax = -1;
                return;
            }
        }
        
        // 为普通文件分配文件描述符
        for (i = 0; i < MAX_FILE_NUM; i++) {
            if (file[i].state == 0) // 找到空闲文件描述符
                break;
        }
        
        if (i == MAX_FILE_NUM) { // 没有可用的文件描述符
            pcb[current].regs.eax = -1;
            return;
        }
        
        // 分配文件描述符
        file[i].state = 1;
        file[i].inodeOffset = destInodeOffset;
        file[i].offset = 0;
        file[i].flags = mode;
        pcb[current].regs.eax = MAX_DEV_NUM + i;
        return;
    }
    else { // file doesn't exist
        if (!(mode & 0x04))
        { // 没有设置创建标志
            pcb[current].regs.eax = -1;
            return;
        }

        // 处理目录路径
        if (mode & 0x08)
        {
            if (str[stringLen(str) - 1] == '/')
                str[stringLen(str) - 1] = 0;
        }

        // 查找父目录
        ret = stringChrR(str, '/', &size);
        if (size == stringLen(str)) {
            pcb[current].regs.eax = -1;
            return;
        }
        
        // 构建父目录路径
        char fatherPath[NAME_LENGTH];
        if (size == 0) {
            fatherPath[0] = '/';
            fatherPath[1] = 0;
        }
        else {
            stringCpy(str, fatherPath, size);
            fatherPath[size] = 0;
        }
        
        // 读取父目录的inode信息
        ret = readInode(&sBlock, gDesc, &fatherInode, &fatherInodeOffset, fatherPath);
        if (ret < 0) {
            pcb[current].regs.eax = -1;
            return;
        }
        
        // 确定要创建的文件类型
        int type = (mode & 0x08) ? DIRECTORY_TYPE : REGULAR_TYPE;

        // 创建新文件或目录
        ret = allocInode(&sBlock, gDesc, &fatherInode, fatherInodeOffset, 
                       &destInode, &destInodeOffset, str + size + 1, type);
        
        if (ret == -1) {
            pcb[current].regs.eax = -1;
            return;
        }
        
        // 为新创建的文件分配文件描述符
        for (i = 0; i < MAX_FILE_NUM; i++) {
            if (file[i].state == 0)
                break;
        }
        
        if (i == MAX_FILE_NUM) {
            pcb[current].regs.eax = -1;
            return;
        }
        
        // 设置文件描述符
        file[i].state = 1;
        file[i].inodeOffset = destInodeOffset;
        file[i].offset = 0;
        file[i].flags = mode;
        pcb[current].regs.eax = MAX_DEV_NUM + i;
        return;
    }
}

void syscallWrite(struct StackFrame *sf) {
    switch(sf->ecx) { // file descriptor
        case STD_OUT:
            if (dev[STD_OUT].state == 1)
                syscallWriteStdOut(sf);
            break; // for STD_OUT
        default:
            // 检查文件描述符是否在有效范围内
            if (sf->ecx >= MAX_DEV_NUM && sf->ecx < MAX_DEV_NUM + MAX_FILE_NUM) {
                // 检查文件是否打开
                if (file[sf->ecx - MAX_DEV_NUM].state == 1)
                    syscallWriteFile(sf);
                else
                    pcb[current].regs.eax = -1;
            } else {
                // 文件描述符无效
                pcb[current].regs.eax = -1;
            }
            break;
    }
    return;
}

void syscallWriteStdOut(struct StackFrame *sf) {
	int sel = sf->ds; 
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		if(character == '\n') {
			displayRow++;
			displayCol=0;
			if(displayRow==MAX_ROW){
				displayRow=MAX_ROW-1;
				displayCol=0;
				scrollScreen();
			}
		}
		else {
			data = character | (0x0c << 8);
			pos = (MAX_COL*displayRow+displayCol)*2;
			asm volatile("movw %0, (%1)"::"r"(data),"r"(pos+0xb8000));
			displayCol++;
			if(displayCol==MAX_COL){
				displayRow++;
				displayCol=0;
				if(displayRow==MAX_ROW){
					displayRow=MAX_ROW-1;
					displayCol=0;
					scrollScreen();
				}
			}
		}
	}
	
	updateCursor(displayRow, displayCol);
	pcb[current].regs.eax = size;
	return;
}

void syscallWriteFile(struct StackFrame *sf) {
    if (file[sf->ecx - MAX_DEV_NUM].flags % 2 == 0) { // if O_WRITE is not set
        pcb[current].regs.eax = -1;
        return;
    }

    int i = 0;
    int j = 0;
    // int ret = 0;
    int baseAddr = (current + 1) * 0x100000; // base address of user process
    uint8_t *str = (uint8_t*)sf->edx + baseAddr; // buffer of user process
    int size = sf->ebx;
    uint8_t buffer[SECTOR_SIZE * SECTORS_PER_BLOCK];
    int quotient = file[sf->ecx - MAX_DEV_NUM].offset / sBlock.blockSize;
    int remainder = file[sf->ecx - MAX_DEV_NUM].offset % sBlock.blockSize;

    Inode inode;
    diskRead(&inode, sizeof(Inode), 1, file[sf->ecx - MAX_DEV_NUM].inodeOffset);
    
    // 计算要写入的数据块范围
    int startBlock = quotient;
    // int endBlock = (file[sf->ecx - MAX_DEV_NUM].offset + size - 1) / sBlock.blockSize;
    
    // 如果起始位置不在块起始位置，需要先读取该块数据
    if (remainder != 0) {
        readBlock(&sBlock, &inode, startBlock, buffer);
        
        // 计算本块可以写入的数据量
        int copySize = sBlock.blockSize - remainder;
        if (copySize > size) 
            copySize = size;
        
        // 复制数据到buffer
        for (i = 0; i < copySize; i++)
            buffer[remainder + i] = str[i];
        
        // 写入数据块
        writeBlock(&sBlock, &inode, startBlock, buffer);
        
        // 更新已处理数据量和偏移
        size -= copySize;
        str += copySize;
        startBlock++;
    }
    
    // 处理完整的数据块
    j = 0;
    while (size >= sBlock.blockSize) {
        // 直接将用户数据复制到buffer
        for (i = 0; i < sBlock.blockSize; i++)
            buffer[i] = str[j * sBlock.blockSize + i];
        
        // 写入数据块
        writeBlock(&sBlock, &inode, startBlock + j, buffer);
        
        // 更新剩余大小
        size -= sBlock.blockSize;
        j++;
    }
    
    // 处理最后一个可能不完整的数据块
    if (size > 0) {
        readBlock(&sBlock, &inode, startBlock + j, buffer);
        
        // 复制剩余数据
        for (i = 0; i < size; i++)
            buffer[i] = str[j * sBlock.blockSize + i];
        
        // 写入数据块
        writeBlock(&sBlock, &inode, startBlock + j, buffer);
    }

    // 更新inode的大小信息
    if (file[sf->ecx - MAX_DEV_NUM].offset + sf->ebx > inode.size)
        inode.size = file[sf->ecx - MAX_DEV_NUM].offset + sf->ebx;
    
    // 将更新后的inode写回磁盘
    diskWrite(&inode, sizeof(Inode), 1, file[sf->ecx - MAX_DEV_NUM].inodeOffset);
    
    // 更新文件偏移量并返回写入的字节数
    pcb[current].regs.eax = sf->ebx;
    file[sf->ecx - MAX_DEV_NUM].offset += sf->ebx;
    return;
}

void syscallRead(struct StackFrame *sf) {
    switch(sf->ecx) { // file descriptor
        case STD_IN:
            if (dev[STD_IN].state == 1)
                syscallReadStdIn(sf);
            break; // for STD_IN
        default:
            // 检查文件描述符是否在有效范围内
            if (sf->ecx >= MAX_DEV_NUM && sf->ecx < MAX_DEV_NUM + MAX_FILE_NUM) {
                // 检查文件是否打开
                if (file[sf->ecx - MAX_DEV_NUM].state == 1)
                    syscallReadFile(sf);
                else
                    pcb[current].regs.eax = -1;
            } else {
                // 文件描述符无效
                pcb[current].regs.eax = -1;
            }
            break;
    }
    return;
}

void syscallReadStdIn(struct StackFrame *sf) {
	if (dev[STD_IN].value == 0) { // no process blocked
		/* Blocked for I/O */
		dev[STD_IN].value --;

		pcb[current].blocked.next = dev[STD_IN].pcb.next;
		pcb[current].blocked.prev = &(dev[STD_IN].pcb);
		dev[STD_IN].pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);

		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = -1; // blocked on STD_IN

		bufferHead = bufferTail;
		asm volatile("int $0x20");
		/* Resumed from Blocked */
		int sel = sf->ds;
		char *str = (char*)sf->edx;
		int size = sf->ebx; // MAX_BUFFER_SIZE, reverse last byte
		int i = 0;
		char character = 0;
		asm volatile("movw %0, %%es"::"m"(sel));
		while(i < size-1) {
			if(bufferHead != bufferTail){ // what if keyBuffer is overflow
				character = getChar(keyBuffer[bufferHead]);
				bufferHead = (bufferHead+1)%MAX_KEYBUFFER_SIZE;
				putChar(character);
				if(character != 0) {
					asm volatile("movb %0, %%es:(%1)"::"r"(character),"r"(str+i));
					i++;
				}
			}
			else
				break;
		}
		asm volatile("movb $0x00, %%es:(%0)"::"r"(str+i));
		pcb[current].regs.eax = i;
		return;
	}
	else if (dev[STD_IN].value < 0) { // with process blocked
		pcb[current].regs.eax = -1;
		return;
	}
}

void syscallReadFile(struct StackFrame *sf) {
    if ((file[sf->ecx - MAX_DEV_NUM].flags >> 1) % 2 == 0) { // if O_READ is not set
        pcb[current].regs.eax = -1;
        return;
    }

    int i = 0;
    int j = 0;
    int baseAddr = (current + 1) * 0x100000; // base address of user process
    uint8_t *str = (uint8_t*)sf->edx + baseAddr; // buffer of user process
    int size = sf->ebx; // 要读取的最大字节数
    uint8_t buffer[SECTOR_SIZE * SECTORS_PER_BLOCK];
    int quotient = file[sf->ecx - MAX_DEV_NUM].offset / sBlock.blockSize;
    int remainder = file[sf->ecx - MAX_DEV_NUM].offset % sBlock.blockSize;
    
    Inode inode;
    diskRead(&inode, sizeof(Inode), 1, file[sf->ecx - MAX_DEV_NUM].inodeOffset);
    
    // 检查是否已经到达文件末尾
    if (file[sf->ecx - MAX_DEV_NUM].offset >= inode.size) {
        pcb[current].regs.eax = 0; // 返回0表示已经读到文件末尾
        return;
    }
    
    // 调整要读取的大小，不能超过文件总大小
    if (file[sf->ecx - MAX_DEV_NUM].offset + size > inode.size) {
        size = inode.size - file[sf->ecx - MAX_DEV_NUM].offset;
    }
    
    int startBlock = quotient;
    // int endBlock = (file[sf->ecx - MAX_DEV_NUM].offset + size - 1) / sBlock.blockSize;
    int bytesRead = 0;
    
    // 处理起始块的不完整读取
    if (remainder != 0) {
        readBlock(&sBlock, &inode, startBlock, buffer);
        
        // 计算从当前块能读取的字节数
        int copySize = sBlock.blockSize - remainder;
        if (copySize > size)
            copySize = size;
        
        // 复制数据到用户缓冲区
        for (i = 0; i < copySize; i++)
            str[i] = buffer[remainder + i];
        
        // 更新已处理数据量
        bytesRead += copySize;
        size -= copySize;
        startBlock++;
    }
    
    // 处理完整的数据块
    j = 0;
    while (size >= sBlock.blockSize) {
        readBlock(&sBlock, &inode, startBlock + j, buffer);
        
        // 复制整个块的数据到用户缓冲区
        for (i = 0; i < sBlock.blockSize; i++)
            str[bytesRead + i] = buffer[i];
        
        bytesRead += sBlock.blockSize;
        size -= sBlock.blockSize;
        j++;
    }
    
    // 处理最后一个不完整的块
    if (size > 0) {
        readBlock(&sBlock, &inode, startBlock + j, buffer);
        
        // 复制剩余数据到用户缓冲区
        for (i = 0; i < size; i++)
            str[bytesRead + i] = buffer[i];
        
        bytesRead += size;
    }

    // 更新文件偏移量并返回实际读取的字节数
    pcb[current].regs.eax = bytesRead;
    file[sf->ecx - MAX_DEV_NUM].offset += bytesRead;
    return;
}

void syscallLseek(struct StackFrame *sf) {
    int i = (int)sf->ecx; // 文件描述符
    int offset = (int)sf->edx; // 偏移量
    Inode inode;
    
    // 检查文件描述符是否有效
    if (i < MAX_DEV_NUM || i >= MAX_DEV_NUM + MAX_FILE_NUM) {
        pcb[current].regs.eax = -1;
        return;
    }
    
    // 检查文件是否打开
    if (file[i - MAX_DEV_NUM].state == 0) {
        pcb[current].regs.eax = -1;
        return;
    }

    // 读取inode信息
    diskRead(&inode, sizeof(Inode), 1, file[i - MAX_DEV_NUM].inodeOffset);

    // 根据whence计算新的文件指针位置
    int newOffset = 0;
    switch(sf->ebx) { // whence
        case SEEK_SET: // 从文件开头计算偏移
            newOffset = offset;
            break;
        case SEEK_CUR: // 从当前位置计算偏移
            newOffset = file[i - MAX_DEV_NUM].offset + offset;
            break;
        case SEEK_END: // 从文件末尾计算偏移
            newOffset = inode.size + offset;
            break;
        default:
            pcb[current].regs.eax = -1;
            return;
    }
    
    // 检查新的文件指针位置是否有效（不能为负数）
    if (newOffset < 0) {
        pcb[current].regs.eax = -1;
        return;
    }
    
    // 设置新的文件指针位置
    file[i - MAX_DEV_NUM].offset = newOffset;
    
    // 返回新的文件指针位置
    pcb[current].regs.eax = newOffset;
    return;
}

void syscallClose(struct StackFrame *sf) {
    int i = (int)sf->ecx;
    
    // 如果是设备文件描述符
    if (i >= 0 && i < MAX_DEV_NUM) {
        // 检查设备是否打开
        if (dev[i].state == 0) {
            pcb[current].regs.eax = -1;  // 设备未打开
            return;
        }
        
        // 关闭设备
        dev[i].state = 0;
        pcb[current].regs.eax = 0;
        return;
    }
    
    // 如果是普通文件描述符
    if (i >= MAX_DEV_NUM && i < MAX_DEV_NUM + MAX_FILE_NUM) {
        // 检查文件是否打开
        if (file[i - MAX_DEV_NUM].state == 0) {
            pcb[current].regs.eax = -1;  // 文件未打开
            return;
        }
        
        // 关闭文件，释放相关资源
        file[i - MAX_DEV_NUM].state = 0;
        file[i - MAX_DEV_NUM].inodeOffset = 0;
        file[i - MAX_DEV_NUM].offset = 0;
        file[i - MAX_DEV_NUM].flags = 0;
        
        pcb[current].regs.eax = 0;  // 返回成功
        return;
    }
    
    // 文件描述符超出有效范围
    pcb[current].regs.eax = -1;
    return;
}

void syscallRemove(struct StackFrame *sf) {
    int i;
    int ret = 0;
    int baseAddr = (current + 1) * 0x100000; // base address of user process
    char *str = (char*)sf->ecx + baseAddr; // file path
    Inode fatherInode;
    Inode destInode;
    int fatherInodeOffset = 0;
    int destInodeOffset = 0;
    int *pDestInodeOffset = &destInodeOffset; // 添加指针变量

    ret = readInode(&sBlock, gDesc, &destInode, &destInodeOffset, str);

    if (ret == 0) { // file exist
        // 检查是否是设备文件
        if (destInode.type == CHARACTER_TYPE || destInode.type == BLOCK_TYPE) {
            // 不允许删除设备文件
            pcb[current].regs.eax = -1;
            putChar('D');
            return;
        }
        
        // 检查文件是否正在被使用
        if (destInode.type == REGULAR_TYPE) {
            // 检查文件是否被打开
            for (i = 0; i < MAX_FILE_NUM; i++) {
                if (file[i].state == 1 && file[i].inodeOffset == destInodeOffset) {
                    // 文件正在使用中，不能删除
                    pcb[current].regs.eax = -1;
                    putChar('U');
                    return;
                }
            }
            
            // 在 syscallRemove 函数中，修改读取父目录的代码
            
            // 处理普通文件的删除
            // 找到父目录
            char fatherPath[128];
            int len = stringLen(str);
            if (str[len - 1] == '/')
                str[len - 1] = 0; // 移除可能的尾部斜杠
            
            // 调试输出完整路径
            putChar('[');
            for (i = 0; i < 3 && i < len; i++) {
                putChar(str[i]);
            }
            putChar(']');
            
            int pos = -1;
            for (i = len - 1; i >= 0; i--) {
                if (str[i] == '/') {
                    pos = i;
                    break;
                }
            }
            
            // 读取父目录信息
            if (pos == 0) { // 如果父目录是根目录
                fatherPath[0] = '/';
                fatherPath[1] = 0;
                putChar('0'); // 调试输出，表示父目录是根目录
            } else {
                stringCpy(str, fatherPath, pos);
                fatherPath[pos] = 0;
                putChar('P'); // 调试输出，表示找到了父目录
            }
            
            // 打印父目录路径的前几个字符
            putChar('<');
            for (i = 0; i < 3 && i < pos; i++) {
                putChar(fatherPath[i]);
            }
            putChar('>');
            
            ret = readInode(&sBlock, gDesc, &fatherInode, &fatherInodeOffset, fatherPath);
            if (ret == -1) {
                pcb[current].regs.eax = -1;
                putChar('R'); // 父目录不存在
                return;
            }

            // 从父目录中删除文件条目并释放inode
            char fileName[128];
            stringCpy(str + pos + 1, fileName, len - pos - 1);
            fileName[len - pos - 1] = 0;

            // // 调试输出文件名
            // putChar('{');
            // putChar(fileName[0]);
            // putChar(fileName[1]);
            // putChar(fileName[2]);
            // putChar('}');

            // // 打印父目录和目标文件的inode偏移量
            // putChar('@');
            // // putChar('0' + (fatherInodeOffset / 1000) % 10);
            // // putChar('0' + (fatherInodeOffset / 100) % 10);
            // // putChar('0' + (fatherInodeOffset / 10) % 10);
            // // putChar('0' + fatherInodeOffset % 10);

            // // 打印父目录和目标文件的inode偏移量
            // putChar(',');
            // putChar('0' + (destInodeOffset / 1000) % 10);
            // putChar('0' + (destInodeOffset / 100) % 10);
            // putChar('0' + (destInodeOffset / 10) % 10);
            // putChar('0' + destInodeOffset % 10);

            // 修改这一行，传入正确的参数类型
            ret = freeInode(&sBlock, gDesc, &fatherInode, fatherInodeOffset,&destInode, pDestInodeOffset, fileName, REGULAR_TYPE);
            if (ret == -1)
            {
                pcb[current].regs.eax = -1;
                putChar('F');
                return;
            }

            pcb[current].regs.eax = 0;
            return;
        }
        else if (destInode.type == DIRECTORY_TYPE) {
            // 检查目录是否为空 - 使用栈上分配内存替代malloc
            uint8_t buffer[SECTOR_SIZE * SECTORS_PER_BLOCK];
            DirEntry *dirEntry = (DirEntry *)buffer;
            
            // 读取目录内容
            readBlock(&sBlock, &destInode, 0, buffer);
            
            // 检查目录是否为空（只有 "." 和 ".." 两个条目）
            int entryCount = 0;
            int maxEntries = sBlock.blockSize / sizeof(DirEntry);
            for (i = 0; i < maxEntries; i++) {
                if (dirEntry[i].inode != 0) {
                    // 忽略 "." 和 ".." 条目
                    if (stringCmp(dirEntry[i].name, ".", 1) != 0 && 
                        stringCmp(dirEntry[i].name, "..", 2) != 0) {
                        // 目录非空，不能删除
                        pcb[current].regs.eax = -1;
                        putChar('E');
                        return;
                    }
                    entryCount++;
                }
            }
            
            // 找到父目录
            char fatherPath[128];
            int len = stringLen(str);
            if (str[len - 1] == '/')
                str[len - 1] = 0;
            
            int pos = -1;
            for (i = len - 1; i >= 0; i--) {
                if (str[i] == '/') {
                    pos = i;
                    break;
                }
            }
            
            // 读取父目录信息
            if (pos == 0) { // 如果父目录是根目录
                fatherPath[0] = '/';
                fatherPath[1] = 0;
            } else {
                stringCpy(fatherPath, str, pos);
                fatherPath[pos] = 0;
            }
            
            ret = readInode(&sBlock, gDesc, &fatherInode, &fatherInodeOffset, fatherPath);
            if (ret == -1) {
                pcb[current].regs.eax = -1;
                putChar('G');
                return;
            }
            
            
            // 从父目录中删除目录条目并释放inode
            char dirName[128];
            // memset(dirName, 0, 128); // 确保数组初始化为0
            
            // 检查路径长度
            if (len <= pos + 1 || pos < 0) {
                pcb[current].regs.eax = -1;
                putChar('L'); // 路径长度错误
                return;
            }
            
            // 打印源字符串以进行验证
            putChar('(');
            for (i = pos + 1; i < len && i < pos + 5; i++) {
                putChar(str[i]);
            }
            putChar(')');
            
            // 安全地复制文件名
            int nameLen = len - pos - 1;
            if (nameLen > 127) nameLen = 127; // 防止缓冲区溢出
            
            for (i = 0; i < nameLen; i++) {
                dirName[i] = str[pos + 1 + i];
            }
            dirName[nameLen] = 0; // 确保字符串以null结尾
            
            // 输出提取的文件名
            putChar('{');
            for (i = 0; i < 3 && i < nameLen; i++) {
                putChar(dirName[i]);
            }
            putChar('}');
            
            // 修改这一行，传入正确的参数类型
            ret = freeInode(&sBlock, gDesc, &fatherInode, fatherInodeOffset, 
                           &destInode, pDestInodeOffset, dirName, DIRECTORY_TYPE);
            
            if (ret == -1) {
                putChar('H');
                pcb[current].regs.eax = -1;
                return;
            }
            
            pcb[current].regs.eax = 0;
            return;
        }
    }
    else { // file not exist
        pcb[current].regs.eax = -1;
        putChar('N');
        return;
    }
    
}

void syscallFork(struct StackFrame *sf) {
	int i, j;
	for (i = 0; i < MAX_PCB_NUM; i++) {
		if (pcb[i].state == STATE_DEAD)
			break;
	}
	if (i != MAX_PCB_NUM) {
		/* copy userspace
		   enable interrupt
		 */
		enableInterrupt();
		for (j = 0; j < 0x100000; j++) {
			*(uint8_t *)(j + (i+1)*0x100000) = *(uint8_t *)(j + (current+1)*0x100000);
			//asm volatile("int $0x20"); // Testing irqTimer during syscall
		}
		/* disable interrupt
		 */
		disableInterrupt();
		/* set pcb
		   pcb[i]=pcb[current] doesn't work
		*/
		pcb[i].stackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].stackTop);
		pcb[i].prevStackTop = (uint32_t)&(pcb[i].stackTop) -
			((uint32_t)&(pcb[current].stackTop) - pcb[current].prevStackTop);
		pcb[i].state = STATE_RUNNABLE;
		pcb[i].timeCount = pcb[current].timeCount;
		pcb[i].sleepTime = pcb[current].sleepTime;
		pcb[i].pid = i;
		/* set regs */
		pcb[i].regs.ss = USEL(2+i*2);
		pcb[i].regs.esp = pcb[current].regs.esp;
		pcb[i].regs.eflags = pcb[current].regs.eflags;
		pcb[i].regs.cs = USEL(1+i*2);
		pcb[i].regs.eip = pcb[current].regs.eip;
		pcb[i].regs.eax = pcb[current].regs.eax;
		pcb[i].regs.ecx = pcb[current].regs.ecx;
		pcb[i].regs.edx = pcb[current].regs.edx;
		pcb[i].regs.ebx = pcb[current].regs.ebx;
		pcb[i].regs.xxx = pcb[current].regs.xxx;
		pcb[i].regs.ebp = pcb[current].regs.ebp;
		pcb[i].regs.esi = pcb[current].regs.esi;
		pcb[i].regs.edi = pcb[current].regs.edi;
		pcb[i].regs.ds = USEL(2+i*2);
		pcb[i].regs.es = pcb[current].regs.es;
		pcb[i].regs.fs = pcb[current].regs.fs;
		pcb[i].regs.gs = pcb[current].regs.gs;
		/* set return value */
		pcb[i].regs.eax = 0;
		pcb[current].regs.eax = i;
	}
	else {
		pcb[current].regs.eax = -1;
	}
	return;
}

void syscallExec(struct StackFrame *sf) {
	return;
}

void syscallSleep(struct StackFrame *sf) {
	if (sf->ecx == 0)
		return;
	else {
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = sf->ecx;
		asm volatile("int $0x20");
		return;
	}
}

void syscallExit(struct StackFrame *sf) {
	pcb[current].state = STATE_DEAD;
	asm volatile("int $0x20");
	return;
}

void syscallSem(struct StackFrame *sf) {
	switch(sf->ecx) {
		case SEM_INIT:
			syscallSemInit(sf);
			break;
		case SEM_WAIT:
			syscallSemWait(sf);
			break;
		case SEM_POST:
			syscallSemPost(sf);
			break;
		case SEM_DESTROY:
			syscallSemDestroy(sf);
			break;
		default:break;
	}
}

void syscallSemInit(struct StackFrame *sf) {
	int i;
	for (i = 0; i < MAX_SEM_NUM ; i++) {
		if (sem[i].state == 0) // not in use
			break;
	}
	if (i != MAX_SEM_NUM) {
		sem[i].state = 1;
		sem[i].value = (int32_t)sf->edx;
		sem[i].pcb.next = &(sem[i].pcb);
		sem[i].pcb.prev = &(sem[i].pcb);
		pcb[current].regs.eax = i;
	}
	else
		pcb[current].regs.eax = -1;
	return;
}

void syscallSemWait(struct StackFrame *sf) {
	int i = (int)sf->edx;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].state == 0) { // not in use
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].value >= 1) { // not to block itself
		sem[i].value --;
		pcb[current].regs.eax = 0;
		return;
	}
	if (sem[i].value < 1) { // block itself on this sem
		sem[i].value --;
		pcb[current].blocked.next = sem[i].pcb.next;
		pcb[current].blocked.prev = &(sem[i].pcb);
		sem[i].pcb.next = &(pcb[current].blocked);
		(pcb[current].blocked.next)->prev = &(pcb[current].blocked);
		
		pcb[current].state = STATE_BLOCKED;
		pcb[current].sleepTime = -1;
		asm volatile("int $0x20");
		pcb[current].regs.eax = 0;
		return;
	}
}

void syscallSemPost(struct StackFrame *sf) {
	int i = (int)sf->edx;
	ProcessTable *pt = NULL;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].state == 0) { // not in use
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].value >= 0) { // no process blocked
		sem[i].value ++;
		pcb[current].regs.eax = 0;
		return;
	}
	if (sem[i].value < 0) { // release process blocked on this sem 
		sem[i].value ++;

		pt = (ProcessTable*)((uint32_t)(sem[i].pcb.prev) -
					(uint32_t)&(((ProcessTable*)0)->blocked));
		pt->state = STATE_RUNNABLE;
		pt->sleepTime = 0;

		sem[i].pcb.prev = (sem[i].pcb.prev)->prev;
		(sem[i].pcb.prev)->next = &(sem[i].pcb);
		
		pcb[current].regs.eax = 0;
		return;
	}
}

void syscallSemDestroy(struct StackFrame *sf) {
	int i = (int)sf->edx;
	if (i < 0 || i >= MAX_SEM_NUM) {
		pcb[current].regs.eax = -1;
		return;
	}
	if (sem[i].state == 0) { // not in use
		pcb[current].regs.eax = -1;
		return;
	}
	sem[i].state = 0;
	sem[i].value = 0;
	sem[i].pcb.next = &(sem[i].pcb);
	sem[i].pcb.prev = &(sem[i].pcb);
	pcb[current].regs.eax = 0;
	return;
}
