#include<zmq.h>
#include<iostream>
#include<sys/wait.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<map>
struct calc{
	pid_t pid;
	void *SR, *SW;  
};

typedef struct Message{
	int id;
	int task;
	long value;
	std::vector<int> to_sum;
}Messages;

struct cntr{
	unsigned int id;
	void *send_port; 
};

struct crtng_arg{
	std::map<int, calc> *clcs;
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
	zmq_connect(send_ans,(std::string("tcp://127.0.0.1:")+std::to_string(arg->his_id)+std::to_string(1)).c_str() );
	if(arg->clcs->find(arg->id)!=arg->clcs->end()){
		//узел найден, проверить на живучесть
		calc cur=(*(arg->clcs))[arg->id];
		char msg[8]="check";
		zmq_send(cur.SW, msg, sizeof(char)*8, 0);
		if(zmq_recv(cur.SR, msg, sizeof(char)*8, 0) == -1 || std::string(msg)!=std::string("alive")){
			int ans = -2;
			zmq_send(send_ans, &ans, sizeof(int), ZMQ_SNDMORE);
			zmq_send(send_ans, &ans, sizeof(int), 0);
		}else{
			int ans = -1;
			zmq_send(send_ans, &ans, sizeof(int), ZMQ_SNDMORE);
			zmq_send(send_ans, &ans, sizeof(int), 0);
		} 
		zmq_close(send_ans);
		delete arg->visited;
		delete arg;
		return 0;
	}
	void* sock_ans = zmq_socket(arg->contex, ZMQ_PULL);
	zmq_bind(sock_ans, (std::string("tcp://127.0.0.1:")+std::to_string(arg->my_id)+std::to_string(1)).c_str());
	int zero=0;
	size_t int_s=sizeof(int);
	for(int i=0; i<arg->neig->size(); ++i){
			if(member((*(arg->neig))[i].id, *(arg->visited))) 
				continue;
			char msg[8]="creat1";
			int n=arg->visited->size();
			zmq_send((*(arg->neig))[i].send_port, msg, sizeof(char)*8, ZMQ_SNDMORE);
			zmq_send((*(arg->neig))[i].send_port, &arg->my_id , sizeof(unsigned int), ZMQ_SNDMORE); //who
			zmq_send((*(arg->neig))[i].send_port, &arg->id , sizeof(unsigned int), ZMQ_SNDMORE); // what
			zmq_send((*(arg->neig))[i].send_port, &n , sizeof(n), ZMQ_SNDMORE);
			for(int j=0; j<n-1; ++j)
				zmq_send((*(arg->neig))[i].send_port, &(*arg->visited)[j], sizeof(unsigned int), ZMQ_SNDMORE);
			zmq_send((*(arg->neig))[i].send_port, &(*arg->visited)[n-1], sizeof(unsigned int), 0);
			int buf;
			zmq_recv(sock_ans, &buf, sizeof(buf), 0);
			if(buf>0){
				unsigned int vis;
				for( int j=0; j<buf-1; ++j){
					zmq_recv(sock_ans, &vis, sizeof(vis), 0);
					if(j+1>arg->visited->size()){
						arg->visited->push_back(vis);
					}
				}
				zmq_recv(sock_ans, &vis, sizeof(vis), 0);
				if(buf>arg->visited->size()){
					arg->visited->push_back(vis);
				}
			}else{ 
			if(buf==0) continue;
				zmq_send(send_ans, &buf, sizeof(buf), ZMQ_SNDMORE);
				zmq_send(send_ans, &buf, sizeof(buf), 0);
				zmq_recv(sock_ans, &buf, sizeof(buf), 0);
				zmq_close(sock_ans);
				zmq_close(send_ans);
				delete arg;
				return 0;
			}
	}
	int buf=0;
	zmq_send(send_ans, &buf, sizeof(buf), ZMQ_SNDMORE);
	zmq_send(send_ans, &buf, sizeof(buf), 0);
	zmq_recv(sock_ans, &buf, sizeof(buf), 0);
	zmq_close(sock_ans);
	zmq_close(send_ans);
	delete arg;
	return 0;
}

void* create_node(void* a){
	crtng_arg *arg=(crtng_arg*)a;
	if(arg->clcs->find(arg->id)!=arg->clcs->end()){
		//узел найден, проверить на живучесть
		calc cur=(*(arg->clcs))[arg->id];
		char msg[8]="check";
		zmq_send(cur.SW, msg, sizeof(char)*8, 0);
		if(zmq_recv(cur.SR, msg, sizeof(char)*8, 0) == -1 || msg!="alive"){
			std::cout<<"ERROR: ALREADY EXISTS, BUT IS NOT AVAILABLE"<<std::endl;
		}else{
			std::cout<<"ERROR: ALREADY EXISTS"<<std::endl;
		} 
		delete arg;
		return 0;
	}
	
	std::vector<unsigned int> visited(1, arg->my_id);
	void* sock_ans = zmq_socket(arg->contex, ZMQ_PULL);
	zmq_bind(sock_ans, (std::string("tcp://127.0.0.1:")+std::to_string(arg->my_id)+std::to_string(1)).c_str());
	for(int i=0; i<arg->neig->size(); ++i){
			if(member((*(arg->neig))[i].id, visited)) 
				continue;
			char msg[8]="creat1";
			int n=visited.size();
			zmq_send((*(arg->neig))[i].send_port, msg, sizeof(char)*8, ZMQ_SNDMORE);
			zmq_send((*(arg->neig))[i].send_port, &arg->my_id , sizeof(unsigned int), ZMQ_SNDMORE); //who
			zmq_send((*(arg->neig))[i].send_port, &arg->id , sizeof(unsigned int), ZMQ_SNDMORE); // what
			zmq_send((*(arg->neig))[i].send_port, &n , sizeof(n), ZMQ_SNDMORE);
			for(int j=0; j<n-1; ++j)
				zmq_send((*(arg->neig))[i].send_port, &visited[j], sizeof(unsigned int), ZMQ_SNDMORE);
			zmq_send((*(arg->neig))[i].send_port, &visited[n-1], sizeof(unsigned int), 0);
			int buf;
			zmq_recv(sock_ans, &buf, sizeof(buf), 0);
			if(buf>0){
				unsigned int vis;
				for( int j=0; j<buf-1; ++j){
					zmq_recv(sock_ans, &vis, sizeof(vis), 0);
					if(j+1>visited.size()){
						visited.push_back(vis);
					}
				}
				zmq_recv(sock_ans, &vis, sizeof(vis), 0);
				if(buf>visited.size()){
					visited.push_back(vis);
				}
			}else if(buf==-1){
				std::cout<<"ERROR: ALREADY EXISTS"<<std::endl;
				zmq_close(sock_ans);
				delete arg;
				return 0;
			}else if(buf==-2){
				std::cout<<"ERROR: ALREADY EXISTS, BUT IS NOT AVAILABLE"<<std::endl;
				zmq_close(sock_ans);
				delete arg;
				return 0;
			}			
	}
	zmq_close(sock_ans);
	void* sread=zmq_socket(arg->contex, ZMQ_PULL);
	void* swrite=zmq_socket(arg->contex, ZMQ_PUSH);
	int pingtime=10, zero=0;
	size_t int_s=sizeof(int);
	zmq_getsockopt(sread, ZMQ_RCVTIMEO,&pingtime, &int_s);
	zmq_getsockopt(sread, ZMQ_LINGER,&zero, &int_s);
	zmq_getsockopt(swrite, ZMQ_SNDTIMEO,&pingtime, &int_s);
	zmq_getsockopt(swrite, ZMQ_LINGER,&zero,&int_s);
	unsigned int i=0;
	while(zmq_bind(sread, (std::string("tcp://127.0.0.1:")+std::to_string(++i)).c_str()));
	std::string rport=std::string("tcp://127.0.0.1:")+std::to_string(i);
	while(zmq_bind(swrite, (std::string("tcp://127.0.0.1:")+std::to_string(++i)).c_str()));
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
		std::cout<<"ERROR: CAN'T CONNECT WITH "<<arg->target<<std::endl;
		zmq_close(sock);
		return 0;		
	}
	if(sock==NULL ||
		zmq_connect(sock, (std::string("tcp://127.0.0.1:")+std::to_string(arg->target)).c_str()) ==-1
	){
		std::cout<<"ERROR: CAN'T CONNECT WITH "<<arg->target<<std::endl;
		delete arg;
		zmq_close(sock);
		return 0;
	}
	int pingtime=10, zero=0;
	size_t int_s=sizeof(int);
	zmq_getsockopt(sock, ZMQ_SNDTIMEO,&pingtime, &int_s);
	zmq_getsockopt(sock, ZMQ_LINGER,&zero,&int_s);
	(arg->neig)->push_back(cntr{arg->target, sock});
	char msg[8]="union1";
	zmq_send(sock, msg, sizeof(char)*8, ZMQ_SNDMORE);
	zmq_send(sock, msg, sizeof(arg->my_id), 0);
	std::cout<<"OK: "<<arg->target;
	delete arg;
	return (void*) 0;
}

void* do_union1(void* a){
	union_arg *arg=(union_arg*)a;
	void* sock=zmq_socket(arg->contex, ZMQ_PUSH);
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
		if( /*pipe_in == NULL ||*/
			zmq_bind(
			pipe_in,
			(std::string("tcp://127.0.0.1:") + argv[1]).c_str()) 
		!=0){
			std::cout<<"PORT "<<(std::string("tcp://127.0.0.1:") + argv[1])<<" IS NOT AVAILABLE"<<std::endl;
			return -4;
		}
		//interface
		char buf[8];
		unsigned int id;
		while(true){
			std::cout<<"("<<argv[1]<<")>";
			std::cin>>buf;
			if(std::string(buf)==std::string("create")){
				std::cin>>id;
				unsigned int dad;
				std::cin>>dad;
				zmq_msg_t mq, ans;
				zmq_msg_init_size(&mq, sizeof(Message));
				Message m;
				m.id = 101;
				memcpy(zmq_msg_data(&mq), &m, sizeof(Message));
				zmq_msg_send(pipe_in, &mq, 0);
				Message *ans = (Message *) zmq_msg_data(&ans);
				zmq_send(pipe_in, (void*) &buf, sizeof(char)*8 , ZMQ_SNDMORE);
				zmq_send(pipe_in, (void*) &id, sizeof(id), /*ZMQ_SNDMORE*/ 0);
				//zmq_send(pipe_in, (void*) &dad, sizeof(dad), 0);
			}else if(std::string(buf)==std::string("remove")){
				std::cin>>id;
				zmq_send(pipe_in, (void*) &buf, sizeof(char)*8 , ZMQ_SNDMORE);
				zmq_send(pipe_in, (void*) &id, sizeof(id), 0);
			}else if(std::string(buf)==std::string("exec")){
				std::cin>>id;
				int n;
				size_t int_s=sizeof(int);
				std::cin>>n;
				std::vector<int> m (n);
				for(int i=0; i<n; ++i) 
					std::cin>>m[i];
				zmq_send(pipe_in, (void*) &buf, sizeof(char)*8 , ZMQ_SNDMORE);
				zmq_send(pipe_in, (void*) &id, sizeof(id), ZMQ_SNDMORE);
				zmq_send(pipe_in, (void*) &n, int_s, ZMQ_SNDMORE);
				for(int i=0; i<n-1; ++i)
					zmq_send(pipe_in, (void*) &m[i], int_s, ZMQ_SNDMORE);
				zmq_send(pipe_in, (void*) &m[n-1], int_s, 0);
			}else if(std::string(buf)==std::string("union")){
				std::cin>>id;
				zmq_send(pipe_in, (void*) &buf, sizeof(char)*8 , ZMQ_SNDMORE);
				zmq_send(pipe_in, (void*) &id, sizeof(id), 0);
			}else if(std::string(buf)==std::string("exit")){
				zmq_send(pipe_in, (void*) &buf, sizeof(char)*8, 0);
				break;
			}else if(std::string(buf)==std::string("pingall")){
				zmq_send(pipe_in, (void*) &buf, sizeof(char)*8, 0);
				break;
			}else{
				std::cout<<"wrong comand"<<std::endl;
			}
		}
		int k;
		waitpid(pid_cntrl, &k, 0);
		return 0;
	}else{
		//cntrl
		void* contex= zmq_ctx_new();
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
		const unsigned int my_id=atoi(argv[1]);
		std::map<int, calc> calcs; //вычислительные
		std::vector<cntr> cntrls;
		char msg[8];
		unsigned int id;
		while(true){
			if( -1 == zmq_recv(pipe_out, msg, sizeof(char)*8, 0)) std::cout<<"error:'(";
			//std::cout<<"msg: "<<msg<<std::endl;
			if(std::string(msg)==std::string("create")){
				zmq_recv(pipe_out, 	&id, sizeof(id), 0);
				pthread_t *crtng_th = new pthread_t;
				crtng_arg *args = new crtng_arg;
				args->clcs=&calcs;
				args->neig= &cntrls;
				args->id=id;
				args->my_id=my_id;
				args->contex=contex;
				pthread_create(crtng_th, NULL,  create_node, args);
				pthread_detach(*crtng_th);
			}else if(std::string(msg)==std::string("remove")){
				zmq_recv(pipe_out, 	&id, sizeof(id), 0);
				rmvng_arg *args = new rmvng_arg;
				args->clcs=&calcs;
				args->neig= &cntrls;
				args->id=id;
				args->my_id=my_id;
				args->contex=contex;
				pthread_t *rmvng_th = new pthread_t;
				pthread_create(rmvng_th, NULL,  create_node, args);
				pthread_detach(*rmvng_th);
				
			}else if(std::string(msg)==std::string("exec")){

			}else if(std::string(msg)==std::string("ping")){

			}else if(std::string(msg)==std::string("union")){
				std::cout<<"come in menu union"<<std::endl;
				zmq_recv(pipe_out, 	&id, sizeof(id), 0);
				if(id==my_id){
					std::cout<<"ERROR: ITS MY ID"<<std::endl;
					continue;
				}
				union_arg *cur = new union_arg;
				cur->neig = &cntrls;
				cur->target = id;
				cur->my_id = my_id;
				cur->contex=contex;
				pthread_t *union_th = new pthread_t;
				pthread_create(union_th, NULL, do_union, (void*) cur);
				pthread_detach(*union_th);
			}else if(std::string(msg)==std::string("union1")){
				zmq_recv(pipe_out, 	&id, sizeof(id), 0);
				union_arg *cur = new union_arg;
				cur->neig = &cntrls;
				cur->target = id;
				cur->my_id = my_id;
				cur->contex=contex;
				pthread_t *union_th = new pthread_t;
				pthread_create(union_th, NULL, do_union1, (void*) cur);
				pthread_detach(*union_th);
			}else if(std::string(msg)==std::string("creat1")){
				zmq_recv(pipe_out, 	&id, sizeof(id), 0);
				pthread_t *crtng_th = new pthread_t;
				crtng_arg *args = new crtng_arg;
				args->clcs=&calcs;
				args->neig= &cntrls;
				args->his_id=id;
				zmq_recv(pipe_out, 	&id, sizeof(id), 0);
				args->id=id;
				args->my_id=my_id;
				args->contex=contex;
				args->visited=new std::vector<unsigned int>;
				int kolvo;
				zmq_recv(pipe_out, 	&kolvo, sizeof(int), 0);
				for(int i=0;i<kolvo-1; ++i){
					zmq_recv(pipe_out, 	&id, sizeof(id), 0);
					args->visited->push_back(id);
				}
				pthread_create(crtng_th, NULL,  create_check, args);
				pthread_detach(*crtng_th);
			}else if(std::string(msg)==std::string("exit")){
				//std::cout<<"try exit"<<std::endl;
				break;
			}else{
				std::cout<<"wrong message: "<<std::string(msg)<<std::endl;
			}
			//std::cout<<"end of if"<<std::endl;
		}
	
	}
	return 0;
}
