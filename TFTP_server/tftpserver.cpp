#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "port.h"
#include "packetformation.c"
#include "packetstruct.h"

#define BUFSIZE 2048
#define DATALEN 512
#define PKTSZ 516
#define TIMEOUT 100000		

using namespace std;

class TFTPServer
{
	struct sockaddr_in myaddr;	/*It denotes our address */
	struct sockaddr_in remaddr;	/*IT denotes remote address */
	socklen_t addrlen;		/*Denotes the length of addresses */
	int recvlen;			/*no. of bytes bytes received */
	int fd;				/* socket */
	int msgcnt = 0;			/* count no. of messages we received */
	unsigned char buf[BUFSIZE];	/* receive buffer */
	int num_timeouts; /*Used for timeout*/

	/* This function sends data and wait for acknowledgement. Since the opcode for sending data
	   is 3 so we define opcode to be 3 and block number is send along with data to keep track 
	   of the particular block.*/
	void sendData(char *data, int block){
		struct Data dpacket;
		int packetsize;

		dpacket.opcode=3;
		dpacket.block=block;
		dpacket.data=data;
		/*The function packetsize is used for making packets and then returning it's size.
		the hhs is used as an input to show that the before the data the headers are of 16
		bits and string format is used for the data*/
		packetsize = pack(buf, (char*)"hhs", dpacket.opcode, dpacket.block, dpacket.data);
		/*sendto function sends the data through the buffer to the opposite side of the socket.
		If the packets are dropped in between then we are getting the error msg. */
		if (sendto(fd, buf, packetsize, 0, (struct sockaddr *)&remaddr, addrlen)==-1) {
			perror("sendto");
			exit(1);
		}
		printf("Sending block #%d of data with packet size %d bytes\n", block, packetsize-2);
	}

	/* Receive acknowledgement returns whether we have successfully received acknowledgement or not.*/
	bool recvAck(int block){
		/* receive acknowledgement returns whether we have successfully received acknowledgement or not.*/
		recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
        if (recvlen > 0) {
        	uint16_t opcode, lblock;
        	//unpacks the opcode to check whether it is an acknowledgement packet or not
        	unpack(buf, (char*)"h", &opcode);
        	if(opcode==4){
        		//unpacks the block number
        		unpack(buf+2, (char*)"h", &lblock);
        		//compares whether the expected block number is same as received block number
        		if(lblock==block){
        			printf("Received Ack #%d\n", lblock);
        		}
        		else{
        			// ERROR
        			printf("expected Ack #%d, received Ack #%d\n", block, lblock);
        		}
        	}
        	/* when opcode is 5 it simply means that the server is not able to receive the acknowledgement 
        		properly due to some error by the client so it closes the file and prints error has occured by 
				calling processError  function */
        	else if(opcode==5){
        		processError();
        		return false;
        	}
        	else{
        		// ERROR
        		printf("expected Ack, received packet with opcode %d\n", opcode);
        		return false;
        	}
        	return true;
        }
        /*if the acknowledgement is not received till a prticular time then server will think
        that client has not received data so it will resend it again . If this operation is done 
        already 10 times it will just print difficult to send */
        else{
        	// ERROR
        	printf("Timout reached. Resending segment %d\n", block);
        }
        return false;
	}
	/* sendAck is used for sending acknowledgement to the client and the opcode for acknowledgement is 4
	so to send the packets we attach the headers opcode 4 and block no received*/
	void sendAck(int block){
		struct Ack ack;
		int packetsize;
		unsigned char abuf[DATALEN];

		ack.opcode = 4;
		ack.block = block;

		//used for packing the acknowledgement along with the headers and it returns the bytes packed
		packetsize = pack(abuf, (char*)"hh", ack.opcode, ack.block);
		//sendto sends the acknowledgement packet to the client.
		if (sendto(fd, abuf, packetsize, 0, (struct sockaddr *)&remaddr, addrlen) < 0)
			perror("sendto");
		printf("Sending Ack #%d\n", block);
	}
	/*This function sends the error packet . The error opcode is 5 and there can be differenent kinds of error
	The error packet contains the error type along with the error message. */
	void sendError(int errcode){
		struct Error err;
		int packetsize;
		unsigned char ebuf[DATALEN];

		err.opcode = 5;
		err.ErrorCode = errcode;
		switch(errcode){
			case 1:
				err.errmsg = (char*)"File not found\n";
				break;
			case 2:
				err.errmsg = (char*)"Access violation\n";
				break;
			case 6:
				err.errmsg = (char*)"File already exists\n";
				break;
			default:
				err.errmsg = (char*)"";
		}
		err.e1=0;
		//hhsc is used to denote 2 16 bits followed by a string and followed by a char 
		packetsize = pack(ebuf, (char*)"hhsc", err.opcode, err.ErrorCode, err.errmsg, err.e1);
		if (sendto(fd, ebuf, packetsize, 0, (struct sockaddr *)&remaddr, addrlen) < 0)
			perror("sendto");
		printf("Sending Error: %s\n", err.errmsg);
	}
	/* This function receives the error packet send by the client unpacks it and after seeing the opcode to
	be 5 it prints that received packet has error */
	void processError(){
		uint16_t ErrorCode;
		char errmsg[DATALEN];
		unpack(buf+2, (char*)"h", &ErrorCode);
		unpack(buf+4, (char*)"512s", errmsg);
		printf("Received Error: %s\n", errmsg);
	}
	/* The readFile function is used to send a given file to the client. 
	After the client requests read file, the server start sending first block of the data. 
	The function does not imply server has to read data . It simply means client want to
	read data so server is sending it's first block.*/
	void readFile(char *filename){
		// open the file and start sending the data
        // and receiving the acknowledgements
        ifstream infile(filename, ifstream::binary);
        if(infile){
	        struct stat stat_buf;
	    	int rc = stat(filename, &stat_buf);
	    	cout<<"File size is "<<stat_buf.st_size<<"bytes \n";

	    	// according to file size, fragment
	    	int block=1;
	    	while(infile.good()){
	    		char *lbuff = new char[DATALEN];
	    		//// read the file and then send the bytes read through sendData
	    		infile.read(lbuff, DATALEN);
	    		sendData(lbuff, block);
	    		num_timeouts=0;
	    		//till we do not receive the acknowledgement for the block
	    		while(!recvAck(block)){
	    			sendData(lbuff, block);
	    			num_timeouts++;// we increase it each time to keep track of how many times we are sending it
	    			/* if we send same packet some 10 times and we don't get acknowledgement we simply
						know that the server is not responding */
	    			if(num_timeouts>10){
	    				printf("seems like client is not responding\n");
 						infile.close();
 						return;
	    			}
	    		}
	    		block++;
	    	}
	    }
	    // if we are not able to find the file then the error msg is printed
	    else{
	    	// ERROR
	    	printf("file doesn't exists\n");
	    	sendError(1);
	    }
	    infile.close();
	    printf("file closed\n");
	}
	/* Whenever the client requests to write a file to the server , the server uses
	this function . First it sends an acknowledgement of block 0  indicating that it 
	is ready to accept from the client and then as he starts receiving the blocks, he send 
	acknowledgements to the client informing that data is being written . If the received 
	packet is less than 516 bytes , it knows that it is the end of the file*/
	void writeFile(char *filename){
		struct stat st_buf;
	    if (stat(filename, &st_buf) != -1)
	    {
	    	printf("File %s already exists\n", filename);
	    	sendError(6);
	        return;
	    }
		// send ack with block=0 to allow the client to send data
		sendAck(0);

		ofstream outfile(filename, ofstream::binary);
		//if file cannot be accessed to write
		if(!outfile){
			// ERROR
			printf("can't create file %s\n", filename);
			sendError(0);
			return;
		}
		uint16_t opcode, hblock=1, nblock;
		while(1){
			// bytes of the received packet
			recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
			if(recvlen>0){
				// unpacks the opcode to know it is a data packet
				unpack(buf, (char*)"h", &opcode);
				if(opcode==3){
					// unpacks the block number to verify whether the expected block number is same as received block number
					unpack(buf+2, (char*)"h", &nblock);
					if(nblock==hblock){
						char ldata[DATALEN];
						unpack(buf+4, (char*)"512s", ldata);
						string lwrt = ldata;
						//writing in the file received bytes
						outfile.write(lwrt.c_str(), lwrt.length());
						printf("Received block #%d of data with packet size %d bytes\n", nblock, recvlen-2);
						//sending Acknowledgement for received data
						sendAck(nblock);
						hblock++;
					}
					else{
						// ERROR----TODO
						printf("expecting block no. %d, received block no. %d\n", hblock, nblock);
						/*if expected block is different from the received block then the server will understand
						that the previous acknowledgement was lost and it will try to resend again*/ 
						sendAck(hblock-1);
					}
				}
				/* when opcode is 5 it simply means that the server is not able to receive the data properly
				due to some error by the client so it closes the file and prints error has occured by calling
				processError function and informs the client about it */
				else if(opcode==5){
					processError();
					outfile.close();
					remove(filename);
					return;
				}
				/* If the server receives the packet which does not contain opcode 3 or 5 the the packet is
				unrecognised so it close the file. */
				else{
					// ERROR------TODO
					printf("expecting data packet, received packet with opcode %d\n", opcode);
					outfile.close();
					remove(filename);
					return;
				}
				// if data received is less than 512 bytes we have to get out since it is the last block of data
				if(recvlen<PKTSZ)	break;
			}
		}
		outfile.close();
		cout<<"file closed\n";
	}
public:
	TFTPServer(){
		addrlen = sizeof(remaddr); //length of remote address
		
		/* Used to create a socket which uses SOCK_DGRAM because it is implemented 
		on  top of UDP */
		if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("cannot create socket\n");
			exit(0);
			// return 0;
		}

		/* The function memset binds it to all local addresses and picks any port number */
		memset((char *)&myaddr, 0, sizeof(myaddr));
		myaddr.sin_family = AF_INET; // to use IPV4
		myaddr.sin_addr.s_addr = htonl(INADDR_ANY);// address long
		myaddr.sin_port = htons(SERVICE_PORT); // port short

		// If bind fails then The IP address is already in use
		if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
			perror("bind failed");
			exit(0);
		}

		// wait for 100ms for recvfrom
		struct timeval tv;
		/* tv_sec and tv_usec are used for timer implementation . If the packet is not received 
			before timeout then it has to be send again*/
		tv.tv_sec = 0;
		tv.tv_usec = TIMEOUT;
		if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		  perror("Error: setsockopt (in constructor)");
		}       
	}
	/* Function through which the server starts and after receiving the first packet 
	checks the opcode . If the opcode received is 1 it means client has requested to 
	read a file, if it is 2 client has requested to write a file and if it is 5 some 
	kind of error has occured */
	void start(){
		/* now loop, receiving data and printing what we received */
		printf("waiting on port %d\n", SERVICE_PORT);
		for (;;) {
			// receiving the request and noting it's size
			recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
			if (recvlen > 0) {
				char *clientadd;
				// to get the remote address
				clientadd=inet_ntoa(remaddr.sin_addr);
				printf("new connection from %s\n", clientadd);
				uint16_t opcode=0;
				// unpacking the packet to know the opcode
				unpack(buf, (char*)"h", &opcode);
				
				if(opcode==1){
					// received opcode is 1 client wants to read a file
					char filename[500];
					unpack(buf+2, (char*)"500s", filename);
					printf("Received [Read Request] for file %s with packet size %d bytes\n", filename, recvlen-2);
					readFile(filename);
					printf("\n\nwaiting on port %d\n", SERVICE_PORT);
				}
				else if(opcode==2){
					//// received opcode is 2 client wants to write a file
					char filename[500];
					unpack(buf+2, (char*)"500s", filename);
					printf("Received [Write Request] for file %s with packet size %d bytes\n", filename, recvlen-2);
					writeFile(filename);
					printf("\n\nwaiting on port %d\n", SERVICE_PORT);
				}
				else{
					// if instead of a reqquest some error packet is send by the client.
					if(opcode==5)	processError();
					printf("expected packet with opcode 1 or 2, received with opcode %d\n", opcode);
					printf("\n\nwaiting on port %d\n", SERVICE_PORT);
				}
			}
		}
	}

	~TFTPServer(){}
};

int main(int argc, char *argv[])
{
	//server is started for client to read,write,request
	TFTPServer server;
	server.start();
	return 0;
}
