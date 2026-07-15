#include "stdafx.h"
#include "NewUISystem.h"
#include "ZzzInterface.h"
#include "CGMCameraWorld.h"
#include "WINHANDLE.h"
#include "TextClien.h"
#include "steady_clock.h"	// FPS_ANIMATION_FACTOR - framerate-independent smoothing
#include <algorithm>	// std::copy (replaces memcpy for typed array copies)

namespace
{
	// ================= Camera tuning (F10 free-cam / CGMCameraWorld) =================
	// Single place for every zoom/tilt knob so nothing below is a bare magic number.
	// Kept in the .cpp (not the header) on purpose: CGMCameraWorld.h is pulled in via
	// the widely-included NewUISystem.h, so touching it forces a full rebuild.

	// Gameplay zoom changes camera distance, never perspective. Keeping the FOV
	// stable prevents world labels and geometry from stretching during wheel zoom.
	const float CAMW_FOV              = 35.0f;
	const float CAMW_DISTANCE_DEFAULT = 1000.0f;
	const float CAMW_DISTANCE_MIN     = 500.0f;
	const float CAMW_DISTANCE_MAX     = 1500.0f;
	// Cover the full MIN..MAX range in roughly five wheel notches. The old 50-unit
	// step needed about 24 notches and made ordinary zooming unnecessarily slow.
	const float CAMW_DISTANCE_STEP    = 250.0f;
	const float CAMW_ZOOM_EASE        = 0.30f;
	const float CAMW_ZOOM_SNAP        = 0.5f;

	// -- Pitch (vertical tilt) ------------------------------------------------------
	// Drives CameraAngle[0] / m_Current->Rot.Y in MAIN_SCENE. LESS negative = camera
	// raised higher / more top-down (GetPitchOrbitHeight: height = -d*cot(pitch)),
	// confirmed in-game. The pitch is ZOOM-COUPLED: at CAMW_PITCH_ZOOM_START and
	// beyond the camera rests top-down (CAMW_PITCH_DEFAULT); zooming in below it
	// eases the view down toward CAMW_PITCH_NEAR, so a fully zoomed-in camera looks
	// straight at the hero instead of at the ground around them. Middle-drag adds a
	// small manual offset (up to CAMW_PITCH_DRAG_RANGE deg further down) on top.
	const float CAMW_PITCH_DEFAULT    = -42.0f;	// resting tilt at/beyond ZOOM_START (top-down)
	const float CAMW_PITCH_NEAR       = -78.0f;	// tilt at full zoom-in (straight at the hero)
	const float CAMW_PITCH_ZOOM_START = 1000.0f;	// distance where the zoom-tilt coupling begins
	const float CAMW_PITCH_MAX        = -42.0f;	// most top-down allowed
	const float CAMW_PITCH_DRAG_RANGE = 10.0f;	// manual middle-drag range below the zoom tilt
	const int   CAMW_PITCH_LEVEL_COUNT = 5;		// five fixed vertical camera positions
	const float CAMW_PITCH_LEVEL_STEP = CAMW_PITCH_DRAG_RANGE / (CAMW_PITCH_LEVEL_COUNT - 1);
	const float CAMW_FOCUS_HEIGHT_NEAR = 120.0f;	// full zoom-in focus lift toward the hero's head

	// Manual middle-drag tilt, applied under the zoom-coupled pitch. File-static on
	// purpose: adding a class member would touch the widely-included header.
	float s_fPitchDragOffset = 0.0f;

	// Thin wrapper so call sites read the same as before; std::clamp does the work.
	float ClampCameraWorldValue(float value, float minValue, float maxValue)
	{
		return std::clamp(value, minValue, maxValue);
	}

	// Normalized close-zoom amount with a smooth start and finish. Shared by pitch
	// and focus height so the camera never changes its orbit and target in two
	// visibly different phases.
	float NearZoomFactor(float distance)
	{
		float t = (CAMW_PITCH_ZOOM_START - distance) / (CAMW_PITCH_ZOOM_START - CAMW_DISTANCE_MIN);
		t = ClampCameraWorldValue(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// Zoom-coupled pitch: top-down at/beyond ZOOM_START, smoothstep down to NEAR at
	// DISTANCE_MIN so the tilt eases gently at both ends of the zoom travel.
	// (Keep ZOOM_START > CAMW_DISTANCE_MIN.)
	float PitchFromDistance(float distance)
	{
		return CAMW_PITCH_DEFAULT
			+ (CAMW_PITCH_NEAR - CAMW_PITCH_DEFAULT) * NearZoomFactor(distance);
	}

	// Final pitch = zoom-coupled tilt + manual drag offset, never above top-down.
	float ComposePitch(float distance)
	{
		return ClampCameraWorldValue(
			PitchFromDistance(distance) + s_fPitchDragOffset, CAMW_PITCH_NEAR - CAMW_PITCH_DRAG_RANGE, CAMW_PITCH_MAX);
	}
}

CGMCameraWorld::CGMCameraWorld()
{
	m_Enable = 0;
	m_IsMove = 0;
	m_IsZoom = 0;
	cursor.X = 0;
	cursor.Y = 0;
	rotateVelocityX = 0.0f;
	m_TargetDistance = CAMW_DISTANCE_DEFAULT;

	m_Default.IsLoad = 0;

	m_Current.zoom = CAMW_FOV;
	m_Current.distance = CAMW_DISTANCE_DEFAULT;
	// Gameplay camera pitch (CameraAngle[0] in MAIN_SCENE, ZzzScene.cpp). Less
	// negative = camera raised higher, looking more top-down at the hero. The pitch
	// is coupled to the zoom distance; tune via CAMW_PITCH_* at the top of this file.
	m_Current.Rot.Y = PitchFromDistance(CAMW_DISTANCE_DEFAULT);
	m_Current.Rot.Z = 150.0;
	m_Current.camAngle = 45.0;

	m_Default.zoom = CAMW_FOV;
	m_Default.distance = CAMW_DISTANCE_DEFAULT;
	m_Default.Rot.Y = m_Current.Rot.Y;	// keep in sync with m_Current (camera pitch)
	m_Default.Rot.Z = 150.0;
	m_Default.camAngle = 45.0;
}

CGMCameraWorld::~CGMCameraWorld()
{
}

void CGMCameraWorld::Init()
{
	ResolutionConfig* conf = gwinhandle->LoadCurrentConfig();

	// These are world-space values. Scaling them by pixel resolution made the GL
	// far plane grow much faster than the terrain frustum at high resolutions,
	// producing both black corners and unnecessary overdraw.
	m_Current.WidthFar[0] = 3000.0;
	m_Current.WidthFar[1] = 2250.0;
	m_Current.WidthFar[2] = 1300.0;
	m_Current.WidthFar[3] = 1250.0;
	m_Current.WidthFar[5] = 1900.0;

	m_Current.WidthNear[0] = 540.0;
	m_Current.WidthNear[1] = 540.0;
	m_Current.WidthNear[2] = 580.0;
	m_Current.WidthNear[3] = 540.0;
	m_Current.WidthNear[5] = 600.0;

	m_Current.camviewFar[0] = 8500.0; //-- WD_6STADIUM
	m_Current.camviewFar[1] = 5100.0; //-- WD_30BATTLECASTLE_2
	m_Current.camviewFar[2] = 3300.0; //-- WD_30BATTLECASTLE
	m_Current.camviewFar[3] = 2850.0; //-- WD_62SANTA_TOWN
	m_Current.camviewFar[5] = 3300.0;

	if (conf)
	{
		m_Current.camWidthFar = conf->view_far;
		m_Current.WidthFar[4] = conf->width_far;
		m_Current.WidthNear[4] = conf->width_near;
		m_Current.camviewFar[4] = conf->view_far; //-- General
	}
	else
	{
		m_Current.camWidthFar = 3200.0;
		m_Current.WidthFar[4] = 1600.0;
		m_Current.WidthNear[4] = 500.0;
		m_Current.camviewFar[4] = 3200.0; //-- General
	}

	m_nBackup = m_Current;
	m_Default.camWidthFar = m_Current.camWidthFar;
	std::copy(std::begin(m_Current.WidthFar), std::end(m_Current.WidthFar), std::begin(m_Default.WidthFar));
	std::copy(std::begin(m_Current.WidthNear), std::end(m_Current.WidthNear), std::begin(m_Default.WidthNear));
	std::copy(std::begin(m_Current.camviewFar), std::end(m_Current.camviewFar), std::begin(m_Default.camviewFar));
}

void CGMCameraWorld::Toggle()
{
	m_Enable = !m_Enable;

	if (m_Enable != 0)
		CreateNotice(gTextClien.TextClien_Khac[0], 0); //"Cam ONLINE"
	else
		CreateNotice(gTextClien.TextClien_Khac[1], 0); //"Cam OFFLINE"

	if (((m_Default.IsLoad == 0) ? (m_Default.IsLoad++) : m_Default.IsLoad) == 0)
	{
		m_Default.Rot.X = CameraAngle[2];
	}
}

void CGMCameraWorld::Backup()
{
	if (this->m_Enable != 0 && SceneFlag == MAIN_SCENE)
	{
		this->DefaultValues();
	}
}

bool CGMCameraWorld::IsEnable()
{
	return this->m_Enable!=0;
}

void CGMCameraWorld::DefaultValues()
{
	CameraAngle[2] = m_Default.Rot.X;
	m_Current.zoom = m_Default.zoom;
	m_Current.distance = m_Default.distance;
	m_TargetDistance = m_Default.distance;
	s_fPitchDragOffset = 0.0f;	// manual tilt resets with the rest of the camera
	m_Current.Rot.X = m_Default.Rot.X;
	m_Current.Rot.Y = m_Default.Rot.Y;
	m_Current.Rot.Z = m_Default.Rot.Z;
	m_Current.camAngle = m_Default.camAngle;
	m_Current.camWidthFar = m_Default.camWidthFar;
	std::copy(std::begin(m_Default.WidthFar), std::end(m_Default.WidthFar), std::begin(m_Current.WidthFar));
	std::copy(std::begin(m_Default.WidthNear), std::end(m_Default.WidthNear), std::begin(m_Current.WidthNear));
	std::copy(std::begin(m_Default.camviewFar), std::end(m_Default.camviewFar), std::begin(m_Current.camviewFar));
}

void CGMCameraWorld::recover_backup_cam()
{
	m_Current = m_nBackup;
	m_TargetDistance = m_Current.distance;
}

void CGMCameraWorld::CalcNearFar()
{
	const float distanceScale = ClampCameraWorldValue(
		m_Current.distance / m_Default.distance, 0.8f, 1.5f);
	const float pitchCoverage = 1.0f + ClampCameraWorldValue(
		fabsf(m_Current.Rot.Y - m_Default.Rot.Y) * 0.005f, 0.0f, 0.20f);
	const float coverageScale = distanceScale * pitchCoverage;

	m_Current.camWidthFar = m_Default.camWidthFar * coverageScale;

	for (int i = 0; i < 6; i++)
	{
		m_Current.camviewFar[i] = m_Default.camviewFar[i] * coverageScale;
		m_Current.WidthFar[i] = m_Default.WidthFar[i] * coverageScale;
		m_Current.WidthNear[i] = m_Default.WidthNear[i] * distanceScale;
	}
	m_nBackup = m_Current;
}

void CGMCameraWorld::PreparedZoom()
{
	m_IsZoom = 0;
	m_TargetDistance = m_Current.distance;
	rotateVelocityX = 0.0f;
}

void CGMCameraWorld::ZoomInNearFar()
{
	if (m_Enable == 0 || SceneFlag != MAIN_SCENE)
	{
		m_IsZoom = 0;
		return;
	}

	// Read the wheel only over the world (not the UI) and accumulate it into a
	// PERSISTENT target, so rapid scrolls add up and glide instead of restarting
	// each tick (the old code reset the whole interpolation on every wheel event).
	if (MouseWheel != 0
		&& !g_pNewUISystem->CheckMouseUse() && MouseOnWindow == 0
		&& SEASON3B::CheckMouseIn(0, 0, GetScreenWidth(), GetWindowsY)
		&& g_dwMouseUseUIID == 0)
	{
		if (!m_IsZoom)
			m_TargetDistance = m_Current.distance;

		const bool zoomingIn = (MouseWheel > 0);
		m_TargetDistance += zoomingIn ? -CAMW_DISTANCE_STEP : CAMW_DISTANCE_STEP;
		m_TargetDistance = ClampCameraWorldValue(m_TargetDistance, CAMW_DISTANCE_MIN, CAMW_DISTANCE_MAX);

		if (zoomingIn && m_TargetDistance <= CAMW_DISTANCE_MIN)
		{
			// The last close-zoom rung is a deliberate camera-mode transition.
			// Land on its final distance, pitch and focus in this frame instead of
			// easing through an awkward near-ground intermediate view.
			m_Current.distance = CAMW_DISTANCE_MIN;
			m_Current.zoom = CAMW_FOV;
			m_Current.Rot.Y = ComposePitch(m_Current.distance);
			m_IsZoom = FALSE;
			CalcNearFar();
		}
		else
		{
			m_IsZoom = TRUE;
		}

		MouseWheel = 0;
	}

	// Framerate-independent exponential ease toward the target. FPS_ANIMATION_FACTOR
	// is the per-frame normalizer (1.0 @ 25 FPS), so the glide feels identical at any
	// FPS. The glide keeps running even if the cursor moves onto the UI mid-zoom.
	if (m_IsZoom)
	{
		const float factor = ClampCameraWorldValue(
			CAMW_ZOOM_EASE * (float)FPS_ANIMATION_FACTOR, 0.0f, 1.0f);
		m_Current.distance += (m_TargetDistance - m_Current.distance) * factor;
		m_Current.zoom = CAMW_FOV;

		if (fabsf(m_TargetDistance - m_Current.distance) < CAMW_ZOOM_SNAP)
		{
			m_Current.distance = m_TargetDistance;
			m_IsZoom = 0;
		}

		// Dynamic tilt: follow the gliding distance every frame, so zooming in
		// smoothly lowers the view toward the hero and zooming out restores top-down.
		m_Current.Rot.Y = ComposePitch(m_Current.distance);

		CalcNearFar();
	}
}

void CGMCameraWorld::RotateInNearFar()
{
	// Manual camera rotation/tilt is intentionally disabled. The mouse wheel is
	// the only camera control; it keeps the existing zoom-coupled camera position.
	m_IsMove = FALSE;
	rotateVelocityX = 0.0f;
}

void CGMCameraWorld::SetAngleX(float fValue)
{
	if (m_Current.Rot.X > 309.0f || m_Current.Rot.X < -417.0f)
		m_Current.Rot.X = -45.0;
	else
		m_Current.Rot.X += fValue;

	CalcNearFar();
	CameraAngle[2] = m_Current.Rot.X;
	m_Current.camAngle = -m_Current.Rot.X;
}

void CGMCameraWorld::SetAngleY(float pitchDelta)
{
	// The drag adjusts a bounded manual offset; the zoom-coupled base pitch stays
	// owned by the current camera distance (see ComposePitch).
	const float oldOffset = s_fPitchDragOffset;
	s_fPitchDragOffset = ClampCameraWorldValue(oldOffset + pitchDelta, -CAMW_PITCH_DRAG_RANGE, 0.0f);

	if (s_fPitchDragOffset != oldOffset)
	{
		m_Current.Rot.Y = ComposePitch(m_Current.distance);
		CalcNearFar();
	}
}

float CGMCameraWorld::GetPitchOrbitHeight(float horizontalDistance) const
{
	// The world camera starts horizontalDistance units behind the hero. Derive
	// the vertical leg from the actual pitch. At close zoom, lift the orbit target
	// toward the hero's upper body instead of keeping the feet at screen centre.
	// The lift shares the same smoothstep as the pitch, giving the newer-season
	// camera feel without a visible position snap at the transition point.
	const float pitchRadians = m_Current.Rot.Y * (Q_PI / 180.0f);
	const float pitchSin = sinf(pitchRadians);

	if (fabsf(pitchSin) < 0.001f)
		return 0.0f;

	const float orbitHeight = -horizontalDistance * cosf(pitchRadians) / pitchSin;
	const float focusHeight = CAMW_FOCUS_HEIGHT_NEAR * NearZoomFactor(m_Current.distance);
	return orbitHeight + focusHeight;
}

CAMERA_INFO* CGMCameraWorld::CurrentCam()
{
	return &m_Current;
}

float CGMCameraWorld::GetSmoothed_zoom_factor()
{
	return m_Current.distance / m_Default.distance;
}
