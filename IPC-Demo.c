/*
IPC-Demo.c
Finds median of 25 integers contained in 5 seperate text files
Through the creation of 5 child processes controlled by parent
Written by Benjamin Jollymore A00400128
Latest version as of 2018-10-06
Run on Ubuntu 18.04
**/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


//Define READ and WRITE end of pipe for readability
#define READ 0
#define WRITE 1

//Define command codes
#define REQUEST 100
#define PIVOT 200
#define LARGE 300
#define SMALL 400
#define READY 500
#define KILL 600

//Define number of child processes to spawn
#define NUM_CHILDREN 5

//used to keep track of number of spawned processes at a given point in execution
int childCounter = 0;

//define arrays to be converted to pipes
int childToParent[NUM_CHILDREN][2];
int parentToChild[NUM_CHILDREN][2];

//process control value, nonzero initialization value
int control = -1;

//Helper function to initalize pipes. Assigns a read and write pipe
//to each child process ID. Child 1 will use childToParent[1] and
//parentToChild[1], and so on.
void intializePipes() {
	for (int i = 1; i < NUM_CHILDREN + 1; i++) {
		pipe(childToParent[i]);
		pipe(parentToChild[i]);
	}
}

//Helper function to read from specified file with id fileID, an array
//of integers into an external array, numArray
void readFileToArray(char fileID, int numArray[]) {

	//Change generic file name to name of specific file to be accessed
	char fileName[] = "input_i.txt";
	fileName[6] = fileID;

	//Access the file
	FILE *inputFile;
	inputFile = fopen(fileName, "r");

	//ensure file is accessable
	if (inputFile == NULL) {
		printf("Error Opening File: %s, Program terminating on exit code 1.\n", fileName);
		exit(1);
	}

	//Send file content to array
	if (fscanf(inputFile, "%d %d %d %d %d",
	       &numArray[0], &numArray[1], &numArray[2], &numArray[3], &numArray[4])<0){
		printf("Error Reading File: %s, Program terminating on exit code 1.\n", fileName);
		exit(1);
	}

	fclose(inputFile);
}

//Helper function to determine if an external array numArray is empty.
//If value at every index is found to be -1, array is deemed to be empty.
int isEmpty(int numArray[]) {
	int empty = 1;
	for (int i = 0; i < 5; i++) {
		if (numArray[i] != -1) {
			empty = 0;
			break;
		}
	}
	return empty;
}

//Each child process executes this code block before termination.
void executeChildProcess() {

	//Child process is alive and will execute
	int alive = 1;

	//Unique process ID retrieved from parent to child pipe, nonzero initalization value
	int id = -1;

	//Array of numbers to be sorted
	int numberArray[5];

	//Declare pivot value
	int pivotValue;

	//Recieve unique ID from parent
	close(parentToChild[childCounter][WRITE]);
	read(parentToChild[childCounter][READ], &id, sizeof(int));

	//Now that the child has an ID, it can open it's respective file
	readFileToArray(id + '0', numberArray);

	//send READY to parent
	control = READY;
	close(childToParent[id][READ]);
	write(childToParent[id][WRITE], &control, sizeof(int));

	while (alive) {

		//Recieve control statement from parent
		close(parentToChild[id][WRITE]);
		read(parentToChild[id][READ], &control, sizeof(int));

		switch (control) {
		case REQUEST: {

			int response;

			//ensure array is not empty, if empty, send -1 to parent
			if (isEmpty(numberArray)) {
				response = -1;
			}

			//Choose random element from array to send to parent
			else {
				int randomIndex;

				//ensure a valid entry is selected
				do {
					randomIndex = rand() % (5 - 1) + 0;
					response = numberArray[randomIndex];
				} while (response == -1 );

				printf("Child %d sends %d to parent\n", id, response);
			}

			close(childToParent[id][READ]);
			write(childToParent[id][WRITE], &response, sizeof(int));

			//end REQUEST control block
			break;
		}

		case PIVOT: {

			//initialize uanity of numbers greater than pivot value in the array
			int largerValues = 0;

			//Recieve pivot value from parent
			close(parentToChild[id][WRITE]);
			read(parentToChild[id][READ], &pivotValue, sizeof(int));

			//find number of valid entries in array larger than pivot value
			for (int i = 0; i < 5; i++) {
				if (numberArray[i] > pivotValue) {
					largerValues++;
				}
			}

			//send quantity of larger values back to parent.
			printf("Child %d recieves pivot and replies %d\n", id, largerValues);
			close(childToParent[id][READ]);
			write(childToParent[id][WRITE], &largerValues, sizeof(int));

			//end PIVOT control block
			break;
		}

		case LARGE:
		{
		
			//drop all values greater than pivot value
			for (int i = 0; i < 5; i++) {
				if (numberArray[i] > pivotValue) {
					numberArray[i] = -1;
				}
			}

			//end LARGE control block
			break;
		}

		case SMALL:
		{

			//Drop all values less than pivot value, if they are valid entries
			for (int i = 0; i < 5; i++) {
				if (numberArray[i] < pivotValue && numberArray[i] != -1) {
					numberArray[i] = -1;
				}
			}

			//end SMALL control block
			break;
		}
		case KILL:

			//end while loop and signify termination
			alive = 0;
			printf("Child %d terminating.\n", id);
			exit(0);

			//end KILL control block
			break;
		}

	}
}

//Parent process executes this code block before termination
void executeParentProcess() {

	//Send IDs to through respective parent to child pipe
	for (int i = 1; i < NUM_CHILDREN + 1; i++) {
		close(parentToChild[i][READ]);
		write(parentToChild[i][WRITE], &i, sizeof(int));
	}

	//Check all processes have sent READY, if not, terminate.
	for (int i = 1; i < NUM_CHILDREN + 1; i++) {
		close(childToParent[i][WRITE]);
		read(childToParent[i][READ], &control, sizeof(int));

		if (control == READY) {
			printf("Child %d sends READY\n", i);
		}

		else {
			printf("Unexpected response from child %d. Program terminating on exit code 1\n", i);
			exit(1);
		}
	}

	//All is well; proceed
	printf("Parent is READY\n");

	//initialize K = n/2 for median finding
	int kVal = 25 / 2;

	//initialize address to place REQUEST value recieved from child process
	int recievedVal = -1;

	//intialize array of m values
	int mValues[5];

	//Boolean to exit loop once median value found
	int medianFound = 0;

	//loop median finding algorithm while median not found
	while (!medianFound) {
		//Pick random child ID and send REQUEST
		do {
			int childID = rand() % (6 - 1) + 1;
			control = REQUEST;

			close(parentToChild[childID][READ]);
			write(parentToChild[childID][WRITE], &control, sizeof(int));

			//Recieve integer from selected child
			close(childToParent[childID][WRITE]);
			read(childToParent[childID][READ], &recievedVal, sizeof(int));


			if (recievedVal != -1) {
				printf("Parent sends REQEUST to Child %d\n", childID);
			}

			//ensure doesn't select child with empty array, which would return -1
		} while (recievedVal == -1);

		//Begin pivot broadcast
		control = PIVOT;
		printf("Parent broadcasting %d as pivot to all children\n", recievedVal);

		for (int i = 1; i < NUM_CHILDREN + 1; i++) {

			//Send PIVOT control to each child
			close(parentToChild[i][READ]);
			write(parentToChild[i][WRITE], &control, sizeof(int));

			//Send pivot value to each child
			close(parentToChild[i][READ]);
			write(parentToChild[i][WRITE], &recievedVal, sizeof(int));

			//Retrieve values from children and place them into mValues array for processing
			close(childToParent[i][WRITE]);
			read(childToParent[i][READ], &mValues[i - 1], sizeof(int));
		}

		//sum value m from array of generic m values
		int mVal = (mValues[0] + mValues[1] + mValues[2] + mValues[3] + mValues[4]);

		//if m is equal to value k, median has been found
		if (mVal == kVal) {
			printf("Parent: m = %d + %d + %d + %d + %d = %d. %d = 25/2, Median found!\n",
			       mValues[0], mValues[1], mValues[2], mValues[3], mValues[4], mVal, recievedVal);

			//exit while loop, median has been found. Terminate all children.
			medianFound = 1;
			control = KILL;
		}

		//M value is higher than K value, drop all values smaller than pivot
		else if (mVal > kVal) {
			control = SMALL;
			printf("Parent: m = %d + %d + %d + %d + %d = %d. %d != 25/2\n",
			       mValues[0], mValues[1], mValues[2], mValues[3], mValues[4], mVal, recievedVal);
			printf("Median not found. Dropping values smaller than pivot and trying again.\n\n");
		}

		//M value is lower than K value, drop all values larger than pivot and reassign k value
		else {
			control = LARGE;
			kVal -= mVal;
			printf("Parent: m = %d + %d + %d + %d + %d = %d. %d != 25/2\n",
			       mValues[0], mValues[1], mValues[2], mValues[3], mValues[4], mVal, recievedVal);
			printf("Median not found. Dropping values larger than pivot and trying again.\n\n");
		}

		//send control statement to each child.
		for (int i = 1; i < NUM_CHILDREN + 1; i++) {
			close(parentToChild[i][READ]);
			write(parentToChild[i][WRITE], &control, sizeof(int));
		}
	}
}

int main() {

	//Set random seed for random selection later on
	srand(time(NULL));

	//Process ID for child, nonzero initialization value
	int child = -1;

	//initialize each pipe prior to fork() calls
	intializePipes();

	//Starts all child processes and assigns independent ID to each child
	//by passing ID into child process function.
	for (int i = 1; i < NUM_CHILDREN + 1; i++) {

		//Spawn new child process with ID i, increment counter of living children
		childCounter++;
		child = fork();

		//If in child, continue
		if (child == 0) {
			executeChildProcess();
			//Exit for loop after each fork to not generate more than intended number of processes
			break;
		}

		//Check process spawned correctly
		else if (child < 0) {
			printf("ERROR: Unsuccessful fork by child %d. Program terminating on exit code 1.\n", i);
			return 1;
		}
	}

	//Continue through Parent Process
	if (child != 0) {
		executeParentProcess();
	}

	//terminate program
	return 0;
}
