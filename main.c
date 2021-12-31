#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "custom_unistd.h"
#if defined(malloc)
#undef malloc
#endif
#if defined(free)
#undef free
#endif
#if defined(calloc)
#undef calloc
#endif
#if defined(realloc)
#undef realloc
#endif
// do naprawy
// testy jednostkowe i ten debug
// zmienienie wielkosci z -/+ na flaga/+
// mozliwy blad: malloc aligned moze sie probowac zaalokowac na ostatniej pustej strukturze
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
struct node_t
{
  size_t size;
  struct node_t *next;
  struct node_t *prev;
  unsigned int fileline;
  const char* filename;
  unsigned char flaga;
  unsigned int CRC;
};
struct memory_t
{
  unsigned int total_size;
  unsigned int free_size;
  int64_t fence[FENCE_AMMOUNT];
  void * start;
  void * end;
  struct node_t *head;
  struct node_t *tail;
}mem;

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
#define malloc_aligned(_size) heap_malloc_aligned_debug(_size,__LINE__,__FILE__)
#define calloc_aligned(_number,_size) heap_calloc_aligned_debug(_number,_size,__LINE__,__FILE__)
#define realloc_aligned( _memblock, _size) heap_realloc_aligned_debug(_memblock,_size,__LINE__,__FILE__)
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
//CRC
unsigned int compute_crc(struct node_t *zm);
//wlasna
void display_size_heap();
void update_CRC(void);
void* pls (void*);

pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv) {
	int status = heap_setup();
	assert(status == 0);
void *w1,*w2,*w3,*w4;
//malloc
w1=malloc(1024);
assert(w1 != NULL);//musi sie udac
w2=heap_malloc(0);
assert(w2 == NULL);//Nie moze sie udac
w3=heap_malloc(-1);
assert(w3==NULL); //nie moze sie udac
//calloc
w2=calloc(8,512*2);
assert(w2!=NULL);//musi sie udac
for(int i=0;i<8*512*2;++i){
  assert(*((char*)w2+i)==0);//musi byc wyzerowany
}
w3=heap_calloc(124, 0);
assert(w3==NULL);//nie moze sie udac
//realloc
w3=heap_realloc(w1, 8192);

assert(w3!=NULL&&w3!=w1);//musi byc przeniesone na inny adres
w3 =heap_realloc(w3, 4096);
assert(w3!=NULL);//musi sie udac
w4=heap_realloc(NULL, 4096+1024);
assert(w4!=NULL);//musi sie udac
w1=heap_realloc(NULL, 0);
assert(w1==NULL);//nie moze sie udac
heap_dump_debug_information();
printf("------------------\n");
//czyszczenie
heap_realloc(w1,0);//musi dzialac jak free
heap_free(w2);
heap_free(w3);
heap_free(w4);
//aligned
w1 = heap_malloc_aligned(4096);
assert(w1!=NULL);
int aligned=0;
while((char*)mem.head+aligned<(char*)w1){
  aligned+=PAGE_SIZE;
}
assert((char*)w1==(char*)mem.head+aligned);//musi zwrocic poczatek strony
w2=heap_malloc_aligned(0);
assert(w2==NULL);//nie moze sie udac
//calloc_aligned
w2=heap_calloc_aligned(8,512);
assert(w2!=NULL);//musi sie udac
for(int i=0;i<8*512;++i){
  assert(*((char*)w2+i)==0);//musi byc wyzerowane
}
aligned=0;
while((char*)mem.head+aligned<(char*)w2){
  aligned+=PAGE_SIZE;
}
assert((char*)w2==(char*)mem.head+aligned);//musi zwrocic poczatek strony
w3=heap_calloc_aligned(124, 0);
assert(w3==NULL);//nie moze sie udac
//realloc
w3=heap_realloc_aligned(w1, 8192);
aligned=0;
while((char*)mem.head+aligned<(char*)w3){
  aligned+=PAGE_SIZE;
}
assert((char*)w3==(char*)mem.head+aligned);// musi zwrocic poczatek strony
assert(w3!=NULL&&w3!=w1);//musi zostac przeniesione
w3 =heap_realloc_aligned(w3, 4096);
assert(w3!=NULL);//musi sie udac
w3 =  realloc_aligned(w3, 4096+1024);
assert(w3 != NULL);
w4=heap_realloc_aligned(NULL, 4096);
assert(w4!=NULL);//alokacja musi sie udac
w1=heap_realloc_aligned(NULL, 0);
assert(w1==NULL);//nie moze sie udac
status = heap_validate();
assert(status==0);//sprawdzenie komplementnosci blokow, sum kontrolnych oraz naruszenie plotkow
//czyszczenie
heap_free(w1);
heap_free(w2);
heap_free(w3);
heap_free(w4);
display_size_heap();
heap_dump_debug_information();
// oryginalny przykladowy testy
	size_t used_bytes = heap_get_used_space();
  status = heap_validate();
	assert(status == 0); 
	void* p1 = heap_malloc(8 * 1024 * 1024); // 8MB
	void* p2 = heap_malloc(8 * 1024 * 1024); // 8MB

	void* p3 = heap_malloc(8 * 1024 * 1024); // 8MB
	void* p4 = heap_malloc(45 * 1024 * 1024); // 45MB
	assert(p1 != NULL); // malloc musi się udać
	assert(p2 != NULL); // malloc musi się udać
	assert(p3 != NULL); // malloc musi się udać
	assert(p4 == NULL); // nie ma prawa zadziałać
    // Ostatnia alokacja, na 45MB nie może się powieść,
    // ponieważ sterta nie może być aż tak 
    // wielka (brak pamięci w systemie operacyjnym).
	status = heap_validate();
	assert(status == 0); // sterta nie może być uszkodzona
	assert(heap_get_used_blocks_count() == 3);

	assert(
        heap_get_used_space() >= 24 * 1024 * 1024 &&
        heap_get_used_space() <= 24 * 1024 * 1024 + 2000
        );

	heap_free(p1);
	heap_free(p2);
	heap_free(p3);
  heap_free(NULL);
	//assert(heap_get_free_space() == free_bytes);
	assert(heap_get_used_space() == used_bytes);
	assert(heap_get_used_blocks_count() == 0);
  status = heap_validate();
	assert(status == 0); // sterta nie może być uszkodzona
  pthread_t zm[5];
  for(int i=0;i<5;++i){
    pthread_create(zm+i, NULL, pls, NULL);
  }
  usleep(10000);
  for(int i=0;i<5;++i){
    pthread_join(*(zm+i), NULL);
  }
  assert(heap_validate()==0);
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
  st->size=(PAGE_SIZE-2*sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
  memcpy((char*)st+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  memcpy((char*)en-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  st->flaga=en->flaga=0;
  st->fileline=en->fileline=0;
  st->filename=en->filename=NULL;
  st->prev=NULL;
  st->next=en;
  en->size=0;
  en->next=NULL;
  en->prev=st;
  mem.head=st;
  mem.tail=en;
  update_CRC();
  return result;
}
void* heap_malloc(size_t count){
  pthread_mutex_lock(&mutex);
  if(heap_validate()) {pthread_mutex_unlock(&mutex); return NULL;}
  if(!count) {pthread_mutex_unlock(&mutex); return NULL;}
  struct node_t* zm=mem.head;
  while( zm->flaga==1||zm->size<count){
    zm=zm->next;
    if(!zm){
      pthread_mutex_unlock(&mutex);
      int x = heap_get_more_memory();
      pthread_mutex_lock(&mutex);
      if(x) {pthread_mutex_unlock(&mutex);return NULL;}
      pthread_mutex_unlock(&mutex);
      return heap_malloc(count);
    }
  }
  void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  if(count+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > 
  zm->size){
    zm->flaga=1;
  }
  else{
    struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+count+2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->size=(zm->size-count-sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->flaga=0;
    tab->filename=NULL;
    tab->fileline=0;
    memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    tab->prev=zm;
    tab->next=zm->next;
    tab->next->prev=tab;
    zm->next=tab;
    zm->size=count;
    zm->flaga=1;
  }
  update_CRC();
  pthread_mutex_unlock(&mutex);
  return ptr;
}
//pomocznicza funkcja realloca (brak pytania o pamiec)
void* heap_malloc_doRealloca(size_t count){
  if(heap_validate()) return NULL;
  if(!count) return NULL;
  struct node_t* zm=mem.head;
  while( zm->flaga==1||zm->size < count){
    zm=zm->next;
    if(!zm){
      return NULL;
    }
  }
  void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  if(count+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > 
  zm->size){
    zm->flaga=1;
  }
  else{
    struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+count+2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->size=(zm->size-count-sizeof(struct node_t)-2*FENCE_AMMOUNT*FENCE_SIZE);
    tab->flaga=0;
    tab->filename=NULL;
    tab->fileline=0;
    memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
    tab->prev=zm;
    tab->next=zm->next;
    tab->next->prev=tab;
    zm->next=tab;
    zm->size=count;
    zm->flaga=1;
  }
  update_CRC();
  return ptr;
}
//calloc
void * heap_calloc(size_t number,size_t size){
  void *ptr=heap_malloc(number*size);
  if(!ptr){ 
    return NULL;
  }
  for(long long int i=0;i<number*size;i++){
    *((char*)ptr+i)=0;
  }
  return ptr;
}
//wlasna do wyswietlania
void display_size_heap(){
  struct node_t *zm=mem.head;
  int i=0;
  while(zm){
    printf("%d: %c = %lu\n",++i,zm->flaga?'z':'w',zm->size);
    zm=zm->next;
  }printf("\n");
}
//free
void heap_free(void *memblock){
  pthread_mutex_lock(&mutex);
  if(heap_validate()){
    pthread_mutex_unlock(&mutex);
    return;
  }
  if(!memblock){
    pthread_mutex_unlock(&mutex);
    return;
  }
  struct node_t *zm =mem.head;
  while(zm){
    void* ptr=(void*)((char*)zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
    if(memblock==ptr) break;
    zm=zm->next;
  }
  if(!zm||zm->flaga==0){ 
    pthread_mutex_unlock(&mutex);
    return;
  }
  if(((zm->prev&&zm->prev->flaga==1)||!zm->prev)&&zm->next->flaga==1){
    zm->flaga=0;
  }
  if(zm->next&&zm->next->flaga==0&&zm->next->next){
    zm->size=zm->size+sizeof(struct node_t)+zm->next->size
    +2*FENCE_SIZE*FENCE_AMMOUNT;
    zm->flaga=0;
    zm->next=zm->next->next;
    zm->next->prev=zm;
  }
  if(zm->prev&&zm->prev->flaga==0)
  { 
    zm->prev->next=zm->next;
    zm->next->prev=zm->prev;
    zm->prev->size+=zm->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
  }
  update_CRC();
  pthread_mutex_unlock(&mutex);
}
//realloc
void* heap_realloc(void *memblock,size_t size){
  pthread_mutex_lock(&mutex);
  if(heap_validate()){
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  if(!memblock){
    pthread_mutex_unlock(&mutex);
    return heap_malloc(size);
  }
  if(!size){
    pthread_mutex_unlock(&mutex);
    heap_free(memblock);
    return NULL;
  }
  struct node_t *zm=(struct node_t*)((char*)memblock-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  if(size==zm->size){pthread_mutex_unlock(&mutex); return memblock;}
  if(zm->flaga==0){pthread_mutex_unlock(&mutex); return NULL;}
  if(zm->size > size){
    //zmiesci sie 
    if(zm->size-size>sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT){
      struct node_t* tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      tab->flaga=1;
      tab->filename=NULL;
      tab->fileline=0;
      tab->prev=zm;
      tab->next=zm->next;
      tab->next->prev=tab->prev->next=tab;
      tab->size=zm->size-size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT;
      zm->size=size;
      update_CRC();
      pthread_mutex_unlock(&mutex);
      heap_free((char*)tab+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
      pthread_mutex_lock(&mutex);
    }else{
    }
    update_CRC();
    pthread_mutex_unlock(&mutex);
    return memblock;
  }            
  if(zm->next->flaga==0&&zm->next->size > size-zm->size){//size->next > size-alok
      struct node_t temp=*zm->next; // pamiec bylego
      struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);//poprawne
      *tab=temp;
      tab->next->prev=tab->prev->next=tab; 
      tab->size=tab->size-(size-zm->size);
      zm->size=size;
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence,          FENCE_AMMOUNT*FENCE_SIZE);
      update_CRC();
      pthread_mutex_unlock(&mutex);
    return memblock;
  }
  if(zm->next->flaga==0&&zm->next->size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > size-zm->size){
    zm->size+=zm->next->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    zm->next->next->prev=zm;
    zm->next=zm->next->next;
    update_CRC();
    pthread_mutex_unlock(&mutex);
    return memblock;
  }
  void* ptr=heap_malloc_doRealloca(size);
  if(!ptr){
    pthread_mutex_unlock(&mutex);
    int x= heap_get_more_memory();
    pthread_mutex_lock(&mutex);
    if(x){pthread_mutex_unlock(&mutex); return NULL;}
    pthread_mutex_unlock(&mutex);
    return heap_realloc(memblock,size);
  }
  memcpy(ptr,memblock,zm->size);
  update_CRC();
  pthread_mutex_unlock(&mutex);
  heap_free(memblock);
  return ptr;
}//daje pamiec
int heap_get_more_memory(void){
  pthread_mutex_lock(&mutex);
  void *ptr =custom_sbrk(PAGE_SIZE);
  if(ptr==(void*)-1){ 
    pthread_mutex_unlock(&mutex);
    return 1;
  }
  mem.total_size+=PAGE_SIZE;
  mem.end=(void*)((char*)mem.end+PAGE_SIZE);
  struct node_t *zm = (struct node_t*)((char*)mem.end-sizeof(struct node_t));
  mem.tail->next=zm;
  zm->prev=mem.tail;
  zm->next=NULL;
  zm->fileline=zm->flaga=0;
  zm->filename=NULL;
  zm->size=0;
  mem.tail=zm;
  memcpy((char*)zm->prev+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
  memcpy((char*)zm-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);

  zm->prev->size=PAGE_SIZE-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT;
  zm->prev->flaga=1;
  update_CRC();
  pthread_mutex_unlock(&mutex);
  heap_free((char*)zm->prev+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  return 0;
}
int heap_validate(void){
  struct node_t *zm=mem.head;
  while(zm){
    if(zm->prev){
      if(memcmp((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT)) return -1;
    }
    if(zm->next){
      if(memcmp((char*)zm+sizeof(struct node_t), mem.fence, FENCE_SIZE*FENCE_AMMOUNT)) return -1;
    }
    if(zm->CRC!=compute_crc(zm)){
      return -2;
    }
    zm=zm->next;
  }
  if(mem.total_size!=heap_get_free_space()+heap_get_used_space())
    return -3;
  return 0;
}
//rodzina debuga
//malloc
void* heap_malloc_debug(size_t count,int fileline,const char* filename){
  void* ptr = heap_malloc(count);
  pthread_mutex_lock(&mutex);
  if(!ptr) {pthread_mutex_unlock(&mutex);return NULL;}
  struct node_t *zm = (struct node_t*)((char*)ptr-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  zm->fileline=fileline;
  zm->filename=filename;
  zm->CRC=compute_crc(zm);
  pthread_mutex_unlock(&mutex);
  return ptr;
}//calloc
void* heap_calloc_debug(size_t number,size_t size,int fileline,const char* filename){
  void *ptr  = heap_malloc_debug(size*number,fileline,filename);
  if(!ptr) {pthread_mutex_unlock(&mutex);return NULL;}
  for(long long int i=0;i<number*size;i++){
    *((char*)ptr+i)=0;
  }
  return ptr;
}//realloc
void* heap_realloc_debug(void* memblock ,size_t size,int fileline,const char* filename){
  void *ptr = heap_realloc(memblock,size);
  pthread_mutex_lock(&mutex);
  if(!ptr){
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  struct node_t *zm = (struct node_t*)((char*)ptr-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  zm->fileline=fileline;
  zm->filename=filename;
  zm->CRC=compute_crc(zm);
  pthread_mutex_unlock(&mutex);
  return ptr;
}
//rodzna aligned
//malloc
void* heap_malloc_aligned(size_t count){
  pthread_mutex_lock(&mutex);
  if(heap_validate()){
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  if(!count) {
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  struct node_t* m=mem.head;
  uint64_t liczba=PAGE_SIZE;
  struct node_t* choice=NULL;  
  while(1){
    while((char*)m<=(char*)mem.head+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT){
      if(!m->next){
        pthread_mutex_unlock(&mutex);
        int x = heap_get_more_memory();
        pthread_mutex_lock(&mutex);
        if(x){
          pthread_mutex_unlock(&mutex);
          return NULL;
        } 
        pthread_mutex_unlock(&mutex);
        return heap_malloc_aligned(count);
      }
      m=m->next;
    }
    m=m->prev;
    if(m->flaga==1){
      liczba+=PAGE_SIZE;
      continue;  
    }
    if((char*)mem.head+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+count < (char*)m->next-FENCE_SIZE*FENCE_AMMOUNT){
      //udalo sie zaalokawc
      choice=(struct node_t*)((char*)mem.head+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
      break;
    }
    liczba+=PAGE_SIZE;
  }
  struct node_t temp=*m;
  void* ptr=(void*)((char*)choice+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  int offset=0;
    for(;;++offset){
      if((char*)m+offset==(char*)choice) break;
    }//jak daleko od m znajduje sie choice
  //ogarniecie to z lewej
  if((char*)choice-FENCE_SIZE*FENCE_AMMOUNT>=(char*)m+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE){
    //mozna wstawic pomiedzy 
    choice->prev=m;
    choice->next=m->next;
    choice->next->prev=choice->prev->next=choice;
    choice->size=m->size-(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT;
    m->size=offset-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT;
  }else{
    choice->prev=temp.prev;
    choice->next=temp.next;
    choice->next->prev=choice->prev->next=choice;
    choice->prev->size+=offset;
    choice->size=temp.size-(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT;
  }
  //ogarniecie to z prawej
  if((char*)choice+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+ count<=(char*)choice->next-FENCE_SIZE*FENCE_AMMOUNT){
    //mozna wstawic pomiedzy
    struct node_t *zm=(struct node_t*)((char*)choice+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count);
    zm->next=choice->next;
    zm->prev=choice;
    zm->next->prev=zm->prev->next=zm;
    zm->size=choice->size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT-count;
    zm->flaga=0;
    zm->filename=NULL;
    zm->fileline=0;
    choice->size=count;
    memcpy((char*)zm+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
    memcpy((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence,   FENCE_SIZE*FENCE_AMMOUNT);
    choice->flaga=1;
  }else{
    choice->flaga=1;
  }
  choice->fileline=0;
  choice->filename=NULL;
  memcpy((char*)choice+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
  memcpy((char*)choice-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT);
  update_CRC();
  pthread_mutex_unlock(&mutex);
  return ptr;
}
//calloc
void* heap_calloc_aligned(size_t number,size_t size){
  void *ptr = heap_malloc_aligned(size*number);
  if(!ptr) {return NULL;}
  for(long int i=0;i<number*size;++i){
    *((char*)ptr+i)=0;
  }
  return ptr;
}//realoc aligned
void* heap_realloc_aligned(void *memblock,size_t size){
  pthread_mutex_lock(&mutex);
  if(heap_validate()) {
    pthread_mutex_unlock(&mutex); 
    return NULL;
  }
  if(!memblock) {
    pthread_mutex_unlock(&mutex);
    return heap_malloc_aligned(size);
  }
  if(!size) {
    pthread_mutex_unlock(&mutex); 
    heap_free(memblock); 
    return NULL;
  }
  struct node_t *zm=(struct node_t*)((char*)memblock-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  uint64_t liczba=PAGE_SIZE;
  while((char*)zm>(char*)mem.start+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT){
    liczba+=PAGE_SIZE;
  }
  if((char*)zm!=(char*)mem.start+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT){
    pthread_mutex_unlock(&mutex);
    void *pt=heap_malloc_aligned(size);
    pthread_mutex_lock(&mutex);
    if(!pt){ 
      pthread_mutex_unlock(&mutex);  
      return NULL;
    }
    memcpy(pt, zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT, zm->size);
    update_CRC();
    pthread_mutex_unlock(&mutex);
    heap_free(zm+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
    return pt;
  }
  if(size==zm->size){pthread_mutex_unlock(&mutex); return memblock;}
  if(zm->flaga==0){pthread_mutex_unlock(&mutex); return NULL;}
  if(zm->size > size){
    if(zm->size-size>sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT){
      struct node_t* tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      tab->flaga=1;
      tab->fileline=0;
      tab->filename=NULL;
      tab->prev=zm;
      tab->next=zm->next;
      zm->next=zm->next->prev=tab;
      tab->size=zm->size-size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT;
      zm->size=size;
      update_CRC();
      pthread_mutex_unlock(&mutex);
      heap_free((char*)tab+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT);
      pthread_mutex_lock(&mutex);
    }else{
    }
    update_CRC();
    pthread_mutex_unlock(&mutex);
    return memblock;
  }            
 if(zm->next->flaga==0&&zm->next->size > size-zm->size){//size->next > size-alok
      struct node_t temp=*zm->next; // pamiec bylego
      struct node_t *tab=(struct node_t*)((char*)zm+sizeof(struct node_t)+size+2*FENCE_SIZE*FENCE_AMMOUNT);//poprawne
      *tab=temp;
      tab->next->prev=tab->prev->next=tab; 
      tab->size=tab->size-(size-zm->size);
      zm->size=size;
      memcpy((char*)tab+sizeof(struct node_t), mem.fence, FENCE_AMMOUNT*FENCE_SIZE);
      memcpy((char*)tab-FENCE_AMMOUNT*FENCE_SIZE, mem.fence,          FENCE_AMMOUNT*FENCE_SIZE);
      update_CRC();
      pthread_mutex_unlock(&mutex);
    return memblock;
  }
  if(zm->next->flaga==0&&zm->next->size+2*FENCE_AMMOUNT*FENCE_SIZE+sizeof(struct node_t) > size-zm->size){
    zm->size+=zm->next->size+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    zm->next->next->prev=zm;
    zm->next=zm->next->next;
    update_CRC();
    pthread_mutex_unlock(&mutex);
    return memblock;
  }
  void* ptr=heap_malloc_aligned_PomocDoRealoca(size);
  if(!ptr){
    pthread_mutex_unlock(&mutex);
    int x = heap_get_more_memory();
    pthread_mutex_lock(&mutex);
    if(x){
      pthread_mutex_unlock(&mutex);
      return NULL;
    }
    pthread_mutex_unlock(&mutex);
    return heap_realloc_aligned(memblock,size);
  }
  memcpy(ptr,memblock,zm->size);
  update_CRC();
  pthread_mutex_unlock(&mutex);
  heap_free(memblock);
  return ptr;
}//pomocnicza do realoca_aligned
void* heap_malloc_aligned_PomocDoRealoca(size_t count){
  if(heap_validate()) return NULL;
  if(!count) return NULL;
  struct node_t* m=mem.head;
  uint64_t liczba=PAGE_SIZE;
  struct node_t* choice=NULL;  
  while(1){
    while((char*)m<=(char*)mem.head+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT){
      if(!m->next){
        return NULL;
      }
      m=m->next;
    }
    m=m->prev;
    if(m->flaga==1){
      liczba+=PAGE_SIZE;
      continue;  
    }
    if((char*)mem.head+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+count < (char*)m->next-FENCE_SIZE*FENCE_AMMOUNT){
      //udalo sie zaalokawc
      choice=(struct node_t*)((char*)mem.head+liczba-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
      break;
    }
    liczba+=PAGE_SIZE;
  }
  struct node_t temp=*m;
  void* ptr=(void*)((char*)choice+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE);
  int offset=0;
    for(;;++offset){
      if((char*)m+offset==(char*)choice) break;
    }//jak daleko od m znajduje sie choice
  //ogarniecie to z lewej
  if((char*)choice-FENCE_SIZE*FENCE_AMMOUNT>=(char*)m+sizeof(struct node_t)+FENCE_AMMOUNT*FENCE_SIZE){
    //mozna wstawic pomiedzy 
    choice->prev=m;
    choice->next=m->next;
    m->next=m->next->prev=choice;
    choice->size=m->size-(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT;
    m->size=offset-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT;
  }else{
    choice->prev=temp.prev;
    choice->next=temp.next;
    choice->next->prev=choice->prev->next=choice;
    choice->prev->size+=offset;
    choice->size=temp.size-(offset-FENCE_SIZE*FENCE_AMMOUNT-sizeof(struct node_t))-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT;
  }
  //ogarniecie to z prawej
  if((char*)choice+sizeof(struct node_t)+FENCE_SIZE*FENCE_AMMOUNT+ count<=(char*)choice->next-FENCE_SIZE*FENCE_AMMOUNT){
    //mozna wstawic pomiedzy
    struct node_t *zm=(struct node_t*)((char*)choice+sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT+count);
    zm->next=choice->next;
    zm->prev=choice;
    zm->next->prev=zm->prev->next=zm;
    zm->size=choice->size-sizeof(struct node_t)-2*FENCE_SIZE*FENCE_AMMOUNT-count;
    zm->flaga=0;
    zm->filename=NULL;
    zm->fileline=0;
    choice->size=count;
    memcpy((char*)zm+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
    memcpy((char*)zm-FENCE_SIZE*FENCE_AMMOUNT, mem.fence,   FENCE_SIZE*FENCE_AMMOUNT);
    choice->flaga=1;
  }else{
    choice->flaga=1;
  }
  choice->fileline=0;
  choice->filename=NULL;
  memcpy((char*)choice+sizeof(struct node_t),mem.fence,FENCE_SIZE*FENCE_AMMOUNT);
  memcpy((char*)choice-FENCE_SIZE*FENCE_AMMOUNT, mem.fence, FENCE_SIZE*FENCE_AMMOUNT);
  update_CRC();
  return ptr;
}
//rodzina aligned debug
//malloc
void* heap_malloc_aligned_debug(size_t count,int fileline,const char* filename){
  void *ptr = heap_malloc_aligned(count);
  pthread_mutex_lock(&mutex);
  if(!ptr) {
    pthread_mutex_unlock(&mutex);
    return NULL;
  }
  struct node_t *zm = (struct node_t*)((char*)ptr-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  zm->fileline=fileline;
  zm->filename=filename;
  zm->CRC=compute_crc(zm);
  pthread_mutex_unlock(&mutex);
  return ptr;
}
//calloc
void* heap_calloc_aligned_debug(size_t number,size_t size,int fileline,const char* filename){
  void *ptr = heap_malloc_aligned_debug(number*size, fileline, filename);
  if(!ptr){
    return NULL;
  }
  for(long int i=0;i<number*size;++i){
    *((char*)ptr+i)=0;
  }
  return ptr;
}//realoc aligned
void* heap_realloc_aligned_debug(void* memblock,size_t size,int fileline,const char* filename){
  void* ptr = heap_realloc_aligned(memblock, size);
  pthread_mutex_lock(&mutex);
  if(!ptr) {
    pthread_mutex_unlock(&mutex);  
    return NULL;
  }
  struct node_t *zm = (struct node_t*)((char*)ptr-sizeof(struct node_t)-FENCE_SIZE*FENCE_AMMOUNT);
  zm->fileline=fileline;
  zm->filename=filename;
  zm->CRC=compute_crc(zm);
  pthread_mutex_unlock(&mutex);
  return ptr;
}
//zwrócenie zuzytej pamieci;
size_t heap_get_used_space(void){
  size_t stan=sizeof(struct node_t);
  struct node_t *zm=mem.start;
  while(zm->next){
    stan+=sizeof(struct node_t)+2*FENCE_SIZE*FENCE_AMMOUNT;
    if(zm->flaga==1) stan+=zm->size;
    zm=zm->next;
  }
  return stan;
}//zwrócenie największego
size_t heap_get_largest_used_block_size(void){
  size_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->flaga==1&&zm->size > (int)stan) stan=zm->size;
    zm=zm->next;
  }
  return stan;
}
uint64_t heap_get_used_blocks_count(void){
  uint64_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->flaga==1) ++stan;
    zm=zm->next;
  }
  return stan;
}
size_t heap_get_free_space(void){
  size_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->flaga==0) stan+=zm->size;
    zm=zm->next;
  }
  return stan;
}
size_t heap_get_largest_free_area(void){
  size_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->flaga==0)
      if(zm->size>stan) stan=zm->size;
    zm=zm->next;
  }
  return stan;
}
uint64_t heap_get_free_gaps_count(void){
  uint64_t stan=0;
  struct node_t *zm=mem.start;
  while(zm){
    if(zm->flaga==0&&zm->size>=8) stan+=1;
    zm=zm->next;
  }
  return stan;
}
//dalej
enum pointer_type_t get_pointer_type(const void* pointer){
  if(!pointer) return pointer_null;
  if(pointer<mem.start||pointer>mem.end) return pointer_out_of_heap;
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
  if(zm->flaga==1) return pointer_inside_data_block;
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
  return tmp->size;
}
void heap_dump_debug_information(void){
  struct node_t *tmp=mem.start;
  while(tmp){
    printf("Blok %p\n",tmp);
    if(tmp->next){
      printf("Flaga %c wielkosc: %lu\n",tmp->flaga?'Z':'W',FENCE_SIZE*FENCE_AMMOUNT*2+sizeof(struct node_t)+tmp->size);
    }else{
      printf("Flaga %c wielkosc: %lu\n",tmp->flaga?'Z':'W',sizeof(struct node_t)+tmp->size);
    }
    if(tmp->filename)
      printf("Wywolane z pliku %s\n",tmp->filename);
    if(tmp->fileline)
      printf("Wywolane z lini %d\n",tmp->fileline);
    tmp=tmp->next;
    putchar('\n');
  }
  printf("Calkowita przestrzen %d\n",mem.total_size);
  printf("Zajeta przestrzen %lu\n",heap_get_used_space());
  printf("Wolna przestrzen %lu\n",heap_get_free_space());
  printf("Najwieksza Wolna przestrzen %lu\n\n",heap_get_largest_free_area());
}
unsigned int compute_crc(struct node_t *zm){
  if(!zm) return 0;
  unsigned int result=0;
  unsigned int save =zm->CRC;
  zm->CRC=0;  
  for(unsigned int i=0;i<sizeof(struct node_t);++i){
    result+=*((char*)zm+i);
  }
  zm->CRC=save;  
  return result;
}
void update_CRC(void){
  struct node_t *zm=mem.head;
  while(zm){
    zm->CRC=compute_crc(zm);
    zm=zm->next;
  }
}
void* pls (void*arg){
  void* tmp[10];
  for(int i=0;i<10;++i){
    if((tmp[i]=heap_malloc(250))==NULL){
      printf("error!\n");
    }
   	//display_size_heap();
    usleep(10000*(rand()%5+1));
  }
  for(int i=0;i<10;++i){
    heap_free(tmp[i]);
  }
  return NULL;
}
