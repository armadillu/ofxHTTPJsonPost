//
//  ofxHTTPJsonPost.h
//  StoryGuideServer
//
//  Created by Oriol Ferrer Mesià on 31/10/2019.
//
//

#pragma once
#include "ofMain.h"
#include <future>

class ofxHTTPJsonPost{

public:
	
	ofxHTTPJsonPost();
	~ofxHTTPJsonPost();

	struct PostDataJob{
		ofJson jsonData;
		string url;
		string response;
		string status;
		string reason;
		bool ok = false;
		float duration = 0;
		size_t jobID;
		map<string,string> customHeaders;
	};

	void update();

	size_t postJsonData(ofJson & jsonData, const string & http); //returns ticketID
	size_t postJsonData(ofJson & jsonData, const string & http, map<string,string> & customHeaders); //returns ticketID

	string getStatus();

	void setTimeout(float seconds){timeout = seconds;}
	void setMaxThreads(int maxT){ maxThreads = ofClamp(maxT, 1, std::thread::hardware_concurrency());}

	void cancelAllSubmissions();

	ofFastEvent<ofxHTTPJsonPost::PostDataJob> eventPostFinished;
	ofFastEvent<ofxHTTPJsonPost::PostDataJob> eventPostFailed;

protected:

	vector<PostDataJob> pendingJobs;
	vector<std::future<PostDataJob>> tasks;

	float timeout = 2;
	int maxThreads = 1;

	//std::future<ofJson>

	void clearQueue();
	PostDataJob runJob(PostDataJob j);

	static size_t jobIDcounter;
};

