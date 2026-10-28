// PC graphics run through the original VSSettings controls, fonts and animations.
#include "abi_bridge.h"
#include "guest_function_imports.h"
#include "pc_menu_hooks.h"
#include "pc_graphics_settings.h"
#include "pc_online_rooms.h"
#include "pc_menu_locale.h"
#include "pc_live_locale.h"
#include <cstring>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cmath>
#include <unordered_set>
namespace {
constexpr uint32_t kLabel=0x7E000000u,kPcButtonId=0x7F00;
struct Resource {std::array<uint32_t,6> args;};
struct ExtraButton {uint32_t button,source,last,count;bool pause;std::vector<std::array<float,3>> pauseLayout;};
std::unordered_map<uint32_t,Resource> resources;
std::unordered_map<uint32_t,ExtraButton> extraButtons;
std::unordered_map<uint32_t,std::u16string> messages;
std::unordered_set<uint32_t> stockLabels;
std::array<uint32_t,5> originalRadioCounts{};
uint32_t activePage=0,ownerPage=0,sheet=0,textScratch=0;
bool creatingPc=false,closingPc=false;
bool backgroundAdded=false;
std::vector<std::pair<uint32_t,uint8_t>> hiddenOwner;
bool languageRefresh=false,captureMessages=true;
struct TextRecord {uint32_t control=0,id=0;std::string pane;bool paneSpecific=false,hasInfo=false;std::array<uint8_t,0xC4> info{};uint64_t order=0;};
std::unordered_map<std::string,TextRecord> recordedText;
uint64_t textOrder=0;
uint64_t onlineRevision=~uint64_t{0};
uint32_t Invoke(CpuContext* ctx,uint32_t address,std::initializer_list<uint32_t> args) {
    const CpuContext saved=*ctx;size_t reg=3;for(auto value:args)ctx->gpr[reg++]=value;
    const auto* entry=TranslatedFunctionRegistry::FindByAddressPtr(address);
    if(!entry||!entry->rawCpuInvoker)throw std::runtime_error("Missing original UI function");
    entry->rawCpuInvoker(ctx);const auto result=ctx->gpr[3];*ctx=saved;return result;
}
uint32_t Alloc(CpuContext* ctx,uint32_t bytes){return Invoke(ctx,MKW_GADDR(80229814),{bytes,4,0});}
uint32_t Section(){return Memory::Read32(Memory::Read32(MKW_GADDR(809C1E38)));}
uint32_t Label(uint32_t key,const std::u16string& value){messages[kLabel+key]=value;return kLabel+key;}
void Text(CpuContext* ctx,uint32_t control,uint32_t key,const std::u16string& value){if(control)Invoke(ctx,MKW_GADDR(8063DDB4),{control,Label(key,value),0});}
using PcMenuLocale::T;
std::array<std::u16string,5> names0,names1,names2;
std::array<std::vector<std::u16string>,5> choices0,choices1,choices2;
std::vector<std::u16string> scales,presets;
void RefreshMenuTranslations(){
names0={PcMenuLocale::Display(),T(u"Formato",u"Aspect Ratio",u"Format",u"Bildformat",u"Formato"),u"V-Sync",T(u"Mostrar FPS",u"Show FPS",u"Afficher les FPS",u"FPS anzeigen",u"Mostra FPS"),u"Bloom"};
names1={u"Antialiasing",T(u"Filtro anisotropico",u"Anisotropic Filter",u"Filtre anisotrope",u"Anisotropischer Filter",u"Filtro anisotropico"),T(u"Imagen nitida",u"Sharp Image",u"Image nette",u"Scharfes Bild",u"Immagine nitida"),T(u"Interpolacion",u"Interpolation",u"Interpolation",u"Interpolation",u"Interpolazione"),T(u"Vibracion",u"Vibration",u"Vibration",u"Vibration",u"Vibrazione")};
names2={T(u"Resolucion de ventana",u"Window Resolution",u"Résolution fenêtre",u"Fensterauflösung",u"Risoluzione finestra"),names1[4],T(u"Silenciar",u"Mute",u"Couper le son",u"Stummschalten",u"Disattiva audio"),u"",u""};
const auto off=T(u"No",u"Off",u"Non",u"Aus",u"No"),on=T(u"Si",u"On",u"Oui",u"An",u"Sì");
choices0={std::vector<std::u16string>{T(u"Ventana",u"Windowed",u"Fenêtre",u"Fenster",u"Finestra"),T(u"Sin bordes",u"Borderless",u"Sans bordure",u"Randlos",u"Senza bordi"),T(u"Completa",u"Fullscreen",u"Plein écran",u"Vollbild",u"Intero")},{T(u"Automatico",u"Automatic",u"Automatique",u"Automatisch",u"Automatico"),u"16:9",u"4:3"},{off,on},{off,on},{off,on}};
choices1={std::vector<std::u16string>{off,u"MSAA 4x"},{u"1x",u"8x",u"16x"},{off,on},{u"60 FPS",u"120 FPS"},{off,on}};
choices2={std::vector<std::u16string>{u"720p",u"1080p",u"1440p",u"2160p"},{off,on},{off,on},{u""},{u""}};
scales={T(u"Segun ventana",u"Window size",u"Selon la fenêtre",u"Fenstergröße",u"Secondo finestra"),u"Original (1x)",u"1.5x",u"2x",u"3x",u"4x",u"6x",u"8x"};
presets={u"Original Wii",T(u"Bajo",u"Low",u"Faible",u"Niedrig",u"Bassa"),T(u"Medio",u"Medium",u"Moyenne",u"Mittel",u"Media"),T(u"Alto",u"High",u"Élevée",u"Hoch",u"Alta"),u"Ultra",T(u"Personalizado",u"Custom",u"Personnalisée",u"Benutzerdefiniert",u"Personalizzata")};
}
void Bottom(CpuContext* ctx,const std::u16string& value){if(activePage)Text(ctx,Memory::Read32(activePage+0x2BC),80,value);}
void Configure(CpuContext* ctx,uint32_t page) {
    RefreshMenuTranslations();
    const auto title=sheet==2?PcMenuLocale::Language():sheet==1?PcMenuLocale::Quality():PcMenuLocale::Display();
    Memory::Write32(page+0x3E4,Label(1,title));Text(ctx,Memory::Read32(page+0x2B8),1,title);
    const auto& choices=sheet==2?choices2:sheet==1?choices1:choices0;const auto& names=sheet==2?names2:sheet==1?names1:names0;
    for(unsigned i=0;i<5;++i) {
        auto radio=page+0x6C4+i*0x298;const auto count=std::min<unsigned>(originalRadioCounts[i],choices[i].size());
        Memory::Write8(radio+0x80,sheet==2&&i>2);Memory::Write8(radio+0x285,sheet==2&&i>2);
        Memory::Write32(radio+0x200,count);auto selected=std::min(settings_overlay::PcGraphicsValue(sheet,i),count?count-1:0);
        Memory::Write32(radio+0x204,selected);Memory::Write32(radio+0x208,selected);
        Text(ctx,radio,10+i,names[i]);const auto options=Memory::Read32(radio+0x294);
        for(unsigned j=0;j<originalRadioCounts[i];++j){auto button=options+j*0x17C;Memory::Write8(button+0x80,j>=count);Invoke(ctx,MKW_GADDR(80606C48),{button,j==selected});if(j<count)Text(ctx,button,20+i*4+j,choices[i][j]);}
        RT_LOGF("pc-options","Native row %u supports %u choices\n",i,originalRadioCounts[i]);
    }
    const auto scroller=page+0x13BC;const auto& values=sheet==2?PcMenuLocale::Languages:sheet==1?presets:scales;
    Invoke(ctx,MKW_GADDR(8063F0C0),{scroller,static_cast<uint32_t>(values.size()),settings_overlay::PcGraphicsScroller(sheet)});
    Text(ctx,scroller,50,sheet==2?PcMenuLocale::Language():sheet==1?T(u"Calidad",u"Quality",u"Qualité",u"Qualität",u"Qualità"):T(u"Resolucion interna",u"Internal Resolution",u"Résolution interne",u"Interne Auflösung",u"Risoluzione interna"));
    const auto value=std::min<unsigned>(settings_overlay::PcGraphicsScroller(sheet),values.size()-1);
    Text(ctx,Memory::Read32(page+0x1984+0x464),51,values[value]);
    const auto next=sheet==0?PcMenuLocale::Quality():sheet==1?PcMenuLocale::Language():PcMenuLocale::Display();
    const auto external=Memory::Read32(page+0x48);if(Memory::Read32(page+0x4C))Text(ctx,Memory::Read32(external),60,next);
    if(const auto bottom=Memory::Read32(page+0x2BC);bottom){const auto ownerId=ownerPage?Memory::Read32(ownerPage+4):0;Memory::Write8(bottom+0x80,ownerId>=0x17&&ownerId<=0x1A);}
    Bottom(ctx,sheet==2?T(u"El idioma cambia inmediatamente. Guardado automatico.",u"Language changes immediately. Saved automatically.",u"Changement de langue immédiat. Sauvegarde auto.",u"Sprache wird sofort geändert. Automatisch gespeichert.",u"La lingua cambia subito. Salvataggio automatico."):sheet==1?T(u"Guardado automatico. AA y filtro se aplican al abrir el juego.",u"Saved automatically. AA and filtering apply next launch.",u"Sauvegarde auto. AA et filtre au prochain lancement.",u"Automatisch gespeichert. AA und Filter beim nächsten Start.",u"Salvataggio automatico. AA e filtro al prossimo avvio."):T(u"Guardado automatico. B: volver.",u"Saved automatically. B: back.",u"Sauvegarde auto. B : retour.",u"Automatisch gespeichert. B: zurück.",u"Salvataggio automatico. B: indietro."));
}
void Open(CpuContext* ctx,uint32_t owner) {
    if(activePage)return;ownerPage=owner;sheet=0;closingPc=false;
    const auto section=Section();auto page=Memory::Read32(section+8+0x73*4);
    if(!page){creatingPc=true;try{Invoke(ctx,MKW_GADDR(80622D08),{section,0x73});}catch(...){creatingPc=false;throw;}creatingPc=false;page=Memory::Read32(section+8+0x73*4);}
    if(!page)throw std::runtime_error("Original graphics page unavailable");
    activePage=page;for(unsigned i=0;i<5;++i)originalRadioCounts[i]=Memory::Read32(page+0x6C4+i*0x298+0x200);
    Memory::Write32(page+0x3E8,0xFFFFFFFFu);Memory::Write32(page+0x3EC,0xFFFFFFFFu);
    hiddenOwner.clear();const auto group=owner+0x24,array=Memory::Read32(group),count=Memory::Read32(group+0x10);
    for(unsigned i=0;i<count;++i){const auto control=Memory::Read32(array+i*4);if(control){hiddenOwner.push_back({control,Memory::Read8(control+0x80)});Memory::Write8(control+0x80,1);}}
    // In a race the original pause backdrop remains visible behind these
    // native controls. A menu-scene background intersects the race depth.
    backgroundAdded=false;
    Invoke(ctx,MKW_GADDR(80622E00),{section,0x73,0});
    RT_LOGF("pc-options","Opened ORIGINAL VSSettings controls from page %u\n",Memory::Read32(owner+4));
}
void Close(CpuContext* ctx,uint32_t page){closingPc=true;Memory::Write32(page+0x3E8,0xFFFFFFFFu);Invoke(ctx,MKW_GADDR(8060247C),{page});}
void AddButton(CpuContext* ctx,uint32_t page,uint32_t source,uint32_t last,unsigned count,bool pause,uint32_t handler) {
    if(extraButtons.contains(page)||!resources.contains(source))return;
    const auto group=page+0x24,n=Memory::Read32(group+0x10);if(n>32)throw std::runtime_error("Unexpected control count");
    auto arrays=Alloc(ctx,(n+1)*8),button=Alloc(ctx,0x254),holder=Alloc(ctx,0x14);if(!arrays||!button||!holder)throw std::runtime_error("UI allocation failed");
    auto original=Memory::Read32(group),z=Memory::Read32(group+4);
    for(unsigned i=0;i<n;++i){Memory::Write32(arrays+i*4,Memory::Read32(original+i*4));Memory::Write32(arrays+(n+1)*4+i*4,Memory::Read32(z+i*4));}
    Memory::Write32(arrays+n*4,0);Memory::Write32(arrays+(n+1)*4+n*4,0);
    Memory::Write32(group,arrays);Memory::Write32(group+4,arrays+(n+1)*4);Memory::Write32(group+0x10,n+1);
    Invoke(ctx,MKW_GADDR(805BD3A8),{button});Invoke(ctx,MKW_GADDR(8060246C),{page,n,button,0});
    const auto args=resources.at(source).args;Invoke(ctx,MKW_GADDR(805BD518),{button,args[0],args[1],args[2],1,args[4],0});
    const auto originalHolder=Memory::Read32(source+0x234);for(unsigned i=0;i<5;++i)Memory::Write32(holder+i*4,Memory::Read32(originalHolder+i*4));
    Memory::Write32(holder+4,page);Memory::Write32(holder+8,0);Memory::Write32(holder+12,0xFFFFFFFFu);Memory::Write32(holder+16,handler);
    Memory::Write32(button+0x240,kPcButtonId);Memory::Write32(button+0x234,holder);
    Text(ctx,button,70,PcMenuLocale::Advanced());extraButtons[page]={button,source,last,count,pause};
    RT_LOGF("pc-options","Added ORIGINAL graphics button on page %u\n",Memory::Read32(page+4));
}
void Position(uint32_t page) {
    const auto it=extraButtons.find(page);if(it==extraButtons.end())return;auto& e=it->second;
    if(e.pause&&e.count>=4&&Memory::Read32(page+8)==4){
        if(e.pauseLayout.empty())for(unsigned slot=0;slot<4;++slot)for(unsigned button=0;button<=e.count;++button){
            const auto p=(button==e.count?e.button:e.source+button*0x254)+4+slot*0x18;
            e.pauseLayout.push_back({Memory::ReadFloat32(p+4),Memory::ReadFloat32(p+12),Memory::ReadFloat32(p+16)});
        }
        for(unsigned slot=0;slot<4;++slot){const auto base=slot*(e.count+1);const auto first=e.pauseLayout[base][0],last=e.pauseLayout[base+e.count-1][0];
            for(unsigned button=0;button<=e.count;++button){const auto p=(button==e.count?e.button:e.source+button*0x254)+4+slot*0x18;
                Memory::WriteFloat32(p+4,first+(last-first)*button/e.count);
                Memory::WriteFloat32(p+12,e.pauseLayout[base+button][1]*0.86f);Memory::WriteFloat32(p+16,e.pauseLayout[base+button][2]*0.86f);
            }
        }return;
    }
    for(unsigned i=0;i<4;++i){const auto offset=4+i*0x18;float gap=55;
        if(e.count>1)gap=std::max(40.0f,std::fabs(Memory::ReadFloat32(e.source+offset+4)-Memory::ReadFloat32(e.last+offset+4))/(e.count-1));
        Memory::WriteFloat32(e.button+offset,Memory::ReadFloat32(e.last+offset));Memory::WriteFloat32(e.button+offset+4,Memory::ReadFloat32(e.last+offset+4)-gap);
    }
}
void UpdateSystemLanguage(){
    // This build is pinned to PAL RMCP01; this is SystemManager::sInstance.
    const auto manager=Memory::Read32(0x80386000u);const auto& language=PcMenuLocale::Current();
    const uint32_t value=language=="de"?2:language=="fr"?3:language=="es"?4:language=="it"?5:1;
    if(manager&&Memory::Contains(manager+0x5C,8)){Memory::Write32(manager+0x5C,value);Memory::Write32(manager+0x60,value);}
}
void Record(CpuContext* ctx,bool paneSpecific){
    if(!captureMessages)return;
    TextRecord record;record.control=ctx->gpr[3];record.paneSpecific=paneSpecific;
    record.id=ctx->gpr[paneSpecific?5:4];const auto info=ctx->gpr[paneSpecific?6:5];
    if(paneSpecific){for(unsigned i=0;i<127&&Memory::Contains(ctx->gpr[4]+i);++i){auto c=Memory::Read8(ctx->gpr[4]+i);if(!c)break;record.pane.push_back(static_cast<char>(c));}}
    if(info&&Memory::Contains(info,record.info.size())){std::memcpy(record.info.data(),Memory::GetPointer(info,record.info.size()),record.info.size());record.hasInfo=true;}
    record.order=++textOrder;recordedText[std::to_string(record.control)+(paneSpecific?"|"+record.pane:"*")]=std::move(record);
}
void RefreshLanguage(CpuContext* ctx){
    UpdateSystemLanguage();
    messages[kLabel+70]=PcMenuLocale::Advanced();messages[kLabel+100]=T(u"Elige un carné",u"Select a License",u"Choisissez un permis",u"Lizenz auswählen",u"Scegli una patente");messages[kLabel+101]=messages[kLabel+102]=PcMenuLocale::Options();messages[kLabel+103]=T(u"Canal Mario Kart",u"Mario Kart Channel",u"Chaîne Mario Kart",u"Mario Kart-Kanal",u"Canale Mario Kart");messages[kLabel+90]=PcOnlineRooms::Summary();
    if(activePage)Configure(ctx,activePage);
    std::vector<TextRecord> records;records.reserve(recordedText.size());for(const auto& [key,value]:recordedText)records.push_back(value);
    std::sort(records.begin(),records.end(),[](const auto& a,const auto& b){return a.order<b.order;});
    captureMessages=false;
    try {for(const auto& record:records){
        if(!Memory::Contains(record.control,0x174))continue;
        const auto info=textScratch+2056,box=textScratch+2260;
        if(record.hasInfo)std::memcpy(Memory::GetPointer(info,record.info.size()),record.info.data(),record.info.size());
        if(record.paneSpecific){for(size_t i=0;i<record.pane.size();++i)Memory::Write8(box+i,record.pane[i]);Memory::Write8(box+record.pane.size(),0);Invoke(ctx,MKW_GADDR(8063DCBC),{record.control,box,record.id,record.hasInfo?info:0});}
        else Invoke(ctx,MKW_GADDR(8063DDB4),{record.control,record.id,record.hasInfo?info:0});
    }}catch(...){captureMessages=true;throw;}
    captureMessages=true;RT_LOGF("pc-language","Changed immediately to %s; refreshed %zu native texts, same process\n",PcMenuLocale::Current().c_str(),records.size());
}
}
namespace PcMenuHooks {void NotifyClosed() noexcept {}void LanguageChanged() noexcept {languageRefresh=true;}}
extern "C" void PcSectionInit(CpuContext* ctx){resources.clear();extraButtons.clear();messages.clear();stockLabels.clear();recordedText.clear();textOrder=0;hiddenOwner.clear();backgroundAdded=false;activePage=ownerPage=textScratch=0;closingPc=false;onlineRevision=~uint64_t{0};UpdateSystemLanguage();PcLiveLocale::SetRaceContext(Invoke(ctx,MKW_GADDR(80631588),{ctx->gpr[4]})==2);MKW_GUEST_NEXT(80621E00)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x80621E00,PcSectionInit,"PcOriginalUI_SectionInit");
extern "C" void PcButtonLoad(CpuContext* ctx){const auto button=ctx->gpr[3];Resource resource{};for(unsigned i=0;i<6;++i)resource.args[i]=ctx->gpr[4+i];MKW_GUEST_NEXT(805BD518)(ctx);resources[button]=resource;}
REGISTER_NATIVE_WRAPPER_AS(0x805BD518,PcButtonLoad,"PcOriginalUI_ResourceCapture");
extern "C" void PcPageInit(CpuContext* ctx){const auto page=ctx->gpr[3],id=ctx->gpr[4];
    if(id==0xC0)Memory::Write32(page,0x808BA3ECu);
    if(id==0x73&&creatingPc)Memory::Write32(page,0x808DA3C8u);
    MKW_GUEST_NEXT(80601A60)(ctx);
    // The game locks its scene heaps during frame updates. Allocate native UI
    // memory while the section is loading, just like its existing pages do.
    if(!textScratch)textScratch=Alloc(ctx,4096);
    if(id==0xC0)AddButton(ctx,page,page+0x404,page+0x8AC,3,false,MKW_GADDR(805FD90C));
    if(id==0xC0||(id>=0x17&&id<=0x1A)){
        const auto section=Section();if(!Memory::Read32(section+8+0x73*4)){
            creatingPc=true;try{Invoke(ctx,MKW_GADDR(80622D08),{section,0x73});}catch(...){creatingPc=false;throw;}creatingPc=false;
        }
    }
}
REGISTER_NATIVE_WRAPPER_AS(0x80601A60,PcPageInit,"PcOriginalUI_PageInit");
extern "C" void PcStockOptionsInit(CpuContext* ctx){MKW_GUEST_FUNC(805FD6E4)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x805FD6E4,PcStockOptionsInit,"PcOriginalUI_StockOptionsInit");
extern "C" void PcPauseInit(CpuContext* ctx){const auto page=ctx->gpr[3];MKW_GUEST_NEXT(80858D60)(ctx);const auto id=Memory::Read32(page+4);if(id>=0x17&&id<=0x1A){const auto buttons=Memory::Read32(page+0x48),count=Memory::Read32(page+0x54);if(buttons&&count&&count<7)AddButton(ctx,page,buttons,buttons+(count-1)*0x254,count,true,MKW_GADDR(8085A0F4));}}
REGISTER_NATIVE_WRAPPER_AS(0x80858D60,PcPauseInit,"PcOriginalUI_PauseInit");
extern "C" void PcPauseClick(CpuContext* ctx){const auto p=ctx->gpr[3];if(extraButtons.contains(p)&&extraButtons.at(p).button==ctx->gpr[4])Open(ctx,p);else MKW_GUEST_NEXT(8085A0F4)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x8085A0F4,PcPauseClick,"PcOriginalUI_PauseClick");
extern "C" void PcOptionsClick(CpuContext* ctx){const auto p=ctx->gpr[3];if(extraButtons.contains(p)&&extraButtons.at(p).button==ctx->gpr[4])Open(ctx,p);else MKW_GUEST_NEXT(805FD90C)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x805FD90C,PcOptionsClick,"PcOriginalUI_OptionsClick");
extern "C" void PcSettingsActivate(CpuContext* ctx){const auto page=ctx->gpr[3];if(page!=activePage){MKW_GUEST_NEXT(8085354C)(ctx);return;}Memory::Write32(page+0x3E4,Label(1,PcMenuLocale::Display()));Invoke(ctx,MKW_GADDR(80837170),{page});Configure(ctx,page);Invoke(ctx,MKW_GADDR(80605D74),{page+0x6C4,0});}
REGISTER_NATIVE_WRAPPER_AS(0x8085354C,PcSettingsActivate,"PcOriginalUI_GraphicsActivate");
extern "C" void PcSettingsButton(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853654)(ctx);return;}if(ctx->gpr[4]==activePage+0x54){Close(ctx,activePage);return;}sheet=(sheet+1)%3;Configure(ctx,activePage);Invoke(ctx,MKW_GADDR(80602378),{activePage});}
REGISTER_NATIVE_WRAPPER_AS(0x80853654,PcSettingsButton,"PcOriginalUI_GraphicsSheet");
extern "C" void PcSettingsBack(CpuContext* ctx){if(ctx->gpr[3]==activePage)Close(ctx,activePage);else MKW_GUEST_NEXT(808536B4)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x808536B4,PcSettingsBack,"PcOriginalUI_GraphicsBack");
extern "C" void PcRadioClick(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853714)(ctx);return;}auto row=Memory::Read32(ctx->gpr[4]+0x20C);if(row<5)settings_overlay::SetPcGraphicsValue(sheet,row,ctx->gpr[6]);}
REGISTER_NATIVE_WRAPPER_AS(0x80853714,PcRadioClick,"PcOriginalUI_GraphicsChoice");
extern "C" void PcRadioSelect(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(8085389C)(ctx);return;}Bottom(ctx,T(u"Izquierda/derecha y A: elegir. B: volver.",u"Left/right and A: select. B: back.",u"Gauche/droite et A : choisir. B : retour.",u"Links/rechts und A: wählen. B: zurück.",u"Sinistra/destra e A: scegli. B: indietro."));}
REGISTER_NATIVE_WRAPPER_AS(0x8085389C,PcRadioSelect,"PcOriginalUI_GraphicsHint");
extern "C" void PcScrollChange(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853998)(ctx);return;}settings_overlay::SetPcGraphicsScroller(sheet,ctx->gpr[6]);if(sheet==1)Configure(ctx,activePage);}
REGISTER_NATIVE_WRAPPER_AS(0x80853998,PcScrollChange,"PcOriginalUI_GraphicsScroll");
extern "C" void PcTextChange(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853A10)(ctx);return;}const auto& values=sheet==2?PcMenuLocale::Languages:sheet==1?presets:scales;auto value=std::min<unsigned>(ctx->gpr[5],values.size()-1);Text(ctx,ctx->gpr[4],51,values[value]);}
REGISTER_NATIVE_WRAPPER_AS(0x80853A10,PcTextChange,"PcOriginalUI_GraphicsValueText");
extern "C" void PcExternalSelect(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853684)(ctx);return;}Bottom(ctx,T(u"Ver la siguiente pagina de opciones avanzadas.",u"Open the next page of advanced options.",u"Ouvrir la page suivante des options avancées.",u"Nächste Seite der erweiterten Optionen öffnen.",u"Apri la pagina successiva delle opzioni avanzate."));}
REGISTER_NATIVE_WRAPPER_AS(0x80853684,PcExternalSelect,"PcOriginalUI_GraphicsNextHint");
extern "C" void PcPageUpdate(CpuContext* ctx){const auto page=ctx->gpr[3];MKW_GUEST_NEXT(80602338)(ctx);Position(page);
    if(languageRefresh&&textScratch){languageRefresh=false;RefreshLanguage(ctx);}
    const auto pageId=Memory::Read32(page+4);if(Memory::Read32(page+8)==4&&!stockLabels.contains(page)){
        if(pageId==0x65){Text(ctx,page+0x2F4,100,T(u"Elige un carné",u"Select a License",u"Choisissez un permis",u"Lizenz auswählen",u"Scegli una patente"));Text(ctx,page+0xDB8,101,PcMenuLocale::Options());stockLabels.insert(page);}
        else if(pageId==0xC0){Text(ctx,page+0x290,102,PcMenuLocale::Options());Text(ctx,page+0x8AC,103,T(u"Canal Mario Kart",u"Mario Kart Channel",u"Chaîne Mario Kart",u"Mario Kart-Kanal",u"Canale Mario Kart"));stockLabels.insert(page);}
    }
    if(std::getenv("MKW_PC_UI_TEST_BRIDGE")){static uint32_t lastTop=0,lastSection=0;const auto section=Section(),layers=Memory::Read32(section+0x37C);if(layers&&layers<=11){const auto top=Memory::Read32(section+0x354+(layers-1)*4);if(top!=lastTop||section!=lastSection){lastTop=top;lastSection=section;RT_LOGF("pc-ui-test","Section %u, top page %u at %08X\n",Memory::Read32(section),Memory::Read32(top+4),top);}}}
    if(activePage&&closingPc&&Memory::Read32(activePage+8)<=1){for(unsigned i=0;i<5;++i){auto radio=activePage+0x6C4+i*0x298;Memory::Write8(radio+0x80,0);Memory::Write8(radio+0x285,0);Memory::Write32(radio+0x200,originalRadioCounts[i]);auto buttons=Memory::Read32(radio+0x294);for(unsigned j=0;j<originalRadioCounts[i];++j)Memory::Write8(buttons+j*0x17C+0x80,0);}activePage=0;closingPc=false;if(backgroundAdded){backgroundAdded=false;Invoke(ctx,MKW_GADDR(80623288),{Section()});}for(auto [control,hidden]:hiddenOwner)Memory::Write8(control+0x80,hidden);hiddenOwner.clear();if(ownerPage)Invoke(ctx,MKW_GADDR(80602378),{ownerPage});}
    if(Memory::Read32(page+4)==0x8B&&Memory::Read32(page+8)==4){const auto status=PcOnlineRooms::Get();if(status.revision!=onlineRevision){Text(ctx,page+0xBC,90,PcOnlineRooms::Summary());onlineRevision=status.revision;}}
}
REGISTER_NATIVE_WRAPPER_AS(0x80602338,PcPageUpdate,"PcOriginalUI_PageUpdate");
extern "C" void PcBmg(CpuContext* ctx){const auto found=messages.find(ctx->gpr[5]);if(found==messages.end()){if(auto message=PcLiveLocale::Find(ctx->gpr[5])){Memory::Write32(ctx->gpr[6],message->address);Memory::Write32(ctx->gpr[7],message->font);ctx->gpr[3]=1;return;}MKW_GUEST_NEXT(805CEF10)(ctx);return;}
    if(!textScratch)textScratch=Alloc(ctx,4096);if(!textScratch)throw std::runtime_error("Text allocation failed");
    const auto count=std::min<size_t>(found->second.size(),1000);for(size_t i=0;i<count;++i)Memory::Write16(textScratch+i*2,found->second[i]);Memory::Write16(textScratch+count*2,0);
    Memory::Write32(ctx->gpr[6],textScratch);Memory::Write32(ctx->gpr[7],1);ctx->gpr[3]=1;
}
REGISTER_NATIVE_WRAPPER_AS(0x805CEF10,PcBmg,"PcOriginalUI_LocalText");
// PaneHandler::SetMessage also inlines GetBMGInfo, so serve the same local
// messages through the three underlying BMG methods used by that path.
extern "C" void PcBmgId(CpuContext* ctx){if(messages.contains(ctx->gpr[4]))ctx->gpr[3]=ctx->gpr[4];else if(auto message=PcLiveLocale::Find(ctx->gpr[4]))ctx->gpr[3]=message->index;else MKW_GUEST_NEXT(805F8C88)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x805F8C88,PcBmgId,"PcOriginalUI_BmgId");
extern "C" void PcBmgString(CpuContext* ctx){const auto found=messages.find(ctx->gpr[4]);if(found==messages.end()){if(auto message=PcLiveLocale::FromIndex(ctx->gpr[4])){ctx->gpr[3]=message->address;return;}MKW_GUEST_NEXT(805F8CF0)(ctx);return;}if(!textScratch)textScratch=Alloc(ctx,4096);if(!textScratch)throw std::runtime_error("Text allocation failed");const auto n=std::min<size_t>(found->second.size(),1000);for(size_t i=0;i<n;++i)Memory::Write16(textScratch+i*2,found->second[i]);Memory::Write16(textScratch+n*2,0);ctx->gpr[3]=textScratch;}
REGISTER_NATIVE_WRAPPER_AS(0x805F8CF0,PcBmgString,"PcOriginalUI_BmgString");
extern "C" void PcBmgFont(CpuContext* ctx){if(!messages.contains(ctx->gpr[4])){if(auto message=PcLiveLocale::FromIndex(ctx->gpr[4])){ctx->gpr[3]=message->fontAddress;return;}MKW_GUEST_NEXT(805F8D2C)(ctx);return;}if(!textScratch)textScratch=Alloc(ctx,4096);if(!textScratch)throw std::runtime_error("Font allocation failed");Memory::Write8(textScratch+2044,1);ctx->gpr[3]=textScratch+2044;}
REGISTER_NATIVE_WRAPPER_AS(0x805F8D2C,PcBmgFont,"PcOriginalUI_BmgFont");
extern "C" void PcRememberMessage(CpuContext* ctx){Record(ctx,false);MKW_GUEST_NEXT(8063DDB4)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x8063DDB4,PcRememberMessage,"PcOriginalUI_RecordMessage");
extern "C" void PcRememberPaneMessage(CpuContext* ctx){Record(ctx,true);MKW_GUEST_NEXT(8063DCBC)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x8063DCBC,PcRememberPaneMessage,"PcOriginalUI_RecordPaneMessage");
extern "C" void PcOnlineActivate(CpuContext* ctx){MKW_GUEST_NEXT(8064BC90)(ctx);PcOnlineRooms::Refresh();onlineRevision=~uint64_t{0};}
REGISTER_NATIVE_WRAPPER_AS(0x8064BC90,PcOnlineActivate,"PcOriginalUI_OnlineRooms");
