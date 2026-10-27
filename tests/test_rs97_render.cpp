// Host-side reference tests for RS-97 paired framebuffer writes.
// Does not test PlayStation emulation speed or hardware compatibility.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <random>
#include <algorithm>
using u16=uint16_t; using u32=uint32_t;
typedef u32 __attribute__((__may_alias__)) pixel_pair_t;
#define RGB16(C) ( (((C) & 0x7c00)>>10) | (((C) & 0x03e0)<<1) | (((C) & 0x1f)<<11) )
#define RGB16X2(C) ( (((C)&(0x1f001f<<10))>>10) | (((C)&(0x1f001f<<5))<<1) | (((C)&(0x1f001f))<<11) )
static constexpr int src256[]={0,1,1,2,3,4,5,5,6,7};
static constexpr int src384[]={0,1,2,3,4,6,7,8,9,10};
static constexpr int src512[]={0,1,3,4,6,8,9,11,12,14};
static constexpr int src640[]={0,2};
static constexpr const int* selection(int mode) {return mode==256?src256:mode==384?src384:mode==512?src512:src640;}
static void ref(int mode,const u16* s,u16*d) {
 const int* sel=selection(mode), n=mode==640?2:10, blocks=mode==640?160:32;
 const int stride=mode==640?4:mode==256?8:mode==384?12:16;
 for(int k=0;k<blocks;k++,s+=stride,d+=n)for(int i=0;i<n;i++)d[i]=RGB16(s[sel[i]]);
}
static void fast(int mode,const u16* s,u16*d) {
 pixel_pair_t* out=(pixel_pair_t*)(void*)d;
 if(mode==640){for(unsigned i=0;i<160;i++,s+=4)out[i]=RGB16X2((u32)s[0]|((u32)s[2]<<16));return;}
 const int blocks=32;
 if(mode==256){for(int i=0;i<blocks;i++,s+=8,out+=5){
 out[0]=RGB16X2((u32)s[0]|((u32)s[1]<<16));out[1]=RGB16X2((u32)s[1]|((u32)s[2]<<16));
 out[2]=RGB16X2((u32)s[3]|((u32)s[4]<<16));out[3]=RGB16X2((u32)s[5]|((u32)s[5]<<16));out[4]=RGB16X2((u32)s[6]|((u32)s[7]<<16));}return;}
 if(mode==384){for(int i=0;i<blocks;i++,s+=12,out+=5){
 out[0]=RGB16X2((u32)s[0]|((u32)s[1]<<16));out[1]=RGB16X2((u32)s[2]|((u32)s[3]<<16));
 out[2]=RGB16X2((u32)s[4]|((u32)s[6]<<16));out[3]=RGB16X2((u32)s[7]|((u32)s[8]<<16));out[4]=RGB16X2((u32)s[9]|((u32)s[10]<<16));}return;}
 if(mode==512){for(int i=0;i<blocks;i++,s+=16,out+=5){
 out[0]=RGB16X2((u32)s[0]|((u32)s[1]<<16));out[1]=RGB16X2((u32)s[3]|((u32)s[4]<<16));
 out[2]=RGB16X2((u32)s[6]|((u32)s[8]<<16));out[3]=RGB16X2((u32)s[9]|((u32)s[11]<<16));out[4]=RGB16X2((u32)s[12]|((u32)s[14]<<16));}return;}
}
static void tile_ref(u16* dst,u32 count,u16 value){while(count--) *dst++=value;}
static void tile_fast(u16* pDst,u32 count,u16 data){
 if(count<8){do{*pDst++=data;}while(--count);}else{
  typedef u32 __attribute__((__may_alias__)) tile_pair_t;
  if((uintptr_t)pDst & 2u){*pDst++=data;--count;}
  tile_pair_t* dst=(tile_pair_t*)(void*)pDst;
  const tile_pair_t packed=(u32)data|((u32)data<<16);
  u32 pairs=count>>1;
  while(pairs--){*dst++=packed;}
  if(count&1u)*(u16*)(void*)dst=data;
 }
}
int main(int argc,char**){
 for (unsigned c=0;c<65536;c++) {
  const u32 pair=(u32)c|((u32)(65535-c)<<16);
  const u32 expected=(u32)RGB16(c)|((u32)RGB16(65535-c)<<16);
  if (RGB16X2(pair)!=expected) { puts("FAIL packed color conversion"); return 1; }
 }
 puts("PASS all 65536 color values in both packed lanes");
 std::mt19937 rng(20261027);u16 src[640+8],expected[320+8],actual[320+8];
 for(int mode: {256,384,512,640}){
  for(int it=0;it<1500;it++){
   for(auto&v:src)v=(u16)rng();
   memset(expected,0x6d,sizeof(expected));memset(actual,0x6d,sizeof(actual));
   ref(mode,src,expected);fast(mode,src,actual);
   if(memcmp(expected,actual,sizeof(actual))){printf("FAILED mode=%d iteration=%d\n",mode,it);return 1;}
  }
  printf("PASS %d->320 pixels x1500\n",mode);
 }
 u16 a[192],b[192];
 for(int size=1;size<160;size++)for(int off=0;off<2;off++)for(int i=0;i<100;i++){
  memset(a,0x49,sizeof(a));memset(b,0x49,sizeof(b));
  u16 val=(u16)rng();tile_ref(a+off,size,val);tile_fast(b+off,size,val);
  if(memcmp(a,b,sizeof(a))){printf("FAILED tile len=%d off=%d\n",size,off);return 1;}
 }
 puts("PASS tile fill length 1..159, aligned and offset by 2 bytes");
 if(argc>1){
  volatile u32 checksum=0;
  for(int mode:{256,384,512,640}){
   auto bench=[&](bool optimized){
    auto start=std::chrono::steady_clock::now();
    for(int r=0;r<300000;r++){
     if(optimized)fast(mode,src,actual);else ref(mode,src,actual);
     checksum=checksum+actual[r%320];
    }
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   };
   auto x=bench(false),y=bench(true);
   printf("Host microbench width %d: scalar=%.1f ms packed=%.1f ms ratio=%.2fx (NOT RS-97 FPS)\n",mode,x,y,x/y);
  }
  printf("checksum=%u\n",(unsigned)checksum);
 }
}
