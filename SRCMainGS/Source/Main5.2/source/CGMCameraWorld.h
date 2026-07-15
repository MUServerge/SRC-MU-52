#pragma once

typedef struct
{
	float X;
	float Y;
	double Z;
} __Angle;

typedef struct
{
	int IsLoad;
	float zoom;			// Vertical FOV. Kept for compatibility with existing call sites.
	float distance;		// Gameplay camera distance; mouse-wheel zoom changes this value.
	__Angle Rot;
	float camAngle;
	float camWidthFar;
	//--
	float camviewFar[6];
	double WidthFar[6];
	double WidthNear[6];
}CAMERA_INFO;


class CGMCameraWorld
{
public:
	CGMCameraWorld();
	virtual~CGMCameraWorld();

	void Init();
	void Toggle();
	void Backup();
	bool IsEnable();
	void DefaultValues();
	void recover_backup_cam();

	void CalcNearFar();
	void PreparedZoom();
	void ZoomInNearFar();
	void RotateInNearFar();
	void SetAngleX(float fValue);
	void SetAngleY(float pitchDelta);
	float GetPitchOrbitHeight(float horizontalDistance) const;

	CAMERA_INFO* CurrentCam();

	float GetSmoothed_zoom_factor();
private:
	BOOL m_Enable;
	BOOL m_IsMove;
	BOOL m_IsZoom;
	__Angle cursor;
	float m_TargetDistance;
	float rotateVelocityX;
	CAMERA_INFO m_Default;
	CAMERA_INFO m_Current;
	CAMERA_INFO m_nBackup;
};
