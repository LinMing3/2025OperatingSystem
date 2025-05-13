#include "lib.h"
#include "types.h"

#define N 5 // 哲学家数量

sem_t forks[N]; // 每把叉子一个信号量

void philosopher(int id)
{
	while (1)
	{
		// 思考
		printf("Philosopher %d: think\n", id);
		sleep(128);

		// 拿起叉子（按奇偶性决定拿叉子的顺序）
		if (id % 2 == 0)
		{
			// 偶数哲学家先拿左边的叉子
			// printf("Philosopher %d: try to get left fork %d\n", id, id);
			sem_wait(&forks[id]);
			sleep(128);

			// printf("Philosopher %d: try to get right fork %d\n", id, (id + 1) % N);
			sem_wait(&forks[(id + 1) % N]);
			sleep(128);
		}
		else
		{
			// 奇数哲学家先拿右边的叉子
			// printf("Philosopher %d: try to get right fork %d\n", id, (id + 1) % N);
			sem_wait(&forks[(id + 1) % N]);
			sleep(128);

			// printf("Philosopher %d: try to get left fork %d\n", id, id);
			sem_wait(&forks[id]);
			sleep(128);
		}

		// 进餐
		printf("Philosopher %d: eat\n", id);
		sleep(128);

		// 放下叉子
		// printf("Philosopher %d: put down left fork %d\n", id, id);
		sem_post(&forks[id]);
		sleep(128);

		// printf("Philosopher %d: put down right fork %d\n", id, (id + 1) % N);
		sem_post(&forks[(id + 1) % N]);
		sleep(128);
	}
}

int uEntry(void) {
// For lab4.1
// // Test 'scanf'
// int dec = 0;
// int hex = 0;
// char str[6];
// char cha = 0;
// int ret = 0;
// while(1){
// 	printf("Input:\" Test %%c Test %%6s %%d %%x\"\n");
// 	ret = scanf(" Test %c Test %6s %d %x", &cha, str, &dec, &hex);
// 	printf("Ret: %d; %c, %s, %d, %x.\n", ret, cha, str, dec, hex);
// 	if(ret==4){
// 		break;
// 	}
// }


// exit();
// For lab4.2
// Test 'Semaphore'
// int i = 4;

// sem_t sem;
// printf("Father Process: Semaphore Initializing.\n");
// ret = sem_init(&sem, 2);
// if (ret == -1) {
// 	printf("Father Process: Semaphore Initializing Failed.\n");
// 	exit();
// }

// ret = fork();
// if (ret == 0) {
// 	while( i != 0) {
// 		i --;
// 		printf("Child Process: Semaphore Waiting.\n");
// 		sem_wait(&sem);
// 		printf("Child Process: In Critical Area.\n");
// 	}
// 	printf("Child Process: Semaphore Destroying.\n");
// 	sem_destroy(&sem);
// 	exit();
// }
// else if (ret != -1) {
// 	while( i != 0) {
// 		i --;
// 		printf("Father Process: Sleeping.\n");
// 		sleep(128);
// 		printf("Father Process: Semaphore Posting.\n");
// 		sem_post(&sem);
// 	}
// 	printf("Father Process: Semaphore Destroying.\n");
// 	sem_destroy(&sem);
// 	exit();
// }i

// For lab4.3
// TODO: You need to design and test the philosopher problem.
// Note that you can create your own functions.
// Requirements are demonstrated in the guide.


// 哲学家就餐问题测试主函数
int i;
int pid;

// 初始化叉子信号量
for (i = 0; i < N; i++)
{
	sem_init(&forks[i], 1); // 初值为1，表示叉子可用
}

// 创建5个哲学家进程
for (i = 0; i < N; i++)
{
	pid = fork();
	if (pid == 0)
	{ // 子进程
		philosopher(i);
		exit(); // 子进程结束
	}
	else if (pid < 0)
	{
		printf("Fork failed for philosopher %d\n", i);
	}
}

// 父进程等待
for (i = 0; i < 10; i++)
{ // 让程序运行一段时间
	sleep(1000);
}

// 清理信号量
for (i = 0; i < N; i++)
{
	sem_destroy(&forks[i]);
}
return 0;
}
