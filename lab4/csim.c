// Andrew id: haoyangy
// Name: Haoyang Yuan


#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>


//use Bi-directional list to achieve LINE in SET
struct node {
    unsigned int valid;
    unsigned long pos;
    struct node *next;
    struct node *pre;
};



int main(int argc, char* argv[]){
    unsigned int hits=0, misses=0, evictions=0;
    unsigned int s=0, E=0, b=0;
    char *t = NULL;
    unsigned int set=0;
    unsigned long tag=0,offset=0;
    int option;
    //Specifying the expected options
    //read parameters from the command line
    while ((option = getopt(argc, argv,"s:E:b:t:")) != -1) {
        switch (option) {
            case 's' : s = atoi(optarg);
                break;
            case 'E' : E = atoi(optarg);
                break;
            case 'b' : b = atoi(optarg);
                break;
            case 't' : t = optarg;
                break;
            default:
                 printf ("?? getopt returned character code 0%o ??\n", option);
        }
    }
    
    //arrange parameters
    int sets = 1<<s;
    struct node *temp;

    //use node** to implement an array of bidirectional node rings.
    int i,j;
    struct node **root = NULL;
    root = (struct node **)malloc(sets * sizeof(struct node*));
    //each head of the ring
    for(i = 0; i < sets; ++i){
        root[i] = malloc(sizeof(struct node));
        root[i]->pos = 0;
        root[i]->valid = 0;
    }
    //detail implementtion of nodes in rings, link them
    for(i = 0; i < sets; ++i){
        temp = root[i];
        for(j = 0; j < E-1; ++j){
            struct node *new = malloc(sizeof(struct node));
            new->pos = 0;
            new->valid = 0;
            temp->next = new;
            new->pre = temp;
            temp = temp->next;
        }
        temp->next = root[i];
        root[i]->pre = temp;
    }
 
    
    
    FILE * pFile; //pointer to FILE object
    if(strlen(t)==0){
        printf("wrong file address\n");
        return -1;
    }
  
    pFile = fopen (t,"r"); //open file for reading
    if(pFile==NULL){
        printf(" file can not open\n");
        return -1;
    }
    //using mask to draw out the tag and set information
    offset = (1<<b)-0x1;
    unsigned long setmask = ( (1<<b)<<s)-0x1 - offset;
    unsigned long tagmask = ULONG_MAX-setmask-offset;
    char identifier;
    unsigned long address;
    int size;
    int flag = 0;
    // Reading lines like " M 20,1" or "L 19,3"
    while(fscanf(pFile," %c %lx,%d", &identifier, &address, &size)>0)
    {
        if(identifier=='I'){
            continue;
        }
        //get tag and set for this line
        tag = (address & tagmask)>>b>>s;
        set = (int)(address & setmask)>>b;
      

        //reset the flag and start point
        flag = 0;
        temp = root[set];
      //for the command S and L
        if(identifier=='S'||identifier=='L'){
         
            for(i=0;i<E;i++){
                
                
                //printf("pos%lx\n",temp->pos);
                //check if hit, roll the ring for one node
                if(temp->pos == tag&&temp->valid==1){
                    
                    
                    hits = hits + 1;
                    flag = 1;
                    
                    if(temp==root[set]){
                        root[set] = root[set]->next;
                        break;
                    }
                    temp->next->pre = temp->pre;
                    temp->pre->next = temp->next;
                    
                    root[set]->pre->next = temp;
                    temp->pre = root[set]->pre;
                    root[set]->pre = temp;
                    
                    temp->next = root[set];
                    
                    break;
                }
                temp = temp->next;
                
                
            }
            
            //hit and continue!
            if(flag == 1){

                continue ;}
            else{
                //check if there is more empty invalid block in line
                temp = root[set];
                for(i=0;i<E;i++){
                    if(temp->valid ==0){
                        misses = misses + 1;
                        temp->pos = tag;
                        temp->valid = 1;
                        flag = 1;
                        root[set]=root[set]->next;
                        
                        break;
                    }
                    temp = temp->next;
                    
                }
                if(flag == 1){
                    // printf("empty space!");
                    //using empty space
                    continue ;
                }
                //miss, evict the head of ring, load new block,
                //and move the start of ring to next one
                evictions = evictions + 1;
                misses = misses + 1;
                root[set]->pos = tag;
                root[set]->valid = 1;
                root[set] = root[set]->next;
            }
        }
        //for situation with command M, we do the same as previously
        else if(identifier=='M'){
            flag = 0;
            temp = root[set];
            
            for(i=0;i<E;i++){
        
                
                //printf("pos%lx\n",temp->pos);
                if(temp->pos == tag&&temp->valid==1){
                    
                    
                    hits = hits + 2;
                    flag = 1;
                    
                    if(temp==root[set]){
                        root[set] = root[set]->next;
                        break;
                    }
                    temp->next->pre = temp->pre;
                    temp->pre->next = temp->next;
                    
                    root[set]->pre->next = temp;
                    temp->pre = root[set]->pre;
                    root[set]->pre = temp;
                    
                    temp->next = root[set];
                    
                    break;
                }
                temp = temp->next;
       
            }
            //hit!
            //printf("%d,%d,%d",hits,misses,evictions);
            if(flag == 1){ continue ;}
            else{
                temp = root[set];
                for(i=0;i<E;i++){
                    if(temp->valid ==0){
                        misses = misses + 1;
                        hits = hits + 1;
                        temp->pos = tag;
                        temp->valid = 1;
                        flag = 1;
                        root[set]=root[set]->next;
                        break;
                    }
                    temp = temp->next;
                    
                }
                if(flag == 1){
                    continue ;
                }
                
                evictions = evictions + 1;
                misses = misses + 1;
                hits = hits + 1;
                root[set]->pos = tag;
                root[set]->valid = 1;
                root[set] = root[set]->next;
                
            }
        }
        else{
            continue;
        }
    }
    //remember to close file when done
    fclose(pFile);
    //free the cache
    for(i = 0; i < sets; ++i){
        temp = root[i];
    
        while(temp->next!=NULL){
            if(temp->next==temp){
                free(temp);
                temp = NULL;
                break;
            }
            temp->next->pre = temp->pre;
            temp->pre->next = temp->next;
            free(temp);
            temp = temp->next;
            
            
        }
    }
    free(root);
    
    
    //out put results
    printSummary(hits, misses, evictions);
    return 0;
    
    
    
    
}
