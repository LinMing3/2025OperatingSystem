#include "lib.h"
#include "types.h"

#define NUM_PHILOSOPHERS 5

sem_t forks[NUM_PHILOSOPHERS-1]; // 5根筷子的信号量
sem_t mutex;                  // 控制同时拿筷子的互斥信号量

// 哲学家行为函数
void philosopher(int id) {
    while(1) {
        // 思考
        printf("Philosopher %d: think\n", id);
        sleep(128);
        
        // 拿筷子（避免死锁）
        sem_wait(&mutex);  // 保证最多只有4位哲学家同时尝试拿筷子
        
        printf("Philosopher %d: hungry\n", id);
        sleep(128);

		// 拿筷子 - 特殊处理最后一个哲学家不使用单独的信号量
		if (id < NUM_PHILOSOPHERS - 1)
		{
			sem_wait(&forks[id]);
			printf("Philosopher %d: take fork %d\n", id, id);
			sleep(128);
		}

		// 拿右边筷子或特殊处理
		if (id < NUM_PHILOSOPHERS - 1)
		{
			if ((id + 1) % NUM_PHILOSOPHERS < NUM_PHILOSOPHERS - 1)
			{
				sem_wait(&forks[(id + 1) % NUM_PHILOSOPHERS]);
				printf("Philosopher %d: take fork %d\n", id, (id + 1) % NUM_PHILOSOPHERS);
				sleep(128);
			}
		}

		// 就餐
		printf("Philosopher %d: eat\n", id);
		sleep(128);

		// 放下筷子
		if (id < NUM_PHILOSOPHERS - 1)
		{
			sem_post(&forks[id]);
			printf("Philosopher %d: put fork %d\n", id, id);
			sleep(128);
		}

		// 放下右边筷子或特殊处理
		if (id < NUM_PHILOSOPHERS - 1)
		{
			if ((id + 1) % NUM_PHILOSOPHERS < NUM_PHILOSOPHERS - 1)
			{
				sem_post(&forks[(id + 1) % NUM_PHILOSOPHERS]);
				printf("Philosopher %d: put fork %d\n", id, (id + 1) % NUM_PHILOSOPHERS);
				sleep(128);
			}
		}

		sem_post(&mutex); // 释放互斥锁
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
	// printf("Input:\" Test %%c Test %%6s %%d %%x\"\n");
	// ret = scanf(" Test %c Test %6s %d %x", &cha, str, &dec, &hex);
	// printf("Ret: %d; %c, %s, %d, %x.\n", ret, cha, str, dec, hex);
	
	
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
	// }

	// For lab4.3
	// TODO: You need to design and test the philosopher problem.
	// Note that you can create your own functions.
	// Requirements are demonstrated in the guide.
	int i;
	int ret;
	int pid[NUM_PHILOSOPHERS];

	// 初始化信号量
	ret = sem_init(&mutex, NUM_PHILOSOPHERS - 1); // 最多允许4个哲学家同时尝试拿筷子
	if (ret == -1)
	{
		printf("Mutex initialization failed.\n");
		exit();
	}

	// 初始化每根筷子的信号量
	for (i = 0; i < NUM_PHILOSOPHERS-1; i++)
	{
		ret = sem_init(&forks[i], 1);
		if (ret == -1)
		{
			printf("Fork %d initialization failed.\n", i);
			exit();
		}
	}

	printf("Philosopher dinner is starting...\n");

	// 创建5个哲学家进程
	for (i = 0; i < NUM_PHILOSOPHERS; i++)
	{
		pid[i] = fork();

		if (pid[i] == 0)
		{ // 子进程
			philosopher(i);
			exit(); // 子进程结束
		}
		else if (pid[i] == -1)
		{
			printf("Fork failed for philosopher %d\n", i);
			exit();
		}
	}

	// 主进程等待所有哲学家进程结束
	// 注意：实际上这段代码不会执行到，因为哲学家函数是无限循环
	while (1)
	{
		sleep(1024);
	}
	return 0;
}
