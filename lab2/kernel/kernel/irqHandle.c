#include "x86.h"
#include "device.h"

extern int displayRow;
extern int displayCol;

extern uint32_t keyBuffer[MAX_KEYBUFFER_SIZE];
extern int bufferHead;
extern int bufferTail;


void GProtectFaultHandle(struct TrapFrame *tf);

void KeyboardHandle(struct TrapFrame *tf);

void syscallHandle(struct TrapFrame *tf);
void syscallWrite(struct TrapFrame *tf);
void syscallPrint(struct TrapFrame *tf);
void syscallRead(struct TrapFrame *tf);
void syscallGetChar(struct TrapFrame *tf);
void syscallGetStr(struct TrapFrame *tf);


void irqHandle(struct TrapFrame *tf) { // pointer tf = esp
	/*
	 * 中断处理程序
	 */
	/* Reassign segment register */
	asm volatile("movw %%ax, %%ds"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%es"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%fs"::"a"(KSEL(SEG_KDATA)));
	//asm volatile("movw %%ax, %%gs"::"a"(KSEL(SEG_KDATA)));
	switch(tf->irq) {
		// TODO: 填好中断处理程序的调用
		case 0xd:
			GProtectFaultHandle(tf);
			break;
		case 0x21:
			KeyboardHandle(tf);
			break;
		case 0x80:
			syscallHandle(tf);
			break;
		default:assert(0);
	}
}

void GProtectFaultHandle(struct TrapFrame *tf){
	assert(0);
	return;
}

void KeyboardHandle(struct TrapFrame *tf){
	uint32_t code = getKeyCode();
	//TODO: 完成键盘中断处理
	if (code == 0xe)
	{ // 如果是退格键
		if (displayCol > 0)
		{																   // 只能退格到当前行的行首
			displayCol--;												   // 光标列号减 1
			uint16_t data = 0 | (0x0c << 8);							   // 空字符，亮红色前景
			int pos = (displayRow * 80 + displayCol) * 2;				   // 计算显存位置
			asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000)); // 写入空字符到显存
		}
	}
	else if (code == 0x1c)
	{					// 如果是回车键
		displayRow++;	// 光标行号加 1
		displayCol = 0; // 光标列号重置为 0
		if (displayRow >= 25)
		{					 // 如果超过屏幕范围
			scrollScreen();	 // 滚动屏幕
			displayRow = 24; // 将光标行号设置为最后一行
		}
	}
	else if (code < 0x81)
	{							 // 如果是普通字符
		char ch = getChar(code); // 将扫描码转换为字符
		if (ch >= 'a' && ch <= 'z')
		{			  // 如果是小写字母
			ch -= 32; // 转换为大写字母
		}
		uint16_t data = ch | (0x0c << 8);							   // 字符，亮红色前景
		int pos = (displayRow * 80 + displayCol) * 2;				   // 计算显存位置
		asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000)); // 写入字符到显存
		displayCol++;												   // 光标列号加 1
		if (displayCol >= 80)
		{					// 如果到达行尾
			displayCol = 0; // 光标列号重置为 0
			displayRow++;	// 光标行号加 1
			if (displayRow >= 25)
			{					 // 如果超过屏幕范围
				scrollScreen();	 // 滚动屏幕
				displayRow = 24; // 将光标行号设置为最后一行
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallHandle(struct TrapFrame *tf) {
	switch(tf->eax) { // syscall number
		case 0:
			syscallWrite(tf);
			break; // for SYS_WRITE
		case 1:
			syscallRead(tf);
			break; // for SYS_READ
		default:break;
	}
}

void syscallWrite(struct TrapFrame *tf) {
	switch(tf->ecx) { // file descriptor
		case 0:
			syscallPrint(tf);
			break; // for STD_OUT
		default:break;
	}
}

void syscallPrint(struct TrapFrame *tf) {
	int sel = USEL(SEG_UDATA); // TODO: segment selector for user data, need further modification
	char *str = (char*)tf->edx;
	int size = tf->ebx;
	int i = 0;
	int pos = 0;
	char character = 0;
	uint16_t data = 0;
	asm volatile("movw %0, %%es"::"m"(sel));
	for (i = 0; i < size; i++) {
		asm volatile("movb %%es:(%1), %0":"=r"(character):"r"(str+i));
		// TODO: 完成光标的维护和打印到显存
		if(character == '\n'){
			displayRow++;
			displayCol = 0;
			if(displayRow >= 25){
				scrollScreen();
				displayRow = 24;
			}
			continue;
		}
		data = character | (0x0c << 8);	
		pos = (displayRow * 80 + displayCol) * 2;
		asm volatile("movw %0, (%1)" ::"r"(data), "r"(pos + 0xb8000));
		displayCol++;
		if(displayCol >= 80){
			displayCol = 0;
			displayRow++;
			if(displayRow >= 25){
				scrollScreen();
				displayRow = 24;
			}
		}
	}
	updateCursor(displayRow, displayCol);
}

void syscallRead(struct TrapFrame *tf){
	switch(tf->ecx){ //file descriptor
		case 0:
			syscallGetChar(tf);
			break; // for STD_IN
		case 1:
			syscallGetStr(tf);
			break; // for STD_STR
		default:break;
	}
}

void syscallGetChar(struct TrapFrame *tf){
	// TODO: 自由实现
	// 等待键盘缓冲区有数据
	while (bufferHead == bufferTail);

	// 从缓冲区读取一个字符
	char ch = keyBuffer[bufferHead];

	// 更新缓冲区头部指针
	bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

	// 将字符返回给用户
	tf->eax = ch;
}

void syscallGetStr(struct TrapFrame *tf){
	// TODO: 自由实现
	char *buf = (char *)tf->edx; // 用户缓冲区地址
	int size = tf->ebx;			 // 缓冲区大小
	int i = 0;					 // 当前读取的字符数

	// 循环读取字符
	while (i < size - 1)
	{ // 留一个位置给字符串结束符
		// 等待键盘缓冲区有数据
		while (bufferHead == bufferTail)
			;

		// 从缓冲区读取一个字符
		char ch = keyBuffer[bufferHead];

		// 更新缓冲区头部指针
		bufferHead = (bufferHead + 1) % MAX_KEYBUFFER_SIZE;

		// 如果遇到换行符，结束读取
		if (ch == '\n')
		{
			break;
		}

		// 将字符存储到用户缓冲区
		buf[i++] = ch;
	}

	// 添加字符串结束符
	buf[i] = '\0';

	// 返回读取的字符数
	tf->eax = i;
}