#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include "port.h"
#include "packetformation.c"
#include "packetstruct.h"

#define BUFLEN 2048
#define DATALEN 512
#define PKTSZ 516				
#define TIMEOUT 100000		

using namespace std;

class TFTPClient
{
	struct sockaddr_in myaddr, remaddr;//structure used for address of server and client;
	socklen_t slen;
	int fd, i;
	unsigned char buf[BUFLEN];	/* message buffer for transmitting the message */
	int recvlen;		/* # bytes received in acknowledgement */
	char *server;	/* used for server's ip address*/
	int num_timeouts;// used for tracking the loss of packets and resending it


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
		if (sendto(fd, buf, packetsize, 0, (struct sockaddr *)&remaddr, slen)==-1) {
			perror("sendto");
			exit(1);
		}
		printf("Sending block #%d of data with packet size %d bytes\n", block, packetsize-2);
	}

	/*The function is used to read data from the file whenever called. It opens a file to write the 
	received data . As soon as it receives the first packet it writes in the file and send the 
	acknowledgement to inform. Then it waits for the next packet to arrive and then after unpacking
	it again writes data  to file . It goes on doing this until the received packet is of length 
	less than 516 bytes. In that case it knows that it is the end packet and after sending the 
	acknowledgement it just closes the file.*/
	void readData(char *filename){
		uint16_t opcode, hblock=1, nblock;
		ofstream outfile(filename, ofstream::binary);
		if(!outfile){
			// ERROR
			printf("can't create file %s\n", filename);
			return;
		}
		while(1){
			//recvlen returns the length of the received packet 
			recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
			if(recvlen>0){
				//the opcode is separated from the headers using this unpack function
				unpack(buf, (char*)"h", &opcode);
				//opcode 3 is used to denote the received packet contains data
				if(opcode==3){
					//header containing block is unpacked
					unpack(buf+2, (char*)"h", &nblock);
					//checked whether the received block is as same as the expected block or not
					if(nblock==hblock){
						char ldata[DATALEN];
						//data is unpacked
						unpack(buf+4, (char*)"512s", ldata);
						string lwrt = ldata;
						//data is written to the file opened by client for writing the data received by the server
						outfile.write(lwrt.c_str(), lwrt.length());
						printf("Received block #%d of data with packet size %d bytes\n", nblock, recvlen-2);

						// sendAck sends the acknowledgement to the server indicating the packet is received
						sendAck(nblock);
						//hblock is increased to denote the block expected the next time.
						hblock++;
					}
					else{
						// ERROR----TODO
						printf("expecting block %d, received block %d\n", hblock, nblock);
						/*if expected block is different from the received block then the client will understand
						that the previous acknowledgement was lost and it will try to resend again*/ 
						sendAck(hblock-1);
					}
				}
				/* when opcode is 5 it simply means that the client is not able to receive the data properly
				due to some error by the server so it closes the file and prints error has occured by calling
				processError  function */
				else if(opcode==5){
					processError();
					outfile.close();
					remove(filename);
					return;
				}
				/* If the client receives the packet which does not contain opcode 3 or 5 the the packet is
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
	/* sendAck is used for sending acknowledgement to the server and the opcode for acknowledgement is 4
	so to send the packets we attach the headers opcode 4 and block no received*/
	void sendAck(int block){
		struct Ack ack;
		int packetsize;
		unsigned char abuf[DATALEN];

		ack.opcode = 4;
		ack.block = block;
		//used for packing the acknowledgement along with the headers  and it returns the bytes packed
		packetsize = pack(abuf, (char*)"hh", ack.opcode, ack.block);
		//sendto sends the acknowledgement packet to the server.
		if (sendto(fd, abuf, packetsize, 0, (struct sockaddr *)&remaddr, slen) < 0)
			perror("sendto");
		printf("Sending Ack #%d\n", block);
	}

	/* receive acknowledgement returns whether we have successfully received acknowledgement or not.*/
	bool recvAck(int block){
		// It receives the acknowledgement from the server and stores the bytes received
		recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
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
        	/* when opcode is 5 it simply means that the client is not able to receive the acknowledgement 
        		properly due to some error by the server so it closes the file and prints error has occured by 
				calling processError  function */
        	else if(opcode==5){
        		processError();
        		exit(0);
        	}
        	else{
        		// ERROR
        		printf("expected Ack, received packet with opcode %d\n", opcode);
        	}
        	return true;
        }
        /*if the acknowledgement is not received till a prticular time then client will think
        that server has not received data so it will resend it again . If this operation is done 
        already 10 times it will just print difficult to send */
        else{
        	// ERROR
        	printf("Timout reached. Resending segment %d\n", block);
        	return false;
        }
	}

	/* The function is used for sending read or write request to the server . If opcode is 1 it will send
	a read request otherwise it will send a write request for opcode 2 the read request packet also contains
	the mode of information such as netascii or octet and it also containg the name of the file to read from 
	or write to.  */
	void sendRequest(int opcode, char *filename){
		struct Request rdwrt;
		int packetsize;
		rdwrt.opcode = opcode;
		rdwrt.filename=filename;
		rdwrt.e1=0;
		rdwrt.mode=(char*)"Netascii";
		rdwrt.e2=0;
		// used for packing the read or write request along with opcode,mode,filename and null characters
		packetsize = pack(buf, (char*)"hscsc", rdwrt.opcode, rdwrt.filename, rdwrt.e1, rdwrt.mode, rdwrt.e2);
		if (sendto(fd, buf, packetsize, 0, (struct sockaddr *)&remaddr, slen)==-1) {
			perror("sendto");
			exit(1);
		}
		if(opcode==2)
			printf("Sending [Write Request] for file %s with packet size %d bytes\n", filename, packetsize-2);
		else if(opcode==1)
			printf("Sending [Read Request] for file %s with packet size %d bytes\n", filename, packetsize-2);
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
		if (sendto(fd, ebuf, packetsize, 0, (struct sockaddr *)&remaddr, slen) < 0)
			perror("sendto");
		printf("Sending Error: %s\n", err.errmsg);
	}
	/* This function receives the error packet send by the server unpacks it and after seeing the opcode to
	be 5 it prints that error is received  */
	void processError(){
		uint16_t ErrorCode;
		char errmsg[DATALEN];
		unpack(buf+2, (char*)"h", &ErrorCode);
		unpack(buf+4, (char*)"512s", errmsg);
		printf("Received Error: %s\n", errmsg);
	}

public:
	TFTPClient(){}
	// The constructor takes the ip address of the server input by the client
	TFTPClient(char *server) : server(server) {
		slen=sizeof(remaddr);//length of remote address

		/* Used to create a socket which uses SOCK_DGRAM because it is implemented 
		on  top of UDP */
		if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
			printf("socket created\n");

		/* The function memset binds it to all local addresses and picks any port number */
		memset((char *)&myaddr, 0, sizeof(myaddr));
		myaddr.sin_family = AF_INET; // to use IPV4
		myaddr.sin_addr.s_addr = htonl(INADDR_ANY);//address long
		myaddr.sin_port = htons(0);// port short
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
		tv.tv_usec = TIMEOUT;// Explicitly frees the port
		if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		  perror("Error: setsockopt (in constructor)");
		}       

		/* The  remaddr is  the address to whom we want to send messages 
		 For convenience, the host address is expressed as a numeric IP address 
		  we will convert to a binary format via inet_aton */

		memset((char *) &remaddr, 0, sizeof(remaddr));
		remaddr.sin_family = AF_INET;
		remaddr.sin_port = htons(SERVICE_PORT);
		if (inet_aton(server, &remaddr.sin_addr)==0) {
			fprintf(stderr, "inet_aton() failed\n");
			printf("give a proper server address\n");
			exit(1);
		}
	}
	/* The readFile function is used to read a give file from the server. If the file already
	exist then error msg is printed*/
	void readFile(char *filename){
		struct stat st_buf;
	    if (stat(filename, &st_buf) != -1)
	    {
	    	printf("File %s already exists\n", filename);
	        return;
	    }
	    //Request is send using opcode 1 and the filename
		sendRequest(1, filename);	
		readData(filename);
	}
	/* The function is used to write a file to the server . Opcode 2 is used for writing */
	void writeFile(char *filename){
        ifstream infile(filename, ifstream::binary);
        if(infile){
        	struct stat stat_buf;
	    	int rc = stat(filename, &stat_buf);
	    	cout<<"File size is "<<stat_buf.st_size<<"bytes \n";

			// Request send for connection. After receiving acknowledgement 1st block of data is send
			sendRequest(2, filename);
			num_timeouts=0; //used to count number of times packet has been sent
			while(!recvAck(0)){
				sendRequest(2, filename);
				num_timeouts++;
				/* if we send same packet some 10 times and we don't get acknowledgement we simply
				know that the server is not responding */
				if(num_timeouts>10){
					printf("seems like server is not responding exiting, try later\n");
					close(fd);
					exit(0);
				}
			}

	    	// according to file size, fragment
	    	int block=1;
	    	while(infile.good()){
	    		char *lbuff = new char[DATALEN];
	    		// read the file and then send the bytes read through sendData
	    		infile.read(lbuff, DATALEN);
	    		sendData(lbuff, block);
	    		num_timeouts=0;
	    		while(!recvAck(block)){
	    			sendData(lbuff, block);
	    			num_timeouts++;// we increase it each time to keep track of how many times we are sending it
	    			/* if we send same packet some 10 times and we don't get acknowledgement we simply
						know that the server is not responding */
	    			if(num_timeouts>10){
	    				printf("seems like server is not responding exiting, try later\n");
	    				infile.close();
	    				close(fd);
 						exit(0);
	    			}
	    		}
	    		block++;
	    	}
	    }
	    // if we are not able to find the file then the error msg is printed
	    else{
	    	// ERROR
	    	printf("file not found\n");return;
	    }
	    infile.close();
	    printf("file closed\n");
	}
	// deconstructor used for closing the file
	~TFTPClient(){
		close(fd);
	}
};

int main(int argc, char *argv[])
{
	//To take proper Ip address, filename and read-write mode
	if(argc<4){
		printf("Usage:./a server r|w file\n");
		return 0;
	}
	char *server, *filename;
	server = argv[1];
	filename = argv[3];
	TFTPClient client(server);
	/*If the input is r we want to read the file otherwise write it */
	if(argv[2][0]=='r'){
		client.readFile(filename);	
	}
	else if(argv[2][0]=='w'){
		client.writeFile(filename);
	}
	return 0;
}
