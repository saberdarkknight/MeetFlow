#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string_view>
namespace{std::string read(const std::filesystem::path&p){std::ifstream f(p);if(!f)throw std::runtime_error("cannot read "+p.string());std::ostringstream s;s<<f.rdbuf();return s.str();}void write(const std::filesystem::path&p,const std::string&s){std::ofstream f(p);if(!f)throw std::runtime_error("cannot write "+p.string());f<<s;}}
int main(int argc,char*argv[]){try{if(argc==2&&std::string_view{argv[1]}=="--help"){std::cout<<"Usage: meetflow-finalize <meeting-directory>\n";return 0;}if(argc!=2){std::cerr<<"Usage: meetflow-finalize <meeting-directory>\n";return 2;}std::filesystem::path d=argv[1];auto draft=read(d/"draft/minutes.draft.md"),tr=read(d/"draft/transcript.md"),y=read(d/"draft/action-items.yaml");std::filesystem::create_directories(d/"final");std::ostringstream o;o<<draft<<"\n## Confirmed action items\n\n| Owner | Action | Due | Source |\n| --- | --- | --- | --- |\n";std::regex r(R"(\s*- id: ([^\n]+)\n\s+status: confirmed\n\s+owner: ([^\n]+)\n\s+action: ['"]?([^'"\n]+)['"]?\n\s+due: ([^\n]+)\n\s+source_segment: ([^\n]+))");int n=0;for(std::sregex_iterator i(y.begin(),y.end(),r),e;i!=e;++i){o<<"| "<<(*i)[2]<<" | "<<(*i)[3]<<" | "<<(*i)[4]<<" | "<<(*i)[5]<<" |\n";++n;}if(!n)o<<"| _None confirmed_ |  |  |  |\n";o<<"\n"<<tr;write(d/"final/minutes.md",o.str());std::cout<<"Created "<<d/"final/minutes.md\n";return 0;}catch(const std::exception&e){std::cerr<<"meetflow-finalize: "<<e.what()<<"\n";return 1;}}
