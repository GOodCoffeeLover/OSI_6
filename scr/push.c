#include<zmq.h>
#include<iostream>
#include<sys/wait.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<map>
#include<cstring>

int main(int argc, char* argv[]){
	std::string port="tcp://*:8080";
	//port+=argv[1];
	void* contex=zmq_ctx_new();
	void* sock_push=zmq_socket(contex, ZMQ_PUSH);
	if( zmq_bind(sock_push, port.c_str()) !=0){
		std::cout<<"can't connect "<<port<<' '<<errno<<std::endl;
		return 0;
	}
	int i=1;
	while(i){
		std::cout<<">>";
		std::cin>>i;
		zmq_msg_t msg;
		zmq_msg_init_size(&msg, sizeof(int));
		memcpy(zmq_msg_data(&msg), &i, sizeof(int));
		std::cout<<">>"<<std::endl;
		zmq_msg_send(&msg,sock_push, 0);
		std::cout<<"send "<<i<<std::endl;
	}
	zmq_close(sock_push);
	zmq_close(contex);
	return 0;
}
