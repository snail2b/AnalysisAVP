﻿/*
 * @Author: gongluck
 * @Date: 2020-10-03 15:36:42
 * @Last Modified by: gongluck
 * @Last Modified time: 2020-10-04 13:50:02
 */

#include <iostream>
#include <fstream>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#pragma comment(lib, "WS2_32.lib")
#endif

#include "../../../analysis/flv/flv.h"

extern "C"
{
#include "rtmp.h"
}

int main(int argc, char* argv[])
{
	std::cout << "librtmp example" << std::endl;

	std::cout << "Usage : " << "thisfile pushurl h264file aacfile." << std::endl;

	if (argc < 4)
	{
		std::cerr << "please see the usage message." << std::endl;
		return -1;
	}
	std::ofstream h264(argv[2], std::ios::binary | std::ios::trunc);
	if (h264.fail())
	{
		std::cerr << "can not open file " << argv[2] << std::endl;
		return -1;
	}
	std::ofstream aac(argv[3], std::ios::binary | std::ios::trunc);
	if (aac.fail())
	{
		std::cerr << "can not open file " << argv[3] << std::endl;
		return -1;
	}

#ifdef _WIN32
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);
	WSAStartup(version, &wsaData);
#endif

	RTMP* rtmp = RTMP_Alloc();
	RTMP_Init(rtmp);
	auto rtmpres = RTMP_SetupURL(rtmp, argv[1]);
	//RTMP_EnableWrite(rtmp);//推流要设置写
	rtmpres = RTMP_Connect(rtmp, nullptr);
	rtmpres = RTMP_ConnectStream(rtmp, 0);

	RTMPPacket packet = { 0 };
	char nalu[] = { 0x00, 0x00, 0x00, 0x01 };
	FLVVIDEOTAG* video = nullptr;
	auto rent = 0;
	while (true)
	{
		rent = RTMP_ReadPacket(rtmp, &packet);
		if (rent <= 0)
		{
			break;
		}

		if (packet.m_body != nullptr)
		{
			switch (packet.m_packetType)
			{
			case FLV_TAG_TYPE_AUDIO:
				break;
			case FLV_TAG_TYPE_VIDEO:
				video = reinterpret_cast<FLVVIDEOTAG*>(packet.m_body);
				if (video->codecid == FLV_VIDEO_CODECID_AVC)
				{
					switch (video->videopacket.avcvideopacket.avcpacketype)
					{
					case AVC_PACKET_HEADER:
					{
						AVCDecoderConfigurationRecordHeader* configheader =
							reinterpret_cast<AVCDecoderConfigurationRecordHeader*>(video->videopacket.avcvideopacket.avcpacketdata);

						SequenceParameterSet* sps =
							reinterpret_cast<SequenceParameterSet*>(configheader->data);
						auto datasize = FLVINT16TOINT((sps->sequenceParameterSetLength));
						h264.write(nalu, sizeof(nalu));
						h264.write(reinterpret_cast<char*>(sps->sequenceParameterSetNALUnit), datasize);

						PictureParameterSet* pps =
							reinterpret_cast<PictureParameterSet*>(sps->sequenceParameterSetNALUnit + FLVINT16TOINT(sps->sequenceParameterSetLength));
						datasize = FLVINT16TOINT((pps->pictureParameterSetLength));
						h264.write(nalu, sizeof(nalu));
						h264.write(reinterpret_cast<char*>(pps->pictureParameterSetNALUnit), datasize);
					}
					break;
					case AVC_PACKET_NALU:
					{
						auto nalsize = reinterpret_cast<FLVINT32*>(&video->videopacket.avcvideopacket.avcpacketdata[0]);
						auto datasize = FLVINT32TOINT((*nalsize));
						h264.write(nalu, sizeof(nalu));
						h264.write(reinterpret_cast<char*>(&video->videopacket.avcvideopacket.avcpacketdata[0 + 4]), datasize);
					}
					break;
					case AVC_PACKET_END:
					{
						h264.flush();
					}
					break;
					default:
						break;
					}
				}
			}
		}
	}

	RTMP_Close(rtmp);
	RTMP_Free(rtmp);
	h264.close();
	aac.close();

#ifdef _WIN32
	WSACleanup();
#endif

	return 0;
}
