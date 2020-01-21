#include<zmq.h>
#include<iostream>
#include<sys/wait.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<map>
#include<cstring>

const int PING_TIME_CNTR=100, PING_TIME_CALC=5, ZERO=0;
const size_t int_s=sizeof(int);

struct Message{
	int cmd;
	unsigned calc_id, cntr_id;
	unsigned vec_vis[100], kolvo_vis;
	unsigned vec_died[100], kolvo_died;
	unsigned port;
	int vec_sum[500], kolvo_sum;
	long long answer;
};

struct calculate_node{
	pid_t pid;
	void* sock_s_r; //REQ REP
};

struct controller_node{
	unsigned id;
	void* sock_send; //PUSH
};

struct arg_create{
	void* context;
	unsigned port;
	std::map<unsigned, calculate_node> *calculators;
	std::vector<controller_node> *controllers;
	unsigned my_id, calc_id;
	std::vector<unsigned> visited;
};

void* create_calc_node(void *a){
	arg_create* arg=(arg_create*) a;
	if(arg->calculators->find(arg->calc_id)!=arg->calculators->end()){
		zmq_msg_t msg; Message msg_com;
		calculate_node cur=(*arg->calculators)[arg->calc_id];
		msg_com.cmd=1;
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
		zmq_msg_send(&msg, cur.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, cur.sock_s_r, 0) == -1){
			std::cout<<"ERROR: "<<arg->calc_id<<" ALREADY EXISTS, BUT IS UNAVAILABLE"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		}else{
			std::cout<<"ERROR: "<<arg->calc_id<<" ALREADY EXISTS"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		}	
		delete arg;
		return 0;
	}

	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check,("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	arg->visited.push_back(arg->my_id);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont)
				continue;
		zmq_msg_t msg; Message msg_com;
		msg_com.cmd=11;
		msg_com.port=port_check;
		msg_com.calc_id=arg->calc_id;
		msg_com.kolvo_vis=arg->visited.size();
		for(int j=0; j<msg_com.kolvo_vis; ++j){
			msg_com.vec_vis[j]=arg->visited[j];
		}
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		
		if(zmq_msg_recv(&msg, sock_check, 0) == -1) continue;
		memcpy(&msg_com, zmq_msg_data(&msg), sizeof(Message));
		// >0 didnt find
		//-1 find 
		//-2 find, unavailable
		if(msg_com.cmd==1){
			for(int j=arg->visited.size(); j<msg_com.kolvo_vis; ++j)
				arg->visited.push_back(msg_com.vec_vis[j]);
		}else if(msg_com.cmd==-1){
			std::cout<<"ERROR: "<<arg->calc_id<<" ALREADY EXISTS"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
			delete arg;
			zmq_close(sock_check);
			return 0;
		}else if(msg_com.cmd==-2){
			std::cout<<"ERROR: "<<arg->calc_id<<" ALREADY EXISTS, BUT IS UNAVAILABLE"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
			delete arg;
			zmq_close(sock_check);
			return 0;
		}	
	}
	zmq_close(sock_check);
	calculate_node new_node;
	new_node.sock_s_r=zmq_socket(arg->context, ZMQ_REQ);
	int port=0;
	while(zmq_bind(new_node.sock_s_r, ("tcp://*:"+std::to_string(++port)).c_str()) !=0);
	zmq_setsockopt(new_node.sock_s_r, ZMQ_RCVTIMEO, &PING_TIME_CALC, int_s);
	zmq_setsockopt(new_node.sock_s_r, ZMQ_SNDTIMEO, &PING_TIME_CALC, int_s);
	zmq_setsockopt(new_node.sock_s_r, ZMQ_LINGER, &ZERO, int_s);
	new_node.pid=fork();
	if(new_node.pid){
		//sleep(1);
		zmq_msg_t msg; Message msgg;
		msgg.cmd=1;//check 
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &msgg, sizeof(Message));
		zmq_msg_send(&msg, new_node.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if( zmq_msg_recv(&msg, new_node.sock_s_r, 0) == -1){
			std::cout<<"ERROR: CAN'T CREATE NEW CALCULATOR"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush; 
			zmq_close(new_node.sock_s_r);
		}else{
			memcpy(&msgg, zmq_msg_data(&msg), sizeof(Message));
				std::cout<<"Ok: "<<new_node.pid<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
				(*arg->calculators)[arg->calc_id]=new_node;
		}
	}else{
		execl("./calculator", "./calculator", (std::to_string(port)).c_str(), NULL);
		exit(-1);
	}
	return 0;
}

void* check_create_calc_node(void *a){
	arg_create* arg=(arg_create*) a;
	void* sock_ans=zmq_socket(arg->context, ZMQ_PUSH);
	zmq_connect(sock_ans, ("tcp://localhost:"+std::to_string(arg->port)).c_str());
	if(arg->calculators->find(arg->calc_id)!=arg->calculators->end()){
		zmq_msg_t msg; Message msg_com;
		calculate_node cur=(*arg->calculators)[arg->calc_id];
		msg_com.cmd=1;
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
		zmq_msg_send(&msg, cur.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, cur.sock_s_r, 0) == -1){
			msg_com.cmd=-2;
		}else{
			msg_com.cmd=-1;
		}	
		memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
		zmq_msg_send(&msg, sock_ans, 0);
		delete arg;
		return 0;
	}

	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check,("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	arg->visited.push_back(arg->my_id);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont)
				continue;
		zmq_msg_t msg; Message msg_com;
		msg_com.cmd=11;
		msg_com.calc_id=arg->calc_id;
		msg_com.port=port_check;
		msg_com.kolvo_vis=arg->visited.size();
		for(int j=0; j<msg_com.kolvo_vis; ++j){
			msg_com.vec_vis[j]=arg->visited[j];
		}
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		
		
		if(zmq_msg_recv(&msg, sock_check, 0) == -1) continue;
		memcpy(&msg_com, zmq_msg_data(&msg), sizeof(Message));
		// 1 didnt find
		//-1 find 
		//-2 find, unavailable
		if(msg_com.cmd==1){
			for(int j=arg->visited.size(); j<msg_com.kolvo_vis; ++j)
				arg->visited.push_back(msg_com.vec_vis[j]);
			zmq_msg_close(&msg);
		}else if(msg_com.cmd==-1 || msg_com.cmd==-2){
			memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
			zmq_close(sock_check);
			delete arg;
			return 0;
		}
	}
	zmq_close(sock_check);
	zmq_msg_t msg; Message msg_com;
	msg_com.cmd=1;
	msg_com.kolvo_vis=arg->visited.size();
	for(int j=0; j<msg_com.kolvo_vis; ++j)
		msg_com.vec_vis[j]=arg->visited[j];
	zmq_msg_init_size(&msg, sizeof(Message));
	memcpy(zmq_msg_data(&msg), &msg_com, sizeof(Message));
	zmq_msg_send(&msg, sock_ans, 0);
	return 0;
}

struct arg_union{
	void* context;
	unsigned port;
	std::vector<controller_node> *controllers;
	unsigned my_id, target_id;
};

void* do_union(void* a){
	arg_union *arg=(arg_union*) a;
	if(arg->my_id==arg->target_id){
		std::cout<<"ERROR: CAN'T UNION WITH MYSELF"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		delete arg;
		return 0;
	}
	for(int i=0; i<arg->controllers->size(); ++i)
		if(arg->target_id==(*arg->controllers)[i].id){
			std::cout<<"ERROR: ALREADY UNIONED WITH "<<arg->target_id<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
			delete arg;
			return 0;
		}
	controller_node new_node{arg->target_id, zmq_socket(arg->context, ZMQ_PUSH)};
	zmq_setsockopt(new_node.sock_send, ZMQ_SNDTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(new_node.sock_send, ZMQ_LINGER, &ZERO, int_s);
	
	if(zmq_bind(new_node.sock_send,("tcp://*:"+std::to_string(arg->target_id)).c_str()) ==0){
		std::cout<<"ERROR: CONTROLLER NODE "<<arg->target_id<<" IS NOT EXSIST"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		zmq_close(new_node.sock_send);
		delete arg;
		return 0;
	} 
	if(zmq_connect(new_node.sock_send, ("tcp://localhost:"+std::to_string(arg->target_id)).c_str())!=0){
		std::cout<<"ERROR: CAN'T CONNECT WITH "<<arg->target_id<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		delete arg;
		return 0;	
	}
	void* sock_check = zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check,("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	zmq_msg_t msg; Message msg_s;
	msg_s.cmd=41;
	msg_s.cntr_id=arg->my_id;
	msg_s.port=port_check;
	zmq_msg_init_size(&msg, sizeof(Message));
	memcpy(zmq_msg_data(&msg), &msg_s, sizeof(Message));
	zmq_msg_send(&msg, new_node.sock_send, 0);
	sleep(1);
	zmq_msg_init_size(&msg, sizeof(Message));
	if(zmq_msg_recv(&msg, sock_check, 0) == -1){
		std::cout<<"ERROR: CONTROLLER "<<arg->target_id<<" IS UNAVAILABLE"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		delete arg;
		zmq_close(sock_check);
		zmq_close(new_node.sock_send);
		return 0;
	}
	zmq_msg_close(&msg);
	zmq_close(sock_check);
	arg->controllers->push_back(new_node);
	std::cout<<"OK: UNION "<<new_node.id<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
	delete arg;
	return 0;
}

void* answerd_union(void* a){
	arg_union *arg=(arg_union *) a;
	controller_node new_node{arg->target_id, zmq_socket(arg->context, ZMQ_PUSH)};
	zmq_connect(new_node.sock_send, ("tcp://localhost:"+std::to_string(arg->target_id)).c_str());
	zmq_setsockopt(new_node.sock_send, ZMQ_SNDTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(new_node.sock_send, ZMQ_LINGER, &ZERO, int_s);
	void* sock_ans=zmq_socket(arg->context, ZMQ_PUSH);
	zmq_connect(sock_ans, ("tcp://localhost:"+std::to_string(arg->port)).c_str() );
	zmq_msg_t msg; Message msgg;
	msgg.cmd=1;
	zmq_msg_init_size(&msg, sizeof(Message));
	memcpy(zmq_msg_data(&msg), &msgg, sizeof(Message));
	zmq_msg_send(&msg, sock_ans, 0);
	arg->controllers->push_back(new_node);
	std::cout<<"UNION WITH "<<arg->target_id<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
	delete arg;
	return 0;
}

struct arg_remove{
	void* context;
	unsigned port;
	std::map<unsigned, calculate_node> *calculators;
	std::vector<controller_node> *controllers;
	unsigned my_id, calc_id;
	std::vector<unsigned> visited;
};

void* remove_node(void *a){
	arg_remove *arg=(arg_remove*) a;
	if(arg->calculators->find(arg->calc_id)!=arg->calculators->end()){
		calculate_node cur=(*arg->calculators)[arg->calc_id];
		zmq_msg_t msg; Message msg_die;
		msg_die.cmd=0;
		zmq_msg_init_size(&msg, sizeof(Message));
		zmq_msg_send(&msg, cur.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, cur.sock_s_r, 0) == -1){
			std::cout<<"ERROR: "<<arg->calc_id<<" IS UNAVAILABLE"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		}else{
			arg->calculators->erase(arg->calc_id);
			zmq_close(cur.sock_s_r);
			std::cout<<"OK: "<<arg->calc_id<<" was removed"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
		}
		delete arg;
		return 0;
	}
	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check,("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	arg->visited.push_back(arg->my_id);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont)
				continue;
		zmq_msg_t msg; Message msg_s_r;
		msg_s_r.cmd=21;
		msg_s_r.calc_id=arg->calc_id;
		msg_s_r.port=port_check;
		msg_s_r.kolvo_vis=arg->visited.size();
		for(int j=0; j<msg_s_r.kolvo_vis; ++j)
			msg_s_r.vec_vis[j]=arg->visited[j];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &msg_s_r, sizeof(Message));
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		
		
		zmq_msg_init_size(&msg, sizeof(Message));
		zmq_msg_recv(&msg, sock_check, 0);
		memcpy(&msg_s_r, zmq_msg_data(&msg), sizeof(Message));
		//1 didn't find
		//0 fidn end remove
		//-1 is not available
		if(msg_s_r.cmd==1){
			for(int j=arg->visited.size(); j<msg_s_r.kolvo_vis; ++j)
				arg->visited.push_back(msg_s_r.vec_vis[j]);
		}else if(msg_s_r.cmd==0){
			zmq_close((*arg->calculators)[arg->calc_id].sock_s_r);
			arg->calculators->erase(arg->calc_id);
			zmq_msg_close(&msg);
			std::cout<<"OK: "<<arg->calc_id<<" was removed"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
			zmq_close(sock_check);
			delete arg;
			return 0;
		}else if(msg_s_r.cmd==-1){
			std::cout<<"ERROR: "<<arg->calc_id<<" IS UNAVAILABLE"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
			zmq_msg_close(&msg);
			zmq_close(sock_check);
			delete arg;
			return 0;
		}
	}
	std::cout<<"ERROR:"<<arg->calc_id<<": NOT FOUND"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
	zmq_close(sock_check);
	delete arg;
	return 0;
}

void* try_remove_node(void* a){
	arg_remove *arg=(arg_remove*) a;
	void* sock_ans = zmq_socket(arg->context, ZMQ_PUSH);
	zmq_connect(sock_ans, ("tcp://localhost:"+std::to_string(arg->port)).c_str());
	if(arg->calculators->find(arg->calc_id)!=arg->calculators->end()){
		calculate_node cur=(*arg->calculators)[arg->calc_id];
		zmq_msg_t msg; Message msg_die;
		msg_die.cmd=0;
		zmq_msg_init_size(&msg, sizeof(Message));
		zmq_msg_send(&msg, cur.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, cur.sock_s_r, 0) == -1){
			msg_die.cmd=-1;
			memcpy(zmq_msg_data(&msg), &msg_die, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
		}else{
			arg->calculators->erase(arg->calc_id);
			zmq_close(cur.sock_s_r);
			msg_die.cmd=0;
			memcpy(zmq_msg_data(&msg), &msg_die, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
		}
		delete arg;
		return 0;
	}
	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check,("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	arg->visited.push_back(arg->my_id);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont)
				continue;
		zmq_msg_t msg; Message msg_s_r;
		msg_s_r.cmd=21;
		msg_s_r.calc_id=arg->calc_id;
		msg_s_r.port=port_check;
		msg_s_r.kolvo_vis=arg->visited.size();
		for(int j=0; j<msg_s_r.kolvo_vis; ++j)
			msg_s_r.vec_vis[j]=arg->visited[j];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &msg_s_r, sizeof(Message));
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		
		
		zmq_msg_init_size(&msg, sizeof(Message));
		zmq_msg_recv(&msg, sock_check, 0);
		memcpy(&msg_s_r, zmq_msg_data(&msg), sizeof(Message));
		//1 didn't find
		//0 fidn end remove
		//-1 is not available
		if(msg_s_r.cmd==1){
			for(int j=arg->visited.size(); j<msg_s_r.kolvo_vis; ++j)
				arg->visited.push_back(msg_s_r.vec_vis[j]);
		}else if(msg_s_r.cmd==0){
			zmq_close((*arg->calculators)[arg->calc_id].sock_s_r);
			arg->calculators->erase(arg->calc_id);
			memcpy(zmq_msg_data(&msg), &msg_s_r, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
			zmq_close(sock_check);
			delete arg;
			return 0;
		}else if(msg_s_r.cmd==-1){
			memcpy(zmq_msg_data(&msg), &msg_s_r, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
			zmq_close(sock_check);
			delete arg;
			return 0;
		}
	}
	zmq_msg_t msg; Message msg_not_die;
	msg_not_die.cmd=1;
	zmq_msg_init_size(&msg, sizeof(Message));
	memcpy(zmq_msg_data(&msg), &msg_not_die, sizeof(Message));
	zmq_msg_send(&msg, sock_ans, 0);
	zmq_close(sock_check);
	delete arg;
	return 0;
}

struct arg_exec{
	void* context;
	unsigned port;
	std::map<unsigned, calculate_node> *calculators;
	std::vector<controller_node> *controllers;
	unsigned my_id, calc_id;
	std::vector<unsigned> visited;
	std::vector<int> summary;
};

void* do_exec(void* a){
	arg_exec* arg=(arg_exec*) a;
	if(arg->calculators->find(arg->calc_id)!=arg->calculators->end()){
		calculate_node cur=(*arg->calculators)[arg->calc_id];
		zmq_msg_t msg; Message do_ex;
		do_ex.cmd=2;
		do_ex.kolvo_sum=arg->summary.size();
		for(int i=0; i<do_ex.kolvo_sum; ++i)
			do_ex.vec_sum[i]=arg->summary[i];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
		zmq_msg_send(&msg, cur.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, cur.sock_s_r, 0) == -1){
			std::cout<<"ERROR:"<<arg->calc_id<<": NODE IS UNANVAILABLE"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id <<"\033[0m"<<"]>>"<<std::flush;
		}else{
			memcpy(&do_ex, zmq_msg_data(&msg), sizeof(Message));
			std::cout<<"OK:"<<arg->calc_id<<':'<<do_ex.answer<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id <<"\033[0m"<<"]>>"<<std::flush;
		}
		zmq_msg_close(&msg);
		delete arg; 
		return 0;
	}
	
	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check, ("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER,   &ZERO,      int_s);
	arg->visited.push_back(arg->my_id);
	//sleep(1);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont!=0)
				continue;
		zmq_msg_t msg; Message do_ex;
		do_ex.cmd=31;
		do_ex.calc_id=arg->calc_id;
		do_ex.port=port_check;
		do_ex.kolvo_vis=arg->visited.size();
		for(int j=0; j<do_ex.kolvo_vis; ++j)
			do_ex.vec_vis[j]=arg->visited[j];
		do_ex.kolvo_sum=arg->summary.size();
		for(int j=0; j<do_ex.kolvo_sum; ++j)
			do_ex.vec_sum[j]=arg->summary[j];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		sleep(1);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, sock_check, 0)==-1){
			zmq_msg_close(&msg);
			continue;
		}
		//1 didn't find
		//0 find, have res
		//-1 find, haven't res
		memcpy(&do_ex, zmq_msg_data(&msg), sizeof(Message));
		if(do_ex.cmd==1){
			for(int j=arg->visited.size(); j<do_ex.kolvo_vis; ++j)
				arg->visited.push_back(do_ex.vec_vis[j]);
			zmq_msg_close(&msg);
		}else if(do_ex.cmd==0){
			std::cout<<"OK:"<<arg->calc_id<<':'<<do_ex.answer<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
			zmq_msg_close(&msg);
			zmq_close(sock_check);
			delete arg;
			return 0;
		}else if(do_ex.cmd==-1){
			std::cout<<"ERROR:"<<arg->calc_id<<": NODE IS UNANVAILABLE"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
			zmq_msg_close(&msg);
			zmq_close(sock_check);
			delete arg;
			return 0;
		}
	}
	std::cout<<"ERROR:"<<arg->calc_id<<": NOT FOUND"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
	zmq_close(sock_check);
	delete arg;
	return 0;
}

void* try_exec(void* a){
	arg_exec* arg=(arg_exec*) a;
	void* sock_ans=zmq_socket(arg->context, ZMQ_PUSH);
	zmq_setsockopt(sock_ans, ZMQ_SNDTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_ans, ZMQ_LINGER,   &ZERO,      int_s);
	zmq_connect(sock_ans, ("tcp://localhost:"+std::to_string(arg->port)).c_str());
	if(arg->calculators->find(arg->calc_id)!=arg->calculators->end()){
		calculate_node cur=(*arg->calculators)[arg->calc_id];
		zmq_msg_t msg; Message do_ex;
		do_ex.cmd=2;
		do_ex.kolvo_sum=arg->summary.size();
		for(int i=0; i<do_ex.kolvo_sum; ++i)
			do_ex.vec_sum[i]=arg->summary[i];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
		zmq_msg_send(&msg, cur.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, cur.sock_s_r, 0) == -1){
			do_ex.cmd=-1;
			zmq_msg_init_size(&msg, sizeof(Message));
			memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
		}else{
			memcpy(&do_ex, zmq_msg_data(&msg), sizeof(Message));
			do_ex.cmd=0;
			zmq_msg_init_size(&msg, sizeof(Message));
			memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
		}
		delete arg; 
		return 0;
	}
	
	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check, ("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	arg->visited.push_back(arg->my_id);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont!=0)
				continue;
		zmq_msg_t msg; Message do_ex;
		do_ex.cmd=31;
		do_ex.calc_id=arg->calc_id;
		do_ex.port=port_check;
		do_ex.kolvo_vis=arg->visited.size();
		for(int j=0; j<do_ex.kolvo_vis; ++j)
			do_ex.vec_vis[j]=arg->visited[j];
		do_ex.kolvo_sum=arg->summary.size();
		for(int j=0; j<do_ex.kolvo_sum; ++j)
			do_ex.vec_sum[j]=arg->summary[j];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, sock_check, 0) == -1){
			zmq_msg_close(&msg);
			continue;
		}
		//1 didn't find
		//0 find, have res
		//-1 find, haven't res
		memcpy(&do_ex, zmq_msg_data(&msg), sizeof(Message));
		if(do_ex.cmd==1){
			for(int j=arg->visited.size(); j<do_ex.kolvo_vis; ++j)
				arg->visited.push_back(do_ex.vec_vis[j]);
			zmq_msg_close(&msg);
		}else if(do_ex.cmd==0 || do_ex.cmd==-1){
			memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
			zmq_msg_send(&msg, sock_ans, 0);
			zmq_close(sock_check);
			delete arg;
			return 0;
		}
	}
	zmq_msg_t msg; Message do_ex;
	do_ex.cmd=1;
	do_ex.kolvo_vis=arg->visited.size(); 
	for(int i=0; i<do_ex.kolvo_vis; ++i)
		do_ex.vec_vis[i]=arg->visited[i];
	zmq_msg_init_size(&msg, sizeof(Message));
	memcpy(zmq_msg_data(&msg), &do_ex, sizeof(Message));
	zmq_msg_send(&msg, sock_ans, 0);
	zmq_close(sock_check);
	delete arg;
	return 0;
}

struct arg_ping{
	void* context;
	unsigned my_id;
	std::map<unsigned, calculate_node> *calculators;
	std::vector<controller_node> *controllers;
	unsigned port;
	std::vector<unsigned> visited;
};

void* do_ping(void* a){
	arg_ping *arg=(arg_ping*) a;
	std::vector<unsigned> died;
	for(const std::pair<unsigned,calculate_node>/*auto*/ &elem : *arg->calculators){
		zmq_msg_t msg; Message check;
		check.cmd=1;
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &check, sizeof(Message));
		zmq_msg_send(&msg, elem.second.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, elem.second.sock_s_r, 0) == -1){
			died.push_back(elem.first);
		}
		zmq_msg_close(&msg);
	}

	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check, ("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	arg->visited.push_back(arg->my_id);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont)
				continue;
		zmq_msg_t msg; Message ping;
		ping.cmd=51;
		ping.kolvo_vis=arg->visited.size();
		ping.port=port_check;
		for(int j=0; j<ping.kolvo_vis; ++j)
			ping.vec_vis[j]=arg->visited[j];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &ping, sizeof(Message));	
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, sock_check, 0) == -1){
			zmq_msg_close(&msg);
			continue;
		}
		memcpy(&ping, zmq_msg_data(&msg), sizeof(Message));	
		for(int j=arg->visited.size(); j<ping.kolvo_vis; ++j)
			arg->visited.push_back(ping.vec_vis[j]);
		for(int j=0; j<ping.kolvo_died; ++j)
			died.push_back(ping.vec_died[j]);
		zmq_msg_close(&msg);
	}
	zmq_close(sock_check);
	if(died.size()==0){
		std::cout<<"OK: -1"<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;		
	}else{
		std::cout<<"OK: "<<died[0];
		for(int i=1; i<died.size(); ++i)
			std::cout<<';'<<died[i];
		std::cout<<std::endl<<'['<<"\033[1m\033[32m"<<arg->my_id<<"\033[0m"<<"]>>"<<std::flush;
	}
	delete arg;
	return 0;
}

void* try_ping(void* a){
	arg_ping *arg=(arg_ping*) a;
	std::vector<unsigned> died;
	for(const std::pair<unsigned,calculate_node>/*auto*/ &elem : *arg->calculators){
		zmq_msg_t msg; Message check;
		check.cmd=1;
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &check, sizeof(Message));
		zmq_msg_send(&msg, elem.second.sock_s_r, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, elem.second.sock_s_r, 0) == -1){
			died.push_back(elem.first);
		}
		zmq_msg_close(&msg);
	}
	void* sock_check=zmq_socket(arg->context, ZMQ_PULL);
	unsigned port_check=0;
	while(zmq_bind(sock_check, ("tcp://*:"+std::to_string(++port_check)).c_str()) !=0);
	zmq_setsockopt(sock_check, ZMQ_RCVTIMEO, &PING_TIME_CNTR, int_s);
	zmq_setsockopt(sock_check, ZMQ_LINGER, &ZERO, int_s);
	arg->visited.push_back(arg->my_id);
	for(int i=0; i<arg->controllers->size(); ++i){
		int cont=0;
		for(int j=0; j<arg->visited.size(); ++j)
			if((*arg->controllers)[i].id==arg->visited[j])
				cont+=1;
			if(cont)
				continue;
		zmq_msg_t msg; Message ping;
		ping.cmd=51;
		ping.kolvo_vis=arg->visited.size();
		ping.port=port_check;
		for(int j=0; j<ping.kolvo_vis; ++j)
			ping.vec_vis[j]=arg->visited[j];
		zmq_msg_init_size(&msg, sizeof(Message));
		memcpy(zmq_msg_data(&msg), &ping, sizeof(Message));	
		zmq_msg_send(&msg, (*arg->controllers)[i].sock_send, 0);
		
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, sock_check, 0) == -1){
			zmq_msg_close(&msg);
			continue;
		}
		memcpy(&ping, zmq_msg_data(&msg), sizeof(Message));	
		for(int j=arg->visited.size(); j<ping.kolvo_vis; ++j)
			arg->visited.push_back(ping.vec_vis[j]);
		for(int j=0; j<ping.kolvo_died; ++j)
			died.push_back(ping.vec_died[j]);
		zmq_msg_close(&msg);
	}
	zmq_close(sock_check);
	void* sock_ans=zmq_socket(arg->context, ZMQ_PUSH);
	zmq_connect(sock_ans, ("tcp://localhost:"+std::to_string(arg->port)).c_str());
	zmq_msg_t msg; Message msgg;
	msgg.kolvo_died=died.size();
	for(int i=0; i<msgg.kolvo_died; ++i)
		msgg.vec_died[i]=died[i];
	msgg.kolvo_vis=arg->visited.size();
	for(int i=0; i<msgg.kolvo_vis; ++i)
		msgg.vec_vis[i]=arg->visited[i];
	zmq_msg_init_size(&msg, sizeof(Message));
	memcpy(zmq_msg_data(&msg), &msgg, sizeof(Message));
	zmq_msg_send(&msg, sock_ans, 0);
	delete arg;
	return 0;
}



int main(int argc, char* argv[]){
	if(argc!=2){
		std::cout<<"WRONG NUMBER OF ARGUMENTS"<<std::endl;
		return -1;
	}
	//pipe for ui -> controller
	pid_t pid_cntrl = fork();
	if(pid_cntrl == -1){
		std::cout<<"FORK ERROR IN CNTRL "<<argv[1]<<" : "<<errno<<std::endl;
		return -2;
	}
	if(pid_cntrl>0){
		sleep(0.1);
		void* context=zmq_ctx_new();
		void* pipe_push=zmq_socket(context, ZMQ_PUSH);
		if(zmq_connect(pipe_push, (std::string("tcp://localhost:")+argv[1]).c_str())!=0){
			std::cout<<"ERROR: PORT "<<argv[1]<<"IS NOT AVAILABLE"<<std::endl;
			return -3;
		}
		const unsigned my_id=atoi(argv[1]);
		std::string cmd;
		while(true){
			std::cout<<'['<<"\033[1m\033[32m"<<my_id<<"\033[0m"<<"]>>";
			zmq_msg_t msg; Message msg_send;
			std::cin>>cmd;
			if(cmd=="create"){
				msg_send.cmd=1;
				int t;
				std::cin>>msg_send.calc_id>>t;
				zmq_msg_init_size(&msg, sizeof(Message));
				memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
				zmq_msg_send(&msg, pipe_push, 0);
			}else if(cmd=="remove"){
				msg_send.cmd=2;
				std::cin>>msg_send.calc_id;
				zmq_msg_init_size(&msg, sizeof(Message));
				memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
				zmq_msg_send(&msg, pipe_push, 0);
			}else if(cmd=="exec"){
				msg_send.cmd=3;
				std::cin>>msg_send.calc_id>>msg_send.kolvo_sum;
				for(int i=0; i<msg_send.kolvo_sum; ++i){
					std::cin>>msg_send.vec_sum[i];
				}
				zmq_msg_init_size(&msg, sizeof(Message));
				memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
				zmq_msg_send(&msg, pipe_push, 0);
			}else if(cmd=="union"){
				msg_send.cmd=4;
				std::cin>>msg_send.cntr_id;
				zmq_msg_init_size(&msg, sizeof(Message));
				memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
				zmq_msg_send(&msg, pipe_push, 0);
			}else if(cmd=="pingall"){
				msg_send.cmd=5;
				zmq_msg_init_size(&msg, sizeof(Message));
				memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
				zmq_msg_send(&msg, pipe_push, 0);
			}else if(cmd=="exit"){
				msg_send.cmd=0;
				zmq_msg_init_size(&msg, sizeof(Message));
				memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
				zmq_msg_send(&msg, pipe_push, 0);
				//zmq_msg_close(&msg);
				break;
			}else{
				std::cout<<"WRONG COMMAND"<<std::endl;
			}
		}
		zmq_close(pipe_push);
		zmq_close(context);
		zmq_ctx_destroy(context);
		int k; wait(&k);
	}else{
		void* context= zmq_ctx_new();
		void* pipe_pull=zmq_socket(context, ZMQ_PULL);
		if(zmq_bind(pipe_pull,(std::string("tcp://*:")+argv[1]).c_str())!=0){
			std::cout<<"ERROR: CANT BIND PORT "<<argv[1]<<std::endl;
			return -3;
		}
		const unsigned my_id=atoi(argv[1]);
		std::map<unsigned, calculate_node> calculators;
		std::vector<controller_node> controllers;
		zmq_msg_t msg; Message msg_recv;
		zmq_msg_init_size(&msg, sizeof(Message));
		while(true){
			zmq_msg_recv(&msg, pipe_pull, 0);
			memcpy(&msg_recv, zmq_msg_data(&msg), sizeof(Message));
			if(msg_recv.cmd==1){
				arg_create *cur=new arg_create;
				cur->my_id=my_id;
				cur->context=context;
				cur->calc_id=msg_recv.calc_id;
				cur->calculators=&calculators;
				cur->controllers=&controllers;
				pthread_t *creating_thread=new pthread_t;
				pthread_create(creating_thread, NULL, create_calc_node, (void*) cur);
				pthread_detach(*creating_thread);
			}else if(msg_recv.cmd==2){
				arg_remove* cur=new arg_remove;
				cur->my_id=my_id;
				cur->calc_id=msg_recv.calc_id;
				cur->context=context;
				cur->calculators=&calculators;
				cur->controllers=&controllers;
				pthread_t *removing_thread=new pthread_t;
				pthread_create(removing_thread, NULL, remove_node, (void*) cur);
				pthread_detach(*removing_thread);
			}else if(msg_recv.cmd==3){
				arg_exec *cur = new arg_exec;
				cur->my_id=my_id;
				cur->calc_id=msg_recv.calc_id;
				cur->context=context;
				cur->calculators=&calculators;
				cur->controllers=&controllers;
				for(int i=0; i<msg_recv.kolvo_sum; ++i)
					cur->summary.push_back(msg_recv.vec_sum[i]);
				pthread_t* exec_thread=new pthread_t;
				pthread_create(exec_thread, NULL, do_exec,(void*) cur );
				pthread_detach(*exec_thread); 
			}else if(msg_recv.cmd==4){
				arg_union *cur=new arg_union;
				cur->context=context;
				cur->my_id=my_id;
				cur->target_id=msg_recv.cntr_id;
				cur->controllers=&controllers;
				pthread_t *unioning_thread=new pthread_t;
				pthread_create(unioning_thread, NULL, do_union, (void*) cur);
				pthread_detach(*unioning_thread);
			}else if(msg_recv.cmd==5){
				arg_ping *cur=new arg_ping;
				cur->my_id=my_id;
				cur->context=context;
				cur->calculators=&calculators;
				cur->controllers=&controllers;
				pthread_t *ping_thread=new pthread_t;
				pthread_create(ping_thread, NULL, do_ping, (void*) cur);
				pthread_detach(*ping_thread);
			}else if(msg_recv.cmd==0){
				break;
			}else if(msg_recv.cmd==11){
				arg_create *cur = new arg_create;
				cur->my_id=my_id;
				cur->context=context;
				cur->port=msg_recv.port;
				cur->calc_id=msg_recv.calc_id;
				for(int i=0; i<msg_recv.kolvo_vis; ++i)
					cur->visited.push_back(msg_recv.vec_vis[i]);
				cur->calculators=&calculators;
				cur->controllers=&controllers;
				pthread_t *creating_thread=new pthread_t;
				pthread_create(creating_thread, NULL, check_create_calc_node, (void*) cur);
				pthread_detach(*creating_thread);
			}else if(msg_recv.cmd==21){
				arg_remove *cur=new arg_remove;
				cur->my_id=my_id;
				cur->context=context;
				cur->controllers=&controllers;
				cur->calculators=&calculators;
				cur->port=msg_recv.port;
				cur->calc_id=msg_recv.calc_id;
				for(int i=0; i<msg_recv.kolvo_vis; ++i)
					cur->visited.push_back(msg_recv.vec_vis[i]);
				pthread_t *removing_thread = new pthread_t;
				pthread_create(removing_thread, NULL, try_remove_node, (void*) cur);
				pthread_detach(*removing_thread);
			}else if(msg_recv.cmd==31){
				arg_exec *cur = new arg_exec;
				cur->my_id=my_id;
				cur->calc_id=msg_recv.calc_id;
				cur->port=msg_recv.port;
				cur->context=context;
				cur->calculators=&calculators;
				cur->controllers=&controllers;
				for(int i=0; i<msg_recv.kolvo_sum; ++i)
					cur->summary.push_back(msg_recv.vec_sum[i]);
				for(int i=0; i<msg_recv.kolvo_vis; ++i)
					cur->visited.push_back(msg_recv.vec_vis[i]);
				pthread_t* exec_thread=new pthread_t;
				pthread_create(exec_thread, NULL, try_exec,(void*) cur );
				pthread_detach(*exec_thread); 
			}else if(msg_recv.cmd==41){
				arg_union *cur=new arg_union;
				cur->context=context;
				cur->my_id=my_id;
				cur->target_id=msg_recv.cntr_id;
				cur->controllers=&controllers;
				cur->port=msg_recv.port;
				pthread_t *unioning_thread=new pthread_t;
				pthread_create(unioning_thread, NULL, answerd_union, (void*) cur);
				pthread_detach(*unioning_thread);
			}else if(msg_recv.cmd==51){
				arg_ping *cur=new arg_ping;
				cur->my_id=my_id;
				cur->context=context;
				cur->calculators=&calculators;
				cur->controllers=&controllers;
				cur->port=msg_recv.port;
				for(int i=0; i<msg_recv.kolvo_vis; ++i)
					cur->visited.push_back(msg_recv.vec_vis[i]);
				pthread_t *ping_thread=new pthread_t;
				pthread_create(ping_thread, NULL, try_ping, (void*) cur);
				pthread_detach(*ping_thread);
			}else if(msg_recv.cmd==6){
				for(int i=0; i<controllers.size(); ++i)
					if(controllers[i].id==msg_recv.cntr_id){
						zmq_close(controllers[i].sock_send);
						controllers.erase(controllers.begin()+i);
					}
			} 
		}
		for(int i=0; i<controllers.size(); ++i){
			zmq_msg_t msg; Message kill_me;
			kill_me.cmd=6;
			kill_me.cntr_id=my_id;
			zmq_msg_init_size(&msg, sizeof(Message));\
			memcpy(zmq_msg_data(&msg), &kill_me, sizeof(Message));
			zmq_msg_send(&msg, controllers[i].sock_send, 0);
			zmq_close(controllers[i].sock_send);
		}
		for(std::pair<unsigned, calculate_node> elem : calculators){
			zmq_msg_t msg; Message kill_me;
			kill_me.cmd=0;
			kill_me.cntr_id=my_id;
			zmq_msg_init_size(&msg, sizeof(Message));
			zmq_msg_send(&msg, elem.second.sock_s_r, 0);
			zmq_msg_init_size(&msg, sizeof(Message));
			zmq_msg_recv(&msg, elem.second.sock_s_r, 0);
			zmq_msg_close(&msg);
			zmq_close(elem.second.sock_s_r);
		}
		zmq_close(pipe_pull);
	//	std::cout<<"close pipe"<<std::endl;
		//zmq_ctx_destroy(context);  <- idk why it blocks after some one from union exits 
	}
	return 0;	
}
