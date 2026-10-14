class PFC_JoystickTuning : JsonApiStruct
{
	protected const string PFC_TUNING_PATH = "$profile:PFC_JoystickTuning.json";
	protected static ref PFC_JoystickTuning s_Instance;

	protected float m_fPitchDeadzone;
	protected float m_fPitchCurve = 1.0;
	protected float m_fRollDeadzone;
	protected float m_fRollCurve = 1.0;
	protected float m_fYawDeadzone;
	protected float m_fYawCurve = 1.0;
	protected float m_fThrottleDeadzone;

	void PFC_JoystickTuning()
	{
		RegV("m_fPitchDeadzone");
		RegV("m_fPitchCurve");
		RegV("m_fRollDeadzone");
		RegV("m_fRollCurve");
		RegV("m_fYawDeadzone");
		RegV("m_fYawCurve");
		RegV("m_fThrottleDeadzone");
	}

	static PFC_JoystickTuning GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new PFC_JoystickTuning();
			s_Instance.Load();
		}
		return s_Instance;
	}

	protected void Load()
	{
		ResetDefaults();
		if (FileIO.FileExists(PFC_TUNING_PATH))
			LoadFromFile(PFC_TUNING_PATH);
		Sanitize();
	}

	void Save()
	{
		Sanitize();
		PackToFile(PFC_TUNING_PATH);
	}

	protected void Sanitize()
	{
		SetPitchDeadzone(m_fPitchDeadzone);
		SetPitchCurve(m_fPitchCurve);
		SetRollDeadzone(m_fRollDeadzone);
		SetRollCurve(m_fRollCurve);
		SetYawDeadzone(m_fYawDeadzone);
		SetYawCurve(m_fYawCurve);
		SetThrottleDeadzone(m_fThrottleDeadzone);
	}

	void ResetDefaults()
	{
		m_fPitchDeadzone = 0;
		m_fPitchCurve = 1.0;
		m_fRollDeadzone = 0;
		m_fRollCurve = 1.0;
		m_fYawDeadzone = 0;
		m_fYawCurve = 1.0;
		m_fThrottleDeadzone = 0;
	}

	float ApplyPitch(float value)
	{
		return ApplyAxis(value, m_fPitchDeadzone, m_fPitchCurve);
	}

	float ApplyRoll(float value)
	{
		return ApplyAxis(value, m_fRollDeadzone, m_fRollCurve);
	}

	float ApplyYaw(float value)
	{
		return ApplyAxis(value, m_fYawDeadzone, m_fYawCurve);
	}

	float ApplyThrottle01(float value)
	{
		float t = Math.Clamp(value, 0, 1);
		float m = Math.Clamp(m_fThrottleDeadzone, 0, 0.4);
		if (m <= 0.001)
			return t;
		return Math.Clamp((t - m) / (1.0 - 2.0 * m), 0, 1);
	}

	protected float ApplyAxis(float value, float deadzone, float curve)
	{
		float clamped = Math.Clamp(value, -1, 1);
		float mag = Math.AbsFloat(clamped);
		float dz = Math.Clamp(deadzone, 0, 0.9);
		if (mag <= dz)
			return 0;

		float normalized = (mag - dz) / (1.0 - dz);
		float shaped = Math.Pow(normalized, Math.Clamp(curve, 1.0, 3.0));
		if (clamped < 0)
			return -shaped;
		return shaped;
	}

	void SetPitchDeadzone(float v) { m_fPitchDeadzone = Math.Clamp(v, 0, 0.5); }
	void SetPitchCurve(float v) { m_fPitchCurve = Math.Clamp(v, 1.0, 3.0); }
	void SetRollDeadzone(float v) { m_fRollDeadzone = Math.Clamp(v, 0, 0.5); }
	void SetRollCurve(float v) { m_fRollCurve = Math.Clamp(v, 1.0, 3.0); }
	void SetYawDeadzone(float v) { m_fYawDeadzone = Math.Clamp(v, 0, 0.5); }
	void SetYawCurve(float v) { m_fYawCurve = Math.Clamp(v, 1.0, 3.0); }
	void SetThrottleDeadzone(float v) { m_fThrottleDeadzone = Math.Clamp(v, 0, 0.2); }

	float GetPitchDeadzone() { return m_fPitchDeadzone; }
	float GetPitchCurve() { return m_fPitchCurve; }
	float GetRollDeadzone() { return m_fRollDeadzone; }
	float GetRollCurve() { return m_fRollCurve; }
	float GetYawDeadzone() { return m_fYawDeadzone; }
	float GetYawCurve() { return m_fYawCurve; }
	float GetThrottleDeadzone() { return m_fThrottleDeadzone; }
}
