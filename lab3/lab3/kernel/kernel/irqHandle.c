#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

void timerHandle(struct StackFrame *sf);

void GProtectFaultHandle(struct StackFrame *sf);

void syscallHandle(struct StackFrame *sf);

void syscallWrite(struct StackFrame *sf);
void syscallPrint(struct StackFrame *sf);
void syscallFork(struct StackFrame *sf);
void syscallSleep(struct StackFrame *sf);
void syscallExit(struct StackFrame *sf);

void irqHandle(struct StackFrame *sf)
{ // pointer sf = esp
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds" ::"a"(KSEL(SEG_KDATA)));
	/*TODO Save esp to stackTop */
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].prevStackTop = pcb[current].stackTop;
	pcb[current].stackTop = (uint32_t)sf;

	switch (sf->irq)
	{
	case -1:
		log("irq=-1\n");
		break;
	case 0xd:
		log("protect\n");
		GProtectFaultHandle(sf);
		break;
	case 0x20:
		log("timer\n");
		timerHandle(sf);
		break;
	case 0x80:
		syscallHandle(sf);
		break;
	default:
		log("default\n");
		// assert(0);
	}
	/*TODO Recover stackTop */
	pcb[current].stackTop = tmpStackTop;
}
void timerHandle(struct StackFrame *sf)
{
	putChar('0' + pcb[current].pid);
	putChar('\n');
	putChar('0' + pcb[current].state);
	putChar('\n');

	// 更新阻塞进程的 sleepTime
	for (int i = 0; i < MAX_PCB_NUM; i++)
	{
		if (pcb[i].state == STATE_BLOCKED)
		{
			pcb[i].sleepTime--;
			if (pcb[i].sleepTime <= 0)
			{
				pcb[i].state = STATE_RUNNABLE; // 唤醒阻塞的进程
			}
		}
	}

	// 如果当前进程处于阻塞状态，切换到下一个进程
	// 如果当前进程处于死亡状态，切换到下一个进程
	// 如果当前进程处于运行状态，检查时间片是否用完
	// 如果用完，切换到下一个进程
	if (pcb[current].state == STATE_RUNNING)
	{
		// 当前进程时间片递减
		pcb[current].timeCount++;
		log("current timeCount\n");
		putChar('0' + pcb[current].timeCount);
		putChar('\n');
		if (pcb[current].timeCount < MAX_TIME_COUNT)
		{
			log("still running,return\n");
			return; // 当前进程时间片未用完，继续运行
		}
		pcb[current].timeCount = 0;			 // 重置当前进程的时间片
		pcb[current].state = STATE_RUNNABLE; // 当前进程重新变为可运行状态
	}

	// 寻找及切换

	// 寻找下一个可运行的进程
	int next = -1;
	for (int i = 1; i < MAX_PCB_NUM; i++)
	{ // 轮转调度，从当前进程的下一个开始查找
		int candidate = (current + i) % MAX_PCB_NUM;
		if (candidate == 0)
		{
			continue;
		}
		if (pcb[candidate].state == STATE_RUNNABLE)
		{
			next = candidate;
			break;
		}
	}
	// 如果没有找到可运行的进程,去0
	if (next == -1)
	{
		log("no available,idle\n");
		next = 0; // 没有可运行的进程
	}
	// 切换到下一个进程
	current = next;
	pcb[current].state = STATE_RUNNING; // 设置新进程为运行状态
	log("switch to next\n");
	putChar('0' + current);
	putChar('\n');
	// putChar('0' + pcb[current].state);
	// putChar('\n');
	// putChar('0' + pcb[current].timeCount);
	// putChar('\n');
	uint32_t tmpStackTop = pcb[current].stackTop;
	pcb[current].stackTop = pcb[current].prevStackTop;
	tss.esp0 = (uint32_t)&(pcb[current].stackTop);
	asm volatile("movl %0,%%esp" ::"m"(tmpStackTop));
	asm volatile("popl %gs");
	asm volatile("popl %fs");
	asm volatile("popl %es");
	asm volatile("popl %ds");
	asm volatile("popal");
	asm volatile("addl $8,%esp");
	asm volatile("iret");
}

void GProtectFaultHandle(struct StackFrame *sf)
{
	assert(0);
	return;
}

void syscallHandle(struct StackFrame *sf)
{
	switch (sf->eax)
	{ // syscall number
	case 0:
		syscallWrite(sf);
		break; // for SYS_WRITE
	case 1:
		syscallFork(sf);
		break;
	case 3:
		syscallSleep(sf);
		break;
	case 4:
		syscallExit(sf);
		break;
	/*TODO Add Fork,Sleep... */
	default:
		break;
	}
}

void syscallWrite(struct StackFrame *sf)
{
	switch (sf->ecx)
	{ // file descriptor
	case 0:
		syscallPrint(sf);
		break; // for STD_OUT
	default:
		break;
	}
}

void syscallPrint(struct StackFrame *sf)
{
	int sel = sf->ds; // TODO segment selector for user data, need further modification
	char *str = (char *)sf->edx;
	int size = sf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++)
	{
		asm volatile("movb %%es:(%1), %0" : "=r"(character) : "r"(str + i));
		if (character == '\n')
		{
			displayRow++;
			displayCol = 0;
			if (displayRow == 25)
			{
				displayRow = 24;
				displayCol = 0;
				scrollScreen();
			}
		}
		else
		{
			data = character | (0x0c << 8);
			pos = (80 * displayRow + displayCol) * 2;
			asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
			displayCol++;
			if (displayCol == 80)
			{
				displayRow++;
				displayCol = 0;
				if (displayRow == 25)
				{
					displayRow = 24;
					displayCol = 0;
					scrollScreen();
				}
			}
		}
		// asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
		// asm volatile("int $0x20":::"memory"); //XXX Testing irqTimer during syscall
	}

	updateCursor(displayRow, displayCol);
	// TODO take care of return value
	return;
}

void *memcpy(void *dest, const void *src, size_t n)
{
	char *d = (char *)dest;
	const char *s = (const char *)src;

	for (size_t i = 0; i < n; i++)
	{
		d[i] = s[i];
	}

	return dest;
}

void syscallFork(struct StackFrame *sf)
{
	// 寻找空闲的 PCB
	int child = -1;
	for (int i = 0; i < MAX_PCB_NUM; i++)
	{
		if (pcb[i].state == STATE_DEAD)
		{
			child = i;
			break;
		}
	}

	// 如果没有空闲的 PCB，返回 -1
	if (child == -1)
	{
		sf->eax = -1; // 父进程返回 -1
		log("fork failed: no available PCB\n");
		return;
	}

	enableInterrupt();
	for (int j = 0; j < 0x100000; j++)
	{
		*(uint8_t *)(j + (child + 1) * 0x100000) = *(uint8_t *)(j + (current + 1) * 0x100000);
		// asm volatile("int $0x20"); //XXX Testing irqTimer during syscall
	}
	disableInterrupt();

	for (int j = 0; j < sizeof(ProcessTable); ++j)
		*((uint8_t *)(&pcb[child]) + j) = *((uint8_t *)(&pcb[current]) + j);

	pcb[child].stackTop = (uint32_t)&(pcb[child].regs);
	pcb[child].prevStackTop = (uint32_t)&(pcb[child].stackTop);

	pcb[child].regs.ss = USEL(4 * child);
	pcb[child].regs.cs = USEL(3 * child);
	pcb[child].regs.ds = USEL(4 * child);
	pcb[child].regs.es = USEL(4 * child);
	pcb[child].regs.fs = USEL(4 * child);
	pcb[child].regs.gs = USEL(4 * child);

	pcb[child].pid = child;			   // 分配新的 PID
	pcb[child].state = STATE_RUNNABLE; // 子进程初始状态为可运行
	pcb[child].timeCount = 0;		   // 初始化时间片计数器
	pcb[child].sleepTime = 0;		   // 初始化睡眠时间

	pcb[child].regs.eax = 0;
	pcb[current].regs.eax = child;
	log("fork\n");
}
void syscallSleep(struct StackFrame *sf)
{

	uint32_t time = sf->ecx;
	pcb[current].state = STATE_BLOCKED;
	pcb[current].sleepTime = time;
	putChar('0' + current);
	log("sleep\n");
	asm volatile("int $0x20");
}
void syscallExit(struct StackFrame *sf)
{
	// TODO
	pcb[current].state = STATE_DEAD;
	log("exit\n");
	asm volatile("int $0x20");
}