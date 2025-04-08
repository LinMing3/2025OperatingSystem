#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;

extern int inGetChar;

void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);

void irqHandle(struct TrapFrame *tf)
{ // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds" ::"a"(KSEL(SEG_KDATA)));
	// asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	// asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	// asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch (tf->irq)
	{
	// TODO: 填好中断处理程序的调用
	case -1:
		break;
	case 0xd:
		log("GProtectFaultHandle\n");
		GProtectFaultHandle(tf);
		break;
	case 0x21:
		log("KeyboardHandle\n");
		KeyboardHandle(tf);
		break;
	case 0x80:
		log("syscallHandle\n");
		syscallHandle(tf);
		break;
	default:
		log("irqHandle: unknown interrupt\n");
		assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf)
{
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf)
{
	uint32_t code = getKeyCode();
	// TODO: 完成键盘中断处理
	if (code == 0xe)
	{ // 如果是退格键
		if (bufferHead != bufferTail)
		{ // 缓冲区中有数据时才允许退格
			bufferTail = (bufferTail - 1 + MAX_KEYBUFFER_SIZE) % MAX_KEYBUFFER_SIZE;
			// 删除屏幕上的字符
			if (displayCol > 0)
			{
				displayCol--;
			}
			else if (displayRow > 0)
			{
				displayRow--;
				displayCol = 79;
			}
			int pos = (displayRow * 80 + displayCol) * 2;
			asm volatile("movw $0x0720, (%0)" ::"r"(pos + 0xb8000)); // 用空格覆盖字符
			updateCursor(displayRow, displayCol);
		}
	}
	else if (code == 0x1c)
	{								  // 如果是回车键
		keyBuffer[bufferTail] = '\n'; // 将换行符添加到缓冲区
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;

		// 如果缓冲区满了，丢弃最旧的数据
		if (bufferTail == bufferHead)
		{
			bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
		}
		// 换行处理
		displayRow++;
		displayCol = 0;
		if (displayRow >= 25)
		{
			scrollScreen();
			displayRow = 24;
		}
		updateCursor(displayRow, displayCol);
	}
	else if (code < 0x81)
	{								// 如果是普通字符
		char ch = getChar(code);	// 将扫描码转换为字符
		keyBuffer[bufferTail] = ch; // 将字符添加到缓冲区
		bufferTail = (bufferTail + 1) % MAX_KEYBUFFER_SIZE;

		// 如果缓冲区满了，丢弃最旧的数据
		if (bufferTail == bufferHead)
		{
			bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;
		}
		// 输出字符到屏幕
		uint16_t data = ch | (0x0c << 8); // 字符和颜色属性
		int pos = (displayRow * 80 + displayCol) * 2;
		asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
		displayCol++;
		if (displayCol >= 80)
		{
			displayCol = 0;
			displayRow++;
			if (displayRow >= 25)
			{
				scrollScreen();
				displayRow = 24;
			}
		}
		updateCursor(displayRow, displayCol);
	}

	// updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf)
{
	switch (tf->eax)
	{ // syscall number
	case 0:
		syscallWrite(tf);
		break; // for SYS_WRITE
	case 1:
		syscallRead(tf);
		break; // for SYS_READ
	default:
		break;
	}
}

void syscallWrite(struct TrapFrame *tf)
{
	switch (tf->ecx)
	{ // file descriptor
	case 0:
		syscallPrint(tf);
		break; // for STD_OUT
	default:
		break;
	}
}

void syscallPrint(struct TrapFrame *tf)
{
	int sel = USEL(SEG_UDATA); // TODO: segment selector for user data, need further modification
	char *str = (char *)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es" ::"m"(sel));
	for (i = 0; i < size; i++)
	{
		asm volatile("movb %%es:(%1), %0" : "=r"(character) : "r"(str + i));
		// TODO: 完成光标的维护和打印到显存
		if (character == '\n')
		{
			displayRow++;
			displayCol = 0;
			if (displayRow >= 25)
			{
				scrollScreen();
				displayRow = 24;
			}
			continue;
		}
		data = character | (0x0c << 8);
		pos = (displayRow * 80 + displayCol) * 2;
		asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
		displayCol++;
		if (displayCol >= 80)
		{
			displayCol = 0;
			displayRow++;
			if (displayRow >= 25)
			{
				scrollScreen();
				displayRow = 24;
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf)
{
	switch (tf->ecx)
	{ // file descriptor
	case 0:
		syscallGetChar(tf);
		break; // for STD_IN
	case 1:
		syscallGetStr(tf);
		break; // for STD_STR
	default:
		break;
	}
}

void syscallGetChar(struct TrapFrame *tf)
{
	log("get char\n");
	// 等待缓冲区有数据
	int i = 0;
	while (i < 1) // 保留最后一个字节用于存储 '\0'
	{
		// 等待键盘缓冲区有数据
		while (bufferHead == bufferTail)
		{
			log("");
		}

		// 从键盘缓冲区读取字符
		char ch = keyBuffer[bufferHead];
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

		
		if (ch == '\n') {
			// 遇到换行符结束
			int lastIndex = (bufferTail - 2 + MAX_KEYBUFFER_SIZE) % MAX_KEYBUFFER_SIZE;
			ch = keyBuffer[lastIndex];
			tf->eax = (uint32_t)ch; 	// 将最后一个字符返回给用户程序
			break;
		}
	}
	log("get char return\n");
}

void syscallGetStr(struct TrapFrame *tf)
{
	log("get str\n");
	// TODO: 自由实现
	int sel = USEL(SEG_UDATA);		 // 用户数据段选择子
	char *userStr = (char *)tf->edx; // 用户态传递的字符串缓冲区地址
	int size = tf->ebx;				 // 用户态传递的缓冲区大小
	int i = 0;

	asm volatile("movw %0, %%es" ::"m"(sel)); // 切换到用户数据段

	while (i < size - 1) // 保留最后一个字节用于存储 '\0'
	{
		// 等待键盘缓冲区有数据
		while (bufferHead == bufferTail){
			log("");
		}

		// 从键盘缓冲区读取字符
		char ch = keyBuffer[bufferHead];
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

		// 将字符写入用户态缓冲区
		asm volatile("movb %0, %%es:(%1)" ::"r"(ch), "r"(userStr + i));

		if (ch == '\n') // 遇到换行符结束
			break;

		i++;
	}

	// 添加字符串结束符
	asm volatile("movb $0, %%es:(%0)" ::"r"(userStr + i));

	log("get str return\n");
}