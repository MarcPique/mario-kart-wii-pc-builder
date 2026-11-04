// PC graphics run through the original VSSettings controls, fonts and animations.
#include "abi_bridge.h"
#include "guest_function_imports.h"
#include "pc_menu_hooks.h"
#include "pc_graphics_settings.h"
#include "pc_online_rooms.h"
#include "pc_match_filter.h"
#include "pc_room_order.h"
#include "pc_menu_locale.h"
#include "pc_live_locale.h"
#include "pc_texture_packs.h"
#include "pc_aspect_ratio.h"
#include "aurora_events.h"
#include <cstring>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <charconv>
extern "C" bool DVDMyStuffEntryVisible(int32_t index);
extern "C" unsigned DVDMyStuffMode();
extern "C" bool DVDMyStuffAvailable();
extern "C" bool DVDMyStuffSwitch(unsigned mode);
extern "C" bool DVDMyStuffRequest(unsigned mode);
extern "C" bool DVDMyStuffBusy();
extern "C" void DVDMyStuffCancel();
extern "C" int DVDMyStuffPump(bool mayApply);
extern "C" bool DVDMyStuffOpenFolder();
extern "C" bool NANDRetroSaveSwitch(bool separate);
extern "C" bool NANDRetroSaveSeparate();
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
unsigned selectedController=0,selectedMyStuff=0;
bool myStuffFailed=false,selectedSeparateSave=false,saveFailed=false,saveApplyPending=false,exitSwitchSave=false;
unsigned selectedRoom=0;
std::vector<std::string> roomIds;
std::vector<std::u16string> roomValues;
std::string pendingRoomId;
std::string pendingRoomKey;
std::string probeChosenRoomId;
std::vector<uint32_t> probeKnownPeers;
size_t probeMemberOffset=0;
bool roomSearchStarted=false;
bool probeJoinedTarget=false;
unsigned roomRouteStage=0,roomRouteFrames=0;
bool roomPageRequested=false,roomPageActive=false;
uint32_t roomPage=0;
size_t roomPageOffset=0;
size_t roomPageCapacity=0;
bool roomPageHasNext=false;
uint64_t roomListRevision=0;
std::u16string roomNotice;
bool roomNoticeRefreshing=false;
uint32_t routeNoticePage=0;
unsigned routeNoticeFrames=0;
std::array<uint32_t,5> roomOriginalHandler{};
struct RoomButtonState {uint32_t address;uint8_t hidden,inaccessible;};
std::vector<RoomButtonState> roomButtonStates;
std::vector<std::u16string> visibleControllers;
std::vector<uint32_t> visibleControllerIds;
std::string visibleUpdateState,visibleTextureState;
bool creatingPc=false,closingPc=false;
uint32_t exitPrompt=0,exitOwner=0,exitBackHandler=0;bool exitClosing=false;
std::vector<std::pair<uint32_t,uint8_t>> exitHidden;
uint32_t exitOriginalBack=0;uint8_t exitOriginalRepeat=0,exitOriginalPointer=0;
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
names0={PcMenuLocale::Display(),T(u"Formato rapido",u"Quick ratio",u"Format rapide",u"Schnellformat",u"Formato rapido"),u"V-Sync",T(u"Mostrar FPS",u"Show FPS",u"Afficher les FPS",u"FPS anzeigen",u"Mostra FPS"),u"Bloom"};
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
std::u16string ControllersTitle(){return T(u"Mandos",u"Controllers",u"Manettes",u"Controller",u"Controller");}
std::vector<std::u16string> ControllerChoices(const std::vector<std::u16string>& devices){
    auto result=devices;
    // The original scroller has a small glyph buffer. Show the device family
    // here and its complete SDL model in the wide native detail panel.
    for(size_t i=1;i<result.size();++i){const auto& name=devices[i];
        if(name.find(u"DualSense")!=std::u16string::npos)result[i]=u"DualSense";
        else if(name.find(u"DualShock")!=std::u16string::npos||name.find(u"PS4")!=std::u16string::npos)result[i]=u"DualShock";
        else if(name.find(u"Joy-Con")!=std::u16string::npos)result[i]=u"Joy-Con";
        else if(name.find(u"Switch Pro")!=std::u16string::npos||name.find(u"Nintendo Switch Pro")!=std::u16string::npos)result[i]=u"Switch Pro";
        else if(name.find(u"Switch")!=std::u16string::npos)result[i]=u"Switch";
        else if(name.find(u"Xbox")!=std::u16string::npos||name.find(u"XInput")!=std::u16string::npos)result[i]=u"Xbox";
    }
    const auto families=result;
    for(size_t i=1;i<result.size();++i)if(std::count(families.begin()+1,families.end(),families[i])>1){const auto number=std::to_string(std::count(families.begin()+1,families.begin()+i,families[i])+1);result[i]+=u" ";result[i].append(number.begin(),number.end());}
    return result;
}
std::u16string ExitTitle(){return T(u"Salir al escritorio",u"Exit to desktop",u"Quitter le jeu",u"Zum Desktop",u"Esci sul desktop");}
std::u16string SaveTitle(){return T(u"Partidas",u"Savegame",u"Sauvegarde",u"Spielstand",u"Salvataggio");}
std::u16string SaveSeparateLabel(){return T(u"Partida separada",u"Separate savegame",u"Sauvegarde séparée",u"Separater Spielstand",u"Salvataggio separato");}
std::vector<std::u16string> SaveChoices(){return {NANDRetroSaveSeparate()?u"RetroWFC2":u"RetroWFC"};}
std::u16string SaveDetail(){
    if(saveFailed||!ownerPage||Memory::Read32(ownerPage+4)!=0xC0)return T(u"Partida ocupada o no disponible. Prueba desde Opciones.",u"Savegame busy or unavailable. Try again from Options.",u"Sauvegarde occupée ou indisponible. Réessayez depuis les options.",u"Spielstand belegt oder nicht verfügbar. In den Optionen erneut versuchen.",u"Salvataggio occupato o non disponibile. Riprova dalle opzioni.");
    return T(u"Cambiar de partida vuelve a la pantalla de titulo.",u"Changing savegame returns to the title screen.",u"Changer de sauvegarde ramène à l'écran titre.",u"Ein Spielstandwechsel führt zum Titelbildschirm zurück.",u"Cambiare salvataggio riporta alla schermata del titolo.");
}
std::u16string MyStuffTitle(){return u"My Stuff";}
std::vector<std::u16string> MyStuffChoices(){
    const auto music=T(u"Musica",u"Music",u"Musique",u"Musik",u"Musica");
    // Keep the music-only choice inside the original scroller's glyph area.
    return {T(u"No",u"Off",u"Non",u"Aus",u"No"),u"CTGP",u"Retro Rewind",u"CTGP - "+music,u"RR - "+music};
}
std::u16string MyStuffDetail(){
    if(DVDMyStuffBusy())return T(u"Preparando My Stuff en segundo plano...",u"Preparing My Stuff in the background...",u"Préparation de My Stuff en arrière-plan...",u"My Stuff wird im Hintergrund vorbereitet...",u"Preparazione di My Stuff in background...");
    if(ownerPage&&Memory::Read32(ownerPage+4)!=0xC0)return T(u"Vuelve a Opciones para cambiar My Stuff.",u"Return to Options to change My Stuff.",u"Retournez aux options pour changer My Stuff.",u"Zum Ändern von My Stuff zu den Optionen zurückkehren.",u"Torna alle opzioni per cambiare My Stuff.");
    if(myStuffFailed||!DVDMyStuffAvailable())return T(u"No se pudo aplicar My Stuff. Revisa los archivos y el registro.",u"Could not apply My Stuff. Check the files and log.",u"My Stuff non appliqué. Vérifiez les fichiers et le journal.",u"My Stuff nicht angewendet. Dateien und Protokoll prüfen.",u"My Stuff non applicato. Controlla file e registro.");
    return T(u"My Stuff se aplica al cargar los siguientes archivos.",u"My Stuff changes apply when files are next loaded.",u"My Stuff s'applique au prochain chargement des fichiers.",u"My Stuff gilt beim nächsten Laden der Dateien.",u"My Stuff si applica al prossimo caricamento dei file.");
}
std::u16string TextureLabel(){return T(u"Texturas",u"Textures",u"Textures",u"Texturen",u"Texture");}
std::u16string TextureTitle(){return T(u"Paquetes de texturas",u"Texture Packs",u"Packs de textures",u"Texturpakete",u"Pacchetti texture");}
std::u16string AspectTitle(){return T(u"Formato",u"Aspect Ratio",u"Format",u"Bildformat",u"Formato");}
std::u16string InternalTitle(){return T(u"Resolucion interna",u"Internal Resolution",u"Résolution interne",u"Interne Auflösung",u"Risoluzione interna");}
std::u16string TextureDetail(){if(PcTexturePacks::Busy())return T(u"Importando ZIP/7z en segundo plano...",u"Importing ZIP/7z in the background...",u"Importation ZIP/7z en arrière-plan...",u"ZIP/7z wird im Hintergrund importiert...",u"Importazione ZIP/7z in background...");if(PcTexturePacks::Failed())return T(u"No se pudo importar un paquete. Se conservan las texturas originales.",u"A pack could not be imported. Original textures retained.",u"Échec d'importation d'un pack. Textures originales conservées.",u"Ein Paket konnte nicht importiert werden. Originaltexturen bleiben erhalten.",u"Impossibile importare un pacchetto. Texture originali mantenute.");return T(u"Si falta una textura o falla, se usa la original. PNG/DDS de Dolphin.",u"Missing or invalid textures use the originals. Dolphin PNG/DDS.",u"Textures absentes ou invalides : textures originales. PNG/DDS Dolphin.",u"Fehlende oder ungültige Texturen verwenden das Original. Dolphin PNG/DDS.",u"Texture mancanti o non valide: originali. PNG/DDS Dolphin.")+u" ZIP/7z";}
std::string TextureState(){return std::to_string(PcTexturePacks::Count())+"/"+std::to_string(PcTexturePacks::Busy())+"/"+std::to_string(PcTexturePacks::Failed());}
std::vector<std::u16string> AspectChoices(){std::vector<std::u16string> result;for(const auto& ratio:PcAspectRatio::ratios)result.emplace_back(ratio.name);result[0]=T(u"Automatico",u"Automatic",u"Automatique",u"Automatisch",u"Automatico");return result;}
std::vector<std::u16string> TextureChoices(){const auto number=std::to_string(PcTexturePacks::Count());std::u16string count(number.begin(),number.end());return {count+u" PNG/DDS"};}
void CloseExit(CpuContext* ctx){if(exitPrompt&&!exitClosing){exitClosing=true;Invoke(ctx,MKW_GADDR(8060247C),{exitPrompt});}}
void OpenExit(CpuContext* ctx,uint32_t owner,bool switchSave=false){
    const auto prompt=Memory::Read32(Section()+8+0x53*4);if(!prompt)return;
    exitPrompt=prompt;exitOwner=owner;exitClosing=false;exitSwitchSave=switchSave;
    // This native page can also serve the original game's dialogs. Restore its
    // global B binding after our layer closes, including the binding flags.
    exitOriginalBack=Memory::Read32(prompt+0x6C);
    exitOriginalRepeat=Memory::Read8(prompt+0x8D);exitOriginalPointer=Memory::Read8(prompt+0x96);
    Invoke(ctx,MKW_GADDR(806530D0),{prompt});
    Invoke(ctx,MKW_GADDR(8065313C),{prompt,Label(170,switchSave?SaveTitle():ExitTitle()),0});
    Invoke(ctx,MKW_GADDR(806525FC),{prompt,Label(171,switchSave?SaveDetail():T(u"¿Seguro que quieres salir al escritorio?",u"Are you sure you want to exit to the desktop?",u"Voulez-vous vraiment quitter le jeu ?",u"Möchtest du das Spiel wirklich beenden?",u"Vuoi davvero uscire sul desktop?")),0});
    Invoke(ctx,MKW_GADDR(80652604),{prompt,0,Label(172,switchSave?T(u"Aplicar",u"Apply",u"Appliquer",u"Anwenden",u"Applica"):T(u"Salir",u"Exit",u"Quitter",u"Beenden",u"Esci")),0,0,0});
    Invoke(ctx,MKW_GADDR(80652604),{prompt,1,Label(173,T(u"Cancelar",u"Cancel",u"Annuler",u"Abbrechen",u"Annulla")),0,0,0});
    Memory::Write32(prompt+0x27C,1); // Safe default: Cancel, including controller navigation.
    Invoke(ctx,MKW_GADDR(805F0D84),{prompt+0x4C,1,exitBackHandler,0,0});
    exitHidden.clear();const auto array=Memory::Read32(owner+0x24),count=Memory::Read32(owner+0x34);
    for(unsigned i=0;i<count;++i){const auto control=Memory::Read32(array+i*4);if(control){exitHidden.push_back({control,Memory::Read8(control+0x80)});Memory::Write8(control+0x80,1);}}
    Invoke(ctx,MKW_GADDR(80622E00),{Section(),0x53,0});
    if(switchSave)RT_LOGF("pc-save","Native confirmation opened; Cancel selected by default\n");
    else RT_LOGF("pc-exit","Native confirmation opened; Cancel selected by default\n");
}
std::u16string UpdatesTitle(){return T(u"Actualizaciones",u"Updates",u"Mises à jour",u"Updates",u"Aggiornamenti");}
std::u16string RoomsTitle(){return T(u"Salas publicas",u"Public rooms",u"Salles publiques",u"Öffentliche Räume",u"Stanze pubbliche");}
void RefreshRoomChoices(){
    const auto snapshot=PcOnlineRooms::Get();roomListRevision=snapshot.revision;
    std::vector<std::string> available;
    if(snapshot.available)for(const auto& room:snapshot.rooms){
        if(!room.publicJoinable || !PcOnlineRooms::IsRetroRoomMode(room.roomKey) || room.memberPids.empty())continue;
        available.push_back(room.id);
    }
    roomIds=PcOnlineRooms::StableRoomOrder(roomPageActive?roomIds:std::vector<std::string>{},available);
    roomValues.clear();
    for(size_t index=0;index<roomIds.size();++index){
        const auto match=std::find_if(snapshot.rooms.begin(),snapshot.rooms.end(),[&](const auto& room){return snapshot.available&&room.id==roomIds[index]&&room.publicJoinable&&!room.memberPids.empty()&&PcOnlineRooms::IsRetroRoomMode(room.roomKey);});
        if(match==snapshot.rooms.end()){const auto text=std::string("-- ")+std::to_string(index+1);roomValues.emplace_back(text.begin(),text.end());continue;}
        const auto& room=*match;
        const auto& key=room.roomKey;
        const auto label=key=="vs_10"?"Retro":key=="vs_11"?"OTT":key=="vs_12"?"200cc":key=="vs_13"?"Items":key=="vs_14"?"Battle":key=="vs_15"?"Elim":key=="vs_20"?"CT":key=="vs_21"?"Normal":"CT200";
        const auto count=std::to_string(room.players)+"/12";
        const auto text=std::string(label)+std::to_string(index+1)+" "+count;
        roomValues.emplace_back(text.begin(),text.end());
    }
    if(roomValues.empty())roomValues.push_back(T(u"Sin salas disponibles",u"No rooms available",u"Aucune salle disponible",u"Keine Räume verfügbar",u"Nessuna stanza disponibile"));
    selectedRoom=std::min<unsigned>(selectedRoom,roomValues.size()-1);
}
void RenderRoomPage(CpuContext* ctx){
    const auto countButtons=roomButtonStates.size();if(!countButtons)return;
    const auto testLimit=std::getenv("MKW_PC_ROOM_PAGE_TEST")?std::min<size_t>(4,countButtons-1):countButtons-1;
    roomPageHasNext=countButtons>1&&roomIds.size()>(std::getenv("MKW_PC_ROOM_PAGE_TEST")?testLimit:countButtons);
    roomPageCapacity=roomPageHasNext?testLimit:countButtons;
    if(roomPageOffset>=roomIds.size())roomPageOffset=0;
    for(unsigned i=0;i<countButtons;++i){const auto button=roomButtonStates[i].address;const bool next=roomPageHasNext&&i==roomPageCapacity;const auto index=roomPageOffset+i;const bool hidden=!next&&(i>=roomPageCapacity||index>=roomIds.size());
        Memory::Write8(button+0x80,hidden);Memory::Write8(button+0x1E9,hidden);
        if(next)Text(ctx,button,120+i,T(u"Mas salas",u"More rooms",u"Plus de salles",u"Mehr Räume",u"Altre stanze"));
        else if(!hidden)Text(ctx,button,120+i,roomValues[index]);
    }
}
bool ShowRoomPage(CpuContext* ctx,uint32_t page){
    const auto count=Memory::Read32(page+0x3D8);
    if(count==0||count>16)return false;
    std::vector<RoomButtonState> controls;
    for(unsigned i=0;i<count;++i){
        const auto button=page+0x6C4+i*0x254;
        if(!resources.contains(button)||Memory::Read32(button+0x234)!=page+0x658)return false;
        controls.push_back({button,Memory::Read8(button+0x80),Memory::Read8(button+0x1E9)});
    }
    for(unsigned i=0;i<5;++i)roomOriginalHandler[i]=Memory::Read32(page+0x658+i*4);
    roomButtonStates=std::move(controls);roomPage=page;roomPageActive=true;
    Memory::Write32(page+0x668,MKW_GADDR(8064BCE8));
    roomIds.clear();RefreshRoomChoices();roomPageOffset=0;roomNotice.clear();roomNoticeRefreshing=false;RenderRoomPage(ctx);
    Text(ctx,Memory::Read32(page+0x2B8),150,RoomsTitle());
    Text(ctx,Memory::Read32(page+0x2BC),151,T(u"Elige una sala publica. B: volver.",u"Choose a public room. B: back.",u"Choisissez une salle. B : retour.",u"Raum wählen. B: zurück.",u"Scegli una stanza. B: indietro."));
    RT_LOGF("pc-ui-test","Native room page configured with %zu entries\n",std::min(roomIds.size(),roomButtonStates.size()));
    return true;
}
void NextRoomPage(CpuContext* ctx){
    if(!roomPageHasNext||!roomPageCapacity)return;
    roomPageOffset=(roomPageOffset+roomPageCapacity>=roomIds.size())?0:roomPageOffset+roomPageCapacity;
    roomNotice.clear();roomNoticeRefreshing=false;RenderRoomPage(ctx);
    RT_LOGF("pc-ui-test","Room list moved to offset %zu\n",roomPageOffset);
}
void RestoreRoomPage(){
    if(!roomPageActive)return;
    for(unsigned i=0;i<5;++i)Memory::Write32(roomPage+0x658+i*4,roomOriginalHandler[i]);
    for(const auto& control:roomButtonStates){Memory::Write8(control.address+0x80,control.hidden);Memory::Write8(control.address+0x1E9,control.inaccessible);}
    roomButtonStates.clear();roomPage=0;roomPageActive=false;roomPageRequested=false;
}
std::string UpdateStateKey(){const auto state=PcOnlineRooms::GetUpdates();return std::to_string(state.checked)+std::to_string(state.available)+std::to_string(state.newer)+state.installed+"/"+state.latest;}
std::u16string UpdateDetail(){const auto state=PcOnlineRooms::GetUpdates();if(!state.checked)return T(u"Buscando actualizaciones...",u"Checking for updates...",u"Recherche de mises à jour...",u"Suche nach Updates...",u"Ricerca aggiornamenti...");if(!state.available)return T(u"No se pudo comprobar la version.",u"Unable to check version.",u"Vérification impossible.",u"Prüfung nicht möglich.",u"Impossibile verificare.");if(state.newer)return T(u"Nueva version: necesita recompilar el juego.",u"New version: the game must be rebuilt.",u"Nouvelle version : recompilation nécessaire.",u"Neue Version: Neuübersetzung nötig.",u"Nuova versione: ricompilazione necessaria.");return T(u"Retro Rewind esta actualizado.",u"Retro Rewind is up to date.",u"Retro Rewind est à jour.",u"Retro Rewind ist aktuell.",u"Retro Rewind è aggiornato.");}
void Configure(CpuContext* ctx,uint32_t page) {
    RefreshMenuTranslations();
    const auto title=sheet==9?SaveTitle():sheet==8?MyStuffTitle():sheet==6?InternalTitle():sheet==7?TextureTitle():sheet==5?RoomsTitle():sheet==4?UpdatesTitle():sheet==3?ControllersTitle():sheet==2?PcMenuLocale::Language():sheet==1?PcMenuLocale::Quality():PcMenuLocale::Display();
    Memory::Write32(page+0x3E4,Label(1,title));Text(ctx,Memory::Read32(page+0x2B8),1,title);
    const auto devices=settings_overlay::PcControllerDevices();
    visibleControllers=devices;
    visibleControllerIds=settings_overlay::PcControllerDeviceIds();
    selectedController=std::min<unsigned>(selectedController,devices.size()-1);
    auto controllerChoices=choices2;
    auto controllerNames=names2;
    for(unsigned i=0;i<4;++i){controllerNames[i]=T(u"Jugador ",u"Player ",u"Joueur ",u"Spieler ",u"Giocatore ");controllerNames[i].push_back(static_cast<char16_t>(u'1'+i));controllerChoices[i]={T(u"Vaciar",u"Clear",u"Effacer",u"Leeren",u"Cancella"),T(u"Asignar",u"Assign",u"Attribuer",u"Zuordnen",u"Assegna")};}
    auto updateChoices=choices2;auto updateNames=names2;
    updateNames[0]=T(u"Comprobacion",u"Version check",u"Vérification",u"Versionsprüfung",u"Verifica");
    updateChoices[0]={T(u"Revisar",u"Check",u"Vérifier",u"Prüfen",u"Verifica")};
    auto roomChoices=updateChoices;auto roomNames=updateNames;
    roomNames[0]=T(u"Sala",u"Room",u"Salle",u"Raum",u"Stanza");roomChoices[0]={T(u"Elegir",u"Select",u"Choisir",u"Wählen",u"Seleziona")};
    auto saveChoices=choices2;auto saveNames=names2;
    saveNames[0]=SaveSeparateLabel();saveChoices[0]={T(u"No",u"Off",u"Non",u"Aus",u"No"),T(u"Si",u"On",u"Oui",u"An",u"Sì")};
    saveNames[1]=T(u"Aplicar",u"Apply",u"Appliquer",u"Anwenden",u"Applica");saveChoices[1]={saveNames[1]};
    auto myStuffChoices=choices2;auto myStuffNames=names2;
    myStuffNames[0]=T(u"Aplicar",u"Apply",u"Appliquer",u"Anwenden",u"Applica");myStuffChoices[0]={myStuffNames[0]};
    myStuffNames[1]=T(u"Carpeta",u"Folder",u"Dossier",u"Ordner",u"Cartella");myStuffChoices[1]={T(u"Abrir",u"Open",u"Ouvrir",u"Öffnen",u"Apri")};
    auto featureChoices=choices2;auto featureNames=names2;
    featureNames[0]=TextureLabel();featureChoices[0]={T(u"No",u"Off",u"Non",u"Aus",u"No"),T(u"Si",u"On",u"Oui",u"An",u"Sì")};
    featureNames[1]=T(u"Recargar",u"Reload",u"Recharger",u"Neu laden",u"Ricarica");featureChoices[1]={featureNames[1]};
    featureNames[2]=T(u"Carpeta",u"Folder",u"Dossier",u"Ordner",u"Cartella");featureChoices[2]={T(u"Abrir",u"Open",u"Ouvrir",u"Öffnen",u"Apri")};
    choices0[1][1]=std::u16string(PcAspectRatio::ratios[settings_overlay::PcAspectIndex()>1?settings_overlay::PcAspectIndex():5].name);
    const auto& choices=sheet==9?saveChoices:sheet==8?myStuffChoices:sheet==7?featureChoices:sheet==5?roomChoices:sheet==4?updateChoices:sheet==3?controllerChoices:sheet==2?choices2:sheet==1?choices1:choices0;const auto& names=sheet==9?saveNames:sheet==8?myStuffNames:sheet==7?featureNames:sheet==5?roomNames:sheet==4?updateNames:sheet==3?controllerNames:sheet==2?names2:sheet==1?names1:names0;
    for(unsigned i=0;i<5;++i) {
        auto radio=page+0x6C4+i*0x298;const auto count=std::min<unsigned>(originalRadioCounts[i],choices[i].size());
        const bool hidden=sheet==6||(sheet==9&&i>1)||(sheet==8&&i>1)||(sheet==7&&i>2)||(sheet==2&&i>2)||(sheet==3&&i>3)||((sheet==4||sheet==5)&&i>0);
        Memory::Write8(radio+0x80,hidden);Memory::Write8(radio+0x285,hidden);
        Memory::Write32(radio+0x200,count);auto selected=std::min(sheet==9?(i==0?unsigned(selectedSeparateSave):0u):sheet==7?(i==0?unsigned(PcTexturePacks::Enabled()):0u):sheet>=4?0u:sheet==3?static_cast<unsigned>(settings_overlay::PcControllerSelected(i,selectedController)):settings_overlay::PcGraphicsValue(sheet,i),count?count-1:0);
        Memory::Write32(radio+0x204,selected);Memory::Write32(radio+0x208,selected);
        Text(ctx,radio,10+i,names[i]);const auto options=Memory::Read32(radio+0x294);
        for(unsigned j=0;j<originalRadioCounts[i];++j){auto button=options+j*0x17C;Memory::Write8(button+0x80,j>=count);Invoke(ctx,MKW_GADDR(80606C48),{button,j==selected});if(j<count)Text(ctx,button,20+i*4+j,choices[i][j]);}
        RT_LOGF("pc-options","Native row %u supports %u choices\n",i,originalRadioCounts[i]);
    }
    const std::vector<std::u16string> updateValues{PcOnlineRooms::UpdateSummary()};
    const auto controllerValues=ControllerChoices(devices);
    const auto aspectValues=AspectChoices(),textureValues=TextureChoices();
    const auto scroller=page+0x13BC;const auto& values=sheet==9?SaveChoices():sheet==8?MyStuffChoices():sheet==0?aspectValues:sheet==6?scales:sheet==7?textureValues:sheet==5?roomValues:sheet==4?updateValues:sheet==3?controllerValues:sheet==2?PcMenuLocale::Languages():sheet==1?presets:scales;
    const auto position=sheet==9?0u:sheet==8?selectedMyStuff:sheet==0?settings_overlay::PcAspectIndex():sheet==6?settings_overlay::PcGraphicsScroller(0):sheet==7?0u:sheet==5?selectedRoom:sheet==4?0u:sheet==3?selectedController:settings_overlay::PcGraphicsScroller(sheet);
    Invoke(ctx,MKW_GADDR(8063F0C0),{scroller,static_cast<uint32_t>(values.size()),position});
    Text(ctx,scroller,50,sheet==9?SaveTitle():sheet==8?MyStuffTitle():sheet==0?AspectTitle():sheet==6?InternalTitle():sheet==7?TextureLabel():sheet==5?RoomsTitle():sheet==4?T(u"Version",u"Version",u"Version",u"Version",u"Versione"):sheet==3?T(u"Dispositivo",u"Device",u"Appareil",u"Gerät",u"Dispositivo"):sheet==2?PcMenuLocale::Language():sheet==1?T(u"Calidad",u"Quality",u"Qualité",u"Qualität",u"Qualità"):T(u"Resolucion interna",u"Internal Resolution",u"Résolution interne",u"Interne Auflösung",u"Risoluzione interna"));
    const auto value=std::min<unsigned>(position,values.size()-1);
    Text(ctx,Memory::Read32(page+0x1984+0x464),51,values[value]);
    const auto next=sheet==6?TextureTitle():sheet==7?MyStuffTitle():sheet==8?SaveTitle():sheet==9?PcMenuLocale::Display():sheet==0?PcMenuLocale::Quality():sheet==1?PcMenuLocale::Language():sheet==2?ControllersTitle():sheet==3?UpdatesTitle():InternalTitle();
    const auto external=Memory::Read32(page+0x48);if(Memory::Read32(page+0x4C)){const auto control=Memory::Read32(external);if(sheet==5)Memory::Write8(control+0x80,1);Text(ctx,control,60,next);}
    if(const auto bottom=Memory::Read32(page+0x2BC);bottom){const auto ownerId=ownerPage?Memory::Read32(ownerPage+4):0;Memory::Write8(bottom+0x80,ownerId>=0x17&&ownerId<=0x1A);}
    if(sheet==4)visibleUpdateState=UpdateStateKey();
    if(sheet==7)visibleTextureState=TextureState();
    Bottom(ctx,sheet==9?SaveDetail():sheet==8?MyStuffDetail():sheet==0?T(u"Automatico llena la ventana. Los formatos fijos conservan las proporciones.",u"Automatic fills the window. Fixed ratios preserve proportions.",u"Automatique remplit la fenêtre. Les formats fixes gardent les proportions.",u"Automatisch füllt das Fenster. Feste Formate erhalten die Proportionen.",u"Automatico riempie la finestra. I formati fissi mantengono le proporzioni."):sheet==7?TextureDetail():sheet==5?T(u"Elige una sala publica. B: volver.",u"Choose a public room. B: back.",u"Choisissez une salle. B : retour.",u"Raum wählen. B: zurück.",u"Scegli una stanza. B: indietro."):sheet==4?UpdateDetail():sheet==3?T(u"Elige un dispositivo y asignalo a un jugador.",u"Choose a device and assign it to a player.",u"Choisissez un appareil et un joueur.",u"Gerät und Spieler auswählen.",u"Scegli dispositivo e giocatore."):sheet==2?T(u"El idioma cambia inmediatamente. Guardado automatico.",u"Language changes immediately. Saved automatically.",u"Changement de langue immédiat. Sauvegarde auto.",u"Sprache wird sofort geändert. Automatisch gespeichert.",u"La lingua cambia subito. Salvataggio automatico."):T(u"Guardado automatico. B: volver.",u"Saved automatically. B: back.",u"Sauvegarde auto. B : retour.",u"Automatisch gespeichert. B: zurück.",u"Salvataggio automatico. B: indietro."));
    if(sheet==3)Bottom(ctx,devices[selectedController]);
}
void Open(CpuContext* ctx,uint32_t owner,bool rooms=false) {
    if(activePage)return;ownerPage=owner;sheet=rooms?5:0;closingPc=false;selectedMyStuff=DVDMyStuffMode();myStuffFailed=false;selectedSeparateSave=NANDRetroSaveSeparate();saveFailed=false;if(rooms){selectedRoom=0;RefreshRoomChoices();PcOnlineRooms::Refresh();}
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
void Close(CpuContext* ctx,uint32_t page){if(page==activePage)DVDMyStuffCancel();closingPc=true;Memory::Write32(page+0x3E8,0xFFFFFFFFu);Invoke(ctx,MKW_GADDR(8060247C),{page});}
bool ApplySaveSwitch(CpuContext* ctx){
    const auto mgr=Memory::Read32(MKW_GADDR(809BD748)),sectionMgr=Memory::Read32(MKW_GADDR(809C1E38));
    if(!ownerPage||Memory::Read32(ownerPage+4)!=0xC0||!mgr||!Memory::Contains(mgr,0x25008)||Memory::Read8(mgr+0x25000))return false;
    const auto requester=Memory::Read32(sectionMgr+0x90),raw=Memory::Read32(mgr+0x14);
    if(!requester||!Memory::Contains(raw,0x28000)||Invoke(ctx,MKW_GADDR(806212FC),{requester}))return false;
    for(const auto address:{MKW_GADDR(80548104),MKW_GADDR(8054454C),MKW_GADDR(8054484C),MKW_GADDR(80620C5C),MKW_GADDR(80635A3C),MKW_GADDR(80635AC8)}){
        const auto* entry=TranslatedFunctionRegistry::FindByAddressPtr(address);if(!entry||!entry->rawCpuInvoker)return false;
    }
    const bool previous=NANDRetroSaveSeparate();
    if(!NANDRetroSaveSwitch(selectedSeparateSave))return false;
    if(!RuntimeConfigFile::WriteSetting("retro","separate_save",selectedSeparateSave?"true":"false")){NANDRetroSaveSwitch(previous);return false;}
    const auto reload=[&]{
        Invoke(ctx,MKW_GADDR(80548104),{raw});
        for(unsigned i=0;i<4;++i)Invoke(ctx,MKW_GADDR(8054454C),{mgr+0x38+i*0x93F0,0});
        Memory::Write8(mgr+0x24FF8,0);Memory::Write8(mgr+0x25001,0);Memory::Write8(mgr+0x25002,0);
        Invoke(ctx,MKW_GADDR(8054484C),{mgr});
    };
    reload();
    const auto error=Memory::Read32(mgr+0x25004);
    // 4 is the original no-save result; its normal first-run flow creates a
    // fresh bank. Corrupt/access/busy results must never overwrite either bank.
    const bool valid=Memory::Read8(mgr+0x25001)&&Memory::Read8(mgr+0x25002);
    if((error!=0&&error!=4)||(error==0&&!valid)){
        if(!NANDRetroSaveSwitch(previous))throw std::runtime_error("Unable to restore previous save root");
        reload();RuntimeConfigFile::WriteSetting("retro","separate_save",previous?"true":"false");
        RT_LOGF("pc-save","Target save rejected (I/O %u, valid %u); previous save restored\n",error,valid?1:0);return false;
    }
    RuntimeConfigFile::Mutable().separateRetroSave=selectedSeparateSave;
    const auto next=Invoke(ctx,MKW_GADDR(80620C5C),{requester});
    Close(ctx,activePage);
    Invoke(ctx,MKW_GADDR(80635A3C),{sectionMgr,next==0xFFFFFFFFu?0x3Fu:next,0});
    Invoke(ctx,MKW_GADDR(80635AC8),{sectionMgr,0,0});
    RT_LOGF("pc-save","Switched savegame and reloaded licenses without restarting process\n");return true;
}
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
    if(Memory::Read32(page+4)==0x5A){
        for(unsigned i=0;i<4;++i){const auto o=4+i*0x18;Memory::WriteFloat32(e.button+o,190.f);Memory::WriteFloat32(e.button+o+4,-125.f);Memory::WriteFloat32(e.button+o+12,0.48f);Memory::WriteFloat32(e.button+o+16,0.48f);}
        return;
    }
    if(Memory::Read32(page+4)==0x8B){
        // Place a copy of the original small settings control beside the
        // first online mode, clear of the room count.
        for(unsigned i=0;i<4;++i){const auto o=4+i*0x18;
            Memory::WriteFloat32(e.button+o,20.0f-Memory::ReadFloat32(e.source+o));
            Memory::WriteFloat32(e.button+o+4,Memory::ReadFloat32(e.source+o+4)-60.0f);
        }
        return;
    }
    if(e.pause&&e.count>=2&&Memory::Read32(page+8)==4){
        if(e.pauseLayout.empty())for(unsigned slot=0;slot<4;++slot)for(unsigned button=0;button<=e.count;++button){
            const auto p=(button==e.count?e.button:e.source+button*0x254)+4+slot*0x18;
            e.pauseLayout.push_back({Memory::ReadFloat32(p+4),Memory::ReadFloat32(p+12),Memory::ReadFloat32(p+16)});
        }
        for(unsigned slot=0;slot<4;++slot){const auto base=slot*(e.count+1);auto first=e.pauseLayout[base][0],last=e.pauseLayout[base+e.count-1][0];
            if(e.count==2){const auto extra=(last-first)*0.25f;first-=extra;last+=extra;}
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
    messages[kLabel+174]=ExitTitle();messages[kLabel+70]=PcMenuLocale::Advanced();messages[kLabel+100]=T(u"Elige un carné",u"Select a License",u"Choisissez un permis",u"Lizenz auswählen",u"Scegli una patente");messages[kLabel+101]=messages[kLabel+102]=PcMenuLocale::Options();messages[kLabel+103]=T(u"Canal Mario Kart",u"Mario Kart Channel",u"Chaîne Mario Kart",u"Mario Kart-Kanal",u"Canale Mario Kart");messages[kLabel+90]=PcOnlineRooms::Summary();
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
extern "C" void PcSectionInit(CpuContext* ctx){DVDMyStuffCancel();resources.clear();extraButtons.clear();exitPrompt=exitOwner=exitBackHandler=0;exitClosing=false;exitHidden.clear();messages.clear();stockLabels.clear();recordedText.clear();textOrder=0;hiddenOwner.clear();backgroundAdded=false;activePage=ownerPage=textScratch=0;roomPage=routeNoticePage=routeNoticeFrames=0;roomPageActive=roomPageRequested=false;roomButtonStates.clear();pendingRoomId.clear();pendingRoomKey.clear();probeChosenRoomId.clear();probeKnownPeers.clear();probeMemberOffset=roomRouteStage=roomRouteFrames=0;roomSearchStarted=probeJoinedTarget=false;closingPc=false;onlineRevision=~uint64_t{0};UpdateSystemLanguage();PcLiveLocale::SetRaceContext(Invoke(ctx,MKW_GADDR(80631588),{ctx->gpr[4]})==2);MKW_GUEST_NEXT(80621E00)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x80621E00,PcSectionInit,"PcOriginalUI_SectionInit");
extern "C" void PcButtonLoad(CpuContext* ctx){const auto button=ctx->gpr[3];Resource resource{};for(unsigned i=0;i<6;++i)resource.args[i]=ctx->gpr[4+i];MKW_GUEST_NEXT(805BD518)(ctx);resources[button]=resource;}
REGISTER_NATIVE_WRAPPER_AS(0x805BD518,PcButtonLoad,"PcOriginalUI_ResourceCapture");
extern "C" void PcPageInit(CpuContext* ctx){const auto page=ctx->gpr[3],id=ctx->gpr[4];
    if(id==0xC0)Memory::Write32(page,0x808BA3ECu);
    if(id==0x73&&creatingPc)Memory::Write32(page,0x808DA3C8u);
    MKW_GUEST_NEXT(80601A60)(ctx);
    if(id==0x8B && std::getenv("MKW_PC_UI_TEST_BRIDGE")) {
        RT_LOGF("pc-ui-test","Online page %08X controls %u\n",page,Memory::Read32(page+0x34));
        for(const auto& [control,resource]:resources)
            if(control>=page && control-page<0x4000)RT_LOGF("pc-ui-test","Online loaded button offset %04X\n",control-page);
    }
    if(id==0x8C && std::getenv("MKW_PC_UI_TEST_BRIDGE")){
        for(const auto& [control,resource]:resources)
            if(control>=page && control-page<0x4000)RT_LOGF("pc-ui-test","Mode button offset %04X id %u\n",control-page,Memory::Read32(control+0x240));
    }
    // The game locks its scene heaps during frame updates. Allocate native UI
    // memory while the section is loading, just like its existing pages do.
    if(!textScratch)textScratch=Alloc(ctx,4096);
    if(id==0x5A){
        if(!Memory::Read32(Section()+8+0x53*4))Invoke(ctx,MKW_GADDR(80622D08),{Section(),0x53});
        const auto confirmation=Memory::Read32(Section()+8+0x53*4);
        AddButton(ctx,page,confirmation+0x294,confirmation+0x294,1,false,MKW_GADDR(8085110C));
        if(extraButtons.contains(page)){Text(ctx,extraButtons.at(page).button,174,ExitTitle());Memory::Write32(extraButtons.at(page).button+0x240,0x7F01);}
        exitBackHandler=Alloc(ctx,0x14);
        // Reuse the game's one-argument page-handler ABI for the prompt's B action.
        for(unsigned i=0;i<5;++i)Memory::Write32(exitBackHandler+i*4,Memory::Read32(page+0x694+i*4));
        Memory::Write32(exitBackHandler+4,Memory::Read32(Section()+8+0x53*4));Memory::Write32(exitBackHandler+8,0);Memory::Write32(exitBackHandler+12,0xFFFFFFFFu);Memory::Write32(exitBackHandler+16,MKW_GADDR(808516AC));
    }
    if(id==0xC0){
        AddButton(ctx,page,page+0x404,page+0x8AC,3,false,MKW_GADDR(805FD90C));
        // This section does not normally create SourceYesNo. Prepare it while
        // the scene heap is unlocked, before a live save-change confirmation.
        if(!Memory::Read32(Section()+8+0x53*4))Invoke(ctx,MKW_GADDR(80622D08),{Section(),0x53});
        exitBackHandler=Alloc(ctx,0x14);
        for(unsigned i=0;i<5;++i)Memory::Write32(exitBackHandler+i*4,Memory::Read32(page+0x58+i*4));
        Memory::Write32(exitBackHandler+4,Memory::Read32(Section()+8+0x53*4));
        Memory::Write32(exitBackHandler+8,0);Memory::Write32(exitBackHandler+12,0xFFFFFFFFu);
        Memory::Write32(exitBackHandler+16,MKW_GADDR(808516AC));
    }
    if(id==0x8B){
        AddButton(ctx,page,page+0xFAC,page+0xFAC,1,false,MKW_GADDR(8064BCE8));
        if(extraButtons.contains(page))Text(ctx,extraButtons.at(page).button,110,T(u"Salas",u"Rooms",u"Salles",u"Räume",u"Stanze"));
    }
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
extern "C" void PcSettingsButton(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853654)(ctx);return;}if(ctx->gpr[4]==activePage+0x54){Close(ctx,activePage);return;}if(sheet==5)return;if(sheet==8)DVDMyStuffCancel();sheet=sheet==4?6:sheet==6?7:sheet==7?8:sheet==8?9:sheet==9?0:sheet+1;Configure(ctx,activePage);Invoke(ctx,MKW_GADDR(80602378),{activePage});}
REGISTER_NATIVE_WRAPPER_AS(0x80853654,PcSettingsButton,"PcOriginalUI_GraphicsSheet");
extern "C" void PcSettingsBack(CpuContext* ctx){if(ctx->gpr[3]==activePage)Close(ctx,activePage);else MKW_GUEST_NEXT(808536B4)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x808536B4,PcSettingsBack,"PcOriginalUI_GraphicsBack");
extern "C" void PcRadioClick(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853714)(ctx);return;}auto row=Memory::Read32(ctx->gpr[4]+0x20C);if(sheet==9){if(row==0){selectedSeparateSave=ctx->gpr[6]!=0;saveFailed=false;Configure(ctx,activePage);}else if(row==1){if(ownerPage&&Memory::Read32(ownerPage+4)==0xC0&&selectedSeparateSave!=NANDRetroSaveSeparate())OpenExit(ctx,activePage,true);else Bottom(ctx,SaveDetail());}}else if(sheet==8){if(row==1){DVDMyStuffOpenFolder();}else if(ownerPage&&Memory::Read32(ownerPage+4)==0xC0){myStuffFailed=!DVDMyStuffRequest(selectedMyStuff);Configure(ctx,activePage);}else Bottom(ctx,MyStuffDetail());}else if(sheet==7){if(row==0)PcTexturePacks::SetEnabled(ctx->gpr[6]!=0);else if(row==1)PcTexturePacks::Reload();else if(row==2)PcTexturePacks::OpenFolder();Configure(ctx,activePage);}else if(sheet==5){if(row==0&&selectedRoom<roomIds.size()){pendingRoomId=roomIds[selectedRoom];RT_LOGF("pc-ui-test","Public room selected in native menu\n");Close(ctx,activePage);}}else if(sheet==4){if(row==0){PcOnlineRooms::RefreshUpdates();Bottom(ctx,UpdateDetail());}}else if(sheet==3&&row<4){const bool assigned=selectedController<visibleControllerIds.size()&&settings_overlay::PcControllerAssign(row,visibleControllerIds[selectedController],ctx->gpr[6]!=0);Configure(ctx,activePage);if(!assigned)Bottom(ctx,T(u"Mando desconectado. Elige otro dispositivo.",u"Controller disconnected. Choose another device.",u"Manette déconnectée. Choisissez un appareil.",u"Controller getrennt. Anderes Gerät wählen.",u"Controller scollegato. Scegli un altro dispositivo."));}else if(row<5&&sheet<3)settings_overlay::SetPcGraphicsValue(sheet,row,ctx->gpr[6]);}
REGISTER_NATIVE_WRAPPER_AS(0x80853714,PcRadioClick,"PcOriginalUI_GraphicsChoice");
extern "C" void PcRadioSelect(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(8085389C)(ctx);return;}const auto row=Memory::Read32(ctx->gpr[4]+0x20C);Bottom(ctx,sheet==9?SaveDetail():sheet==8?MyStuffDetail():sheet==3&&row<4?settings_overlay::PcControllerPlayerName(row):sheet==4?UpdateDetail():T(u"Izquierda/derecha y A: elegir. B: volver.",u"Left/right and A: select. B: back.",u"Gauche/droite et A : choisir. B : retour.",u"Links/rechts und A: wählen. B: zurück.",u"Sinistra/destra e A: scegli. B: indietro."));}
REGISTER_NATIVE_WRAPPER_AS(0x8085389C,PcRadioSelect,"PcOriginalUI_GraphicsHint");
extern "C" void PcScrollChange(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853998)(ctx);return;}if(sheet==9)return;if(sheet==8){selectedMyStuff=std::min<unsigned>(ctx->gpr[6],4);myStuffFailed=false;Bottom(ctx,MyStuffDetail());return;}else if(sheet==0){settings_overlay::PcSetAspect(ctx->gpr[6]);Configure(ctx,activePage);return;}if(sheet==6){settings_overlay::SetPcGraphicsScroller(0,ctx->gpr[6]);return;}if(sheet==7)return;if(sheet==5){selectedRoom=std::min<unsigned>(ctx->gpr[6],roomValues.size()-1);return;}if(sheet==4)return;if(sheet==3){const auto devices=settings_overlay::PcControllerDevices();selectedController=std::min<unsigned>(ctx->gpr[6],devices.size()-1);Configure(ctx,activePage);}else{settings_overlay::SetPcGraphicsScroller(sheet,ctx->gpr[6]);if(sheet==1)Configure(ctx,activePage);}}
REGISTER_NATIVE_WRAPPER_AS(0x80853998,PcScrollChange,"PcOriginalUI_GraphicsScroll");
extern "C" void PcTextChange(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853A10)(ctx);return;}const auto devices=settings_overlay::PcControllerDevices();const auto controllerValues=ControllerChoices(devices);const std::vector<std::u16string> updateValues{PcOnlineRooms::UpdateSummary()};const auto aspectValues=AspectChoices(),textureValues=TextureChoices();const auto& values=sheet==9?SaveChoices():sheet==8?MyStuffChoices():sheet==0?aspectValues:sheet==6?scales:sheet==7?textureValues:sheet==5?roomValues:sheet==4?updateValues:sheet==3?controllerValues:sheet==2?PcMenuLocale::Languages():sheet==1?presets:scales;auto value=std::min<unsigned>(ctx->gpr[5],values.size()-1);Text(ctx,ctx->gpr[4],51,values[value]);if(sheet==3)Bottom(ctx,devices[value]);}
REGISTER_NATIVE_WRAPPER_AS(0x80853A10,PcTextChange,"PcOriginalUI_GraphicsValueText");
extern "C" void PcExternalSelect(CpuContext* ctx){if(ctx->gpr[3]!=activePage){MKW_GUEST_NEXT(80853684)(ctx);return;}Bottom(ctx,T(u"Ver la siguiente pagina de opciones avanzadas.",u"Open the next page of advanced options.",u"Ouvrir la page suivante des options avancées.",u"Nächste Seite der erweiterten Optionen öffnen.",u"Apri la pagina successiva delle opzioni avanzate."));}
REGISTER_NATIVE_WRAPPER_AS(0x80853684,PcExternalSelect,"PcOriginalUI_GraphicsNextHint");
extern "C" void PcPageUpdate(CpuContext* ctx){const auto page=ctx->gpr[3];MKW_GUEST_NEXT(80602338)(ctx);Position(page);
    if(exitPrompt&&exitClosing&&Memory::Read32(exitPrompt+8)<=1){Memory::Write32(exitPrompt+0x6C,exitOriginalBack);Memory::Write8(exitPrompt+0x8D,exitOriginalRepeat);Memory::Write8(exitPrompt+0x96,exitOriginalPointer);for(auto [control,hidden]:exitHidden)Memory::Write8(control+0x80,hidden);exitHidden.clear();exitPrompt=0;exitClosing=false;if(exitOwner)Invoke(ctx,MKW_GADDR(80602378),{exitOwner});exitOwner=0;RT_LOGF("pc-exit","Exit cancelled; returned to main menu\n");}
    const auto myStuffResult=DVDMyStuffPump(activePage&&sheet==8&&!closingPc&&ownerPage&&Memory::Read32(ownerPage+4)==0xC0);
    if(myStuffResult&&activePage&&sheet==8&&!closingPc){myStuffFailed=myStuffResult==-1;if(myStuffResult==1)selectedMyStuff=DVDMyStuffMode();Configure(ctx,activePage);}
    if(saveApplyPending&&!exitPrompt){saveApplyPending=false;saveFailed=!ApplySaveSwitch(ctx);if(!saveFailed)return;if(activePage)Bottom(ctx,SaveDetail());}
    if(!pendingRoomId.empty() && Memory::Read32(page+4)==0x8C && Memory::Read32(page+8)==4){
        const auto section=Section(),layers=Memory::Read32(section+0x37C);
        if(layers&&layers<=11 && Memory::Read32(section+0x354+(layers-1)*4)==page)roomSearchStarted=true;
    }
    if(roomPageRequested && !roomPageActive && Memory::Read32(page+4)==0x108 && Memory::Read32(page+8)==4){
        if(!ShowRoomPage(ctx,page)){roomPageRequested=false;RT_LOGF("pc-ui-test","Room page layout mismatch; original settings retained\n");}
    }
    if(roomPageActive && page==roomPage && Memory::Read32(page+8)==4){
        if(PcOnlineRooms::Get().revision!=roomListRevision){
            const bool refreshing=roomNoticeRefreshing;RefreshRoomChoices();RenderRoomPage(ctx);
            if(std::getenv("MKW_PC_UI_TEST_BRIDGE"))RT_LOGF("pc-ui-test","Refreshed visible room list at revision %llu\n",static_cast<unsigned long long>(roomListRevision));
            if(refreshing){roomNotice=PcOnlineRooms::Get().available?T(u"Sala no disponible. Lista actualizada.",u"Room unavailable. List updated.",u"Salle indisponible. Liste actualisée.",u"Raum nicht verfügbar. Liste aktualisiert.",u"Stanza non disponibile. Lista aggiornata."):T(u"No se pudo actualizar la lista.",u"Could not refresh the list.",u"Actualisation impossible.",u"Liste konnte nicht aktualisiert werden.",u"Impossibile aggiornare la lista.");roomNoticeRefreshing=false;}
        }
        Text(ctx,Memory::Read32(page+0x2BC),151,roomNotice.empty()?T(u"Elige una sala publica. B: volver.",u"Choose a public room. B: back.",u"Choisissez une salle. B : retour.",u"Raum wählen. B: zurück.",u"Scegli una stanza. B: indietro."):roomNotice);
    }
    if(roomPageActive && page==roomPage && Memory::Read32(page+8)<=1)RestoreRoomPage();
    if(roomPageActive && Memory::Read32(page+4)==0x8B && Memory::Read32(page+8)==4){
        const auto section=Section(),layers=Memory::Read32(section+0x37C);
        if(layers&&layers<=11 && Memory::Read32(section+0x354+(layers-1)*4)==page)RestoreRoomPage();
    }
    if(roomSearchStarted && Memory::Read32(page+4)==0x8B && Memory::Read32(page+8)==4){
        const auto section=Section(),layers=Memory::Read32(section+0x37C);
        if(layers&&layers<=11 && Memory::Read32(section+0x354+(layers-1)*4)==page){pendingRoomId.clear();pendingRoomKey.clear();probeChosenRoomId.clear();probeKnownPeers.clear();probeMemberOffset=0;roomSearchStarted=probeJoinedTarget=false;RT_LOGF("pc-ui-test","Cleared room selection after returning to online menu\n");}
    }
    if(roomRouteStage && !roomPageActive && Memory::Read32(page+8)==4){
        const auto route=PcOnlineRooms::RouteForRoomMode(pendingRoomKey);const auto id=Memory::Read32(page+4);
        if(route && roomRouteStage==1 && id==0x8B && ++roomRouteFrames>=10){
            const auto snapshot=PcOnlineRooms::Get();const bool forceFailure=std::getenv("MKW_PC_UI_TEST_BRIDGE")&&std::getenv("MKW_PC_ROOM_ROUTE_FAIL_TEST");const bool available=!forceFailure&&snapshot.available&&std::any_of(snapshot.rooms.begin(),snapshot.rooms.end(),[](const auto& room){return room.id==pendingRoomId&&room.roomKey==pendingRoomKey&&room.publicJoinable;});
            const auto button=page+route->mainOffset;
            if(available&&resources.contains(button)&&Memory::Read32(button+0x240)==route->mainId){
                const auto holder=Memory::Read32(button+0x234),handler=holder?Memory::Read32(holder+16):0;
                if(handler){roomRouteStage=2;roomRouteFrames=0;RT_LOGF("pc-ui-test","Routing selected room through native online category\n");Invoke(ctx,handler,{page,button,0});}
                else {roomRouteStage=0;pendingRoomId.clear();pendingRoomKey.clear();probeKnownPeers.clear();routeNoticePage=page;routeNoticeFrames=300;RT_LOGF("pc-ui-test","Selected room category handler unavailable\n");}
            }else {roomRouteStage=0;pendingRoomId.clear();pendingRoomKey.clear();probeKnownPeers.clear();routeNoticePage=page;routeNoticeFrames=300;PcOnlineRooms::Refresh();RT_LOGF("pc-ui-test","Selected room or online category unavailable before routing\n");}
        }else if(route && roomRouteStage==2 && id==0x8C && ++roomRouteFrames>=10){
            uint32_t target=0;
            for(const auto& [control,resource]:resources)if(control>=page&&control-page<0x4000&&Memory::Read32(control+0x240)==route->modeId&&!Memory::Read8(control+0x80)&&!Memory::Read8(control+0x1E9)){target=control;break;}
            roomRouteStage=0;
            if(target){const auto holder=Memory::Read32(target+0x234),handler=holder?Memory::Read32(holder+16):0;
                if(handler){RT_LOGF("pc-ui-test","Routing selected room through native mode\n");Invoke(ctx,handler,{page,target,0});}
                else {pendingRoomId.clear();pendingRoomKey.clear();probeKnownPeers.clear();routeNoticePage=page;routeNoticeFrames=300;RT_LOGF("pc-ui-test","Selected room mode handler unavailable\n");}
            }else {pendingRoomId.clear();pendingRoomKey.clear();probeKnownPeers.clear();routeNoticePage=page;routeNoticeFrames=300;RT_LOGF("pc-ui-test","Selected room mode unavailable\n");}
        }
    }
    if(page==routeNoticePage && routeNoticeFrames && Memory::Read32(page+8)==4){
        const auto id=Memory::Read32(page+4),bottom=id==0x8B?0xB98u:id==0x8C?0x914u:0u;
        if(bottom)Text(ctx,page+bottom,152,T(u"Sala no disponible. Elige otra sala.",u"Room unavailable. Choose another room.",u"Salle indisponible. Choisissez une autre salle.",u"Raum nicht verfügbar. Anderen Raum wählen.",u"Stanza non disponibile. Scegli un'altra stanza."));
        if(!--routeNoticeFrames){routeNoticePage=0;if(bottom)Text(ctx,page+bottom,153,T(u"Elige una opcion. B: volver.",u"Choose an option. B: back.",u"Choisissez une option. B : retour.",u"Option wählen. B: zurück.",u"Scegli un'opzione. B: indietro."));}
    }
    if(std::getenv("MKW_PC_UI_TEST_BRIDGE") && Memory::Read32(page+4)==0x8B && Memory::Read32(page+8)==4 && extraButtons.contains(page)){
        static unsigned samples=0;if(++samples==180){const auto button=extraButtons.at(page).button,other=page+0x1200;
            RT_LOGF("pc-ui-test","Room bounds %.1f %.1f %.1f %.1f; main bounds %.1f %.1f %.1f %.1f; inaccessible %u, parent %08X\n",
                Memory::ReadFloat32(button+0x184),Memory::ReadFloat32(button+0x188),Memory::ReadFloat32(button+0x190),Memory::ReadFloat32(button+0x194),
                Memory::ReadFloat32(other+0x184),Memory::ReadFloat32(other+0x188),Memory::ReadFloat32(other+0x190),Memory::ReadFloat32(other+0x194),
                Memory::Read8(button+0x1E9),Memory::Read32(button+0x1B0));
        }
    }
    if(page==activePage&&sheet==7&&!closingPc&&visibleTextureState!=TextureState())Configure(ctx,page);
    if(page==activePage&&sheet==3&&!closingPc){const auto current=settings_overlay::PcControllerDeviceIds();if(current!=visibleControllerIds||settings_overlay::PcControllerDevices()!=visibleControllers){const auto chosen=selectedController<visibleControllerIds.size()?visibleControllerIds[selectedController]:0;const auto found=std::find(current.begin(),current.end(),chosen);selectedController=found==current.end()?0:static_cast<unsigned>(found-current.begin());Configure(ctx,page);}}
    if(page==activePage&&sheet==4&&!closingPc&&UpdateStateKey()!=visibleUpdateState)Configure(ctx,page);
    if(languageRefresh&&textScratch){languageRefresh=false;RefreshLanguage(ctx);}
    const auto pageId=Memory::Read32(page+4);if(Memory::Read32(page+8)==4&&!stockLabels.contains(page)){
        if(pageId==0x65){Text(ctx,page+0x2F4,100,T(u"Elige un carné",u"Select a License",u"Choisissez un permis",u"Lizenz auswählen",u"Scegli una patente"));Text(ctx,page+0xDB8,101,PcMenuLocale::Options());stockLabels.insert(page);}
        else if(pageId==0xC0){Text(ctx,page+0x290,102,PcMenuLocale::Options());Text(ctx,page+0x8AC,103,T(u"Canal Mario Kart",u"Mario Kart Channel",u"Chaîne Mario Kart",u"Mario Kart-Kanal",u"Canale Mario Kart"));stockLabels.insert(page);}
    }
    if(std::getenv("MKW_PC_UI_TEST_BRIDGE")){static uint32_t lastTop=0,lastSection=0;const auto section=Section(),layers=Memory::Read32(section+0x37C);if(layers&&layers<=11){const auto top=Memory::Read32(section+0x354+(layers-1)*4);if(top!=lastTop||section!=lastSection){lastTop=top;lastSection=section;RT_LOGF("pc-ui-test","Section %u, top page %u at %08X\n",Memory::Read32(section),Memory::Read32(top+4),top);}}}
    if(activePage&&closingPc&&Memory::Read32(activePage+8)<=1){for(unsigned i=0;i<5;++i){auto radio=activePage+0x6C4+i*0x298;Memory::Write8(radio+0x80,0);Memory::Write8(radio+0x285,0);Memory::Write32(radio+0x200,originalRadioCounts[i]);auto buttons=Memory::Read32(radio+0x294);for(unsigned j=0;j<originalRadioCounts[i];++j)Memory::Write8(buttons+j*0x17C+0x80,0);}if(sheet==5){const auto external=Memory::Read32(activePage+0x48);if(Memory::Read32(activePage+0x4C))Memory::Write8(Memory::Read32(external)+0x80,0);}activePage=0;closingPc=false;if(backgroundAdded){backgroundAdded=false;Invoke(ctx,MKW_GADDR(80623288),{Section()});}for(auto [control,hidden]:hiddenOwner)Memory::Write8(control+0x80,hidden);hiddenOwner.clear();if(ownerPage)Invoke(ctx,MKW_GADDR(80602378),{ownerPage});}
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
extern "C" void PcOnlineRoomsClick(CpuContext* ctx){
    const auto page=ctx->gpr[3];
    if(roomPageActive && page==roomPage){
        const auto button=ctx->gpr[4];const auto it=std::find_if(roomButtonStates.begin(),roomButtonStates.end(),[button](const auto& item){return item.address==button;});
        if(it!=roomButtonStates.end()){
            const auto slot=static_cast<size_t>(it-roomButtonStates.begin());
            if(roomPageHasNext && slot==roomPageCapacity){NextRoomPage(ctx);return;}
            const auto index=roomPageOffset+slot;const auto snapshot=PcOnlineRooms::Get();
            if(!std::getenv("MKW_PC_ROOM_UNAVAILABLE_TEST") && index<roomIds.size() && snapshot.available && std::any_of(snapshot.rooms.begin(),snapshot.rooms.end(),[&](const auto& room){return room.id==roomIds[index]&&room.publicJoinable;})){
                pendingRoomId=roomIds[index];for(const auto& room:snapshot.rooms)if(room.id==pendingRoomId){pendingRoomKey=room.roomKey;probeKnownPeers=room.memberPids;break;}
                if(!textScratch || !PcOnlineRooms::RouteForRoomMode(pendingRoomKey)){
                    pendingRoomId.clear();pendingRoomKey.clear();probeKnownPeers.clear();
                    roomNoticeRefreshing=false;roomNotice=T(u"Este modo aun no esta disponible.",u"This mode is not available yet.",u"Ce mode n'est pas encore disponible.",u"Dieser Modus ist noch nicht verfügbar.",u"Questa modalità non è ancora disponibile.");
                    RT_LOGF("pc-ui-test","Selected room has no verified native route\n");return;
                }
                roomRouteStage=1;roomRouteFrames=0;
                RT_LOGF("pc-ui-test","Public room selected in native page\n");Invoke(ctx,MKW_GADDR(80837B4C),{page,button});
            }else {roomNoticeRefreshing=true;roomNotice=T(u"Sala no disponible. Actualizando...",u"Room unavailable. Refreshing...",u"Salle indisponible. Actualisation...",u"Raum nicht verfügbar. Aktualisierung...",u"Stanza non disponibile. Aggiornamento...");PcOnlineRooms::Refresh();RT_LOGF("pc-ui-test","Selected room became unavailable\n");}
        }return;
    }
    if(extraButtons.contains(page) && extraButtons.at(page).button==ctx->gpr[4]){
        const auto source=extraButtons.at(page).source;const auto holder=Memory::Read32(source+0x234),handler=Memory::Read32(holder+16);
        if(handler){roomPageRequested=true;PcOnlineRooms::Refresh();Invoke(ctx,handler,{page,source,ctx->gpr[5]});}
        return;
    }
    MKW_GUEST_NEXT(8064BCE8)(ctx);
}
REGISTER_NATIVE_WRAPPER_AS(0x8064BCE8,PcOnlineRoomsClick,"PcOriginalUI_RoomButtonProbe");
// Preserve the game's original matchmaking constraints and narrow them only
// while the player is joining a particular public room.
extern "C" void PcMatchFilterProbe(CpuContext* ctx){
    if(std::getenv("MKW_PC_UI_TEST_BRIDGE") || !pendingRoomId.empty()){
        const auto address=ctx->gpr[8];std::string filter;
        for(unsigned i=0;i<255 && address && Memory::Contains(address+i);++i){
            const auto c=Memory::Read8(address+i);if(!c)break;
            filter.push_back(static_cast<char>(c));
        }
        if(std::getenv("MKW_PC_UI_TEST_BRIDGE")){auto redacted=filter;for(auto& c:redacted)if(c>='0'&&c<='9')c='#';RT_LOGF("pc-ui-test","Match filter: %s\n",redacted.c_str());}
        if(textScratch && !pendingRoomId.empty()){
            bool applied=false;
            const auto snapshot=PcOnlineRooms::Get();
            if(snapshot.available)for(const auto& room:snapshot.rooms){
                if(!room.publicJoinable || (!probeChosenRoomId.empty() && room.id!=probeChosenRoomId) || (!pendingRoomId.empty() && room.id!=pendingRoomId))continue;
                std::array<uint32_t,1> impossible{};
                std::span<const uint32_t> targets=room.memberPids;
                if(std::getenv("MKW_PC_UI_TEST_BRIDGE") && std::getenv("MKW_PC_ROOM_PROBE_NO_MATCH")){
                    const auto start=filter.find("dwc_pid != ");
                    if(start!=std::string::npos){const auto begin=filter.data()+start+11;std::from_chars(begin,filter.data()+filter.size(),impossible[0]);}
                    targets=impossible;
                }
                const auto narrowed=PcOnlineRooms::NarrowMatchFilterBatch(filter,room.roomKey,targets,probeMemberOffset);
                if(!narrowed)continue;
                probeChosenRoomId=room.id;probeKnownPeers=room.memberPids;probeMemberOffset=narrowed->nextOffset;roomSearchStarted=true;
                const auto scratch=textScratch+3072;
                for(size_t i=0;i<narrowed->expression.size();++i)Memory::Write8(scratch+i,static_cast<uint8_t>(narrowed->expression[i]));
                Memory::Write8(scratch+narrowed->expression.size(),0);ctx->gpr[8]=scratch;
                applied=true;RT_LOGF("pc-ui-test","Selected public room filter applied to compatible search\n");break;
            }
            // If a selected room is stale, full or for a different mode, do
            // not let the diagnostic silently search unrelated rooms.
            if(!applied && !pendingRoomId.empty() && filter.find("dwc_mver = ")!=std::string::npos){
                const auto blocked=PcOnlineRooms::BlockUnrelatedMatches(filter);
                if(blocked){const auto scratch=textScratch+3072;for(size_t i=0;i<blocked->size();++i)Memory::Write8(scratch+i,static_cast<uint8_t>((*blocked)[i]));Memory::Write8(scratch+blocked->size(),0);ctx->gpr[8]=scratch;roomSearchStarted=true;RT_LOGF("pc-ui-test","Selected room unavailable or wrong mode; unrelated matches blocked\n");}
                else {RT_LOGF("pc-ui-test","Selected room filter could not be safely constrained\n");ctx->gpr[3]=6;return;}
            }
        }
    }
    MKW_GUEST_NEXT(8011E2A0)(ctx);
}
REGISTER_NATIVE_WRAPPER_AS(0x8011E2A0,PcMatchFilterProbe,"PcOriginalUI_MatchFilterProbe");
extern "C" void PcServerSetupProbe(CpuContext* ctx){
    if(std::getenv("MKW_PC_UI_TEST_BRIDGE") && std::getenv("MKW_PC_ROOM_PROBE"))
        RT_LOGF("pc-ui-test","Game requested server setup during room probe\n");
    MKW_GUEST_NEXT(800D1984)(ctx);
}
REGISTER_NATIVE_WRAPPER_AS(0x800D1984,PcServerSetupProbe,"PcOriginalUI_ServerSetupProbe");
extern "C" void PcIncomingReservationProbe(CpuContext* ctx){
    if(!pendingRoomId.empty() && ctx->gpr[3]==1){
        const auto peer=ctx->gpr[4];const auto snapshot=PcOnlineRooms::Get();bool member=false,known=false;
        const auto match=Memory::Read32(0x8038630Cu);const auto server=match&&Memory::Contains(match+0x638,4)?Memory::Read32(match+0x638):0;
        const bool connected=match&&Memory::Contains(match+0xE)&&Memory::Read8(match+0xE)>0;
        for(const auto& room:snapshot.rooms)if(room.id==pendingRoomId){known=true;member=std::find(room.memberPids.begin(),room.memberPids.end(),peer)!=room.memberPids.end();}
        if(PcOnlineRooms::IsConnectedToSelectedRoom(connected,server,probeKnownPeers))probeJoinedTarget=true;
        RT_LOGF("pc-ui-test","Incoming reservation while choosing room: %s\n",member?"listed member":"other or unknown");
        if(!probeJoinedTarget && !member && textScratch){
            // DWC normally replies to a refused reservation with a one-word
            // reason. Keep the remote peer on the ordinary denial path.
            Memory::Write32(textScratch+3328,0x12000000u);
            Invoke(ctx,MKW_GADDR(800DBD38),{3,peer,ctx->gpr[5],ctx->gpr[6],textScratch+3328,1});
            RT_LOGF("pc-ui-test","Rejected unrelated incoming reservation during directed search%s\n",known?"":" (room status unavailable)");return;
        }
    }
    MKW_GUEST_NEXT(800DC234)(ctx);
}
REGISTER_NATIVE_WRAPPER_AS(0x800DC234,PcIncomingReservationProbe,"PcOriginalUI_IncomingReservationProbe");
extern "C" void PcDvdPathVisibility(CpuContext* ctx){MKW_GUEST_NEXT(8015DF4C)(ctx);if(!DVDMyStuffEntryVisible(static_cast<int32_t>(ctx->gpr[3])))ctx->gpr[3]=0xFFFFFFFFu;}
REGISTER_NATIVE_WRAPPER_AS(0x8015DF4C,PcDvdPathVisibility,"PcOriginalDvd_PathVisibility");
extern "C" void PcDvdOpenVisibility(CpuContext* ctx){if(!DVDMyStuffEntryVisible(static_cast<int32_t>(ctx->gpr[3]))){ctx->gpr[3]=0;return;}MKW_GUEST_NEXT(8015E254)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x8015E254,PcDvdOpenVisibility,"PcOriginalDvd_OpenVisibility");

extern "C" void PcMainQuitClick(CpuContext* ctx){const auto owner=ctx->gpr[3];if(extraButtons.contains(owner)&&extraButtons.at(owner).button==ctx->gpr[4])OpenExit(ctx,owner);else MKW_GUEST_NEXT(8085110C)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x8085110C,PcMainQuitClick,"PcOriginalUI_QuitMain");
extern "C" void PcExitConfirmClick(CpuContext* ctx){if(ctx->gpr[3]!=exitPrompt){MKW_GUEST_NEXT(80652678)(ctx);return;}if(ctx->gpr[4]==exitPrompt+0x294){if(exitSwitchSave)saveApplyPending=true;else {RT_LOGF("pc-exit","Exit confirmed from native menu\n");ExitForAuroraWindowClose();}}CloseExit(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x80652678,PcExitConfirmClick,"PcOriginalUI_QuitConfirm");
extern "C" void PcExitBack(CpuContext* ctx){if(ctx->gpr[3]==exitPrompt){CloseExit(ctx);return;}MKW_GUEST_NEXT(808516AC)(ctx);}
REGISTER_NATIVE_WRAPPER_AS(0x808516AC,PcExitBack,"PcOriginalUI_QuitCancel");
