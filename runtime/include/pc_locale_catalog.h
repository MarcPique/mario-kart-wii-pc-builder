#pragma once
#include <array>
#include <string_view>

namespace PcLocaleCatalog {
struct Language {std::string_view code;std::u16string_view name;};
inline constexpr std::array Languages{
    Language{"es",u"Español"},Language{"en",u"English"},Language{"fr",u"Français"},
    Language{"de",u"Deutsch"},Language{"it",u"Italiano"},Language{"nl",u"Nederlands"},
    Language{"cs",u"Čeština"},Language{"fi",u"Suomi"},Language{"ru",u"Русский"},
    Language{"tr",u"Türkçe"},Language{"es-419",u"Español LA"},Language{"ca",u"Català"},
    Language{"gl",u"Galego"},Language{"eu",u"Euskara"},Language{"and_ES",u"Andalûh"}
};
inline bool Known(std::string_view code){for(const auto& language:Languages)if(language.code==code)return true;return false;}
inline bool AutomaticTranslation(std::string_view code){return code=="ca"||code=="gl"||code=="eu";}
inline std::u16string_view Name(std::string_view code){for(const auto& language:Languages)if(language.code==code)return language.name;return {};}
}
