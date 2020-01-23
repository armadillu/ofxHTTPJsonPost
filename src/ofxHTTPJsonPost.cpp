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


size_t ofxHTTPJsonPost::jobIDcounter = 0;


ofxHTTPJsonPost::ofxHTTPJsonPost(){}

ofxHTTPJsonPost::~ofxHTTPJsonPost(){
	clearQueue();
}


void ofxHTTPJsonPost::cancelAllSubmissions(){
	if(pendingJobs.size()){
		ofLogWarning("ofxHTTPJsonPost") << "deleting " << pendingJobs.size() << " pending submissions!";
		pendingJobs.clear();
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


size_t ofxHTTPJsonPost::postJsonData(ofJson & jsonData, const string & url){
	PostDataJob job;
	job.jobID = jobIDcounter; jobIDcounter++;
	job.jsonData = jsonData;
	job.url = url;
	pendingJobs.push_back(job);
	ofLogNotice("ofxHTTPJsonPost") << "postJsonData() new job with jobID \"" << job.jobID << "\"";
	return job.jobID;
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
	for(int i = 0; i < pendingJobs.size(); i++){
		if(tasks.size() < maxThreads){
			try{
				tasks.push_back( std::async(std::launch::async, &ofxHTTPJsonPost::runJob, this, pendingJobs[i]));
			}catch(exception e){
				ofLogError("ofxHTTPJsonPost") << "Job \"" << pendingJobs[i].jobID << "\" Exception at async() " <<  e.what();
			}
			spawnedJobs.push_back(i);
		}else{
			break;
		}
	}

	//removed newly spawned jobs
	for(int i = spawnedJobs.size() - 1; i >= 0; i--){
		pendingJobs.erase(pendingJobs.begin() + spawnedJobs[i]);
	}
}


ofxHTTPJsonPost::PostDataJob ofxHTTPJsonPost::runJob(PostDataJob j){

	#ifdef TARGET_WIN32
	#elif defined(TARGET_LINUX)
	pthread_setname_np(pthread_self(), "ofxHTTPJsonPost");
	#else
	pthread_setname_np("ofxHTTPJsonPost");
	#endif

	static std::mutex mutex;

	try{

		j.duration = ofGetElapsedTimef();
		Poco::URI uri(j.url);

		Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());
		session.setTimeout( Poco::Timespan(timeout, 0) );

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

		int status = response.getStatus();
		j.status = ofToString(status);
		j.reason = response.getReason();
		j.response = respoStr;

		mutex.lock();
		if(status >= 200 && status < 300){
			ofLogNotice("ofxHTTPJsonPost") << "Job \"" << j.jobID << "\" Good response from server " << j.url << " Status: " << j.status <<  " Reason: " << j.reason;
			j.ok = true;
		}else{
			ofLogError("ofxHTTPJsonPost") << "Job \"" << j.jobID << "\" Bad response from server " << j.url << " Status: " << j.status <<  " Reason: " << j.reason << " Response: " << respoStr;
			ofLogError("ofxHTTPJsonPost") << "Job \"" << j.jobID << "\" We sent this JSON data: " << j.jsonData.dump();
			j.ok = false;
		}
		mutex.unlock();

	}catch(std::exception e){
		mutex.lock();
		ofLogError("ofxHTTPJsonPost") << "Job \"" << j.jobID << "\" Exception at runJob() \"" << j.url << "\" - " <<  e.what();
		mutex.unlock();
		j.status = "error";
		j.ok = false;
		j.reason = e.what();
		ofNotifyEvent(eventPostFailed, j, this);
	}

	j.duration = ofGetElapsedTimef() - j.duration;

	return j;
}
