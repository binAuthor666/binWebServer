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

    //this slot is null
    if(!slots[slot]){
        printf("add timer,rotation is %d,slot is %d,cur_slot is %d\n",rotation,slot,cur_slot);
        slots[slot]=timer;
    }
    else{
        timer->next=slots[slot];
        slots[slot]->prev=timer;
        slots[slot]=timer;
    }
    return timer;
}

void TimeWheel::del_timer(TWTimer* timer){
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
        //this round is not arrived
        if(tmp->rotation>0){
            tmp->rotation--;
            tmp=tmp->next;
        }else{
            tmp->cb_func(tmp->clientData);
            //is head
            if(tmp==slots[cur_slot]){
                printf("delete header in cur_slot\n");
                slots[cur_slot]=tmp->next;
                if(slots[cur_slot]){
                    slots[cur_slot]->prev=nullptr;
                }
                delete tmp;
                tmp=slots[cur_slot];
            }
            else{
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