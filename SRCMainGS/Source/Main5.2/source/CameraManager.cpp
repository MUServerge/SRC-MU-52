#include "stdafx.h"
#include "CameraManager.h"
#include <string>	// std::stof (replaces atof)
#include <stdexcept>	// std::invalid_argument / std::out_of_range from std::stof

extern int GetScreenWidth();

CCameraManager* CCameraManager::Instance()
{
	static CCameraManager sInstance;
	return &sInstance;
}

CCameraManager::CCameraManager()
{
	Enable = false;
	Ratio = 1.0;
	CameraFOV = 55.0;
	CameraDistance = 1000.0;
	CameraDistanceTarget = 80.0;

	CameraAngle[0] = 0.0;
	CameraAngle[1] = 0.0;
	CameraAngle[2] = 0.0;
	CameraPosition[0] = 0.0;
	CameraPosition[1] = 0.0;
	CameraPosition[2] = 0.0;

	CameraWidthFOV[0] = 0.0;
	CameraWidthFOV[1] = 0.0;
	CameraWidthFOV[2] = 0.0;
	CameraWidthFOV[3] = 0.0;
	CameraWidthFOV[4] = 0.0;
	CameraWidthFOV[5] = 0.0;
	CameraWidthFOV[6] = 0.0;
	CameraWidthFOV[7] = 0.0;
	CameraWidthFOV[8] = 0.0;
}

CCameraManager::~CCameraManager()
{
}

void CCameraManager::OpenCameraSetting(char* WorldName)
{
	Enable = false;

	int version = Open_Camera_Angle_Position(WorldName);

	if (version == 0)
		return;

	Enable = true;

	char FileName[64];
	char ReturnedString[MAX_PATH];

	// snprintf instead of sprintf: WorldName is caller-provided and could in
	// principle exceed the 64-byte buffer, so bound the write explicitly.
	snprintf(FileName, sizeof(FileName), "Data\\%s\\CAP.ini", WorldName);

	// Reads one INI value and parses it as a float, replacing the previous
	// GetPrivateProfileString+atof pair that was duplicated 17 times below.
	// The buffer size passed to GetPrivateProfileString now matches the real
	// buffer (sizeof(ReturnedString)) instead of the old hardcoded 0xB/0x1E,
	// which understated it and could silently truncate longer values.
	auto ReadFloat = [&](const char* section, const char* key, const char* defaultValue) -> float
	{
		GetPrivateProfileString(section, key, defaultValue, ReturnedString, sizeof(ReturnedString), FileName);
		try
		{
			return std::stof(ReturnedString);
		}
		catch (const std::exception&)
		{
			return 0.0f;
		}
	};

	CameraAngle[0] = ReadFloat("CAMERA ANGLE", "Angle X", "\0");
	CameraAngle[1] = ReadFloat("CAMERA ANGLE", "Angle Y", "\0");
	CameraAngle[2] = ReadFloat("CAMERA ANGLE", "Angle Z", "\0");

	CameraPosition[0] = ReadFloat("CAMERA POSITION", "Position X", "\0");
	CameraPosition[1] = ReadFloat("CAMERA POSITION", "Position Y", "\0");
	CameraPosition[2] = ReadFloat("CAMERA POSITION", "Position Z", "\0");

	CameraDistance = ReadFloat("CAMERA DISTANCE", "Distance", "\0");
	CameraDistanceTarget = ReadFloat("CAMERA DISTANCE", "Z_Distance", "\0");

	CameraFOV = ReadFloat("CAMERA FOV", "FOV", "35.000000");

	CameraWidthFOV[0] = ReadFloat("CAMERA FOV", "FOV1", "35.000000");
	CameraWidthFOV[1] = ReadFloat("CAMERA FOV", "FOV2", "35.000000");
	CameraWidthFOV[2] = ReadFloat("CAMERA FOV", "FOV3", "35.000000");
	CameraWidthFOV[3] = ReadFloat("CAMERA FOV", "FOV4", "35.000000");
	CameraWidthFOV[4] = ReadFloat("CAMERA FOV", "FOV5", "35.000000");
	CameraWidthFOV[5] = ReadFloat("CAMERA FOV", "FOV6", "35.000000");
	CameraWidthFOV[6] = ReadFloat("CAMERA FOV", "FOV7", "35.000000");
	CameraWidthFOV[7] = ReadFloat("CAMERA FOV", "FOV8", "35.000000");
	CameraWidthFOV[8] = ReadFloat("CAMERA FOV", "FOV9", "35.000000");

	//Ratio = ReadFloat("CAMERA RATIO", "Ratio", "\0");
	DeleteFile(FileName);
}

float CCameraManager::GetFov()
{
	if (m_Resolution >= 0 && m_Resolution < 9)
	{
		return CameraWidthFOV[m_Resolution];
	}
	else
	{
		return CameraFOV;
	}
}

float CCameraManager::GetDistance()
{
	return CameraDistance;
}

float CCameraManager::GetDistanceTarget()
{
	return CameraDistanceTarget;
}

void CCameraManager::GetAngle(vec3_t Angle)
{
	VectorCopy(CameraAngle, Angle);
}

void CCameraManager::GetPosition(vec3_t Position)
{
	VectorCopy(CameraPosition, Position);
}

double CCameraManager::CalcViewFar(float a2)
{
	return (float)((float)((float)(a2 * CameraDistance) * CameraDistanceTarget) * Ratio);
}

double CCameraManager::CalcWidthCam()
{
	float Width = ((float)GetScreenWidth() / GetWindowsX);
	return (float)((float)((float)(Width * CameraDistance) * CameraDistanceTarget) * Ratio);
}
