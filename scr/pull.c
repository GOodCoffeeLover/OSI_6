#include<zmq.h>
#include<iostream>
#include<sys/wait.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<map>
#include<cstring>

int main(int argc, char *argv[]){
	std::string port="tcp://localhost:8080";
	//port+=argv[1];
	void* contex=zmq_ctx_new();
	std::cout<<"new contex create"<<std::endl;
	void* sock_pull=zmq_socket(contex, ZMQ_PULL);
	std::cout<<"new socket create"<<std::endl;
	if( zmq_connect(sock_pull, port.c_str()) !=0){
		std::cout<<"can't bind "<<port<<' '<<errno<<std::endl;
		return 0;
	}
	std::cout<<"new socket binded"<<std::endl;
	int i=1;
	while(i){
		zmq_msg_t msg;
		zmq_msg_init_size(&msg, sizeof(int));
		std::cout<<">>"<<std::endl;
		zmq_msg_recv(&msg, sock_pull, 0);
		memcpy(&i, zmq_msg_data(&msg), sizeof(int));
		std::cout<<"recv "<<i<<std::endl;
	}
	std::cout<<"exit from while"<<std::endl;
	zmq_close(sock_pull);
	zmq_close(contex);
	return 0;
}
