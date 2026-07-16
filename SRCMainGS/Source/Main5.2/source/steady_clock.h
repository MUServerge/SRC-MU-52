#pragma once
#define REFERENCE_FPS						25.0
#define FPS_ANIMATION_FACTOR				(gsteady_clock->GetLegacyVisualStep())
#define checkNormalizer						(gsteady_clock->CheckNormalizer())
#define rand_fps_check(reference)			(gsteady_clock->rand_calc_check(reference))
#define gsteady_clock						(csteady_clock::Instance())



class CheckerTime
{
public:
	CheckerTime(float intervalInSeconds){
		intervalSet(intervalInSeconds);
	}
	CheckerTime() {
		intervalSet(0.0f);
	}

	CheckerTime& operator=(float _interval) {

		intervalSet(_interval);
		return *this;
	}

	void intervalSet(float intervalInSeconds) {
		if (intervalInSeconds == 0.0f) {
			initialize = false;
		}
		else {
			initialize = true;
			interval = std::chrono::duration<float>(intervalInSeconds);
			lastCheckTime = std::chrono::steady_clock::now();
		}
	}

	// Retorna true si ha pasado el intervalo especificado.
	bool now() {
		return this->hasElapsed();
	}
	float work_duration() {
		if (!initialize) return 0.0f;
		auto currentTime = std::chrono::steady_clock::now();
		auto elapsedTime = std::chrono::duration<float>(currentTime - lastCheckTime).count();
		return min((elapsedTime / interval.count()), 1.0f);
	}

	bool hasElapsed() {
		if (initialize)
		{
			auto currentTime = std::chrono::steady_clock::now();
			auto elapsedTime = currentTime - lastCheckTime;

			if (elapsedTime >= interval) {
				lastCheckTime = currentTime; // Reinicia el tiempo
				return true;
			}
		}
		return false;
	}
	bool CheckElapsed() {
		if (initialize)
		{
			auto currentTime = std::chrono::steady_clock::now();
			auto elapsedTime = currentTime - lastCheckTime;

			return (elapsedTime >= interval);
		}
		return false;
	}

private:
	bool initialize;
	std::chrono::duration<float> interval;
	std::chrono::time_point<std::chrono::steady_clock> lastCheckTime;
};

class steady_clock_
{
public:
	double _runvalue;
	double _runvalueback;

	// Constructor
	steady_clock_() : _runvalue(0.0), _runvalueback(0.0) {}
	steady_clock_(double value) : _runvalue(value), _runvalueback(value) {}

	steady_clock_& operator=(double value) {
		_runvalueback = value;
		_runvalue = value;
		return *this;
	}

	explicit operator double() const {
		return _runvalue;
	}

	explicit operator float() const {
		return static_cast<float>(_runvalue);
	}

	explicit operator int() const {
		return static_cast<int>(_runvalue);
	}

	//__forceinline int operator << (int Data)
	//{
	//	return (*this);
	//}

	// Adici�n y Asignaci�n
	template<typename T>
	steady_clock_& operator+=(T value) {
		_runvalueback = _runvalue;
		_runvalue += static_cast<double>(value);
		return *this;
	}

	// Sustracci�n y Asignaci�n
	template<typename T>
	steady_clock_& operator-=(T value) {
		_runvalueback = _runvalue;
		_runvalue -= static_cast<double>(value);
		return *this;
	}

	// Multiplicaci�n y Asignaci�n
	template<typename T>
	steady_clock_& operator*=(T value) {
		_runvalueback = _runvalue;
		_runvalue *= static_cast<double>(value);
		return *this;
	}

	// Divisi�n y Asignaci�n
	template<typename T>
	steady_clock_& operator/=(T value) {
		_runvalueback = _runvalue;
		_runvalue /= static_cast<double>(value);
		return *this;
	}

	// Incremento
	steady_clock_& operator++() { // Pre-incremento
		_runvalueback = _runvalue;
		++_runvalue;
		return *this;
	}

	steady_clock_ operator++(int) {
		steady_clock_ temp = *this;
		_runvalueback = _runvalue;
		++_runvalue;
		return temp;
	}

	// Incremento
	steady_clock_& operator--() { // Pre-incremento
		_runvalueback = _runvalue;
		--_runvalue;
		return *this;
	}

	steady_clock_ operator--(int) {
		steady_clock_ temp = *this;
		_runvalueback = _runvalue;
		--_runvalue;
		return temp;
	}

	// M�dulo
	double operator%(int element) const
	{
		return std::fabs(fmod(_runvalue, (double)element));
	}

	// Igualdad
	template<typename T>
	bool operator==(T value) const {
		return numeral(value);
	}

	// Igualdad
	template<typename T>
	bool operator!=(T value) const {
		return !numeral(value);
	}

	// Menor o igual que
	template<typename T>
	bool operator<=(T value) const {
		return _runvalue <= value;
	}

	// Mayor o igual que
	template<typename T>
	bool operator>=(T value) const {
		return _runvalue >= value;
	}

	// Menor que
	template<typename T>
	bool operator<(T value) const {
		return _runvalue < value;
	}

	// Mayor que
	template<typename T>
	bool operator>(T value) const {
		return _runvalue > value;
	}

	bool numeral(int element) const;
	bool duration(int element);
	bool residual_duration(int element, int time);

	int factor_res(int time);
};


class CTimer;

enum FramePacingMode
{
	FRAME_PACING_SOFTWARE = 0,
	FRAME_PACING_VSYNC
};

class csteady_clock
{
public:
	csteady_clock();
	virtual~csteady_clock();

public:
	bool CheckNormalizer();
	int GetLimitFps();
	double Getframe_per_second();

	// Real elapsed frame time. Use this for interpolation, camera smoothing and UI.
	double GetFrameDeltaSeconds();
	double GetDeltaTimeSeconds();

	// MU's authored update step is 25 FPS. Use the update step for simulation-style
	// legacy counters, and the capped visual step for effects that must stay smooth.
	double GetLegacyUpdateStep();
	double GetLegacyVisualStep();
	double GetRealLegacyStep();
	double GetVisualLegacyStep();
	double GetNormalizerFps();
	double GetDeltAccumulated();

	void normalizefps();
	void LoadInformationFps();
	std::chrono::steady_clock::time_point GetthreadTime();
	double thread_sleep(const std::chrono::steady_clock::time_point thread_tick);
	void SetFramePacingMode(FramePacingMode mode);
	FramePacingMode GetFramePacingMode() const;
	bool IsSoftwareFrameLimitEnabled() const;

	bool rand_calc_check(int fr);
	void runtime_send_ping();
	void runtime_recv_ping(int ping_id);
	int get_ping_time();
	static csteady_clock* Instance();
private:
	int32_t counterframe;
	bool normal_check;

	double fpsNormalizer;
	double realDeltaTime;
	double realLegacyStep;
	double speedNormalizer;
	double deltaAccumulated;
	CheckerTime frame_limit;
	FramePacingMode framePacingMode;
	std::chrono::steady_clock::time_point mainthread;

	double save_time;
	CTimer* threadTime;
	//std::chrono::steady_clock::time_point last_save_time;
	std::chrono::steady_clock::time_point last_check_time;

	int ping_reg_id;
	long long ping_time;
	std::unordered_map<int, std::chrono::high_resolution_clock::time_point> pingMap;
};


extern DWORD standlimit(DWORD x);
extern double timepow(double x = 1.0);
extern double timefac(double x = 1.0);
extern double timeNormalizer(double x = 1.0);
