/* Hannah Buzard
CSCI 367
Lab 5
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>

#define QLEN 6 /* size of request queue */

/*------------------------------------------------------------------------
* Program: demo_server
*
* Purpose: allocate separate sockets for reader and writer clients then repeatedly call select
*
* Syntax: ./demo_server reader_port writer_port
*
* reader_port - port for reader-clients
* writer_port - port for writer-clients
*------------------------------------------------------------------------
*/

void initListenerSD(int port, int *sd) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */

	int optval = 1; /* boolean value when we set socket option */

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */
	sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */
    sad.sin_port = htons((u_short)port);

	/* Map TCP transport protocol name to protocol number */
	if(((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket */
	*sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if(*sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if(setsockopt(*sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(*sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"Error: Bind failed (%d)\n", port);
		perror("bind");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(*sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char **argv) {
    int sd;
	int listenerSDs[2]; /* socket descriptors */
	int reader_port; /* protocol port number for readers */
	int writer_port; /* protocol port number for writers */
	int alen; /* length of address */
	struct sockaddr_in cad; /* structure to hold client's address */
	fd_set active_FD_set, read_FD_set;

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server reader_port writer_port\n");
		exit(EXIT_FAILURE);
	}

	reader_port = atoi(argv[1]); /* convert argument to binary */
	if (reader_port < 0) { /* test for illegal value */
		fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}

	writer_port = atoi(argv[2]); /* convert argument to binary */
	if (writer_port < 0) { /* test for illegal value */
		fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
		exit(EXIT_FAILURE);
	}

	initListenerSD(reader_port, &listenerSDs[0]);
	initListenerSD(writer_port, &listenerSDs[1]);

	FD_ZERO(&active_FD_set);
	FD_SET(listenerSDs[0], &active_FD_set);
	FD_SET(listenerSDs[1], &active_FD_set);

	/* Main server loop - accept and handle requests */
	int readers[255]; //array to hold readers (for data sending) - up to 255
	int numreaders = 0; //count of connected readers
	while (1) {
		read_FD_set = active_FD_set;
      	if (select (FD_SETSIZE, &read_FD_set, NULL, NULL, NULL) < 0) {
			perror ("select");
			exit (EXIT_FAILURE);
		}

		for(int i = 0; i < FD_SETSIZE; i++) {
			if( FD_ISSET(i, &read_FD_set)) {
				if(i == listenerSDs[0]) {
					printf("A new reader has joined.\n");
					sd = accept(listenerSDs[0], (struct sockaddr *)&cad, &alen);
					if (sd < 0) { //accept new reader
						fprintf(stderr, "Error: Accept failed\n");
						exit(EXIT_FAILURE);
					} else {
    					readers[numreaders] = sd; //add reader to array of readers (for message sending later)
    					char buf[1000] = {0}; //buffer for data
    					sprintf(buf, "A new reader has joined.\n");
    					for(int j=0; j< numreaders; j++) { //send data to all readers
        				 send(readers[j],buf,strlen(buf),0);
        			}
    				numreaders++; //increase number of readers (for array usage)
					}
				} else if (i == listenerSDs[1]) {
					printf("A new writer has joined.\n");
					sd = accept(listenerSDs[1], (struct sockaddr *)&cad, &alen);
					if (sd < 0) { //accept new writer
						fprintf(stderr, "Error: Accept failed\n");
						exit(EXIT_FAILURE);
					}
    				FD_SET(sd, &active_FD_set); //add writer to active FD set
    				char buf[1000] = {0}; //buffer for data
    				sprintf(buf, "A new writer has joined.\n");
    				for(int j=0; j< numreaders; j++) { //send data to all readers
        				 send(readers[j],buf,strlen(buf),0);
        			}
				} else {
				  char buf[1000] = {0}; //buffer for data
					int numbytes; //number of bytes read
					numbytes = recv(i, buf, sizeof(buf),0); //receive data from a writer
					if(numbytes == 0) { //remove writer from active FD set and notify of writer's exit
					    FD_CLR(i, &active_FD_set);
					    printf("A writer has left\n");
					    sprintf(buf, "A writer has left\n");
					    for(int j=0; j< numreaders; j++) { //send data to all readers
    						send(readers[j],buf,strlen(buf),0);
    					}
					} else {
    					for(int j=0; j< numreaders; j++) { //send data to all readers
    						send(readers[j],buf,strlen(buf),0);
    				  }
					}
				}
			}
		}

	}
}