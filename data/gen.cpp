// Generate 1000 files, each file contains a string with length of 1000
#include<cstring>
#include<string>
#include<iostream> 
#include<cstdio>
#include<stdlib.h>
using namespace std;
#define str_len 1000
#define file_num 1000
int main() {
    srand((unsigned)time(NULL));
    for( int ii=0 ; ii<1000 ; ii++ ) {
        string filename = "./data_"+to_string(ii);
        freopen(filename.c_str(),"w",stdout);
        for( int i=1 ; i<=1000 ; i++ ){
            printf("%c",rand()%1+'a');
        }
        puts("");
    }
}
