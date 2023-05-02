#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

volatile uint64_t counter;
uint8_t array[256 * 512]; 
int access[100];
int flag[256];
int result[256];
uint8_t temp = 0;

static inline void flush(void *addr) {
	asm volatile ("DC CIVAC, %[ad]" : : [ad] "r" (addr));
	asm volatile("DSB SY");
}

void *inc_counter(void *para)
{
    /*
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(0, &cpu_set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set)) {
        perror("pthread_setaffinity_np");
    }
    */
    while (1) {
        counter++;
        asm volatile ("DMB SY");
    }
}

static uint64_t counter_read(volatile uint8_t *addr) {
	uint64_t ns = counter;

	asm volatile (
		"DSB SY\n"
		"LDR X5, [%[ad]]\n"
		"DSB SY\n"
		: : [ad] "r" (addr) : "x5");

	return counter - ns;
}


static inline uint64_t pmccntr_read(volatile uint8_t *addr) {
    uint64_t count_num1 = 0, count_num2 = 0;
    asm volatile ("DSB SY");
    asm volatile ("mrs %0, pmccntr_el0": "+r"(count_num1));
    asm volatile (
		"DSB SY\n"
		"LDR X5, [%[ad]]\n"
		"DSB SY\n"
		: : [ad] "r" (addr) : "x5");
    asm volatile ("mrs %0, pmccntr_el0": "+r"(count_num2));
    asm volatile ("DSB SY");
    return count_num2 - count_num1;
}

static inline uint64_t clock_read(volatile uint8_t *addr) {
    struct timespec time1, time2;
	asm volatile ("DSB SY");
    clock_gettime(CLOCK_MONOTONIC, &time1);
    asm volatile ("DSB SY");
    asm volatile (
		"DSB SY\n"
		"LDR X5, [%[ad]]\n"
		"DSB SY\n"
		: : [ad] "r" (addr) : "x5");
    asm volatile ("DSB SY");
    clock_gettime(CLOCK_MONOTONIC, &time2);
    asm volatile ("DSB SY");

	return time2.tv_nsec - time1.tv_nsec;
}

void victim_function()
{
    /*for (int i = 0; i < 100; i++) {
        array[fl[i]]++;
    }*/
    for (int i = 0; i < 100; i++) {
		temp &= array[access[i] * 512]++;
    }
}

void flush_reload() {
	static int results[256];
	int tries, i, j, k, mix_i;
	register uint64_t time2;

	for (i = 0; i < 256; i++) {
		results[i] = 0;
    }

	/* Flush array2[256*(0..255)] from cache */
	for (i = 0; i < 256; i++) {
		flush(&array[i * 512]); /* intrinsic for clflush instruction */
    }

    victim_function();

	/* Time reads. Order is lightly mixed up to prevent stride prediction */
	for (i = 0; i < 256; i++)
	{
		mix_i = ((i * 167) + 13) & 255;
		time2 = counter_read(&array[mix_i * 512]);
		result[mix_i] = time2;
	}
}

int main()
{
    int i = 0, mix_i = 0;
    register uint64_t tmp_time;
    time_t t;

    for (i = 0; i < 256 * 512; i++) {
		array[i] = 1; /* write to array2 so in RAM not copy-on-write zero pages */
    }

    srand((unsigned)time(&t));
    for (i = 0; i < 100; i++) {
        access[i] = 0;
    } 
    for (i = 0; i < 256; i++) {
        flag[i] = 0;
    }
    for (i = 0; i < 100; i++) {
        access[i] = rand() % 256;
    }
    for (i = 0; i < 100; i++) {
        flag[access[i]] = 1;
    }

    pthread_t inc_counter_thread;
    if (pthread_create(&inc_counter_thread, NULL, inc_counter, NULL) != 0) {
        printf("pthread_create failed.\n");
        return 1;
    }
	while (counter < 10000000);
    asm volatile ("DSB SY");
    
    /*
    for (i = 0; i < 256; i++) {
        fl[0] = 0;
    }
    for (i = 0; i < 256; i++) {
        fl[rand() % 256] = 1;
        //array[i * 512]++;
    }*/


    for (int try = 999; try > 998; try--) {
        flush_reload();
/*
        for (i = 0; i < 256; i++) {
            flush(&array[i * 512]);
        }

        //victim_function();

        for (i = 0; i < 256; i++) {
            mix_i = ((i * 167) + 13) & 255;
			tmp_time = counter_read(&array[mix_i * 512]);
			printf("%llu\t", tmp_time);
            //flush(&array[mix_i * 512]);
            //printf("%llu\t", clock_read(&array[mix_i * 512]));
            if (fl[mix_i] == 1) {
                //flush(&array[mix_i * 512]);
                //printf("%llu\t", counter_read(&array[mix_i * 512]));
                result[mix_i] = (result[mix_i] * try + counter_read(&array[mix_i * 512])) / (try + 1);
            } else{
                //printf("%llu\t", counter_read(&array[mix_i * 512]));
                result[mix_i] = (result[mix_i] * try + counter_read(&array[mix_i * 512])) / (try + 1);
            }
        }
    */
    }

    FILE *fp = NULL;

    fp = fopen("./time.log", "w+");
    for (i = 0; i < 256; i++) {
        if (flag[i] == 1) {
            printf("%llu\t", result[i]);
            fprintf(fp, "%d %d\n", i, result[i]);
        } else{
            printf("-%llu\t", result[i]);
            fprintf(fp, "%d %d\n", i, -result[i]);
        }
    }
    fclose(fp);

    return 0;
}

/*
*三种方法获取时间：
*1、读PMCCNTR_EL0寄存器，需要内核态使能
*2、另起一个计数线程用来模拟时间戳
*3、使用clock_gettime函数

        mix_i = ((i * 167) + 13) & 255;
        printf("%llu\t", pmccntr_read(&array[mix_i * 512]));
*/
/*
int main()
{
    
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(1, &cpu_set);
    if (sched_setaffinity(0, sizeof(cpu_set), &cpu_set) < 0) {
        perror("sched_setaffinity");
    }
    
    uint64_t count_num1, count_num2, ns1, ns2;
    volatile uint64_t a = 0;
    
    asm volatile ("DSB SY");
    asm volatile ("mrs %0, pmccntr_el0": "+r"(count_num1));
    asm volatile ("DSB SY");
    a++;
    asm volatile ("DSB SY");
    asm volatile ("mrs %0, pmccntr_el0": "+r"(count_num2));
    asm volatile ("DSB SY");

    printf("%llu\n%llu\n", pmccntr_read(b), count_num2 - count_num1);

    pthread_t inc_counter_thread;
    if (pthread_create(&inc_counter_thread, NULL, inc_counter, NULL) != 0) {
        printf("pthread_create");
        return 1;
    }

	while (counter < 10000000);
    asm volatile ("DSB SY");
    ns1 = counter;
    asm volatile ("DSB SY");
    a++;
    asm volatile ("DSB SY");
    ns2 = counter;
    asm volatile ("DSB SY");

    struct timespec time1, time2;
    asm volatile ("DSB SY");
    clock_gettime(CLOCK_MONOTONIC, &time1);
    asm volatile ("DSB SY");
    a++;
    asm volatile ("DSB SY");
    clock_gettime(CLOCK_MONOTONIC, &time2);
    asm volatile ("DSB SY");
    printf("%llu\n%llu\n%llu\n", ns2 - ns1, count_num2 - count_num1, time2.tv_nsec - time1.tv_nsec);
    return 0;
}*/
