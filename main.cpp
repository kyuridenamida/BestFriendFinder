#include <stdio.h>
#include <cstdlib>
#include <ctime>
#include <set>
#include <map>
#include <functional>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>

using namespace std;

#include "ujson/ujson.hpp"
#include "webclient.h"
#include "openssl/ssl.h"

string randomString(int n){
	const string table = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	string res;
	for (int i = 0; i < n; i++) res += table[(unsigned int)rand() % table.size()];
	return res;
}
string itos(unsigned long long n){
	stringstream ss;
	ss << n;
	return ss.str();
}
string toLower(string s){
	for( auto &c : s ){
		if( 'A' <= c && c <= 'Z' ){
			c |= 32;
		}
	}
	return s;
}


/**
* Escape 'string' according to RFC3986 and
* http://oauth.net/core/1.0/#encoding_parameters.
*
* @param string The data to be encoded
* @return encoded string otherwise NULL
* The caller must free the returned string.
*/
std::string oauth_url_escape(const char *string)
{
	unsigned char in;
	size_t length;

	if (!string) {
		return std::string();
	}

	length = strlen(string);

	std::stringbuf sb;

	while (length--) {
		in = *string;
		if (strchr("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_~.-", in)) {
			sb.sputc(in);
		}
		else {
			char tmp[10];
			snprintf(tmp, 4, "%%%02X", in);
			sb.sputc(tmp[0]);
			sb.sputc(tmp[1]);
			sb.sputc(tmp[2]);
		}
		string++;
	}
	return sb.str();
}

vector<unsigned char> calcHash(const std::string& key, const std::string& data)
{
	unsigned char res[1024];
	unsigned int  reslen;
	unsigned int  keylen = key.length();
	unsigned int  datalen = data.length();
    
	//‚±‚ê‚ÅŒvŽZ
	HMAC(EVP_sha1(), (const unsigned char*)key.c_str(), keylen, (const unsigned char*)data.c_str(), datalen, res, &reslen);
	vector<unsigned char> result;
	for (int i = 0; i < reslen; i++){
		result.push_back(res[i]);
	}

	return result;
}

vector<string> getFiles(string dirname){
	DIR *dir;
	dir=opendir(dirname.c_str());
	vector<string> files;
	for(struct dirent *dp=readdir(dir) ; dp ; dp=readdir(dir) ){
		if( dp->d_type == DT_REG ){ //通常ファイルなら
			files.push_back(dp->d_name);
		}
	}
	closedir(dir);
	return files;
}

string encode(const std::vector<unsigned char>& src)
{
    const std::string table("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    std::string       cdst;
    
    for (std::size_t i = 0; i < src.size(); ++i) {
        switch (i % 3) {
            case 0:
                cdst.push_back(table[(src[i] & 0xFC) >> 2]);
                if (i + 1 == src.size()) {
                    cdst.push_back(table[(src[i] & 0x03) << 4]);
                    cdst.push_back('=');
                    cdst.push_back('=');
                }
                
                break;
            case 1:
                cdst.push_back(table[((src[i - 1] & 0x03) << 4) | ((src[i + 0] & 0xF0) >> 4)]);
                if (i + 1 == src.size()) {
                    cdst.push_back(table[(src[i] & 0x0F) << 2]);
                    cdst.push_back('=');
                }
                
                break;
            case 2:
                cdst.push_back(table[((src[i - 1] & 0x0F) << 2) | ((src[i + 0] & 0xC0) >> 6)]);
                cdst.push_back(table[src[i] & 0x3F]);
                
                break;
        }
    }
    return cdst;
}

class Cache{
public:
	map<string, vector<string> > following;
	map<string, vector<string> > followed;
	map<string, string> toId; 
	map<string, map<string,string> > information;
	void load(){
		// followed
		cerr << "[LOG] loading 'followed' cache." << endl;
		string dirname = "./followed/";
		for( auto filename : getFiles(dirname) ){
			ifstream ifs(dirname+filename);
			string tmp;
			vector<string> arr;
			while (getline(ifs, tmp)){
				arr.push_back(tmp);
			}
			sort(arr.begin(),arr.end());
			followed[toLower(filename)] = arr; // toLowerしたほうが統一感あってよい
		}
        // following
		cerr << "[LOG] loading 'following' cache." << endl;
        dirname = "./following/";
        for( auto filename : getFiles(dirname) ){
            ifstream ifs(dirname+filename);
            string tmp;
            vector<string> arr;
            while (getline(ifs, tmp)){
                arr.push_back(tmp);
            }
            sort(arr.begin(),arr.end());
            following[toLower(filename)] = arr;

        }
		// information
		cerr << "[LOG] loading 'information' cache." << endl;
		ifstream ifs(string("./info/information.txt").c_str());
		string tmp;
		while (getline(ifs, tmp)){
			string a, b,key,val;
			stringstream ss(tmp);
			ss >> a;
			while (ss >> b){
				b[b.find("=")] = ' ';
				stringstream ss(b);
				ss >> key >> val;
				if( key == "screen_name" ){
					val = toLower(val); // toLowerしたほうが統一感あってよい
					toId[val] = a;
				}
				information[a][key] = val;
			}
		}
	}
	void writeFollowed(string a, vector<string> b){
		sort(b.begin(),b.end());
		followed[a] = b;
		ofstream ofs(string("./followed/" + a).c_str());
		for (int i = 0; i < b.size(); i++)
			ofs << b[i] << endl;
		ofs.close();
	}
	void writeFollowing(string a, vector<string> b){
		sort(b.begin(),b.end());
		following[a] = b;
		ofstream ofs(string("./following/" + a).c_str());
		for (int i = 0; i < b.size(); i++)
			ofs << b[i] << endl;
		ofs.close();
	}
	void writeInfo(string a, map<string,string> info){
		info["__regtime__"] = itos(time(NULL));
		ofstream ofs(string("./info/information.txt").c_str(), ios::app);
		ofs << a;
		for (auto x : info){
			ofs << " " << x.first << "=" << x.second;
			if( x.first == "screen_name" ){
				x.second = toLower(x.second);
				toId[x.second] = a;
			}
			information[a][x.first] = x.second;
		}

		ofs << endl;
		ofs.close();
	}
};

class Controller{
public:
	string c_key, c_sec, t_key, t_sec;
	Cache* cache;
	Controller(string c_key, string c_sec, string t_key, string t_sec, Cache *cache) : c_key(c_key), c_sec(c_sec), t_key(t_key), t_sec(t_sec), cache(cache){}

	static string http_request(string url,string data,string header,bool post)
	{
		/*
		string cmd = "curl --get '" + url + "' --data '" + data + "' --header '" + header + "'";
		string res = Popen(cmd);
		return res;
		*/
		WebClient client;
		client.add_header(header.c_str());
		if( post == 0 ){
			URI uri(string(url+"?"+data).c_str());
			client.get(uri);
		}else{
			URI uri(string(url).c_str());
			WebClient::Post post;
			post.data.assign(data.begin(),data.end());
			client.post(uri,&post);
		}
		WebClient::Result const &res = client.result();
		return string(res.content.begin(),res.content.end());
	}
	string request(const string &url, const map<string, string> &data_,const bool post=false){
		map<string,string> data = data_;
		if (data.size() == 0) return "error";
		
		//url‚Égetƒwƒbƒ_‚ð’Ç‰Á‚µ‚½‚à‚Ìuri‚ð‚Â‚­‚é
		string getRequest = "";
		for (auto x : data)
			getRequest += "&" + oauth_url_escape(x.first.c_str()) + "=" + oauth_url_escape(x.second.c_str());
		getRequest = getRequest.substr(1);

		// oath_signature‚ðì‚é
		data["oauth_consumer_key"] = c_key;
		data["oauth_nonce"] = randomString(30);
		data["oauth_signature_method"] = "HMAC-SHA1";
		data["oauth_timestamp"] = itos(time(NULL));
		data["oauth_token"] = t_key;
		data["oauth_version"] = "1.0";
		string params = "";
		for (auto x : data)
			params += "&" + oauth_url_escape(x.first.c_str()) + "=" + oauth_url_escape(x.second.c_str());
		params = params.substr(1);
        
		string base = (post?"POST":"GET");
		base += "&";
		base += oauth_url_escape(url.c_str());
		base += "&";
		base += oauth_url_escape(params.c_str());
		string signingKey = c_sec + "&" + t_sec;
        
		string digest = encode(calcHash(signingKey, base));
		data["oauth_signature"] = digest;
		
		//ƒwƒbƒ_‚ðì‚é
		string header = "Authorization: OAuth ";
		int notFirst = 0;
		for (auto x : data){
			if (notFirst++) header += ", ";
			header += oauth_url_escape(x.first.c_str()) + "=\"" + oauth_url_escape(x.second.c_str()) + "\"";
		}
		cerr << "[LOG] Issue a query '" << url << "'" << endl;
		//cerr << getRequest << endl;
		string res = http_request(url, getRequest,header,post);
		if( res.find("Rate limit exceeded") != -1 ){ //この判定はやばいですよ
			cerr << "[LOG] Failed. Rate limit exceeded. I'll retry after 180 secs." << endl;
			sleep(180);
			return request(url,data_,post);
		}else{
			cerr << "[LOG] Succeeded." << endl;
			return res;
		}
	}
	vector<string> getFollowers(string user, bool is_screen_name = true,string cursor="-1"){
		user = toLower(user); // ユーザー名をtolowerしたほうがよい
		if (cache->followed.count(user)) return cache->followed[user];

		string res = request("https://api.twitter.com/1.1/followers/ids.json", { { (is_screen_name ? "screen_name" : "user_id"), user }, { "stringify_ids", "true" }, { "count", "5000" }, { "cursor", cursor } });

		try {
			auto value = ujson::parse(res);
			std::vector<std::pair<std::string, ujson::value>> object = ujson::object_cast(std::move(value));
			vector<string> ids;
			auto it = ujson::find(object, "ids");
            if( it == object.end() ) throw std::domain_error("unknown error");
            
			std::vector<ujson::value> array = ujson::array_cast(std::move(it->second));

			for (auto it = array.begin(); it != array.end(); ++it) {
				ids.push_back(ujson::string_cast(std::move(*it)));
			}
			auto it2 = ujson::find(object, "next_cursor_str");
            if( it2 == object.end() ) throw std::domain_error("unknown error");
            
			string next_cursor = ujson::string_cast(std::move(it2->second));
			if (next_cursor != "0" ){
				auto nextids = getFollowers(user, is_screen_name, next_cursor);
				ids.insert(ids.end(), nextids.begin(), nextids.end());
			}
			if (cursor == "-1" ){
				cache->writeFollowed(user, ids);
			}
			return ids;

		}
		catch (std::exception const &e) {
			std::cerr << e.what() << std::endl; // prints 'Invalid syntax on line 1.'
			std::cerr << "getFollowers(" + user + ")" << endl;
			std::cerr << "[JSON DATA]" << endl;
			std::cerr << res << endl;
			return {};
		}
	}
	vector<string> getFollowing(string user, bool is_screen_name = true,string cursor="-1"){
        user = toLower(user); // ユーザー名をtolowerしたほうがよい
		if (cache->following.count(user)) return cache->following[user];
		string res = request("https://api.twitter.com/1.1/friends/ids.json", { { (is_screen_name ? "screen_name" : "user_id"), user }, { "stringify_ids", "true" }, { "count", "5000" }, { "cursor", cursor } });
		try {
			auto value = ujson::parse(res);
			std::vector<std::pair<std::string, ujson::value>> object = ujson::object_cast(std::move(value));
			vector<string> ids;
			auto it = ujson::find(object, "ids");
            if( it == object.end() ) throw std::domain_error("unknown error");
            
			std::vector<ujson::value> array = ujson::array_cast(std::move(it->second));
			
			for (auto it = array.begin(); it != array.end(); ++it) {
				ids.push_back(ujson::string_cast(std::move(*it)));
			}
			auto it2 = ujson::find(object, "next_cursor_str");
            if( it2 == object.end() ) throw std::domain_error("unknown error");
            
			string next_cursor = ujson::string_cast(std::move(it2->second));
			if (next_cursor != "0"){
				auto nextids = getFollowing(user, is_screen_name, next_cursor);
				ids.insert(ids.end(), nextids.begin(), nextids.end());
			}
			if (cursor == "-1"){
				cache->writeFollowing(user, ids);
			}
			return ids;
			
		}
		catch (std::exception const &e) {
			std::cerr << e.what() << std::endl; // prints 'Invalid syntax on line 1.'
			std::cerr << "getFollowing(" + user + ")" << endl;
			std::cerr << "[JSON DATA]" << endl;
			std::cerr << res << endl;
			return {};
		}
	}


	string unicodeKiller(string s){
		string t;
		int skip = 0;
		for (int i = 0; i < s.size();i++){
			if (s[i] == '=') continue;
			if (s[i] == '@') continue;
			if (s[i] == '/') continue;
			if (s[i] == '\\' ){
				i++;
			}else{
				if( skip == 0 ) t += s[i];
			}
		}
		return t;
	}

	void cacheInformation(vector<string> id){
		vector<string> newid;
		for (int i = 0; i < id.size(); i++){
			if (!cache->information.count(id[i])){
				newid.push_back(id[i]);
			}
		}
		id = newid;
		
		if (id.size() == 0) return;
		if (id.size() > 100){
			cacheInformation(vector<string>(id.begin() + 100, id.end()));
			id.resize(100);
		}
		string users;
		for (auto x : id) users += "," + x;
		users = users.substr(1);

		string res = request("https://api.twitter.com/1.1/users/lookup.json", { { "user_id", users }, { "trim_user", "false" }, { "include_entities", "false" } },true);
		res = unicodeKiller(res);

		try {
			auto value = ujson::parse(res);
			std::vector<ujson::value> array = ujson::array_cast(std::move(value));
			for (auto x : array){
				std::vector<std::pair<std::string, ujson::value>> object = ujson::object_cast(std::move(x));
				auto it1 = ujson::find(object, "id_str");
                if( it1 == object.end() ) throw std::domain_error("unknown error");
                
				string id = ujson::string_cast(std::move(it1->second));
				pair<string, string> collect[] = { 
					{ "screen_name", "string" }, 
					{ "followers_count", "int" }, 
					{ "friend_count", "int" },
					{ "protected","bool"}
				};
				map<string, string> info;
				for (auto key : collect){
					auto it2 = ujson::find(object, key.first.c_str());
                    if( it2 == object.end() ) throw std::domain_error("unknown error");
                    
					string value;
					if (key.second == "int"){
						value = itos(ujson::double_cast(std::move(it2->second)));
					}
					else if (key.second == "string"){
						value = ujson::string_cast(std::move(it2->second));
					}else if( key.second == "bool"){
						value = ujson::bool_cast(std::move(it2->second)) ? "true" : "false";
					}
					else{
						throw;
					}
					info[key.first] = value;
				}
				cache->writeInfo(id,info); //ƒLƒƒƒbƒVƒ…
			}
			return;
		}
		catch (std::exception const &e) {
			std::cerr << e.what() << std::endl; // prints 'Invalid syntax on line 1.'
			std::cerr << "cacheInformation(" << "{" <<  users << "})" << endl;
			std::cerr << "[JSON DATA]" << endl;
			std::cerr << res << endl;
			return;
		}
		return;
	}
};

struct Data{
	set<string> data;
	bool notflag;
};
int abs(const Data &x){
	return x.data.size();
}
Data operator!(Data x);
Data operator*(Data a, Data b);
Data operator+(Data a, Data b);
Data operator-(Data a, Data b);
Data operator^(Data a, Data b);

Data operator!(Data x){
	return {x.data,!x.notflag};
}

Data operator+(Data a, Data b){
	if( a.notflag || b.notflag ){
		return !((!a)*(!b));
	}else{
		for (auto x : b.data) a.data.insert(x);
		return a;
	}
}

Data operator-(Data a, Data b);

Data operator*(Data a, Data b){
	if( a.notflag && b.notflag ){
		return !((!a)+(!b));
	}else if( a.notflag ){
		for (auto x : a.data) b.data.erase(x);
		return b;
	}else if( b.notflag ){
		for (auto x : b.data) a.data.erase(x);
		return a;
	}else{
		if( a.data.size() < b.data.size() ) a.data.swap(b.data);
		Data c = {set<string>(),false};
		for (auto x : b.data){
			if (a.data.count(x)) c.data.insert(x);
		}
		return c;
	}
}
Data operator-(Data a, Data b){
	return a * !(b);
}

Data operator^(Data a, Data b){
	return (a+b)-(a*b);
}
class Parser{
	/*
	<expr>   := <factor> {('+'|'-') <factor>}
	<factor> := <number> {'*' <number>}
	<number> := '(' <expr> ')' | string | function(<expr>)
	*/
	int pos;
	string str;
	Controller *ctrl;
	map<string, std::function<Data(Data)>> func;
public:
	Parser(string str, Controller *ctrl) : pos(0), str(str), ctrl(ctrl){
		func["follow"] = [](Data x){return x; };
	}
	vector<string> doParse(){
		pos = 0;
		try{
			auto res = expr();
			if (pos != str.size()) throw -3;
			if( res.notflag ) throw -114514; 
			return vector<string>(res.data.begin(),res.data.end());
		}
		catch(int x){
			if( x == -114514 ){
				cerr << "Error! answer is (!x) form. please limit the scope ;(" << endl;
			}else{
				cerr << "parse error code: " << x << endl;
			}
		}
		return{};
	}
	Data doParse2(){
		pos = 0;
		try{
			auto res = expr();
			if (pos != str.size()) throw -3;
			if( res.notflag ) throw -114514; 
			return res;
		}
		catch(int x){
			if( x == -114514 ){
				cerr << "Error! answer is (!x) form. please limit the scope ;(" << endl;
			}else{
				cerr << "parse error code: " << x << endl;
			}
		}
		return{set<string>(),false};
	}

	Data expr(){
		auto res = factor();
		while (str[pos] == '-' || str[pos] == '+' || str[pos] == '^' ){
			if (str[pos] == '+'){
				pos++;
				res = res + factor();
			}else if( str[pos] == '-' ){
				pos++;
				res = res - factor();
			}
			else{
				pos++;
				res = res ^ factor();
			}
		}
		return res;
	}
	Data factor(){
		auto res = number();
		while (str[pos] == '*'){
			pos++;
			res = res * number();
		}
		return res;
	}
	Data number(){
		if( str[pos] == '!' || str[pos] == '~' ){
			pos++;
			return !number();
		}else if (str[pos] == '('){
			pos++;
			auto res = expr();
			if ( str[pos] != ')' ) throw pos;
			pos++;
			return res;
		}
		else{
			auto name = word();
			if (name.size() == 0) throw -1;
			if (str[pos] == '('){
				if (name == "ing" || name == "following"){
					pos++;
					auto arg = word();
					if (str[pos] != ')') throw pos;
					pos++;
					auto got = ctrl->getFollowing(arg);
					return {set<string>(got.begin(),got.end()),false};
				}else if (name == "ed" || name == "followed"){
					pos++;
					auto arg = word();
					if (str[pos] != ')') throw pos;
					pos++;
					auto got= ctrl->getFollowers(arg);
					return {set<string>(got.begin(),got.end()),false};
				}
				else if (name == "both" || name == "b" ){
					pos++;
					auto arg = word();
					if (str[pos] != ')') throw pos;
					pos++;
					auto got1 = ctrl->getFollowers(arg);
					auto got2 = ctrl->getFollowing(arg);
					Data cGot1 = {set<string>(got1.begin(),got1.end()),false};
					Data cGot2 = {set<string>(got2.begin(),got2.end()),false};
					return cGot1*cGot2;
				}else if (name == "either" || name == "e" ){
					pos++;
					auto arg = word();
					if (str[pos] != ')') throw pos;
					pos++;
					auto got1 = ctrl->getFollowers(arg);
					auto got2 = ctrl->getFollowing(arg);
					Data cGot1 = {set<string>(got1.begin(),got1.end()),false};
					Data cGot2 = {set<string>(got2.begin(),got2.end()),false};
					return cGot1+cGot2;
				}else{
					if (!func.count(name)) throw - 2;
					auto res = number();
					return func[name](res);
				}
			}else{
				throw -4;
			}
		}
	}

	string word(){
		string name;
		const string table = "()+*-^!";
		while (str[pos] != 0 && table.find(str[pos]) == -1){
			name += str[pos++];
		}
		return name;
	}
};

void analyse(string screen_name,Controller *ctrl,double threshold,bool seek_protected,int analyse_mode,string filter,bool killOneDistance=true,bool cutMode=true){
	
	screen_name = toLower(screen_name);
	//filter = "(" + filter + ")";

	string cmd = analyse_mode == 0 ? "ing" : analyse_mode == 1 ? "ed" : analyse_mode == 2 ? "either" : "both";
	Cache *cache = ctrl->cache;
	vector<string> ids = Parser(cmd+"("+screen_name+")"+(filter==""?"":"*("+filter+")"),ctrl).doParse();
	ctrl->cacheInformation(ids);
	int unconsidered = 0;
	int considered = 0;
	
	//とりあえずクロールした中でフォロー情報リストがある人だけを対象にする
	for( auto &x : ids ) x = cache->information[x]["screen_name"];

	ids.erase( remove_if(ids.begin(),ids.end(),[&](string x){

		if( (cmd != "ed" && cache->following.count(x) == 0) || (cmd != "ing" && cache->followed.count(x)==0) ){
			cerr << "user '" << x << "' will not be considered." << endl;
			unconsidered++;
			return 1;
		}else{
			considered++;
			return 0;
		}
	}),ids.end());
	
	cerr << "Total " << considered << "/" << unconsidered+considered << " users will be considered." << endl;
	//フォローしてる人のフォローしてる人を全列挙．出現頻度でスコア付け
	vector<string> all = ids;
	map<string,int> cnt;
	if( cmd == "both"){
		for( auto id : ids ){
			set<string> tmp1(cache->following[id].begin(),cache->following[id].end());
			set<string> tmp2(cache->followed[id].begin(),cache->followed[id].end());
			set<string> tmpr = (Data{tmp1,false}*Data{tmp2,false}).data;
			all.insert(all.end(),tmpr.begin(),tmpr.end());	
		}
	}else{
		if( cmd != "ed"){
			for( auto id : ids )
				all.insert(all.end(),cache->following[id].begin(),cache->following[id].end());
		}
		if( cmd != "ing"){
			for( auto id : ids )
				all.insert(all.end(),cache->followed[id].begin(),cache->followed[id].end());
		}
	}
	for( auto x : all){ cnt[x]++; }
	sort(all.begin(),all.end());
	all.erase(unique(all.begin(),all.end()),all.end());

	Data cluster = {{cache->toId[screen_name]},false};    
	if(cutMode){
		// 距離2の人の列挙でこういうことをしてしまっているのどうなんやろ...積をとっているのはノイズ対策です．
		// とりあえず ids.size() 回ランダムにサンプリングします．つまりn*(n-1)/2回必要なところを1/n回に抑えてるってワケ
		// 数学的な裏付けがあるわけではない
		for(int i = 0 ; i < ids.size() ; i++){
			string A = ids[rand()%ids.size()];
			string B = ids[rand()%ids.size()];
			if( A == B ) continue;
			string query = cmd+"("+A+")"+"*"+cmd+"("+B+")";
			cluster = cluster + Parser(query,ctrl).doParse2();
		}
	}else{
		//正直カットしない意味はないです
		for(int i = 0 ; i < ids.size() ; i++){
			string A = ids[i];
			string query = cmd+"("+A+")";
			cerr << A << " " << ids[i] << endl;
			cluster = cluster + Parser(query,ctrl).doParse2();
		}
    }

	auto me = Parser(cmd+"("+screen_name+")",ctrl).doParse2();
	if( killOneDistance ) cluster = cluster - me; //距離1の人々を除く
    
	// 距離2の人のスコアを計算 スコアは∑(その人をフォローしてる距離1の人のスコア)
	map<string,int> score;
	for( auto x : me.data ){
		string id = cache->information[x]["screen_name"];
		int weight = cnt[x];
		for( auto y : cache->following[id] ) {
			if( cluster.data.count(y) ){
				score[y] += weight;
			}
		}
	}
	vector<string> lst(cluster.data.begin(),cluster.data.end());

	ctrl->cacheInformation(lst);
	auto eval = [&](string x){
		return 100. * score[x] / score[cache->toId[screen_name]];
	};
	lst.erase( remove_if(lst.begin(),lst.end(),[&](string a){
		return eval(a) < threshold;
	}),lst.end());
	sort(lst.begin(),lst.end(),[&](string a,string b){ return eval(a) > eval(b); });
	int skipCount = 0;
	for (auto x : lst){
		if( seek_protected && cache->information[x]["protected"] == "false"){
			skipCount++;
			continue;
		}

		int flag = 0;
		flag |= 1*(int)binary_search(cache->following[screen_name].begin(),cache->following[screen_name].end(),x);
		flag |= 2*(int)binary_search(cache->followed[screen_name].begin(),cache->followed[screen_name].end(),x);
		if( flag == 0 ){
			printf("     X");
		}else if( flag == 1 ) {
			printf("    =>");
		}else if( flag == 2){
			printf("    <=");
		}else{
			printf("   <=>");
		}
		printf("%16s %10s:%6s %10s:%6s %13s:%10s %10s:%6s %6s:%.2lf%%\n"
			,cache->information[x]["screen_name"].c_str()
			,"follower"
			,cache->information[x]["followers_count"].c_str()
			,"follow"
			,cache->information[x]["friend_count"].c_str()
			,"cached_time"
			,cache->information[x]["__regtime__"].c_str()
			,"protected"
			,cache->information[x]["protected"].c_str()
			,"score"
			,eval(x)
		);
	}
	cout << "Total: " << lst.size() - skipCount << " users (Threshold score: not less than " << threshold << "%)" << endl;
	return;
}

struct Edge{
	int src,dst;
};

void specify(string screen_name,Controller *ctrl,bool seek_protected){
	screen_name = toLower(screen_name);
	Cache *cache = ctrl->cache;
	if( cache->toId.count(screen_name) == 0 ){
		cerr << "Lack of information 1." << endl;
		return;
	}else{
		string id = cache->toId[screen_name];
		vector<string> lst;
		for( auto x : cache->following ){
			if( binary_search(x.second.begin(),x.second.end(),id) ){
				lst.push_back(cache->toId[x.first]);
			}
		}
		for( auto x : cache->followed ){
			if( binary_search(x.second.begin(),x.second.end(),id) ){
				lst.push_back(cache->toId[x.first]);
			}
		}
		sort(lst.begin(),lst.end());
		lst.erase(unique(lst.begin(),lst.end()),lst.end());

		//ユーザーについての偽キャッシュもついでに生成する．これは記録されない．
		int skipCount = 0;
		for (auto x : lst){
			string X = cache->information[x]["screen_name"];
			if( seek_protected && cache->information[x]["protected"] == "false"){
				skipCount++;
				continue;
			}

			int flag = 0;
			flag |= 1*(int)binary_search(cache->followed[X].begin(),cache->followed[X].end(),id);
			flag |= 2*(int)binary_search(cache->following[X].begin(),cache->following[X].end(),id);
			if(flag&1)cache->following[screen_name].push_back(x);
			if(flag&2)cache->followed[screen_name].push_back(x); 
			if( flag == 0 ){
				printf("     X");
			}else if( flag == 1 ) {
				printf("    =>");
			}else if( flag == 2){
				printf("    <=");
			}else{
				printf("   <=>");
			}
			printf("%16s %10s:%6s %10s:%6s %13s:%10s %10s:%6s\n"
				,cache->information[x]["screen_name"].c_str()
				,"follower"
				,cache->information[x]["followers_count"].c_str()
				,"follow"
				,cache->information[x]["friend_count"].c_str()
				,"cached_time"
				,cache->information[x]["__regtime__"].c_str()
				,"protected"
				,cache->information[x]["protected"].c_str()
			);
		}
		cout << "Total: " << lst.size() - skipCount << " users were specified." << endl;
		cerr << "[LOG] Make an approximated ing/ed list of '" << screen_name << "'." << endl;
		return;
	}

}


int main(int argc, char **argv)
{

	if (argc < 2){
		cerr << "need an expression." << endl;
		cerr << "!X / ~X := not X" << endl;
		cerr << "A+B     := A or B" << endl;
		cerr << "A-B     := A and !B" << endl;
		cerr << "A*B     := A and B" << endl;
		cerr << "A^B     := A xor B" << endl;
		cerr << "ing(X)    := users followed by X" << endl;
		cerr << "ed(X)     := users following X" << endl;
		cerr << "both(X)   := users followed by X and following X" << endl;
		cerr << "either(X) := users followed by X or following X" << endl;
		cerr << "@Ex. ~(~both(user1)^both(user2))*either(user3)" << endl;

		cerr << "[Special Argument]" << endl;
		cerr << "--specify          : specify an user from information which you have." << endl;          
		cerr << "--protected/--key  : list only protected account." << endl;
		cerr << "--specialFollowing : list people who you should follow but you don't follow now." << endl;
		cerr << "--specialFollowed  : list people who you should be followed but you aren't followed by now." << endl;
		cerr << "--specialEither    : list people who you should have any connection with but you don't have." << endl;
		cerr << "--specialBoth      : list people who you should have <=> connection with but you don't have." << endl;
		cerr << "--threshold=X      : set threshold to X on the special listing mode(Default:X=1)." << endl;
		cerr << "--filter=Y         : limit the scope on the special listing mode." << endl;
		cerr << "--reflesh=Y        : reflesh following/followed information." << endl;
		
		return 1;
	}
	// キャッシュ読み込み
	Cache cache;
	cache.load();
	WebClient::initialize();
    fstream ifs("./keys/mykey.txt");
	if (!ifs.is_open()){
		cerr << "\"./keys/mykey.txt\" is not found." << endl;
		return 1;
	}
	string tmp;
	map<string, string> keys;
	while (getline(ifs, tmp)){
		stringstream ss(tmp);
		string a, b;
		ss >> a >> b;
		keys[a] = b;
	}
	if (keys.count("c_key") == 0 || keys.count("t_key") == 0 || keys.count("t_sec") == 0 || keys.count("c_sec") == 0){
		cerr << "some contents of mykey.txt are not found." << endl;
		return 1;
	}
	Controller ctrl(keys["c_key"], keys["c_sec"], keys["t_key"], keys["t_sec"], &cache);

	string query = "";
	bool seek_protected = false;
	double threshold = 0.01;
	bool killOneDistance = true;
	bool cut = true;
	string specifyuser = "";
	int analyse_mode = -1;
	string filter="";
	for(int i = 1 ; i < argc ; i++){
		if( argv[i][0] == '-' && argv[i][1] == '-' ){
			if( string(argv[i]+2).find("threshold=") == 0 ){
				threshold = atof(argv[i]+2+10);
			}else if( string(argv[i]+2) == "protected" || string(argv[i]+2) == "key" ){
				seek_protected = true;
			}else if( string(argv[i]+2).find("special") == 0 ){
				if( analyse_mode != -1 ){
					cerr << "too many arguments" << endl;
					return 1;
				}
				if( string(argv[i]+2) == "specialFollowing" ){
					analyse_mode = 0;
				}else if( string(argv[i]+2) == "specialFollowed" ){
					analyse_mode = 1;
				}else if( string(argv[i]+2) == "specialEither" ){
					analyse_mode = 2;
				}else if( string(argv[i]+2) == "specialBoth" ){
					analyse_mode = 3;
				}else{
					cerr << "option '" << string(argv[i]) << "' is unknown." << endl;						
				}
			}else if( string(argv[i]+2).find("filter=") == 0 ){
				if( filter != "" ){
					cerr << "too many arguments" << endl;
					return 1;
				}
				filter = string(argv[i]+2+7);
			}else if( string(argv[i]+2) == "specify" ){
				specifyuser = "@empty@";
			}else if( string(argv[i]+2).find("specify=") == 0 ){
				specifyuser = string(argv[i]+2+8);
				specify(specifyuser,&ctrl,seek_protected);
				specifyuser = "";
			}else if( string(argv[i]+2).find("stopKOD") == 0 ){ //裏コマンド
				killOneDistance = false;
			}else if( string(argv[i]+2).find("stopCut") == 0 ){ //裏コマンド
				cut = false;
			}else if( string(argv[i]+2).find("reflesh=") == 0 ){	
				string sc_name = string(argv[i]+2+8);
				cache.following.erase(sc_name);
				cache.followed.erase(sc_name);
				cerr << "information erased [" << sc_name << "]" << endl;
				ctrl.getFollowers(sc_name);
				ctrl.getFollowing(sc_name);
			}else{
				cerr << "option '" << string(argv[i]) << "' is unknown." << endl;
			}
		}else{
			if( query == "" ) query = argv[i];
			else {
				cerr << "too many arguments" << endl;
				return 1;
			}
		}
	}
	/*
	if( specify_mode && analyse_mode != -1 ){
		cerr << "more than 1 modes are selected." << endl;
		return 0;
	}
	*/

	// specify modeとアナライズモードだけは両立できる設定
	if( specifyuser != "" ){
		if( specifyuser == "@empty@" ) specifyuser = query;
		specify(specifyuser,&ctrl,seek_protected);
		if( analyse_mode == -1 ){
			return 1;
		}
	}
	if( query == "" ){
		cerr << "Error: Query is empty." << endl;
		return 1;
	}

	if( analyse_mode != -1 ){
		analyse(query,&ctrl,threshold,seek_protected,analyse_mode,filter,killOneDistance,cut);
		return 0;	
	}


	// normal_mode
	auto result = Parser(query, &ctrl).doParse();
	ctrl.cacheInformation(result);
	sort(result.begin(), result.end(), [&](const string &a, const string &b){
			return atoi(cache.information[a]["followers_count"].c_str()) < atoi(cache.information[b]["followers_count"].c_str());
	});

	int skipCount = 0;
	for (auto x : result){
		if( seek_protected && cache.information[x]["protected"] == "false"){
			skipCount++;
			continue;
		}
		printf("%16s %10s:%6s %10s:%6s %13s:%10s %10s:%6s\n"
			,cache.information[x]["screen_name"].c_str()
			,"follower"
			,cache.information[x]["followers_count"].c_str()
			,"follow"
			,cache.information[x]["friend_count"].c_str()
			,"cached_time"
			,cache.information[x]["__regtime__"].c_str()
			,"protected"
			,cache.information[x]["protected"].c_str()
		);
	}
	cout << "Total: " << result.size() - skipCount << " users" << endl;
	return 0;
}