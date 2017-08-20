#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>
//init thread
pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

//states of threads
#define RUNNING 	1
#define SUSPENDED	2
#define TERMINATED	3
#define BLOCKED 	4

//quantum times
#define QUANTUM_TIME 500
//To show how many clock cycles in one second
//this value is set by get_clock_freq() function
		FILE *input,
*output;

//Working Schemas of the queues
#define FIFO 1
#define SJB 2
#define PRI 3

//PCB structure
typedef struct pcb {
	int IOFlag;		// for I/O control;
	int pid, cputime, priority, runtime; // from the file
	int run_control_flag, finished_flag;
	int number_of_preempt, number_of_queued; // stats
	void * main_thread; //main thread function pointer, set at initialization
	pthread_t thread_id; // thread id used in createthread function
	struct pcb *next; //to queue a process we need a next pointer
	struct pcb *previous;
} PCB;
typedef struct queue {
	struct pcb *head;
	int count;
	int work_schema;
} QUEUE;

void* main_thread(void *runtime);
void* enqueue(PCB *n, QUEUE* h);
int* standart_dequeue(QUEUE* h);
int get_time();
void initialize();
int findmax(QUEUE* r);
int dequeue_by_pid(QUEUE* h, int n);
void *create_queue(void);
int randm(int ma, int mi);

QUEUE* tqueue;
// Termination queue
QUEUE* bqueue;
// block queue
QUEUE* rqueue;
//Ready queue
QUEUE* pqueue;
//Process queue
QUEUE* squeue;
//Suspended queue

int returnToMainThread = 0;
int returnToAdmission = 0;
int uniquepid = 5;
int miniProcessControl = 0;

void initialize() {
	//get_clock_freq();		/* ***init *** */
	//get_time();			/* ***timers***  */

}

void fillProcessQueue() {
	int pid;
	int runtime;
	int priority;
	int cputime;
	output = fopen("inputsim.txt", "r");
	while (!feof(output)) {
		PCB* pcb = malloc(sizeof(PCB));
		fscanf(output, "%d	%d	%d	%d", &pid, &runtime, &priority, &cputime);
		pcb->pid = pid;
		pcb->runtime = runtime;
		pcb->priority = priority;
		pcb->cputime = cputime;
		pcb->IOFlag = 0;
		pcb->number_of_queued++;
		if (feof(output)) {
			break;
		}
		enqueue(pcb, pqueue);
	}
	fclose(output);
}

void admission() {
	int randomBlockFlag = randm(20, 1);  // random create chance 1/15

	if (randomBlockFlag == 1) {
		PCB* pcb1 = malloc(sizeof(PCB));
		int randomBlockPriority = randm(5, 0);
		uniquepid++;
		pcb1->pid = uniquepid;
		pcb1->priority = randomBlockPriority; //block process input by user.
		pcb1->IOFlag = 1;
		enqueue(pcb1, rqueue);
	}
	PCB* pcb = rqueue->head; /* thread  */
	while (pcb != NULL) {
		int max = findmax(rqueue);
		if (max == pcb->priority) {
			pcb->run_control_flag = RUNNING;
			int a = pcb->pid;
			pthread_create(&pcb->thread_id, NULL, main_thread, (void *) a);
			break;
		}
		pcb = pcb->next;

	}
	returnToAdmission = 1;
}

void sched() {
	PCB* pcb2 = pqueue->head;
	while (pcb2 != NULL) {
		if (pcb2->cputime <= get_time() + 5
				&& pcb2->cputime >= get_time() - 5) {
			dequeue_by_pid(pqueue, pcb2->pid);
			enqueue(pcb2, rqueue);
			pcb2->number_of_queued++;
		}
		pcb2 = pcb2->next;
	}
	if (rqueue->head != NULL) {
		if (returnToAdmission == 0 || returnToMainThread == 0) {
			admission();
		} else if (returnToMainThread == 1 || returnToAdmission == 1) {
			int randomBlockReturnTime = randm(3, 1); // IO beklemesinin bitip bitmediği random olarak tanımlı 1-2
			PCB* rpcb;
			rpcb = rqueue->head;
			while (rpcb != NULL) {
				int max = findmax(rqueue);
				if (max == rpcb->priority || rpcb->run_control_flag == BLOCKED) {
					if (max == rpcb->priority) {
						printf(
								"thread id =%d geldi runtime = %d controlflag %d ",
								rpcb->pid, rpcb->runtime,
								rpcb->run_control_flag);
						if (rpcb->run_control_flag == SUSPENDED) {
							dequeue_by_pid(rqueue, rpcb->pid);
							enqueue(rpcb, squeue);
							rpcb->number_of_preempt++; // sembolik olarak quantum zamanda işlemi bitmeyen processler suspend queue ya atılır ve
							dequeue_by_pid(squeue, rpcb->pid); // priorityleri arttırılarak ready queue ya geri konur.
							enqueue(rpcb, rqueue);
							rpcb->priority++;
							rpcb->number_of_queued++;
						} else if (rpcb->run_control_flag == TERMINATED) {
							dequeue_by_pid(rqueue, rpcb->pid);
							enqueue(rpcb, tqueue);
							rpcb->finished_flag = 1;
						}
					}
				}
				if (rpcb->run_control_flag == BLOCKED) {
					dequeue_by_pid(rqueue, rpcb->pid);
					enqueue(rpcb, bqueue);
					rpcb->number_of_preempt++;
					if (randomBlockReturnTime == 1) {
						dequeue_by_pid(bqueue, rpcb->pid);
						enqueue(rpcb, rqueue);
						rpcb->number_of_queued++;
					} else if (randomBlockReturnTime != 1) {
						dequeue_by_pid(bqueue, rpcb->pid);
						enqueue(rpcb, tqueue);
						rpcb->number_of_queued++;
					}
				}
				rpcb = rpcb->next;
			}

			returnToMainThread = 0;
			returnToAdmission = 0;
		}
	} else {
		printf("Ready queue is empty");
	}
}

void *main_thread(void *pid) {
	PCB* pcb;
	int* a = (int *) pid;
	pcb = rqueue->head;
	while (pcb != NULL) {
		if (a == pcb->pid) {
			int runtime = pcb->runtime;
			int runningTime = runtime - QUANTUM_TIME;
			if (runningTime > 0) {
				pthread_mutex_lock(&init_mutex);
				usleep(QUANTUM_TIME);
				pthread_mutex_unlock(&init_mutex);
				pcb->runtime = runningTime;
				pcb->run_control_flag = SUSPENDED;
				break;
			} else if (pcb->IOFlag == 1) {
				pthread_mutex_lock(&init_mutex);
				usleep(QUANTUM_TIME);
				pthread_mutex_unlock(&init_mutex);
				pcb->run_control_flag = BLOCKED;
				break;
			} else {
				miniProcessControl = 1;
				pthread_mutex_lock(&init_mutex);
				usleep(runtime); //quantum time dan küçük ise kendi runtime ı kadar mutexte kalır.
				pthread_mutex_unlock(&init_mutex);
				pcb->finished_flag = 1;
				pcb->run_control_flag = TERMINATED;
				break;
			}
		}

		else {
			pcb = pcb->next;

		}

	}
	returnToMainThread = 1;
}

/*
 After all the threads are finished. it should write the stats for each process in a output file.
 */
void output_stats(QUEUE* rqueue, QUEUE* tqueue, QUEUE* pqueue) {

	FILE *f = fopen("system.txt", "w");
	if (f == NULL) {
		printf("Error opening file!\n");
		exit(1);
	}

	PCB* ppcb;
	ppcb = pqueue->head;

	while (ppcb != NULL) {
		fprintf(f, " **** Process queue *** \n");
		fprintf(f,
				" Process id		%d \n Process runtime	%d\n Process priority	%d\n Process cputime	%d\n number of preempt  %d\n number of queued	%d\n IO process flag	%d\n Run control flag	%d\n Finished flag	%d\n",
				ppcb->pid, ppcb->runtime, ppcb->priority, ppcb->cputime,
				ppcb->number_of_preempt, ppcb->number_of_queued, ppcb->IOFlag,
				ppcb->run_control_flag, ppcb->finished_flag);
		ppcb = ppcb->next;
	}

	PCB* rpcb;
	rpcb = rqueue->head;

	while (rpcb != NULL) {
		fprintf(f, " **** Ready queue *** \n");
		fprintf(f,
				" Process id		%d \n Process runtime	%d\n Process priority	%d\n Process cputime	%d\n number of preempt  %d\n number of queued	%d\n IO process flag	%d\n Run control flag	%d\n Finished flag	%d\n",
				rpcb->pid, rpcb->runtime, rpcb->priority, rpcb->cputime,
				rpcb->number_of_preempt, rpcb->number_of_queued, rpcb->IOFlag,
				rpcb->run_control_flag, rpcb->finished_flag);
		rpcb = rpcb->next;
	}

	PCB* tpcb;
	tpcb = tqueue->head;
	while (tpcb != NULL) {
		fprintf(f, " **** Terminated queue **** \n");
		fprintf(f,
				" Process id	%d \n Process runtime	%d\n Process priority	%d\n Process cputime	%d\n number of preempt  %d\n number of queued	%d\n IO process flag	%d\n Run control flag	%d\n Finished flag	%d\n",
				tpcb->pid, tpcb->runtime, tpcb->priority, tpcb->cputime,
				tpcb->number_of_preempt, tpcb->number_of_queued, tpcb->IOFlag,
				tpcb->run_control_flag, tpcb->finished_flag);
		tpcb = tpcb->next;
	}

}

void* create_queue(void) {

	QUEUE *queue_created;
	queue_created = (QUEUE*) malloc(sizeof(QUEUE));
	if (queue_created) {
		queue_created->head = NULL;
		queue_created->count = 0;
		queue_created->work_schema = 0;
	}
	return queue_created;

} // create a queue

void* enqueue(PCB *n, QUEUE* h) {

	PCB *temp;
	if (h->count == 0) {
		h->head = n;
		n->next = NULL;
		n->previous = NULL;
		h->count++;
		return 1;
	} else {
		temp = h->head;
		while (temp->next != NULL) {
			temp = temp->next;
		}

		if (temp == h->head) {
			temp->next = n;
			n->next = NULL;
			n->previous = h->head;
			h->count++;
			return 1;
		} else {
			temp->next = n;
			n->next = NULL;
			n->previous = temp;
			h->count++;
			return 1;
		}
	}
	return 0;

} //enqueue a process into a queue

int findmax(QUEUE* r) {
	PCB *n;
	n = r->head;
	int max = 0;
	while (n != NULL) {
		if (n->priority > max) {
			max = n->priority;
		}
		n = n->next;
	}
	return max;
}

int dequeue_by_pid(QUEUE* h, int n) {
	PCB *temp;
	temp = h->head;
	while (temp != NULL) {
		if (temp->pid == n) {
			break;
		}
		temp = temp->next;
	}
	if (temp == h->head || h->count == 1) {
		if (h->count == 1) {
			h->head = NULL;
			h->count--;
		} else {
			h->head = h->head->next;
			h->head->previous = NULL;
			h->count--;
		}
	} else if (temp->previous == h->head && temp->next == NULL) {
		h->head->next = NULL;
		h->count--;
	} else if (temp->previous != h->head && temp->next == NULL) {
		temp->previous->next = NULL;
		h->count--;
	} else {
		temp->previous->next = temp->next;
		temp->next->previous = temp->previous;
		h->count--;

	}
	return 1;
}

int checkEquality(QUEUE* queue) {
	PCB * rpcb;
	rpcb = queue->head;
	while (rpcb->next != NULL) {
		if (rpcb->cputime == rpcb->next->cputime) {
			if (rpcb->priority > rpcb->priority) {
				return rpcb->pid;
			} else {
				return rpcb->next->pid;
			}

		} else {
			return 0;
		}
		rpcb = rpcb->next;
	}
	return 0;
}

int randm(int ma, int mi) {
	return rand() % ma + mi;
}

int t = 0;
int get_time(int a) {
	if (a == 0) {
		int t = 0;
	}
	while (1) {
		usleep(5000);    // 1/8 sec
		return t++;

	}
}

int main() {
	rqueue = create_queue();
	tqueue = create_queue();
	bqueue = create_queue();
	pqueue = create_queue();
	squeue = create_queue();
	fillProcessQueue();
	int run = 1;
	int counter;
	char szKey[] = "kalk\n";
	char szInput[80];
	while (run) {
		printf("%d\n", get_time(1));
		if (miniProcessControl == 0) {

			usleep(5000);
			sched();
		} else {
			sched();
		}
		output_stats(rqueue, tqueue, pqueue);

		if (rqueue->head == NULL && pqueue->head == NULL) {
			counter++;
			if (counter >= 15) {
				run = 0;

				do {
					printf("Schedular şimdilik uyuyor.\n");
					printf(
							"Schedular ile ilgili log file  system.txt adında work spacesinizde bulunmaktadır.\n");
					printf(
							"Programı daha ağır takip etmek için 429. ve 408. satırdaki usleep fonksiyonunun paramatrelerine 0 ekleyiniz.\n");
					printf(
							"Uyandırmak için process ya da ready queue' yu doldurup 'kalk' yazın.\n");

					fgets(szInput, 80, stdin);
				} while (strcmp(szKey, szInput) != 0);
				run = 1;
				sched();
			}

		} else if (rqueue->head != NULL && get_time(1) > 550) {
			get_time(0);
		}
	}
//start initializations and run scheduler.
	return 0;
}
