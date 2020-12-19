#include<bits/stdc++.h>
using namespace std;
#define ull unsigned long long

enum cache_line_states{
    invalid=1,
    modified,
    exclusive,
    shared
};

enum messages{
    GET=1, GETX, PUT, PUTX, UPGR, INV, ACK, NACK, SWB, UPGR_ACK, WB, WB_ACK
};

enum bank_id{
    bank0=8, bank1, bank2, bank3, bank4, bank5, bank6, bank7
};

typedef struct thread_entry{
    unsigned long long addr;
    int op_type;
    unsigned long long global_count;
    int tid;
}thread_entry;

typedef struct msg{
    unsigned long long addr;
    int src;
    int dest;
    int msg_type;
    vector<int> bit_v;
}msg;

typedef struct cache_line{
    unsigned long long tag;
    unsigned long long timestamp;
    int state;
} cache_line;


class L1{
    public:
        static const int n_sets=64;
        static const int associativity=8;
        ull hits=0;
        // ull miss=0;
        ull read_miss=0;
        ull write_miss=0;
        ull upgrade_miss=0;
        queue<msg> msg_queue;
        queue<thread_entry> trace_queue;
        map<ull, queue<char> > ott;  // Will see later
        cache_line cache_struct[n_sets][associativity];
        map<ull,int> ack_count;
        map<ull, pair<int, int> > nack_count;
        ull ts=0;
        void initialize();
        int isMissorHit(ull addr);
        pair<ull,ull>  LRU(ull addr, int state);
};

void L1::initialize(){
    for(int i=0; i<n_sets; i++){
        for(int j=0; j< associativity; j++){
            cache_struct[i][j].state=invalid;
        }
    }
}

int L1::isMissorHit(ull addr){
    ull set = (addr & 0xFC0) >> 6;
    ull tag = (addr & 0xFFFFFFFFFFFFF000) >> 12;
    for(int i=0; i<associativity; i++){
        if(cache_struct[set][i].tag==tag)   return cache_struct[set][i].state;
    }
    return invalid;
}

pair<ull,ull> L1::LRU(ull addr, int state){  //change the return type accordingly
    int victim;
    ull set = (addr & 0xFC0) >> 6;
    ull tag = (addr & 0xFFFFFFFFFFFFF000) >> 12;
    for(int i=0; i<associativity; i++){
        // cout<<"checking\n";
        if(cache_struct[set][i].state==invalid){
            cache_struct[set][i].tag=tag;
            cache_struct[set][i].state=state;
            cache_struct[set][i].timestamp=ts++;
            // cout<<"going from here\n";
            return make_pair(-1,-1);
        }
    }
    ull min_ts=ts;
    for(int i=0; i<associativity; i++){
        if(cache_struct[set][i].timestamp < min_ts){
            victim=i;
            min_ts=cache_struct[set][i].timestamp;
        }
    }
    ull temp_set, temp_tag;
    temp_set=set;
    temp_tag=cache_struct[set][victim].tag;
    int orig_state=cache_struct[set][victim].state;
    cache_struct[set][victim].tag=tag;
    cache_struct[set][victim].state=state;
    cache_struct[set][victim].timestamp=ts++;
    if(orig_state==modified){
        return make_pair(temp_tag, temp_set);
    }
    return make_pair(-1,-1);
}

typedef struct bank_line{
    unsigned long long tag;
    unsigned long long timestamp;
    int dirty;
    int pending;
    vector<int> directory;  
}bank_line;

typedef struct l2_evicted{
    unsigned long long addr;
    vector<int> bit_v;
    int flag;
    int pending;
    int dirty;
}l2_evicted;

class L2_bank{
    public:
        static const int n_sets=512;
        static const int associativity=16;
        ull hits=0;
        ull miss=0;
        ull ts=0;
        queue<msg> msg_queue;
        map<ull, l2_evicted> eviction_buffer;
        map<ull, vector<bank_line> > bank_structure;   //resize it when inserting
        int isMissorHit(ull addr);
        l2_evicted LRU(ull addr);
};

int L2_bank::isMissorHit(ull addr){
    ull sett = (addr & 0x1FFE0) >> 6;
    ull tag = (addr & 0xFFFFFFFFFFFC0000) >> 18;
    auto it = bank_structure.find(sett);
    if(it==bank_structure.end())
        return -1;
    else{
        // pair<ull, vector<bank_line> > e=*it;
        for(int i=0;i<bank_structure[sett].size();i++){
            if(bank_structure[sett][i].tag==tag){
                return i;    
            }
        }
        return -1;
    }
}

l2_evicted L2_bank::LRU(ull addr){
    ull sett = (addr & 0x1FFE0) >> 6;
    ull tag = (addr & 0xFFFFFFFFFFFC0000) >> 18;
    // printf("Inside the LRU : tag: %ull \n", tag);
    // cout<<"Inside the LRU : tag: "<<tag<<endl;
    l2_evicted evicted;
    auto it=bank_structure.find(sett);
    if(it==bank_structure.end()){
        // cout<<"this line not present in cache, inserting it\n";
        vector<bank_line> v;
        // bank_structure.insert(make_pair(sett, v));
        bank_line b;
        b.directory.resize(8);
        for(int j=0;j<8;j++){
            b.directory[j]=-1;
        }
        b.tag=tag;
        b.timestamp=ts++;
        b.pending=0;
        b.dirty=0;
        v.push_back(b);
        bank_structure[sett]=v;
        // bank_structure[sett].push_back(b);
        evicted.flag=-1;
        return evicted;
    }
    else{
        if(bank_structure[sett].size()<16){
            bank_line b;
            b.tag=tag;
            b.dirty=0;
            b.pending=0;
            b.directory.resize(8);
            for(int j=0;j<8;j++){
                b.directory[j]=-1;
            }
            b.timestamp=ts++;
            bank_structure[sett].push_back(b);
            // return (sett, bank_structure[sett].size()-1);
            evicted.flag=-1;
            return evicted;
        }
        else{
            int victim;
            ull min_ts=ts;
            for(int i=0; i<bank_structure[sett].size(); i++){
                if(bank_structure[sett][i].timestamp < min_ts){
                    min_ts=bank_structure[sett][i].timestamp;
                    victim=i;
                }
            }
            // invalidate(bank_line bank_structure[sett][victim]);
            evicted.flag=1;
            evicted.pending=bank_structure[sett][victim].pending;
            evicted.dirty=bank_structure[sett][victim].dirty;
            evicted.bit_v=bank_structure[sett][victim].directory;
            string tag_string, set_string, dummy_string;
            tag_string=std::bitset<46>(bank_structure[sett][victim].tag).to_string();
            set_string=std::bitset<12>(sett).to_string();
            dummy_string=std::bitset<6>(0).to_string();
            string temp=tag_string+set_string+dummy_string;
            ull address=stoull(temp, nullptr, 2);
            evicted.addr=address;
            bank_structure[sett][victim].tag=tag;
            bank_structure[sett][victim].timestamp=ts++;
            bank_structure[sett][victim].dirty=0;
            bank_structure[sett][victim].pending=0;
            for(auto &e : bank_structure[sett][victim].directory){
                e=-1;
            }
            return evicted;
        }
    }
}
