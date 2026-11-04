#pragma once
#include "runtime_config.h"
#include "pc_live_locale.h"
#include "pc_locale_catalog.h"
#include <string>
namespace PcMenuLocale {
inline const std::string& Current(){static const std::string fallback="en";const auto& value=RuntimeConfigFile::Get().gameLanguage;return value?*value:fallback;}
inline std::u16string T(const char16_t* es,const char16_t* en,const char16_t* fr,const char16_t* de,const char16_t* it){auto text=PcLiveLocale::MenuText(en);if(!text.empty())return text;const auto& l=Current();if(l=="es")return es;if(l=="fr")return fr;if(l=="de")return de;if(l=="it")return it;return en;}
inline std::u16string Advanced(){return T(u"Opciones avanzadas",u"Advanced Options",u"Options avancées",u"Erweiterte Optionen",u"Opzioni avanzate");}
inline std::u16string Display(){return T(u"Pantalla",u"Display",u"Affichage",u"Anzeige",u"Schermo");}
inline std::u16string Quality(){return T(u"Calidad grafica",u"Graphics Quality",u"Qualité graphique",u"Grafikqualität",u"Qualità grafica");}
inline std::u16string Language(){return T(u"Idioma",u"Language",u"Langue",u"Sprache",u"Lingua");}
inline std::u16string Options(){return T(u"Opciones",u"Options",u"Options",u"Optionen",u"Opzioni");}
inline const std::vector<std::string>& Codes(){return PcLiveLocale::LanguageCodes();}
inline std::vector<std::u16string> Languages(){std::vector<std::u16string> names;for(const auto& code:Codes())names.emplace_back(PcLocaleCatalog::Name(code));return names;}
inline unsigned LanguageIndex(const std::string& code){const auto& codes=Codes();auto found=std::find(codes.begin(),codes.end(),code);if(found==codes.end())found=std::find(codes.begin(),codes.end(),"en");return static_cast<unsigned>(found-codes.begin());}
}
