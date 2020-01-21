#include<zmq.h>
#include<iostream>
#include<sys/wait.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<map>
#include<cstring>

struct Message{
	int cmd;
	unsigned calc_id, cntr_id;
	unsigned vec_vis[100], kolvo_vis;
	unsigned vec_died[100], kolvo_died;
	unsigned port;
	int vec_sum[500], kolvo_sum;
	long long answer;
};

int main(int argc, char* argv[]){
	if(argc!=2){
		std::cerr<<"WRONG USAGE OF CALCULATOR"<<std::endl;
		return -1;
	}
	void* context = zmq_ctx_new();
	void* socket  = zmq_socket(context, ZMQ_REP);
	if(zmq_connect(socket, (std::string("tcp://localhost:")+argv[1]).c_str()) == -1){
		std::cerr<<"ERROR: CALCULATOR CAN'T TO PORT "<<argv[1]<<std::endl;
		return -2;
	}
	
	while(true){
		zmq_msg_t msg; Message msg_com;
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, socket, 0) == -1){
			zmq_msg_send(&msg, socket, 0);
			//zmq_msg_close(&msg);
			//std::cout<<"continue"<<std::endl;
			continue;
		}
		memcpy(&msg_com,zmq_msg_data(&msg), sizeof(Message));
		//std::cout<<"in calc "<<argv[1]<<" cmd = "<<msg_com.cmd<<std::endl;
		if(msg_com.cmd == 1){
			zmq_msg_init_size(&msg, sizeof(Message));
			memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
			zmq_msg_send(&msg, socket, 0);
		}else if(msg_com.cmd == 2){
			long long res=0;
			for(int i=0; i<msg_com.kolvo_sum; ++i)
				res+=msg_com.vec_sum[i];
			msg_com.answer=res;
			memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
			zmq_msg_send(&msg, socket, 0);
		}else if(msg_com.cmd == 0){
			zmq_msg_send(&msg, socket, 0);
			break;
		}
			
	}
	return 0;
}
