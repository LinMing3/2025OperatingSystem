#include "x86.h"
#include "device.h"

#define SYS_WRITE 0
#define SYS_READ 1
#define SYS_FORK 2
#define SYS_EXEC 3
#define SYS_SLEEP 4
#define SYS_EXIT 5
#define SYS_SEM 6

#define STD_OUT 0
#define STD_IN 1

#define SEM_INIT 0
#define SEM_WAIT 1
#define SEM_POST 2
#define SEM_DESTROY 3

extern TSS tss;

extern ProcessTable pcb[MAX_PCB_NUM];
extern int current;

extern Semaphore sem[MAX_SEM_NUM];
extern Device dev[MAX_DEV_NUM];

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

void GProtectFaultHandle(struct StackFrame *sf);
void timerHandle(struct StackFrame *sf);
void keyboardHandle(struct StackFrame *sf);
void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallRead(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallExec(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);
void syscallSem(struct StackFrame *sf);

void syscallWriteStdOut(struct StackFrame *sf);

void syscallReadStdIn(struct StackFrame *sf);

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
	log("keyboardHandle\n");
	ProcessTable *pt = NULL;
	uint32_t keyCode = getKeyCode();
	if (keyCode == 0) // illegal keyCode
		return;
	
	// putChar(getChar(keyCode));
	keyBuffer[bufferTail] = keyCode;
	bufferTail=(bufferTail+1)%MAX_KEYBUFFER_SIZE;

	if (dev[STD_IN].value < 0) { // with process blocked
		// TODO: deal with blocked situation
		// 获取阻塞队列中的第一个进程控制块
		pt = (ProcessTable *)((uint32_t)(dev[STD_IN].pcb.prev) - (uint32_t)&(((ProcessTable *)0)->blocked));
		dev[STD_IN].pcb.prev = dev[STD_IN].pcb.prev->prev;
		dev[STD_IN].pcb.prev->next = &(dev[STD_IN].pcb);

		// 调整设备value值（减少阻塞进程计数）
		dev[STD_IN].value++;

		// 唤醒进程
		pt->state = STATE_RUNNABLE;
	}

	return;
}

void syscallHandle(struct StackFrame *sf) {
	switch(sf->eax) { // syscall number
		case SYS_WRITE:
			syscallWrite(sf);
			break; // for SYS_WRITE
		case SYS_READ:
			syscallRead(sf);
			break; // for SYS_READ
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

void syscallWrite(struct StackFrame *sf) {
	switch(sf->ecx) { // file descriptor
		case STD_OUT:
			if (dev[STD_OUT].state == 1)
				syscallWriteStdOut(sf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallWriteStdOut(struct StackFrame *sf) {
	int sel = sf->ds; // segment selector for user data, need further modification
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
	return;
}

void syscallRead(struct StackFrame *sf) {
	switch(sf->ecx) {
		case STD_IN:
			if (dev[STD_IN].state == 1)
				syscallReadStdIn(sf);
			break; // for STD_IN
		default:
			break;
	}
}

void syscallReadStdIn(struct StackFrame *sf) {
	//TODO: complete `syscallReadStdIn`
    int sel = sf->ds; // segment selector for user data, need further modification
	char *str = (char*)sf->edx;
	int size = sf->ebx;
	int i = 0;
	char character = 0;

	asm volatile("movw %0, %%es"::"m"(sel));

	if(bufferHead == bufferTail) { // 缓冲区为空
		if(dev[STD_IN].value<0){ // 有进程阻塞在设备上
			sf->eax = -1;
			return;
		}
		else {	// 没有进程阻塞在设备上
			// 将当前进程加入等待队列
			pcb[current].blocked.next = dev[STD_IN].pcb.next;
			pcb[current].blocked.prev = &(dev[STD_IN].pcb);
			dev[STD_IN].pcb.next->prev = &(pcb[current].blocked);
			dev[STD_IN].pcb.next = &(pcb[current].blocked);

			// 调整设备value值（增加阻塞进程计数）
			dev[STD_IN].value--;

			// 阻塞当前进程
			pcb[current].state = STATE_BLOCKED;

			// 触发中断，切换到其他进程
			asm volatile("int $0x20");
		}
	}

	i = 0;
	while (i < size && bufferHead != bufferTail) {
		// 从键盘缓冲区获取一个字符
		uint32_t keyCode = keyBuffer[bufferHead];
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

		// 将键码转换为ASCII字符
		character = getChar(keyCode);

		// 如果是有效字符，存入用户提供的缓冲区
		if (character != 0)
		{
			asm volatile("movb %0, %%es:(%1)" ::"r"(character), "r"(str + i));
			i++;

			// 如果是换行符，停止读取
			if (character == '\n')
			{
				break;
			}
		}
	}

	sf->eax = i; // 返回实际读取的字符数


	return;
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
	//TODO
    int i;
    for (i = 0; i < MAX_SEM_NUM; i++) {
        if (sem[i].state == 0) // 找到一个未使用的信号量
            break;
    }
    
    if (i != MAX_SEM_NUM) {
        sem[i].state = 1;
        sem[i].value = (int)sf->edx; // 初始值
        sem[i].pcb.next = &(sem[i].pcb);
        sem[i].pcb.prev = &(sem[i].pcb); // 初始化等待队列
        pcb[current].regs.eax = i; // 返回信号量ID
    }
    else {
        pcb[current].regs.eax = -1; // 无可用信号量
    }
    
    return;
}

void syscallSemWait(struct StackFrame *sf) {
	//TODO
    int i = (int)sf->edx;
    if (i < 0 || i >= MAX_SEM_NUM) {
        pcb[current].regs.eax = -1;
        return;
    }
    
    if (sem[i].state == 0) { // 信号量未初始化
        pcb[current].regs.eax = -1;
        return;
    }
    
    if (sem[i].value > 0) { // 信号量值大于0，可以获取
        sem[i].value--;
        pcb[current].regs.eax = 0;
        return;
    }
    else { // 信号量值为0，需要阻塞当前进程
        // 将当前进程加入等待队列
        pcb[current].blocked.next = sem[i].pcb.next;
        pcb[current].blocked.prev = &(sem[i].pcb);
        sem[i].pcb.next->prev = &(pcb[current].blocked);
        sem[i].pcb.next = &(pcb[current].blocked);
        
        // 阻塞当前进程
        pcb[current].state = STATE_BLOCKED;
        pcb[current].sleepTime = -1; // 使用-1表示不是因为sleep阻塞
        
        asm volatile("int $0x20"); // 触发调度
        
        // 当进程被唤醒时，返回0表示成功
        pcb[current].regs.eax = 0;
    }
    
    return;
}

void syscallSemPost(struct StackFrame *sf) {
    int i = (int)sf->edx;
    ProcessTable *pt = NULL;
    
    if (i < 0 || i >= MAX_SEM_NUM) {
        pcb[current].regs.eax = -1;
        return;
    }
    //todo
    if (sem[i].state == 0) { // 信号量未初始化
        pcb[current].regs.eax = -1;
        return;
    }
    
    if (sem[i].pcb.next == &(sem[i].pcb)) { // 等待队列为空
        sem[i].value++; // 直接增加信号量值
    }
    else { // 等待队列非空，唤醒一个进程
        // 获取等待队列中的第一个进程
        pt = (ProcessTable*)((uint32_t)(sem[i].pcb.next) - (uint32_t)&(((ProcessTable*)0)->blocked));
        // 从等待队列中移除该进程
        sem[i].pcb.next = pt->blocked.next;
        pt->blocked.next->prev = &(sem[i].pcb);
        // 唤醒进程
        pt->state = STATE_RUNNABLE;
    }
    
    pcb[current].regs.eax = 0; // 返回成功
    return;
}

void syscallSemDestroy(struct StackFrame *sf) {
	//TODO
    int i = (int)sf->edx;
    ProcessTable *pt = NULL;
    
    if (i < 0 || i >= MAX_SEM_NUM) {
        pcb[current].regs.eax = -1;
        return;
    }
    
    if (sem[i].state == 0) { // 信号量未初始化
        pcb[current].regs.eax = -1;
        return;
    }
    
    // 唤醒所有在此信号量上等待的进程
    while (sem[i].pcb.next != &(sem[i].pcb)) {
        // 获取等待队列中的第一个进程
        pt = (ProcessTable*)((uint32_t)(sem[i].pcb.next) - (uint32_t)&(((ProcessTable*)0)->blocked));
        // 从等待队列中移除该进程
        sem[i].pcb.next = pt->blocked.next;
        pt->blocked.next->prev = &(sem[i].pcb);
        // 唤醒进程
        pt->state = STATE_RUNNABLE;
    }
    
    // 重置信号量状态
    sem[i].state = 0;
    sem[i].value = 0;
    
    pcb[current].regs.eax = 0; // 返回成功
    return;
}
