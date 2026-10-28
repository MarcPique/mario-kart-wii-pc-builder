#pragma once
#include "runtime_config.h"
#include <string>
namespace PcMenuLocale {
inline const std::string& Current(){static const std::string fallback="en";const auto& value=RuntimeConfigFile::Get().gameLanguage;return value?*value:fallback;}
inline std::u16string T(const char16_t* es,const char16_t* en,const char16_t* fr,const char16_t* de,const char16_t* it){const auto& l=Current();return l=="es"?es:l=="fr"?fr:l=="de"?de:l=="it"?it:en;}
inline std::u16string Advanced(){return T(u"Opciones avanzadas",u"Advanced Options",u"Options avancées",u"Erweiterte Optionen",u"Opzioni avanzate");}
inline std::u16string Display(){return T(u"Pantalla",u"Display",u"Affichage",u"Anzeige",u"Schermo");}
inline std::u16string Quality(){return T(u"Calidad grafica",u"Graphics Quality",u"Qualité graphique",u"Grafikqualität",u"Qualità grafica");}
inline std::u16string Language(){return T(u"Idioma",u"Language",u"Langue",u"Sprache",u"Lingua");}
inline std::u16string Options(){return T(u"Opciones",u"Options",u"Options",u"Optionen",u"Opzioni");}
inline unsigned LanguageIndex(const std::string& code){return code=="es"?0:code=="fr"?2:code=="de"?3:code=="it"?4:1;}
inline const std::array<std::string,5> Codes={"es","en","fr","de","it"};
inline const std::vector<std::u16string> Languages={u"Español",u"English",u"Français",u"Deutsch",u"Italiano"};
}
