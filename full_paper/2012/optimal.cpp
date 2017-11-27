#include <math.h>
#include <iostream>
#include <map>
#include <queue>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <utility>
#include <iterator>
#include <indri/QueryEnvironment.hpp>
#include <indri/XMLReader.hpp>
#include <indri/SnippetBuilder.hpp>
#include <indri/KrovetzStemmer.hpp>
#include <indri/QueryExpander.hpp>
#include <indri/RMExpander.hpp>
#include <indri/ParsedDocument.hpp>
#include <unordered_map>

struct filter_by_threshold {
        std::vector<std::string> filtered_doc_no;
        int threshold;
        void operator () (std::pair<std::string, int> x)
        {
                if (x.second>=threshold) {
                        filtered_doc_no.push_back(x.first);
                }
        }
};

struct greater_value {
        bool operator () (const std::pair<std::string, double> &x, const std::pair<std::string, double> &y) const
        {
                return x.second<y.second;
        }
};

lemur::api::DOCID_T doc_id_of(const indri::api::ScoredExtentResult& r)
{
        return r.document;
}

void print_result(indri::api::ScoredExtentResult r)
{
        cout << r.document << "\t" << r.score << endl;
}

void print_usage(char *prog)
{
        cout << "Usage:";
        cout << prog << " -session=<session file> -index=<index path> -stopper=<stopword list file>" << endl;
}

void print_help()
{
        cout << endl;
}

std::vector<std::string> split_query(const std::string &query)
{
        std::stringstream query_stream(query);
        std::string part;
        std::getline(query_stream, part, ')');
}

struct interaction {
        std::vector<std::string> docno;
        std::vector<std::string> url;
        std::vector<std::string> title;
        std::vector<std::string> snippet;
        std::vector<double> clicked;
        void write (ostream &oo)
        {
                int size = docno.size();
                for (int i=0; i<size; ++i) {
                        oo << "\t" << docno[i] << endl;
                        oo << "\t" << url[i] << endl;
                        oo << "\t" << title[i] << endl;
                        oo << "\t" << snippet[i] << endl;
                }
        }
};

double ttod(std::string time)
{
        std::stringstream time_stream(time);
        double second = 0;
        string word;
        int found = time.find(":");
        if (found!=std::string::npos) {
             std::getline(time_stream, word, ':');
             second += atof(word.c_str()) * 3600;
             std::getline(time_stream, word, ':');
             second += atof(word.c_str()) * 60;
             std::getline(time_stream, word, ':');
             second += atof(word.c_str());
        }else{
             std::getline(time_stream, word);
             second += atof(word.c_str());
        }
        
        return second;
}

class SessionParser {
public:
         int relExplore, relExploit, nonrelExplore, nonrelExploit;
         std::map<std::string, std::map<std::string, int> > transMap;
         std::vector<std::string> stopwords;
         std::map<std::string, double> transSum;
         std::vector<int> sessionFilter;
         indri::query::QueryExpander* expander;

         //for POMDP
         std::map<std::string, double> allStatusBlief; //status key : status blief
         std::map<std::string, std::map<std::string, double> > transitionMap; //action key : transition key : probability
         std::vector<std::string> userActions;
         std::vector<std::string> seActions;
         std::vector<std::string> statuses;
         std::vector<double> degrees;
         std::vector<double> dDegrees;
         int fbDocs;
         std::map<std::string, std::vector<indri::api::ScoredExtentResult> > rsltMap; //session and step index, result

         //for reward function
         std::string grdtruthFile;
         std::map<int, std::map<std::string, int> > qrelMap; //topic id, docno, relevance grade
         std::string topicFile;
         std::map<int, int> topicMap; //session id : topic id
         std::string runid;

         void init(){
             //set b0
             allStatusBlief["NonRelExploit"] = 0.1916;
             allStatusBlief["NonRelExplore"] = 0.4981;
             allStatusBlief["RelExploit"] = 0.1245;
             allStatusBlief["RelExplore"] = 0.1858;

             //set statuses
             statuses.push_back("NonRelExploit");
             statuses.push_back("NonRelExplore");
             statuses.push_back("RelExploit");
             statuses.push_back("RelExplore");

             //set user actions
             userActions.push_back("Add"); //add terms
             userActions.push_back("Delete"); //delete terms
             userActions.push_back("Keep"); //keep terms

             //set search engine actions
	         seActions.push_back("FullAllQuery");
             seActions.push_back("AllQuery");
             seActions.push_back("IncreaseNew");
             seActions.push_back("DecreaseNew");
             seActions.push_back("FeedBack");
	         seActions.push_back("Adhoc");
	         seActions.push_back("EqualAllQuery");

             //set increase degrees
             degrees.push_back(1.05); //0.952
             degrees.push_back(1.10); //0.9
             degrees.push_back(1.15); //0.87
             degrees.push_back(1.20); //0.83
             degrees.push_back(1.25); //0.8
             degrees.push_back(1.5); //0.67
             degrees.push_back(1.75); //0.57
             degrees.push_back(2); //0.5

             //set decrease degree
             dDegrees.push_back(1.05); //0.952
             dDegrees.push_back(1.10); //0.9
             dDegrees.push_back(1.15); //0.87
             dDegrees.push_back(1.20); //0.83
             dDegrees.push_back(1.25); //0.8
             dDegrees.push_back(1.5); //0.67
             dDegrees.push_back(1.75); //0.57
             dDegrees.push_back(2); //0.5

             //input groundtruth
             std::ifstream qrel_file(grdtruthFile.c_str());
             int sessionID, topicID,subjectID,relDegree;
             std::string docno;
             while (qrel_file >> topicID >> subjectID >> docno >> relDegree) {
                    if(relDegree > 0){
                       qrelMap[topicID][docno] = relDegree;
                    }
             }
             //input topic mapping file
             std::ifstream topic_file(topicFile.c_str());
             std::string product, goal, type;
             while (topic_file >> sessionID >> topicID >> subjectID >> product >> goal >> type) {
                    topicMap[sessionID] = topicID;
             }
               
         }

         double Reward(std::vector<std::string> & result, int cutoff, int sessionID){
             double score = 0;
             int topicID = topicMap[sessionID];//start from 1
             std::map<std::string, int> & qrelSession = qrelMap[topicID];
             
             //dcg
             std::map<std::string, int>::iterator it;
             for(int i=0; i<result.size() && i< cutoff; i++){
                 std::string docno = result[i];
                 it = qrelSession.find(docno);
                 if(it!=qrelSession.end()){
                    score += (pow(2, it->second) - 1) * log(2)/ log(2+i) ;
                 }
             }

             //no need to normalize

             return score;
         }


         //blief update, session q_i-1 information and q_i-2
         void bliefUpdate(interaction & interRslt, std::string preQuery){ //preQuery is used to determine which action bring machine to status_i-1
               
         }

         //for one step, here we only have one step
         std::vector<indri::api::ScoredExtentResult> loopSEValue(int sessionID, int step, std::string & finalAction){ //step start with 1
                  double maxValue = 0;
                  std::string optAction = "";
                  std::vector<std::string> optRslt;
                  std::string optQuery="";

                  std::string c_query = current_query[sessionID];
                  std::vector<std::string> &p_query = past_query[sessionID];
                  if(step > p_query.size() + 1){
                      step = p_query.size() + 1;
	              }
                  std::ostringstream convert;
                  convert << sessionID << "&sep;" << step;      // session id starts from 0, step starts from 1
                  std::string key = convert.str();
                  std::map<std::string, std::vector<indri::api::ScoredExtentResult> >::iterator itRslt = rsltMap.find(key);
                  if(itRslt != rsltMap.end()){
                     return rsltMap[key];
                  }

                  double value = 0;
                  std::vector<indri::api::ScoredExtentResult> result;

                  for(int i=0;i<seActions.size();i++){
                      std::string actionName = seActions[i];
                      if(actionName == "IncreaseNew"){
                         for(int j=0;j<degrees.size();j++){
                             double degree = degrees[j];

                             std::string q;
                             result = doSEAction(sessionID, actionName, degree, q, step);
                             std::vector<std::string> result_doc_no;
                             result_doc_no = environment.documentMetadata(result, "docno");
                             double r = Reward(result_doc_no,10,sessionID+1);

                             value = 0;
                             for(int m=0;m<statuses.size();m++){
                                std::string statusName = statuses[m];
                                double blief = allStatusBlief[statusName];
                                value += blief*r; 
                             }

                             if(value > maxValue){
                                maxValue=value;
                                std::ostringstream convert;
                                convert << degree;
                                optAction = actionName + " " + convert.str();
                                optRslt = result_doc_no;
                                optQuery = q;
                             }

                             //initiative status
                             if(value == maxValue && maxValue == 0 && optQuery=="" && optAction == ""){
                                maxValue=value;
                                std::ostringstream convert;
                                convert << degree;
                                optAction = actionName + " " + convert.str();
                                optRslt = result_doc_no;
                                optQuery = q;
                             }
                         }
                      }


                      if(actionName == "DecreaseNew"){
                         for(int j=0;j<dDegrees.size();j++){
                             double degree = dDegrees[j];

                             std::string q;
                             result = doSEAction(sessionID, actionName, degree, q, step);
                             std::vector<std::string> result_doc_no;
                             result_doc_no = environment.documentMetadata(result, "docno");
                             double r = Reward(result_doc_no,10,sessionID+1);

                             value = 0;
                             for(int m=0;m<statuses.size();m++){
                                std::string statusName = statuses[m];
                                double blief = allStatusBlief[statusName];
                                value += blief*r; 
                             }

                             if(value > maxValue){
                                maxValue=value;
                                std::ostringstream convert;
                                convert << degree;
                                optAction = actionName + " " + convert.str();
                                optRslt = result_doc_no;
                                optQuery = q;
                             }

                             //initiative status
                             if(value == maxValue && maxValue == 0 && optQuery=="" && optAction == ""){
                                maxValue=value;
                                std::ostringstream convert;
                                convert << degree;
                                optAction = actionName + " " + convert.str();
                                optRslt = result_doc_no;
                                optQuery = q;
                             }
                         }
                      }

                      if(actionName == "FullAllQuery"){
                         std::string q;
                         result = doSEAction(sessionID, actionName, -1, q,step);
                         std::vector<std::string> result_doc_no;
                         result_doc_no = environment.documentMetadata(result, "docno");

                         double r = Reward(result_doc_no,10,sessionID + 1);

                         value = 0;                             
                         for(int m=0;m<statuses.size();m++){
                             std::string statusName = statuses[m];
                             double blief = allStatusBlief[statusName];
                             value += blief*r; 
                         }

                         if(value > maxValue){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }

                         //initiative status
                         if(value == maxValue && maxValue == 0 && optQuery=="" && optAction == ""){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }
                      }

                      if(actionName == "AllQuery"){
                         std::string q;
                         result = doSEAction(sessionID, actionName, -1, q, step);
                         std::vector<std::string> result_doc_no;
                         result_doc_no = environment.documentMetadata(result, "docno");

                         double r = Reward(result_doc_no,10,sessionID + 1);

                         value = 0;                             
                         for(int m=0;m<statuses.size();m++){
                             std::string statusName = statuses[m];
                             double blief = allStatusBlief[statusName];
                             value += blief*r; 
                         }

                         if(value > maxValue){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }

                         //initiative status
                         if(value == maxValue && maxValue == 0 && optQuery=="" && optAction == ""){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }
                      }

                      if(actionName == "Adhoc"){
                         std::string q;
                         result = doSEAction(sessionID, actionName, -1, q, step);
                         std::vector<std::string> result_doc_no;
                         result_doc_no = environment.documentMetadata(result, "docno");

                         double r = Reward(result_doc_no,10,sessionID + 1);

                         value = 0;                             
                         for(int m=0;m<statuses.size();m++){
                             std::string statusName = statuses[m];
                             double blief = allStatusBlief[statusName];
                             value += blief*r; 
                         }

                         if(value > maxValue){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }

                         //initiative status
                         if(value == maxValue && maxValue == 0 && optQuery=="" && optAction == ""){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }
                      }

                      if(actionName == "EqualAllQuery"){
                         std::string q;
                         result = doSEAction(sessionID, actionName, -1, q, step);
                         std::vector<std::string> result_doc_no;
                         result_doc_no = environment.documentMetadata(result, "docno");

                         double r = Reward(result_doc_no,10,sessionID + 1);

                         value = 0;                             
                         for(int m=0;m<statuses.size();m++){
                             std::string statusName = statuses[m];
                             double blief = allStatusBlief[statusName];
                             value += blief*r; 
                         }

                         if(value > maxValue){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }

                         //initiative status
                         if(value == maxValue && maxValue == 0 && optQuery=="" && optAction == ""){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }
                      }

                      if(actionName == "FeedBack"){
                         std::string q;
                         result = doSEAction(sessionID, actionName, -1, q , step);
                         std::vector<std::string> result_doc_no;
                         result_doc_no = environment.documentMetadata(result, "docno");

                         double r = Reward(result_doc_no,10,sessionID + 1);

                         value = 0;                             
                         for(int m=0;m<statuses.size();m++){
                             std::string statusName = statuses[m];
                             double blief = allStatusBlief[statusName];
                             value += blief*r; 
                         }

                         if(value > maxValue){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }

                         //initiative status
                         if(value == maxValue && maxValue == 0 && optQuery=="" && optAction == ""){
                            maxValue=value;
                            optAction = actionName + " -1";
                            optRslt = result_doc_no;
                            optQuery = q;
                         }
                      }
                  }//end of for

cout << "max score:" << maxValue << endl;
cout << "session " << sessionID+1 << ". final action: " <<  optAction << ". final query: " << optQuery << endl;

                  std::vector<indri::api::ScoredExtentResult> finalresult;
                  if(optQuery!=""){

        			if(optAction =="FullAllQuery -1"){
        				finalresult = environment.runQuery(optQuery,2000);
        			}else{
                        finalresult = environment.runQuery(optQuery, filtered_doc_id, 2000);
        			}
                  }
		          finalAction = optAction;

                  return finalresult;
         }

         std::vector<indri::api::ScoredExtentResult> doSEAction(int sessionID, std::string actionName, double degree , std::string & finalQuery , int step){
               std::string query;
               int i = sessionID; //start from 0

               std::string c_query = current_query[i];
               std::vector<std::string> &p_query = past_query[i];
               std::vector<struct interaction> &p_result = past_result[i];
               if(step > p_query.size() + 1){
                   step = p_query.size() + 1;
	           }
               
    	       if(step <= p_query.size()){
    		      c_query = p_query[step-1];
    	       }  

               if(actionName == "IncreaseNew"){
                   std::map<std::string, double> current_weight = RL0_expand(c_query);
                   std::map<std::string, double> last_weight;
    		       if(step > 1){
    			      if(step > p_query.size()){
    			         last_weight = RL0_expand(p_query[p_query.size()-1]);
    			      }else{
    			         last_weight = RL0_expand(p_query[step -2]);
    			      }
    		       }
                   std::map<std::string, double>::iterator it;
                   std::set<std::string> buffer_occurred;
                   std::set<std::string> buffer_new;
                   std::set<std::string> buffer_canceled;
                   for (it = current_weight.begin(); it != current_weight.end(); ++it) {
                           if ( last_weight.count(it->first) ) {
                                   buffer_occurred.insert(it->first);
                           } else {
                                   buffer_new.insert(it->first);
                           }
                   }
                   for (it = last_weight.begin(); it != last_weight.end(); ++it) {
                           if ( !current_weight.count(it->first) ) {
                                   buffer_canceled.insert(it->first);
                           }
                   }

                   std::set<std::string>::iterator it2;
                   for (it2 = buffer_new.begin(); it2 != buffer_new.end(); ++it2) {
                        current_weight[*it2] *= degree;
                   }

                   query = "#weight(" + build_query(1,current_weight) + ")";
               }

        	   if(actionName == "Adhoc"){
        			std::map<std::string, double> current_weight = RL0_expand(c_query);
        			std::stringstream result_stream;
        			std::map<std::string, double>::iterator it;
        	        for (it = current_weight.begin(); it != current_weight.end(); ++it) {
                         result_stream << it->first << " ";
                    }

        			query = result_stream.str();
        	    }

        		if(actionName == "EqualAllQuery"){
        			query = get_all_query(i, step);
        		}

                if(actionName == "DecreaseNew"){
                       std::map<std::string, double> current_weight = RL0_expand(c_query);
                       std::map<std::string, double> last_weight;
        		       if(step > 1){
        			      if(step > p_query.size()){
        			         last_weight = RL0_expand(p_query[p_query.size()-1]);
        			      }else{
        			         last_weight = RL0_expand(p_query[step -2]);
        			      }
        		       } 
                       std::map<std::string, double>::iterator it;
                       std::set<std::string> buffer_occurred;
                       std::set<std::string> buffer_new;
                       std::set<std::string> buffer_canceled;
                       for (it = current_weight.begin(); it != current_weight.end(); ++it) {
                               if ( last_weight.count(it->first) ) {
                                       buffer_occurred.insert(it->first);
                               } else {
                                       buffer_new.insert(it->first);
                               }
                       }
                       for (it = last_weight.begin(); it != last_weight.end(); ++it) {
                               if ( !current_weight.count(it->first) ) {
                                       buffer_canceled.insert(it->first);
                               }
                       }

                       std::set<std::string>::iterator it2;
                       for (it2 = buffer_new.begin(); it2 != buffer_new.end(); ++it2) {
                                current_weight[*it2] /= degree;
                       }

                       query = "#weight(" + build_query(1,current_weight) + ")";
                }
                   

                if(actionName == "AllQuery"){
                       query = get_RL4_query(i,step);
                }

                if(actionName == "FullAllQuery"){
                       query = get_RL4_query(i, step);
                }

                std::vector<indri::api::ScoredExtentResult> result;
                if(actionName == "FeedBack"){
                       query = get_all_query(i, step);
            		   std::vector<int> list = get_sat_docid_list(i, step);
    			       result = environment.runQuery(query, list, fbDocs);
                       query = expander->expand(query, result);
                }

               result.clear();
               finalQuery = query;
		       if(actionName == "FullAllQuery"){
                       result = environment.runQuery(query,10);
               }else{
		               result = environment.runQuery(query,filtered_doc_id,10);
		       }   
               return result;
        }

        SessionParser(std::string session_file, indri::api::Parameters &p, std::vector<lemur::api::DOCID_T> &ws) : param(p), filtered_doc_id(ws)
        {
                flag_current_query = false;
                std::ifstream xml_file(session_file.c_str());
                std::string content;
                std::string buffer;
                relExplore = 0;
                relExploit = 0;
                nonrelExplore = 0;
                nonrelExploit = 0;
                transMap.clear();

                while (getline(xml_file, buffer)) content.append(buffer);
                root = xml.read(content);
                parse_xml();

                std::string index = param.get("index");
                grdtruthFile = param.get("qrel", "");
                topicFile = param.get("topic", "");
                runid = param.get("runid", "");
                environment.addIndex(index);
                //environment.addIndex("/data1/index/SessionSearchIndex");
                simpleEnvironment.addIndex(index);
                //simpleEnvironment.addIndex("/data1/index/SessionSearchIndex");
                std::vector<std::string> rule;
                if (param.exists("rule")) {
                        indri::api::Parameters slice = param["rule"];
                        for (int i=0; i<slice.size(); ++i) {
                                rule.push_back(slice[i]);
                        }
                        environment.setScoringRules(rule);
                        simpleEnvironment.setScoringRules(rule);
                }
                std::vector<std::string> stopwords;
                if (param.exists("stopper")) {
                        std::string stopper = param.get("stopper");
                        std::ifstream stopper_file(stopper.c_str());
                        std::string stopword;
                        while (stopper_file>>stopword) {
                                stopwords.push_back(stopword);
                        }
                        environment.setStopwords(stopwords);
                        simpleEnvironment.setStopwords(stopwords);
                }
                past_weight = param.get("past", 0.5);
                anchor_weight = param.get("anchor", 0.3);
                alpha = param.get("alpha", 1.0);
                beta = param.get("beta", 1.0);
                gamma = param.get("gamma", 1.0);
                int fbDocs=10;
                expander = 0;
                if (param.exists("fbDocs")) {
                        fbDocs = param.get("fbDocs");
                        expander = new indri::query::RMExpander(&simpleEnvironment, param);
                }

                update_anchor_list_sorted();
                init_correct_table();

                init();
        }

        ~SessionParser() {
                delete expander;
         }

        void init_correct_table()
        {
                correct_table["Pseudoscyesis"] = "Pseudocyesis";
                correct_table["cuo"] = "cup";
                correct_table["Philidelphia"] = "Philadelphia";
                correct_table["Arther"] = "Arthur";
                correct_table["lobbists"] = "lobbyists";
                correct_table["preganancy"] = "pregnancy";
                correct_table["servering"] = "severing";
                correct_table["consequenses"] = "consequences";
                correct_table["condiser"] = "consider";
                correct_table["diease"] = "disease";
                correct_table["coinds"] = "coins";
                correct_table["effecs"] = "effects";
                correct_table["idead"] = "ideas";
                correct_table["goverment"] = "government";
                correct_table["healty"] = "healthy";
                correct_table["ingridients"] = "ingredients";
                correct_table["sillicon"] = "silicon";
                correct_table["assement"] = "assessment";
                correct_table["teache"] = "teacher";
                correct_table["maijuana"] = "marijuana";
                correct_table["contolled"] = "controlled";
                correct_table["tean"] = "team";
        }

        std::string get_current_query(int id)
        {
                return current_query[id];
        }

        int get_session_num(int id)
        {
                return sessionIDs[id];
        }

        int get_session_number()
        {
                return current_query.size();
        }

        std::string build_query(double weight, const std::map<std::string, double> &term_weight)
        {
                std::map<std::string, double>::const_iterator it = term_weight.begin();
                std::stringstream result_stream;
                result_stream << " " << weight << " #weight(";
                for (it = term_weight.begin(); it != term_weight.end(); ++it) {
                        result_stream << " " << it->second << " " << it->first << " ";
                }
                result_stream << ") ";
                return result_stream.str();
        }

         int getMaxStep(){
                   int maxStep = 0;
                   for(int i=0;i<past_query.size();i++){
                       std::vector<std::string> &p_query = past_query[i];
                       if(p_query.size() + 1 > maxStep){
                          maxStep = p_query.size() + 1;
                       }
                   }
                   return maxStep;
         }

        void remove_duplicate(int id, std::vector<bool> &cancel, int step)
        {
                std::string c_query = current_query[id];
                std::vector<std::string> &p_query = past_query[id];
                std::vector<struct interaction> &p_result = past_result[id];
                int size = p_query.size();
                cancel.resize(size+1, false);
               
	         if(step <= p_query.size()){
		      c_query = p_query[step-1];
	         }

                for (int i = 0; i < size && i < step - 1; ++i) {
                        if ( same(p_query[i], c_query) ) {
                                for (int k = 0; k < size && k < step - 1; ++k) cancel[k] = true;
                                break;
                        }
                        for (int j = i+1; j < size && j < step - 1; ++j) {
                                if ( !cancel[i] && same(p_query[i], p_query[j]) ) {
                                        for (int k = i; k <= j && k < step - 1; ++k) cancel[k] = true;
                                }
                        }
                }
        }

        std::string get_RL4_query(int id, int step)
        {
            std::vector<bool> cancel;
            remove_duplicate(id, cancel, step);
            std::string c_query = current_query[id];
            std::vector<std::string> &p_query = past_query[id];
            std::vector<struct interaction> &p_result = past_result[id];
            std::vector<std::map<std::string, double> > term_weight;
            int size = p_query.size();

	 	    if(step > p_query.size() + 1){
	           step = p_query.size() + 1;
	        }
               
	        if(step <= p_query.size()){
		      c_query = p_query[step-1];
	        }

                if (size > 0) term_weight.push_back( RL0_expand(p_query[0]) );
                for (int i = 1; i < size && i < step; ++i) {
                        if (1==1) { //!cancel[i] && !cancel[i-1]
                                term_weight.push_back( get_weight(term_weight[term_weight.size()-1], p_query[i], p_result[i-1]) );
                        } else if (!cancel[i]) {
                                term_weight.push_back( RL0_expand(p_query[i]) );
                        }
                }
                if (size > 0 && step > size) term_weight.push_back( get_weight(term_weight[term_weight.size()-1], c_query, p_result[size-1]) ); //&& !cancel[size-1]
                else{ if(step>size) term_weight.push_back( RL0_expand(c_query) );}

                int num = term_weight.size();
                std::map<std::string, double>::iterator it;
                std::stringstream result_stream;
                
		        result_stream << "#weight(";
                double decay = past_weight;
                result_stream << build_query(1, term_weight[num-1]);
                for (int i = num-2; i >= 0; --i) {
                        result_stream << build_query(decay, term_weight[i]);
                        decay *= past_weight;
                }
                result_stream << ")";
                return result_stream.str();
        }

        std::vector<int> get_sat_docid_list(int id, int step)
        {
                std::vector<bool> cancel;
                remove_duplicate(id, cancel, step);
                std::string c_query = current_query[id];
                std::vector<std::string> &p_query = past_query[id];
                std::vector<struct interaction> &p_result = past_result[id];
                int size = p_query.size();

    	 	    if(step > p_query.size() + 1){
    	           step = p_query.size() + 1;
    	        }
                   
    	        if(step <= p_query.size()){
    		      c_query = p_query[step-1];
    	        }

    		    std::vector<std::string> ids;
    		    for(int i=0; i< p_result.size() && i < step; i++){
    			    struct interaction & act = p_result[i];
                    	
                    int size = act.docno.size();
                    for (int i = 0; i < size; ++i) {
                        if (act.clicked[i] >= 30) {
    				        if(std::find(ids.begin(), ids.end(), act.docno[i]) == ids.end()){
                            		ids.push_back(act.docno[i]);
    				        }
                        }
                    }			
    		    }

                std::vector<int> docids = environment.documentIDsFromMetadata("docno", ids);

                return docids;
        }


        std::string get_all_query(int id, int step)
        {
                std::string c_query = current_query[id];
                std::vector<std::string> &p_query = past_query[id];
                std::vector<struct interaction> &p_result = past_result[id];
                int size = p_query.size();

    	 	    if(step > p_query.size() + 1){
    	           step = p_query.size() + 1;
    	        }
                   
    	        if(step <= p_query.size()){
    		       c_query = p_query[step-1];
    	        }

		        std::vector<std::map<std::string, double> > term_weight;
                for (int i = 0; i < size && i < step; ++i) {
                     term_weight.push_back( RL0_expand(p_query[i]) );
                }
                term_weight.push_back( RL0_expand(c_query) );

		        std::unordered_map<std::string, int> qKeyWords;
                int num = term_weight.size();
                std::map<std::string, double>::iterator it;
                std::stringstream result_stream;
                for (int i = 0; i < num; ++i) {
    			   std::map<std::string, double> & termW = term_weight[i];
    			   for (it = termW.begin(); it != termW.end(); ++it) {
                        result_stream << it->first << " ";
                   }
                }

    		    istringstream iss(result_stream.str());
    		    string result = "";
    		    do
        		{
            		string sub;
            		iss >> sub;
    			    std::unordered_map<std::string, int>::iterator findp = qKeyWords.find(sub);
        			if(findp == qKeyWords.end()){
        			   qKeyWords[sub] =1;
        			   result = result + " " + sub;
        			}
        		} while (iss);
		  
                return result;
        }


        std::map<std::string, double> get_weight(std::map<std::string, double> &last_weight, const string &current_q, const interaction &act)
        {
                std::map<std::string, double> current_weight = RL0_expand(current_q);
                std::map<std::string, double>::iterator it;
                std::set<std::string> buffer_occurred;
                std::set<std::string> buffer_new;
                std::set<std::string> buffer_canceled;
                for (it = current_weight.begin(); it != current_weight.end(); ++it) {
                        if ( last_weight.count(it->first) ) {
                                buffer_occurred.insert(it->first);
                        } else {
                                buffer_new.insert(it->first);
                        }
                }
                for (it = last_weight.begin(); it != last_weight.end(); ++it) {
                        if ( !current_weight.count(it->first) ) {
                                buffer_canceled.insert(it->first);
                        }
                }
                double max_prob;
                std::set<std::string>::iterator it2;
                for (it2 = buffer_occurred.begin(); it2 != buffer_occurred.end(); ++it2) {
                        std::set<std::string> temp;
                        temp.insert(*it2);
                        max_prob = get_max_probability(temp, act);
                        current_weight[*it2] += alpha * max_prob;
                }
                for (it2 = buffer_new.begin(); it2 != buffer_new.end(); ++it2) {
                        std::set<std::string> temp;
                        temp.insert(*it2);
                        max_prob = get_max_probability(temp, act);
                        current_weight[*it2] -= beta * max_prob;
                }
                for (it2 = buffer_canceled.begin(); it2 != buffer_canceled.end(); ++it2) {
                        std::set<std::string> temp;
                        temp.insert(*it2);
                        max_prob = get_max_probability(temp, act);
                        last_weight[*it2] -= gamma * max_prob;
                }
                return current_weight;
        }

        void correct(std::string &term)
        {
                if (correct_table.count(term) > 0) term = correct_table[term];
        }

        double get_max_probability(const std::set<std::string> &q, const struct interaction &act)
        {
                int size = act.snippet.size();
                if (size == 0) return 0.5 / gamma;
                double max_prob = 0;
                for (int j = 0; j < size; ++j) {
                        double prob = get_probability(q, act.snippet[j]);
                        if (prob > max_prob) max_prob = prob;
                }
                std::vector<std::string> sat_documents = get_sat_documents(act);
                int num = sat_documents.size();
                for (int j = 0; j < num; ++j) {
                        double prob = get_probability(q, sat_documents[j]);
                        if (prob > max_prob) max_prob = prob;
                }
                return max_prob;
        }

        std::vector<std::string> get_sat_documents(const struct interaction &act)
        {
                std::vector<std::string> ids;
                int size = act.docno.size();
                for (int i = 0; i < size; ++i) {
                        if (act.clicked[i] >= 30) {
                                ids.push_back(act.docno[i]);
                        }
                }
                std::vector<indri::api::ParsedDocument *> docs = simpleEnvironment.documentsFromMetadata("docno", ids);
                int num = docs.size();
                std::vector<std::string> texts;
                for (int i = 0; i < num; ++i) {
                        texts.push_back(docs[i]->getContent());
                }
                for (int i = 0; i < num; ++i) {
                        delete docs[i];
                }
                return texts;
        }

        std::vector<std::string> get_read_documents(const struct interaction &act)
        {
                int maxClickRank = -1;
                std::vector<std::string> ids;
                int size = act.docno.size();
                for (int i = 0; i < size; ++i) {
                        //all click data
                        if (act.clicked[i] > 0) {
                                if(i> maxClickRank) { maxClickRank = i;}
                                ids.push_back(act.docno[i]);
                        }
                }
                if(maxClickRank == -1) {maxClickRank = act.clicked.size()-1;}
                std::vector<indri::api::ParsedDocument *> docs = simpleEnvironment.documentsFromMetadata("docno", ids);
                int num = docs.size();
                std::vector<std::string> texts;
                for (int i = 0; i < num; ++i) {
                        texts.push_back(docs[i]->getContent());
                }
                for (int i = 0; i < num; ++i) {
                        delete docs[i];
                }
                for(int i=0; i<=maxClickRank && i< act.snippet.size();i++){
                        std::string context = filter(act.snippet[i]);
                        texts.push_back(context);
                }

                return texts;
        }

        double get_probability(const std::set<std::string> &q, const std::string &d)
        {
                indri::parse::KrovetzStemmer stemmer;
                std::map<std::string, double> doc_map = RL0_expand(d);
                double length = 0;
                std::map<std::string, double>::iterator it;
                for (it = doc_map.begin(); it != doc_map.end(); ++it) {
                        length += it->second;
                }
                double prob = 0;
                std::set<std::string>::const_iterator it2;
                for (it2 = q.begin(); it2 != q.end(); ++it2) {
                        if (doc_map.count(*it2))
                                prob += log( 1 - doc_map[*it2]/length);
                }
                return 1 - exp(prob);
         }

        std::map<std::string, double> RL0_expand(std::string query)
        {
                int found = query.find("&amp;amp;");
                while (found!=std::string::npos) {
                         query[found] = ' ';
                         found = query.find("&amp;amp;", found+1);
                }
                found = query.find("\'s");
                while (found!=std::string::npos) {
                        query[found++] = ' ';
                        query[found++] = ' ';
                        found = query.find("\'s");
                }

                std::string punctuation = ":?.,!\"\';{}[]<>()*/\\";
                found = query.find_first_of(punctuation);
                while (found!=std::string::npos) {
                        query[found] = ' ';
                        found = query.find_first_of(punctuation, found+1);
                }

                //return query;
                indri::parse::KrovetzStemmer stemmer;
                std::stringstream query_stream(query);
                std::map<std::string, double> buffer;
                std::string word;
                while (query_stream >> word) {
                        correct(word);
                        char *cp = const_cast<char *>(word.c_str());
                        std::string stemmed_word = stemmer.kstem_stemmer(cp);
                        buffer[stemmed_word]++;
                }
                return buffer;
        }

        std::string filter(std::string query)
        {
                int found = query.find("&amp;amp;");
                while (found!=std::string::npos) {
                         query[found] = ' ';
                         found = query.find("&amp;amp;", found+1);
                }
                found = query.find("&amp;nbsp;");
                while (found!=std::string::npos) {
                         query[found] = ' ';
                         found = query.find("&amp;nbsp;", found+1);
                }
                found = query.find("\'s");
                while (found!=std::string::npos) {
                        query[found++] = ' ';
                        query[found++] = ' ';
                        found = query.find("\'s");
                }
                std::string punctuation = "+:?.,!\"\';{}[]<>()*/\\";
                found = query.find_first_of(punctuation);
                while (found!=std::string::npos) {
                        query[found] = ' ';
                        found = query.find_first_of(punctuation, found+1);
                }
                found = query.find("-");
                while (found!=std::string::npos) {
                             if(found != query.length()-1){
                                 if(query[found + 1]<'0' || query[found + 1] > '9'){
                                query[found] = ' ';
                                 }
                             }
                         found = query.find("-", found+1);
                }

                //return query;
                indri::parse::KrovetzStemmer stemmer;
                std::stringstream query_stream(query);
                std::ostringstream buffer;
                std::string word;
                while (query_stream >> word) {
                        correct(word);
                        char *cp = const_cast<char *>(word.c_str());
                        std::string stemmed_word = stemmer.kstem_stemmer(cp);
                        buffer << stemmed_word << " ";
                }
                return buffer.str();
        }

private:
        bool same(std::string query1, std::string query2)
        {
                if (query1==query2) return true;
                std::stringstream query_stream1(query1);
                std::stringstream query_stream2(query2);
                std::vector<std::string> term1;
                std::vector<std::string> term2;
                std::string word;
                const std::string lowercase = "abcdefghijklmnopqrstuvwxyz";
                while (query_stream1 >> word) {
                        if (word.find_first_of(lowercase)==std::string::npos) {
                                int length = word.size();
                                for (int i=0; i<length; ++i) term1.push_back(std::string(1, word[i]));
                        } else {
                                indri::parse::KrovetzStemmer stemmer;
                                char *cp = const_cast<char *>(word.c_str());
                                std::string stemmed_word = stemmer.kstem_stemmer(cp);
                                term1.push_back(stemmed_word);
                        }
                }
                while (query_stream2 >> word) {
                        if (word.find_first_of(lowercase)==std::string::npos) {
                                int length = word.size();
                                for (int i=0; i<length; ++i) term2.push_back(std::string(1, word[i]));
                        } else {
                                indri::parse::KrovetzStemmer stemmer;
                                char *cp = const_cast<char *>(word.c_str());
                                std::string stemmed_word = stemmer.kstem_stemmer(cp);
                                term2.push_back(stemmed_word);
                        }
                }
                int size1 = term1.size();
                int size2 = term2.size();
                if (size1!=size2) return false;
                for (int i=0; i<size1; ++i) {
                        if (term1[i].find_first_of(lowercase)==std::string::npos || term2[i].find_first_of(lowercase)==std::string::npos) {
                                int diff = abs(term1[i][0]-term2[i][0]);
                                if (diff!=0 && diff!=32) return false;
                        } else if (term1!=term2) {
                                return false;
                        }
                }
                return true;
        }

        void update_anchor_list_sorted(const std::vector<std::string> *sorted_url=0)
        {
                std::string anchor_file = param.get("anchor_list", "anchor_list.txt");
                std::string punctuation = ":?.,!\"\';{}[]<>()*/\\";
                std::ifstream anchor_log(anchor_file.c_str());
                std::string line;

                while (std::getline(anchor_log, line)) {
                        std::stringstream line_stream(line);
                        std::string link;
                        line_stream >> link;
                        if (sorted_url==0 || std::binary_search(sorted_url->begin(), sorted_url->end(), link)) {
                                std::vector<std::string> &content = anchor_list[link];
                                std::string anchor;
                                std::getline(line_stream, anchor, '\t');
                                std::getline(line_stream, anchor, '\t');
                                std::getline(line_stream, anchor, '\t');
                                std::transform(anchor.begin(), anchor.end(), anchor.begin(), ::tolower);
                                int found = anchor.find_first_of(punctuation);
                                while (found!=std::string::npos) {
                                        anchor[found] = ' ';
                                        found = anchor.find_first_of(punctuation, found+1);
                                }
                                content.push_back(anchor);
                        }
                }
        }

        void parse_xml()
        {
                const std::vector<indri::xml::XMLNode*>& children = root->getChildren();
                std::stack<indri::xml::XMLNode*> s;
                int size = children.size();
                for (int i=0; i<size; ++i) {
                        process_node(children[i]); 
                }
        }

        void process_node(const indri::xml::XMLNode* r)
        {
                if ("query"==r->getName()) {
                        if (flag_current_query) {
                                current_query.push_back(r->getValue());
                                     sessionIDs.push_back(session_id);
                                flag_current_query = false;
                        } else {
                                if (past_query.size()>=session_id) {
                                        std::vector<std::string> &p_q = past_query[session_id-1];
                                        p_q.push_back(r->getValue());
                                } else {
                                        std::vector<std::string> p_q;
                                        p_q.push_back(r->getValue());
                                        past_query.push_back(p_q);
                                }
                        }
                        return;
                }
                if ("url"==r->getName()) {
                        struct interaction &ii = past_result[session_id-1][interaction_id-1];
                        ii.url.push_back(r->getValue());
                }
                if ("clueweb09id"==r->getName() || "clueweb12id"==r->getName()) {
                        struct interaction &ii = past_result[session_id-1][interaction_id-1];
                        ii.docno.push_back(r->getValue());
                }
                if ("title"==r->getName()) {
                        struct interaction &ii = past_result[session_id-1][interaction_id-1];
                        ii.title.push_back(r->getValue());
                }
                if ("snippet"==r->getName()) {
                        struct interaction &ii = past_result[session_id-1][interaction_id-1];
                        ii.snippet.push_back(r->getValue());
                }
                if ("click"==r->getName()) {
                        std::string start = r->getAttribute("starttime");
                        std::string end = r->getAttribute("endtime");
                        attention_time = ttod(end) - ttod(start);
                }
                if ("rank"==r->getName()) {
                        std::string sr = r->getValue();
                        struct interaction &ii = past_result[session_id-1][interaction_id-1];
                        int rank = atoi(sr.c_str());
                            if(attention_time > ii.clicked[rank-1]){
                           ii.clicked[rank-1] = attention_time;
                            }
                }
                if ("session"==r->getName()) {
                        std::string attr = r->getAttribute("num");
                        session_id = atoi(attr.c_str());
                        std::vector<struct interaction> p_r;
                        past_result.push_back(p_r);
                } else if ("currentquery"==r->getName()) {
                        flag_current_query = true;
                } else if ("interaction"==r->getName()) {
                        std::string attr = r->getAttribute("num");
                        interaction_id = atoi(attr.c_str());
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                        struct interaction ii;
                        p_r.push_back(ii);
                } else if ("result"==r->getName()) {
                        std::string attr = r->getAttribute("rank");
                        rank = atoi(attr.c_str());
                } else if ("clicked"==r->getName()) {
                        struct interaction &ii = past_result[session_id-1][interaction_id-1];
                        ii.clicked.assign(ii.url.size(), 0);
                } else if ("topic"==r->getName()) {
                        flag_topic = true;
                }
                const std::vector<indri::xml::XMLNode*>& children = r->getChildren();
                int size = children.size();
                for (int i=0; i<size; ++i) {
                        process_node(children[i]);
                }
                if ("topic"==r->getName()) flag_topic = false;
        }

        indri::api::QueryEnvironment environment;
        indri::api::QueryEnvironment simpleEnvironment;

        indri::api::Parameters &param;
        std::vector<lemur::api::DOCID_T> &filtered_doc_id;
        indri::xml::XMLReader xml;
        indri::xml::XMLNode* root;
        std::vector<std::string> current_query;
        std::vector<int> sessionIDs;
        std::vector<std::vector<std::string> > past_query;
        std::vector<std::vector<struct interaction> > past_result;
        std::vector<std::string> expand_table;
        double past_weight;
        double anchor_weight;
        bool flag_current_query;
        int session_id;
        int interaction_id;
        int rank;
        std::map<std::string, std::vector<std::string> > anchor_list;
        double attention_time;
        bool flag_topic;
        std::map<std::string, std::map<std::string, int> > reference_doc;
        double alpha;
        double beta;
        double gamma;
        std::map<std::string, std::string> correct_table;
};

int main(int argc, char **argv)
{
        indri::api::QueryEnvironment environment;

        try {
                indri::api::Parameters &param = indri::api::Parameters::instance();
                param.loadCommandLine(argc, argv);
                if(param.size() == 0){
                   print_usage(argv[0]);
                   print_help();
                   return 0;
                }
                if (!param.exists("session")) {
                        print_usage(argv[0]);
                        LEMUR_THROW( LEMUR_MISSING_PARAMETER_ERROR, "Must specify session file." );
                }
                if (!param.exists("runid")) {
                        print_usage(argv[0]);
                        LEMUR_THROW( LEMUR_MISSING_PARAMETER_ERROR, "Must specify run id." );
                }
                if (!param.exists("index")) {
                        print_usage(argv[0]);
                        LEMUR_THROW( LEMUR_MISSING_PARAMETER_ERROR, "Must specify index path." );
                }
                std::string index = param.get("index");
                std::string spam = param.get("spam","");
                std::string session_file = param.get("session");
                std::string run = param.get("runid");
                int count = param.get("count", 100);
                int lower_bound = param.get("threshold", 70);
                int sessionNum = param.get("sessionNum", -1); 

                std::vector<std::string> rule;
                if (param.exists("rule")) {
                        indri::api::Parameters slice = param["rule"];
                        for (int i=0; i<slice.size(); ++i) {
                                rule.push_back(slice[i]);
                        }
                        environment.setScoringRules(rule);
                }

                std::vector<std::string> stopwords;
                if (param.exists("stopper")) {
                        std::string stopper = param.get("stopper");
                        std::ifstream stopper_file(stopper.c_str());
                        std::string stopword;
                        while (stopper_file>>stopword) {
                                stopwords.push_back(stopword);
                        }
                        environment.setStopwords(stopwords);
                }
                environment.addIndex(index);

                //import waterloo spam scores
                std::map<std::string, int> spam_score;
                std::ifstream spam_file(spam.c_str());
                std::string doc_no;
                int score;
                //very slow
                while (spam_file>>score>>doc_no) {
                        spam_score[doc_no] = score;
                }
                struct filter_by_threshold filter;
                filter.threshold = lower_bound;
                filter = std::for_each(spam_score.begin(), spam_score.end(), filter);
                std::vector<lemur::api::DOCID_T> filtered_doc_id;
                filtered_doc_id = environment.documentIDsFromMetadata("docno", filter.filtered_doc_no);

                SessionParser session(session_file, param, filtered_doc_id);
                session.stopwords = stopwords;

                int session_number = session.get_session_number();
                int maxStep = session.getMaxStep();
                for(int step=maxStep-1;step<maxStep;step++){
                    stringstream ss;
                    ss << "eval/" << run << ".rl2";
                    std::ofstream result_RL4(ss.str().c_str());

                    for (int i=0; i<session_number; ++i) {
                        if(sessionNum!=-1 && sessionNum != session.get_session_num(i))
                        {
                           continue;
                        }

		        std::string finalAction = "";
                        std::vector<indri::api::ScoredExtentResult> optRslt = session.loopSEValue(i, step + 1, finalAction);
                        std::vector<std::string> result_doc_no;
		        if(finalAction =="FullAllQuery -1"){
                    		result_doc_no = environment.documentMetadata(optRslt, "docno");
	 	        }else{
				result_doc_no = environment.documentMetadata(optRslt, "docno");
		        }

                        for (int j=0; j<optRslt.size(); ++j) {
                             result_RL4 << i+1 << "\t" << "Q0" << "\t" << result_doc_no[j] << "\t" << j+1 << "\t" << optRslt[j].score << "\t" << run << endl;
                        }
                    }
                }

                environment.close();
        } catch (lemur::api::Exception& e) {
                environment.close();
                LEMUR_ABORT(e);
        } catch (...) {
                environment.close();
                cout << "Caught unhandled exception" << endl;
                return -1;
        }
        return 0;
}
