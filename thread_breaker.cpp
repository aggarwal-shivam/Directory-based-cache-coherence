#include<bits/stdc++.h>
using namespace std;

#define ull unsigned long long

int main(int argc, char *argv[]){
    // string name=argv[1];
    string tempo=argv[1];
    string input_name=tempo + ".out";
    ifstream inputfile(input_name);
    fstream fout0, fout1, fout2, fout3, fout4, fout5, fout6, fout7;
    ull gc=0, addr, tid, optype;
    string out_names[8];
    out_names[0]="thread0.out";
    out_names[1]="thread1.out";
    out_names[2]="thread2.out";
    out_names[3]="thread3.out";
    out_names[4]="thread4.out";
    out_names[5]="thread5.out";
    out_names[6]="thread6.out";
    out_names[7]="thread7.out";
    fout0.open(out_names[0], ios::out | ios::app);
    fout1.open(out_names[1], ios::out | ios::app);
    fout2.open(out_names[2], ios::out | ios::app);
    fout3.open(out_names[3], ios::out | ios::app);
    fout4.open(out_names[4], ios::out | ios::app);
    fout5.open(out_names[5], ios::out | ios::app);
    fout6.open(out_names[6], ios::out | ios::app);
    fout7.open(out_names[7], ios::out | ios::app);
    while(inputfile >> tid >> optype >> addr){
        if(tid==0){
            fout0 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        if(tid==1){
            fout1 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        if(tid==2){
            fout2 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        if(tid==3){
            fout3 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        if(tid==4){
            fout4 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        if(tid==5){
            fout5 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        if(tid==6){
            fout6 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        if(tid==7){
            fout7 << addr <<" "<< optype <<" "<< gc << "\n";
        }
        gc++;
    }
    return 0;
}