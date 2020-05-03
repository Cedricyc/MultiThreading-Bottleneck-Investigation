#include<cstdio>
#include<cstring>
#include<time.h>
#include"sys/time.h"
#include<algorithm>
#include<pthread.h>
#include<semaphore.h>
#include<stdlib.h>
#include<atomic>
#define file_num 1000  // number of files
#define THREAD_NUM_READER 32 // number of reader threads
#define THREAD_NUM_CALC 32// number of calc_worker threads
#define NO_OUTPUT_MODE // delete this if you want debug info output
//#define NO_CALC // delete this, calculation will perform
#define MAX_CHAR_PER_FILE 1024// maxium size of a file string
#define THREAD_READ_FILES_CHUNKSIZE 1 // Number of files (chunk) read by one reader thread each time
#define PATHSTR string("./../data/1000F1000C/")
#define SHAREPREFIX string("data_")


using namespace std ; 


//----------GLOBAL-----------
#ifndef USE_LOCK
atomic<int> 
#endif
#ifdef USE_LOCK
int
#endif
queue_tail, queue_head ;// When read a new file, tail++; when calc up one queue element, head++
char * queue_string[file_num]; // a queue, each element is a string corresponding to one file
sem_t feed_string;  // sem_t for producer&consumer model
//---------------------------


//----------clocking----------
struct timeval st,en;  
//tik() starts a clock
void tik() {
	gettimeofday(&st, NULL );  
}
//tok() ends a clock
long timeuse ; 
void tok(const char * msg ) {
	gettimeofday(&en, NULL );  
	timeuse =1000000 * ( en.tv_sec - st.tv_sec ) + en.tv_usec - st.tv_usec;  
	printf("%s : %lf\n",msg,timeuse /1000000.0);
	
}
//----------clocking----------

//----------reader------------
pthread_mutex_t queue_push;

void Add_queue(char * s ) {
    #ifdef USE_LOCK
        pthread_mutex_lock(&queue_push);
        queue_string[queue_tail++] = s ; 
        pthread_mutex_unlock(&queue_push);
    #endif
    #ifndef USE_LOCK 
        queue_string[queue_tail.fetch_add(1)] = s ; 
    #endif
    sem_post(&feed_string);
}

// Read one file 
void * thread_read_file(void* filename ){
#ifndef NO_OUTPUT_MODE
    printf("reading %s\n",(* (string *)filename ).c_str());
#endif
    FILE * fp = fopen((* (string *)filename ).c_str(),"r");
    char * buf  = new char[MAX_CHAR_PER_FILE] ;
    fscanf(fp,"%s",buf);
    fclose(fp);
#ifndef NO_OUTPUT_MODE
    printf("done reading %s\n",(* (string *)filename ).c_str());
#endif
    Add_queue(buf);

}

typedef pair<int,int> IntPair ; 
// Each thread execute this, read file id range of [section.l,section.r]
void * thread_read_range_files( void * section ){
    for( int i=((IntPair*)section)->first ; i<((IntPair*)section)->second ; i++){
        string *filename = new string[THREAD_NUM_READER];
        *filename= PATHSTR+SHAREPREFIX+to_string(i);
        thread_read_file((void*)filename);
    }
}

void * Reader_Init( void * arg_NULL ) {
    pthread_t * threads = new pthread_t[THREAD_NUM_READER];
    pthread_mutex_init(&queue_push,NULL);
    int latest_file = 0 ; 
    for( int i=0 ; i<file_num ; i=latest_file) {
        IntPair * range4threads = new IntPair[THREAD_NUM_READER];
        // Fork THREAD_NUM_READER threads, each thread will
        // read THREAD_READ_FILE_CHUNKSIZE files each time.
        // if THREAD_READ_FILE_CHUNKSIZE is small, will cause
        // too much overhead on synchronizing. if is big, will
        // cause load imbalance.
        for( int j=0 ; j<THREAD_NUM_READER ; j++ ) {
            range4threads[j] = make_pair(latest_file,min(file_num,latest_file+THREAD_READ_FILES_CHUNKSIZE));
            pthread_create(&threads[j],NULL,thread_read_range_files,(void*)&range4threads[j]);
            latest_file = range4threads[j].second;
        }
        for(int j=0 ; j<THREAD_NUM_READER ; j++ )
            pthread_join(threads[j],NULL);
    }
    pthread_mutex_destroy(&queue_push);
}

//-----------reader------------


//-----------palicalc_kernel-----------
// Palindrome Automata node 
struct PA_node { 
    int len , cnt , fa , s[27];
};

int calc_fail_chain( char * s , int node_id , PA_node * tr, int position ){
    while( position-tr[node_id].len==0 || s[position-tr[node_id].len-1]!=s[position] )node_id=tr[node_id].fa;
    return node_id ; 
}

void Add_PA_node( char * s , int position , char c , int &last_node, PA_node * tr , int &PA_node_num) {
    int cur = calc_fail_chain(s,last_node,tr,position);
    int tmp = 0  ;
    if( tr[cur].s[c-'a']==0 ){
        tmp = ++ PA_node_num ; 
        tr[PA_node_num].fa = tr[ calc_fail_chain(s,tr[cur].fa,tr,position)].s[c-'a'];
        tr[PA_node_num].len = tr[cur].len + 2 ; 
        tr[PA_node_num].cnt = 0 ;
        tr[cur].s[c-'a'] = tmp ; 
    } else tmp = tr[cur].s[c-'a'];
    last_node = tmp ; 
    tr[last_node].cnt ++ ; 
}

int Calc_Palindrome(char * s){
    int len = strlen(s);
    int ret = 0 ;
#ifndef BRUTE_FORCE // compile flag
// USE Palindrome Automata
    int last_node = 0 , PA_node_num = 1 ; 
    PA_node * tr = new PA_node[len];
    memset( tr , 0 , sizeof(PA_node)*len);
    tr[0].len = 0 , tr[0].fa = 1 ;
    tr[1].len =-1 , tr[1].fa = 0 ; 
    for( int i=0 ; i<len ; i++ ) Add_PA_node(s,i,s[i],last_node,tr,PA_node_num);

    for( int i=PA_node_num ; i>1 ; i-- ){
        tr[tr[i].fa].cnt += tr[i].cnt ; 
        ret += tr[i].cnt ; 
    }
    delete [] tr ; 
#endif
#ifdef BRUTE_FORCE
// USE BruteForce
    for( int i=0 ; i<len ; i++ ){
        for(int j=0 ; i+j<len && i-j>=0 ; j++ ) if( s[i+j]==s[i-j] )
            ret ++ ;
        else break ; 
        if( i!=len-1 )
            for(int j=0 ; i+j+1<len && i-j>=0 ; j++ )if(s[i+j+1]==s[i-j])
                ret ++ ; 
        else break ; 
    }
#endif
    return ret ; 
}
//-----------palicalc_kernel-----------

//-----------calc----------------------
pthread_mutex_t queue_pop ; 

// On purpose to end all calc threads
void Terminate_Calc_workers(){
    for( int i=0 ; i<file_num ; i++ )
        sem_post(&feed_string);

}


// Multi-threading of calc_worker, each thread keep consuming 
// string that feeded by reader
int ret[THREAD_NUM_CALC] = {0};
atomic_flag atom_queue_pop = ATOMIC_FLAG_INIT ; 
void *Calc_worker( void * arg_int){
    int my_worker_id = *(int*)arg_int;
#ifndef NO_OUTPUT_MODE
	printf("my worker id is %d\n",my_worker_id);
#endif
    for( ;; ) {
        sem_wait(&feed_string);
        int id ;
        #ifdef USE_LOCK
            pthread_mutex_lock(&queue_pop);
            id = queue_head ++ ; 
            pthread_mutex_unlock(&queue_pop);
        #endif
        #ifndef USE_LOCK
            id=queue_head.fetch_add(1);
        #endif
        if( id >= file_num ) {
            return (void*)&ret[my_worker_id]; 
        }
        // Palindrome calculation is implemented at 
        // Calc_Palindrome in palicalc_kernel.h
        ret[my_worker_id] += Calc_Palindrome(queue_string[id]);
#ifndef NO_OUTPUT_MODE
        printf("working on:%d, contain %d palindromes,my id is %d\n",id,ret[my_worker_id],my_worker_id);
#endif
    }

}

// This function start THREAD_NUM_CALC worker threads
void* Calc_Init( void * arg_NULL) {
    static int sum=0 ; 
    pthread_t * threads = new pthread_t[THREAD_NUM_CALC];
    pthread_mutex_init(&queue_pop,NULL);
    int * worker_id = new int[THREAD_NUM_CALC];
    // ---------Fork workers-----------
    for( int i=0 ; i<THREAD_NUM_CALC ; i++ ) {
        worker_id[i]=i;
        pthread_create(&threads[i],NULL,Calc_worker,(void*)&worker_id[i]);
    }
    // ---------Join worker and reduce the answer-------
    for( int i=0 ; i<THREAD_NUM_CALC ; i++ ) {
        void * ret ; 
        pthread_join(threads[i],&ret);
        sum += *(int *)ret;
    }
    return (void*)&sum ; 
}
//-----------calc-------------



int main() {
    //test_palindrome_calc() ;
    tik();
    pthread_t *thread4read = new pthread_t ,
              *thread4calc = new pthread_t ;
    
    sem_init(&feed_string,0,0);
//--------------Fork producer and consumer Initializer-------
    pthread_create(thread4read,NULL,Reader_Init,NULL);
    #ifndef NO_CALC
    pthread_create(thread4calc,NULL,Calc_Init,NULL);
    #endif
//-----------------------------------------------------------

//--------------Join-----------------
    pthread_join(*thread4read,NULL);
    #ifndef NO_CALC
    Terminate_Calc_workers();
    //test_reading();
    void * ans ; 
    pthread_join(*thread4calc,&ans);
    printf("Result::%d\n",*(int*)ans);
    #endif
//----------------------------------
    tok("Time use:");
    printf("Throughput:%.6lfMBps\n",file_num*1.0*MAX_CHAR_PER_FILE/1024/1024 /( timeuse /1e6)) ; 
}
