#include "pc_texture_archive.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace PcTextureArchive;
bool bad(const std::string& text){try{ValidateListing(text);return false;}catch(const std::exception&){return true;}}
int main(int argc,char** argv){
    const std::string file="tex1_128x128_80581df292c2e29c_5.png";
    assert(ValidateListing("Path = textures\\RMCP01\\"+file+"\r\r\nSize = 128\r\r\nEncrypted = -\r\r\n\r\r\n").textures==1);
    for(const auto& path:{"../a.png","/absolute","C:\\escape","a/../b","a/./b","a//b","AUX.png","sub/COM1/a","a:stream","folder./a","star*/x","bad?/x"})assert(bad(std::string("Path = ")+path+"\nSize = 1\n\n"));
    assert(bad("Path = a\nSymbolic Link = /outside\n\n"));
    assert(bad("Path = a\nAttributes = XU A1FF0000\n\n"));
    assert(bad("Path = a\nHard Link = outside\n\n"));
    assert(bad("Path = a\nEncrypted = +\n\n"));
    assert(bad("Path = a\n\nPath = A\n\n"));
    assert(bad("Path = "+file+"\nSize = 999999999999\n\n"));
    assert(bad("Path = a\nnewline-injected-path\n\n"));
    assert(bad("Path = logo.png\nSize = 999999999999\n\n"));
    if(argc==2){std::ifstream in(argv[1],std::ios::binary);assert(in);const std::string text{std::istreambuf_iterator<char>(in),{}};auto listing=ValidateListing(text);assert(listing.textures==1);std::cout<<"Real 7-Zip listing: "<<listing.entries<<" entries, "<<listing.textures<<" Dolphin texture\n";}
    std::cout<<"Archive paths, device names, links, encryption, duplicates, limits and CRLF validated\n";
}
