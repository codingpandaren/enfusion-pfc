class PFC_FlightControlsSettingsSubMenu : SCR_SettingsSubMenuBase
{
	protected PFC_JoystickTuning m_Tuning;

	protected SCR_SliderComponent m_PitchDeadzone;
	protected SCR_SliderComponent m_PitchCurve;
	protected SCR_SliderComponent m_RollDeadzone;
	protected SCR_SliderComponent m_RollCurve;
	protected SCR_SliderComponent m_YawDeadzone;
	protected SCR_SliderComponent m_YawCurve;
	protected SCR_SliderComponent m_ThrottleDeadzone;

	override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
	{
		super.OnTabCreate(menuRoot, buttonsLayout, index);
		m_Tuning = PFC_JoystickTuning.GetInstance();

		m_PitchDeadzone = BindSlider("PitchDeadzone", 0, 50, 1, m_Tuning.GetPitchDeadzone() * 100);
		m_PitchCurve = BindSlider("PitchCurve", 1.0, 3.0, 0.05, m_Tuning.GetPitchCurve());
		m_RollDeadzone = BindSlider("RollDeadzone", 0, 50, 1, m_Tuning.GetRollDeadzone() * 100);
		m_RollCurve = BindSlider("RollCurve", 1.0, 3.0, 0.05, m_Tuning.GetRollCurve());
		m_YawDeadzone = BindSlider("YawDeadzone", 0, 50, 1, m_Tuning.GetYawDeadzone() * 100);
		m_YawCurve = BindSlider("YawCurve", 1.0, 3.0, 0.05, m_Tuning.GetYawCurve());
		m_ThrottleDeadzone = BindSlider("ThrottleDeadzone", 0, 20, 1, m_Tuning.GetThrottleDeadzone() * 100);
	}

	protected SCR_SliderComponent BindSlider(string widgetName, float min, float max, float step, float value)
	{
		Widget w = m_wRoot.FindAnyWidget(widgetName);
		if (!w)
		{
			Print("[PFC_FlightControls] slider widget missing: " + widgetName, LogLevel.WARNING);
			return null;
		}

		SCR_SliderComponent slider = SCR_SliderComponent.Cast(w.FindHandler(SCR_SliderComponent));
		if (!slider)
			return null;

		slider.SetMin(min);
		slider.SetMax(max);
		slider.SetStep(step);
		slider.SetValue(value);
		slider.GetOnChangedFinal().Insert(OnSliderChanged);
		return slider;
	}

	protected void OnSliderChanged(SCR_WLibComponentBase component, float value)
	{
		if (component == m_PitchDeadzone)
			m_Tuning.SetPitchDeadzone(value * 0.01);
		else if (component == m_PitchCurve)
			m_Tuning.SetPitchCurve(value);
		else if (component == m_RollDeadzone)
			m_Tuning.SetRollDeadzone(value * 0.01);
		else if (component == m_RollCurve)
			m_Tuning.SetRollCurve(value);
		else if (component == m_YawDeadzone)
			m_Tuning.SetYawDeadzone(value * 0.01);
		else if (component == m_YawCurve)
			m_Tuning.SetYawCurve(value);
		else if (component == m_ThrottleDeadzone)
			m_Tuning.SetThrottleDeadzone(value * 0.01);
		else
			return;

		m_Tuning.Save();
	}
}
