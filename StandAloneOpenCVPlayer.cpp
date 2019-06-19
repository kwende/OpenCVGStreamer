// StandAloneOpenCVPlayer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <gst/gst.h>
#include <stdio.h>

struct Context {
	GstElement* pipeline;
	GstElement* rtspsrc;
	GstElement* parse;
	GstElement* depay;
	GstElement* appsink;
	GstElement* decode;
};

static GstFlowReturn OnNewSample(GstElement* sink, void* data)
{
	GstSample* sample;
	/* Retrieve the buffer */
	g_signal_emit_by_name(sink, "pull-sample", &sample);
	if (sample) {
		GstBuffer* buffer = gst_sample_get_buffer(sample);
		GstClockTime time = buffer->pts / 1000000.0;

		GstCaps* caps = gst_sample_get_caps(sample);
		GstStructure* capsStructure = gst_caps_get_structure(caps, 0);
		gint width, height;
		gst_structure_get_int(capsStructure, "width", &width);
		gst_structure_get_int(capsStructure, "height", &height);

		GstMapInfo info;
		gst_buffer_map(buffer, &info, GST_MAP_READ);

		cv::Mat picYV12 = cv::Mat(height * 3 / 2, width, CV_8UC1, info.data);
		cv::Mat picBGR;
		cv::cvtColor(picYV12, picBGR, cv::COLOR_YUV2RGB_NV12);
		cv::medianBlur(picBGR, picBGR, 21);
		char szNumBuf[25]; 
		::sprintf_s(szNumBuf, "%d", time); 
		cv::putText(picBGR, szNumBuf, cv::Point(300, 100), cv::FONT_HERSHEY_PLAIN, 4, cv::Scalar(255, 255, 255, 0));
		cv::imshow("Test", picBGR);
		cv::waitKey(1);

		gst_buffer_unmap(buffer, &info);
		gst_sample_unref(sample);
	}
	return GST_FLOW_OK;
}

static void OnNewPad(GstElement* element, GstPad* pad, gpointer data)
{
	Context* context = (Context*)data;
	char* name = element->object.name;
	if (strcmp("rtspsrc", name) == 0)
	{
		gboolean linked = ::gst_element_link(element, context->depay);
		linked = ::gst_element_link(context->depay, context->parse);
		linked = ::gst_element_link(context->parse, context->decode);
		linked = ::gst_element_link(context->decode, context->appsink);

		//::g_object_set(context->appsink, "emit-signals", TRUE);
		::g_signal_connect(context->appsink,
			"new-sample", G_CALLBACK(OnNewSample), NULL);

		g_object_set(context->appsink,
			"emit-signals", TRUE, NULL); 

		g_object_set(context->appsink,
			"sync", FALSE,
			"enable-last-sample", FALSE,
			"emit-signals", TRUE,
			"qos", FALSE,
			"max-buffers", 1,
			"async", FALSE,
			"drop", FALSE, NULL);
	}
}

int main()
{
	const char* RTSPURL = "rtsp://address/live"; 

	gst_init(NULL, NULL); 

	Context* context = new Context(); 
	context->pipeline = ::gst_pipeline_new("pipeline");

	context->rtspsrc = ::gst_element_factory_make("rtspsrc", "rtspsrc");
	g_object_set(G_OBJECT(context->rtspsrc), "location", RTSPURL, NULL);
	g_object_set(G_OBJECT(context->rtspsrc), "latency", 0, NULL);
	g_object_set(G_OBJECT(context->rtspsrc), "buffer-mode", 0, NULL);
	g_object_set(G_OBJECT(context->rtspsrc), "drop-on-latency", (gboolean)TRUE, NULL);

	context->depay = ::gst_element_factory_make("rtph264depay", "rtph264depay");
	context->parse = ::gst_element_factory_make("h264parse", "h264parse");
	context->decode = ::gst_element_factory_make("mfxh264dec", "mfxh264dec");
	context->appsink = ::gst_element_factory_make("appsink", "appsink");

	::gst_bin_add_many(GST_BIN(context->pipeline),
		context->rtspsrc,
		context->depay,
		context->parse,
		context->decode,
		context->appsink, NULL);

	::g_signal_connect(context->rtspsrc,
		"pad-added", G_CALLBACK(OnNewPad), context);
	
	::gst_element_set_state(context->pipeline, GstState::GST_STATE_PLAYING);

	std::cout << "Press ENTER to quit." << std::endl; 
	getchar(); 

	return 0; 
}
