// Gerald the Frog: Pondside Deluxe - Nintendo 3DS homebrew (libctru + citro2d)
// All in-game artwork uses low-resolution pixel rectangles; touch UI and key controls.
#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define SAVE_DIR  "sdmc:/3ds/gerald"
#define SAVE_FILE SAVE_DIR "/save.bin"
#define SAVE_MAGIC 0x47524C44u
#define SAVE_VERSION 2u
#define COL(r,g,b) C2D_Color32((r),(g),(b),255)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

// Sprite palette: 0 transparent, o outline, g green, h highlight, d shadow,
// l belly, w eyes, k pupils/mouth, p cheeks.
#define FW 24
#define FH 19
static const char *FROG[FH] = {
    "........................",
    "....ooooo......ooooo....",
    "...ogggggo....ogggggo...",
    "..ogwwwwgo....ogwwwwgo..",
    "..ogwkkwgo....ogwkkwgo..",
    "..ogwwwwgo....ogwwwwgo..",
    "..oogggggoooooogggggoo..",
    ".ooghhhhgggggggghhhhgoo.",
    "ooghhhhgggggggggghhhhgoo",
    "oghhhhgggggggggggghhhhgo",
    "ogggpggggggggggggpggggo.",
    "ogggppgggkkkkggggppgggo.",
    "oggggggggkggkgggggggggo.",
    "oogggggllllllllggggggoo.",
    ".oogggllllllllllggggoo..",
    "..oogdlllllllllldgoo....",
    ".oooodddlllllldddoooo...",
    "ooggggoooooooooooggggoo.",
    ".oooooo........oooooo..."
};

static u32 pc(char c) {
    switch (c) {
        case 'o': return COL(31,75,48); case 'g': return COL(79,189,65);
        case 'h': return COL(134,231,96); case 'd': return COL(48,132,54);
        case 'l': return COL(201,240,150); case 'w': return COL(255,253,225);
        case 'k': return COL(24,50,39); case 'p': return COL(240,118,125);
        case 'r': return COL(239,87,115); case 'y': return COL(255,217,83);
        case 'b': return COL(106,68,38);
    }
    return COL(31,75,48);
}
static void rect(float x,float y,float w,float h,u32 c) { C2D_DrawRectSolid(x,y,0.2f,w,h,c); }
static void circ(float x,float y,float r,u32 c) { C2D_DrawCircleSolid(x,y,0.2f,r,c); }
static void sprite(const char*const* rows,int count,float x,float y,float unit) {
    for(int j=0;j<count;j++)
        for(int i=0;rows[j][i];i++)
            if(rows[j][i]!='.') rect(x+i*unit,y+j*unit,unit+0.05f,unit+0.05f,pc(rows[j][i]));
}
static C2D_TextBuf fontBuf;
static void label(const char *s,float x,float y,float scale,u32 c) {
    C2D_Text t; C2D_TextParse(&t,fontBuf,s); C2D_TextOptimize(&t);
    C2D_DrawText(&t,C2D_WithColor,x,y,0.85f,scale,scale,c);
}
static void centered(const char *s,float cx,float y,float scale,u32 c) {
    C2D_Text t; float w,h;
    C2D_TextParse(&t,fontBuf,s); C2D_TextOptimize(&t);
    C2D_TextGetDimensions(&t,scale,scale,&w,&h);
    C2D_DrawText(&t,C2D_WithColor,cx-w*0.5f,y,0.85f,scale,scale,c);
}

// Little chiptune sounds: raw mono signed 16-bit, 22050Hz, bundled in RomFS.
enum { S_RIBBIT,S_EAT,S_HAPPY,S_CLICK,S_SLEEP,S_CLEAN,S_COIN,S_BUY,S_ERROR,S_COUNT };
typedef struct { const char*file; s16*data; ndspWaveBuf wb; } Sfx;
static Sfx sound[S_COUNT] = {
    {.file="romfs:/ribbit.pcm"},{.file="romfs:/eat.pcm"},{.file="romfs:/happy.pcm"},
    {.file="romfs:/click.pcm"},{.file="romfs:/sleep.pcm"},{.file="romfs:/clean.pcm"},
    {.file="romfs:/coin.pcm"},{.file="romfs:/buy.pcm"},{.file="romfs:/error.pcm"}
};
static bool soundEnabled=false;
static void initAudio(void) {
    if(R_FAILED(ndspInit())) return;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    soundEnabled=true;
    for(int i=0;i<S_COUNT;i++) {
        FILE*f=fopen(sound[i].file,"rb"); if(!f)continue;
        fseek(f,0,SEEK_END);long len=ftell(f); rewind(f);
        if(len<2 || len>262144 || (len&1)) {fclose(f);continue;}
        sound[i].data=linearAlloc((size_t)len);if(!sound[i].data){fclose(f);continue;}
        size_t got=fread(sound[i].data,1,(size_t)len,f);fclose(f);
        if(got!=(size_t)len){linearFree(sound[i].data);sound[i].data=NULL;continue;}
        DSP_FlushDataCache(sound[i].data,(u32)len);
        float mix[12]={0};mix[0]=mix[1]=0.63f;
        ndspChnReset(i);ndspChnSetInterp(i,NDSP_INTERP_LINEAR);
        ndspChnSetRate(i,22050.f);ndspChnSetFormat(i,NDSP_FORMAT_MONO_PCM16);
        ndspChnSetMix(i,mix);
        memset(&sound[i].wb,0,sizeof(ndspWaveBuf));
        sound[i].wb.data_vaddr=sound[i].data;
        sound[i].wb.nsamples=(u32)(len/2);
        sound[i].wb.status=NDSP_WBUF_DONE;
    }
}
static void sfx(int n) {
    if(!soundEnabled||n<0||n>=S_COUNT||!sound[n].data)return;
    if(sound[n].wb.status!=NDSP_WBUF_DONE&&sound[n].wb.status!=NDSP_WBUF_FREE)return;
    ndspChnWaveBufAdd(n,&sound[n].wb);
}
static void finishAudio(void) {
    if(!soundEnabled)return;
    ndspExit();for(int i=0;i<S_COUNT;i++)if(sound[i].data)linearFree(sound[i].data);
}

// New save is versioned; old 4-button Gerald saves are migrated automatically.
typedef struct {
    u32 magic, version;
    float hunger,happy,clean,energy;
    u32 savedAt,age,coins,owned,hat,deco,companion,fliesCaught,hopScore;
} Save;
typedef struct {u32 magic;float hunger,happy,clean,energy;u32 savedAt,age;} OldSave;
static Save g={SAVE_MAGIC,SAVE_VERSION,82,83,80,85,0,0,75,0,0,0,0,0,0};
static bool asleep=false;
static bool savePending=false;
static float seconds=0, autosave=0, blinkTimer=0, tipTime=0, petAnim=0, needsAccum=0;
static char tip[72]="Welcome back to the pond!";
typedef enum { PG_HOME, PG_SHOP, PG_GAMES, PG_BAG, PG_FLY, PG_HOP } Page;
static Page page=PG_HOME;
static int shopPage=0,shopSelection=0,bagSelection=0,gameSelection=0;
static float miniTime=0,flyX=150,flyY=110,flyTick=0;
static int miniScore=0,miniTarget=0;
static bool miniDone=false;
static int animType=0;
static float scoreFlash=0;
static u32 ink,cream,woodDark,woodLight,leaf,accent;

// Shop catalog: type 0 consumable, 1 hat, 2 scenery, 3 friend.
typedef struct {const char*name;const char*desc;int cost;int type;u32 flag;int variant;} Item;
static const Item catalog[]={
    {"Fly snack","Hunger +28",8,0,0,0},
    {"Berry pie","Hunger +55, joy +8",19,0,0,1},
    {"Star cap","A sparkly frog hat",55,1,1u<<0,1},
    {"Sun hat","Perfect for warm days",80,1,1u<<1,2},
    {"Pink lotus","Pond flower decoration",30,2,1u<<2,1},
    {"Mushroom","Tiny red mushroom",42,2,1u<<3,2},
    {"Duck buddy","A forever friend",68,3,1u<<4,1},
    {"Firefly jar","Cozy evening lights",85,2,1u<<5,3}
};
static const int bagItems[]={2,3,4,5,6,7};

static float lim(float v){return v<0?0:(v>100?100:v);}
static void message(const char*s) {snprintf(tip,sizeof(tip),"%s",s);tipTime=3.2f;}
static void saveGame(void) {
    mkdir("sdmc:/3ds",0777);mkdir(SAVE_DIR,0777);
    g.savedAt=(u32)time(NULL); g.version=SAVE_VERSION;g.magic=SAVE_MAGIC;
    FILE*f=fopen(SAVE_FILE ".tmp","wb");
    if(!f)return;
    bool ok=fwrite(&g,sizeof(g),1,f)==1;
    if(fclose(f)!=0)ok=false;
    if(!ok)return;
    // Keep a backup until the replacement has safely reached the save slot.
    remove(SAVE_FILE ".bak");
    rename(SAVE_FILE,SAVE_FILE ".bak");
    if(rename(SAVE_FILE ".tmp",SAVE_FILE)==0) {
        remove(SAVE_FILE ".bak");savePending=false;
    } else rename(SAVE_FILE ".bak",SAVE_FILE);
}
static void loadGame(void) {
    FILE*f=fopen(SAVE_FILE,"rb");
    if(!f) f=fopen(SAVE_FILE ".bak","rb");
    if(!f)return;
    fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);
    if(len==(long)sizeof(Save)) {
        Save tmp;
        if(fread(&tmp,sizeof(tmp),1,f)==1&&tmp.magic==SAVE_MAGIC&&tmp.version==SAVE_VERSION)g=tmp;
    }else if(len==(long)sizeof(OldSave)) {
        OldSave old;
        if(fread(&old,sizeof(old),1,f)==1&&old.magic==SAVE_MAGIC) {
            g.hunger=old.hunger;g.happy=old.happy;g.clean=old.clean;g.energy=old.energy;
            g.age=old.age;g.savedAt=old.savedAt;message("Old Gerald save upgraded!");savePending=true;
        }
    }
    fclose(f);
    time_t now=time(NULL);
    if(g.savedAt>0 && now>(time_t)g.savedAt) {
        double elapsed=difftime(now,(time_t)g.savedAt);
        if(elapsed>14400)elapsed=14400; // cap absence at 4h
        g.age+=(u32)elapsed;
        g.hunger=lim(g.hunger-(float)(elapsed/900.0));
        g.happy=lim(g.happy-(float)(elapsed/1200.0));
        g.clean=lim(g.clean-(float)(elapsed/1700.0));
        g.energy=lim(g.energy+(float)(elapsed/1500.0));
    }
    g.hunger=lim(g.hunger);g.happy=lim(g.happy);
    g.clean=lim(g.clean);g.energy=lim(g.energy);
    if(g.coins>999999)g.coins=999999;
    g.hat=(g.hat<=2?g.hat:0);g.deco=(g.deco<=3?g.deco:0);
    g.companion=(g.companion<=1?g.companion:0);
}
static void earn(int n){if(g.coins<=999999-(u32)n)g.coins+=(u32)n;savePending=true;}
static void action(int a) { // feed, play, clean, sleep
    if(a==3) {asleep=!asleep;message(asleep?"Shhh... Gerald is dreaming":"Gerald woke up. Ribbit!");sfx(asleep?S_SLEEP:S_RIBBIT);return;}
    if(asleep){message("Let sleeping frogs lie!");sfx(S_ERROR);return;}
    if(a==0){
        if(g.hunger>=97){message("Gerald's tummy is full!");sfx(S_CLICK);return;}
        g.hunger=lim(g.hunger+22);g.happy=lim(g.happy+2);
        animType=1;petAnim=1.8f;message("Nom nom! Delicious flies!");sfx(S_EAT);
    }else if(a==1){
        if(g.energy<12){message("Too sleepy to play!");sfx(S_ERROR);return;}
        g.happy=lim(g.happy+23);g.energy=lim(g.energy-7);
        g.hunger=lim(g.hunger-3);animType=2;petAnim=2.0f;
        earn(2);message("Boing! +2 pond coins");sfx(S_HAPPY);
    }else if(a==2){
        if(g.clean>=97){message("Gerald is already squeaky clean!");sfx(S_CLICK);return;}
        g.clean=lim(g.clean+37);g.happy=lim(g.happy+3);
        animType=3;petAnim=1.5f;earn(1);message("Squeaky clean! +1 coin");sfx(S_CLEAN);
    }
    savePending=true;
}
static bool unlocked(const Item*it){return it->type==0||(g.owned&it->flag)!=0;}
static bool equipped(const Item*it) {
    if(it->type==1)return g.hat==(u32)it->variant;
    if(it->type==2)return g.deco==(u32)it->variant;
    if(it->type==3)return g.companion==(u32)it->variant;
    return false;
}
static void equip(const Item*it) {
    if(!unlocked(it)){message("Buy it in the shop first!");sfx(S_ERROR);return;}
    if(it->type==1)g.hat=equipped(it)?0:(u32)it->variant;
    if(it->type==2)g.deco=equipped(it)?0:(u32)it->variant;
    if(it->type==3)g.companion=equipped(it)?0:(u32)it->variant;
    message(equipped(it)?"Equipped! Gerald likes it!":"Decoration put away");sfx(S_CLICK);savePending=true;
}
static void buy(int idx) {
    if(idx<0||idx>=(int)ARRAY_SIZE(catalog))return;
    const Item*it=&catalog[idx];
    if(unlocked(it)&&it->type!=0){message("Already owned! Check your bag");sfx(S_CLICK);return;}
    if(g.coins<(u32)it->cost){message("Not enough pond coins!");sfx(S_ERROR);return;}
    g.coins-=(u32)it->cost;
    if(it->type==0) {
        g.hunger=lim(g.hunger+(it->variant?55:28));
        if(it->variant)g.happy=lim(g.happy+8);
        animType=1;petAnim=1.5f;message("Yum! Gerald loved that!");
    } else {
        g.owned|=it->flag;
        if(it->type==1)g.hat=(u32)it->variant;
        if(it->type==2)g.deco=(u32)it->variant;
        if(it->type==3)g.companion=(u32)it->variant;
        message("Purchased & equipped!");
    }
    sfx(S_BUY);savePending=true;
}
static void beginMini(Page target) {
    page=target;miniTime=20;miniDone=false;miniScore=0;flyTick=0;
    flyX=100+(rand()%120);flyY=65+(rand()%90);
    miniTarget=rand()%3;message(target==PG_FLY?"Catch flies! Tap the moving bug":"Lily Hop! Tap the glowing pad");sfx(S_HAPPY);
}
static void miniInput(int x,int y,bool select) {
    if(miniDone){page=PG_GAMES;sfx(S_CLICK);return;}
    if(page==PG_FLY) {
        if(select||((x-flyX)*(x-flyX)+(y-flyY)*(y-flyY)<25*25)) {
            ++miniScore;g.fliesCaught++;earn(3);flyX=52+rand()%215;flyY=65+rand()%95;flyTick=0;
            scoreFlash=.35f;sfx(S_COIN);
        } else sfx(S_CLICK);
    } else if(page==PG_HOP) {
        int chosen=-1;
        if(x>=16&&x<304&&y>85&&y<190) chosen=x<112?0:(x<210?1:2);
        if(chosen==miniTarget) {
            ++miniScore;g.hopScore++;earn(4);miniTarget=rand()%3;
            scoreFlash=.4f;sfx(S_COIN);
        } else sfx(S_ERROR);
    }
}
static void tick(float dt) {
    seconds+=dt;autosave+=dt;blinkTimer+=dt;
    needsAccum+=dt;
    if(needsAccum>=1.f) {
        float e=needsAccum;needsAccum=0;
        g.age+=(u32)e;
        g.hunger=lim(g.hunger-(asleep?.008f:.022f)*e);
        g.happy=lim(g.happy-(asleep?.007f:.016f)*e);
        g.clean=lim(g.clean-.010f*e);
        g.energy=lim(g.energy+(asleep?.47f:-.017f)*e);
        if(asleep&&g.energy>=100){asleep=false;message("Gerald feels refreshed!");sfx(S_RIBBIT);}
        if(!asleep&&g.energy<=0){asleep=true;message("Gerald went to sleep");sfx(S_SLEEP);}
    }
    if(tipTime>0)tipTime-=dt;
    if(petAnim>0)petAnim-=dt;
    if(scoreFlash>0)scoreFlash-=dt;
    if((page==PG_FLY||page==PG_HOP)&&!miniDone) {
        miniTime-=dt;
        if(page==PG_FLY) {
            flyTick+=dt;
            flyX+=sinf(seconds*3.9f)*35.f*dt;
            flyY+=cosf(seconds*5.2f)*28.f*dt;
            if(flyX<34)flyX=34;
            if(flyX>288)flyX=288;
            if(flyY<64)flyY=64;
            if(flyY>172)flyY=172;
        }
        if(miniTime<=0){miniTime=0;miniDone=true;message("Finished! Your coins are saved");sfx(S_HAPPY);saveGame();}
    }
    if(autosave>=25){autosave=0;saveGame();}
}

// Textured wooden GUI helpers. The grain is purposefully pixelated, not an image filter.
static void wood(float x,float y,float w,float h,bool lightwood) {
    rect(x,y,w,h,lightwood?COL(157,99,54):COL(77,49,33));
    for(int i=0;i<7;i++){
        int yy=(int)y+5+i*(int)(h/7.f);
        if(yy>y+h-2)break;
        rect(x+2,yy,w-4,(i%2)?2:1,lightwood?COL(187,121,67):COL(104,67,41));
        rect(x+7+(i*29)%(int)(w>30?w-23:8),yy+2,13,1,lightwood?COL(128,77,42):COL(56,34,25));
    }
}
static void panel(float x,float y,float w,float h,bool bright) {
    rect(x+2,y+3,w,h,COL(48,29,21));
    wood(x,y,w,h,bright);
    rect(x,y,w,3,bright?COL(217,153,82):COL(126,81,48));
    rect(x,y,3,h,bright?COL(208,141,74):COL(121,77,48));
    rect(x+w-3,y,3,h,COL(59,37,25));
    rect(x,y+h-3,w,3,COL(64,37,24));
    if(w>20&&h>18){
        rect(x+6,y+7,3,3,COL(65,60,55));rect(x+w-10,y+7,3,3,COL(65,60,55));
        rect(x+6,y+h-10,3,3,COL(65,60,55));rect(x+w-10,y+h-10,3,3,COL(65,60,55));
    }
}
static void pad(float x,float y,float w,float h,u32 c) {
    rect(x+2,y+3,w,h,COL(40,30,25));rect(x,y,w,h,c);
    rect(x+3,y+3,w-6,2,COL(229,191,113));
    rect(x+3,y+h-5,w-6,2,COL(91,55,34));
}
static void coin(float x,float y,float r) {
    circ(x,y,r+2,COL(107,58,31));circ(x,y,r,COL(247,170,43));
    circ(x-1,y-1,r-3,COL(255,215,89));
    rect(x-1,y-5,2,10,COL(154,98,30));
}
static void icon(int id,float x,float y,float u) {
    // Icons draw as tiny blocks at multiple scaled sizes.
    if(id==0) { // fly
        rect(x+4*u,y+2*u,3*u,3*u,COL(31,49,40));
        rect(x+1*u,y+1*u,3*u,2*u,COL(214,247,234));
        rect(x+6*u,y+1*u,3*u,2*u,COL(214,247,234));
        rect(x+5*u,y+4*u,1*u,2*u,COL(31,49,40));
    }else if(id==1) { // heart
        rect(x+1*u,y+1*u,3*u,3*u,COL(243,100,111));
        rect(x+5*u,y+1*u,3*u,3*u,COL(243,100,111));
        rect(x+2*u,y+3*u,5*u,3*u,COL(243,100,111));
        rect(x+3*u,y+6*u,3*u,1*u,COL(243,100,111));
    }else if(id==2){ // bubbles
        circ(x+3*u,y+4*u,2*u,COL(166,219,232));circ(x+7*u,y+2*u,1.8f*u,COL(205,245,250));
        circ(x+8*u,y+7*u,1.2f*u,COL(173,222,241));
    }else if(id==3){ // moon
        circ(x+4*u,y+4*u,3.5f*u,COL(255,223,109));
        circ(x+6*u,y+2*u,3.0f*u,COL(109,75,45));
    }else if(id==4){ // lily
        circ(x+5*u,y+5*u,3*u,COL(64,151,70));
        rect(x+5*u,y+2*u,3*u,2*u,COL(84,176,90));
    }else if(id==5){ // star
        rect(x+4*u,y,2*u,9*u,COL(255,216,90));
        rect(x,y+3*u,10*u,2*u,COL(255,216,90));
        rect(x+2*u,y+2*u,6*u,5*u,COL(255,216,90));
    }
}
static void bar(float x,float y,float w,float value,u32 c){
    rect(x,y,w,10,COL(36,28,27));
    rect(x+2,y+2,(w-4)*value/100.f,6,c);
    rect(x+1,y+1,w-2,1,COL(222,174,95));
}

// Top screen: chunky pixel-art landscape; scene stays interactive as you care for Gerald.
static void cattail(int x,int y,int sc) {
    rect(x,y,sc,y+26<240?30:12,COL(46,111,55));
    rect(x-2*sc,y+7*sc,2*sc,7*sc,COL(74,149,71));
    rect(x+sc,y+6*sc,2*sc,7*sc,COL(83,167,74));
    rect(x,y-6*sc,3*sc,8*sc,COL(126,76,42));
    rect(x,y-6*sc,3*sc,2*sc,COL(174,116,67));
}
static void lotus(float x,float y,int s) {
    rect(x-8*s,y,16*s,2*s,COL(44,103,54));
    rect(x-4*s,y-8*s,8*s,6*s,COL(246,151,186));
    rect(x-8*s,y-5*s,5*s,4*s,COL(255,194,215));
    rect(x+3*s,y-5*s,5*s,4*s,COL(255,194,215));
    rect(x-2*s,y-5*s,4*s,4*s,COL(255,225,98));
}
static void drawFrog(float x,float y,float scale) {
    bool blink=(fmodf(seconds,5.4f)<.15f),smile=g.happy>50;
    float bob= sinf(seconds*2.1f)*1.6f;
    float hop=(animType==2&&petAnim>0)?fabsf(sinf(petAnim*8))*24:0;
    if(asleep){bob=sinf(seconds*.9f);hop=0;}
    static char body[FH][FW+1];static const char*ptrs[FH];
    for(int j=0;j<FH;j++){strncpy(body[j],FROG[j],FW);body[j][FW]=0;ptrs[j]=body[j];}
    if(blink||asleep){for(int k=4;k<=5;k++)for(int j=0;j<2;j++){
        body[k][6+j]='k';body[k][16+j]='k';
    }}
    if(!smile){for(int j=9;j<=14;j++)body[11][j]='g';body[12][10]='k';body[12][11]='k';body[12][12]='k';}
    if(g.clean<30){body[9][8]='b';body[15][15]='b';body[13][4]='b';}
    sprite(ptrs,FH,x,y+bob-hop,scale);
    // Cosmetic hats are drawn on top, never permanently overwrite Gerald's sprite.
    if(g.hat==1){
        rect(x+scale*9,y-scale*3+bob-hop,scale*6,scale*2,COL(242,194,74));
        icon(5,x+scale*10,y-scale*6+bob-hop,scale*.45f);
    } else if(g.hat==2){
        rect(x+scale*5,y-scale*2+bob-hop,scale*14,scale,COL(242,188,91));
        rect(x+scale*9,y-scale*5+bob-hop,scale*6,scale*4,COL(238,202,120));
        rect(x+scale*9,y-scale*2+bob-hop,scale*6,scale,COL(180,105,60));
    }
    if(animType==3&&petAnim>0)for(int i=0;i<6;i++){
        float bx=x+(i*31)%115,by=y+(i*23)%80;
        circ(bx,by-petAnim*15,3+(i%2)*2,COL(200,238,245));
    }
    if(animType==2&&petAnim>0){icon(1,x+scale*24,y+2,1.6f);}
    if(animType==1&&petAnim>0){icon(0,x+scale*21,y+scale*6,1.1f);}
}
static void drawPond(void) {
    time_t now=time(NULL);struct tm*tmv=localtime(&now);
    bool night=tmv&&(tmv->tm_hour>=20||tmv->tm_hour<6);
    // Sky with crisp 16px bands and mountain layers.
    rect(0,0,400,148,night?COL(31,58,101):COL(104,196,226));
    rect(0,35,400,48,night?COL(40,70,117):COL(131,213,230));
    rect(0,84,400,45,night?COL(54,85,128):COL(163,223,226));
    for(int i=0;i<17;i++){
        int x=i*27;
        rect(x,108-(i%4)*7,28,43,night?COL(47,98,115):COL(95,158,137));
        rect(x+10,97-(i%4)*7,12,13,night?COL(47,98,115):COL(95,158,137));
    }
    if(night){
        circ(325,32,14,COL(249,234,160));
        for(int i=0;i<15;i++)rect(12+(i*53)%375,14+(i*31)%70,3,3,COL(230,239,209));
    }else{
        circ(338,35,19,COL(251,234,163));
        int cx=(int)fmodf(seconds*3,455)-60;
        rect(cx,28,58,12,COL(244,253,244));rect(cx+12,20,28,13,COL(244,253,244));
        rect(cx+26,40,36,8,COL(222,244,245));
    }
    // Water and ripples.
    rect(0,146,400,94,night?COL(30,87,119):COL(39,139,173));
    rect(0,146,400,5,night?COL(52,103,128):COL(80,187,198));
    for(int i=0;i<24;i++){
        float x=fmodf(i*57+(seconds*(i%3+1)*2.8f),435)-20;
        int y=155+(i*19)%81;
        rect(x,y,12+(i%3)*9,2,night?COL(66,132,157):COL(115,203,208));
    }
    // Left tree trunk and leafy canopy.
    rect(4,52,34,104,COL(88,58,40));rect(11,48,15,108,COL(119,76,42));
    rect(17,63,3,80,COL(160,101,54));
    for(int i=0;i<10;i++){
        int tx=-8+(i%5)*23, ty=8+(i/5)*23;
        rect(tx,ty,40,27,i%3?COL(38,110,66):COL(54,136,72));
        rect(tx+6,ty+4,23,9,COL(88,162,74));
    }
    // Wooden dock on the left + den.
    rect(0,155,106,12,COL(92,57,36));
    for(int i=0;i<4;i++){
        rect(i*26,154,23,7,COL(162,104,55));
        rect(i*26+7,159,2,3,COL(66,44,29));
    }
    rect(23,165,8,22,COL(105,66,39));rect(88,165,8,20,COL(105,66,39));
    rect(32,110,64,44,COL(102,67,43));rect(26,105,76,9,COL(68,117,56));
    rect(37,115,53,36,COL(152,97,55));rect(58,125,16,26,COL(57,39,31));
    rect(51,121,28,5,COL(79,54,34));rect(76,139,3,3,COL(245,209,107));
    // Cattails and flowers frame the scene.
    cattail(18,183,2);cattail(376,180,2);cattail(355,212,1);
    rect(360,174,28,7,COL(52,118,57));
    for(int i=0;i<6;i++){
        int xx=30+i*62,yy=181+(i%2)*38;
        rect(xx,yy,32,6,COL(35,115,75));rect(xx+6,yy-3,24,4,COL(80,168,91));
    }
    lotus(43,214,1);lotus(350,196,1);
    // Gerald's lily pad, with dimensional leafy edges.
    rect(117,169,176,18,COL(24,89,61));
    rect(124,163,164,17,COL(54,146,68));
    rect(137,160,138,5,COL(93,184,83));
    rect(206,163,20,8,COL(37,116,65));
    // Scenery chosen in the shop.
    if(g.deco==1)lotus(304,167,2);
    if(g.deco==2){
        rect(307,154,4,13,COL(236,221,184));rect(298,149,24,9,COL(229,77,81));
        rect(302,147,15,4,COL(247,122,92));rect(304,151,3,3,COL(250,232,192));
    }
    if(g.deco==3){
        rect(317,126,5,35,COL(106,68,39));rect(307,114,26,18,COL(173,118,68));
        for(int i=0;i<4;i++)circ(311+i*6,122+(i%2)*4,2.5f,COL(255,216,95));
    }
    if(g.companion==1){
        rect(289,175,29,15,COL(255,220,93));rect(294,169,15,13,COL(255,222,91));
        rect(305,173,4,3,COL(31,58,50));rect(309,178,7,5,COL(243,134,53));
        rect(299,189,21,3,COL(115,195,198));
    }
    drawFrog(140,79,5.f);
    // Pixelated edge decorations and HUD.
    rect(0,0,400,17,night?COL(40,59,61):COL(44,104,73));
    label("GERALD'S POND",9,1,.58f,COL(250,239,175));
    char info[64];snprintf(info,sizeof(info),"DAY %lu",(unsigned long)(g.age/86400+1));
    label(info,316,1,.5f,COL(250,239,175));
    rect(0,209,400,31,COL(26,56,56));rect(0,209,400,2,COL(76,155,125));
    const char*stat=asleep?"Zzz... dreaming of lily pads":
        (g.hunger<25?"Gerald wants a snack!":(g.happy<30?"Gerald is feeling bored":
        (g.clean<25?"Time for a bubble bath!":"A lovely day at the pond!")));
    centered(tipTime>0?tip:stat,200,216,.56f,COL(255,242,204));
}

// Lower-screen wooden control deck.
static void frameBottom(void) {
    wood(0,0,320,240,false);
    rect(0,0,320,6,COL(51,33,23));rect(0,234,320,6,COL(43,28,22));
    for(int i=0;i<20;i++){
        int x=(i*43+7)%320,y=(i*31+13)%240;
        rect(x,y,12+(i%4)*4,1,COL(104,69,44));
    }
    rect(5,7,310,33,COL(53,34,25));
    label("Gerald",14,9,.75f,COL(243,210,144));
    coin(246,22,9);
    char count[36];snprintf(count,sizeof(count),"%lu",(unsigned long)g.coins);
    label(count,260,12,.62f,COL(255,232,155));
}
static void navigation(void) {
    static const char*names[]={"HOME","SHOP","GAMES","BAG"};
    for(int i=0;i<4;i++){
        int x=6+i*78;
        panel(x,205,74,31,(int)page==i);
        centered(names[i],x+37,211,.50f,(int)page==i?COL(61,35,25):COL(245,216,158));
    }
}
static void drawHome(void) {
    panel(8,43,304,65,true);
    label("HUNGER",17,47,.38f,ink);bar(87,51,65,g.hunger,COL(111,194,83));
    label("JOY",165,47,.38f,ink);bar(222,51,79,g.happy,COL(252,198,71));
    label("CLEAN",17,72,.38f,ink);bar(87,76,65,g.clean,COL(92,204,235));
    label("ENERGY",165,72,.38f,ink);bar(222,76,79,g.energy,COL(163,150,228));
    label("Care for your tiny green friend!",18,90,.39f,ink);
    static const char*names[]={"FEED  A","PLAY  B","CLEAN  X","SLEEP Y"};
    for(int i=0;i<4;i++){
        int x=8+(i%2)*155,y=114+(i/2)*41;
        panel(x,y,148,36,true);
        icon(i,x+10,y+7,2.1f);
        label(names[i],x+43,y+10,.43f,ink);
    }
    label("Catch flies & hop for more coins!",17,195,.39f,cream);
}
static void drawShop(void) {
    label("THE LILY SHOP",14,41,.60f,cream);
    char pg[24];snprintf(pg,sizeof(pg),"%d / 2",shopPage+1);
    pad(208,42,29,21,COL(146,98,57));
    centered("<",222,45,.53f,ink);
    centered(pg,261,45,.44f,cream);
    pad(288,42,26,21,COL(146,98,57));
    centered(">",301,45,.53f,ink);
    for(int i=0;i<4;i++){
        int idx=shopPage*4+i;const Item *it=&catalog[idx];
        int y=66+i*34;
        panel(9,y,302,31,idx==shopSelection);
        char buf[80];
        snprintf(buf,sizeof(buf),"%s",it->name);label(buf,19,y+4,.49f,ink);
        label(it->desc,19,y+18,.31f,ink);
        if(unlocked(it)&&it->type!=0){
            label("OWNED",256,y+8,.39f,COL(47,104,49));
        }else{
            char price[18];snprintf(price,sizeof(price),"%d",it->cost);
            coin(252,y+14,6);label(price,266,y+7,.43f,ink);
        }
    }
    label("Touch to buy - arrows change page",15,198,.36f,cream);
}
static void drawGames(void) {
    label("POND ARCADE",16,42,.60f,cream);
    panel(10,69,300,55,gameSelection==0);
    icon(0,22,83,3);
    label("CATCH THE FLIES",64,76,.56f,ink);
    label("20 seconds - earn 3 coins per fly",64,101,.33f,ink);
    panel(10,134,300,55,gameSelection==1);
    icon(4,22,150,3);
    label("LILY HOP",64,140,.57f,ink);
    label("Tap the glowing lily - 4 coins",64,165,.33f,ink);
    label("Touch a game, or use D-pad + A",18,195,.37f,cream);
}
static void drawBag(void) {
    label("GERALD'S COLLECTION",13,42,.55f,cream);
    for(int i=0;i<6;i++){
        int idx=bagItems[i];const Item*it=&catalog[idx];
        int col=i%2,row=i/2,x=8+col*155,y=70+row*39;
        panel(x,y,148,36,i==bagSelection);
        if(unlocked(it)){
            label(it->name,x+8,y+5,.43f,ink);
            label(equipped(it)?"[ON]":"tap to equip",x+8,y+20,.33f,equipped(it)?COL(49,117,47):ink);
        }else{
            label("??? LOCKED",x+8,y+7,.45f,ink);
            label("Visit the shop",x+8,y+23,.31f,ink);
        }
    }
    label("Hats, buddies and pond decorations",14,194,.37f,cream);
}
static void drawMini(void) {
    char buf[80];
    snprintf(buf,sizeof(buf),"%d SEC     SCORE %d",(int)ceilf(miniTime),miniScore);
    centered(page==PG_FLY?"FLY CATCHER":"LILY HOP",160,47,.65f,cream);
    centered(buf,160,76,.47f,cream);
    if(miniDone){
        panel(14,106,292,74,true);
        snprintf(buf,sizeof(buf),"GOOD JOB! +%d COINS",miniScore*(page==PG_FLY?3:4));
        centered(buf,160,115,.63f,ink);
        centered("Tap / A to return",160,151,.46f,ink);
    }else if(page==PG_FLY){
        rect(19,109,282,82,COL(64,112,84));
        for(int i=0;i<5;i++)rect(25+i*57,182-(i%3)*15,25,3,COL(117,168,105));
        icon(0,flyX-13,flyY-12,2.5f);
        centered("Tap the fly! (A also catches)",160,192,.38f,cream);
    }else{
        for(int i=0;i<3;i++){
            int x=26+i*101;
            panel(x,115,88,58,i==miniTarget);
            rect(x+14,140,60,16,i==miniTarget?COL(111,216,93):COL(62,146,78));
            rect(x+19,134,50,7,i==miniTarget?COL(154,242,111):COL(94,183,90));
            if(i==miniTarget)icon(5,x+36,120,1.7f);
        }
        centered("Tap the glowing pad!",160,188,.42f,cream);
    }
    centered("B: leave  |  START: save & exit",160,216,.38f,cream);
}
static void drawBottom(void) {
    frameBottom();
    if(page==PG_HOME)drawHome();
    else if(page==PG_SHOP)drawShop();
    else if(page==PG_GAMES)drawGames();
    else if(page==PG_BAG)drawBag();
    else drawMini();
    if(page<=PG_BAG)navigation();
}

static void touch(int x,int y) {
    if(page==PG_FLY||page==PG_HOP){miniInput(x,y,false);return;}
    if(y>=205) {
        int t=(x-6)/78;
        if(x>=6&&t>=0&&t<4){page=(Page)t;sfx(S_CLICK);}
        return;
    }
    if(page==PG_HOME){
        if(y>=114&&y<191){int col=(x>=163)?1:0,row=(y>=155)?1:0;
            action(row*2+col);}
    }else if(page==PG_SHOP){
        if(y>=42&&y<65&&x>=208) {
            if(x<239)shopPage=0;
            if(x>=286)shopPage=1;
            shopSelection=shopPage*4;sfx(S_CLICK);return;
        }
        if(y>=66&&y<202) {
            int idx=shopPage*4+(y-66)/34;
            if(idx>=0&&idx<8){shopSelection=idx;buy(idx);}
        }
    }else if(page==PG_GAMES){
        if(y>=69&&y<125){gameSelection=0;beginMini(PG_FLY);}
        if(y>=134&&y<190){gameSelection=1;beginMini(PG_HOP);}
    }else if(page==PG_BAG){
        if(y>=70&&y<187){int col=x>=163?1:0,row=(y-70)/39;
            int bi=row*2+col;
            if(bi>=0&&bi<6){bagSelection=bi;equip(&catalog[bagItems[bi]]);}
        }
    }
}
static void buttons(u32 kd) {
    if(kd&KEY_START){saveGame();return;}
    if(page==PG_FLY||page==PG_HOP){
        if(kd&KEY_B){page=PG_GAMES;sfx(S_CLICK);return;}
        if(kd&KEY_A)miniInput((int)flyX,(int)flyY,true);
        if(page==PG_HOP){
            int choice=-1;
            if(kd&KEY_LEFT)choice=0;
            if(kd&KEY_DOWN)choice=1;
            if(kd&KEY_RIGHT)choice=2;
            if(choice>=0)miniInput(60+choice*101,145,false);
        }
        return;
    }
    if(kd&KEY_L){page=(Page)(((int)page+3)%4);sfx(S_CLICK);}
    if(kd&KEY_R){page=(Page)(((int)page+1)%4);sfx(S_CLICK);}
    if(page==PG_HOME){
        if(kd&KEY_A)action(0);
        if(kd&KEY_B)action(1);
        if(kd&KEY_X)action(2);
        if(kd&KEY_Y)action(3);
    }else if(page==PG_SHOP){
        if(kd&KEY_LEFT){shopPage=0;shopSelection=0;sfx(S_CLICK);}
        if(kd&KEY_RIGHT){shopPage=1;shopSelection=4;sfx(S_CLICK);}
        if(kd&KEY_DOWN){shopSelection=(shopSelection+1)%8;shopPage=shopSelection/4;}
        if(kd&KEY_UP){shopSelection=(shopSelection+7)%8;shopPage=shopSelection/4;}
        if(kd&KEY_A)buy(shopSelection);
    }else if(page==PG_GAMES){
        if(kd&(KEY_UP|KEY_DOWN))gameSelection^=1;
        if(kd&KEY_A)beginMini(gameSelection?PG_HOP:PG_FLY);
    }else if(page==PG_BAG){
        if(kd&KEY_DOWN)bagSelection=(bagSelection+1)%6;
        if(kd&KEY_UP)bagSelection=(bagSelection+5)%6;
        if(kd&KEY_A)equip(&catalog[bagItems[bagSelection]]);
    }
}
int main(void) {
    gfxInitDefault();romfsInit();
    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)){romfsExit();gfxExit();return 1;}
    if(!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)){C3D_Fini();romfsExit();gfxExit();return 1;}
    C2D_Prepare();
    C3D_RenderTarget*top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
    C3D_RenderTarget*bottom=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
    fontBuf=C2D_TextBufNew(4096);
    ink=COL(51,33,24);cream=COL(245,224,176);
    woodDark=COL(85,50,32);woodLight=COL(170,112,65);
    leaf=COL(68,148,69);accent=COL(244,205,107);
    (void)woodDark;(void)woodLight;(void)leaf;(void)accent;
    srand((unsigned)time(NULL));initAudio();loadGame();sfx(S_RIBBIT);
    u64 prev=osGetTime();
    while(aptMainLoop()){
        u64 now=osGetTime();float dt=(now-prev)/1000.f;prev=now;
        if(dt<0||dt>0.25f)dt=1.f/60.f;
        hidScanInput();u32 kd=hidKeysDown();
        if(kd&KEY_START)break;
        if(kd&KEY_TOUCH){touchPosition pos;hidTouchRead(&pos);touch((int)pos.px,(int)pos.py);}
        buttons(kd);tick(dt);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(fontBuf);
        C2D_TargetClear(top,COL(0,0,0));C2D_SceneBegin(top);drawPond();
        C2D_TargetClear(bottom,COL(0,0,0));C2D_SceneBegin(bottom);drawBottom();
        C3D_FrameEnd(0);
    }
    saveGame();finishAudio();C2D_TextBufDelete(fontBuf);
    C2D_Fini();C3D_Fini();romfsExit();gfxExit();return 0;
}
