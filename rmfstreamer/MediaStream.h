/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


/**
* @defgroup rmf_mediastreamer
* @{
**/

/**
 * @file MediaStream.h
 */



#ifndef MEDIASTREAM_H
#define MEDIASTREAM_H

using namespace std;
#include <stdint.h>
#include <semaphore.h>

#include "rmf_osal_sync.h"
#include "rmf_osal_event.h"
#include "HTTPRequest.h"


#define MEDIASTREAM_PTS_TIMEBASE   (45*1000)
#define MAX_URL_LEN (1024+1)

/**
 * @class HNClientID
 * @brief Stores the HN client information such as UUID, IP address and session number.
 * @ingroup RMF_MEDIASTREAMER_CLASS
 */
class HNClientID
{
public:
	unsigned char uuid[16]; // uuid of the client - for future
	unsigned long ip_addr; // IP address of client
	unsigned long session_num;  // Identifies session within a client.
};

/**
 * @class TrickPlayParams
 * @brief Stores trick play parameters such as play speed, play position, time range, etc., and defines
 * methods for resetting these parameters.
 * @ingroup RMF_MEDIASTREAMER_CLASS
 */
class TrickPlayParams
{
public:
	float playSpeed;
	float timePos;
	float timeRange;
	int64_t bytePos;
	int64_t byteSize;
	bool modified;

/**
 * @brief This function is used to reset the trickplay parameters such as playSpeed, byteSize 
 * etc... to default.
 *
 * @return None
 */
	void reset() {playSpeed=1.0; timePos=0.0; timeRange=0.0; bytePos = 0; byteSize= 0; modified=false;}
};

/**
 * @enum http_event_type
 * @brief It defines an enumeration to represent http event type.
 * @ingroup RMF_MEDIASTREAMER_TYPES
 */
typedef enum {
	HTTP_STOP_REQUEST = 0x100,
	HTTP_HEADER_READY,
}http_event_type;

/**
 * @class MediaStream
 * @brief Class to hold the stream related information such as
 * URL types, stream direction, number of bytes streamed etc.
 * It also manages the media stream by providing interfaces to control
 * the trick play and to update the streaming information.
 * @ingroup RMF_MEDIASTREAMER_CLASS
 */
class MediaStream
{
	friend class MediaStreamManager;

public:


/**
 * @enum StreamType
 * @brief Enumeration to indicate the streaming types/protocol.
 * @ingroup RMF_MEDIASTREAMER_TYPES
 */
	enum StreamType
	{
		RTP = 0,
		RTSP,
		HTTP,
	};

/**
 * @enum StreamDir
 * @brief Enumeration to indicate the stream direction as input or output.
 * @ingroup RMF_MEDIASTREAMER_TYPES
 */
	enum StreamDir
	{
		INPUT = 0,
		OUTPUT
	};


	int is_default();
	int initialize();
	void reset();
	int init_complete();

	// Open a media url
	virtual int open(HTTPRequest *pRequest) = 0;

	// Open a media url
	virtual int close () = 0;

	// Read data from the stream
	virtual unsigned long read(unsigned char *buf, unsigned long size );

/**
 * @brief This function is used to check if the stream is open.
 *
 * @return bool
 * @retval TRUE Indicates that the stream is open.
 * @retval FALSE Indicates that the stream is not open.
 */
	bool isOpen() { return opened; }

/**
 * @brief This function is used to get the requested media URL.
 *
 * @return mediaUrl A pointer to the media URL.
 */
	const char* get_url() { return mediaUrl;}

/**
 * @brief This function is used to get the media streaming type.
 * The stream types can be RTP, RTSP or HTTP.
 *
 * @return StreamType
 * @retval mediaStreamType Indicates URL type.
 */
	StreamType getType() { return mediaStreamType;}

/**
 * @brief This function is used to get the stream direction such as input or output.
 *
 * @return StreamDir
 * @retval mediaStreamDir Indicates the direction of stream flow.
 */
	StreamDir getDir() { return mediaStreamDir;}

/**
 * @brief This function is used to get the total bytes of media streamed.
 *
 * @return uint64_t
 * @retval totalBytesStreamed Indicates the toltal number of bytes streamed.
 */
	uint64_t gettotalBytesStreamed() {return totalBytesStreamed;}

/**
 * @brief This function is used to save the total number of bytes streamed count.
 *
 * @return None
 */
	void savetotalBytesStreamed() {totalBytesStreamedPrev=totalBytesStreamed;}

/**
 * @brief this function is used to get the media streaming bitrate.
 *
 * @return uint64_t
 * @retval bitRate Holds the bitrate value.
 */
	uint64_t getBitRate() {return bitRate;}
	uint64_t calcBitRate(int duration);

/**
 * @brief This function is used to set the user's private data.
 *
 * @param[in] user_data User's private data to be updated.
 *
 * @return None
 */
	void setUserData(void *  user_data) {userData = user_data;}

/**
 * @brief This function is used to get the user's private data.
 *
 * @return void*
 * @retval userData A pointer to user's private data.
 */
	void * getUserData() { return userData;}

/**
 * @brief This function is used to get the media stream session Id.
 *
 * @return int
 * @retval mediaStreamSessionId Indicates the media stream session Id.
 */
	int getMediaStreamSessionId() { return mediaStreamSessionId;}

	// get the unique ID of the client/peer device connected on this stream
	virtual int getClientID (HNClientID * clientId);

	int request();

	int release();

	bool isInUse();

	int getUseCount();

	float getTrickPlayRate();
	void setTrickPlayRate(float rate);
	float getTrickPlayTimeSeek();
	void setTrickPlayTimeSeek(float pos);
	int setTrickPlayTimeSeekPts(unsigned long long seekPts);
	int getElapsedTime(unsigned long long curPosPts);
	int getMediaDuration();
	
	int64_t getTrickPlayBytePos();
	void setTrickPlayBytePos(int64_t bPos);
	int64_t getTrickPlayByteSize();
	void setTrickPlayByteSize(int64_t bSize);

	void updatePTS();

/**
 * @brief This function is used to get the recording Id.
 *
 * @return record_id Recording Id is returned.
 */
	long long getRecordId() {return record_id;}

/**
 * @brief This function is used to set the recording Id.
 *
 * @param[in] rec_id Indicates the recording Id.
 *
 * @return None
 */
	void setRecordId(long long rec_id){ record_id = rec_id;}

protected:

	MediaStream(MediaStream::StreamType _type, MediaStream::StreamDir _dir);
	virtual ~MediaStream();

	// Whether stream is open
	bool opened;

	/**
	  * Maximum length of url could be 1024
	  */
	char mediaUrl[MAX_URL_LEN];

	// user's private data
	void * userData;

	static int gHTTPInputSessionCount;
	static int gHTTPOutputSessionCount;
	static int gRTPInputSessionCount;
	static int gRTPOutputSessionCount;

	static int gMediaStreamSessionId;

	StreamType mediaStreamType;
	StreamDir mediaStreamDir;
	uint64_t totalBytesStreamed;
	uint64_t totalBytesStreamedPrev;
	uint64_t bitRate;


	int mediaStreamSessionId;

	TrickPlayParams trickPlayParams;

	// Number of users sharing this mediastream
	int usageCount;
	rmf_osal_Mutex mMutex;

	unsigned long startPTS;
	unsigned long endPTS;
	bool ptsUpdated;
	unsigned long lastGoodPTS;
	int lastPlayPos;

	// If there is a recording associated with this
	// mediastream, then the id is set here
	// This is normally the leaf_id of the recording.
	long long record_id;

	bool m_dtcpContent;
};

/**
 * @class HTTPInputMediaStream
 * @brief This class is extended from MediaStream class.
 * It is used to manage the input media stream of HTTP type
 * by providing the interface to open, close, read, get media
 * duration and so on.
 * @ingroup RMF_MEDIASTREAMER_CLASS
 */
class HTTPInputMediaStream : public MediaStream
{

	friend class MediaStreamManager;

public:

	// Open a media url
	int open(HTTPRequest *pRequest);

	// Open a media url
	int openHeadRequest(const char * url);

	// Open a media url
	int close ();

	// Read data from the stream
	unsigned long read(unsigned char *buf, unsigned long size );

	//int setPlaySpeed(int playSpeed);
	//int setTimeSeek(int timePos);
	int applyTrickPlaySettings();
	int setTrickPlayTimeSeekPts(unsigned long long seekPts);
	int getElapsedTime(unsigned long long curPosPts);
	int getMediaDuration();

	void updatePTS();

	// Set a proprietary header string
	int setHeader(const char * headerStr );

private:

	HTTPInputMediaStream(MediaStream::StreamType _type, MediaStream::StreamDir _dir);
	~HTTPInputMediaStream();

#ifdef	USE_CURL_HTTPCLIENT
	CurlHttp * curlHttpCtx;
#else
	// ffmpeg's context
//	AVFormatContext *pFormatCtx;
#endif

};


#define HTTP_TIMEOUT_WAIT_MAX   30

/**
 * @class HTTPOutputMediaStream
 * @brief This class is extended from MediaStream class.
 * It is used to manage the output media stream of HTTP type by providing the interfaces
 * to open, close, read, set/get client IP address and so on.
 * @ingroup RMF_MEDIASTREAMER_CLASS
 */
class HTTPOutputMediaStream : public MediaStream
{
	friend class MediaStreamManager;

public:
/**
 * @enum ConnectionState
 * @brief It defines an enumeration to represent HTTP connection state.
 * @ingroup RMF_MEDIASTREAMER_TYPES
 */
	enum ConnectionState
	{
		CONN_INIT = 0,
		CONN_OPENED,
		CONN_CLOSED
	};

	HTTPOutputMediaStream( MediaStream::StreamType _type, MediaStream::StreamDir _dir );
	~HTTPOutputMediaStream();

	int open(HTTPRequest *pRequest);
	int close ();

	// Read data from the stream
	unsigned long read(unsigned char *buf, unsigned long size );

/**
 * @brief This function is used to set the client IP address. 
 *
 * @param[in] remoteIp IP address to set for client.
 *
 * @return None
 */
	void setClientIpAddr(long remoteIp) {clientIpAddr = remoteIp;}

/**
 * @brief This function is used to get the client IP address.
 *
 * @return clientIpAddr Holds the client's IP address.
 */
	long getClientIpAddr() {return clientIpAddr;}
	char* getClientIpAddrStr(char *ipStr);
	
/**
 * @brief This function is used to set the http output media connection ID. 
 *
 * @param[in] id Connection ID to be set.
 *
 * @return None.
 */
	void setConnId(unsigned long id) {connId = id;}

/**
 * @brief This function is used to get the http output media connection ID. 
 *
 * @return Connection ID.
 */	
	unsigned long getConnId() {return connId;}


	void send_http_response_header(int status_code, char * msg);
	int create_http_response_header(char *buffer, int status_code, char * msg);

	void send_http_response_header_dlna(int status_code, char * msg);
	int create_http_response_header_dlna(char *buffer, int status_code, char * msg);

	// get the unique ID of the client/peer device connected on this stream
	int getClientID (HNClientID * clientId);

	HTTPRequest * getHTTPRequest (); 

	rmf_Error rmfEventHandlerAndClientNotifier(rmf_osal_eventqueue_handle_t  eventqueue, sem_t *pSessionDoneSem );

private:
	int does_client_want_keep_alive();
	int getRecordPts45k (long long leafId, unsigned long *startPTS45k, unsigned long *endPTS45k);

	char fileToServe[1024];
	bool streamingEnabled;
	ConnectionState connState;
	long clientIpAddr;
	unsigned long connId;
	HTTPRequest *mRequest;
};

#endif



/** @} */
