#include <string.h>
#include <stdio.h>
#include <cache-flash.h>
#include <w25qxxx.h>
#include <fat_filelib.h>

//extern const w25qxxx_drv_t4K w25qxxx_drv4K;
extern const w25qxxx_drv_t w25qxxx_drv;

uint8_t lowmemRAM[PAGE_SIZE];
uint8_t cacheRAM1[PAGE_SIZE];
uint8_t cacheRAM2[PAGE_SIZE];
//uint8_t cacheRAM3[BLOCK];
//uint8_t cacheRAM4[BLOCK];

uint32_t block1=0;
uint32_t block2=0;
//uint32_t block3=0;
//uint32_t block4=0;

uint8_t dirty1=0;
uint8_t dirty2=0;
//uint8_t dirty3=0;
//uint8_t dirty4=0;

uint8_t NextCacheUse=0;
uint32_t max=0;

//desde 14000000 hasta 16000000> 2000000
//sector init 14000000/BLOCK>3417.96875
//sector end  16000000/BLOCK>3906.25
static const uint32_t base = 14000000;
uint32_t sector = base / PAGE_SIZE;





void cachebegin(uint32_t size){
   max=size;
   block1=sector;
   block2=sector+1;
   //block3=sector+2;
   //block4=sector+3;

   dirty1=0;
   dirty2=0;
   //dirty3=0;
   //dirty4=0;
 
   NextCacheUse=0;
   memset(lowmemRAM,0,sizeof(lowmemRAM));
   memset(cacheRAM1,0,PAGE_SIZE);
   memset(cacheRAM2,0,PAGE_SIZE);
   
   char *file="/blink/blink.elf";
   FILE* fd;
   int part=0;

    if (!(fd = fopen(file,"r"))){
        printf("not found %s\n",file);
        return;
    }
    
    fseek(fd, 0, SEEK_END);
    unsigned int fileLength = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    
    while(!feof(fd)){
		char c = fgetc(fd);
		cachewrite(part,c);
		//printf("(%x)--%x\n",c,cacheread(part));
		if(cacheread(part)!=c){
			printf("error load image into cache (%x)--%x\n",c,cacheread(part));
			while(1);
		}
		part++;
    }

    fclose(fd);


   for(int i=0;i<(size/PAGE_SIZE);i++){
   	   //printf("%d %d\n",i,sector+i);
   }
   printf("size memory: %d, base: %08x, sector: %d\n",max,base,sector );
}






uint32_t cacheget_size(){
       return max;
}



void cachewrite(uint32_t address,uint8_t value){
    if (address < sizeof(lowmemRAM)) {
        lowmemRAM[address] = value; //If RAM lower than 0x600 write direct RAM
        return;
    }
    uint32_t block = (address & 0xfffffe00); //Else write into cached block

    if (block1 == block) {
        cacheRAM1[address & 0x000001ff] = value; //Write in cache1
        dirty1 = 1; //mark cache1 as dirty
        return;
    }

    if (block2 == block) {
        cacheRAM2[address & 0x000001ff] = value; //Write in cache2
        dirty2 = 1; //mark cache2 as dirty
        return;
    }

    if (NextCacheUse==0) {
      if (dirty1) {           //Write back the cached block if it's dirty
        w25qxxx_drv.write((cacheRAM1), block1, 1);
      }
      w25qxxx_drv.read( (cacheRAM1), block, 1);
      block1 = (address & 0xfffffe00);
      NextCacheUse += 1;
      NextCacheUse &= 2;
      cacheRAM1[address & 0x000001ff] = value; //Now write it in cache1
      dirty1 = 1;
      return;
    }
    
    if (NextCacheUse==1) {
      if (dirty2) {           //Write back the cached block if it's dirty
      	w25qxxx_drv.write((cacheRAM2), block2, 1);
      }
      w25qxxx_drv.read((cacheRAM2), block, 1);
      block2 = (address & 0xfffffe00);
      NextCacheUse += 1;
      NextCacheUse &= 2;
      cacheRAM2[address & 0x000001ff] = value; //Now write it in cache2
      dirty2 = 1;
      return;
    }
}

uint8_t cacheread(uint32_t address){
    if (address<sizeof(lowmemRAM)) return lowmemRAM[address]; //If RAM lower than 0x600 return direct RAM
    uint32_t block = (address & 0xfffffe00); //Else look into cached RAM
    if (block1 == block) return cacheRAM1[address & 0x000001ff]; //Cache hit in cache1
    if (block2 == block) return cacheRAM2[address & 0x000001ff]; //Cache hit in cache2
    if (NextCacheUse==0) {
      if (dirty1) {           //Write back the cached block if it's dirty
        w25qxxx_drv.write((cacheRAM1), block1, 1);
        dirty1=0;
      }

      w25qxxx_drv.read((cacheRAM1), block, 1);
      block1 = (address & 0xfffffe00);
      NextCacheUse += 1;
      NextCacheUse &= 2;
      return cacheRAM1[address & 0x000001ff]; //Now found it in cache1
    }
    
    if (NextCacheUse==1) {
      if (dirty2) {           //Write back the cached block if it's dirty
        w25qxxx_drv.write((cacheRAM2), block2, 1);
        dirty2=0;
      }
      w25qxxx_drv.read((cacheRAM2), block, 1);
      block2 = (address & 0xfffffe00);
      NextCacheUse += 1;
      NextCacheUse &= 2;
      return cacheRAM2[address & 0x000001ff]; //Now found it in cache2
    }
}




