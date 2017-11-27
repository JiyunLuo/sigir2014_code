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

#define N 50220423.0

int current_session_id = 0; //Sicong

//jiyun 2014/08/07
std::unordered_map<int, std::unordered_map<std::string, double>> doclist; //sessionID, docNo, clicked time
std::unordered_map<int, int> topicMap; //session id : topic id

struct DOC_RSLT{
	int docid;
	string docno;
	double score;
	int rank;

	bool operator < (const DOC_RSLT& str) const
	{
   	return (score > str.score);
	}
};

bool cmpRslt(const indri::api::ScoredExtentResult & d1, const indri::api::ScoredExtentResult & d2){
        return d1.score > d2.score;
}


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
        unordered_map<int, int> rank2index; //document rank to its array position

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

//2013 Sicong
double ttod(std::string time)
{
        std::stringstream time_stream(time);
        double second = 0;
        string word;
        std::getline(time_stream, word);
        second += atof(word.c_str());
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
                         result_doc_no = fullenvironment.documentMetadata(result, "docno");

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
//cout << "opt:" << optQuery << endl;
        			if(optAction =="FullAllQuery -1"){
        				finalresult = fullenvironment.runQuery(optQuery,2000);
        			}else{
                        finalresult = environment.runQuery(optQuery,2000);
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
        			query = combine_all_query(i,step); //get_all_query(i, step);
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
                          //std::map<std::string, double> doc_map =  RL0_expand(c_query);
                          //query = "#weight(" + build_query(1,doc_map) + ")";
                          //result = simpleEnvironment.runQuery(query, fbDocs);
            			 query = get_all_query(i, step);
            			 std::vector<int> list = get_sat_docid_list(i, step);
            			 for(int k=0;k<list.size();k++){
            				cout << "doc list: " <<  list[k] << endl;
            			 }
            			 result = environment.runQuery(query, list, fbDocs);
                         query = expander->expand(query, result);
               }

               result.clear();
               finalQuery = query;
//cout << "trial:" << query << "  action is:" << actionName << " " << degree << endl;
        	   if(actionName == "FullAllQuery"){
                    result = fullenvironment.runQuery(query,10);
               }else{
                 	if(actionName == "AllQuery"){
        			        result = environment.runQuery(query,10);
                    }else{
        		    	    result = environment.runQuery(query,10);
                    }
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

                std::string index = "/data1/index/ClueWeb12CatB_Clean"; //param.get("index");
                grdtruthFile = param.get("qrel", "");
                topicFile = param.get("topic", "");
                runid = param.get("runid", "");
                environment.addIndex(index);
		        environment.addIndex("/data1/index/Qrel_Click");

                fullenvironment.addIndex("/data1/index/ClueWeb12CatB");
		        fullenvironment.addIndex("/data1/index/Qrel_Click");
                simpleEnvironment.addIndex("/data1/index/Qrel_Click");
                std::vector<std::string> rule;
                if (param.exists("rule")) {
                        indri::api::Parameters slice = param["rule"];
                        for (int i=0; i<slice.size(); ++i) {
                                rule.push_back(slice[i]);
                        }
                        environment.setScoringRules(rule);
                        fullenvironment.setScoringRules(rule);
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
                        fullenvironment.setStopwords(stopwords);
                        simpleEnvironment.setStopwords(stopwords);
                }
                past_weight = param.get("past", 0.8);
                anchor_weight = param.get("anchor", 0.3);
                alpha = param.get("alpha", 1.0);
                beta = param.get("beta", 1.0);
                gamma = param.get("gamma", 1.0);
		        epsilon = param.get("epsilon", 0.07);
                omega = param.get("omega", 1);
		        badquery = param.get("badquery", 0.8); 

                int fbDocs=0;
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
                //2013 New
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

                correct_table["-usa"] = "usa";
                correct_table["-job"] = "job";
                correct_table["\'EUROPEAN"] = "\"EUROPEAN";

                correct_table["isn't"] = "isnt";
                correct_table["teachers'"] = "teachers";
                correct_table["delopment"] = "development";
                correct_table["devolopmental"] = "developmental";
                correct_table["-hpv"] = "hpv";
                correct_table["can't"] = "cant";
                correct_table["percenage"] = "percentage";
                correct_table["trax"] = "tax";
                correct_table["iraadisadvangtage"] = "ira disadvantage";
                correct_table["adisadvangtage"] = "disadvantage";
                correct_table["empoyess"] = "employee";
		 // correct_table["dallas"] = "dulles";
        }

        std::string get_current_query(int id)
        {
                return current_query[id];
        }

        int get_session_number()
        {
                return current_query.size();
        }

        std::string get_RL2_query(int id)
        {
                std::string c_query = current_query[id];
                std::vector<std::string> p_query = past_query[id];
                std::stringstream result_stream;
                result_stream << "#weight(";
                int size = p_query.size();
                for (int i=0; i<size; ++i) {
                        std::map<std::string, double> term_weight = RL0_expand(p_query[i]);
                        std::map<std::string, double>::iterator it;
                        std::string ss;
                        for (it = term_weight.begin(); it != term_weight.end(); ++it) {
                                ss += it->first + " ";
                        }
                        if (ss.size() > 0) {
                                ss.erase(ss.size()-1);
                                result_stream << past_weight << " #combine(" << ss << ") ";
                        }
                }
                std::map<std::string, double> term_weight = RL0_expand(c_query);
                std::map<std::string, double>::iterator it;
                std::string ss;
                for (it = term_weight.begin(); it != term_weight.end(); ++it) {
                        ss += it->first + " ";
                }
                if (ss.size() > 0) {
                        ss.erase(ss.size()-1);
                        result_stream << 1 - past_weight << " #combine(" << ss << ") ";
                }
                result_stream << ")";
                return result_stream.str();
        }

        std::string build_query(double weight, std::map<std::string, double> &term_weight)
        {
                std::map<std::string, double>::iterator it = term_weight.begin();
                int bigN = 15702181 + 3449;

                bool isSwa=false; bool isKenya=false;
                  for (it = term_weight.begin(); it != term_weight.end(); ++it) {
                        if (it->first == "swahili"){
                                isSwa = true;
                        }
                        if (it->first == "kenya"){
                                isKenya = true;
                        }
                }
                if(isSwa && !isKenya){
                        for (it = term_weight.begin(); it != term_weight.end(); ++it) {
                                if (it->first == "swahili"){
                                        double weight = it->second;
                                        term_weight.erase(it);
                                        term_weight["kenya-swahili"] = weight;
                                         break;
                                }
                        }
                }

                std::stringstream result_stream;
                result_stream << " " << weight << " #weight(";
                std::string first_t;    //Sicong
                for (it = term_weight.begin(); it != term_weight.end(); ++it) {
                        if(it->second == -1) continue;
                        first_t = it->first;
                        if (first_t.find(" ") != first_t.npos){
                                first_t = "#1(" + first_t + ")";
                        }
                     result_stream << " " << it->second << " " << first_t << " ";
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

        /*void remove_duplicate(int id, std::vector<bool> &cancel, int step)
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
        }*/

        std::string get_RL4_query(int id, int step)
        {
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

                if (size > 0 ) term_weight.push_back( RL0_expand_query(p_query[0]) );
                for (int i = 1; i < size && i < step; ++i) {
                     term_weight.push_back( get_weight(term_weight[i-1], p_query[i], p_result[i-1]) );
                }
                if (size > 0 && step > size) term_weight.push_back( get_weight(term_weight[term_weight.size()-1], c_query, p_result[size-1]) );
                else{ if(step>size) term_weight.push_back( RL0_expand_query(c_query) );}

                int num = term_weight.size();
                std::map<std::string, double>::iterator it;
                std::stringstream result_stream;
                result_stream << "#weight(";

                double decay = 1;
                bool allbad = true;
                for (int i = 0; i < num-1; ++i) {
                        interaction & act = p_result[i];
                        bool isBadQuery = true;
                        int size = act.docno.size();
                        for (int j = 0; j < size; ++j) {
                            if (act.clicked[j] >= 10) {
                                allbad = false;
                                isBadQuery = false; break;
                            }
                        }
                        if(isBadQuery){
                                decay = badquery;
                        }else{
                                decay = 1;
                        }

                        result_stream << build_query(decay, term_weight[i]);
                }
                result_stream << build_query(1, term_weight[num-1]);
                result_stream << ")";

                return result_stream.str();
        }

        std::vector<int> get_sat_docid_list(int id, int step)
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

                std::vector<int> docids = fullenvironment.documentIDsFromMetadata("docno", ids);

                return docids ;
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
                        term_weight.push_back( RL0_expand_query(p_query[i]) );
                }
                term_weight.push_back( RL0_expand_query(c_query) );

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

	    std::string combine_all_query(int id, int step)
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
                        term_weight.push_back( RL0_expand_query(p_query[i]) );
                }
                if(step > size)
                        term_weight.push_back( RL0_expand_query(c_query) );

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

                return result_stream.str();
        }

        std::map<std::string, double> get_weight(std::map<std::string, double> &last_weight, const string &current_q, const interaction &act)
        {
                std::map<std::string, double> current_weight = RL0_expand_query(current_q);
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

                        INT64 df = environment.documentCount(*it2);
                        if (df > 0) current_weight[*it2] += epsilon * int(1 - max_prob) * log(N/df);
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

        void get_topic_scores(int id, int step) // session id start from 0, step from 0 to p_result.size()
        {
                std::string c_query = current_query[id];
                std::vector<std::string> p_query = past_query[id];
                std::vector<struct interaction> p_result = past_result[id];

                doclist.clear();

                  std::unordered_map<int, std::unordered_map<std::string, double>>::iterator doclistFinder = doclist.find(id+1); //sessionID, docNo, clicked time
                  if(doclistFinder == doclist.end()){
                        std::unordered_map<std::string, double> tmp;
                        doclist[id+1] = tmp;
                  }
                  std::unordered_map<std::string, double> & doclistByTopic = doclist[id+1];

                  for(int index=0; index < p_result.size() && index < step; index++){
                        interaction & act = p_result[index];
                        std::vector<std::string> ids;
                        int size = act.docno.size();
                        for (int i = 0; i < size; ++i) {
                           if (act.clicked[i] >= 10) {
                               std::unordered_map<std::string, double>::iterator doclistBTFinder = doclistByTopic.find(act.docno[i]);
                                if(doclistBTFinder == doclistByTopic.end()){
                                        doclistByTopic[act.docno[i]]=0;
                                }
                           }
                        if (act.clicked[i] >= 30) {
                            doclistByTopic[act.docno[i]]+=2;
                        }else{
                                if (act.clicked[i] >= 10) {
                                    doclistByTopic[act.docno[i]]+=1;
                                }
                           }
                        }
                  }
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

        std::map<std::string, double> RL0_expand_query(std::string query)
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
                found = query.find("OR");
                while (found!=std::string::npos) {
                        query[found++] = ' ';
                        query[found++] = ' ';
                        found = query.find("OR");
                }

                //Sicong
                std::string punctuation;
                punctuation = "+:?.,!;{}[]<>()*/\\";
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
                std::string phrase_word; //Sicong
                while (query_stream >> word) {
                        correct(word);
                        if (word.find("\"") != word.npos){
                                word[word.find("\"")] = ' ';
                                phrase_word = word;
                                while ((phrase_word.find("\"") == phrase_word.npos) && (query_stream >> word)){
                                        phrase_word += " ";
                                        phrase_word += word;
                                }
                                word = phrase_word;
                        }
                        if (word.find("\"") != word.npos){
                                word[word.find("\"")] = ' ';
                        }
                        if (word[0]==' '){
                                word = word.substr(1, word.length()-1);
                        }
                        word.erase(word.find_last_not_of(" \n\r\t")+1);
                        char *cp = const_cast<char *>(word.c_str());
                        std::string stemmed_word = stemmer.kstem_stemmer(cp);
                        buffer[stemmed_word]++;
                }
                return buffer;
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

        std::vector<std::string> get_top_url(std::string query, int count, std::string index="/data1/index/ClueWeb12CatB_Clean")
        {
                environment.removeIndex(param.get("index", "/data1/index/ClueWeb12CatB_Clean"));
                environment.addIndex("/data1/index/Qrel_Click");
                environment.addIndex(index);
                //std::vector<indri::api::ScoredExtentResult> result = environment.runQuery(query, filtered_doc_id, count);
                std::vector<indri::api::ScoredExtentResult> result = environment.runQuery(query, count);
                std::vector<std::string> result_url;
                result_url = environment.documentMetadata(result, "url");
                environment.removeIndex("/data1/index/Qrel_Click");
                environment.removeIndex(index);
                environment.addIndex("/data1/index/Qrel_Click");
                environment.addIndex(param.get("index", "/data1/index/ClueWeb12CatB_Clean"));
                return result_url;
        }

        void update_anchor_list(std::vector<std::string> url)
        {
                std::sort(url.begin(), url.end());
                update_anchor_list_sorted(&url);
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
                        //Jiyun add for 2014 begin
                        if(current_query.size() < past_result.size()){
                           vector<string> & p_q = past_query[past_query.size()-1];
                           current_query.push_back(p_q[p_q.size()-1]);
                           p_q.erase(p_q.begin() + p_q.size()-1);
                           vector<interaction> & ii= past_result[past_result.size()-1];
                           ii.erase(ii.begin() + ii.size()-1);
                        }
                        //Jiyun add for 2014 end 
                }
        }

        void process_node(const indri::xml::XMLNode* r)
        {
                if ("query"==r->getName()) {
                        if(isPaging && !flag_current_query) return;

                        if (flag_current_query) {
                                current_query.push_back(r->getValue());
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
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                        struct interaction &ii = past_result[session_id-1][p_r.size()-1];
                        if(isNewRank)
                           ii.url.push_back(r->getValue());
                }
                if ("clueweb12id"==r->getName()) {
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                        struct interaction &ii = past_result[session_id-1][p_r.size()-1];
                        if(isNewRank)
                                ii.docno.push_back(r->getValue());
                }
                if ("title"==r->getName()) {
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                        struct interaction &ii = past_result[session_id-1][p_r.size()-1];
                        if(isNewRank)
                                ii.title.push_back(r->getValue());
                }
                if ("snippet"==r->getName()) {
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                        struct interaction &ii = past_result[session_id-1][p_r.size()-1];
                        if(isNewRank)
                                ii.snippet.push_back(r->getValue());
                }
                if ("click"==r->getName()) {
                        std::string start = r->getAttribute("starttime");
                        std::string end = r->getAttribute("endtime");
                        attention_time = ttod(end) - ttod(start);
                }
                if ("rank"==r->getName()) {
                           //jiyun 2014/08/07
                        /*std::string sr = r->getValue();
                        struct interaction &ii = past_result[session_id-1][interaction_id-1];
                        int rank = atoi(sr.c_str());
                        ii.clicked[rank-1] = attention_time;*/
                        std::string sr = r->getValue();
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                        struct interaction &ii = past_result[session_id-1][p_r.size()-1];
                        int myrank = atoi(sr.c_str());
                           int position = ii.rank2index[myrank];
                        ii.clicked[position] += attention_time;

                        //if(attention_time > ii.clicked[rank-1]){
                        //   ii.clicked[rank-1] += attention_time;
                        //}
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
                           //jiyun 2014 modified add type
                        std::string type = r->getAttribute("type");

                        if(type != "page"){
                            isPaging = false;
                            interaction_id = atoi(attr.c_str());
                            std::vector<struct interaction> &p_r = past_result[session_id-1];
                            struct interaction ii;
                            p_r.push_back(ii);
                        }else{
                            isPaging = true;
                        }
                } else if ("result"==r->getName()) {
                        std::string attr = r->getAttribute("rank");
                        rank = atoi(attr.c_str());
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                           struct interaction &ii = past_result[session_id-1][p_r.size()-1];
                           unordered_map<int,int>::iterator it;
                           unordered_map<int,int> & tmpMap = ii.rank2index;
                        it = tmpMap.find(rank);

                        if(it != tmpMap.end()){
                                isNewRank = false;
                        }else{
                                isNewRank = true;
                                tmpMap[rank] = ii.url.size();
                        }
                } else if ("clicked"==r->getName()) {
                        std::vector<struct interaction> &p_r = past_result[session_id-1];
                        struct interaction &ii = past_result[session_id-1][p_r.size()-1];
                        //modified jiyun 2014 
                        //ii.clicked.assign(ii.url.size(), 0);
                        ii.clicked.resize(ii.url.size(), 0);
                } else if ("topic"==r->getName()) {
                        flag_topic = true;
                }

                const std::vector<indri::xml::XMLNode*>& children = r->getChildren();
                int size = children.size();
                for (int i=0; i<size; ++i) {
                        process_node(children[i]);
                }
                if ("topic"==r->getName()) flag_topic = false;

                if ("interaction"==r->getName()){
                    std::vector<struct interaction> &p_r = past_result[session_id-1];
                    struct interaction &ii = past_result[session_id-1][p_r.size()-1];

                    ii.clicked.resize(ii.url.size(), 0);
                }
        }

        indri::api::QueryEnvironment environment;
        indri::api::QueryEnvironment fullenvironment;
        indri::api::QueryEnvironment simpleEnvironment;

        indri::api::Parameters &param;
        std::vector<lemur::api::DOCID_T> &filtered_doc_id;
        indri::xml::XMLReader xml;
        indri::xml::XMLNode* root;
        std::vector<std::string> current_query;
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
        bool isPaging;
        bool isNewRank;
        double epsilon;
        double omega;
        double badquery;
        std::map<std::string, std::string> correct_table;
};

int main(int argc, char **argv)
{
        indri::api::QueryEnvironment environment;
    	indri::api::QueryEnvironment fullenvironment;
    	indri::query::QueryExpander* expander = 0;

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
                std::string index = param.get("index", "/data1/index/ClueWeb12CatB_Clean");
                std::string spam = param.get("spam","");
                std::string session_file = param.get("session");
                std::string run = param.get("runid");
                int count = param.get("count", 100);
		        double badquery = param.get("badquery", 0.8);
                int lower_bound = param.get("threshold", 70);
                int sessionNum = param.get("sessionNum", -1); 

                std::vector<std::string> rule;
                if (param.exists("rule")) {
                        indri::api::Parameters slice = param["rule"];
                        for (int i=0; i<slice.size(); ++i) {
                                rule.push_back(slice[i]);
                        }
                        environment.setScoringRules(rule);
			            fullenvironment.setScoringRules(rule);
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
			            fullenvironment.setStopwords(stopwords);
                }
                environment.addIndex(index);
		        environment.addIndex("/data1/index/Qrel_Click");
                fullenvironment.addIndex("/data1/index/ClueWeb12CatB");
		        fullenvironment.addIndex("/data1/index/Qrel_Click");

                std::vector<lemur::api::DOCID_T> filtered_doc_id;

                //jiyun 2014 add
                //input topic mapping file
                std::string topicFile;
                topicFile = param.get("topic", "");
                std::ifstream topic_file(topicFile.c_str());
                int sessionID, topicID,subjectID;
                std::string product, goal, type;
                while (topic_file >> sessionID >> topicID >> subjectID >> product >> goal >> type) {
                    topicMap[sessionID] = topicID;
                }
                //jiyun end

                SessionParser session(session_file, param, filtered_doc_id);
                session.stopwords = stopwords;

                int session_number = session.get_session_number();
                int maxStep = session.getMaxStep();

                for(int step=maxStep-1;step<maxStep;step++){
                    stringstream ss;
                    //ss << "eval/" << run << "_" << step << ".RL4";
                    ss << "eval/" << run  << ".rl2";
                    std::ofstream result_RL4(ss.str().c_str());

                    for (int i=0; i<session_number && i<100; ++i) {
                        if(sessionNum!=-1 && sessionNum != i+1)
                        {
                           continue;
                        }
			            if(i+1 > 1021){continue;}

                        session.get_topic_scores(i,step);
                        std::unordered_map<int, std::unordered_map<std::string, double>>::iterator it;
                        for (it = doclist.begin(); it != doclist.end(); ++it ){
                             std::unordered_map<std::string, double> & doclistBtTopic = it->second;
                             double max = 0;
                             std::unordered_map<std::string, double>::iterator itSecond;
                             for (itSecond = doclistBtTopic.begin(); itSecond != doclistBtTopic.end(); ++itSecond ){
                                  max += itSecond ->second;
                             }
                             for (itSecond = doclistBtTopic.begin(); itSecond != doclistBtTopic.end(); ++itSecond ){
                                  itSecond ->second /= max;
                             }
                        }

		                std::string finalAction = "";
                        std::vector<indri::api::ScoredExtentResult> optRslt = session.loopSEValue(i, step + 1, finalAction);
                        std::vector<std::string> result_doc_no;
    		            if(finalAction =="FullAllQuery -1"){
                        	 result_doc_no = fullenvironment.documentMetadata(optRslt, "docno");
    	 	            }else{
    				         result_doc_no = environment.documentMetadata(optRslt, "docno");
    		            }
                        if(optRslt.size()>0){
                             for (int j=0; j<optRslt.size(); ++j) {
                                 result_RL4 << i+1 << " " << "Q0" << " " << result_doc_no[j] << " " << j+1 << " " << optRslt[j].score << " " << run << endl;
                             }
            			}else{
            			     result_RL4 << i+1 << " Q0 clueweb12-0000wb-00-00000 1 -0.0001 " << run << endl;
            			}
                    }
                }
                delete expander;
                expander = 0;
                environment.close();
                fullenvironment.close();
        } catch (lemur::api::Exception& e) {
                delete expander;
                expander = 0;
                environment.close();
                fullenvironment.close();
                LEMUR_ABORT(e);
        } catch (...) {
                delete expander;
                expander = 0;
                environment.close();
                fullenvironment.close();
                cout << "Caught unhandled exception" << endl;
                return -1;
        }
        return 0;
}
