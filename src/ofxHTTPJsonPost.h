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

	struct PostData{
		ofJson jsonData;
		string url;
		string response;
		string status;
		string reason;
		bool ok = false;
		float duration = 0;
	};

	void update();

	void postJsonData(ofJson & jsonData, const string & http);

	string getStatus();

	void setTimeout(float seconds){timeout = seconds;}
	void setMaxThreads(int maxT){ maxThreads = ofClamp(maxT, 1, std::thread::hardware_concurrency());}

	ofFastEvent<ofxHTTPJsonPost::PostData> eventPostFinished;

protected:

	vector<PostData> pendingPosts;
	vector<std::future<PostData>> tasks;

	float timeout = 10;
	int maxThreads = 1;

	//std::future<ofJson>

	void clearQueue();
	PostData runJob(PostData j);
};

