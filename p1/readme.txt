CSC361 
Chengxiang Xiong

1. typing "make" to compile the sws.c file.

2. testing the simple server by open a new termnal under the same folder and typing "./testfile.sh".

3. sws.c including 3 major parts:
	main function: seting the socket, binding the ip address and creating a thread to receive the request.
	recv_proc: receiving the request, checking the legality by call is_valid_http_request function and return the message to client
	is_valid_http_request: checking if the input is a legal language or not.
	
