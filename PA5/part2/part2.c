/****************************************************************
 *
 * Author: Gabriel Clark
 * Title: Memory assignment
 * Date: May 5
 * Description: (Hopefully helpful pseudocode) Template for PA5
 *
 ****************************************************************/


/******************************************************
 * Declarations
 ******************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// #Define'd sizes
#define TLB_SIZE 16
#define PAGE_SIZE 256
#define PAGE_TABLE_SIZE 256
#define FRAME_SIZE 256
#define FRAMES_IN_MEMORY 128
#define NOT_FOUND 2147483647

// Is TLB index occupied
bool TLB_index_occupied[TLB_SIZE];
// Make the TLB array
// Need pages associated with frames (could be 2D array, or C++ list, etc.)
int TLB_array[TLB_SIZE][2];
int TLB_index = 0;

// Is page table index occupied
bool page_table_occupied[PAGE_TABLE_SIZE];
// Make the Page Table
// Again, need pages associated with frames (could be 2D array, or C++ list, etc.)
int page_table[PAGE_TABLE_SIZE];

// Is a memory address occupied
bool memory_frame_occupied[FRAMES_IN_MEMORY];
// Make the memory
// Memory array (easiest to have a 2D array of size x frame_size)
char memory_array[FRAMES_IN_MEMORY][FRAME_SIZE];

// FIFO page replacement
int last_page_index = 0;

// LRU page replacement
// index 0 is the head of the queue (element to get next) and index n is the last element
int lru_queue[FRAMES_IN_MEMORY];
int next_free_frame = 0;

char* mode;
/******************************************************
 * Function Declarations
 ******************************************************/

// inclusive_stop will be overwritten by inclusive inclusive_stop-1
// there will be a blank spot in the array where inclusive_start is
void shift_array_range_down_one(int array[], int inclusive_start, int inclusive_stop){
	for (int i=inclusive_stop; i>inclusive_start; i--) {
		array[i] = array[i-1];
	}
}

// LRU functions
void update_lru_queue(int page_accessed){
	int page_position_in_queue = -1;
	for (int i=0; i<FRAMES_IN_MEMORY; i++) {
		if (lru_queue[i] == page_accessed) {
			page_position_in_queue = i;
		}
	}
	if (page_position_in_queue == -1) { // case: element not in queue
		shift_array_range_down_one(lru_queue, 0, FRAMES_IN_MEMORY-1);
		lru_queue[0] = page_accessed;
	}else {  // case: middle element
		shift_array_range_down_one(lru_queue, 0, page_position_in_queue);
		lru_queue[0] = page_accessed;
	}
}

/***********************************************************
 * Function: get_page_and_offset - get the page and offset from the logical address
 * Parameters: logical_address
 *   page_num - where to store the page number
 *   offset - where to store the offset
 * Return Value: none
 ***********************************************************/
void get_page_and_offset(int logical_address, int *page_num, int *offset){
	const int bit_mask = 0x0000FFFF; // 1s in first 16 bits
	int page_size = PAGE_SIZE; // 2^8 bites
	int masked_address = bit_mask & logical_address;
	*page_num = logical_address/page_size;
	*offset = logical_address%page_size;
}

/***********************************************************
 * Function: get_frame_TLB - tries to find the frame number in the TLB
 * Parameters: page_num
 * Return Value: the frame number, else NOT_FOUND if not found
 ***********************************************************/
int get_frame_TLB(int page_num){
	for (int i=0; i<TLB_SIZE; i++) {
		if (TLB_index_occupied[i] && TLB_array[i][0] == page_num) {
			return TLB_array[i][1];
		}
	}
	return NOT_FOUND;
}

/***********************************************************
 * Function: get_available_frame - get a valid frame
 * Parameters: none
 * Return Value: frame number
 ***********************************************************/
int get_available_frame(){
	//mode = "fifo";
	if (strcmp(mode, "fifo") == 0) {
		int value_to_return = last_page_index;
		if (last_page_index+1 == FRAMES_IN_MEMORY) {
			last_page_index = 0;
		}else {
			last_page_index += 1;
		}
		for (int i=0; i<PAGE_TABLE_SIZE; i++) {
			if (page_table[i]==value_to_return) {
				page_table_occupied[i] = false;
			}
		}
		memory_frame_occupied[value_to_return] = 1;
		return value_to_return;
	}else if (strcmp(mode, "lru") == 0) {
		int end_of_queue = FRAMES_IN_MEMORY-1;
		while(lru_queue[end_of_queue]==-1){
			end_of_queue--;
		}
		if (end_of_queue+1<FRAMES_IN_MEMORY && lru_queue[end_of_queue+1] == -1) {
			return next_free_frame++;
		}else {
			for (int i=0; i<FRAMES_IN_MEMORY; i++) {
				if (lru_queue[end_of_queue]==i) {
					return i;
				}
			}
			printf("DANGER page's frame not found");
		}
	}else {
		printf("Mode not correctly set:");
		printf("%s\n",mode);
	}
}

/***********************************************************
 * Function: get_frame_pagetable - tries to find the frame in the page table
 * Parameters: page_num
 * Return Value: page number, else NOT_FOUND if not found (page fault)
 ***********************************************************/
int get_frame_pagetable(int page_num){
	if (page_table_occupied[page_num]) {
		return page_table[page_num];
	}else {
		return NOT_FOUND;
	}
}

/***********************************************************
 * Function: backing_store_to_memory - finds the page in the backing store and
 *   puts it in memory
 * Parameters: page_num - the page number (used to find the page)
 *   frame_num - the frame number for storing in physical memory
 * Return Value: none
 ***********************************************************/
void backing_store_to_memory(int page_num, int frame_num, FILE *fname) {
	char temp_page[PAGE_SIZE];
	fseek(fname, page_num*PAGE_SIZE, SEEK_SET);
	fread(temp_page, PAGE_SIZE, 1, fname);
	memcpy(memory_array[frame_num], temp_page, sizeof(temp_page));
}

/***********************************************************
 * Function: update_page_table - update the page table with frame info
 * Parameters: page_num, frame_num
 * Return Value: none
 ***********************************************************/
void update_page_table(int page_num, int frame_num);

/***********************************************************
 * Function: update_TLB - update TLB (FIFO)
 * Parameters: page_num, frame_num
 * Return Value: none
 ***********************************************************/
void update_TLB(int page_num, int frame_num){
	int current_TLB_index = TLB_index;
	if (TLB_index+1 == TLB_SIZE) {
		TLB_index = 0;
	}else {
		TLB_index += 1;
	}
	TLB_array[current_TLB_index][0] = page_num;
	TLB_array[current_TLB_index][1] = frame_num;
	TLB_index_occupied[current_TLB_index] = 1;
}


/******************************************************
 * Assumptions:
 *   If you want your solution to match follow these assumptions
 *   1. In Part 1 it is assumed memory is large enough to accommodate
 *      all frames -> no need for frame replacement
 *   2. Part 1 solution uses FIFO for TLB updates
 *   3. In the solution binaries it is assumed a starting point at frame 0,
 *      subsequently, assign frames sequentially
 *   4. In Part 2 you should use 128 frames in physical memory
 ******************************************************/

int main(int argc, char * argv[]) {
	memset(page_table, 0, sizeof(page_table));
	memset(memory_array, 0, sizeof(memory_array));
	memset(page_table_occupied, 0, sizeof(page_table_occupied));
	memset(memory_frame_occupied, 0, sizeof(memory_frame_occupied));

	// argument processing
	// ./part1 BACKING_STORE.bin addresses.txt
	char* backing_store_location = malloc(51 * sizeof(char));
	char* addresses_file_location = malloc(51 * sizeof(char));
	mode = malloc(51 * sizeof(char));
	if (argc!=4) {
		printf("Please enter correct number of arguments\n");
	}else {
		backing_store_location = argv[1];
		addresses_file_location = argv[2];
		mode = argv[3]; // "fifo" or "lru"
	}
	FILE *backing_store;
	backing_store = fopen(backing_store_location, "rb");
	
	// Init lru_queue
	for (int i=0; i<FRAMES_IN_MEMORY; i++) {
		lru_queue[i] = -1;
	}
	
	FILE* output_file;
	output_file = fopen("./correct.txt", "w");

	// initialization
	FILE * addresses_fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t line_len;

	addresses_fp = fopen(addresses_file_location, "r");

	int number_of_translated_addresses = 0;
	int number_of_faults = 0;
	int number_of_TLB_hits = 0;
	// read addresses.txt
	while((getline(&line, &len, addresses_fp)) != -1) {
		// pull addresses out of the file
		int unmasked_address = atoi(line);

		// get page number and offset
		int page;
		int offset;
		int frame;
		get_page_and_offset(unmasked_address, &page, &offset);
		
		frame = get_frame_TLB(page);
		if (frame == NOT_FOUND) {
			frame = get_frame_pagetable(page);
			if (frame == NOT_FOUND) { // page fault
				frame = get_available_frame();
				if (frame == NOT_FOUND) {
				}
				backing_store_to_memory(page, frame, backing_store);
				page_table[page] = frame;
				page_table_occupied[page] = true;
				number_of_faults += 1;
			}
			update_TLB(page, frame);
		}else {
			number_of_TLB_hits += 1;
		}

		update_lru_queue(page);
		//printf("page %d frame %d\n", page, frame);
		if (frame > FRAMES_IN_MEMORY || offset > FRAME_SIZE) {
			printf("%d %d \n", frame, offset);
			printf("DANGER\n");
		}
		fprintf(output_file, "Virtual address: %d Physical address: %d Value: %d\n", (page*PAGE_SIZE)+offset, (frame*FRAME_SIZE)+offset, memory_array[frame][offset]);
		//printf("Virtual address: %d Physical address: %d Value: %d\n", (page*PAGE_SIZE)+offset, (frame*FRAME_SIZE)+offset, memory_array[frame][offset]);
		number_of_translated_addresses += 1;
	}

	// output useful information for grading purposes
	
	fprintf(output_file, "Number of Translated Addresses = %d\n", number_of_translated_addresses);
	fprintf(output_file, "Page Faults = %d\n", number_of_faults);
	fprintf(output_file, "Page Fault Rate = %.3f\n", (float)number_of_faults/(float)number_of_translated_addresses);

	fprintf(output_file, "TLB Hits = %d\n", number_of_TLB_hits);
	fprintf(output_file, "TLB Hit Rate = %.3f\n", (float)number_of_TLB_hits/(float)number_of_translated_addresses);

	// close program
	fclose(addresses_fp);
	fclose(output_file);
	if (line)
		free(line);
	/*
	free(backing_store_location);
	free(addresses_file_location);
	*/
	exit(EXIT_SUCCESS);
}
