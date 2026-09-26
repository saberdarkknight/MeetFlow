#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace {
struct Segment { std::string id,speaker,text; double start{},end{}; };
std::string read(const std::filesystem::path&p){std::ifstream f(p);if(!f)throw std::runtime_error("cannot read "+p.string());std::ostringstream s;s<<f.rdbuf();return s.str();}
void write(const std::filesystem::path&p,const std::string&s){std::ofstream f(p);if(!f)throw std::runtime_error("cannot write "+p.string());f<<s;}
std::string quote(std::string s){std::string o="'";for(char c:s){if(c=='\'')o+="''";else if(c!='\n'&&c!='\r')o+=c;}return o+"'";}
std::vector<Segment> parse(const std::string&j){std::regex r(R"(\"id\"\s*:\s*\"([^\"]+)\"[\s\S]*?\"start_seconds\"\s*:\s*([0-9.]+)[\s\S]*?\"end_seconds\"\s*:\s*([0-9.]+)[\s\S]*?\"speaker\"\s*:\s*\"([^\"]+)\"[\s\S]*?\"text\"\s*:\s*\"((?:\\.|[^\"])*)\")");std::vector<Segment>v;for(std::sregex_iterator i(j.begin(),j.end(),r),e;i!=e;++i){std::string t=(*i)[5];std::string u;for(size_t n=0;n<t.size();++n){if(t[n]=='\\'&&n+1<t.size())u+=t[++n]=='n'?'\n':t[n];else u+=t[n];}v.push_back({(*i)[1],(*i)[4],u,std::stod((*i)[2]),std::stod((*i)[3])});}return v;}
std::string name(const std::string&y,const std::string&id){std::regex r("^  "+id+R"(\s*:\s*(.+)$)",std::regex::multiline);std::smatch m;return std::regex_search(y,m,r)?m[1].str():id;}
}
int main(int argc,char*argv[]){try{if(argc==2&&std::string_view{argv[1]}=="--help"){std::cout<<"Usage: meetflow-draft <meeting-directory>\n";return 0;}if(argc!=2){std::cerr<<"Usage: meetflow-draft <meeting-directory>\n";return 2;}std::filesystem::path d=argv[1];if(!std::filesystem::is_directory(d))throw std::runtime_error("meeting directory does not exist");auto ss=parse(read(d/"transcript.json"));auto sy=read(d/"speakers.yaml");auto out=d/"draft";std::filesystem::create_directories(out);std::ostringstream tr,mi,ac;tr<<"# Transcript\n\n";mi<<"# Meeting minutes (draft)\n\n> Review and edit this draft before finalization.\n\n## Summary\n\n_Add a concise summary._\n\n## Decisions\n\n_Add confirmed decisions._\n\n## Candidate action items\n\nSee `action-items.yaml`.\n\n## Transcript\n\n";ac<<"schema_version: 1\naction_items:\n";int n=0;for(auto&s:ss){tr<<"- ["<<s.start<<"–"<<s.end<<"] **"<<name(sy,s.speaker)<<"**: "<<s.text<<"\n";std::string l=s.text;for(char&c:l)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));if(l.find("action")!=std::string::npos||l.find("todo")!=std::string::npos||l.find(" will ")!=std::string::npos){++n;ac<<"  - id: action_"<<n<<"\n    status: needs_review\n    owner: unknown\n    action: "<<quote(s.text)<<"\n    due: null\n    source_segment: "<<s.id<<"\n";}}if(!n)ac<<"  []\n";write(out/"transcript.md",tr.str());write(out/"minutes.draft.md",mi.str());write(out/"action-items.yaml",ac.str());std::cout<<"Created draft artifacts in "<<out<<"\n";return 0;}catch(const std::exception&e){std::cerr<<"meetflow-draft: "<<e.what()<<"\n";return 1;}}
