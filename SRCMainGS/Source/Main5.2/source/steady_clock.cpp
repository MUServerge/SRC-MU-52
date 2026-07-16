#include "stdafx.h"
#include "SkillManager.h"
#include "ZzzOpenglUtil.h"
#include "steady_clock.h"
#include "wsclientinline.h"
#include "Time/Timer.h"
#include "CGMProtect.h"

csteady_clock::csteady_clock()
{
	ping_reg_id = 0;
	counterframe = 0;
	deltaAccumulated = 0.0;
	fixedUpdateAccumulator = 1.0 / REFERENCE_FPS;
	fixedUpdateStepCount = 0;
	droppedFixedUpdateStepCount = 0;
	fpsNormalizer = REFERENCE_FPS;
	realDeltaTime = 1.0 / REFERENCE_FPS;
	realLegacyStep = 1.0;

	speedNormalizer = 1.0;
	normal_check = true;

	frame_limit = 1.f;
	framePacingMode = FRAME_PACING_SOFTWARE;
	//mainthread = GetTickCount64();

	mainthread = std::chrono::steady_clock::now();

	threadTime = new CTimer();
}

csteady_clock::~csteady_clock()
{
	SAFE_DELETE(threadTime);
}

bool csteady_clock::CheckNormalizer()
{
	return normal_check;
}

double csteady_clock::GetNormalizerFps()
{
	return GetLegacyVisualStep();
}

double csteady_clock::GetFrameDeltaSeconds()
{
	return realDeltaTime;
}

double csteady_clock::GetDeltaTimeSeconds()
{
	return GetFrameDeltaSeconds();
}

double csteady_clock::GetLegacyUpdateStep()
{
	return realLegacyStep;
}

double csteady_clock::GetLegacyVisualStep()
{
	return speedNormalizer;
}

double csteady_clock::GetRealLegacyStep()
{
	return GetLegacyUpdateStep();
}

double csteady_clock::GetVisualLegacyStep()
{
	return GetLegacyVisualStep();
}

double csteady_clock::GetDeltAccumulated()
{
	return deltaAccumulated;
}

int csteady_clock::GetFixedUpdateStepCount() const
{
	return fixedUpdateStepCount;
}

int csteady_clock::GetDroppedFixedUpdateStepCount() const
{
	return droppedFixedUpdateStepCount;
}

double csteady_clock::GetFixedUpdateAlpha() const
{
	const double fixedStepSeconds = 1.0 / REFERENCE_FPS;
	if (fixedStepSeconds <= 0.0)
	{
		return 0.0;
	}

	const double alpha = fixedUpdateAccumulator / fixedStepSeconds;
	if (alpha <= 0.0)
		return 0.0;
	if (alpha >= 1.0)
		return 1.0;
	return alpha;
}

int csteady_clock::GetLimitFps()
{
	return 60;
}

void csteady_clock::SetFramePacingMode(FramePacingMode mode)
{
	framePacingMode = mode;
}

FramePacingMode csteady_clock::GetFramePacingMode() const
{
	return framePacingMode;
}

bool csteady_clock::IsSoftwareFrameLimitEnabled() const
{
	return framePacingMode == FRAME_PACING_SOFTWARE;
}

double csteady_clock::Getframe_per_second()
{
	return 1000.0 / static_cast<double>(this->GetLimitFps());
}

void csteady_clock::normalizefps()
{
	const double fixedStepSeconds = 1.0 / REFERENCE_FPS;
	const int maxFixedStepsPerFrame = 5;
	const double maxAccumulatedSeconds = fixedStepSeconds * maxFixedStepsPerFrame;

	fixedUpdateStepCount = 0;
	droppedFixedUpdateStepCount = 0;

	double pendingSeconds = fixedUpdateAccumulator + realDeltaTime;
	if (pendingSeconds > maxAccumulatedSeconds)
	{
		const double discardedSeconds = pendingSeconds - maxAccumulatedSeconds;
		droppedFixedUpdateStepCount = static_cast<int>(discardedSeconds / fixedStepSeconds);
		pendingSeconds = maxAccumulatedSeconds;
	}
	else if (pendingSeconds < 0.0)
	{
		pendingSeconds = 0.0;
	}

	fixedUpdateStepCount = static_cast<int>(pendingSeconds / fixedStepSeconds);
	if (fixedUpdateStepCount > maxFixedStepsPerFrame)
	{
		fixedUpdateStepCount = maxFixedStepsPerFrame;
	}

	fixedUpdateAccumulator = pendingSeconds -
		(static_cast<double>(fixedUpdateStepCount) * fixedStepSeconds);
	if (fixedUpdateAccumulator < 0.0)
	{
		fixedUpdateAccumulator = 0.0;
	}

	normal_check = fixedUpdateStepCount > 0;
}

void csteady_clock::LoadInformationFps()
{
	static bool timeinit = false;

	if (!timeinit)
	{
		timeinit = true;
		counterframe = 0;
		frame_limit = 1.0;
		FPS = this->GetLimitFps();
		mainthread = std::chrono::steady_clock::now();
		save_time = threadTime->GetTimeElapsed();
	}

	auto current_time = threadTime->GetTimeElapsed();

	// session-relative ms: absolute timeGetTime() exceeds float precision after
	// hours of OS uptime (32ms+ steps -> jerky WorldTime-driven animations)
	WorldTime = static_cast<float>(current_time);

	double difTime = (current_time - save_time);

	realDeltaTime = (difTime <= 0.0) ? (1.0 / static_cast<double>(GetLimitFps())) : (difTime * 0.001);
	DeltaT = realDeltaTime;
	fpsNormalizer = 1.0 / realDeltaTime;
	realLegacyStep = realDeltaTime * REFERENCE_FPS;

	speedNormalizer = (double)min(realLegacyStep, 1.0);
	save_time = current_time;
	counterframe++;

	bool finishwating = frame_limit.hasElapsed();
	//
	if (finishwating)
	{
		FPS = static_cast<float>(counterframe);
		counterframe = 0;
	}

	if (SceneFlag == MAIN_SCENE)
	{
		if (finishwating)
		{
			runtime_send_ping();
		}
		gSkillManager.CalcSkillDelay(static_cast<int>(difTime));
	}

	deltaAccumulated += speedNormalizer;

	normalizefps();
}

std::chrono::steady_clock::time_point csteady_clock::GetthreadTime()
{
	// The returned timestamp belongs to the frame that is about to be processed.
	// The previous implementation returned the prior frame start and made the
	// limiter react to a stale interval instead of the current frame's work.
	mainthread = std::chrono::steady_clock::now();
	return mainthread;
}

double csteady_clock::thread_sleep(const std::chrono::steady_clock::time_point frameStart)
{
	auto now = std::chrono::steady_clock::now();

	if (!IsSoftwareFrameLimitEnabled())
	{
		mainthread = now;
		return std::chrono::duration<double, std::milli>(now - frameStart).count();
	}

	const int limitFps = this->GetLimitFps();
	if (limitFps <= 0)
	{
		mainthread = now;
		return std::chrono::duration<double, std::milli>(now - frameStart).count();
	}

	const auto targetFrameDuration = std::chrono::duration<double>(
		1.0 / static_cast<double>(limitFps));
	const auto frameDeadline = frameStart + std::chrono::duration_cast<
		std::chrono::steady_clock::duration>(targetFrameDuration);

	if (now < frameDeadline)
	{
		const auto remaining = frameDeadline - now;
		const auto spinReserve = std::chrono::microseconds(1500);

		if (remaining > spinReserve)
		{
			std::this_thread::sleep_for(remaining - spinReserve);
		}

		while ((now = std::chrono::steady_clock::now()) < frameDeadline)
		{
		}
	}

	mainthread = now;
	return std::chrono::duration<double, std::milli>(now - frameStart).count();
}

bool csteady_clock::rand_calc_check(int fr)
{
	static std::random_device rd;  // a seed source for the random number engine
	static std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
	static std::uniform_real_distribution<> distrib(0.0, 1.0);

	const auto rand_value = distrib(gen);
	const auto chance = (fr == 1) ? speedNormalizer : (1.0 / fr) * speedNormalizer;

	return rand_value <= chance;
}

void csteady_clock::runtime_send_ping()
{
	int ping_id = ping_reg_id++;

	SendPing(ping_id);

	pingMap[ping_id] = std::chrono::high_resolution_clock::now();
}

void csteady_clock::runtime_recv_ping(int ping_id)
{
	if (pingMap.find(ping_id) != pingMap.end())
	{
		auto endTime = std::chrono::high_resolution_clock::now();
		auto startTime = pingMap[ping_id];
		pingMap.erase(ping_id);
		ping_time = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
	}
}

int csteady_clock::get_ping_time()
{
	return ping_time;
}

csteady_clock* csteady_clock::Instance()
{
	static csteady_clock sInstance;
	return &sInstance;
}

DWORD standlimit(DWORD x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	return (gsteady_clock->GetLimitFps() / REFERENCE_FPS * x);
}

double timepow(double x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	return pow(x, gsteady_clock->GetRealLegacyStep());
}

double timefac(double x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	return (x * gsteady_clock->GetRealLegacyStep());
}

double timeNormalizer(double x)
{
	if (gsteady_clock->GetLimitFps() == (int)REFERENCE_FPS)
		return x;

	return (x * gsteady_clock->GetVisualLegacyStep());
}

bool steady_clock_::numeral(int element) const
{
	if (_runvalueback >= _runvalue)
		return (_runvalueback >= element && _runvalue <= element);
	else
		return (_runvalue >= element && _runvalueback <= element);
}

bool steady_clock_::duration(int element)
{
	if (_runvalueback >= _runvalue)
	{
		return _runvalue <= (std::floor((_runvalueback / element)) * element);
	}
	else
	{
		return _runvalue >= (std::ceil((_runvalueback / element)) * element);
	}
}

bool steady_clock_::residual_duration(int element, int time)
{
	int multiplo;
	double residual_memory1, residual_memory2;

	if (_runvalueback >= _runvalue)
	{
		multiplo = (std::floor((_runvalueback / element)) * element);

		residual_memory1 = (_runvalue - multiplo);
		residual_memory2 = (_runvalueback - multiplo);

		return (residual_memory2 >= time && residual_memory1 <= time);
	}
	else
	{
		multiplo = (std::floor((_runvalue / element)) * element);

		residual_memory1 = (_runvalue - multiplo);
		residual_memory2 = (_runvalueback - multiplo);

		return (residual_memory2 <= time && residual_memory1 >= time);
	}
}

int steady_clock_::factor_res(int time)
{
	int rounded_back = 0;
	int rounded_value = 0;

	if (_runvalueback >= _runvalue)
	{
		rounded_back = static_cast<int>(std::floor(_runvalueback));
		rounded_value = static_cast<int>(std::ceil(_runvalue));
	}
	else
	{
		rounded_back = static_cast<int>(std::ceil(_runvalueback));
		rounded_value = static_cast<int>(std::floor(_runvalue));
	}

	if (rounded_back == rounded_value)
	{
		return (rounded_value % time);
	}
	return -1;
}
