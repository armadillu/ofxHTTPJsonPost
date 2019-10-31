//
//  ofxHTTPJsonPost.cpp
//  StoryGuideServer
//
//  Created by Oriol Ferrer Mesià on 31/10/2019.
//
//

#include "ofxHTTPJsonPost.h"

#include "Poco/Net/HTTPClientSession.h"
#include "Poco/Net/HTTPRequest.h"
#include "Poco/Net/HTTPResponse.h"
#include "Poco/Net/HTMLForm.h"
#include "Poco/StreamCopier.h"
#include "Poco/Path.h"
#include "Poco/URI.h"
#include "Poco/Exception.h"

ofxHTTPJsonPost::ofxHTTPJsonPost(){}

ofxHTTPJsonPost::~ofxHTTPJsonPost(){
	clearQueue();
}


void ofxHTTPJsonPost::cancelAllSubmissions(){
	if(pendingPosts.size()){
		ofLogWarning("ofxHTTPJsonPost") << "deleting " << pendingPosts.size() << " pending submissions!";
		pendingPosts.clear();
	}
}

void ofxHTTPJsonPost::clearQueue(){

	ofLogNotice("ofxHTTPJsonPost") << "clearQueue()...";
	cancelAllSubmissions();
	if(tasks.size())
		ofLogNotice("ofxHTTPJsonPost") << "We may have to wait a bit, " << tasks.size() << " are currently executing.";
	while (tasks.size()) {
		update();
	}
}


string ofxHTTPJsonPost::getStatus(){
	string status;//TODO: report # of queued items, running, etc
	return status;
}


void ofxHTTPJsonPost::postJsonData(ofJson & jsonData, const string & url){
	PostData job;
	job.jsonData = jsonData;
	job.url = url;
	pendingPosts.push_back(job);
}


void ofxHTTPJsonPost::update(){

	//check for finished tasks
	for(int i = tasks.size() - 1; i >= 0; i--){
		std::future_status status = tasks[i].wait_for(std::chrono::microseconds(0));
		if(status == std::future_status::ready){
			auto job = tasks[i].get();
			tasks.erase(tasks.begin() + i);
			ofNotifyEvent(eventPostFinished, job, this);
		}
	}

	//spawn new jobs if pending
	vector<size_t> spawnedJobs;
	for(int i = 0; i < pendingPosts.size(); i++){
		if(tasks.size() < maxThreads){
			try{
				tasks.push_back( std::async(std::launch::async, &ofxHTTPJsonPost::runJob, this, pendingPosts[i]));
			}catch(exception e){
				ofLogError("ofxHTTPJsonPost") << "Exception at async() " <<  e.what();
			}
			spawnedJobs.push_back(i);
		}else{
			break;
		}
	}

	//removed newly spawned jobs
	for(int i = spawnedJobs.size() - 1; i >= 0; i--){
		pendingPosts.erase(pendingPosts.begin() + spawnedJobs[i]);
	}
}


ofxHTTPJsonPost::PostData ofxHTTPJsonPost::runJob(PostData j){

	#ifdef TARGET_WIN32
	#elif defined(TARGET_LINUX)
	pthread_setname_np(pthread_self(), "ofxHTTPJsonPost");
	#else
	pthread_setname_np("ofxHTTPJsonPost");
	#endif

	try{

		j.duration = ofGetElapsedTimef();
		Poco::URI uri(j.url);

		Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());
		std::string path(uri.getPathAndQuery());
		if (path.empty()) path = "/";

		Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, path, Poco::Net::HTTPMessage::HTTP_1_1);
		Poco::Net::HTTPResponse response;

		string jsonStr = j.jsonData.dump(); //get json as string

		request.setContentType("application/json");
		request.setContentLength(jsonStr.size());
		std::ostream& o = session.sendRequest(request);

		o << jsonStr; //push json into ostream

		std::istream& s = session.receiveResponse(response);
		std::string respoStr(std::istreambuf_iterator<char>(s), {});

		j.status = ofToString(response.getStatus());
		j.reason = response.getReason();
		j.response = respoStr;
		j.ok = true;

	}catch(std::exception e){
		ofLogError("ErrorReports") << e.what();
		j.status = "error";
		j.ok = false;
		j.reason = e.what();
	}

	j.duration = ofGetElapsedTimef() - j.duration;

	return j;
}
