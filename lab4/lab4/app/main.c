#include "lib.h"
#include "types.h"


#define NUM_PHILOSOPHERS 5
#define LEFT(id) (id)
#define RIGHT(id) ((id + 1) % 3)

sem_t forks[3];  // 3个叉子信号量
sem_t mutex;     // 1个互斥信号量

// 哲学家行为函数
void philosopher(int id) {
    int i = 0;
    int pid = getpid();  // 获取进程ID
    
    while(i < 3) {  // 每个哲学家吃3次饭
        i++;
        
        // 思考
        printf("Philosopher %d (PID %d): think\n", id, pid);
        sleep(128);
        
        // 获取互斥锁
        sem_wait(&mutex);
        printf("Philosopher %d (PID %d): hungry\n", id, pid);
        sleep(128);
        
        // 哲学家0-2使用实际叉子
        if (id < 3) {
            // 拿左边叉子
            sem_wait(&forks[LEFT(id)]);
            printf("Philosopher %d (PID %d): take left fork %d\n", id, pid, LEFT(id));
            sleep(128);
            
            // 拿右边叉子
            sem_wait(&forks[RIGHT(id)]);
            printf("Philosopher %d (PID %d): take right fork %d\n", id, pid, RIGHT(id));
            sleep(128);
        }
        // 哲学家3-4使用虚拟叉子
        else {
            printf("Philosopher %d (PID %d): takes virtual forks\n", id, pid);
            sleep(128);
        }
        
        // 释放互斥锁，允许其他哲学家拿叉子
        sem_post(&mutex);
        
        // 就餐
        printf("Philosopher %d (PID %d): eat\n", id, pid);
        sleep(128);
        
        // 放下叉子
        if (id < 3) {
            // 放下左边叉子
            sem_post(&forks[LEFT(id)]);
            printf("Philosopher %d (PID %d): put left fork %d\n", id, pid, LEFT(id));
            sleep(128);
            
            // 放下右边叉子
            sem_post(&forks[RIGHT(id)]);
            printf("Philosopher %d (PID %d): put right fork %d\n", id, pid, RIGHT(id));
            sleep(128);
        }
        else {
            printf("Philosopher %d (PID %d): puts down virtual forks\n", id, pid);
            sleep(128);
        }
    }
    
    printf("Philosopher %d (PID %d): finished dining\n", id, pid);
}

int uEntry(void) {
    int i;
    int ret;
    int pid[NUM_PHILOSOPHERS];
    
    // 初始化互斥锁
    ret = sem_init(&mutex, 1);  // 互斥锁初始值为1
    if (ret == -1) {
        printf("Mutex initialization failed.\n");
        exit();
    }
    
    // 初始化3个叉子的信号量
    for (i = 0; i < 3; i++) {
        ret = sem_init(&forks[i], 1);  // 每个叉子初始值为1
        if (ret == -1) {
            printf("Fork %d initialization failed.\n", i);
            exit();
        }
    }
    
    printf("Philosopher dinner is starting...\n");
    
    // 创建5个哲学家进程
    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        pid[i] = fork();
        
        if (pid[i] == 0) {  // 子进程
            philosopher(i);
            exit();  // 子进程结束
        }
        else if (pid[i] == -1) {
            printf("Fork failed for philosopher %d\n", i);
            exit();
        }
    }
    
    // 主进程等待所有哲学家用餐完毕
    for (i = 0; i < NUM_PHILOSOPHERS; i++) {
        sleep(1024);  // 简单等待，确保有足够时间
    }
    
    // 销毁信号量
    for (i = 0; i < 3; i++) {
        sem_destroy(&forks[i]);
    }
    sem_destroy(&mutex);
    
    printf("Dinner is over.\n");
    return 0;
}

// int uEntry(void) {
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
	
// 	return 0;
// }
