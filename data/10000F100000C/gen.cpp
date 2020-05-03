// Generate 1000 files, each file contains a string with length of 1000
#include<cstring>
#include<string>
#include<iostream> 
#include<cstdio>
#include<stdlib.h>
using namespace std;
#define str_len 100000
#define file_num 10000
int main() {
    srand((unsigned)time(NULL));
    for( int ii=0 ; ii<file_num; ii++ ) {
        string filename = "./data_"+to_string(ii);
        freopen(filename.c_str(),"w",stdout);
        for( int i=1 ; i<=str_len; i++ ){
            printf("%c",rand()%1+'a');
        }
        puts("");
    }
}
