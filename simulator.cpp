#include<bits/stdc++.h>
#include "cache.h"
using namespace std;
#define ull unsigned long long

map<int,ull> msg_counts;
map<int,ull> l1_msg_counts;
map<int, ull> l2_msg_counts;


ull cycle=0;
ull latest_consumed=-1;  

L1 p_cache[8];
L2_bank banks[8]; 

bool comparator(thread_entry a, thread_entry b){
    if(a.global_count < b.global_count) return true;
    return false;
}

int calculate_home_bank(ull addr){  
    ull sett = (addr & 0x1FFE0) >> 6;
    int bank = (sett & 0X007);
    return bank;
}

void increment_l1_ts(int core, ull addr){
    ull set = (addr & 0xFC0) >> 6;
    ull tag = (addr & 0xFFFFFFFFFFFFF000) >> 12;
    for(int i=0; i<p_cache[core].associativity; i++){
        if(p_cache[core].cache_struct[set][i].tag==tag){
            p_cache[core].cache_struct[set][i].timestamp=p_cache[core].ts++;
            break;
        }   
    }
}

void change_l1_state(int core, ull addr, int state){
    ull set = (addr & 0xFC0) >> 6;
    ull tag = (addr & 0xFFFFFFFFFFFFF000) >> 12;
    for(int i=0; i<p_cache[core].associativity; i++){
        if(p_cache[core].cache_struct[set][i].tag==tag){
            p_cache[core].cache_struct[set][i].state=state;
            break;
        }   
    }
}

void process_thread_entry(thread_entry current){  //This function is to process a thread entry from private L1 cache.  
    int src=current.tid;
    ull addr=current.addr;
    int dest=calculate_home_bank(addr);
    // cout<<calculate_home_bank<<endl;
    int op_type=current.op_type;
    int state=p_cache[src].isMissorHit(addr); // this function will return the state of the cache blocks in private L1 cache.
    // cout<<state<<"    ..\n";
    char operation= (current.op_type == 0) ? 'R' : 'W';
    // cout<<operation<<"   ..\n";
    ull key_addr=addr>>6;
    if(state==invalid){
        // p_cache[src].miss++;      
        if(p_cache[src].ott.find(key_addr) != p_cache[src].ott.end() ){
            // cout<<"t1\n";
            // cout<<"here\n";
            p_cache[src].ott[key_addr].push(operation);
            // cout<<"ott insertion\n";
        }
        else{
            if(operation=='R')  p_cache[src].read_miss++;
            else
            {
                p_cache[src].write_miss++;
            }
            queue<char> q;
            q.push(operation);
            p_cache[src].ott[key_addr]=q;
            // p_cache[src].
            msg tmp;
            tmp.src=src;
            tmp.dest=dest+8;
            tmp.addr=addr;
            tmp.msg_type = (operation=='R') ? GET : GETX; 
            banks[dest].msg_queue.push(tmp);
            msg_counts[tmp.msg_type]++;
        }
    }
    else if(state==shared){
        // cout<<"t3\n";
        if(operation=='R'){
            // cout<<"t4\n";
            p_cache[src].hits++;
            increment_l1_ts(src, addr);
        }
        else{
            // cout<<"t5\n";
            p_cache[src].upgrade_miss++;
            if(p_cache[src].ott.find(key_addr) != p_cache[src].ott.end() ){
                // cout<<"t6\n";
                p_cache[src].ott[key_addr].push(operation);
            }
            else{
                // cout<<"t7\n";
                msg tmp;
                tmp.src=src;
                tmp.dest=dest+8;
                tmp.addr=addr;
                tmp.msg_type = UPGR; 
                msg_counts[tmp.msg_type]++;
                banks[dest].msg_queue.push(tmp);
                msg_counts[tmp.msg_type]++;
            }
        }
    }
    else if(state==modified){ //for both kinds of operations, we can treat it as a hit
        // cout<<"t8\n";
        p_cache[src].hits++;
        increment_l1_ts(src, addr);    
    }
    else if(state==exclusive){
        // cout<<"t9\n";
        p_cache[src].hits++;
        increment_l1_ts(src, addr);
        if(operation=='W'){
            // cout<<"t10\n";
            change_l1_state(src, addr, modified);
        }
    }
}

void process_message(int index, msg message){
    ull key_address=message.addr>>6;
    if(index<=7){
        l1_msg_counts[message.msg_type]++;
        // cout<<"m1\n";   //index tell at which privatee cache or bank we are currently processing the request
        if(message.msg_type==PUT){
            // cout<<"here\n";
            // cout<<"m2\n";            
            auto p=p_cache[index].LRU(message.addr, shared);
            // cout<<"lru done\n";
            if(p.first!=-1){
                // cout<<"m3\n";
                // WE'll see this if need to send a message or we can call L2 LRU directly.
                msg temp;
                temp.msg_type=WB;
                temp.src=index;
                string tag_string, set_string, dummy_string;
                tag_string=std::bitset<52>(p.first).to_string();
                set_string=std::bitset<6>(p.second).to_string();
                dummy_string=std::bitset<6>(0).to_string();
                string temp_str=tag_string+set_string+dummy_string;
                ull address=stoull(temp_str, nullptr, 2);
                temp.dest=calculate_home_bank(address)+8;
                temp.addr=address;
                banks[temp.dest-8].msg_queue.push(temp);
                msg_counts[temp.msg_type]++;
            }
            // cout<<"m4\n";
            // // cout<<"reached"<<endl;
            while(!p_cache[message.dest].ott[key_address].empty()){
                if(p_cache[message.dest].ott[key_address].front()=='R'){
                    // p_cache[message.dest].hits++;
                    // cout<<"m5\n";
                    p_cache[message.dest].hits++;
                    p_cache[message.dest].ott[key_address].pop();
                }
                else{
                    // cout<<"m6\n";
                    msg temp;
                    p_cache[message.dest].upgrade_miss++;
                    temp.src=message.dest;
                    // temp.dest=message.src;
                    temp.dest=calculate_home_bank(message.addr)+8;
                    temp.addr=message.addr;
                    temp.msg_type=UPGR;
                    banks[temp.dest-8].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                    break;
                }
            }
            p_cache[message.dest].hits--;            
        }
        else if(message.msg_type==PUTX){
            // cout<<"m7\n";
            if(message.src<=7){
                // cout<<"m8\n";     //PUTX coming from a p_cache
                auto p=p_cache[index].LRU(message.addr, modified);
                if(p.first!=-1){
                    // cout<<"m9\n";
                    // We'll see this if need to send a message or we can call L2 LRU directly.
                    msg temp;
                    temp.msg_type=WB;
                    temp.src=index;
                    string tag_string, set_string, dummy_string;
                    tag_string=std::bitset<52>(p.first).to_string();
                    set_string=std::bitset<6>(p.second).to_string();
                    dummy_string=std::bitset<6>(0).to_string();
                    string temp_str=tag_string+set_string+dummy_string;
                    ull address=stoull(temp_str, nullptr, 2);
                    temp.dest=calculate_home_bank(address)+8;
                    temp.addr=address;
                    banks[temp.dest-8].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                }
                // cout<<"m10\n";
                while(!p_cache[message.dest].ott[key_address].empty()){
                    p_cache[message.dest].hits++;
                    p_cache[message.dest].ott[key_address].pop();
                }        
            }
            else{       //PUTX coming from a bank
                // cout<<"m11\n";
                vector<int> sharers= message.bit_v;
                int flag=0;
                for(int i=0;i<8;i++){
                    if(sharers[i]==1){
                        flag++;
                    }
                }
                if(flag==0){
                    // cout<<"m12\n";
                    auto p=p_cache[index].LRU(message.addr, exclusive);
                    if(p.first!=-1){
                        // We'll see this if need to send a message or we can call L2 LRU directly.
                        // cout<<"m13\n";
                        msg temp;
                        temp.msg_type=WB;
                        temp.src=index;
                        string tag_string, set_string, dummy_string;
                        tag_string=std::bitset<52>(p.first).to_string();
                        set_string=std::bitset<6>(p.second).to_string();
                        dummy_string=std::bitset<6>(0).to_string();
                        string temp_str=tag_string+set_string+dummy_string;
                        ull address=stoull(temp_str, nullptr, 2);
                        temp.dest=calculate_home_bank(address)+8;
                        temp.addr=address;
                        banks[temp.dest-8].msg_queue.push(temp);
                        msg_counts[temp.msg_type]++;
                    }
                    while(!p_cache[message.dest].ott[key_address].empty()){
                        // p_cache[message.dest].hits++;
                        p_cache[message.dest].ott[key_address].pop();
                    }
                }
                else{
                    // cout<<"m14\n";
                    p_cache[index].ack_count[key_address]=flag;
                }
            }
        }
        else if(message.msg_type==ACK){
            // cout<<"m15\n";
            if(--p_cache[index].ack_count[key_address]==0){
                // cout<<"m16\n";
                auto p=p_cache[index].LRU(message.addr, modified);
                if(p.first!=-1){
                        // We'll see this if need to send a message or we can call L2 LRU directly.
                    // cout<<"m17\n";
                    msg temp;
                    temp.msg_type=WB;
                    temp.src=index;
                    string tag_string, set_string, dummy_string;
                    tag_string=std::bitset<52>(p.first).to_string();
                    set_string=std::bitset<6>(p.second).to_string();
                    dummy_string=std::bitset<6>(0).to_string();
                    string temp_str=tag_string+set_string+dummy_string;
                    ull address=stoull(temp_str, nullptr, 2);
                    temp.dest=calculate_home_bank(address)+8;
                    temp.addr=address;
                    banks[temp.dest-8].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                
                }
                while(!p_cache[message.dest].ott[key_address].empty()){
                    p_cache[message.dest].hits++;
                    p_cache[message.dest].ott[key_address].pop();
                }
            }
        }
        else if(message.msg_type==UPGR_ACK){
            // cout<<"m18\n";
            change_l1_state(message.dest, message.addr, modified);
            increment_l1_ts(message.dest, message.addr);
            while(!p_cache[message.dest].ott[key_address].empty()){
                // p_cache[message.dest].hits++;
                p_cache[message.dest].ott[key_address].pop();
            }
        }
        else if(message.msg_type==GET){
            // cout<<"m19\n"; 
            msg temp;
            temp.src=message.dest;
            temp.dest=message.src;
            temp.addr=message.addr;
            temp.msg_type=PUT;
            change_l1_state(temp.src, temp.addr, shared);
            p_cache[temp.dest].msg_queue.push(temp);
            msg_counts[temp.msg_type]++;
            //SWB
            temp.dest=calculate_home_bank(temp.addr)+8;
            temp.msg_type=SWB;
            banks[temp.dest-8].msg_queue.push(temp);
            msg_counts[temp.msg_type]++;
        }
        else if(message.msg_type==GETX){
            // cout<<"m20\n";
            msg temp;
            temp.src=message.dest;
            temp.dest=message.src;
            temp.addr=message.addr;
            temp.msg_type=PUTX;
            change_l1_state(temp.src, temp.addr, invalid);
            p_cache[temp.dest].msg_queue.push(temp);
            msg_counts[temp.msg_type]++;
            //ACK
            temp.dest=calculate_home_bank(temp.addr)+8;
            temp.msg_type=ACK;
            banks[temp.dest-8].msg_queue.push(temp);
            msg_counts[temp.msg_type]++;
        }
        else if(message.msg_type==NACK){
            // cout<<"m21\n";   
            pair<ull, pair<int,int> > temp;
            temp.first=key_address;
            temp.second.first=message.msg_type;
            temp.second.second=10;
            p_cache[message.dest].nack_count.insert(temp);
        }
        else if(message.msg_type=INV){
            if(message.src>=8){         // eviction buffer ack handling
                change_l1_state(message.dest, message.addr, invalid);    
            }
            else{               // This is the handling of ack if it is coming from home (by faking)
                // cout<<"m22\n";
                // cout<<message.src<<"    "<<message.dest<<endl;
                //suppose home is faking here also
                change_l1_state(message.dest, message.addr, invalid);
                msg temp;
                temp.src=message.dest;
                temp.dest=message.src;
                temp.msg_type=ACK;
                temp.addr=message.addr;
                // cout<<temp.dest<<"   "<<temp.src<<endl;
                p_cache[temp.dest].msg_queue.push(temp);
                // cout<<"here\n";
                msg_counts[temp.msg_type]++;
                }
        }
    }
    else{ // Processing at home -> GET, GETX, UPGRD, SWB, ACK
        // cout<<message.msg_type<<endl;
        // cout<<"m23\n";
        l2_msg_counts[message.msg_type]++;
        ull set = (message.addr & 0x1FFC0) >> 6;
        // ull tag = (message.addr & 0xFFFFFFFFFFFD0000) >> 17;
        ull tag = (message.addr & 0xFFFFFFFFFFFC0000) >> 18;
        index-=8;
        int line_num=-1;
        int state=banks[index].isMissorHit(message.addr); 
        l2_evicted p;
        if(state==-1){
            // cout<<"m24\n";
            // banks[index].miss++;
            p=banks[index].LRU(message.addr);
        }
        if(state==-1 && p.flag != -1){
            //invalidate in L1 to maintain inclusion
            // cout<<"m25\n";
            banks[index].eviction_buffer.insert(make_pair(key_address, p)); //We'l see (LRU present and processing of this buffer)
        }
        // cout<<tag<<endl;
        // int x=banks[index].isMissorHit(message.addr);
        // printf("x:%d\n", x);
        //Get line umber
        for(int i=0; i<banks[index].bank_structure[set].size(); i++){
            // printf("finding\n");
            // cout<<banks[index].bank_structure[set][i].tag<<endl;
            if(banks[index].bank_structure[set][i].tag==tag){
                // cout<<"m26\n";
                line_num=i;
                break;
            }
        }
        // cout<<"line num : "<<line_num<<endl;
        // cout<<banks[index].bank_structure[set][line_num].directory.size()<<endl;
        // cout<<banks[index].bank_structure[set][line_num].tag<<endl;
        // for(auto e: banks[index].bank_structure[set][line_num].directory){
        //     cout<<e<<" ";
        // }
        // cout<<endl;
        // cout<<line_num<<"  ....\n";
        if(message.msg_type==GET){
            // cout<<"m27\n";
            // cout<<"heremm\n";
            if(state==-1)   banks[index].miss++;
            else banks[index].hits++;
            // bank_line banks[index].bank_structure[set][line_num]=banks[index].bank_structure[set][line_num];
            if(banks[index].bank_structure[set][line_num].pending==1){
                // cout<<"m28\n";
                msg temp;
                temp.src=index+8;
                temp.dest=message.src;
                temp.addr=message.addr;
                temp.msg_type=NACK;
                p_cache[temp.dest].msg_queue.push(temp);
                msg_counts[temp.msg_type]++;
            }
            else{
                // cout<<"m29\n";
                // cout<<"pending not 1\n";
                if(banks[index].bank_structure[set][line_num].dirty==0){
                    // cout<<"not dirty\n";
                    // cout<<"m30\n";
                    banks[index].bank_structure[set][line_num].directory[message.src]=1;
                    msg temp; 
                    temp.src=message.dest;
                    temp.dest=message.src;
                    temp.addr=message.addr;
                    temp.msg_type=PUT;
                    p_cache[temp.dest].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                    // cout<<"sent\n";
                }
                else{
                    // cout<<"m31\n";
                    // cout<<"dirty\n";
                    banks[index].bank_structure[set][line_num].pending=1;
                    banks[index].bank_structure[set][line_num].dirty=0;
                    
                    int owner;
                    for(int i=0;i<8;i++){
                        if(banks[index].bank_structure[set][line_num].directory[i]==1){
                            // cout<<"m31b\n";
                            owner=i;
                            break;
                        }
                    }
                    // cout<<"m31a\n";
                    // cout<<owner<<endl;
                    banks[index].bank_structure[set][line_num].directory[message.src]=1;
                    // cout<<"m31a\n";
                    msg temp; 
                    temp.src=message.src;
                    temp.dest=owner;
                    temp.addr=message.addr;
                    temp.msg_type=GET;
                    p_cache[owner].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                }
            }
        }
        else if(message.msg_type==GETX){
            // cout<<"m32\n";
            if(state==-1)   banks[index].miss++;
            else banks[index].hits++;
            bank_line current_line=banks[index].bank_structure[set][line_num];
            if(banks[index].bank_structure[set][line_num].pending==1){
                // cout<<"m33\n";
                msg temp;
                temp.src=index+8;
                temp.dest=message.src;
                temp.addr=message.addr;
                temp.msg_type=NACK;
                p_cache[temp.dest].msg_queue.push(temp);
                msg_counts[temp.msg_type]++;
            }
            else{
                // cout<<"m34\n";
                if(banks[index].bank_structure[set][line_num].dirty==0){
                    // cout<<"m35\n";
                    for(int i=0; i<8;i++){
                        if(banks[index].bank_structure[set][line_num].directory[i]==1){
                            // cout<<"m36\n";
                            msg temp;
                            temp.src=message.src;
                            temp.dest=i;
                            temp.msg_type=INV;
                            temp.addr=message.addr;
                            p_cache[temp.dest].msg_queue.push(temp);
                            msg_counts[temp.msg_type]++;
                            // current_line.directory[i]=-1;
                        }
                    }
                    msg temp;
                    temp.src=message.dest;
                    temp.msg_type=PUTX;
                    temp.dest=message.src;
                    temp.addr=message.addr;
                    temp.bit_v=banks[index].bank_structure[set][line_num].directory;
                    p_cache[temp.dest].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                    for(int i=0; i<8;i++){
                        if(banks[index].bank_structure[set][line_num].directory[i]==1){
                            // current_line.directory[i]=-1;
                            // cout<<"m37\n";
                            banks[index].bank_structure[set][line_num].directory[i]=-1;

                        }
                    }
                    // current_line.dirty=1;
                    // current_line.directory[temp.dest]=1;
                    banks[index].bank_structure[set][line_num].dirty=1;
                    banks[index].bank_structure[set][line_num].directory[temp.dest]=1;
                }
                else{
                    // cout<<"m38\n";
                    banks[index].bank_structure[set][line_num].pending=1;
                    int owner;
                    for(int i=0;i<8;i++){
                        if(banks[index].bank_structure[set][line_num].directory[i]==1){
                            // cout<<"m39\n";
                            owner=i;
                            banks[index].bank_structure[set][line_num].directory[i]=-1;
                            break;
                        }
                    }
                    banks[index].bank_structure[set][line_num].directory[message.src]=1;
                    msg temp;
                    temp.src=message.src;
                    temp.dest=owner;
                    temp.addr=message.addr;
                    temp.msg_type=GETX;
                    p_cache[owner].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                }
            }
        }

        else if(message.msg_type==UPGR){
            // cout<<"m40\n";
            if(state==-1)   banks[index].miss++;
            else banks[index].hits++;
            // bank_line current_line=banks[index].bank_structure[set][line_num];
            if(banks[index].bank_structure[set][line_num].pending==1){
                // cout<<"m41\n";
                msg temp;
                temp.src=index+8;
                temp.dest=message.src;
                temp.addr=message.addr;
                temp.msg_type=NACK;
                p_cache[temp.dest].msg_queue.push(temp);
                msg_counts[temp.msg_type]++;
            }
            else{
                // cout<<"m42\n";
                if(banks[index].bank_structure[set][line_num].dirty==0){
                        // cout<<"m43\n";
                        for(int i=0; i<8;i++){
                            if(banks[index].bank_structure[set][line_num].directory[i]==1 && i!=message.src){
                                // cout<<"m44\n";
                                msg temp;
                                temp.src=message.src;
                                temp.dest=i;
                                temp.msg_type=INV;
                                temp.addr=message.addr;
                                p_cache[temp.dest].msg_queue.push(temp);
                                msg_counts[temp.msg_type]++;
                                // current_line.directory[i]=-1;
                            }
                        }
                        msg temp;
                        temp.src=message.dest;
                        temp.msg_type=UPGR_ACK;
                        temp.dest=message.src;
                        temp.addr=message.addr;
                        temp.bit_v=banks[index].bank_structure[set][line_num].directory;
                        p_cache[temp.dest].msg_queue.push(temp);
                        msg_counts[temp.msg_type]++;
                        for(int i=0; i<8;i++){
                            if(banks[index].bank_structure[set][line_num].directory[i]==1){
                                // cout<<"m45\n";
                                banks[index].bank_structure[set][line_num].directory[i]=-1;
                            }
                        }
                        banks[index].bank_structure[set][line_num].dirty=1;
                        banks[index].bank_structure[set][line_num].directory[temp.dest]=1;
                }
                else{
                    // cout<<"m46\n";
                    int owner;
                    for(int i=0;i<8;i++){
                        if(banks[index].bank_structure[set][line_num].directory[i]==1){
                            // cout<<"m47\n";
                            owner=i;
                            banks[index].bank_structure[set][line_num].directory[i]=-1;
                            break;
                        }
                    }
                    msg temp;
                    temp.src=message.src;
                    temp.dest=owner;
                    temp.addr=message.addr;
                    temp.msg_type=GETX;
                    banks[index].bank_structure[set][line_num].pending=1;
                    banks[index].bank_structure[set][line_num].directory[owner]=-1;
                    banks[index].bank_structure[set][line_num].directory[message.src]=1;
                    p_cache[owner].msg_queue.push(temp);
                    msg_counts[temp.msg_type]++;
                }
            }    
        }

        else if(message.msg_type==ACK){     //Eviction buffer handling 
            // cout<<"m48\n";
            // bank_line current_line=banks[index].bank_structure[set][line_num];
            banks[index].bank_structure[set][line_num].pending=0;
            auto it=banks[index].eviction_buffer.find(key_address);
            if(it != banks[index].eviction_buffer.end()){
                // cout<<"m49\n";
                l2_evicted tmpo=it->second;
                for(int i=0; i<8; i++){
                    if(tmpo.bit_v[i]==1){
                        // cout<<"m50\n";
                        msg temp;
                        temp.msg_type=INV;
                        temp.src=index+8;
                        temp.dest=i;
                        temp.addr=message.addr;
                        p_cache[i].msg_queue.push(temp);
                        msg_counts[temp.msg_type]++;
                    }
                }
            }
            banks[index].eviction_buffer.erase(key_address);
        }
        else if(message.msg_type==SWB){
            // cout<<"m51\n";
            // bank_line current_line=banks[index].bank_structure[set][line_num];
            banks[index].bank_structure[set][line_num].pending=0;
            auto it=banks[index].eviction_buffer.find(key_address);
            if(it != banks[index].eviction_buffer.end()){
                // cout<<"m52\n";
                l2_evicted tmpo=it->second;
                for(int i=0; i<8; i++){
                    if(tmpo.bit_v[i]==1){
                        // cout<<"m53\n";
                        msg temp;
                        temp.msg_type=INV;
                        temp.src=index+8;
                        temp.dest=i;
                        temp.addr=message.addr;
                        p_cache[i].msg_queue.push(temp);
                        msg_counts[temp.msg_type]++;
                    }
                }
            }
        banks[index].eviction_buffer.erase(key_address);
        }   
        else if(message.msg_type==WB){
            // cout<<"m54\n";
            // bank_line current_line=banks[index].bank_structure[set][line_num];
            if(banks[index].bank_structure[set][line_num].pending==0){
                // cout<<"m55\n";
                banks[index].bank_structure[set][line_num].dirty=0;
                for(auto &e: banks[index].bank_structure[set][line_num].directory){
                    e=-1;
                }
                msg temp;
                temp.msg_type=WB_ACK;
                temp.src=message.dest;
                temp.dest=message.src;
                temp.addr=message.addr;
                p_cache[temp.dest].msg_queue.push(temp);
                msg_counts[temp.msg_type]++;
            }    
            else{
                // cout<<"m56\n";
                int dest;
                for(int i=0;i<8; i++){
                    if(i != message.src && banks[index].bank_structure[set][line_num].directory[i]==1){
                        // cout<<"m57\n";
                        dest=i;
                        break;
                    }
                }
                msg temp;
                banks[index].bank_structure[set][line_num].directory[message.src]=-1;
                banks[index].bank_structure[set][line_num].dirty=1;
                vector<int> faking(8,-1);
                temp.msg_type=PUTX;
                temp.src=index+8;
                temp.dest=dest;
                temp.addr=message.addr;
                temp.bit_v=faking;
                p_cache[temp.dest].msg_queue.push(temp);
                msg_counts[temp.msg_type]++;
                banks[index].bank_structure[set][line_num].pending=0;

                temp.msg_type=WB_ACK;
                temp.dest=message.src;
                p_cache[temp.dest].msg_queue.push(temp);
                msg_counts[temp.msg_type]++;
            }
        }
    }
}



int main(){
    ifstream t0("thread0.out");
    ifstream t1("thread1.out");
    ifstream t2("thread2.out");
    ifstream t3("thread3.out");
    ifstream t4("thread4.out");
    ifstream t5("thread5.out");
    ifstream t6("thread6.out");
    ifstream t7("thread7.out");

    bool is_all_empty[24]={false};
    bool flag=true;
    ull addr, global_count;
    int op_type;
    vector<thread_entry> min_gc_finder(8);
    thread_entry temp;
    ull nack=0;

    do{
        flag=true;
        min_gc_finder.clear();
        if(t0>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.op_type=op_type;
            temp.global_count=global_count;
            temp.tid=0;
            p_cache[0].trace_queue.push(temp);
        }
        if(t1>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.tid=1;
            temp.op_type=op_type;
            temp.global_count=global_count;
            p_cache[1].trace_queue.push(temp);
            // cout<<"t0 read\n";
        }
        if(t2>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.tid=2;
            temp.op_type=op_type;
            temp.global_count=global_count;
            p_cache[2].trace_queue.push(temp);
            // cout<<"t0 read\n";
        }
        if(t3>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.tid=3;
            temp.op_type=op_type;
            temp.global_count=global_count;
            p_cache[3].trace_queue.push(temp);
            // cout<<"t0 read\n";
        }
        if(t4>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.tid=4;
            temp.op_type=op_type;
            temp.global_count=global_count;
            p_cache[4].trace_queue.push(temp);
            // cout<<"t0 read\n";
        }
        if(t5>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.tid=5;
            temp.op_type=op_type;
            temp.global_count=global_count;
            p_cache[5].trace_queue.push(temp);
            // cout<<"t0 read\n";
        }
        if(t6>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.tid=6;
            temp.op_type=op_type;
            temp.global_count=global_count;
            p_cache[6].trace_queue.push(temp);
            // cout<<"t0 read\n";
        }
        if(t7>>addr>>op_type>>global_count){
            temp.addr=addr;
            temp.tid=7;
            temp.op_type=op_type;
            temp.global_count=global_count;
            p_cache[7].trace_queue.push(temp);
            // cout<<"t0 read\n";
        }
        for(int i=0;i<8;i++){
            if(!p_cache[i].trace_queue.empty()){
                temp=p_cache[i].trace_queue.front();
                min_gc_finder.push_back(temp);
                // cout<<"min_gc created\n";
            }
        }
        sort(min_gc_finder.begin(), min_gc_finder.end(), comparator);
        // cout<<"sorted\n";

        for(int i=0;i<8;i++){
            p_cache[i].initialize();
        }

        for(auto e: min_gc_finder){
            // cout<<e.tid<<endl;
        }
        vector<int> to_process;  //It will keep the cores corresponding to which we can process in this cycle.
        for(int i=0; i<min_gc_finder.size();i++){
            if(min_gc_finder[i].global_count == latest_consumed+1){
                to_process.push_back(min_gc_finder[i].tid);
                latest_consumed++;
            }
        }
        vector<int> l1_temp, l2_temp; // These will keep track of the msg queues which are not empty
        for(int i=0;i<8;i++){
            if(!p_cache[i].msg_queue.empty()){
                l1_temp.push_back(1);
            }
            else{
                l1_temp.push_back(0);
            }

            if(!banks[i].msg_queue.empty()){
                l2_temp.push_back(1);
            }
            else{
                l2_temp.push_back(0);
            }
        }
        vector<int>key_val;
        bool can_process=true;
        for(int i=0;i<8;i++){
            if(!p_cache[i].nack_count.empty()){
                for(auto &e: p_cache[i].nack_count){
                    if(e.second.second==0){
                        msg temp;
                        temp.src=i;
                        ull original_addr=e.first<<6;
                        temp.dest=calculate_home_bank(original_addr)+8;
                        temp.msg_type=e.second.first;
                        temp.addr=original_addr;
                        banks[temp.dest-8].msg_queue.push(temp);
                        msg_counts[temp.msg_type]++;
                        // nack++;
                        key_val.push_back(e.first);
                        can_process=false;
                    }
                    // else{
                        e.second.second--;
                        // cout<<e.second.second<<endl;
                }
            }
            for(int i=0;i<key_val.size();i++){
                p_cache[i].nack_count.erase(key_val[i]);
            }
            key_val.clear();
            bool nack_flag=p_cache[i].nack_count.empty();
            // cout<<nack_flag<<endl;
            flag=flag & nack_flag;
        }
        
        if(can_process){
            for(auto e : to_process){ // This one will process heads of each trace queue which is eligible to process ( by to_process )
                thread_entry current=p_cache[e].trace_queue.front();
                p_cache[e].trace_queue.pop();
                // p_cache[e].ts++;
                process_thread_entry(current);  //current-> thread_entry 
            }

        }
        for(int i=0;i<8;i++){
            if(l1_temp[i]){
                msg temp=p_cache[i].msg_queue.front();
                p_cache[i].msg_queue.pop();
                process_message(i,temp);
            }
        }
        for(int i=0;i<8;i++){
            if(l2_temp[i]){
                msg temp=banks[i].msg_queue.front();
                banks[i].msg_queue.pop();
                process_message(i+8, temp);
            }
        }


        

        //checking the NACk counter
        

        cycle++;
        // cout<<"Latest consumd : "<<latest_consumed<<endl;
        for(int i=0;i<8;i++){
            if(p_cache[i].trace_queue.empty()){
                is_all_empty[i]=true;
                // cout<<"true"<<endl;
            }
            else{
                is_all_empty[i]=false;
                // cout<<"false"<<endl;
            }
            flag = flag & is_all_empty[i];

            if(p_cache[i].msg_queue.empty()){
                is_all_empty[i+8]=true;
                // cout<<"true"<<endl;
            }
            else{
                is_all_empty[i+8]=false;
                // cout<<"false"<<endl;
            }
            flag = flag & is_all_empty[i+8];
            if(banks[i].msg_queue.empty()){
                is_all_empty[i+16]=true;
                // cout<<"true"<<endl;
            }
            else{
                is_all_empty[i+16]=false;
                // cout<<"false"<<endl;
            }
            flag = flag & is_all_empty[i+16];        
        }
        // cout<<"flag :::"<<flag<<endl;
    }while(!flag); //think about it later
    // cout<<".......................\n";
    cout<<"Cycles simulated : "<<cycle<<endl;
    cout<<".....................................\n";
    cout<<"Hits and miss for private caches\n";
    for(int i=0;i<8;i++){
        cout<<"core number :"<<i<<"  Hits : "<<p_cache[i].hits<<"     Read Miss : "<<p_cache[i].read_miss<<"     Write Miss : "<<p_cache[i].write_miss<<"     Upgrade Miss : "<<p_cache[i].upgrade_miss<<endl;
    }
    cout<<".....................................\n";
    cout<<"Hits and Miss for shared banks\n";
    for(int i=0;i<8;i++){
        cout<<"Bank Id : "<<i<<"  Hits : "<<banks[i].hits<<"     Miss : "<<banks[i].miss<<endl;
    }
    cout<<".....................................\n";
    cout<<"Count of messages at private caches:\n";
    for(auto e: l1_msg_counts){
        if(e.first==1){
            cout<<"GET : "<<e.second<<endl;
        }
        if(e.first==2){
            cout<<"GETX : "<<e.second<<endl;
        }
        if(e.first==3){
            cout<<"PUT : "<<e.second<<endl;
        }
        if(e.first==4){
            cout<<"PUTX : "<<e.second<<endl;
        }
        if(e.first==5){
            cout<<"UPGR : "<<e.second<<endl;
        }
        if(e.first==6){
            cout<<"INV : "<<e.second<<endl;
        }
        if(e.first==7){
            cout<<"ACK : "<<e.second<<endl;
        }
        if(e.first==8){
            cout<<"NACK : "<<e.second<<endl;
        }
        if(e.first==9){
            cout<<"SWB : "<<e.second<<endl;
        }
        if(e.first==10){
            cout<<"UPGR_ACK : "<<e.second<<endl;
        }
        if(e.first==11){
            cout<<"WB : "<<e.second<<endl;
        }
        if(e.first==12){
            cout<<"WB_ACK : "<<e.second<<endl;
        }
    }
    cout<<".....................................\n";
    cout<<"Count of messages at L2 caches:\n";
    for(auto e: l2_msg_counts){
        if(e.first==1){
            cout<<"GET : "<<e.second<<endl;
        }
        if(e.first==2){
            cout<<"GETX : "<<e.second<<endl;
        }
        if(e.first==3){
            cout<<"PUT : "<<e.second<<endl;
        }
        if(e.first==4){
            cout<<"PUTX : "<<e.second<<endl;
        }
        if(e.first==5){
            cout<<"UPGR : "<<e.second<<endl;
        }
        if(e.first==6){
            cout<<"INV : "<<e.second<<endl;
        }
        if(e.first==7){
            cout<<"ACK : "<<e.second<<endl;
        }
        if(e.first==8){
            cout<<"NACK : "<<e.second<<endl;
        }
        if(e.first==9){
            cout<<"SWB : "<<e.second<<endl;
        }
        if(e.first==10){
            cout<<"UPGR_ACK : "<<e.second<<endl;
        }
        if(e.first==11){
            cout<<"WB : "<<e.second<<endl;
        }
        if(e.first==12){
            cout<<"WB_ACK : "<<e.second<<endl;
        }
    }
    cout<<".....................................\n";
    return 0;
}
