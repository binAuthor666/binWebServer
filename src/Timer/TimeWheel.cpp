#include"TimeWheel.h"

TimeWheel::TimeWheel():cur_slot(0){
    for(int i=0;i<N;i++){
        slots[i]=nullptr;
    }
}

TimeWheel::~TimeWheel(){

    for(int i=0;i<N;i++){
        TWTimer* tmp=slots[i];
        while(tmp){
            slots[i]=tmp->next;
            delete tmp;
            tmp=slots[i];
        }
    }
}

TWTimer* TimeWheel::add_timer(int timeout){
    printf("in TimeWheel::add_timer\n");
    fflush(stdout);
    
    if(timeout<0){
        return nullptr;
    }
    int ticks=0;
    if(timeout<SI){
        ticks=1;
    }
    else{
        ticks=timeout/SI;
    }

    int rotation=ticks/N;

    int slot=(cur_slot+(ticks%N))%N;

    TWTimer *timer=new TWTimer(rotation,slot);

    printf("add timer,rotation is %d,slot is %d,cur_slot is %d\n",rotation,slot,cur_slot);
    fflush(stdout);
    //this slot is null
    if(!slots[slot]){
        printf("this slot is null when add timer\n");
        fflush(stdout);
        slots[slot]=timer;
    }
    else{
        printf("this slot is not null when add timer\n");
        fflush(stdout);
        timer->next=slots[slot];
        slots[slot]->prev=timer;
        slots[slot]=timer;
    }

    return timer;
}

void TimeWheel::del_timer(TWTimer* timer){
    printf("into TimeWheel::del_timer\n");
    fflush(stdout);
    if(!timer){
        return;
    }

    int slot=timer->time_slot;
    //is head
    if(timer==slots[slot]){
        slots[slot]=timer->next;
        if(slots[slot]){
            slots[slot]->prev=nullptr;
        }
        delete timer;
    }
    else{
        timer->prev->next=timer->next;
        if(timer->next)
            timer->next->prev=timer->prev;
        delete timer;
    }
}

//after SI time,to next slot
void TimeWheel::tick(){
    TWTimer *tmp=slots[cur_slot];
    printf("current slot is %d\n",cur_slot);
    while(tmp){
        printf("tick the timer %d once\n",tmp->time_slot);
        printf("rotation:%d\n",tmp->rotation);
        //this round is not arrived
        if(tmp->rotation>0){
            printf("this round is not arrived\n");
            fflush(stdout);
            tmp->rotation--;
            tmp=tmp->next;
        }else{
            printf("this round is arrived\n");
            fflush(stdout);
            if(tmp->cb_func==nullptr){
                printf("timer cbfunc is null\n");
            }else{
                printf("timer cbfunc is not null\n");
                tmp->cb_func(tmp->clientData);
            }
            
            //is head
            if(tmp==slots[cur_slot]){
                printf("delete header in cur_slot\n");
                fflush(stdout);
                slots[cur_slot]=tmp->next;
                if(slots[cur_slot]){
                    slots[cur_slot]->prev=nullptr;
                }
                delete tmp;
                tmp=slots[cur_slot];
            }
            else{
                printf("delete body in cur_slot\n");
                fflush(stdout);
                tmp->prev->next=tmp->next;
                if(tmp->next){
                    tmp->next->prev=tmp->prev;
                }
                TWTimer* tmp2=tmp->next;
                delete tmp;
                tmp=tmp2;
            }

        }
    }
    cur_slot=(cur_slot+1)%N;
}