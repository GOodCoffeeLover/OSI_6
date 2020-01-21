#include<zmq.h>
#include<iostream>
#include<sys/wait.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<map>
#include<cstring>
struct calc{
	pid_t pid;
	void *SR, *SW;  
};

struct cntr{
	unsigned int id;
	void *send_port; 
};

struct Message{
	int cmd;
	unsigned int calc_id, cntr_id;
	unsigned int vec_vis[50], kolvo_vis;
	char port_adress[30];
	int vec_sum[100], kolvo_sum;
};

struct crtng_arg{
	std::map<int, calc> *clcs;
	std::string adrs;
	std::vector<cntr> *neig;
	std::vector<unsigned int> *visited; //
	unsigned int id, my_id, his_id;//создаём id, запрос либо от my_id, либо от his_id
	void* contex;
};

struct rmvng_arg{
	std::map<int, calc> *clcs;
	std::vector<cntr> *neig;
	std::vector<unsigned int> *visited; //
	unsigned int id, my_id, his_id;//создаём id, запрос либо от my_id, либо от his_id
	void* contex;
};

struct union_arg{
	std::vector<cntr> *neig;
	std::string adress;
	unsigned int target, my_id;
	void* contex;
};
bool operator ==(const cntr& a, const cntr& b){
	return a.id == b.id;
}

template<class t>
bool member(t& elem, const std::vector<t> &vec){
	for(int i=0; i<vec.size(); ++i)
		if(vec[i]==elem) 
			return true;
	return false;
}

void *create_check(void* a){
	crtng_arg *arg=(crtng_arg*)a;
	void* send_ans=zmq_socket(arg->contex, ZMQ_PUSH);
	zmq_connect(send_ans,arg->adrs.c_str());
	Message msg_cmd; zmq_msg_t msg;
	if(arg->clcs->find(arg->id)!=arg->clcs->end()){
		//узел найден, проверить на живучесть
		calc cur=(*(arg->clcs))[arg->id];
		zmq_msg_init_size(&msg, sizeof(Message));
		msg_cmd.cmd=12;
		memcpy(zmq_msg_data(&msg), &msg_cmd, sizeof(Message));
		zmq_msg_send( &msg,cur.SW, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv( &msg,cur.SR, 0) == EAGAIN){
			msg_cmd.cmd=-2;
		}else{
			memcpy(&msg_cmd, zmq_msg_data(&msg), 0);
			if(msg_cmd.cmd!=0){
				msg_cmd.cmd=-2;
			}else{
				msg_cmd.cmd=-1;
			}	
		} 
		memcpy(zmq_msg_data(&msg),&msg_cmd, sizeof(Message));
		zmq_msg_send(&msg, send_ans, 0);
		zmq_close(send_ans);
		delete arg->visited;
		delete arg;
		return 0;
	}
	void* recv_ans = zmq_socket(arg->contex, ZMQ_PULL);
	int i=0;
	while((i<=10'000) && zmq_bind(recv_ans, (std::string("tcp://127.0.0.1:")+std::to_string(++i)).c_str())!=0);
	std::string adress=std::string("tcp://127.0.0.1:")+std::to_string(i);
	for(int i=0; i<arg->neig->size(); ++i){
			zmq_msg_init_size(&msg, sizeof(Message));
			if(member((*(arg->neig))[i].id, *arg->visited)) 
				continue;
			msg_cmd.cmd=11;
			msg_cmd.kolvo_vis=arg->visited->size();
			for(int j=0; j<arg->visited->size(); ++j){
				msg_cmd.vec_vis[j]=(*(arg->visited))[j] ;
			}
			msg_cmd.calc_id=arg->id;
			msg_cmd.cntr_id=arg->my_id;
			int j=0;
			for(j=0; j<adress.size(); ++j)
				msg_cmd.port_adress[j]=adress[j];
			msg_cmd.port_adress[j]='\0';
			memcpy(&msg_cmd, zmq_msg_data(&msg), sizeof(Message));
			zmq_msg_send(&msg, (*(arg->neig))[i].send_port, 0);
//====================================================================
			zmq_msg_init_size(&msg, sizeof(Message));
			zmq_msg_recv( &msg,recv_ans, 0);
			memcpy(&msg_cmd, zmq_msg_data(&msg), sizeof(Message));
			if(msg_cmd.cmd>0){
				for( int j=0; j<msg_cmd.cmd; ++j){
					if(j>arg->visited->size()){
						arg->visited->push_back(msg_cmd.vec_vis[j]);
					}
				}
			}else if(msg_cmd.cmd!=0){ // -1 or -2
				memcpy(zmq_msg_data(&msg), &msg_cmd, sizeof(Message));
				zmq_msg_send(&msg, send_ans, 0);
				zmq_close(send_ans);
				zmq_close(recv_ans);
				delete arg->visited;
				delete arg;
				return 0;
			}
	}
	zmq_msg_init_size(&msg, sizeof(Message));
	msg_cmd.cmd=0;
	memcpy(zmq_msg_data(&msg), &msg_cmd, sizeof(Message));
	zmq_msg_send(&msg, send_ans, 0);
	zmq_close(recv_ans);
	zmq_close(send_ans);
	delete arg->visited;
	delete arg;
	return 0;
}

void* create_node(void* a){
	crtng_arg *arg=(crtng_arg*)a;
	Message msg_cmd; zmq_msg_t msg;
	zmq_msg_init_size(&msg, sizeof(Message));
	if(arg->clcs->find(arg->id)!=arg->clcs->end()){
		//узел найден, проверить на живучесть
		calc cur=(*(arg->clcs))[arg->id];
		msg_cmd.cmd=12;
		memcpy(zmq_msg_data(&msg), &msg_cmd, sizeof(Message));
		zmq_msg_send(&msg, cur.SW, 0);
		zmq_msg_init_size(&msg, sizeof(Message));
		if(zmq_msg_recv(&msg, cur.SR, 0) == EAGAIN){
			std::cout<<"ERROR: ALREADY EXISTS, BUT IS NOT AVAILABLE"<<std::endl;
		}else{
			memcpy(&msg_cmd, zmq_msg_data(&msg), sizeof(Message));
			if(msg_cmd.cmd!=0){
				std::cout<<"ERROR: ALREADY EXISTS, BUT IS NOT AVAILABLE"<<std::endl;
			}else{
				std::cout<<"ERROR: ALREADY EXISTS"<<std::endl;
			}
		} 
		zmq_msg_close(&msg);
		delete arg;
		return 0;
	}
	std::vector<unsigned> visited; visited.push_back(arg->my_id);
	void* recv_ans = zmq_socket(arg->contex, ZMQ_PULL);
	int i=0;
	while((i<=10'000) && zmq_bind(recv_ans, (std::string("tcp://127.0.0.1:")+std::to_string(++i)).c_str())!=0);
	std::string adress=std::string("tcp://127.0.0.1:")+std::to_string(i);
	for(i=0; i<adress.size(); ++i){
		msg_cmd.port_adress[i]=adress[i];	
	}
	for(int i=0; i<arg->neig->size(); ++i){
			if(member((*(arg->neig))[i].id, visited)) 
				continue;
			msg_cmd.cmd=11;
			msg_cmd.kolvo_vis=visited.size();
			for(int j=0; j<visited.size(); ++j){
				msg_cmd.vec_vis[j]=visited[j];
			}
			msg_cmd.calc_id=arg->id;
			msg_cmd.cntr_id=arg->my_id;
			int j=0;
			for(j=0; j<adress.size(); ++j)
				msg_cmd.port_adress[j]=adress[j];
			msg_cmd.port_adress[j]='\0';
			memcpy(&msg_cmd, zmq_msg_data(&msg), sizeof(Message));
			zmq_msg_send(&msg, (*(arg->neig))[i].send_port, 0);
//====================================================================
			zmq_msg_init_size(&msg, sizeof(Message));
			zmq_msg_recv( &msg,recv_ans, 0);
			memcpy(&msg_cmd, zmq_msg_data(&msg), sizeof(Message));
			if(msg_cmd.cmd>0){
				for( int j=0; j<msg_cmd.cmd; ++j){
					if(j>visited.size()){
						visited.push_back(msg_cmd.vec_vis[j]);
					}
				}
			}else if(msg_cmd.cmd==-1){
				std::cout<<"ERROR: ALREADY EXISTS"<<std::endl;
				zmq_msg_close(&msg);
				zmq_close(recv_ans);
				delete arg;
				return 0;
			}else if(msg_cmd.cmd==-2){
				std::cout<<"ERROR: ALREADY EXISTS, BUT IS NOT AVAILABLE"<<std::endl;
				zmq_msg_close(&msg);
				zmq_close(recv_ans);
				delete arg;
				return 0;
			}			
	}
	zmq_close(recv_ans);
	void* sread=zmq_socket(arg->contex, ZMQ_PULL);
	void* swrite=zmq_socket(arg->contex, ZMQ_PUSH);
	int pingtime=10, zero=0;
	size_t int_s=sizeof(int);
	zmq_getsockopt(sread, ZMQ_RCVTIMEO,&pingtime, &int_s);
	zmq_getsockopt(sread, ZMQ_LINGER,&zero, &int_s);
	zmq_getsockopt(swrite, ZMQ_SNDTIMEO,&pingtime, &int_s);
	zmq_getsockopt(swrite, ZMQ_LINGER,&zero,&int_s);
	i=0;
	while((i<=10'000) && zmq_bind(sread, (std::string("tcp://127.0.0.1:")+std::to_string(++i)).c_str())!=0);
	std::string rport=std::string("tcp://127.0.0.1:")+std::to_string(i);
	while((i<=10'000) && zmq_bind(swrite, (std::string("tcp://127.0.0.1:")+std::to_string(++i)).c_str())!=0);
	std::string wport=std::string("tcp://127.0.0.1:")+std::to_string(i);
	pid_t frk=fork();
	if(frk<0){
		std::cout<<"ERROR: CANT DO FORK FOR CALCULATOR "<<arg->id<<std::endl;
		delete arg;
		return 0;	
	}
	if(frk>0){
		std::cout<<"OK: "<<frk<<std::endl;
		(*(arg->clcs))[arg->id]=calc{frk, sread, swrite};
		delete arg;
		return 0;
	}else{
		execl("./calculator", "./calculator", rport.c_str(), wport.c_str(), NULL);
		exit(-1);
	}
//	std::cout<<"exit"
	return 0;
}
void* do_union(void* a){
	union_arg *arg=(union_arg*)a;
	cntr targ { arg->target, 0};
	if(member(targ, *(arg->neig))){
		std::cout<<"ERROR : ALREDY UNION WITH "<<arg->target<<std::endl;
		delete arg;
		return (void* )-1;
	}
	void* sock=zmq_socket(arg->contex, ZMQ_PUSH);
	if(zmq_bind(sock, (std::string("tcp://127.0.0.1:")+std::to_string(arg->target)).c_str())==0 ){
		std::cout<<"ERROR: "<<arg->target<<	" IS NOT EXSISTS"<<std::endl;
		zmq_close(sock);
		return 0;		
	}
	if( zmq_connect(sock, (std::string("tcp://127.0.0.1:")+std::to_string(arg->target)).c_str()) ==-1 ){
		std::cout<<"ERROR: CAN'T CONNECT WITH "<<arg->target<<' '<<errno<<std::endl;
		delete arg;
		zmq_close(sock);
		return 0;
	}
	char port[30];
	size_t port_s=sizeof(char)*30;
	zmq_getsockopt(sock, ZMQ_LAST_ENDPOINT, port, &port_s);
	std::cout<<"sock endpoint = "<<port<<std::endl;
	int pingtime=10, zero=0;
	size_t int_s=sizeof(int);
	zmq_getsockopt(sock, ZMQ_SNDTIMEO, &pingtime, &int_s);
	zmq_getsockopt(sock, ZMQ_LINGER, &zero, &int_s);
	void* sock_check=zmq_socket(arg->contex, ZMQ_PULL);
	zmq_getsockopt(sock_check, ZMQ_RCVTIMEO,&pingtime, &int_s);
	Message msg_cmd; zmq_msg_t msg;
	zmq_msg_init_size(&msg, sizeof(Message));
	std::cout<<"try make port"<<std::endl;
	int i=0;
	while((i<=10'000) && (zmq_bind(sock_check, (std::string("tcp://*:")+std::to_string(++i)).c_str())!=0));
	std::string adress=(std::string("tcp://*:")+std::to_string(i));
	zmq_getsockopt(sock_check, ZMQ_LAST_ENDPOINT, msg_cmd.port_adress, &port_s);
	/*for(i=0; i<adress.size(); ++i){
		msg_cmd.port_adress[i]=adress[i];	
	}
	msg_cmd.port_adress[i]='\0';*/
	std::cout<<"MAKE PORT "<<msg_cmd.port_adress<<std::endl;
	msg_cmd.cntr_id=arg->my_id;
	msg_cmd.cmd=41;
	memcpy(zmq_msg_data(&msg), &msg_cmd, sizeof(Message));
	zmq_msg_send(&msg, sock, 0);
	std::cout<<zmq_msg_send(&msg, sock, 0)<<"send msg"<<std::endl;
	zmq_msg_init_size(&msg, sizeof(Message));
	if(zmq_msg_recv(&msg, sock_check, 0) == EAGAIN){
		std::cout<<"ERROR: CONTROLLER "<<arg->target<<"IS NOT AVAILABLE"<<std::endl;
		zmq_close(sock_check);
		zmq_close(sock);
		zmq_msg_close(&msg);
	}
	std::cout<<"recv msg"<<std::endl;
	zmq_msg_close(&msg);
	zmq_close(sock_check);
	(arg->neig)->push_back(cntr{arg->target, sock});
	std::cout<<"OK: "<<arg->target<<std::endl;
	delete arg;
	return (void*) 0;
}

void* do_union1(void* a){
	std::cout<<"union 1"<<std::endl;
	union_arg *arg=(union_arg*)a;
	void* sock=zmq_socket(arg->contex, ZMQ_PUSH);
	zmq_connect(sock, arg->adress.c_str());
	zmq_msg_t msg; Message my_msg; my_msg.cmd=0;
	zmq_msg_init_size(&msg, sizeof(Message));
	memcpy(zmq_msg_data(&msg), &my_msg, sizeof(Message));
	zmq_msg_send(&msg, sock, 0);
	zmq_close(sock);
	zmq_close(&msg);
	sock=zmq_socket(arg->contex, ZMQ_PUSH);
	zmq_connect(sock, (std::string("tcp://127.0.0.1:")+std::to_string(arg->target)).c_str());
	int pingtime=10, zero=0;
	size_t int_s=sizeof(int);
	zmq_getsockopt(sock, ZMQ_SNDTIMEO,&pingtime, &int_s);
	zmq_getsockopt(sock, ZMQ_LINGER,&zero,&int_s);
	(arg->neig)->push_back(cntr{arg->target, sock});
	delete arg;
	return (void*) 0;
}


int main(int argc, char* argv[]){
	if(argc!=2){
		std::cout<<"TOO FEW ARGUMENTS"<<std::endl;
		return -1;
	}
	//pipe for ui -> controller
	pid_t pid_cntrl = fork();
	if(pid_cntrl == -1){
		std::cout<<"FORK ERROR IN CNTRL "<<argv[1]<<" : "<<errno<<std::endl;
		return -2;
	}
	if(pid_cntrl>0){
		//ui
		void* contex= zmq_ctx_new();
		if(contex==NULL){
			std::cout<<"CONTEX ERROR: "<<errno<<std::endl;
			return-3;
		}
		void* pipe_in = zmq_socket(contex, ZMQ_PUSH);
		if( zmq_bind(
			  pipe_in,
			  (std::string("tcp://*:") + argv[1]).c_str()
			) 
		!=0){
			std::cout<<"PORT "<<(std::string("tcp://*:") + argv[1])<<" IS NOT AVAILABLE"<<std::endl;
			return -4;
		}
		int pingtime=10, zero=0;
		size_t int_s=sizeof(int);
		zmq_getsockopt(pipe_in, ZMQ_SNDTIMEO, &pingtime, &int_s);
		zmq_getsockopt(pipe_in, ZMQ_LINGER, &zero, &int_s);
		//interface
		zmq_msg_t msg; 
		std::string buf;
		
		while(true){
			zmq_msg_init_size(&msg, sizeof(Message));
			Message msg_send;
			std::cout<<"("<<argv[1]<<")>";
			std::cin>>buf;
			if(buf=="create"){
				msg_send.cmd=1;
				std::cin>>msg_send.calc_id>>msg_send.cntr_id;//cntr- фиктивный, не используется
			}else if(buf=="remove"){
				msg_send.cmd=2;
				std::cin>>msg_send.calc_id;
			}else if(buf=="exec"){
				msg_send.cmd=3;
				std::cin>>msg_send.calc_id>>msg_send.kolvo_sum;
				for(int i=0; i<msg_send.kolvo_sum; ++i){ 
					if(i%100==0 && i!=0){
						memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
						zmq_msg_send(&msg, pipe_in, ZMQ_SNDMORE);
					}
					std::cin>>msg_send.vec_sum[i%100];
				}
			}else if(buf=="union"){
				msg_send.cmd=4;
				std::cin>>msg_send.cntr_id;
			}else if(buf=="pingall"){
				msg_send.cmd=5;
			}else if(buf=="exit"){
				msg_send.cmd=0;
				memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
				zmq_msg_send(&msg, pipe_in, 0);
				break;
			}else{
				std::cout<<"wrong comand"<<std::endl;
				continue;
			}
			memcpy(zmq_msg_data(&msg), &msg_send, sizeof(Message));
			zmq_msg_send(&msg, pipe_in, 0);
		}
		int k;
		waitpid(pid_cntrl, &k, 0);
		zmq_msg_close(&msg);
		zmq_close(pipe_in);
		zmq_close(contex);
		return 0;
	}else{
		//cntrl
		void* contex = zmq_ctx_new();
		if(contex==NULL){
			std::cout<<"CONTEX ERROR: "<<errno<<std::endl;
			return-3;
		}
		void* pipe_out = zmq_socket(contex, ZMQ_PULL);
		if(zmq_connect(
			pipe_out,
			(std::string("tcp://127.0.0.1:") + argv[1]).c_str())
		!=0){
			std::cout<<"PORT "<<(std::string("tcp://127.0.0.1:") + argv[1])<<" IS NOT AVAILABLE"<<std::endl;
			return -4;
		}
		int pingtime=10, zero=0;
		size_t int_s=sizeof(int);
		zmq_getsockopt(pipe_out, ZMQ_RCVTIMEO, &pingtime, &int_s);
		zmq_getsockopt(pipe_out, ZMQ_LINGER, &zero, &int_s);
		const unsigned int my_id=atoi(argv[1]);
		std::map<int, calc> calcs; //вычислительные
		std::vector<cntr> cntrls;
		zmq_msg_t msg; Message msg_recv;
		zmq_msg_init_size(&msg, sizeof(Message));
		while(true){
			if( -1 == zmq_msg_recv(&msg, pipe_out, 0)){
				std::cout<<"error:'(";
				continue;
			}
			std::cout<<"msg recv"<<std::endl;
			memcpy(&msg_recv,zmq_msg_data(&msg), sizeof(Message));
			if(msg_recv.cmd==1){//create
				crtng_arg *args = new crtng_arg;
				args->clcs=&calcs;
				args->neig= &cntrls;
				args->id=msg_recv.calc_id;
				args->my_id=my_id;
				args->contex=contex;
				pthread_t *crtng_th = new pthread_t;
				pthread_create(crtng_th, NULL,  create_node, args);
				pthread_detach(*crtng_th);
			}else if(msg_recv.cmd==2){//remove
				rmvng_arg *args = new rmvng_arg;
				args->clcs=&calcs;
				args->neig= &cntrls;
				args->id=msg_recv.calc_id;
				args->my_id=my_id;
				args->contex=contex;
				pthread_t *rmvng_th = new pthread_t;
				pthread_create(rmvng_th, NULL,  create_node, args);
				pthread_detach(*rmvng_th);
				
			}else if(msg_recv.cmd==3){//exec

			}else if(msg_recv.cmd==4){//union
		//		std::cout<<"come in menu union"<<std::endl;
				if(msg_recv.cntr_id==my_id){
					std::cout<<"ERROR: ITS MY ID"<<std::endl;
					continue;
				}
				union_arg *cur = new union_arg;
				cur->neig = &cntrls;
				cur->target = msg_recv.cntr_id;
				cur->my_id = my_id;
				cur->contex=contex;
				pthread_t *union_th = new pthread_t;
				std::cout<<"union"<<std::endl;
				pthread_create(union_th, NULL, do_union, (void*) cur);
				pthread_detach(*union_th);
			}else if(msg_recv.cmd==5){//ping

			}else if(msg_recv.cmd==41){//union1
				union_arg *cur = new union_arg;
				cur->neig = &cntrls;
				cur->target = msg_recv.cntr_id;
				cur->my_id = my_id;
				cur->contex=contex;
				cur->adress=msg_recv.port_adress;
				pthread_t *union_th = new pthread_t;
				std::cout<<"union1"<<std::endl;
				pthread_create(union_th, NULL, do_union1, (void*) cur);
				pthread_detach(*union_th);
			}else if(msg_recv.cmd==11){//create1
				crtng_arg *args = new crtng_arg;
				args->clcs=&calcs;
				args->neig= &cntrls;
				args->his_id=msg_recv.cntr_id;
				args->id=msg_recv.calc_id;
				args->my_id=my_id;
				args->contex=contex;
				args->adrs=msg_recv.port_adress;
				args->visited=new std::vector<unsigned int>;
				for(int i=0;i<msg_recv.kolvo_vis; ++i){
					args->visited->push_back(msg_recv.vec_vis[i]);
				}
				args->visited->push_back(my_id);
				pthread_t *crtng_th = new pthread_t;
				pthread_create(crtng_th, NULL,  create_check, args);
				pthread_detach(*crtng_th);
			}else if(msg_recv.cmd==0){ //exit
				//std::cout<<"try exit"<<std::endl;
				break;
			}else{
				std::cout<<"wrong message"<<std::endl;
			}
			//std::cout<<"end of if"<<std::endl;
		}
		//todo: close all sock with all calcs and cntrs 
		zmq_msg_close(&msg);
		zmq_close(pipe_out);
		zmq_close(contex);
		std::cout<<"close contex"<<std::endl;
	}
	return 0;
}
