///////////////////////////////////////////////////////////////////////////////
//
//  File          : pr1.c 
//  Description   : This file takes an input file, opens 2 pipes, and sends the input
//		    		file data to a  2end process. The 2end process reads from the first pipe
//	           		and writes to the second pipe. Finally the third pipe reads from the second
//		   			pipe and writes to the specified output file. Arguement inputs are described in parse_args()
//
//  Author        : Gabe Harms 
//  Last Modified : 01/22/14 
//

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>

// Defines
#define ARGUEMENTS "i:o:m:" 		//Allowable arguements to the program
#define MAX_KEY_NUM 25			//Maximum number of searchable strings allowed
#define MAX_SEARCH_SIZE 256		//The maximum number characters that can be searched for
#define BUFFER_SIZE 30000		//Max buffer size


// Debugging
#define DEBUG 1				//Gives more print out info if 1


// Functional Prototypes
void middleman ( int fd );
void write_handle( int fd, char *str, char *file);	
char *read_handle( int fd, char *file );	
void * thread_read ( void * arguements );
void parent_actions( int fd, pid_t child_pid);
void child_two_actions( int fd_in, int fd_out );
void *find_matches ( void * arguements );
int search_string ( char *str, char *key, char *parse_parameter );
void parse_args( int argc, char *argv[]);
void get_file_size();
void err_sys(char *msg);

// Type Definitions
typedef struct { //Used to pass arguements to our thread process
        char key[BUFFER_SIZE];
	char str[BUFFER_SIZE];
} THREAD_ARGS_1;

// Type Definitions
typedef struct { //Used to pass arguements to our thread process
	int fd;
} THREAD_ARGS_2;

// Global data
char *INPUT_FILE= "/dev/null";		//Input file name
char *OUTPUT_FILE = "/dev/null";	//Output file name
char search[MAX_SEARCH_SIZE];		//Holds all keys inputed in one string (key example: -m there; there is one key )
int total_lines = 0;			//Stores total lines in input/output file
sem_t read_lock;
sem_t thread_lock;
char **keys;
int key_count = 0;
char *pipe_read;
pthread_mutex_t pipe_read_lock;



int main(int argc, char *argv[])
{
	printf("Demonstration of pipe() and fork()\n");

	int pipe_one[2];		// pipe endpoints
	pid_t child_pid;

 sem_t *mutex = mmap(NULL, sizeof(mutex), 
      PROT_READ |PROT_WRITE,MAP_SHARED,
      -1, 0);

	sem_init ( mutex, 1, 0 );

	//Parse all arguements 
	parse_args( argc, argv );
	
	

	if (pipe(pipe_one) < 0)  //Case pipe() failed
	{
		err_sys("pipe error");
	}

	if ((child_pid = fork()) < 0)  //Case fork failed()
	{
		err_sys("fork error");
	}
	else if (child_pid > 0)	//This is the parent (P1)
	{

		

		close(pipe_one[0]);
		if ( DEBUG ) { printf ( "P1 Beginning. => %d\n", getpid() ); }		
	
		//Call function to read input file and write
		//to the pipe. Also gets file size			
		parent_actions( pipe_one[1], child_pid);

		sem_post ( mutex );
	}
	else	//This is the child (P2)
	{




		sem_wait ( mutex );
		close(pipe_one[1]);	
	
		if ( DEBUG ) { printf( "P2 Beginning. => %d\n", getpid() ); }
		
		//Handles the 2end process while making
		//a third process. Performs read/writes on both pipes			
		middleman( pipe_one[0]);		
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : middleman
// Description  : This function handles the 2end and 3rd process. Here the 2end
//		  		process creates the 2end pipe, and then forks the third process. 
//		  the second process then reads from the first pipe, and prints 
//		  to the second pipe. The third process then reads the second pipe,
//	          and prints to the output file
//
// Inputs       : argc - num of args
//		  argv - the arguements
//		  fd - the file handle/pipe to read from
// Outputs      : none
//
void middleman ( int fd ) {


	pid_t child_pid;	//Child Process (P3)
	int pipe_two[2];
	
	
	sem_init( &thread_lock, 0, 0 );

	if (pipe(pipe_two) < 0) //Case pipe() failed
	{
		err_sys("pipe error");
	}

	if ((child_pid = fork()) < 0) //Case fork() failed
	{
		err_sys("fork error");
	}
	else if (child_pid > 0)	//This is the parent (P2)
	{
		close( pipe_two[0] );
		
		child_two_actions( fd, pipe_two[1] );
			
		//Wait for P3
		if (waitpid(child_pid, NULL, 0) < 0)	
			{ err_sys("waitpid error"); }
	
	}
	else	//This is the child (P3)
	{	

		if ( DEBUG ) { printf("P3 Beginning. => %d\n", getpid()); }
		close(pipe_two[1]);			
		
		//Read from the second pipe and store it in line	
		char *line = read_handle( pipe_two[0], NULL );	
	
		printf ( "Testing. line = %s\n", line );
		//Write line to the output file	
		write_handle ( 0, line, OUTPUT_FILE );
		
		//Again, line was dynamically allocated by 
		//read_handle, so we will free it up in order to reduce
		//memory leakage
		free ( line );
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : parent_actions
// Description  : First finds the file size, then reads the input file. After reading
//		  the input file, it is printed to the file handle passed to the function.
//		  If the input file is not specified, no bytes are read.
//
// Inputs       : argc - num of args
//		  argv - the arguements
//		  fd - the file handle/pipe to read from
//		  child_pid - allows this function to call wait()
// Outputs      : none
//
void parent_actions( int fd, pid_t child_pid )
{
	char *file_input = NULL;	//Store read result
	
	//Get the file size
	get_file_size();

	if ( strcmp(INPUT_FILE,"/dev/null") != 0 ) //Case input file was specified
	{
		//Read input file
		file_input = read_handle ( 0, INPUT_FILE );		
	}	

	//Write the input file to the pipe
	write_handle( fd, (file_input != NULL) ? file_input : "hello", NULL);	// write to fd[1]
	
	//Since we called read_handle, we have dynamically allocated 
	//memory that needs to be released back to the heap
	free ( file_input );
	
	//Wait for P2
	if (waitpid(child_pid, NULL, 0) < 0)	// wait for child
		{ err_sys("waitpid error"); }	
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : child_two_actions
// Description  : Handles all of 2end process actions. This includes 
//		     reading the first pipe, and writing to the second one
//		     as well as searching the input for key strings
//
// Inputs       : fd - the file handle/pipe to read from
//		  child_pid - allows this function to call wait()
// Outputs      : none
//
void child_two_actions( int fd_in, int fd_out )
{
	pthread_t search_thread;
	
	//Initialize Semaphore in order to keep our global 
	//"total_matches" variable consistent
	sem_init( &read_lock, 0, 1 );
	
	//Create threads pool. One for each key
	pthread_t *thread_pool = (pthread_t*)malloc ( sizeof(pthread_t) * key_count );
		
	//Must dynamically allocate data so that when the string
	//is returned, it doesn't dissapear from the stack
	pipe_read = (char*)calloc( sizeof(char), BUFFER_SIZE );
	
	
	//Read original pipe, and store its contents
	//into the "line" string
	sem_wait ( &read_lock );
	THREAD_ARGS_2 *readArgs = (THREAD_ARGS_2*)malloc( sizeof(THREAD_ARGS_2) );
	readArgs->fd = fd_in;
	pthread_create ( &search_thread, NULL, thread_read, (void*)readArgs );

	//Critical Sections
	sem_wait ( &read_lock );

	//Now that we have our keys, go through the data	
	//and look for the keys dispatching threads so that we 
	//can search in parallel. Each key will be given its
	//own thread in order to maximize throughput
	for (int i =0; i < key_count; i++ ) {
		THREAD_ARGS_1 *args = (THREAD_ARGS_1*)malloc ( sizeof(THREAD_ARGS_1));
		strcpy (args->key, keys[i] );
		strcpy (args->str, pipe_read );
		pthread_create ( &thread_pool[i], NULL, find_matches, (void *)args );	
	}
	
	//Wait for all threads to finish
	for ( int i = 0; i < key_count; i++ )
		pthread_join( thread_pool[i], NULL );

	//Since we are done using our threads, and they were
	//Dynmaically allocated to the "thread_array" variable,
	//we must free it back up
	free ( thread_pool );	
	
	//Find the total amount of lines using our string search function
	total_lines = search_string ( pipe_read, "Find Total Lines", "\n");
	
	
	//Print out output file
	printf( "P3: file %s, lines %d\n", OUTPUT_FILE, (strcmp(OUTPUT_FILE, "/dev/null")) != 0 ? total_lines: 0);

	//Write what was just read, "line", to the
	//second pipe that P2 has created
	write_handle( fd_out, pipe_read, NULL);
	
	sem_post ( &thread_lock );	
	
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : write_handle
// Description  : This function writes a given string to the specified file handle.
//		  It can take either a file or a file descriptor as an input. If a
//		  file descripter wants to be used NULL must be inputed as the file
//
// Inputs       : fd - the file write to if file is NULL
//		  str - the string to write
//		  file - file to write to
// Outputs      : none
//
void write_handle( int fd, char *str, char *file)
{
	FILE *fp;	

	if ( file == NULL ) //Case opening file descriptor
	{
		if ( DEBUG ) { printf("%d: Opening file descriptor: %d\n", getpid(), fd); }
		fp = fdopen(fd, "w");		
	}
	else //Case opening file
	{	
		if ( DEBUG ) { printf("%d: Opening file: %s\n", getpid(), file); }
		//strcmp for dev/lkfd		
		fp = fopen(file, "w");
	}

	if (fp == NULL) //Case file wasn't opened
		 err_sys("fdopen(w) error"); 

	//Setup fp to line-buffering
	static char buffer[BUFFER_SIZE];
	int ret = setvbuf(fp, buffer, _IOLBF, BUFFER_SIZE);
	if (ret != 0)
	  err_sys("setvbuf error (parent)"); 


	if ( DEBUG ) { printf("%d: Writing to file/descriptor\n", getpid()); }
	
	//Writing str to fp
	fprintf(fp, "%s\n", str);
	printf ( "%d: Testing. Wrote: %s ", getpid(), str );

	fclose ( fp );
	// fflush(fp);
}




////////////////////////////////////////////////////////////////////////////////
//
// Function     : read_handle
// Description  : This function reads to the specified file handle
//
// Inputs       : fd - the file read from if file is NULL
//		   str - the string to write
//		   file - file to read from
// Outputs     : line - string data from the file
char * read_handle( int fd, char *file )
{	
	FILE *fp;
	char *str; 			//Holds the read from the file
	char line[BUFFER_SIZE];	//Holds a single line at a time from the file

	if ( file == NULL ) //Case opening file descriptor
	{
		if ( DEBUG ) { printf("%d: Opening file descriptor: %d\n", getpid(), fd); }
		fp = fdopen(fd, "r");
	}		
	else //Case opening file
	{
		if ( DEBUG ) { printf("%d: Opening file: %s\n", getpid(), file); }
		fp = fopen(file, "r");
	}
	
	if (fp == NULL) //Case file was not opened
		 err_sys("fdopen(r) error"); 

	//Setup fp to line-buffering
	static char buffer[BUFFER_SIZE];
	int ret = setvbuf(fp, buffer, _IOLBF, BUFFER_SIZE);
	if (ret != 0)
		err_sys("setvbuf error (child)"); 

	if ( DEBUG ) { printf("%d: Reading from file/descriptor\n", getpid()); }

	//Must dynamically allocate data so that when the string
	//is returned, it doesn't dissapear from the stack
	str = (char*)calloc( sizeof(char), BUFFER_SIZE );

	//Read each line of the file until we reach the bottom
	//Use strcat to add each line to our full string of the file
	while (fgets(line, BUFFER_SIZE, fp) != NULL)	// ends with newline and null_char
		strcat(str, line);
	
	printf("%d: Testing. Read: %s", getpid(), str );
	fclose(fp);
	
	
	return str;
	// fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : thread_read
// Description  : This is the read handle function for our thread created
//		     in the second process
//
// Inputs       : fd - the file read from if file is NULL
// Outputs     : line - string data from the file
void * thread_read ( void * arguements  )
{	
	FILE *fp;
	char line[BUFFER_SIZE];	//Holds a single line at a time from the file

	THREAD_ARGS_2 *args = arguements;

	if ( DEBUG ) { printf("%d: Opening file descriptor: %d\n", getpid(), args->fd); }
	fp = fdopen( args->fd, "r");
		
	if (fp == NULL) //Case file was not opened
		 err_sys("fdopen(r) error"); 

	//Setup fp to line-buffering
	static char buffer[BUFFER_SIZE];
	int ret = setvbuf(fp, buffer, _IOLBF, BUFFER_SIZE);
	if (ret != 0)
		err_sys("setvbuf error (child)"); 

	if ( DEBUG ) { printf("%d: Reading from file/descriptor\n", getpid()); }


		
	//Read each line of the file until we reach the bottom
	//Use strcat to add each line to our full string of the file
	while (fgets(line, BUFFER_SIZE, fp) != NULL)	// ends with newline and null_char
		strcat( pipe_read, line);
	
	sem_post ( &read_lock );
	
	fclose(fp);
	free ( args );
	
	pthread_exit ( 0 );
	// fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : find_matches
// Description  : This is a subfunction of search, and handles all of the 
//		  gritty searching after the search function finds all of the
//		  keys. Searches separately for matches by word, and matches by line
//
// Inputs       : key - array of strings to search for
//		  str - string to search in
//		  key_count - number of keys in the key array
// Outputs      : none
void * find_matches ( void * arguements  )
{
	THREAD_ARGS_1 *args = arguements;

	int word_matches = 0;	        //Match Counter array. Each index corresponds to a specific key
	int line_matches = 0;   	//Line match counter array. Each index corresponds to a specific key
	char copy_of_str[BUFFER_SIZE];

	

	//Create a copy of the data so that we can 
	strcpy ( copy_of_str, args->str);

	word_matches = search_string ( args->str, args->key, " .,-\n" );

	line_matches = search_string ( copy_of_str, args->key, "\n" );

	sem_post ( &read_lock );	

	//Print out results to stdout
	printf ( "P2: String %s, lines %d, matches %d\n", args->key, line_matches, word_matches );
	
	//Since arguements were dynamically allocated, we must free them
	free ( args );
	pthread_exit(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : search_string
// Description  : This function searches an input string for a specific
//		  string key, and returns the matches
//
// Inputs       : str - string to search in
//		  key - string to search for
//		  parse_parameter - how to tokenize the string
// Outputs      : matches - number of matches in the string
int search_string ( char *str, char *key, char *parse_parameter ) 
{
	char *tmp;
	int matches = 0;
	//Use strtok to go through our long string
	//of keys, and break them up into their
	//own individual strings If a key exists in 
	//the part of the string then we increment the 
	//matches variable
	tmp = strtok ( str, parse_parameter );
	while ( tmp != NULL ) 
	{	
		if ( strstr( tmp, key ) != NULL ) //Case match was found
			matches++;
		if ( strcmp ( key, "Find Total Lines" ) == 0 ) //Case we are only finding total number of lines in the file
			matches++;

		tmp = strtok ( NULL, parse_parameter );
	}
	return matches;
}



////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_file_size
// Description  : This function gets the size of the specified input file
//
// Inputs       : None
// Outputs      : none
void get_file_size()
{
	FILE *fp = fopen( INPUT_FILE, "r" );
	
	if ( fp == NULL ) { //Case file cannot be opened
		fprintf( stderr, "File:%s not found\n", INPUT_FILE );		
		err_sys("fopen(r) error");
		system("exit");
	}

	fseek( fp, 0L, SEEK_END );
	printf("P1: file %s, bytes %d\n", INPUT_FILE, (int)ftell(fp) );
	fseek( fp, 0L, SEEK_SET );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : parse_args
// Description  : This function takes all of the inputed arguements
//  		  and picks out relevent information, setting important
//		  global variables
//
// Inputs       : argc - num of args
//		  argv - the arguements
// Outputs      : -1 if failure or 0 if successful
void parse_args ( int argc, char *argv[]) 
{
	int c;
	char **temp;
	
	while ( (c = getopt(argc, argv, ARGUEMENTS)) != -1 )
	{
		switch (c) 
		{
			case 'i':  //Input file
					INPUT_FILE = optarg;
					break;
			case 'o':  //Output file
					OUTPUT_FILE = optarg;
					break;
			case 'm':  //String search
					key_count++;

					temp = keys;
					
					keys = (char **)malloc ( key_count * sizeof(char*) );
					
					for ( int i = 0; i < key_count-1; i++ ) {
						keys[i] = (char *) malloc ( MAX_SEARCH_SIZE * sizeof(char) );
						strcpy ( keys[i], temp[i] );
					}
					
					keys[key_count-1] = malloc ( MAX_SEARCH_SIZE * sizeof ( char ) );
					strcpy ( keys[key_count-1], optarg );
					
					free ( temp );
					
					break;
			default:   //Unknown
					break;
		} 	
	}
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : err_sys
// Description  : Print message to stderr and quit
//
// Inputs       : msg - message to be printed to stderrrr
// Outputs      : -1 if failure or 0 if successful
//
void err_sys(char *msg)
{
	fprintf( stderr, "error: PID %d, %s\n", getpid(), msg);
	exit(0);
}


