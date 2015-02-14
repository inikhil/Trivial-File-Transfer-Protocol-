														   TFTP Implementation
---------------------------------------------------------------------------------------------------------------------------------------------						
														   CS349 : Networks Lab
														   Name: Nikhil Agarwal
															Roll No : 11012323
															   IIT Guwahati
															   Date:20/02/14
-----------------------------------------------------------------------------------------------------------------------------------------------
															  Code compilation

Withot implementing the server it was difficult for me to check the send packet options. So I implemented both. My folder contain two 
subfolders, one is named TFTP_client which contains the client implementation and the othet one is named TFTP_server. To compile
go the corresponding subfolders using cd command and then

                                                               On server side

write the following code: g++ -std=c++0x tftpserver.cpp
after compilation write: ./a.out

															   On client side 

write the following code: g++ -std=c++0x tftpclient.cpp	
afetr compilation write: ./a.out Ipaddress r/w filename
For example,if we want to read: ./a.out 10.1.2.49 read nik.txt  		
For example,if we want to write: ./a.out 10.1.2.49 write nik.txt  	

                                                         This completes our compilation portion
---------------------------------------------------------------------------------------------------------------------------------------------------
                                                           Basic walk through the code
---------------------------------------------------------------------------------------------------------------------------------------------------

Firstly, we were required to make socket handling code to open the UDP socket to the server. for this socket functions and bind functions were used.
The proper implementation can be found in tftpclient.cpp along with the written comments.

Secondly, we were required to construct packets such as request packet, data packet and acknowledgement packets. For this I declared different 
structures for this packet. Since the request packet contains the opcode 1 corresponding to read and 2 corresponding to write. These packet 
have similar structures except for the opcode value. They contain opcode, mode,filename ,etc . The data packet contains opcode, blocknumber
and data, the opcode for data packets are given by 3 while the acknowledgement packets have opcode 4 and they include block number too.
After constructing this packet we were pretty ready for sending and receiving files from the client to the server.

The sending and receiving from client to the server were dealt by using sendto and recvfrom kind of predefined functions.

Thirdly, we send the request from the client to read file from the server. The server then responds with the first block of data after which client
sends the acknowledgement. If the acknowledgement is not received by the server, it will not send next block of data and after some time-out 
client has to send the acknowledgement again. For this we used sendRequest, sendAck , readFile, readData kind of declared functions and timer
structures.

After that we dealt with sending a request to write file to the server. The server then responded with the acknowledgement after which client
sent the first block of data packet. If the data packet is not received by the server, it will not send acknowledgement and after some time-out 
client has to send the packet again. For this we used sendRequest, recvAck , writeFile, writeData kind of declared functions and timer structures .

Lastly, we dealt with error packets which have opcode 5 and contains an error number along with an error message. The server responds to the client
by error packet. The error messages are generally of the form File not found , Access violation , File already exists, etc and for this we used
declared functions such as sendError, processError.

-----------------------------------------------------------------------------------------------------------------------------------------------------
																	Results
-----------------------------------------------------------------------------------------------------------------------------------------------------

Results are displayed in a pdf file in an arranged format named TFTP.pdf which is in my folder.

--------------------------------------------------------------------------------------------------------------------------------------------------



