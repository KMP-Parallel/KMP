/*
 ECE 5720 KMP parallel
 Feng Qi, fq26
 Ying Zong, yz887
 mpicc -std=gnu11 -o out kmp_MPI.c
 mpirun -mca plm_rsh_no_tree_spawn 1 --hostfile hostfile -np 3 ./out
 */
#include "mpi.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h> 

#define master 0
#define MAX(a,b) ((a) > (b) ? a : b)

int* kmp(char* target, char* pattern, int* table, int myrank);
int* preKMP(char* pattern, int len);
void print(char* arr, int id, int len);
void fillup(char** matrix, int rankid, char* msg, int len, int offset);
void freeDouble(char** matrix, int nproc);
void printMatrix(char **matrix, int nproc, int id, int length);
int* getRealIdx(int* answer, int len, int myrank, int x, int y, int* length);
void pinpoint(int* result, int longlen, int* msg, int shortlen);


void pinpoint(int* result, int longlen, int* msg, int shortlen){
	int j = 0;
	for(j=0; j<shortlen; j++){
		if(j > 0 && msg[j] == 0)
			break;
		else{
			result[msg[j]] = 1;
		}
	}
}

// get the exact idx(es) in each process
// return back an int pointer
int* getRealIdx(int* answer, int len, int myrank, int x, int y, int* length){
	// int package[len];
	int* package = (int *) calloc(len, sizeof(int));
	int index;
	int real_len = 0;
	int i;
	for(i=0;i < len; i++){
		if(i > 0 && answer[i] == 0)
			break;
		else{
			if(myrank == 0){
				index =  answer[i];
				package[real_len++] = index;
				printf("This is REAL index: %d\n", index);
			}else{
				index = x * myrank - y + answer[i];
				package[real_len++] = index;
				printf("This is REAL index: %d\n", index);
			}
			
		}
	}
	int* result = (int*)malloc((real_len) * sizeof(int));

	for(i =0 ; i<real_len; i++){
		if(i > 0 && package[i] == 0)
			break;
		else{
			result[i] = package[i];
		}
	}
	*length = real_len;
	free(package);
	return result;
}

void printMatrix(char **matrix, int nproc, int id, int length){
	printf("This matrix at %d row.\n", id);
	int i ,j;
	for(i=0 ;i < nproc; i++){
		if(i == id){
			for(j=0;j<length;j++){
				printf("%c", matrix[i][j]);
				break;
			}
			printf("\n");
		}
	}
	printf("\n");
}


// free two-pointer matrix
void freeDouble(char** matrix, int nproc){
	for (int i=0; i<nproc; ++i) {
		free(matrix[i]);
	}
	free(matrix);
}


void fillup(char** matrix, int rankid, char* msg, int len, int offset){
	int i;
	for(i=0;i<len;i++)
		matrix[rankid][offset + i] = msg[i];
}

// print out one pointer list
void print(char* arr, int id, int len){
	int j;
	printf("This is rank %d\n", id);
	for(j=0; j<len; j++){
		printf("%c", arr[j]);
	}
	printf("\n");
}

// key func kmp
int* kmp(char* target, char* pattern, int* table, int myrank){
	// printf("This is myrank %d.\n", myrank);
	int n = strlen(target);
	int m = strlen(pattern);
	int* answer = (int*) calloc(n-m+1,sizeof(int));
	int j=0;
	int i=0;
	int index = 0;
	while(i<n){
		if(pattern[j]== target[i])
		{
			j++;
			i++;
		}
		if(j == m){
			// printf("this is matching %d.\n", i-j);
			answer[index++] = i-j;
			j = table[j-1];

		}else if(i < n && pattern[j] != target[i]){
			if(j!=0)
				j = table[j-1];
			else
				i++;
		}
	}
	return answer;
}


int main(int argc, char** argv){
	char* target = "AABAABAABAAAABAABAABAA";
	char* pattern = "ABA";
	int n = strlen(target);
	int m = strlen(pattern);

	int tag = 1;
	int tag2 = 2;
	int* kmp_table = preKMP(pattern, m);

	int myrank, nproc;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Comm_size(MPI_COMM_WORLD, &nproc);
	MPI_Status status;

	// prerequisite
	// nproc <= len(target)/(2 * len(pattern))
	int start_pos = n/nproc;
	int str_per_proc = n/nproc;
	int end = str_per_proc + n % nproc;
	int i;

	char** matrix = (char**)malloc(nproc*sizeof(char*));
	for(i=0;i<nproc;i++){
		matrix[i] = (char*)malloc((end+m-1) * sizeof(char));
	}

    char send_msg[MAX(m-1, end)];
	char recv_msg[MAX(m-1, end)];
    char end_msg[MAX(m-1, end)];

	if(myrank == master){
		for(i=1;i< nproc;i++){
			if(i == nproc-1){
				strncpy(send_msg, target + i*start_pos, end);	
				MPI_Send(send_msg, end, MPI_CHAR, i, tag, MPI_COMM_WORLD);
			}else{
				strncpy(send_msg, target + i*start_pos, str_per_proc);
				MPI_Send(send_msg, str_per_proc, MPI_CHAR, i, tag, MPI_COMM_WORLD);
			}
		}
		strncpy(send_msg, target + master * start_pos, str_per_proc);
		fillup(matrix, myrank, send_msg, str_per_proc, 0);

	// The last process
	}else if(myrank == nproc-1){
		MPI_Recv(recv_msg, end, MPI_CHAR, master, tag, MPI_COMM_WORLD, &status);
		fillup(matrix, myrank, recv_msg, end, m-1);
	}else{
		MPI_Recv(recv_msg, str_per_proc, MPI_CHAR, master, tag, MPI_COMM_WORLD, &status);	
		fillup(matrix, myrank, recv_msg, str_per_proc, m-1);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if(myrank == nproc-1){
		MPI_Recv(end_msg, m-1, MPI_CHAR, myrank-1, tag, MPI_COMM_WORLD, &status);

		fillup(matrix, myrank, end_msg, m-1, 0);
		int* answer = kmp(matrix[myrank], pattern, kmp_table, myrank);
		int len;
		int* result = getRealIdx(answer, end-m+1, myrank, str_per_proc, m-1, &len);
		free(answer);

		MPI_Send(result, len, MPI_INT, master, tag2, MPI_COMM_WORLD);
		free(result);
	}else{
		strncpy(send_msg, target + str_per_proc-m + 1 + myrank * str_per_proc, m-1);
		MPI_Send(send_msg, m-1, MPI_CHAR, myrank+1, tag, MPI_COMM_WORLD);

		// the master process does not to recv
		if(myrank!=master){
			MPI_Recv(end_msg, m-1, MPI_CHAR, myrank-1, tag, MPI_COMM_WORLD, &status);
			fillup(matrix, myrank, end_msg, m-1, 0);
		}
		// implement kmp to get the matching result
		int* answer = kmp(matrix[myrank], pattern, kmp_table, myrank);
		int len;

		int* result = getRealIdx(answer, end-m+1, myrank, str_per_proc, m-1, &len);
		free(answer);

		// master process does not need to send
		if(myrank!=master){
			MPI_Send(result, len, MPI_INT, master, tag2, MPI_COMM_WORLD);
		}
		free(result);

		// merge all the processess's result and set the matching bit to 1 -- pinpoint() func
		if(myrank == master){
			int final_result[n];
			int recv[end+m-1];
			int j;
			// Recv all other processes msg
			for(i = 1; i <nproc; i++){
				MPI_Recv(recv, end+m-1, MPI_INT, i, tag2, MPI_COMM_WORLD, &status);
				pinpoint(final_result,n, recv, end+m-1);
			}

			//print out the check whether it is correct
			for(j=0;j<n;j++){
				if(final_result[j] == 1)
					printf("this is index is : %d\n", j);
			}
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	freeDouble(matrix, nproc);
	MPI_Finalize();

	return 0;
}

// KMP match table
// public to every process
int* preKMP(char* pattern, int len){
	int k = 0;
	int i = 1;
	int* table = (int*)malloc(len * sizeof(int));
	table[0] = k;
	for(i=1;i<len;i++){
		while(k > 0 && pattern[i-1] != pattern[i]){
			table[i] = k-1;
			k = table[i];
		}
		if(pattern[i] == pattern[k])
			k++;
		table[i] = k;
	}
	return table;
}