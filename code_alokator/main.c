#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "custom_unistd.h"

#if defined(malloc)
#undef malloc
#endif
#if defined(free)
#undef freee
#endif
#if defined(calloc)
#undef calloc
#endif
#if defined(realloc)
#undef realloc
#endif
// do naprawy
// malloc_aligned sie wykrzacza gdy spróbowałem 5600
// testy jednostkowe i ten debug
#define PAGE_SIZE       4096    // Długość strony w bajtach
#define PAGE_FENCE      1       // Liczba stron na jeden płotek
#define PAGES_AVAILABLE 16384   // Liczba stron dostępnych dla sterty
#define PAGES_TOTAL     (PAGES_AVAILABLE + 2 * PAGE_FENCE)
#define FENCE_AMMOUNT   4
#define FENCE_SIZE  sizeof(int64_t)

enum pointer_type_t
{
  pointer_null,
  pointer_out_of_heap,
  pointer_control_block,
  pointer_inside_data_block,
  pointer_unallocated,
  pointer_valid
};

int heap_setup(void);
int heap_get_more_memory(void);
int heap_validate(void);
// podstawowe funkcje
void* heap_malloc(size_t count);
void* heap_calloc(size_t,size_t);
void heap_free(void *memblock);
void* heap_realloc(void *memblock,size_t size);
//debug funkcje
void* heap_malloc_debug(size_t count,int fileline,const char* filename);
void* heap_calloc_debug(size_t number,size_t size,int fileline,const char* filename);
void* heap_realloc_debug(void* memblock ,size_t size,int fileline,const char* filename);
#define malloc(_size) heap_malloc_debug(_size,__LINE__,__FILE__)
#define calloc(_number,_size) heap_calloc_debug(_number,_size,__LINE__,__FILE__)
#define realloc( _memblock, _size) heap_realloc_debug(_memblock,_size,__LINE__,__FILE__)
// funkcje "aligned"
void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t,size_t);
void* heap_realloc_aligned(void *memblock,size_t size);
void* heap_malloc_aligned_PomocDoRealoca(size_t count);
// funkcje "aligned debug"
void* heap_malloc_aligned_debug(size_t count,int fileline,const char* filename);
void* heap_calloc_aligned_debug(size_t number,size_t size,int fileline,const char* filename);
void* heap_realloc_aligned_debug(void* memblock ,size_t size,int fileline,const char* filename);
#define malloc_aligned(_size) heap_malloc_debug(_size,__LINE__,__FILE__)
#define calloc_aligned(_number,_size) heap_calloc_debug(_number,_size,__LINE__,__FILE__)
#define realloc_aligned( _memblock, _size) heap_realloc_debug(_memblock,_size,__LINE__,__FILE__)
//Funkcje kontrolujące stertę:
size_t heap_get_used_space(void);
size_t heap_get_largest_used_block_size(void);
uint64_t heap_get_used_blocks_count(void);
size_t heap_get_free_space(void);
size_t heap_get_largest_free_area(void);
uint64_t heap_get_free_gaps_count(void);
//dalej
enum pointer_type_t get_pointer_type(const void* pointer);
void* heap_get_data_block_start(const void* pointer);
size_t heap_get_block_size(const void* memblock);
void heap_dump_debug_information(void);

void display_size_heap();

struct node_t
{
  long long int size;
  
  struct node_t *next;
  struct node_t *prev;
};
struct memory_t
{
  int total_size;
  int free_size;
  int64_t fence[FENCE_AMMOUNT];
  void * start;
  void * end;
  struct node_t *head;
  struct node_t *tail;
}mem;

int main(int argc, char **argv)
{
  printf("Struktura = %lu\nPłotek = %lu\n",sizeof(struct node_t),FENCE_AMMOUNT*FENCE_SIZE);
  int status = heap_setup();
  display_size_heap();
  printf("%p\n",mem.start);
  printf("%p %p\n",mem.head,mem.tail);
  printf("%p %p\n",(char*)mem.head+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+mem.head->size,mem.head->next);
  void* p1=heap_malloc_aligned(1000);
  void *p=p1;
  display_size_heap();
  printf("%s\n",heap_validate()?"Co zepsules?":"OK!");
  void* p2=malloc(1000);
  display_size_heap();
  printf("%s\n",heap_validate()?"Co zepsules?":"OK!");
  void* p3=heap_malloc_aligned(PAGE_SIZE+PAGE_SIZE);
  display_size_heap();
  printf("%s\n",heap_validate()?"Co zepsules?":"OK!");
  void* p4=heap_calloc_aligned(10,sizeof(double));
  display_size_heap();
  printf("%s\n",heap_validate()?"Co zepsules?":"OK!");
  printf("%lu",heap_get_used_space());
  void* p5=heap_malloc(5600);
  display_size_heap();
  printf("%s\n",heap_validate()?"Co zepsules?":"OK!");
  printf("%ld",heap_get_largest_used_block_size());
  printf("\n");
  heap_dump_debug_information();
	//printf("hello world\n");
	return 0;
}

int heap_setup(void){
  int result = 0;
  mem.start=custom_sbrk(PAGE_SIZE);
  if(*(int*)mem.start==-1) return -1;
  mem.end=(char*)mem.start+PAGE_SIZE;
  mem.total_size=PAGE_SIZE;
  struct node_t *st=(struct node_t*)(mem.start);
  struct node_t *en=(struct node_t*)((char*)mem.end-sizeof(struct node_t));
  for(int i=0;i<FENCE_AMMOUNT;++i){
    mem.fence[i]=rand();
  }
  st->size=-(PAGE_SIZE-2*sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
  memcpy((char*)st+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  memcpy((char*)en-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  st->prev=NULL;
  st->next=en;
  en->size=0;
  en->next=NULL;
  en->prev=st;
  mem.head=st;
  mem.tail=en;
  return result;
}
void* heap_malloc(size_t count){
  long long int x = count;
  if(!count) return NULL;
  struct node_t* zm=mem.head;
  while( (-zm->size) < x){
    zm=zm->next;
    if(!zm){
      if(heap_get_more_memory()) return NULL;
      return heap_malloc(count);
    }
  }
  void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  /* Daunie jak to jest czesc struktury to juz mam plotki
  memcpy((char*)zm+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  memcpy((char*)zm-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  */
  if(count+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > 
  -(zm->size)){
    zm->size=-(zm->size);
  }
  else{
    struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+count+2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->size=-(-zm->size-count-sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    tab->prev=zm;
    tab->next=zm->next;
    tab->next->prev=tab;
    zm->next=tab;
    zm->size=count;
  }
  return ptr;
}
//pomocznicza funkcja realloca (brak pytania o pamiec)
void* heap_malloc_doRealloca(size_t count){
  long long int x = count;
  if(!count) return NULL;
  struct node_t* zm=mem.head;
  while( (-zm->size) < x){
    zm=zm->next;
    if(!zm) return NULL;
  }
  void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  if(count+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > 
  -(zm->size)){
    zm->size=-(zm->size);
  }
  else{
    struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+count+2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->size=-(-zm->size-count-sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    tab->prev=zm;
    tab->next=zm->next;
    tab->next->prev=tab;
    zm->next=tab;
    zm->size=count;
  }
  return ptr;
}
//calloc
void * heap_calloc(size_t number,size_t size){
  long long int x = number*size;
  if(x<1) return NULL;
  struct node_t* zm=mem.head;
  while( (-zm->size) < x){
    zm=zm->next;
    if(!zm){
      if(heap_get_more_memory()) return NULL;
      return heap_calloc(number,size);
    }
  }
  void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  if(number*size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > 
  -(zm->size)){
    zm->size=-(zm->size);
  }
  else{
    struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+number*size+2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->size=-(-zm->size-number*size-sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    tab->prev=zm;
    tab->next=zm->next;
    tab->next->prev=tab;
    zm->next=tab;
    zm->size=number*size;
  }
  for(long long int i=0;i<x;i++){
    *((char*)ptr+i)=0;
  }
  return ptr;
}
//wlasna do wyswietlania
void display_size_heap(){
  struct node_t *zm=mem.head;
  int i=0;
  while(zm){
    printf("%d = %lld\n",++i,zm->size);
    zm=zm->next;
    if(i>25){
      printf("something is wrong!\n");
      break;
    }
  }printf("\n");
}
//free
void heap_free(void *memblock){
  if(!memblock) return;
  
  struct node_t *zm =mem.head;
  while(zm){
    void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
    if(memblock==ptr) break;
    zm=zm->next;
  }
  if(!zm||zm->size<0) return;
  if(((zm->prev&&zm->prev->size>0)||!zm->prev)&&zm->next->size>0){
    zm->size=-zm->size;
  }
  if(zm->next->size<0||(!zm->next->size&&zm->next&&zm->next->next)){
    zm->size=-(zm->size+sizeof(struct node_t)-zm->next->size+2*FENCE_SIZE*FENCE_AMMOUNT);
    zm->next=zm->next->next;
    zm->next->prev=zm;
  }
  if(zm->prev&&zm->prev->size<=0)
  { //5904 /-1808
    zm->prev->next=zm->next;
    zm->next->prev=zm->prev;
    if(zm->size<0) zm->prev->size+=zm->size-sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    else zm->prev->size-=zm->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
  }
}
//realloc
void* heap_realloc(void *memblock,size_t size){
  if(!memblock) return heap_malloc(size);
  if(!size) {heap_free(memblock); return NULL;}
  struct node_t *zm=(struct node_t*)((char*)memblock-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  if(size==zm->size) return memblock;
  int _size=size;
  if(zm->size<0) return NULL;
  if(zm->size > _size){
    if(zm->size-size>sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT){
      struct node_t* tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      tab->prev=zm;
      tab->next=zm->next;
      zm->next=zm->next->prev=tab;
      tab->size=-(zm->size-size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT);
      zm->size=size;
      heap_free(tab);
    }else{
    }
    return memblock;
  }            
  if(-zm->next->size > _size-zm->size){
      struct node_t temp;
      struct node_t* memory=zm->next;// pamiec bylego
      struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);//poprawne
      temp=*memory;
      *tab=temp;
      tab->next->prev=tab->prev->next=tab; 
      tab->size+=_size-zm->size;
      zm->size=size;
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence,          FENCE_AMMOUNT*FENCE_SIZE);
    return memblock;
  }
  if(-zm->next->size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > _size-zm->size){
    zm->size+=-zm->next->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    zm->next->next->prev=zm;
    zm->next=zm->next->next;
    return memblock;
  }
  void* ptr=heap_malloc_doRealloca(size);
  if(!ptr){
    if(heap_get_more_memory()) return NULL;
    return heap_realloc(memblock,size);
  }
  memcpy(ptr,memblock,zm->size);
  heap_free(memblock);
  return ptr;
}//daje pamiec
int heap_get_more_memory(void){
  void *ptr =custom_sbrk(PAGE_SIZE);
  if(!ptr) return 1;
  mem.total_size+=PAGE_SIZE;
  mem.end=(void*)((char*)mem.end+PAGE_SIZE);
  struct node_t *zm = (struct node_t*)((char*)mem.end-sizeof(struct node_t));
  mem.tail->next=zm;
  zm->prev=mem.tail;
  zm->next=NULL;
  zm->size=0;
  //mem.tail->size=-(int)(PAGE_SIZE-2*FENCE_SIZE*FENCE_AMMOUNT+sizeof(struct node_t));
  mem.tail=zm;
  memcpy((char*)zm->prev+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  memcpy((char*)zm-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);

  zm->prev->size=PAGE_SIZE-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT;
  heap_free((char*)zm->prev+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  return 0;
}
int heap_validate(void){
  struct node_t *zm=mem.head;
  while(zm){
    if(zm->prev){
      if(memcmp((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT)) return -1;
      //printf("Ok before!\n");
    }
    if(zm->next){
      if(memcmp((char*)zm+sizeof(struct node_t), mem.fence, FENCE_SIZE*FENCE_AMMOUNT)) return -1;
      //printf("Ok after!\n");
    }
    zm=zm->next;
  }
  return 0;
}
//rodzina debuga
//malloc
void* heap_malloc_debug(size_t count,int fileline,const char* filename){
  printf("Wywolane z linii %d z pliku %s\n",fileline,filename);
  long long int x = count;
  if(!count) return NULL;
  struct node_t* zm=mem.head;
  while((-zm->size) < x){
    zm=zm->next;
    if(!zm){
      printf("Próba przyznania nowej pamięci\n");
      if(heap_get_more_memory()){
        printf("Nie przyznano miejsca\n");
        return NULL;
      }
      printf("przyznano miejsce,ponowne wywołanie funkcji\n");
      return malloc(count);
    }
  }
  void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  if(count+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > 
  -(zm->size)){
    zm->size=-(zm->size);
  }
  else{
    struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+count+2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->size=-(-zm->size-count-sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    tab->prev=zm;
    tab->next=zm->next;
    tab->next->prev=tab;
    zm->next=tab;
    zm->size=count;
  }
  printf("Nowy blok: %p wielkosc w bajtach: %lld\n",zm,zm->size);
  return ptr;
}//calloc
void* heap_calloc_debug(size_t number,size_t size,int fileline,const char* filename){
  printf("Wywolane z linii %d z pliku %s\n",fileline,filename);
  long long int x = number*size;
  if(!number*size) return NULL;
  struct node_t* zm=mem.head;
  while((-zm->size) < x){
    zm=zm->next;
    if(!zm){
      printf("Próba przyznania nowej pamięci\n");
      if(heap_get_more_memory()){
        printf("Nie przyznano miejsca\n");
        return NULL;
      }
      printf("przyznano miejsce,ponowne wywołanie funkcji\n");
      return calloc(number,size);
    }
  }
  void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  if(number*size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > 
  -(zm->size)){
    zm->size=-(zm->size);
  }
  else{
    struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+number*size+2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->size=-(-zm->size-number*size-sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    tab->prev=zm;
    tab->next=zm->next;
    tab->next->prev=tab;
    zm->next=tab;
    zm->size=number*size;
  }
  for(long long int i=0;i<x;i++){
    *((char*)ptr+i)=0;
  }
  printf("Nowy blok: %p wielkosc w bajtach: %lld\n",zm,zm->size);
  return ptr;
}//realloc
void* heap_realloc_debug(void* memblock ,size_t size,int fileline,const char* filename){
  printf("Wywolane z linii %d z pliku %s\n",fileline,filename);
  if(!memblock) return heap_malloc(size);
  if(!size) {heap_free(memblock); return NULL;}
  struct node_t *zm=(struct node_t*)((char*)memblock-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  if(size==zm->size) return memblock;
  int _size=size;
  if(zm->size<0) return NULL;
  if(zm->size > _size){
    if(zm->size-size>sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT){ 
      struct node_t* tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      tab->prev=zm;
      tab->next=zm->next;
      zm->next=zm->next->prev=tab;
      tab->size=-(zm->size-size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT);
      zm->size=size;
      heap_free(tab);
    }else{
    }
    printf("Zmniejszono blok: %p wielkosc: %lld\n",memblock,zm->size);
    return memblock;
  }  
  if(-zm->next->size > _size-zm->size){
      struct node_t temp;
      struct node_t* memory=zm->next;// pamiec bylego
      struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);//poprawne
      temp=*memory;
      *tab=temp;
      tab->next->prev=tab->prev->next=tab; 
      tab->size+=_size-zm->size;
      zm->size=size;
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence,          FENCE_AMMOUNT*FENCE_SIZE);
    printf("Powiększono blok: %p wielkosc: %lld\n",memblock,zm->size);
    return memblock;
  }
  if(-zm->next->size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > _size-zm->size&&zm->next->size<=0){
    zm->size+=-zm->next->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    zm->next->next->prev=zm;
    zm->next=zm->next->next;
    printf("Powiększono blok: %p wielkosc: %lld\n",memblock,zm->size);
    return memblock;
  }
  printf("Próba realokacji\n");
  void* ptr=heap_malloc_doRealloca(size);
  if(!ptr){
    printf("Brak miejsca, pytanie o pamiec\n ");
    if(heap_get_more_memory()){
      printf("Brak pamieci\n");
      return NULL;
    }
    printf("Przyznano pamiec, ponowne realloc\n");
    return heap_realloc(memblock,size);
  }
  memcpy(ptr,memblock,zm->size);
  heap_free(memblock);
  printf("Powiększono oraz zmieniono pozycje bloku: %p wielkosc: %lld\n",memblock,zm->size);
  return ptr;
}
//rodzna aligned
//malloc
void* heap_malloc_aligned(size_t count){
  long long int x = count;
  if(!count) return NULL;
  struct node_t* m=mem.head;
  uint64_t liczba=0;
  struct node_t* choice=NULL;  
  while(1){
    while((char*)m<=(char*)mem.head+liczba){
      if(!m->next){
        if(heap_get_more_memory()){
          return NULL;
        } 
        continue;
      }
      m=m->next;
    }
    m=m->prev;
    if(m->size>=0){
      liczba+=PAGE_SIZE;
      continue;  
    }
    if((char*)mem.head+liczba+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+count < (char*)m->next-FENCE_SIZE*FENCE_AMMOUNT){
      //udalo sie zaalokawc, gg wp
      choice=(struct node_t*)((char*)mem.head+liczba);
      break;
    }
    liczba+=PAGE_SIZE;
  }
  if(mem.head==(void*)choice) return heap_malloc(count);
  struct node_t temp=*m;
  void* ptr=(void*)((char*)choice+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  int offset=0;
    for(;;++offset){
      if((char*)m+offset==(char*)choice) break;
    }//jak daleko od m znajduje sie choice
    //offset = struct+Fence+Pamiec+Fence
  //ogarniecie to z lewej
  if((char*)choice-FENCE_SIZE*FENCE_AMMOUNT>=(char*)m+FENCE_AMMOUNT*FENCE_SIZE){
    //mozna wstawic pomiedzy //nie moze byc to m
    choice->prev=m;
    choice->next=m->next;
    m->next=m->next->prev=choice;
    choice->size=m->size+(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT;
    m->size+=(sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+(-choice->size));
  }else{
    choice->prev=temp.prev;
    choice->next=temp.next;
    choice->next->prev=choice->prev->next=choice;
    choice->prev->size+=offset;
    choice->size=temp.size+offset;
  }
  //ogarniecie to z prawej
  if((char*)choice+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+ count<=(char*)choice->next-FENCE_SIZE*FENCE_AMMOUNT){
    //mozna wstawic pomiedzy
    struct node_t *zm=(struct node_t*)((char*)choice+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count);
    zm->next=choice->next;
    zm->prev=choice;
    zm->next->prev=zm->prev->next=zm;
    zm->size=choice->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count;
    choice->size=count;
    memcpy((char*)zm+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
    memcpy((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence,   FENCE_SIZE*FENCE_AMMOUNT);
  }else{
    choice->size*=-1;
  }
  memcpy((char*)choice+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
  memcpy((char*)choice-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT);
  return ptr;
}
//calloc
void* heap_calloc_aligned(size_t number,size_t size){
  long long int x = number*size;
  if(!number*size) return NULL;
  struct node_t* m=mem.head;
  uint64_t liczba=0;
  struct node_t* choice=NULL;  
  while(1){
    while((char*)m<=(char*)mem.head+liczba){
      if(!m->next){
        if(heap_get_more_memory()){
          return NULL;
        } 
        continue;
      }
      m=m->next;
    }
    m=m->prev;
    if(m->size>=0){
      liczba+=PAGE_SIZE;
      continue;  
    }
    if((char*)mem.head+liczba+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+number*size < (char*)m->next-FENCE_SIZE*FENCE_AMMOUNT){
      //udalo sie zaalokawc, gg wp
      choice=(struct node_t*)((char*)mem.head+liczba);
      break;
    }
    liczba+=PAGE_SIZE;
  }
  if(mem.head==(void*)choice) return heap_calloc(number,size);
  struct node_t temp=*m;
  void* ptr=(void*)((char*)choice+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  int offset=0;
    for(;;++offset){
      if((char*)m+offset==(char*)choice) break;
    }//jak daleko od m znajduje sie choice
    //offset = struct+Fence+Pamiec+Fence
  //ogarniecie to z lewej
  if((char*)choice-FENCE_SIZE*FENCE_AMMOUNT>=(char*)m+FENCE_AMMOUNT*FENCE_SIZE){
    //mozna wstawic pomiedzy //nie moze byc to m
    choice->prev=m;
    choice->next=m->next;
    m->next=m->next->prev=choice;
    choice->size=m->size+(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT;
    m->size+=(sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+(-choice->size));
  }else{
    choice->prev=temp.prev;
    choice->next=temp.next;
    choice->next->prev=choice->prev->next=choice;
    choice->prev->size+=offset;
    choice->size=temp.size+offset;
  }
  //ogarniecie to z prawej
  if((char*)choice+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+ number*size<=(char*)choice->next-FENCE_SIZE*FENCE_AMMOUNT){
    //mozna wstawic pomiedzy
    struct node_t *zm=(struct node_t*)((char*)choice+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+number*size);
    zm->next=choice->next;
    zm->prev=choice;
    zm->next->prev=zm->prev->next=zm;
    zm->size=choice->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+number*size;
    choice->size=number*size;
    memcpy((char*)zm+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
    memcpy((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence,   FENCE_SIZE*FENCE_AMMOUNT);
  }else{
    choice->size*=-1;
  }
  memcpy((char*)choice+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
  memcpy((char*)choice-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT);
  for(long int i=0;i<number*size;++i){
    *((char*)ptr+i)=0;
  }
  return ptr;
}//realoc aligned
void* heap_realloc_aligned(void *memblock,size_t size){
  if(!memblock) return heap_malloc_aligned(size);
  if(!size) {heap_free(memblock); return NULL;}
  struct node_t *zm=(struct node_t*)((char*)memblock-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  uint64_t liczba=0;
  while((char*)zm<(char*)mem.start+liczba){
    liczba+=PAGE_SIZE;
  }
  if((char*)zm!=(char*)mem.start+liczba){
    void *pt=heap_malloc_aligned(size);
    if(!pt) return NULL;
    memcpy(pt, zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT, zm->size);
    heap_free(zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
    return pt;
  }
  if(size==zm->size) return memblock;
  int _size=size;
  if(zm->size<0) return NULL;
  if(zm->size > _size){
    if(zm->size-size>sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT){
      struct node_t* tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      tab->prev=zm;
      tab->next=zm->next;
      zm->next=zm->next->prev=tab;
      tab->size=-(zm->size-size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT);
      zm->size=size;
      heap_free(tab);
    }else{
    }
    return memblock;
  }            
  if(-zm->next->size > _size-zm->size){
      struct node_t temp;
      struct node_t* memory=zm->next;// pamiec bylego
      struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);//poprawne
      temp=*memory;
      *tab=temp;
      tab->next->prev=tab->prev->next=tab; 
      tab->size+=_size-zm->size;
      zm->size=size;
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence,          FENCE_AMMOUNT*FENCE_SIZE);
    return memblock;
  }
  if(-zm->next->size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > _size-zm->size){
    zm->size+=-zm->next->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    zm->next->next->prev=zm;
    zm->next=zm->next->next;
    return memblock;
  }
  void* ptr=heap_malloc_aligned_PomocDoRealoca(size);
  if(!ptr){
    if(heap_get_more_memory()) return NULL;
    return heap_realloc_aligned(memblock,size);
  }
  memcpy(ptr,memblock,zm->size);
  heap_free(memblock);
  return ptr;
}// --------------------------------pomocnicza do realoca_aligned
void* heap_malloc_aligned_PomocDoRealoca(size_t count){
  long long int x = count;
  if(!count) return NULL;
  struct node_t* m=mem.head;
  uint64_t liczba=0;
  struct node_t* choice=NULL;  
  while(1){
    while((char*)m<=(char*)mem.head+liczba){
      if(!m->next){
          return NULL;
      }
      m=m->next;
    }
    m=m->prev;
    if(m->size>=0){
      liczba+=PAGE_SIZE;
      continue;  
    }
    if((char*)mem.head+liczba+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+count < (char*)m->next-FENCE_SIZE*FENCE_AMMOUNT){
      //udalo sie zaalokawc, gg wp
      choice=(struct node_t*)((char*)mem.head+liczba);
      break;
    }
    liczba+=PAGE_SIZE;
  }
  if(mem.head==(void*)choice) return heap_malloc(count);
  struct node_t temp=*m;
  void* ptr=(void*)((char*)choice+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  int offset=0;
    for(;;++offset){
      if((char*)m+offset==(char*)choice) break;
    }//jak daleko od m znajduje sie choice
    //offset = struct+Fence+Pamiec+Fence
  //ogarniecie to z lewej
  if((char*)choice-FENCE_SIZE*FENCE_AMMOUNT>=(char*)m+FENCE_AMMOUNT*FENCE_SIZE){
    //mozna wstawic pomiedzy //nie moze byc to m
    choice->prev=m;
    choice->next=m->next;
    m->next=m->next->prev=choice;
    choice->size=m->size+(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT;
    m->size+=(sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+(-choice->size));
  }else{
    choice->prev=temp.prev;
    choice->next=temp.next;
    choice->next->prev=choice->prev->next=choice;
    choice->prev->size+=offset;
    choice->size=temp.size+offset;
  }
  //ogarniecie to z prawej
  if((char*)choice+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+ count<=(char*)choice->next-FENCE_SIZE*FENCE_AMMOUNT){
    //mozna wstawic pomiedzy
    struct node_t *zm=(struct node_t*)((char*)choice+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count);
    zm->next=choice->next;
    zm->prev=choice;
    zm->next->prev=zm->prev->next=zm;
    zm->size=choice->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count;
    choice->size=count;
    memcpy((char*)zm+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
    memcpy((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence,   FENCE_SIZE*FENCE_AMMOUNT);
  }else{
    choice->size*=-1;
  }
  memcpy((char*)choice+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
  memcpy((char*)choice-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT);
  return ptr;
}
//rodzna aligned
//malloc
void* heap_malloc_aligned_debug(size_t count,int fileline,const char* filename){
  printf("Wywolano z pliku %s w linii %d\n",filename,fileline);
  long long int x = count;
  if(!count) return NULL;
  struct node_t* m=mem.head;
  uint64_t liczba=0;
  struct node_t* choice=NULL;  
  while(1){
    while((char*)m<=(char*)mem.head+liczba){
      if(!m->next){
        if(heap_get_more_memory()){
          return NULL;
        } 
        continue;
      }
      m=m->next;
    }
    m=m->prev;
    if(m->size>=0){
      liczba+=PAGE_SIZE;
      continue;  
    }
    if((char*)mem.head+liczba+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+count < (char*)m->next-FENCE_SIZE*FENCE_AMMOUNT){
      //udalo sie zaalokawc, gg wp
      choice=(struct node_t*)((char*)mem.head+liczba);
      break;
    }
    liczba+=PAGE_SIZE;
  }
  if(mem.head==(void*)choice) return heap_malloc(count);
  struct node_t temp=*m;
  void* ptr=(void*)((char*)choice+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  int offset=0;
    for(;;++offset){
      if((char*)m+offset==(char*)choice) break;
    }//jak daleko od m znajduje sie choice
    //offset = struct+Fence+Pamiec+Fence
  //ogarniecie to z lewej
  if((char*)choice-FENCE_SIZE*FENCE_AMMOUNT>=(char*)m+FENCE_AMMOUNT*FENCE_SIZE){
    //mozna wstawic pomiedzy //nie moze byc to m
    choice->prev=m;
    choice->next=m->next;
    m->next=m->next->prev=choice;
    choice->size=m->size+(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT;
    m->size+=(sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+(-choice->size));
  }else{
    choice->prev=temp.prev;
    choice->next=temp.next;
    choice->next->prev=choice->prev->next=choice;
    choice->prev->size+=offset;
    choice->size=temp.size+offset;
  }
  //ogarniecie to z prawej
  if((char*)choice+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+ count<=(char*)choice->next-FENCE_SIZE*FENCE_AMMOUNT){
    //mozna wstawic pomiedzy
    struct node_t *zm=(struct node_t*)((char*)choice+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count);
    zm->next=choice->next;
    zm->prev=choice;
    zm->next->prev=zm->prev->next=zm;
    zm->size=choice->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count;
    choice->size=count;
    memcpy((char*)zm+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
    memcpy((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence,   FENCE_SIZE*FENCE_AMMOUNT);
  }else{
    choice->size*=-1;
  }
  memcpy((char*)choice+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
  memcpy((char*)choice-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT);
  return ptr;
}
//calloc
void* heap_calloc_aligned_debug(size_t number,size_t size,int fileline,const char* filename){
  printf("Wywolano z pliku %s w linii %d\n",filename,fileline);
  long long int x = number*size;
  if(!number*size) return NULL;
  struct node_t* m=mem.head;
  uint64_t liczba=0;
  struct node_t* choice=NULL;  
  while(1){
    while((char*)m<=(char*)mem.head+liczba){
      if(!m->next){
        if(heap_get_more_memory()){
          return NULL;
        } 
        continue;
      }
      m=m->next;
    }
    m=m->prev;
    if(m->size>=0){
      liczba+=PAGE_SIZE;
      continue;  
    }
    if((char*)mem.head+liczba+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+number*size < (char*)m->next-FENCE_SIZE*FENCE_AMMOUNT){
      //udalo sie zaalokawc, gg wp
      choice=(struct node_t*)((char*)mem.head+liczba);
      break;
    }
    liczba+=PAGE_SIZE;
  }
  if(mem.head==(void*)choice) return heap_calloc(number,size);
  struct node_t temp=*m;
  void* ptr=(void*)((char*)choice+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  int offset=0;
    for(;;++offset){
      if((char*)m+offset==(char*)choice) break;
    }//jak daleko od m znajduje sie choice
    //offset = struct+Fence+Pamiec+Fence
  //ogarniecie to z lewej
  if((char*)choice-FENCE_SIZE*FENCE_AMMOUNT>=(char*)m+FENCE_AMMOUNT*FENCE_SIZE){
    //mozna wstawic pomiedzy //nie moze byc to m
    choice->prev=m;
    choice->next=m->next;
    m->next=m->next->prev=choice;
    choice->size=m->size+(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT;
    m->size+=(sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+(-choice->size));
  }else{
    choice->prev=temp.prev;
    choice->next=temp.next;
    choice->next->prev=choice->prev->next=choice;
    choice->prev->size+=offset;
    choice->size=temp.size+offset;
  }
  //ogarniecie to z prawej
  if((char*)choice+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+ number*size<=(char*)choice->next-FENCE_SIZE*FENCE_AMMOUNT){
    //mozna wstawic pomiedzy
    struct node_t *zm=(struct node_t*)((char*)choice+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+number*size);
    zm->next=choice->next;
    zm->prev=choice;
    zm->next->prev=zm->prev->next=zm;
    zm->size=choice->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+number*size;
    choice->size=number*size;
    memcpy((char*)zm+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
    memcpy((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence,   FENCE_SIZE*FENCE_AMMOUNT);
  }else{
    choice->size*=-1;
  }
  memcpy((char*)choice+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
  memcpy((char*)choice-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT);
  for(long int i=0;i<number*size;++i){
    *((char*)ptr+i)=0;
  }
  return ptr;
}//realoc aligned
void* heap_realloc_aligned_debug(void* memblock,size_t size,int fileline,const char* filename){
  printf("Wywolano z pliku %s w linii %d\n",filename,fileline);
  if(!memblock) return malloc_aligned(size);
  if(!size) {heap_free(memblock); return NULL;}
  struct node_t *zm=(struct node_t*)((char*)memblock-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  uint64_t liczba=0;
  while((char*)zm<(char*)mem.start+liczba){
    liczba+=PAGE_SIZE;
  }
  if((char*)zm!=(char*)mem.start+liczba){
    void *pt=malloc_aligned(size);
    if(!pt) return NULL;
    memcpy(pt, zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT, zm->size);
    heap_free(zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
    return pt;
  }
  if(size==zm->size) return memblock;
  int _size=size;
  if(zm->size<0) return NULL;
  if(zm->size > _size){
    if(zm->size-size>sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT){
      struct node_t* tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      tab->prev=zm;
      tab->next=zm->next;
      zm->next=zm->next->prev=tab;
      tab->size=-(zm->size-size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT);
      zm->size=size;
      heap_free(tab);
    }else{
    }
    return memblock;
  }            
  if(-zm->next->size > _size-zm->size){
      struct node_t temp;
      struct node_t* memory=zm->next;// pamiec bylego
      struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);//poprawne
      temp=*memory;
      *tab=temp;
      tab->next->prev=tab->prev->next=tab; 
      tab->size+=_size-zm->size;
      zm->size=size;
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence,          FENCE_AMMOUNT*FENCE_SIZE);
    return memblock;
  }
  if(-zm->next->size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > _size-zm->size){
    zm->size+=-zm->next->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    zm->next->next->prev=zm;
    zm->next=zm->next->next;
    return memblock;
  }
  void* ptr=heap_malloc_aligned_PomocDoRealoca(size);
  if(!ptr){
    if(heap_get_more_memory()) return NULL;
    return realloc_aligned(memblock,size);
  }
  memcpy(ptr,memblock,zm->size);
  heap_free(memblock);
  return ptr;
}
//zwrócenie zuzytej pamieci;
size_t heap_get_used_space(void){
  size_t stan=sizeof(struct node_t);
  struct node_t *zm=mem.start;
  while(zm->next){
    stan+=sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    if(zm->size>0) stan+=zm->size;
    zm=zm->next;
  }
  return stan;
}//zwrócenie największego
size_t heap_get_largest_used_block_size(void){
  size_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->size > (int)stan) stan=zm->size;
    zm=zm->next;
  }
  return stan;
}
uint64_t heap_get_used_blocks_count(void){
  uint64_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->size>0) ++stan;
    zm=zm->next;
  }
  return stan;
}
size_t heap_get_free_space(void){
  size_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->size<0) stan+=-zm->size;
    zm=zm->next;
  }
  return stan;
}
size_t heap_get_largest_free_area(void){
  size_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->size<0)
      if(-zm->size>stan) stan=-zm->size;
    zm=zm->next;
  }
  return stan;
}
uint64_t heap_get_free_gaps_count(void){
  uint64_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(-zm->size>=8) stan+=1;
    zm=zm->next;
  }
  return stan;
}
//dalej
enum pointer_type_t get_pointer_type(const void* pointer){
  if(!pointer) return pointer_null;
  if(pointer<mem.start||pointer>mem.end) return pointer_out_of_heap;
  return pointer_null;
  struct node_t *zm=mem.head;
  while(zm&&(void*)zm<=pointer){
    zm=zm->next;
  }
  if(!zm) return pointer_control_block;
  zm=zm->prev;
  if((char*)zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT==pointer){
    return pointer_valid;
  }
  if((char*)zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT>(char*)pointer||(char*)zm->next-FENCE_SIZE*FENCE_AMMOUNT<(char*)pointer){
    return pointer_control_block;
  }
  if(zm->size>0) return pointer_inside_data_block;
  return pointer_unallocated;
}
void* heap_get_data_block_start(const void* pointer){
  enum pointer_type_t typ=get_pointer_type(pointer);
  if(typ==pointer_valid) return (void*)pointer;
  if(typ!=pointer_unallocated&&typ!=pointer_inside_data_block) return NULL;
  struct node_t *tmp=mem.head;
  while((void*)tmp<(void*)pointer){
    tmp=tmp->next;
  }
  return (void*)((char*)tmp->prev+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
}
size_t heap_get_block_size(const void* memblock){
  if(get_pointer_type(memblock)!=pointer_valid) return 0;
  struct node_t *tmp=(struct node_t*)((char*)memblock-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  return labs(tmp->size);
}
void heap_dump_debug_information(void){
  struct node_t *tmp=mem.start;
  while(tmp){
    printf("%p\n",tmp);
    if(tmp->next){
      printf("%lu\n",FENCE_SIZE*FENCE_AMMOUNT*2+sizeof(struct node_t)+labs(tmp->size));
    }else{
      printf("%lu\n",sizeof(struct node_t)+labs(tmp->size));
    }
    printf("Skąd mam wiedziec? xd\n");
    printf("Skąd mam wiedziec? xd\n");
    tmp=tmp->next;
  }
  printf("%d\n",mem.total_size);
  printf("%lu\n",heap_get_used_space());
  printf("%lu\n",heap_get_free_space());
  printf("%lu\n",heap_get_largest_free_area());
}
